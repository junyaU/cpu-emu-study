#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emulator.h"
#include "instructions.h"
#include "emulator_func.h"
#include "modrm.h"

instruction_func_t *instructions[256];

static void mov_r32_imm32(Emulator* emu)
{
    uint8_t reg = get_code8(emu, 0) - 0xB8;
    uint32_t value = get_code32(emu, 1);
    set_register32(emu, reg, value);
    emu->eip += 5;
}

static void  mov_r32_rm32(Emulator* emu)
{
    emu->eip++;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t rm32 = get_rm32(emu, &modrm);
    set_r32(emu, &modrm, rm32);
}

static void add_rm32_r32(Emulator* emu)
{
    emu->eip++;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t r32 = get_r32(emu, &modrm);
    uint32_t rm32 = get_rm32(emu, &modrm);
    set_rm32(emu, &modrm, rm32 + r32);
}

static void add_r32_rm32(Emulator* emu){
    emu->eip++;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t r32 = get_r32(emu, &modrm);
    uint32_t rm32 = get_rm32(emu, &modrm);
    set_r32(emu, &modrm, rm32 + r32);
}

static void mov_rm32_r32(Emulator* emu)
{
    emu->eip++;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t r32 = get_r32(emu, &modrm);
    set_rm32(emu, &modrm, r32);
}

static void push_r32(Emulator* emu)
{
    uint8_t reg = get_code8(emu, 0) - 0x50;
    push32(emu, get_register32(emu, reg));
    emu->eip++;
}

static void pop_r32(Emulator* emu)
{
    uint8_t reg = get_code8(emu, 0) - 0x58;
    set_register32(emu, reg, pop32(emu));
    emu->eip++;
}

static void push_imm32(Emulator* emu)
{
    uint32_t value = get_code32(emu, 1);
    push32(emu, value);
    emu->eip += 5;
}

static void push_imm8(Emulator* emu)
{
    uint8_t value = get_code8(emu, 1);
    push32(emu, value);
    emu->eip += 2;
}

static void add_rm32_imm8(Emulator* emu, ModRM* modrm)
{
    uint32_t rm32 = get_rm32(emu, modrm);
    uint32_t imm8 = (uint32_t) get_sign_code8(emu, 0);
    emu->eip++;
    set_rm32(emu, modrm, rm32 + imm8);
}

static void sub_rm32_imm8(Emulator* emu, ModRM* modrm)
{
    uint32_t rm32 = get_rm32(emu, modrm);
    uint32_t imm8 = (uint32_t) get_sign_code8(emu, 0);
    emu->eip++;
    set_rm32(emu, modrm, rm32 - imm8);
}

static void code_83(Emulator* emu)
{
    emu->eip++;
    ModRM modrm;
    parse_modrm(emu, &modrm);

    switch (modrm.opecode) {
        case 0:
            add_rm32_imm8(emu, &modrm);
            break;
        case 5:
            sub_rm32_imm8(emu, &modrm);
            break;
        default:
            printf("not implemented: 83 /%d\n", modrm.opecode);
            exit(1);
    }
}

static void mov_rm32_imm32(Emulator* emu)
{
    emu->eip++;
    ModRM modrm;
    parse_modrm(emu, &modrm);
    uint32_t value = get_code32(emu, 0);
    emu->eip += 4;
    set_rm32(emu, &modrm, value);
}

static void inc_rm32(Emulator* emu, ModRM* modrm)
{
    uint32_t value = get_rm32(emu, modrm);
    set_rm32(emu, modrm, value + 1);
}

static void code_ff(Emulator* emu)
{
    emu->eip++;
    ModRM modrm;
    parse_modrm(emu, &modrm);

    switch (modrm.opecode) {
        case 0:
            inc_rm32(emu, &modrm);
            break;
        default:
            printf("not implemented: FF /%d\n", modrm.opecode);
            exit(1);
    }
}

static void call_rel32(Emulator* emu)
{
    int32_t diff = get_sign_code32(emu, 1);
    push32(emu, emu->eip + 5);
    emu->eip +=  (diff + 5);
}

static void ret(Emulator* emu)
{
    emu->eip = pop32(emu);
}

static  void leave(Emulator* emu)
{
    uint32_t ebp = get_register32(emu, EBP);
    set_register32(emu, ESP, ebp);
    set_register32(emu, EBP, pop32(emu));
}

static void short_jump(Emulator* emu)
{
    int8_t diff = get_sign_code8(emu, 1);
    emu->eip += (diff + 2);
}

static void near_jump(Emulator* emu)
{
    int32_t diff = get_sign_code32(emu, 1);
    emu->eip += (diff + 5);
}

void init_instructions(void)
{
    memset(instructions, 0, sizeof(instructions));
    int i;

    instructions[0x01] = add_rm32_r32;
    instructions[0x03] = add_r32_rm32;

    for (i = 0; i < 8; i++) {
        instructions[0x50 + i] = push_r32;
    }

    for (i = 0; i < 8; i++) {
        instructions[0x58 + i] = pop_r32;
    }

    instructions[0x68] = push_imm32;
    instructions[0x6A] = push_imm8;

    instructions[0x83] = code_83;
    instructions[0x89] = mov_rm32_r32;
    instructions[0x8B] = mov_r32_rm32;

    for (i = 0; i < 8; i++) {
        instructions[0xB8 + i] = mov_r32_imm32;
    }

    instructions[0xC3] = ret;
    instructions[0xC7] = mov_rm32_imm32;
    instructions[0xC9] = leave;

    instructions[0xE8] = call_rel32;
    instructions[0xE9] = near_jump;
    instructions[0xEB] = short_jump;
    instructions[0xFF] = code_ff;
}