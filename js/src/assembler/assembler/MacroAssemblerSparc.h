/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacroAssemblerSparc_h
#define MacroAssemblerSparc_h

#include <assembler/wtf/Platform.h>

#if ENABLE_ASSEMBLER && WTF_CPU_SPARC

#include "SparcAssembler.h"
#include "AbstractMacroAssembler.h"

namespace JSC {

    class MacroAssemblerSparc : public AbstractMacroAssembler<SparcAssembler> {
    public:
        enum Condition {
            Equal = SparcAssembler::ConditionE,
            NotEqual = SparcAssembler::ConditionNE,
            Above = SparcAssembler::ConditionGU,
            AboveOrEqual = SparcAssembler::ConditionCC,
            Below = SparcAssembler::ConditionCS,
            BelowOrEqual = SparcAssembler::ConditionLEU,
            GreaterThan = SparcAssembler::ConditionG,
            GreaterThanOrEqual = SparcAssembler::ConditionGE,
            LessThan = SparcAssembler::ConditionL,
            LessThanOrEqual = SparcAssembler::ConditionLE,
            Overflow = SparcAssembler::ConditionVS,
            Signed = SparcAssembler::ConditionNEG,
            Zero = SparcAssembler::ConditionE,
            NonZero = SparcAssembler::ConditionNE
        };

        enum DoubleCondition {
            // These conditions will only evaluate to true if the comparison is ordered - i.e. neither operand is NaN.
            DoubleEqual = SparcAssembler::DoubleConditionE,
            DoubleNotEqual = SparcAssembler::DoubleConditionNE,
            DoubleGreaterThan = SparcAssembler::DoubleConditionG,
            DoubleGreaterThanOrEqual = SparcAssembler::DoubleConditionGE,
            DoubleLessThan = SparcAssembler::DoubleConditionL,
            DoubleLessThanOrEqual = SparcAssembler::DoubleConditionLE,
            // If either operand is NaN, these conditions always evaluate to true.
            DoubleEqualOrUnordered = SparcAssembler::DoubleConditionUE,
            DoubleNotEqualOrUnordered = SparcAssembler::DoubleConditionNE,
            DoubleGreaterThanOrUnordered = SparcAssembler::DoubleConditionUG,
            DoubleGreaterThanOrEqualOrUnordered = SparcAssembler::DoubleConditionUGE,
            DoubleLessThanOrUnordered = SparcAssembler::DoubleConditionUL,
            DoubleLessThanOrEqualOrUnordered = SparcAssembler::DoubleConditionULE
        };

        static const RegisterID stackPointerRegister = SparcRegisters::sp;

        static const Scale ScalePtr = TimesFour;
        static const unsigned int TotalRegisters = 24;

        void add32(RegisterID src, RegisterID dest)
        {
            m_assembler.addcc_r(dest, src, dest);
        }

        void add32(TrustedImm32 imm, Address address)
        {
            load32(address, SparcRegisters::g2);
            add32(imm, SparcRegisters::g2);
            store32(SparcRegisters::g2, address);
        }

        void add32(TrustedImm32 imm, RegisterID dest)
        {
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.addcc_imm(dest, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.addcc_r(dest, SparcRegisters::g3, dest);
            }
        }

        void add32(Address src, RegisterID dest)
        {
            load32(src, SparcRegisters::g2);
            m_assembler.addcc_r(dest, SparcRegisters::g2, dest);
        }

        void and32(Address src, RegisterID dest)
        {
            load32(src, SparcRegisters::g2);
            m_assembler.andcc_r(dest, SparcRegisters::g2, dest);
        }

        void add32(TrustedImm32 imm, RegisterID src, RegisterID dest)
        {
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.addcc_imm(src, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.addcc_r(src, SparcRegisters::g3, dest);
            }
        }

        void and32(RegisterID src, RegisterID dest)
        {
            m_assembler.andcc_r(dest, src, dest);
        }

        void and32(Imm32 imm, RegisterID dest)
        {
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.andcc_imm(dest, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.andcc_r(dest, SparcRegisters::g3, dest);
            }
        }


        void lshift32(RegisterID shift_amount, RegisterID dest)
        {
            m_assembler.sll_r(dest, shift_amount, dest);
        }

        void lshift32(Imm32 imm, RegisterID dest)
        {
            // No need to check if imm.m_value.
            // The last 5 bit of imm.m_value will be used anyway.
            m_assembler.sll_imm(dest, imm.m_value, dest);
        }

        void mul32(RegisterID src, RegisterID dest)
        {
            m_assembler.smulcc_r(dest, src, dest);
        }

        void mul32(Imm32 imm, RegisterID src, RegisterID dest)
        {
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.smulcc_imm(dest, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.smulcc_r(SparcRegisters::g3, dest, dest);
            }
        }

        void neg32(RegisterID srcDest)
        {
            m_assembler.subcc_r(SparcRegisters::g0, srcDest, srcDest);
        }

        void not32(RegisterID dest)
        {
            m_assembler.xnorcc_r(dest, SparcRegisters::g0, dest);
        }

        void or32(RegisterID src, RegisterID dest)
        {
            m_assembler.orcc_r(dest, src, dest);
        }

        void or32(TrustedImm32 imm, RegisterID dest)
        {
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.orcc_imm(dest, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.or_r(SparcRegisters::g3, dest, dest);
            }
        }


        void or32(Address address, RegisterID dest)
        {
            load32(address, SparcRegisters::g2);
            or32(SparcRegisters::g2, dest);
        }

        void rshift32(RegisterID shift_amount, RegisterID dest)
        {
            m_assembler.sra_r(dest, shift_amount, dest);
        }

        void rshift32(Imm32 imm, RegisterID dest)
        {
            // No need to check if imm.m_value.
            // The last 5 bit of imm.m_value will be used anyway.
            m_assembler.sra_imm(dest, imm.m_value, dest);
        }
    
