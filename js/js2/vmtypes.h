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

namespace JavaScript {
namespace VM {
    
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
        GET_ELEMENT, /* dest, array, index */
        GET_PROP, /* dest, object, prop name */
        LOAD_IMMEDIATE, /* dest, immediate value (double) */
        LOAD_NAME, /* dest, name */
        LOAD_VAR, /* dest, index of frame slot */
        MOVE, /* dest, source */
        MULTIPLY, /* dest, source1, source2 */
        NEW_ARRAY, /* dest */
        NEW_OBJECT, /* dest */
        NOP, /* do nothing and like it */
        NOT, /* dest, source */
        RETURN, /* return value or NotARegister */
        SAVE_NAME, /* name, source */
        SAVE_VAR, /* index of frame slot, source */
        SET_ELEMENT, /* base, source1, source2 */
        SET_PROP, /* object, name, source */
        SUBTRACT /* dest, source1, source2 */
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
        "GET_ELEMENT   ",
        "GET_PROP      ",
        "LOAD_IMMEDIATE",
        "LOAD_NAME     ",
        "LOAD_VAR      ",
        "MOVE          ",
        "MULTIPLY      ",
        "NEW_ARRAY     ",
        "NEW_OBJECT    ",
        "NOP           ",
        "NOT           ",
        "RETURN        ",
        "SAVE_NAME     ",
        "SAVE_VAR      ",
        "SET_ELEMENT   ",
        "SET_PROP      ",
        "SUBTRACT      "
    };

    /********************************************************************/

    /* super-class for all instructions */
    class Instruction
    {
    public:
        Instruction(ICodeOp opcodeA) : opcode(opcodeA) { }
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[opcode] << "\t<unk>";
            return f;
        }                
        
        ICodeOp getBranchOp() \
        { return ((opcode >= COMPARE_EQ) && (opcode <= COMPARE_NE)) ? \
              (ICodeOp)(BRANCH_EQ + (opcode - COMPARE_EQ)) : NOP;  }
        
        ICodeOp op() { return opcode; }
        
