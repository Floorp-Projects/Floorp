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

#include "numerics.h" /* needed for formatter << double */
#include "jstypes.h"
#include "world.h"
#include <vector>

namespace JavaScript {
namespace VM {

    using JSTypes::JSValue;
    using JSTypes::JSValues;
    using JSTypes::JSString;

    enum ICodeOp {
        ADD, /* dest, source1, source2 */
        AND, /* dest, source1, source2 */
        BITNOT, /* dest, source */
        BRANCH, /* target label */
        BRANCH_FALSE, /* target label, condition */
        BRANCH_TRUE, /* target label, condition */
        CALL, /* result, target, args */
        COMPARE_EQ, /* dest, source1, source2 */
        COMPARE_GE, /* dest, source1, source2 */
        COMPARE_GT, /* dest, source1, source2 */
        COMPARE_IN, /* dest, source1, source2 */
        COMPARE_LE, /* dest, source1, source2 */
        COMPARE_LT, /* dest, source1, source2 */
        COMPARE_NE, /* dest, source1, source2 */
        DIVIDE, /* dest, source1, source2 */
        ELEM_XCR, /* dest, base, index, value */
        GET_ELEMENT, /* dest, base, index */
        GET_PROP, /* dest, object, prop name */
        INSTANCEOF, /* dest, source1, source2 */
        JSR, /* target */
        LOAD_IMMEDIATE, /* dest, immediate value (double) */
        LOAD_NAME, /* dest, name */
        LOAD_STRING, /* dest, immediate value (string) */
        LOAD_VALUE, /* dest, immediate value (JSValue) */
        MOVE, /* dest, source */
        MULTIPLY, /* dest, source1, source2 */
        NAME_XCR, /* dest, name, value */
        NEGATE, /* dest, source */
        NEW_ARRAY, /* dest */
        NEW_OBJECT, /* dest */
        NOP, /* do nothing and like it */
        NOT, /* dest, source */
        OR, /* dest, source1, source2 */
        POSATE, /* dest, source */
        PROP_XCR, /* dest, source, name, value */
        REMAINDER, /* dest, source1, source2 */
        RETURN, /* return value */
        RETURN_VOID, /* Return without a value */
        RTS, /* Return to sender */
        SAVE_NAME, /* name, source */
        SET_ELEMENT, /* base, index, value */
        SET_PROP, /* object, name, source */
        SHIFTLEFT, /* dest, source1, source2 */
        SHIFTRIGHT, /* dest, source1, source2 */
        STRICT_EQ, /* dest, source1, source2 */
        STRICT_NE, /* dest, source1, source2 */
        SUBTRACT, /* dest, source1, source2 */
        TEST, /* dest, source */
        THROW, /* exception value */
        TRYIN, /* catch target, finally target */
        TRYOUT, /* mmm, there is no try, only do */
        USHIFTRIGHT, /* dest, source1, source2 */
        VAR_XCR, /* dest, source, value */
        WITHIN, /* within this object */
        WITHOUT, /* without this object */
        XOR, /* dest, source1, source2 */
    };



    /********************************************************************/