        void urshift32(RegisterID shift_amount, RegisterID dest)
        {
            m_assembler.srl_r(dest, shift_amount, dest);
        }
    
        void urshift32(Imm32 imm, RegisterID dest)
        {
            // No need to check if imm.m_value.
            // The last 5 bit of imm.m_value will be used anyway.
            m_assembler.srl_imm(dest, imm.m_value, dest);
        }

        void sub32(RegisterID src, RegisterID dest)
        {
            m_assembler.subcc_r(dest, src, dest);
        }

        void sub32(TrustedImm32 imm, RegisterID dest)
        {
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.subcc_imm(dest, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.subcc_r(dest, SparcRegisters::g3, dest);
            }
        }

        void sub32(TrustedImm32 imm, Address address)
        {
            load32(address, SparcRegisters::g2);
            sub32(imm, SparcRegisters::g2);
            store32(SparcRegisters::g2, address);
        }

        void sub32(Address src, RegisterID dest)
        {
            load32(src, SparcRegisters::g2);
            sub32(SparcRegisters::g2, dest);
        }

        void xor32(RegisterID src, RegisterID dest)
        {
            m_assembler.xorcc_r(src, dest, dest);
        }

        void xor32(TrustedImm32 imm, RegisterID dest)
        {
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.xorcc_imm(dest, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.xorcc_r(dest, SparcRegisters::g3, dest);
            }
        }

        void xor32(Address src, RegisterID dest)
        {
            load32(src, SparcRegisters::g2);
            xor32(SparcRegisters::g2, dest);
        }

        void load8(ImplicitAddress address, RegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.ldub_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.ldub_r(address.base, SparcRegisters::g3, dest);
            }
        }