    protected:
        ICodeOp opcode;
        
    };

    /********************************************************************/

    enum { NotARegister = 0xFFFFFFFF };
    enum { NotALabel = 0xFFFFFFFF };
    enum { NotAnOffset = 0xFFFFFFFF };
    
    /********************************************************************/
    
    typedef uint32 Register;
    typedef std::vector<Register> RegisterList;        
    typedef std::vector<Instruction *> InstructionStream;
    typedef InstructionStream::iterator InstructionIterator;
    
    /********************************************************************/
    
    Formatter& operator<< (Formatter& f, Instruction& i);
    Formatter& operator<< (Formatter& f, RegisterList& rl);
    
    /********************************************************************/
    
    class Label {
    public:
        Label(InstructionStream* baseA) :
            base(baseA), offset(NotALabel) {}
        
        InstructionStream *base;
        uint32 offset;
    };
    
    typedef std::vector<Label *> LabelList;
    typedef LabelList::iterator LabelIterator;
    
    /********************************************************************/
    
    /* 1, 2 and 3 operand opcode templates */
    
    template <typename Operand1>
    class Instruction_1 : public Instruction {
    public:
        Instruction_1(ICodeOp opcodeA, Operand1 op1A) : 
            Instruction(opcodeA), op1(op1A) { }            
        Operand1& o1() { return op1; }
        
    protected:
        Operand1 op1;
    };
    
    template <typename Operand1, typename Operand2>
    class Instruction_2 : public Instruction {
    public:
        Instruction_2(ICodeOp opcodeA, Operand1 op1A, Operand2 op2A) :
            Instruction(opcodeA), op1(op1A), op2(op2A) {}
        Operand1& o1() { return op1; }
        Operand2& o2() { return op2; }
        
    protected:
        Operand1 op1;
        Operand2 op2;
    };
    
    template <typename Operand1, typename Operand2, typename Operand3>
    class Instruction_3 : public Instruction {
    public:
        Instruction_3(ICodeOp opcodeA, Operand1 op1A, Operand2 op2A, 
                      Operand3 op3A) :
            Instruction(opcodeA), op1(op1A), op2(op2A), op3(op3A) { }
        Operand1& o1() { return op1; }
        Operand2& o2() { return op2; }
        Operand3& o3() { return op3; }
        
    protected:
        Operand1 op1;
        Operand2 op2;
        Operand3 op3;
    };
    
    /********************************************************************/

    /* Instruction groups */
    
    class Arithmetic : public Instruction_3<Register, Register, Register> {
    public:
        Arithmetic (ICodeOp opcodeA, Register dest, Register src1,
                    Register src2) :
            Instruction_3<Register, Register, Register>(opcodeA,
                                                        dest, src1, src2) {}
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[opcode] << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            if (op3 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op3;
            }
            return f;
        }
    };
    
    class Compare : public Instruction_2<Register, Register> {
    public:
        Compare(ICodeOp opcodeA, Register dest, Register src) :
            Instruction_2<Register, Register>(opcodeA, dest, src) {}
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[opcode] << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };
    
    class GenericBranch : public Instruction_2<Label*, Register> {
    public:
        GenericBranch (ICodeOp opcodeA, Label* label, 
                       Register r = NotARegister) :
            Instruction_2<Label*, Register>(opcodeA, label, r) {}
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[opcode] << "\t" << "Offset " << op1->offset;
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
        void resolveTo (uint32 offsetA) { op1->offset = offsetA; }
        uint32 getOffset() { return op1->offset; }
        
    };

        /********************************************************************/
            
        /* Specific opcodes */

    class Add : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Add (Register op1A, Register op2A, Register op3A) :
            Arithmetic
            (ADD, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[ADD];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            f << ", ";
            if (op3 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op3;
            }
            return f;
        }
    };

    class Branch : public GenericBranch {
    public:
        /* target label */
        Branch (Label* op1A) :
            GenericBranch
            (BRANCH, op1A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[BRANCH];
            f << "\t";
            f << "Offset " << op1->offset;
            return f;
        }
    };

    class BranchEQ : public GenericBranch {
    public:
        /* target label, condition */
        BranchEQ (Label* op1A, Register op2A) :
            GenericBranch
            (BRANCH_EQ, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[BRANCH_EQ];
            f << "\t";
            f << "Offset " << op1->offset;
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class BranchGE : public GenericBranch {
    public:
        /* target label, condition */
        BranchGE (Label* op1A, Register op2A) :
            GenericBranch
            (BRANCH_GE, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[BRANCH_GE];
            f << "\t";
            f << "Offset " << op1->offset;
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class BranchGT : public GenericBranch {
    public:
        /* target label, condition */
        BranchGT (Label* op1A, Register op2A) :
            GenericBranch
            (BRANCH_GT, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[BRANCH_GT];
            f << "\t";
            f << "Offset " << op1->offset;
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class BranchLE : public GenericBranch {
    public:
        /* target label, condition */
        BranchLE (Label* op1A, Register op2A) :
            GenericBranch
            (BRANCH_LE, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[BRANCH_LE];
            f << "\t";
            f << "Offset " << op1->offset;
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class BranchLT : public GenericBranch {
    public:
        /* target label, condition */
        BranchLT (Label* op1A, Register op2A) :
            GenericBranch
            (BRANCH_LT, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[BRANCH_LT];
            f << "\t";
            f << "Offset " << op1->offset;
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class BranchNE : public GenericBranch {
    public:
        /* target label, condition */
        BranchNE (Label* op1A, Register op2A) :
            GenericBranch
            (BRANCH_NE, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[BRANCH_NE];
            f << "\t";
            f << "Offset " << op1->offset;
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class Call : public Instruction_3<Register, Register, RegisterList> {
    public:
        /* result, target, args */
        Call (Register op1A, Register op2A, RegisterList op3A) :
            Instruction_3<Register, Register, RegisterList>
            (CALL, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[CALL];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            f << ", ";
            f << op3;
            return f;
        }
    };

    class CompareEQ : public Compare {
    public:
        /* dest, source */
        CompareEQ (Register op1A, Register op2A) :
            Compare
            (COMPARE_EQ, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[COMPARE_EQ];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class CompareGE : public Compare {
    public:
        /* dest, source */
        CompareGE (Register op1A, Register op2A) :
            Compare
            (COMPARE_GE, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[COMPARE_GE];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class CompareGT : public Compare {
    public:
        /* dest, source */
        CompareGT (Register op1A, Register op2A) :
            Compare
            (COMPARE_GT, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[COMPARE_GT];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class CompareLE : public Compare {
    public:
        /* dest, source */
        CompareLE (Register op1A, Register op2A) :
            Compare
            (COMPARE_LE, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[COMPARE_LE];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class CompareLT : public Compare {
    public:
        /* dest, source */
        CompareLT (Register op1A, Register op2A) :
            Compare
            (COMPARE_LT, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[COMPARE_LT];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class CompareNE : public Compare {
    public:
        /* dest, source */
        CompareNE (Register op1A, Register op2A) :
            Compare
            (COMPARE_NE, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[COMPARE_NE];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class Divide : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Divide (Register op1A, Register op2A, Register op3A) :
            Arithmetic
            (DIVIDE, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[DIVIDE];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            f << ", ";
            if (op3 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op3;
            }
            return f;
        }
    };

    class GetElement : public Instruction_3<Register, Register, Register> {
    public:
        /* dest, array, index */
        GetElement (Register op1A, Register op2A, Register op3A) :
            Instruction_3<Register, Register, Register>
            (GET_ELEMENT, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[GET_ELEMENT];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            f << ", ";
            if (op3 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op3;
            }
            return f;
        }
    };

    class GetProp : public Instruction_3<Register, Register, StringAtom*> {
    public:
        /* dest, object, prop name */
        GetProp (Register op1A, Register op2A, StringAtom* op3A) :
            Instruction_3<Register, Register, StringAtom*>
            (GET_PROP, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[GET_PROP];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            f << ", ";
            f << "'" << *op3 << "'";
            return f;
        }
    };

    class LoadImmediate : public Instruction_2<Register, double> {
    public:
        /* dest, immediate value (double) */
        LoadImmediate (Register op1A, double op2A) :
            Instruction_2<Register, double>
            (LOAD_IMMEDIATE, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[LOAD_IMMEDIATE];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            f << op2;
            return f;
        }
    };

    class LoadName : public Instruction_2<Register, StringAtom*> {
    public:
        /* dest, name */
        LoadName (Register op1A, StringAtom* op2A) :
            Instruction_2<Register, StringAtom*>
            (LOAD_NAME, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[LOAD_NAME];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            f << "'" << *op2 << "'";
            return f;
        }
    };

    class LoadVar : public Instruction_2<Register, uint32> {
    public:
        /* dest, index of frame slot */
        LoadVar (Register op1A, uint32 op2A) :
            Instruction_2<Register, uint32>
            (LOAD_VAR, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[LOAD_VAR];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            f << op2;
            return f;
        }
    };

    class Move : public Instruction_2<Register, Register> {
    public:
        /* dest, source */
        Move (Register op1A, Register op2A) :
            Instruction_2<Register, Register>
            (MOVE, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[MOVE];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class Multiply : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Multiply (Register op1A, Register op2A, Register op3A) :
            Arithmetic
            (MULTIPLY, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[MULTIPLY];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            f << ", ";
            if (op3 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op3;
            }
            return f;
        }
    };

    class NewArray : public Instruction_1<Register> {
    public:
        /* dest */
        NewArray (Register op1A) :
            Instruction_1<Register>
            (NEW_ARRAY, op1A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[NEW_ARRAY];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            return f;
        }
    };

    class NewObject : public Instruction_1<Register> {
    public:
        /* dest */
        NewObject (Register op1A) :
            Instruction_1<Register>
            (NEW_OBJECT, op1A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[NEW_OBJECT];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            return f;
        }
    };

    class Nop : public Instruction {
    public:
        /* do nothing and like it */
        Nop () :
            Instruction
            (NOP) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[NOP];
            return f;
        }
    };

    class Not : public Instruction_2<Register, Register> {
    public:
        /* dest, source */
        Not (Register op1A, Register op2A) :
            Instruction_2<Register, Register>
            (NOT, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[NOT];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class Return : public Instruction_1<Register> {
    public:
        /* return value or NotARegister */
        Return (Register op1A = NotARegister) :
            Instruction_1<Register>
            (RETURN, op1A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[RETURN];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            return f;
        }
    };

    class SaveName : public Instruction_2<StringAtom*, Register> {
    public:
        /* name, source */
        SaveName (StringAtom* op1A, Register op2A) :
            Instruction_2<StringAtom*, Register>
            (SAVE_NAME, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[SAVE_NAME];
            f << "\t";
            f << "'" << *op1 << "'";
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class SaveVar : public Instruction_2<uint32, Register> {
    public:
        /* index of frame slot, source */
        SaveVar (uint32 op1A, Register op2A) :
            Instruction_2<uint32, Register>
            (SAVE_VAR, op1A, op2A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[SAVE_VAR];
            f << "\t";
            f << op1;
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            return f;
        }
    };

    class SetElement : public Instruction_3<Register, Register, Register> {
    public:
        /* base, source1, source2 */
        SetElement (Register op1A, Register op2A, Register op3A) :
            Instruction_3<Register, Register, Register>
            (SET_ELEMENT, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[SET_ELEMENT];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            f << ", ";
            if (op3 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op3;
            }
            return f;
        }
    };

    class SetProp : public Instruction_3<Register, StringAtom*, Register> {
    public:
        /* object, name, source */
        SetProp (Register op1A, StringAtom* op2A, Register op3A) :
            Instruction_3<Register, StringAtom*, Register>
            (SET_PROP, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[SET_PROP];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            f << "'" << *op2 << "'";
            f << ", ";
            if (op3 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op3;
            }
            return f;
        }
    };

    class Subtract : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Subtract (Register op1A, Register op2A, Register op3A) :
            Arithmetic
            (SUBTRACT, op1A, op2A, op3A) {};
        virtual Formatter& print (Formatter& f) {
            f << opcodeNames[SUBTRACT];
            f << "\t";
            if (op1 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op1;
            }
            f << ", ";
            if (op2 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op2;
            }
            f << ", ";
            if (op3 == NotARegister) {
                f << "R~";
            } else {
                f << "R" << op3;
            }
            return f;
        }
    };

} /* namespace VM */

} /* namespace JavaScript */

#endif /* vmtypes_h */