    static char *opcodeNames[] = {
        "ADD           ",
        "AND           ",
        "BITNOT        ",
        "BRANCH        ",
        "BRANCH_FALSE  ",
        "BRANCH_TRUE   ",
        "CALL          ",
        "COMPARE_EQ    ",
        "COMPARE_GE    ",
        "COMPARE_GT    ",
        "COMPARE_IN    ",
        "COMPARE_LE    ",
        "COMPARE_LT    ",
        "COMPARE_NE    ",
        "DIVIDE        ",
        "ELEM_XCR      ",
        "GET_ELEMENT   ",
        "GET_PROP      ",
        "INSTANCEOF    ",
        "JSR           ",
        "LOAD_IMMEDIATE",
        "LOAD_NAME     ",
        "LOAD_STRING   ",
        "LOAD_VALUE    ",
        "MOVE          ",
        "MULTIPLY      ",
        "NAME_XCR      ",
        "NEGATE        ",
        "NEW_ARRAY     ",
        "NEW_OBJECT    ",
        "NOP           ",
        "NOT           ",
        "OR            ",
        "POSATE        ",
        "PROP_XCR      ",
        "REMAINDER     ",
        "RETURN        ",
        "RETURN_VOID   ",
        "RTS           ",
        "SAVE_NAME     ",
        "SET_ELEMENT   ",
        "SET_PROP      ",
        "SHIFTLEFT     ",
        "SHIFTRIGHT    ",
        "STRICT_EQ     ",
        "STRICT_NE     ",
        "SUBTRACT      ",
        "TEST          ",
        "THROW         ",
        "TRYIN         ",
        "TRYOUT        ",
        "USHIFTRIGHT   ",
        "VAR_XCR       ",
        "WITHIN        ",
        "WITHOUT       ",
        "XOR           ",
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
    typedef std::map<String, Register, std::less<String> > VariableMap;
    
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
    Formatter& operator<< (Formatter& f, InstructionStream& is);
    
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
    
    template <typename Operand1, typename Operand2, typename Operand3, typename Operand4>
    class Instruction_4 : public Instruction {
    public:
        Instruction_4(ICodeOp aOpcode, Operand1 aOp1, Operand2 aOp2,
                      Operand3 aOp3, Operand4 aOp4) :
            Instruction(aOpcode), mOp1(aOp1), mOp2(aOp2), mOp3(aOp3), mOp4(aOp4) { }
        Operand1& o1() { return mOp1; }
        Operand2& o2() { return mOp2; }
        Operand3& o3() { return mOp3; }
        Operand4& o4() { return mOp4; }

        virtual int32 count() { return 4; }
        
    protected:
        Operand1 mOp1;
        Operand2 mOp2;
        Operand3 mOp3;
        Operand4 mOp4;
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
    
    class Unary : public Instruction_2<Register, Register> {
    public:
        Unary(ICodeOp aOpcode, Register aDest, Register aSrc) :
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
        void setTarget(Label *label) { mOp1 = label; }
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

    class And : public Arithmetic {
    public:
        /* dest, source1, source2 */
        And (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (AND, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Bitnot : public Instruction_2<Register, Register> {
    public:
        /* dest, source */
        Bitnot (Register aOp1, Register aOp2) :
            Instruction_2<Register, Register>
            (BITNOT, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[BITNOT] << "\t" << "R" << mOp1 << ", " << "R" << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };

    class Branch : public GenericBranch {
    public:
        /* target label */
        Branch (Label* aOp1) :
            GenericBranch
            (BRANCH, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[BRANCH] << "\t" << "Offset " << ((mOp1) ? mOp1->mOffset : NotAnOffset);
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class BranchFalse : public GenericBranch {
    public:
        /* target label, condition */
        BranchFalse (Label* aOp1, Register aOp2) :
            GenericBranch
            (BRANCH_FALSE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class BranchTrue : public GenericBranch {
    public:
        /* target label, condition */
        BranchTrue (Label* aOp1, Register aOp2) :
            GenericBranch
            (BRANCH_TRUE, aOp1, aOp2) {};
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

    class CompareEQ : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        CompareEQ (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (COMPARE_EQ, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class CompareGE : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        CompareGE (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (COMPARE_GE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class CompareGT : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        CompareGT (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (COMPARE_GT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class CompareIN : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        CompareIN (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (COMPARE_IN, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class CompareLE : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        CompareLE (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (COMPARE_LE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class CompareLT : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        CompareLT (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (COMPARE_LT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class CompareNE : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        CompareNE (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (COMPARE_NE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class Divide : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Divide (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (DIVIDE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class ElemXcr : public Instruction_4<Register, Register, Register, double> {
    public:
        /* dest, base, index, value */
        ElemXcr (Register aOp1, Register aOp2, Register aOp3, double aOp4) :
            Instruction_4<Register, Register, Register, double>
            (ELEM_XCR, aOp1, aOp2, aOp3, aOp4) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[ELEM_XCR] << "\t" << "R" << mOp1 << ", " << "R" << mOp2 << ", " << "R" << mOp3 << ", " << mOp4;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2] << ", " << "R" << mOp3 << '=' << registers[mOp3];
            return f;
        }
    };

    class GetElement : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, base, index */
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

    class GetProp : public Instruction_3<Register, Register, const StringAtom*> {
    public:
        /* dest, object, prop name */
        GetProp (Register aOp1, Register aOp2, const StringAtom* aOp3) :
            Instruction_3<Register, Register, const StringAtom*>
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

    class Instanceof : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        Instanceof (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (INSTANCEOF, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class Jsr : public GenericBranch {
    public:
        /* target */
        Jsr (Label* aOp1) :
            GenericBranch
            (JSR, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[JSR] << "\t" << "Offset " << ((mOp1) ? mOp1->mOffset : NotAnOffset);
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

    class LoadName : public Instruction_2<Register, const StringAtom*> {
    public:
        /* dest, name */
        LoadName (Register aOp1, const StringAtom* aOp2) :
            Instruction_2<Register, const StringAtom*>
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

    class LoadString : public Instruction_2<Register, JSString*> {
    public:
        /* dest, immediate value (string) */
        LoadString (Register aOp1, JSString* aOp2) :
            Instruction_2<Register, JSString*>
            (LOAD_STRING, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[LOAD_STRING] << "\t" << "R" << mOp1 << ", " << "'" << *mOp2 << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class LoadValue : public Instruction_2<Register, JSValue> {
    public:
        /* dest, immediate value (JSValue) */
        LoadValue (Register aOp1, JSValue aOp2) :
            Instruction_2<Register, JSValue>
            (LOAD_VALUE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[LOAD_VALUE] << "\t" << "R" << mOp1 << ", " << mOp2;
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

    class NameXcr : public Instruction_3<Register, const StringAtom*, double> {
    public:
        /* dest, name, value */
        NameXcr (Register aOp1, const StringAtom* aOp2, double aOp3) :
            Instruction_3<Register, const StringAtom*, double>
            (NAME_XCR, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NAME_XCR] << "\t" << "R" << mOp1 << ", " << "'" << *mOp2 << "'" << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class Negate : public Instruction_2<Register, Register> {
    public:
        /* dest, source */
        Negate (Register aOp1, Register aOp2) :
            Instruction_2<Register, Register>
            (NEGATE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NEGATE] << "\t" << "R" << mOp1 << ", " << "R" << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
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

    class Or : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Or (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (OR, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Posate : public Instruction_2<Register, Register> {
    public:
        /* dest, source */
        Posate (Register aOp1, Register aOp2) :
            Instruction_2<Register, Register>
            (POSATE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[POSATE] << "\t" << "R" << mOp1 << ", " << "R" << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };

    class PropXcr : public Instruction_4<Register, Register, const StringAtom*, double> {
    public:
        /* dest, source, name, value */
        PropXcr (Register aOp1, Register aOp2, const StringAtom* aOp3, double aOp4) :
            Instruction_4<Register, Register, const StringAtom*, double>
            (PROP_XCR, aOp1, aOp2, aOp3, aOp4) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[PROP_XCR] << "\t" << "R" << mOp1 << ", " << "R" << mOp2 << ", " << "'" << *mOp3 << "'" << ", " << mOp4;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };

    class Remainder : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Remainder (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (REMAINDER, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
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

    class SaveName : public Instruction_2<const StringAtom*, Register> {
    public:
        /* name, source */
        SaveName (const StringAtom* aOp1, Register aOp2) :
            Instruction_2<const StringAtom*, Register>
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
        /* base, index, value */
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

    class SetProp : public Instruction_3<Register, const StringAtom*, Register> {
    public:
        /* object, name, source */
        SetProp (Register aOp1, const StringAtom* aOp2, Register aOp3) :
            Instruction_3<Register, const StringAtom*, Register>
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

    class Shiftleft : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Shiftleft (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (SHIFTLEFT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Shiftright : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Shiftright (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (SHIFTRIGHT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class StrictEQ : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        StrictEQ (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (STRICT_EQ, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class StrictNE : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, source1, source2 */
        StrictNE (Register aOp1, Register aOp2, Register aOp3) :
            Instruction_3<Register, Register, Register>
            (STRICT_NE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<Register, Register, Register> */
    };

    class Subtract : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Subtract (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (SUBTRACT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Test : public Instruction_2<Register, Register> {
    public:
        /* dest, source */
        Test (Register aOp1, Register aOp2) :
            Instruction_2<Register, Register>
            (TEST, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[TEST] << "\t" << "R" << mOp1 << ", " << "R" << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
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

    class Tryin : public Instruction_2<Label*, Label*> {
    public:
        /* catch target, finally target */
        Tryin (Label* aOp1, Label* aOp2) :
            Instruction_2<Label*, Label*>
            (TRYIN, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[TRYIN] << "\t" << "Offset " << ((mOp1) ? mOp1->mOffset : NotAnOffset) << ", " << "Offset " << ((mOp2) ? mOp2->mOffset : NotAnOffset);
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class Tryout : public Instruction {
    public:
        /* mmm, there is no try, only do */
        Tryout () :
            Instruction
            (TRYOUT) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[TRYOUT];
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class Ushiftright : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Ushiftright (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (USHIFTRIGHT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class VarXcr : public Instruction_3<Register, Register, double> {
    public:
        /* dest, source, value */
        VarXcr (Register aOp1, Register aOp2, double aOp3) :
            Instruction_3<Register, Register, double>
            (VAR_XCR, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[VAR_XCR] << "\t" << "R" << mOp1 << ", " << "R" << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1] << ", " << "R" << mOp2 << '=' << registers[mOp2];
            return f;
        }
    };

    class Within : public Instruction_1<Register> {
    public:
        /* within this object */
        Within (Register aOp1) :
            Instruction_1<Register>
            (WITHIN, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[WITHIN] << "\t" << "R" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << "R" << mOp1 << '=' << registers[mOp1];
            return f;
        }
    };

    class Without : public Instruction {
    public:
        /* without this object */
        Without () :
            Instruction
            (WITHOUT) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[WITHOUT];
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) {
            return f;
        }
    };

    class Xor : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Xor (Register aOp1, Register aOp2, Register aOp3) :
            Arithmetic
            (XOR, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };



} /* namespace VM */

} /* namespace JavaScript */

#endif /* vmtypes_h */
