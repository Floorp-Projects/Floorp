/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifndef vmtypes_h
#define vmtypes_h

#include <vector>
#include "world.h"
#include "numerics.h" /* needed for formatter << double */
#include "jstypes.h"

namespace JavaScript {
namespace VM {

    using JSTypes::JSValues;

    enum ICodeOp {
        ADD, /* dest, source1, source2 */
        BRANCH, /* target label */
        BRANCH_EQ, /* target label, condition */
        BRANCH_GE, /* target label, condition */
        BRANCH_GT, /* target label, condition */
        BRANCH_LE, /* target label, condition */
        BRANCH_LT, /* target label, condition */
        BRANCH_NE, /* target label, condition */
        CALL, /* result, target, args */
        COMPARE_EQ, /* dest, source */
        COMPARE_GE, /* dest, source */
        COMPARE_GT, /* dest, source */
        COMPARE_LE, /* dest, source */
        COMPARE_LT, /* dest, source */
        COMPARE_NE, /* dest, source */
        DIVIDE, /* dest, source1, source2 */
        ENDTRY, /* mmm, there is no try, only do */
        GET_ELEMENT, /* dest, array, index */
        GET_PROP, /* dest, object, prop name */
        JSR, /* target */
        LOAD_IMMEDIATE, /* dest, immediate value (double) */
        LOAD_NAME, /* dest, name */
        MOVE, /* dest, source */
        MULTIPLY, /* dest, source1, source2 */
        NEW_ARRAY, /* dest */
        NEW_OBJECT, /* dest */
        NOP, /* do nothing and like it */
        NOT, /* dest, source */
        RETURN, /* return value */
        RETURN_VOID, /* Return without a value */
        RTS, /* Return to sender */
        SAVE_NAME, /* name, source */
        SET_ELEMENT, /* base, source1, source2 */
        SET_PROP, /* object, name, source */
        SUBTRACT, /* dest, source1, source2 */
        THROW, /* exception value */
        TRY /* catch target, finally target */
    };
    
    /********************************************************************/
   
    static char *opcodeNames[] = {
        "ADD           ",
        "BRANCH        ",
        "BRANCH_EQ     ",
        "BRANCH_GE     ",
        "BRANCH_GT     ",
        "BRANCH_LE     ",
        "BRANCH_LT     ",
        "BRANCH_NE     ",
        "CALL          ",
        "COMPARE_EQ    ",
        "COMPARE_GE    ",
        "COMPARE_GT    ",
        "COMPARE_LE    ",
        "COMPARE_LT    ",
        "COMPARE_NE    ",
        "DIVIDE        ",
        "ENDTRY        ",
        "GET_ELEMENT   ",
        "GET_PROP      ",
        "JSR           ",
        "LOAD_IMMEDIATE",
        "LOAD_NAME     ",
        "MOVE          ",
        "MULTIPLY      ",
        "NEW_ARRAY     ",
        "NEW_OBJECT    ",
        "NOP           ",
        "NOT           ",
        "RETURN        ",
        "RETURN_VOID   ",
        "RTS           ",
        "SAVE_NAME     ",
        "SET_ELEMENT   ",
        "SET_PROP      ",
        "SUBTRACT      ",
        "THROW         ",
        "TRY           ",
    };

    /********************************************************************/

    /* super-class for all instructions */
    class Instruction
    {
    public:
        Instruction(ICodeOp aOpcode) : mOpcode(aOpcode) { }
        virtual Formatter& print(Formatter& f)
        {
            f << opcodeNames[mOpcode] << "\t<unk>";
            return f;
        }
        
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/)
        {
            return f;
        }
        
        ICodeOp getBranchOp()
        {
            return ((mOpcode >= COMPARE_EQ) && (mOpcode <= COMPARE_NE)) ?
              (ICodeOp)(BRANCH_EQ + (mOpcode - COMPARE_EQ)) : NOP;
        }
        
        ICodeOp op() { return mOpcode; }
        
        virtual int32 count() { return 0; }
        
    protected:
        ICodeOp mOpcode;
        
    };

    /********************************************************************/

    enum { NotARegister = 0xFFFFFFFF };
    enum { NotALabel = 0xFFFFFFFF };
    enum { NotAnOffset = 0xFFFFFFFF };
    enum { NotABanana = 0xFFFFFFFF };
    
    /********************************************************************/
    