        void load32(ImplicitAddress address, RegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.lduw_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.lduw_r(address.base, SparcRegisters::g3, dest);
            }
        }

        void load32(BaseIndex address, RegisterID dest)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.lduw_r(address.base, SparcRegisters::g2, dest);
        }

        void load32WithUnalignedHalfWords(BaseIndex address, RegisterID dest)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset+3), SparcRegisters::g2);
            m_assembler.ldub_r(address.base, SparcRegisters::g2, dest);
            m_assembler.subcc_imm(SparcRegisters::g2, 1, SparcRegisters::g2);
            m_assembler.ldub_r(address.base, SparcRegisters::g2, SparcRegisters::g3);
            m_assembler.sll_imm(SparcRegisters::g3, 8, SparcRegisters::g3);
            m_assembler.or_r(SparcRegisters::g3, dest, dest);
            m_assembler.subcc_imm(SparcRegisters::g2, 1, SparcRegisters::g2);
            m_assembler.ldub_r(address.base, SparcRegisters::g2, SparcRegisters::g3);
            m_assembler.sll_imm(SparcRegisters::g3, 16, SparcRegisters::g3);
            m_assembler.or_r(SparcRegisters::g3, dest, dest);
            m_assembler.subcc_imm(SparcRegisters::g2, 1, SparcRegisters::g2);
            m_assembler.ldub_r(address.base, SparcRegisters::g2, SparcRegisters::g3);
            m_assembler.sll_imm(SparcRegisters::g3, 24, SparcRegisters::g3);
            m_assembler.or_r(SparcRegisters::g3, dest, dest);
        }

        DataLabel32 load32WithAddressOffsetPatch(Address address, RegisterID dest)
        {
            DataLabel32 dataLabel(this);
            m_assembler.move_nocheck(0, SparcRegisters::g3);
            m_assembler.lduw_r(address.base, SparcRegisters::g3, dest);
            return dataLabel;
        }

        DataLabel32 load64WithAddressOffsetPatch(Address address, RegisterID hi, RegisterID lo)
        {
            DataLabel32 dataLabel(this);
            m_assembler.move_nocheck(0, SparcRegisters::g3);
            m_assembler.add_imm(SparcRegisters::g3, 4, SparcRegisters::g2);
            m_assembler.lduw_r(address.base, SparcRegisters::g3, hi);
            m_assembler.lduw_r(address.base, SparcRegisters::g2, lo);
            return dataLabel;
        }

        Label loadPtrWithPatchToLEA(Address address, RegisterID dest)
        {
            Label label(this);
            load32(address, dest);
            return label;
        }

        void load16(BaseIndex address, RegisterID dest)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.lduh_r(address.base, SparcRegisters::g2, dest);
        }
    
        void load16(ImplicitAddress address, RegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.lduh_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.lduh_r(address.base, SparcRegisters::g3, dest);
            }
        }

        void store8(RegisterID src, ImplicitAddress address)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.stb_imm(src, address.base, address.offset);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.stb_r(src, address.base, SparcRegisters::g3);
            }
        }

        void store8(RegisterID src, BaseIndex address)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.stb_r(src, address.base, SparcRegisters::g2);
        }

        void store8(Imm32 imm, ImplicitAddress address)
        {
            m_assembler.move_nocheck(imm.m_value, SparcRegisters::g2);
            store8(SparcRegisters::g2, address);
        }

        void store8(Imm32 imm, BaseIndex address)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            move(imm, SparcRegisters::g3);
            m_assembler.stb_r(SparcRegisters::g3, SparcRegisters::g2, address.base);
        }

        void store16(RegisterID src, ImplicitAddress address)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.sth_imm(src, address.base, address.offset);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.sth_r(src, address.base, SparcRegisters::g3);
            }
        }

        void store16(RegisterID src, BaseIndex address)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.sth_r(src, address.base, SparcRegisters::g2);
        }

        void store16(Imm32 imm, ImplicitAddress address)
        {
            m_assembler.move_nocheck(imm.m_value, SparcRegisters::g2);
            store16(SparcRegisters::g2, address);
        }

        void store16(Imm32 imm, BaseIndex address)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            move(imm, SparcRegisters::g3);
            m_assembler.sth_r(SparcRegisters::g3, SparcRegisters::g2, address.base);
        }

        void load8ZeroExtend(BaseIndex address, RegisterID dest)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.ldub_r(address.base, SparcRegisters::g2, dest);
        }

        void load8ZeroExtend(Address address, RegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.ldub_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.ldub_r(address.base, SparcRegisters::g3, dest);
            }
        }

        void load8SignExtend(BaseIndex address, RegisterID dest)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.ldsb_r(address.base, SparcRegisters::g2, dest);
        }

        void load8SignExtend(Address address, RegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.ldsb_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.ldsb_r(address.base, SparcRegisters::g3, dest);
            }
        }

        void load16SignExtend(BaseIndex address, RegisterID dest)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.ldsh_r(address.base, SparcRegisters::g2, dest);
        }

        void load16SignExtend(Address address, RegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.ldsh_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.ldsh_r(address.base, SparcRegisters::g3, dest);
            }
        }

        DataLabel32 store32WithAddressOffsetPatch(RegisterID src, Address address)
        {
            DataLabel32 dataLabel(this);
            // Since this is for patch, we don't check is offset is imm13.
            m_assembler.move_nocheck(0, SparcRegisters::g3);
            m_assembler.stw_r(src, address.base, SparcRegisters::g3);
            return dataLabel;
        }


        DataLabel32 store64WithAddressOffsetPatch(RegisterID hi, RegisterID lo, Address address)
        {
            DataLabel32 dataLabel(this);
            m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
            m_assembler.add_r(SparcRegisters::g3, address.base, SparcRegisters::g3);
            m_assembler.stw_imm(lo, SparcRegisters::g3, 4);
            m_assembler.stw_imm(hi, SparcRegisters::g3, 0);
            return dataLabel;
        }

        DataLabel32 store64WithAddressOffsetPatch(Imm32 hi, RegisterID lo, Address address)
        {
            DataLabel32 dataLabel(this);
            m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
            m_assembler.add_r(SparcRegisters::g3, address.base, SparcRegisters::g3);
            m_assembler.stw_imm(lo, SparcRegisters::g3, 4);
            move(hi, SparcRegisters::g2);
            m_assembler.stw_imm(SparcRegisters::g2, SparcRegisters::g3, 0);

            return dataLabel;
        }

        DataLabel32 store64WithAddressOffsetPatch(Imm32 hi, Imm32 lo, Address address)
        {
            DataLabel32 dataLabel(this);
            m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
            m_assembler.add_r(SparcRegisters::g3, address.base, SparcRegisters::g3);
            move(lo, SparcRegisters::g2);
            m_assembler.stw_imm(SparcRegisters::g2, SparcRegisters::g3, 4);
            move(hi, SparcRegisters::g2);
            m_assembler.stw_imm(SparcRegisters::g2, SparcRegisters::g3, 0);

            return dataLabel;
        }


        void store32(RegisterID src, ImplicitAddress address)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.stw_imm(src, address.base, address.offset);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.stw_r(src, address.base, SparcRegisters::g3);
            }
        }

        void store32(RegisterID src, BaseIndex address)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.stw_r(src, address.base, SparcRegisters::g2);
        }

        void store32(TrustedImm32 imm, BaseIndex address)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            move(imm, SparcRegisters::g3);
            m_assembler.stw_r(SparcRegisters::g3, SparcRegisters::g2, address.base);
        }

        void store32(TrustedImm32 imm, ImplicitAddress address)
        {
            m_assembler.move_nocheck(imm.m_value, SparcRegisters::g2);
            store32(SparcRegisters::g2, address);
        }

        void store32(RegisterID src, void* address)
        {
            m_assembler.move_nocheck((int)address, SparcRegisters::g3);
            m_assembler.stw_r(src, SparcRegisters::g0, SparcRegisters::g3);
        }

        void store32(TrustedImm32 imm, void* address)
        {
            move(imm, SparcRegisters::g2);
            store32(SparcRegisters::g2, address);
        }

        void pop(RegisterID dest)
        {
            m_assembler.lduw_imm(SparcRegisters::sp, 0x68, dest);
            m_assembler.addcc_imm(SparcRegisters::sp, 4, SparcRegisters::sp);
        }

        void push(RegisterID src)
        {
            m_assembler.subcc_imm(SparcRegisters::sp, 4, SparcRegisters::sp);
            m_assembler.stw_imm(src, SparcRegisters::sp, 0x68);
        }

        void push(Address address)
        {
            load32(address, SparcRegisters::g2);
            push(SparcRegisters::g2);
        }

        void push(Imm32 imm)
        {
            move(imm, SparcRegisters::g2);
            push(SparcRegisters::g2);
        }

        void move(TrustedImm32 imm, RegisterID dest)
        {
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.or_imm(SparcRegisters::g0, imm.m_value, dest);
            else
                m_assembler.move_nocheck(imm.m_value, dest);
        }

        void move(RegisterID src, RegisterID dest)
        {
            m_assembler.or_r(src, SparcRegisters::g0, dest);
        }

        void move(TrustedImmPtr imm, RegisterID dest)
        {
            move(Imm32(imm), dest);
        }

        void swap(RegisterID reg1, RegisterID reg2)
        {
            m_assembler.or_r(reg1, SparcRegisters::g0, SparcRegisters::g3);
            m_assembler.or_r(reg2, SparcRegisters::g0, reg1);
            m_assembler.or_r(SparcRegisters::g3, SparcRegisters::g0, reg2);
        }

        void signExtend32ToPtr(RegisterID src, RegisterID dest)
        {
            if (src != dest)
                move(src, dest);
        }

        void zeroExtend32ToPtr(RegisterID src, RegisterID dest)
        {
            if (src != dest)
                move(src, dest);
        }

        Jump branch8(Condition cond, Address left, Imm32 right)
        {
            load8(left, SparcRegisters::g2);
            return branch32(cond, SparcRegisters::g2, right);
        }

        Jump branch32_force32(Condition cond, RegisterID left, TrustedImm32 right)
        {
            m_assembler.move_nocheck(right.m_value, SparcRegisters::g3);
            m_assembler.subcc_r(left, SparcRegisters::g3, SparcRegisters::g0);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branch32FixedLength(Condition cond, RegisterID left, TrustedImm32 right)
        {
            m_assembler.move_nocheck(right.m_value, SparcRegisters::g2);
            return branch32(cond, left, SparcRegisters::g2);
        }

        Jump branch32WithPatch(Condition cond, RegisterID left, TrustedImm32 right, DataLabel32 &dataLabel)
        {
            // Always use move_nocheck, since the value is to be patched.
            dataLabel = DataLabel32(this);
            m_assembler.move_nocheck(right.m_value, SparcRegisters::g3);
            m_assembler.subcc_r(left, SparcRegisters::g3, SparcRegisters::g0);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branch32(Condition cond, RegisterID left, RegisterID right)
        {
            m_assembler.subcc_r(left, right, SparcRegisters::g0);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branch32(Condition cond, RegisterID left, TrustedImm32 right)
        {
            if (m_assembler.isimm13(right.m_value))
                m_assembler.subcc_imm(left, right.m_value, SparcRegisters::g0);
            else {
                m_assembler.move_nocheck(right.m_value, SparcRegisters::g3);
                m_assembler.subcc_r(left, SparcRegisters::g3, SparcRegisters::g0);
            }
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branch32(Condition cond, RegisterID left, Address right)
        {
            load32(right, SparcRegisters::g2);
            return branch32(cond, left, SparcRegisters::g2);
        }

        Jump branch32(Condition cond, Address left, RegisterID right)
        {
            load32(left, SparcRegisters::g2);
            return branch32(cond, SparcRegisters::g2, right);
        }

        Jump branch32(Condition cond, Address left, TrustedImm32 right)
        {
            load32(left, SparcRegisters::g2);
            return branch32(cond, SparcRegisters::g2, right);
        }

        Jump branch32(Condition cond, BaseIndex left, TrustedImm32 right)
        {

            load32(left, SparcRegisters::g2);
            return branch32(cond, SparcRegisters::g2, right);
        }

        Jump branch32WithUnalignedHalfWords(Condition cond, BaseIndex left, TrustedImm32 right)
        {
            load32WithUnalignedHalfWords(left, SparcRegisters::g4);
            return branch32(cond, SparcRegisters::g4, right);
        }

        Jump branch16(Condition cond, BaseIndex left, RegisterID right)
        {
            (void)(cond);
            (void)(left);
            (void)(right);
            ASSERT_NOT_REACHED();
            return jump();
        }

        Jump branch16(Condition cond, BaseIndex left, Imm32 right)
        {
            load16(left, SparcRegisters::g3);
            move(right, SparcRegisters::g2);
            m_assembler.subcc_r(SparcRegisters::g3, SparcRegisters::g2, SparcRegisters::g0);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchTest8(Condition cond, Address address, Imm32 mask = Imm32(-1))
        {
            load8(address, SparcRegisters::g2);
            return branchTest32(cond, SparcRegisters::g2, mask);
        }

        Jump branchTest32(Condition cond, RegisterID reg, RegisterID mask)
        {
            m_assembler.andcc_r(reg, mask, SparcRegisters::g0);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchTest32(Condition cond, RegisterID reg, Imm32 mask = Imm32(-1))
        {
            if (m_assembler.isimm13(mask.m_value))
                m_assembler.andcc_imm(reg, mask.m_value, SparcRegisters::g0);
            else {
                m_assembler.move_nocheck(mask.m_value, SparcRegisters::g3);
                m_assembler.andcc_r(reg, SparcRegisters::g3, SparcRegisters::g0);
            }
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchTest32(Condition cond, Address address, Imm32 mask = Imm32(-1))
        {
            load32(address, SparcRegisters::g2);
            return branchTest32(cond, SparcRegisters::g2, mask);
        }

        Jump branchTest32(Condition cond, BaseIndex address, Imm32 mask = Imm32(-1))
        {
            // FIXME. branchTest32 only used by PolyIC.
            // PolyIC is not enabled for sparc now.
            ASSERT(0);
            return jump();
        }

        Jump jump()
        {
            return Jump(m_assembler.jmp());
        }

        void jump(RegisterID target)
        {
            m_assembler.jmpl_r(SparcRegisters::g0, target, SparcRegisters::g0);
            m_assembler.nop();
        }

        void jump(Address address)
        {
            load32(address, SparcRegisters::g2);
            m_assembler.jmpl_r(SparcRegisters::g2, SparcRegisters::g0, SparcRegisters::g0);
            m_assembler.nop();
        }

        void jump(BaseIndex address)
        {
            load32(address, SparcRegisters::g2);
            m_assembler.jmpl_r(SparcRegisters::g2, SparcRegisters::g0, SparcRegisters::g0);
            m_assembler.nop();
        }

        Jump branchAdd32(Condition cond, RegisterID src, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            m_assembler.addcc_r(src, dest, dest);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchAdd32(Condition cond, Imm32 imm, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.addcc_imm(dest, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.addcc_r(dest, SparcRegisters::g3, dest);
            }
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchAdd32(Condition cond, Address src, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            load32(src, SparcRegisters::g2);
            return branchAdd32(cond, SparcRegisters::g2, dest);
        }

        void mull32(RegisterID src1, RegisterID src2, RegisterID dest)
        {
            m_assembler.smulcc_r(src1, src2, dest);
        }

        Jump branchMul32(Condition cond, RegisterID src, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            m_assembler.smulcc_r(src, dest, dest);
            if (cond == Overflow) {
                m_assembler.rdy(SparcRegisters::g2);
                m_assembler.sra_imm(dest, 31, SparcRegisters::g3);
                m_assembler.subcc_r(SparcRegisters::g2, SparcRegisters::g3, SparcRegisters::g2);
                cond = NotEqual;
            }
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchMul32(Condition cond, Imm32 imm, RegisterID src, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            if (m_assembler.isimm13(imm.m_value))
                m_assembler.smulcc_imm(src, imm.m_value, dest);
            else {
                m_assembler.move_nocheck(imm.m_value, SparcRegisters::g3);
                m_assembler.smulcc_r(src, SparcRegisters::g3, dest);
            }
            if (cond == Overflow) {
                m_assembler.rdy(SparcRegisters::g2);
                m_assembler.sra_imm(dest, 31, SparcRegisters::g3);
                m_assembler.subcc_r(SparcRegisters::g2, SparcRegisters::g3, SparcRegisters::g2);
                cond = NotEqual;
            }
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchMul32(Condition cond, Address src, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            load32(src, SparcRegisters::g2);
            return branchMul32(cond, SparcRegisters::g2, dest);
        }

        Jump branchSub32(Condition cond, RegisterID src, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            m_assembler.subcc_r(dest, src, dest);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchSub32(Condition cond, Imm32 imm, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            sub32(imm, dest);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchSub32(Condition cond, Address src, RegisterID dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            load32(src, SparcRegisters::g2);
            return branchSub32(cond, SparcRegisters::g2, dest);
        }

        Jump branchSub32(Condition cond, Imm32 imm, Address dest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            sub32(imm, dest);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchNeg32(Condition cond, RegisterID srcDest)
        {
            ASSERT((cond == Overflow) || (cond == Signed) || (cond == Zero) || (cond == NonZero));
            neg32(srcDest);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        Jump branchOr32(Condition cond, RegisterID src, RegisterID dest)
        {
            ASSERT((cond == Signed) || (cond == Zero) || (cond == NonZero));
            m_assembler.orcc_r(src, dest, dest);
            return Jump(m_assembler.branch(SparcCondition(cond)));
        }

        void breakpoint()
        {
            m_assembler.ta_imm(8);
        }

        Call nearCall()
        {
            return Call(m_assembler.call(), Call::LinkableNear);
        }

        Call call(RegisterID target)
        {
            m_assembler.jmpl_r(target, SparcRegisters::g0, SparcRegisters::o7);
            m_assembler.nop();
            JmpSrc jmpSrc;
            return Call(jmpSrc, Call::None);
        }

        void call(Address address)
        {
            if (m_assembler.isimm13(address.offset)) {
                m_assembler.jmpl_imm(address.base, address.offset, SparcRegisters::o7);
                m_assembler.nop();
            } else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.jmpl_r(address.base, SparcRegisters::g3, SparcRegisters::o7);
                m_assembler.nop();
            }
        }

        void ret()
        {
            m_assembler.jmpl_imm(SparcRegisters::i7, 8, SparcRegisters::g0);
            m_assembler.nop();
        }

        void ret_and_restore()
        {
            m_assembler.jmpl_imm(SparcRegisters::i7, 8, SparcRegisters::g0);
            m_assembler.restore_r(SparcRegisters::g0, SparcRegisters::g0, SparcRegisters::g0);
        }

        void save(Imm32 size)
        {
            if (m_assembler.isimm13(size.m_value)) {
                m_assembler.save_imm(SparcRegisters::sp, size.m_value, SparcRegisters::sp);
            } else {
                m_assembler.move_nocheck(size.m_value, SparcRegisters::g3);
                m_assembler.save_r(SparcRegisters::sp, SparcRegisters::g3, SparcRegisters::sp);
            }
        }

        void set32(Condition cond, Address left, RegisterID right, RegisterID dest)
        {
            load32(left, SparcRegisters::g2);
            set32(cond, SparcRegisters::g2, right, dest);
        }

        void set32(Condition cond, RegisterID left, Address right, RegisterID dest)
        {
            load32(right, SparcRegisters::g2);
            set32(cond, left, SparcRegisters::g2, dest);
        }

        void set32(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
        {
            m_assembler.subcc_r(left, right, SparcRegisters::g0);
            m_assembler.or_imm(SparcRegisters::g0, 0, dest);
            m_assembler.movcc_imm(1, dest, SparcCondition(cond));
        }

        void set32(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
        {
            if (m_assembler.isimm13(right.m_value))
                m_assembler.subcc_imm(left, right.m_value, SparcRegisters::g0);
            else {
                m_assembler.move_nocheck(right.m_value, SparcRegisters::g3);
                m_assembler.subcc_r(left, SparcRegisters::g3, SparcRegisters::g0);
            }
            m_assembler.or_imm(SparcRegisters::g0, 0, dest);
            m_assembler.movcc_imm(1, dest, SparcCondition(cond));
        }

        void set32(Condition cond, Address left, Imm32 right, RegisterID dest)
        {
            load32(left, SparcRegisters::g2);
            set32(cond, SparcRegisters::g2, right, dest);
        }

        void set8(Condition cond, RegisterID left, RegisterID right, RegisterID dest)
        {
            // Sparc does not have byte register.
            set32(cond, left, right, dest);
        }

        void set8(Condition cond, Address left, RegisterID right, RegisterID dest)
        {
            // Sparc doesn't have byte registers
            load32(left, SparcRegisters::g2);
            set32(cond, SparcRegisters::g2, right, dest);
        }

        void set8(Condition cond, RegisterID left, Imm32 right, RegisterID dest)
        {
            // Sparc does not have byte register.
            set32(cond, left, right, dest);
        }

        void setTest32(Condition cond, Address address, Imm32 mask, RegisterID dest)
        {
            load32(address, SparcRegisters::g2);
            if (m_assembler.isimm13(mask.m_value))
                m_assembler.andcc_imm(SparcRegisters::g2, mask.m_value, SparcRegisters::g0);
            else {
                m_assembler.move_nocheck(mask.m_value, SparcRegisters::g3);
                m_assembler.andcc_r(SparcRegisters::g3, SparcRegisters::g2, SparcRegisters::g0);
            }
            m_assembler.or_imm(SparcRegisters::g0, 0, dest);
            m_assembler.movcc_imm(1, dest, SparcCondition(cond));
        }

        void setTest8(Condition cond, Address address, Imm32 mask, RegisterID dest)
        {
            // Sparc does not have byte register.
            setTest32(cond, address, mask, dest);
        }

        void lea(Address address, RegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.add_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.add_r(address.base, SparcRegisters::g3, dest);
            }
        }

        void lea(BaseIndex address, RegisterID dest)
        {
            // lea only used by PolyIC.
            // PolyIC is not enabled for sparc now.
            ASSERT(0);
        }

        void add32(Imm32 imm, AbsoluteAddress address)
        {
            load32(address.m_ptr, SparcRegisters::g2);
            add32(imm, SparcRegisters::g2);
            store32(SparcRegisters::g2, address.m_ptr);
        }

        void sub32(TrustedImm32 imm, AbsoluteAddress address)
        {
            load32(address.m_ptr, SparcRegisters::g2);
            sub32(imm, SparcRegisters::g2);
            store32(SparcRegisters::g2, address.m_ptr);
        }

        void load32(void* address, RegisterID dest)
        {
            m_assembler.move_nocheck((int)address, SparcRegisters::g3);
            m_assembler.lduw_r(SparcRegisters::g3, SparcRegisters::g0, dest);
        }

        Jump branch32(Condition cond, AbsoluteAddress left, RegisterID right)
        {
            load32(left.m_ptr, SparcRegisters::g2);
            return branch32(cond, SparcRegisters::g2, right);
        }

        Jump branch32(Condition cond, AbsoluteAddress left, TrustedImm32 right)
        {
            load32(left.m_ptr, SparcRegisters::g2);
            return branch32(cond, SparcRegisters::g2, right);
        }

        Call call()
        {
            m_assembler.rdpc(SparcRegisters::g2);
            m_assembler.add_imm(SparcRegisters::g2, 32, SparcRegisters::g2);
            m_assembler.stw_imm(SparcRegisters::g2, SparcRegisters::fp, -8);
            Call cl = Call(m_assembler.call(), Call::Linkable);
            m_assembler.lduw_imm(SparcRegisters::fp, -8, SparcRegisters::g2);
            m_assembler.jmpl_imm(SparcRegisters::g2, 0, SparcRegisters::g0);
            m_assembler.nop();
            return cl;
        }

        Call tailRecursiveCall()
        {
            return Call::fromTailJump(jump());
        }

        Call makeTailRecursiveCall(Jump oldJump)
        {
            return Call::fromTailJump(oldJump);
        }

        DataLabelPtr moveWithPatch(TrustedImmPtr initialValue, RegisterID dest)
        {
            DataLabelPtr dataLabel(this);
            Imm32 imm = Imm32(initialValue);
            m_assembler.move_nocheck(imm.m_value, dest);
            return dataLabel;
        }

        DataLabel32 moveWithPatch(TrustedImm32 initialValue, RegisterID dest)
        {
            DataLabel32 dataLabel(this);
            m_assembler.move_nocheck(initialValue.m_value, dest);
            return dataLabel;
        }

        Jump branchPtrWithPatch(Condition cond, RegisterID left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
        {
            dataLabel = moveWithPatch(initialRightValue, SparcRegisters::g2);
            Jump jump = branch32(cond, left, SparcRegisters::g2);
            return jump;
        }

        Jump branchPtrWithPatch(Condition cond, Address left, DataLabelPtr& dataLabel, ImmPtr initialRightValue = ImmPtr(0))
        {
            load32(left, SparcRegisters::g2);
            dataLabel = moveWithPatch(initialRightValue, SparcRegisters::g3);
            Jump jump = branch32(cond, SparcRegisters::g3, SparcRegisters::g2);
            return jump;
        }

        DataLabelPtr storePtrWithPatch(TrustedImmPtr initialValue, ImplicitAddress address)
        {
            DataLabelPtr dataLabel = moveWithPatch(initialValue, SparcRegisters::g2);
            store32(SparcRegisters::g2, address);
            return dataLabel;
        }

        DataLabelPtr storePtrWithPatch(ImplicitAddress address)
        {
            return storePtrWithPatch(ImmPtr(0), address);
        }

        // Floating point operators
        bool supportsFloatingPoint() const
        {
            return true;
        }

        bool supportsFloatingPointTruncate() const
        {
            return true;
        }

        bool supportsFloatingPointSqrt() const
        {
            return true;
        }

        void moveDouble(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.fmovd_r(src, dest);
        }

        void loadFloat(BaseIndex address, FPRegisterID dest)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.ldf_r(address.base, SparcRegisters::g2, dest);
            m_assembler.fstod_r(dest, dest);
        }

        void loadFloat(ImplicitAddress address, FPRegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.ldf_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.ldf_r(address.base, SparcRegisters::g3, dest);
            }
            m_assembler.fstod_r(dest, dest);
        }

        void loadFloat(const void* address, FPRegisterID dest)
        {
            m_assembler.move_nocheck((int)address, SparcRegisters::g3);
            m_assembler.ldf_r(SparcRegisters::g3, SparcRegisters::g0, dest);
            m_assembler.fstod_r(dest, dest);
        }

        void loadDouble(BaseIndex address, FPRegisterID dest)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.ldf_r(address.base, SparcRegisters::g2, dest);
            m_assembler.add_imm(SparcRegisters::g2, 4, SparcRegisters::g2);
            m_assembler.ldf_r(address.base, SparcRegisters::g2, dest + 1);
        }

        void loadDouble(ImplicitAddress address, FPRegisterID dest)
        {
            m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
            m_assembler.ldf_r(address.base, SparcRegisters::g3, dest);
            m_assembler.add_imm(SparcRegisters::g3, 4, SparcRegisters::g3);
            m_assembler.ldf_r(address.base, SparcRegisters::g3, dest + 1);
        }

        DataLabelPtr loadDouble(const void* address, FPRegisterID dest)
        {
            DataLabelPtr dataLabel(this);
            m_assembler.move_nocheck((int)address, SparcRegisters::g3);
            m_assembler.ldf_imm(SparcRegisters::g3, 0, dest);
            m_assembler.ldf_imm(SparcRegisters::g3, 4, dest + 1);
            return dataLabel;
        }

        void storeFloat(FPRegisterID src, BaseIndex address)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.stf_r(src, address.base, SparcRegisters::g2);
        }

        void storeFloat(FPRegisterID src, ImplicitAddress address)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.stf_imm(src, address.base, address.offset);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.stf_r(src, address.base, SparcRegisters::g3);
            }
        }

        void storeFloat(ImmDouble imm, Address address)
        {
            union {
                float f;
                uint32_t u32;
            } u;
            u.f = imm.u.d;
            store32(Imm32(u.u32), address);
        }

        void storeFloat(ImmDouble imm, BaseIndex address)
        {
            union {
                float f;
                uint32_t u32;
            } u;
            u.f = imm.u.d;
            store32(Imm32(u.u32), address);
        }

        void storeDouble(FPRegisterID src, BaseIndex address)
        {
            m_assembler.sll_imm(address.index, address.scale, SparcRegisters::g2);
            add32(Imm32(address.offset), SparcRegisters::g2);
            m_assembler.stf_r(src, address.base, SparcRegisters::g2);
            m_assembler.add_imm(SparcRegisters::g2, 4, SparcRegisters::g2);
            m_assembler.stf_r(src + 1, address.base, SparcRegisters::g2);
        }

        void storeDouble(FPRegisterID src, ImplicitAddress address)
        {
            if (m_assembler.isimm13(address.offset + 4)) {
                m_assembler.stf_imm(src, address.base, address.offset);
                m_assembler.stf_imm(src + 1, address.base, address.offset + 4);
            } else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.stf_r(src, address.base, SparcRegisters::g3);
                m_assembler.add_imm(SparcRegisters::g3, 4, SparcRegisters::g3);
                m_assembler.stf_r(src + 1, address.base, SparcRegisters::g3);
            }
        }

        void storeDouble(ImmDouble imm, Address address)
        {
            store32(Imm32(imm.u.s.msb), address);
            store32(Imm32(imm.u.s.lsb), Address(address.base, address.offset + 4));
        }

        void storeDouble(ImmDouble imm, BaseIndex address)
        {
            store32(Imm32(imm.u.s.msb), address);
            store32(Imm32(imm.u.s.lsb),
                    BaseIndex(address.base, address.index, address.scale, address.offset + 4));
        }

        void addDouble(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.faddd_r(src, dest, dest);
        }

        void addDouble(Address src, FPRegisterID dest)
        {
            loadDouble(src, SparcRegisters::f30);
            m_assembler.faddd_r(SparcRegisters::f30, dest, dest);
        }

        void divDouble(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.fdivd_r(dest, src, dest);
        }

        void divDouble(Address src, FPRegisterID dest)
        {
            loadDouble(src, SparcRegisters::f30);
            m_assembler.fdivd_r(dest, SparcRegisters::f30, dest);
        }

        void subDouble(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.fsubd_r(dest, src, dest);
        }

        void subDouble(Address src, FPRegisterID dest)
        {
            loadDouble(src, SparcRegisters::f30);
            m_assembler.fsubd_r(dest, SparcRegisters::f30, dest);
        }

        void mulDouble(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.fmuld_r(src, dest, dest);
        }

        void mulDouble(Address src, FPRegisterID dest)
        {
            loadDouble(src, SparcRegisters::f30);
            m_assembler.fmuld_r(SparcRegisters::f30, dest, dest);
        }

        void absDouble(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.fabsd_r(src, dest);
        }

        void sqrtDouble(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.fsqrtd_r(src, dest);
        }

        void negDouble(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.fnegd_r(src, dest);
        }

        void convertUInt32ToDouble(RegisterID src, FPRegisterID dest)
        {
            m_assembler.move_nocheck(0x43300000, SparcRegisters::g1);
            m_assembler.stw_imm(SparcRegisters::g1, SparcRegisters::sp, 0x60);
            m_assembler.stw_imm(src, SparcRegisters::sp, 0x64);
            m_assembler.ldf_imm(SparcRegisters::sp, 0x60, SparcRegisters::f30);
            m_assembler.ldf_imm(SparcRegisters::sp, 0x64, SparcRegisters::f31);
            m_assembler.stw_imm(SparcRegisters::g0, SparcRegisters::sp, 0x64);
            m_assembler.ldf_imm(SparcRegisters::sp, 0x60, dest);
            m_assembler.ldf_imm(SparcRegisters::sp, 0x64, dest + 1);
            m_assembler.fsubd_r(SparcRegisters::f30, dest, dest);
            m_assembler.fabss_r(dest, dest);
        }

        void convertInt32ToDouble(RegisterID src, FPRegisterID dest)
        {
            m_assembler.stw_imm(src, SparcRegisters::sp, 0x60);
            m_assembler.ldf_imm(SparcRegisters::sp, 0x60, dest);
            m_assembler.fitod_r(dest, dest);
        }

        void convertInt32ToDouble(Address address, FPRegisterID dest)
        {
            if (m_assembler.isimm13(address.offset))
                m_assembler.ldf_imm(address.base, address.offset, dest);
            else {
                m_assembler.move_nocheck(address.offset, SparcRegisters::g3);
                m_assembler.ldf_r(address.base, SparcRegisters::g3, dest);
            }
            m_assembler.fitod_r(dest, dest);
        }

        void convertInt32ToDouble(AbsoluteAddress src, FPRegisterID dest)
        {
            m_assembler.move_nocheck((int)src.m_ptr, SparcRegisters::g3);
            m_assembler.ldf_r(SparcRegisters::g3, SparcRegisters::g0, dest);
            m_assembler.fitod_r(dest, dest);
        }

        void fastLoadDouble(RegisterID lo, RegisterID hi, FPRegisterID fpReg)
        {
            m_assembler.stw_imm(lo, SparcRegisters::sp, 0x64);
            m_assembler.stw_imm(hi, SparcRegisters::sp, 0x60);
            m_assembler.ldf_imm(SparcRegisters::sp, 0x60, fpReg);
            m_assembler.ldf_imm(SparcRegisters::sp, 0x64, fpReg + 1);
        }

        void convertDoubleToFloat(FPRegisterID src, FPRegisterID dest)
        {
            m_assembler.fdtos_r(src, dest);
        }

        void breakDoubleTo32(FPRegisterID srcDest, RegisterID typeReg, RegisterID dataReg) {
            // We don't assume stack is aligned to 8.
            // Always using stf, ldf instead of stdf, lddf.
            m_assembler.stf_imm(srcDest, SparcRegisters::sp, 0x60);
            m_assembler.stf_imm(srcDest + 1, SparcRegisters::sp, 0x64);
            m_assembler.lduw_imm(SparcRegisters::sp, 0x60, typeReg);
            m_assembler.lduw_imm(SparcRegisters::sp, 0x64, dataReg);
        }

        Jump branchDouble(DoubleCondition cond, FPRegisterID left, FPRegisterID right)
        {
            m_assembler.fcmpd_r(left, right);
            return Jump(m_assembler.fbranch(SparcDoubleCondition(cond)));
        }

        // Truncates 'src' to an integer, and places the resulting 'dest'.
        // If the result is not representable as a 32 bit value, branch.
        // May also branch for some values that are representable in 32 bits
        // (specifically, in this case, INT_MIN).
        Jump branchTruncateDoubleToInt32(FPRegisterID src, RegisterID dest)
        {
            m_assembler.fdtoi_r(src, SparcRegisters::f30);
            m_assembler.stf_imm(SparcRegisters::f30, SparcRegisters::sp, 0x60);
            m_assembler.lduw_imm(SparcRegisters::sp, 0x60, dest);

            m_assembler.or_r(SparcRegisters::g0, SparcRegisters::g0, SparcRegisters::g2);
            m_assembler.move_nocheck(0x80000000, SparcRegisters::g3);
            m_assembler.subcc_r(SparcRegisters::g3, dest, SparcRegisters::g0);
            m_assembler.movcc_imm(1, SparcRegisters::g2, SparcCondition(Equal));
            m_assembler.move_nocheck(0x7fffffff, SparcRegisters::g3);
            m_assembler.subcc_r(SparcRegisters::g3, dest, SparcRegisters::g0);
            m_assembler.movcc_imm(1, SparcRegisters::g2, SparcCondition(Equal));

            return branch32(Equal, SparcRegisters::g2, Imm32(1));
        }

        // Convert 'src' to an integer, and places the resulting 'dest'.
        // If the result is not representable as a 32 bit value, branch.
        // May also branch for some values that are representable in 32 bits
        // (specifically, in this case, 0).
        void branchConvertDoubleToInt32(FPRegisterID src, RegisterID dest, JumpList& failureCases, FPRegisterID fpTemp)
        {
            m_assembler.fdtoi_r(src, SparcRegisters::f30);
            m_assembler.stf_imm(SparcRegisters::f30, SparcRegisters::sp, 0x60);
            m_assembler.lduw_imm(SparcRegisters::sp, 0x60, dest);

            // Convert the integer result back to float & compare to the original value - if not equal or unordered (NaN) then jump.
            m_assembler.fitod_r(SparcRegisters::f30, SparcRegisters::f30);
            failureCases.append(branchDouble(DoubleNotEqualOrUnordered, src, SparcRegisters::f30));

            // If the result is zero, it might have been -0.0, and 0.0 equals to -0.0
            failureCases.append(branchTest32(Zero, dest));
        }

        void zeroDouble(FPRegisterID srcDest)
        {
            fastLoadDouble(SparcRegisters::g0, SparcRegisters::g0, srcDest);
        }

    protected:
        SparcAssembler::Condition SparcCondition(Condition cond)
        {
            return static_cast<SparcAssembler::Condition>(cond);
        }

        SparcAssembler::DoubleCondition SparcDoubleCondition(DoubleCondition cond)
        {
            return static_cast<SparcAssembler::DoubleCondition>(cond);
        }

    private:
        friend class LinkBuffer;
        friend class RepatchBuffer;

        static void linkCall(void* code, Call call, FunctionPtr function)
        {
            SparcAssembler::linkCall(code, call.m_jmp, function.value());
        }

        static void repatchCall(CodeLocationCall call, CodeLocationLabel destination)
        {
            SparcAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
        }

        static void repatchCall(CodeLocationCall call, FunctionPtr destination)
        {
            SparcAssembler::relinkCall(call.dataLocation(), destination.executableAddress());
        }

    };

}


#endif // ENABLE(ASSEMBLER) && CPU(SPARC)

#endif // MacroAssemblerSparc_h
