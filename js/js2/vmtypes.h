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
#include "jsclasses.h"
#include "world.h"
#include <vector>

/* forward declare classes from JavaScript::ICG */
namespace JavaScript {
namespace ICG {
    class ICodeModule;
} /* namespace ICG */
} /* namespace JavaScript */

namespace JavaScript {
namespace VM {

    using namespace JSTypes;
    using namespace JSClasses;
    
    /********************************************************************/

    typedef uint32 ICodeOp;
    extern char *opcodeNames[];

    /* super-class for all instructions */
    class Instruction
    {
    public:
        Instruction(ICodeOp aOpcode) : mOpcode(aOpcode) {}
        
        virtual Formatter& print(Formatter& f) = 0;
        virtual Formatter& printOperands(Formatter& f, const JSValues& /*registers*/) = 0;
        
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
    
    typedef std::pair<Register, JSType*> TypedRegister;
    typedef std::vector<TypedRegister> RegisterList;        
    typedef std::vector<Instruction *> InstructionStream;
    typedef InstructionStream::iterator InstructionIterator;
    typedef std::map<String, TypedRegister, std::less<String> > VariableMap;

    
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
    Formatter& operator<< (Formatter& f, TypedRegister& r);
    
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
    
    class Arithmetic : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        Arithmetic (ICodeOp aOpcode, TypedRegister aDest, TypedRegister aSrc1,
                    TypedRegister aSrc2) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>(aOpcode, aDest, aSrc1, aSrc2) {}

        virtual Formatter& print(Formatter& f)
        {
            f << opcodeNames[mOpcode] << "\tR" << mOp1.first << ", R" << mOp2.first << ", R" << mOp3.first;
            return f;
        }
        
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers)
        {
            f << "R" << mOp1.first << '=' << registers[mOp1.first] << ", " << "R" << mOp2.first << '=' << registers[mOp2.first] << ", " << "R" << mOp3.first << '=' << registers[mOp3.first];
            return f;
        }
    };
    
    class Unary : public Instruction_2<TypedRegister, TypedRegister> {
    public:
        Unary(ICodeOp aOpcode, TypedRegister aDest, TypedRegister aSrc) :
            Instruction_2<TypedRegister, TypedRegister>(aOpcode, aDest, aSrc) {}
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[mOpcode] << "\tR" << mOp1.first << ", R" << mOp2.first;
            return f;
        }

        virtual Formatter& printOperands(Formatter& f, const JSValues& registers)
        {
            f << "R" << mOp1.first << '=' << registers[mOp1.first] << ", " << "R" << mOp2.first << '=' << registers[mOp2.first];
            return f;
        }
    };
    
    class GenericBranch : public Instruction_2<Label*, TypedRegister> {
    public:
        GenericBranch (ICodeOp aOpcode, Label* aLabel, 
                       TypedRegister aR = TypedRegister(NotARegister, &Any_Type) ) :
            Instruction_2<Label*, TypedRegister>(aOpcode, aLabel, aR) {}
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[mOpcode] << "\tOffset " << mOp1->mOffset;
            if (mOp2.first == NotARegister) {
                f << ", R~";
            } else {
                f << ", R" << mOp2.first;
            }
            return f;
        }

        virtual Formatter& printOperands(Formatter& f, const JSValues& registers)
        {
            if (mOp2.first != NotARegister)
                f << "R" << mOp2.first << '=' << registers[mOp2.first];
            return f;
        }

        void resolveTo (uint32 aOffset) { mOp1->mOffset = aOffset; }
        uint32 getOffset() { return mOp1->mOffset; }
        void setTarget(Label *label) { mOp1 = label; }
    };

    /********************************************************************/

    class BinaryOperator {
    public:

        // Wah, here's a third enumeration of opcodes - ExprNode, ICodeOp and now here, this can't be right??
        typedef enum { 
            BinaryOperatorFirst,
            Add = BinaryOperatorFirst, Subtract, Multiply, Divide,
            Remainder, LeftShift, RightShift, LogicalRightShift,
            BitwiseOr, BitwiseXor, BitwiseAnd, Less, LessOrEqual,
            Equal, Identical, BinaryOperatorCount
        } BinaryOp;

        BinaryOperator(const JSType *t1, const JSType *t2, JSBinaryOperator *function) :
            t1(t1), t2(t2), function(function) { }

        BinaryOperator(const JSType *t1, const JSType *t2, JSFunction *function) :
            t1(t1), t2(t2), function(function) { }

        static BinaryOp mapICodeOp(ICodeOp op);

        const JSType *t1;
        const JSType *t2;
        JSFunction *function;

    };

    typedef std::vector<BinaryOperator *> BinaryOperatorList;
    
    Formatter& operator<<(Formatter &f, BinaryOperator::BinaryOp &b);

    /********************************************************************/

    #include "icode.h"

} /* namespace VM */

} /* namespace JavaScript */

#endif /* vmtypes_h */