    typedef uint32 Register;
    typedef std::vector<Register> RegisterList;        
    typedef std::vector<Instruction *> InstructionStream;
    typedef InstructionStream::iterator InstructionIterator;
    
    /**
     * Helper to print Call operands.
     */
    struct ArgList {
        const RegisterList& mList;
        const JSValues& mRegisters;
        ArgList(const RegisterList& rl, const JSValues& registers)
            :   mList(rl), mRegisters(registers) {}
    };        
    
    /********************************************************************/
    
    Formatter& operator<< (Formatter& f, Instruction& i);
    Formatter& operator<< (Formatter& f, RegisterList& rl);
    Formatter& operator<< (Formatter& f, const ArgList& al);
    
    /********************************************************************/
    
    class Label {
    public:
        Label(InstructionStream* aBase) :
            mBase(aBase), mOffset(NotALabel) {}
        
        InstructionStream *mBase;
        uint32 mOffset;
    };
    
    typedef std::vector<Label *> LabelList;
    typedef LabelList::iterator LabelIterator;
    
    /********************************************************************/
    
    /* 1, 2 and 3 operand opcode templates */
    
    template <typename Operand1>
    class Instruction_1 : public Instruction {
    public:
        Instruction_1(ICodeOp aOpcode, Operand1 aOp1) : 
            Instruction(aOpcode), mOp1(aOp1) { }            
        Operand1& o1() { return mOp1; }

        virtual int32 count() { return 1; }
        
    protected:
        Operand1 mOp1;
    };
    
    template <typename Operand1, typename Operand2>
    class Instruction_2 : public Instruction {
    public:
        Instruction_2(ICodeOp aOpcode, Operand1 aOp1, Operand2 aOp2) :
            Instruction(aOpcode), mOp1(aOp1), mOp2(aOp2) {}
        Operand1& o1() { return mOp1; }
        Operand2& o2() { return mOp2; }

        virtual int32 count() { return 2; }
        
    protected:
        Operand1 mOp1;
        Operand2 mOp2;
    };
    
    template <typename Operand1, typename Operand2, typename Operand3>
    class Instruction_3 : public Instruction {
    public:
        Instruction_3(ICodeOp aOpcode, Operand1 aOp1, Operand2 aOp2,
                      Operand3 aOp3) :
            Instruction(aOpcode), mOp1(aOp1), mOp2(aOp2), mOp3(aOp3) { }
        Operand1& o1() { return mOp1; }
        Operand2& o2() { return mOp2; }
        Operand3& o3() { return mOp3; }

        virtual int32 count() { return 3; }
        
    protected:
        Operand1 mOp1;
        Operand2 mOp2;
        Operand3 mOp3;
    };
    
    /********************************************************************/

    /* Instruction groups */
    
    class Arithmetic : public Instruction_3<Register, Register, Register> {
    public:
        Arithmetic (ICodeOp aOpcode, Register aDest, Register aSrc1,
                    Register aSrc2) :
            Instruction_3<Register, Register, Register>(aOpcode, aDest, aSrc1,
                                                        aSrc2) {}
        virtual Formatter& print(Formatter& f)
        {
            f << opcodeNames[mOpcode] << "\tR" << mOp1 << ", R" << mOp2 << ", R" << mOp3;
            return f;
        }
        
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers)
        {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2] << ", " << "R" << mOp3 << '=' << registers[mOp3];
            return f;
        }
    };
    
    class Compare : public Instruction_2<Register, Register> {
    public:
        Compare(ICodeOp aOpcode, Register aDest, Register aSrc) :
            Instruction_2<Register, Register>(aOpcode, aDest, aSrc) {}
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[mOpcode] << "\tR" << mOp1 << ", R" << mOp2;
            return f;
        }

        virtual Formatter& printOperands(Formatter& f, const JSValues& registers)
        {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };
    
    class GenericBranch : public Instruction_2<Label*, Register> {
    public:
        GenericBranch (ICodeOp aOpcode, Label* aLabel, 
                       Register aR = NotARegister) :
            Instruction_2<Label*, Register>(aOpcode, aLabel, aR) {}
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[mOpcode] << "\tOffset " << mOp1->mOffset;
            if (mOp2 == NotARegister) {
                f << ", R~";
            } else {
                f << ", R" << mOp2;
            }
            return f;
        }

        virtual Formatter& printOperands(Formatter& f, const JSValues& registers)
        {
            if (mOp2 != NotARegister)
                f << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }

        void resolveTo (uint32 aOffset) { mOp1->mOffset = aOffset; }
        uint32 getOffset() { return mOp1->mOffset; }
    };

    /********************************************************************/
    
    /* Specific opcodes */

    class Add : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Add (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (ADD, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Branch : public GenericBranch {
    public:
        /* target label */
        Branch (Label* aOp1) :
            GenericBranch
            (BRANCH, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[BRANCH] << "\t" << "Offset " << mOp1->mOffset;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class BranchEQ : public GenericBranch {
    public:
        /* target label, condition */
        BranchEQ (Label* aOp1, Register aOp2) :
            GenericBranch
            (BRANCH_EQ, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class BranchGE : public GenericBranch {
    public:
        /* target label, condition */
        BranchGE (Label* aOp1, Register aOp2) :
            GenericBranch
            (BRANCH_GE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class BranchGT : public GenericBranch {
    public:
        /* target label, condition */
        BranchGT (Label* aOp1, Register aOp2) :
            GenericBranch
            (BRANCH_GT, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class BranchLE : public GenericBranch {
    public:
        /* target label, condition */
        BranchLE (Label* aOp1, Register aOp2) :
            GenericBranch
            (BRANCH_LE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class BranchLT : public GenericBranch {
    public:
        /* target label, condition */
        BranchLT (Label* aOp1, Register aOp2) :
            GenericBranch
            (BRANCH_LT, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class BranchNE : public GenericBranch {
    public:
        /* target label, condition */
        BranchNE (Label* aOp1, Register aOp2) :
            GenericBranch
            (BRANCH_NE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class Call : public Instruction_3<Register, Register, RegisterList> {
    public:
        /* result, target, args */
        Call (Register aOp1, Register aOp2, RegisterList aOp3) :
            Instruction_3<Register, Register, RegisterList>
            (CALL, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[CALL] << "\t" << "R" << mOp1 << ", " << "R" << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2] << ", " << ArgList(mOp3, registers);
            return f;
        }
    };

    class CompareEQ : public Compare {
    public:
        /* dest, source */
        CompareEQ (Register aOp1, Register aOp2) :
            Compare
            (COMPARE_EQ, aOp1, aOp2) {};
        /* print() and printOperands() inherited from Compare */
    };

    class CompareGE : public Compare {
    public:
        /* dest, source */
        CompareGE (Register aOp1, Register aOp2) :
            Compare
            (COMPARE_GE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from Compare */
    };

    class CompareGT : public Compare {
    public:
        /* dest, source */
        CompareGT (Register aOp1, Register aOp2) :
            Compare
            (COMPARE_GT, aOp1, aOp2) {};
        /* print() and printOperands() inherited from Compare */
    };

    class CompareLE : public Compare {
    public:
        /* dest, source */
        CompareLE (Register aOp1, Register aOp2) :
            Compare
            (COMPARE_LE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from Compare */
    };

    class CompareLT : public Compare {
    public:
        /* dest, source */
        CompareLT (Register aOp1, Register aOp2) :
            Compare
            (COMPARE_LT, aOp1, aOp2) {};
        /* print() and printOperands() inherited from Compare */
    };

    class CompareNE : public Compare {
    public:
        /* dest, source */
        CompareNE (Register aOp1, Register aOp2) :
            Compare
            (COMPARE_NE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from Compare */
    };

    class Divide : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Divide (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (DIVIDE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Endtry : public Instruction {
    public:
        /* mmm, there is no try, only do */
        Endtry () :
            Instruction
            (ENDTRY) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[ENDTRY];
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class GetElement : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, array, index */
        GetElement (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (GET_ELEMENT, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[GET_ELEMENT] << "\t" << "R" << mOp1 << ", " << "R" << mOp2 << ", " << "R" << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2] << ", " << "R" << mOp3 << '=' << registers[mOp3];
            return f;
        }
    };

    class GetProp : public Instruction_3<Register, Register, StringAtom*> {
    public:
        /* dest, object, prop name */
        GetProp (Register aOp1, Register aOp2, StringAtom* aOp3) :
            Instruction_3<Register, Register, StringAtom*>
            (GET_PROP, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[GET_PROP] << "\t" << "R" << mOp1 << ", " << "R" << mOp2 << ", " << "'" << *mOp3 << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };

    class Jsr : public GenericBranch {
    public:
        /* target */
        Jsr (Label* aOp1) :
            GenericBranch
            (JSR, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[JSR] << "\t" << "Offset " << mOp1->mOffset;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class LoadImmediate : public Instruction_2<Register, double> {
    public:
        /* dest, immediate value (double) */
        LoadImmediate (Register aOp1, double aOp2) :
            Instruction_2<Register, double>
            (LOAD_IMMEDIATE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[LOAD_IMMEDIATE] << "\t" << "R" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class LoadName : public Instruction_2<Register, StringAtom*> {
    public:
        /* dest, name */
        LoadName (Register aOp1, StringAtom* aOp2) :
            Instruction_2<Register, StringAtom*>
            (LOAD_NAME, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[LOAD_NAME] << "\t" << "R" << mOp1 << ", " << "'" << *mOp2 << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class Move : public Instruction_2<Register, Register> {
    public:
        /* dest, source */
        Move (Register aOp1, Register aOp2) :
            Instruction_2<Register, Register>
            (MOVE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[MOVE] << "\t" << "R" << mOp1 << ", " << "R" << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };

    class Multiply : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Multiply (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (MULTIPLY, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class NewArray : public Instruction_1<Register> {
    public:
        /* dest */
        NewArray (Register aOp1) :
            Instruction_1<Register>
            (NEW_ARRAY, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NEW_ARRAY] << "\t" << "R" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class NewObject : public Instruction_1<Register> {
    public:
        /* dest */
        NewObject (Register aOp1) :
            Instruction_1<Register>
            (NEW_OBJECT, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NEW_OBJECT] << "\t" << "R" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class Nop : public Instruction {
    public:
        /* do nothing and like it */
        Nop () :
            Instruction
            (NOP) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NOP];
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class Not : public Instruction_2<Register, Register> {
    public:
        /* dest, source */
        Not (Register aOp1, Register aOp2) :
            Instruction_2<Register, Register>
            (NOT, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NOT] << "\t" << "R" << mOp1 << ", " << "R" << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };

    class Return : public Instruction_1<Register> {
    public:
        /* return value */
        Return (Register aOp1) :
            Instruction_1<Register>
            (RETURN, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[RETURN] << "\t" << "R" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class ReturnVoid : public Instruction {
    public:
        /* Return without a value */
        ReturnVoid () :
            Instruction
            (RETURN_VOID) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[RETURN_VOID];
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class Rts : public Instruction {
    public:
        /* Return to sender */
        Rts () :
            Instruction
            (RTS) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[RTS];
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class SaveName : public Instruction_2<StringAtom*, Register> {
    public:
        /* name, source */
        SaveName (StringAtom* aOp1, Register aOp2) :
            Instruction_2<StringAtom*, Register>
            (SAVE_NAME, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SAVE_NAME] << "\t" << "'" << *mOp1 << "'" << ", " << "R" << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };

    class SetElement : public Instruction_3<Register, Register, Register> {
    public:
        /* base, source1, source2 */
        SetElement (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (SET_ELEMENT, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SET_ELEMENT] << "\t" << "R" << mOp1 << ", " << "R" << mOp2 << ", " << "R" << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2] << ", " << "R" << mOp3 << '=' << registers[mOp3];
            return f;
        }
    };

    class SetProp : public Instruction_3<Register, StringAtom*, Register> {
    public:
        /* object, name, source */
        SetProp (Register aOp1, StringAtom* aOp2, Register aOp3) :
            Instruction_3<Register, StringAtom*, Register>
            (SET_PROP, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SET_PROP] << "\t" << "R" << mOp1 << ", " << "'" << *mOp2 << "'" << ", " << "R" << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp3 << '=' << registers[mOp3];
            return f;
        }
    };

    class Subtract : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Subtract (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (SUBTRACT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Throw : public Instruction_1<Register> {
    public:
        /* exception value */
        Throw (Register aOp1) :
            Instruction_1<Register>
            (THROW, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[THROW] << "\t" << "R" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class Try : public Instruction_2<Label*, Label*> {
    public:
        /* catch target, finally target */
        Try (Label* aOp1, Label* aOp2) :
            Instruction_2<Label*, Label*>
            (TRY, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[TRY] << "\t" << "Offset " << mOp1->mOffset << ", " << "Offset " << mOp2->mOffset;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

} /* namespace VM */

} /* namespace JavaScript */

#endif /* vmtypes_h */
