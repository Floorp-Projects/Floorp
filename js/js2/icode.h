// THIS FILE IS MACHINE GENERATED! DO NOT EDIT BY HAND!

#if !defined(OPCODE_NAMES)

    enum {
        ADD, /* dest, source1, source2 */
        AND, /* dest, source1, source2 */
        BITNOT, /* dest, source */
        BRANCH, /* target label */
        BRANCH_FALSE, /* target label, condition */
        BRANCH_TRUE, /* target label, condition */
        CALL, /* result, target, this, args */
        CAST, /* dest, rvalue, toType */
        COMPARE_EQ, /* dest, source1, source2 */
        COMPARE_GE, /* dest, source1, source2 */
        COMPARE_GT, /* dest, source1, source2 */
        COMPARE_IN, /* dest, source1, source2 */
        COMPARE_LE, /* dest, source1, source2 */
        COMPARE_LT, /* dest, source1, source2 */
        COMPARE_NE, /* dest, source1, source2 */
        DEBUGGER, /* drop to the debugger */
        DELETE_PROP, /* dest, object, prop name */
        DIRECT_CALL, /* result, target, args */
        DIVIDE, /* dest, source1, source2 */
        ELEM_XCR, /* dest, base, index, value */
        GENERIC_BINARY_OP, /* dest, op, source1, source2 */
        GET_ELEMENT, /* dest, base, index */
        GET_METHOD, /* result, target base, index */
        GET_PROP, /* dest, object, prop name */
        GET_SLOT, /* dest, object, slot number */
        GET_STATIC, /* dest, class, index */
        INSTANCEOF, /* dest, source1, source2 */
        JSR, /* target */
        LOAD_BOOLEAN, /* dest, immediate value (boolean) */
        LOAD_IMMEDIATE, /* dest, immediate value (double) */
        LOAD_NAME, /* dest, name */
        LOAD_STRING, /* dest, immediate value (string) */
        MOVE, /* dest, source */
        MULTIPLY, /* dest, source1, source2 */
        NAME_XCR, /* dest, name, value */
        NEGATE, /* dest, source */
        NEW_ARRAY, /* dest */
        NEW_CLASS, /* dest, class */
        NEW_FUNCTION, /* dest, ICodeModule, Function Definition */
        NEW_OBJECT, /* dest, constructor */
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
        SET_SLOT, /* object, slot number, source */
        SET_STATIC, /* class, index, source */
        SHIFTLEFT, /* dest, source1, source2 */
        SHIFTRIGHT, /* dest, source1, source2 */
        SLOT_XCR, /* dest, source, slot number, value */
        STATIC_XCR, /* dest, class, index, value */
        STRICT_EQ, /* dest, source1, source2 */
        STRICT_NE, /* dest, source1, source2 */
        SUBTRACT, /* dest, source1, source2 */
        SUPER, /* dest */
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

    class Add : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Add (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (ADD, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class And : public Arithmetic {
    public:
        /* dest, source1, source2 */
        And (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (AND, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Bitnot : public Instruction_2<TypedRegister, TypedRegister> {
    public:
        /* dest, source */
        Bitnot (TypedRegister aOp1, TypedRegister aOp2) :
            Instruction_2<TypedRegister, TypedRegister>
            (BITNOT, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[BITNOT] << "\t" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
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
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            return f;
        }
    };

    class BranchFalse : public GenericBranch {
    public:
        /* target label, condition */
        BranchFalse (Label* aOp1, TypedRegister aOp2) :
            GenericBranch
            (BRANCH_FALSE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class BranchTrue : public GenericBranch {
    public:
        /* target label, condition */
        BranchTrue (Label* aOp1, TypedRegister aOp2) :
            GenericBranch
            (BRANCH_TRUE, aOp1, aOp2) {};
        /* print() and printOperands() inherited from GenericBranch */
    };

    class Call : public Instruction_4<TypedRegister, TypedRegister, TypedRegister, ArgumentList> {
    public:
        /* result, target, this, args */
        Call (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3, ArgumentList aOp4) :
            Instruction_4<TypedRegister, TypedRegister, TypedRegister, ArgumentList>
            (CALL, aOp1, aOp2, aOp3, aOp4) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[CALL] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3 << ", " << mOp4;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first) << ", " << getRegisterValue(registers, mOp3.first) << ", " << ArgList(mOp4, registers);
            return f;
        }
    };

    class Cast : public Instruction_3<TypedRegister, TypedRegister, JSType*> {
    public:
        /* dest, rvalue, toType */
        Cast (TypedRegister aOp1, TypedRegister aOp2, JSType* aOp3) :
            Instruction_3<TypedRegister, TypedRegister, JSType*>
            (CAST, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[CAST] << "\t" << mOp1 << ", " << mOp2 << ", " << "'" << mOp3->getName() << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class CompareEQ : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        CompareEQ (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (COMPARE_EQ, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class CompareGE : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        CompareGE (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (COMPARE_GE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class CompareGT : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        CompareGT (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (COMPARE_GT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class CompareIN : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        CompareIN (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (COMPARE_IN, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class CompareLE : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        CompareLE (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (COMPARE_LE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class CompareLT : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        CompareLT (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (COMPARE_LT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class CompareNE : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        CompareNE (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (COMPARE_NE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class Debugger : public Instruction {
    public:
        /* drop to the debugger */
        Debugger () :
            Instruction
            (DEBUGGER) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[DEBUGGER];
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            return f;
        }
    };

    class DeleteProp : public Instruction_3<TypedRegister, TypedRegister, const StringAtom*> {
    public:
        /* dest, object, prop name */
        DeleteProp (TypedRegister aOp1, TypedRegister aOp2, const StringAtom* aOp3) :
            Instruction_3<TypedRegister, TypedRegister, const StringAtom*>
            (DELETE_PROP, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[DELETE_PROP] << "\t" << mOp1 << ", " << mOp2 << ", " << "'" << *mOp3 << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class DirectCall : public Instruction_3<TypedRegister, JSFunction *, ArgumentList> {
    public:
        /* result, target, args */
        DirectCall (TypedRegister aOp1, JSFunction * aOp2, ArgumentList aOp3) :
            Instruction_3<TypedRegister, JSFunction *, ArgumentList>
            (DIRECT_CALL, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[DIRECT_CALL] << "\t" << mOp1 << ", " << "JSFunction" << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << ArgList(mOp3, registers);
            return f;
        }
    };

    class Divide : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Divide (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (DIVIDE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class ElemXcr : public Instruction_4<TypedRegister, TypedRegister, TypedRegister, double> {
    public:
        /* dest, base, index, value */
        ElemXcr (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3, double aOp4) :
            Instruction_4<TypedRegister, TypedRegister, TypedRegister, double>
            (ELEM_XCR, aOp1, aOp2, aOp3, aOp4) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[ELEM_XCR] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3 << ", " << mOp4;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first) << ", " << getRegisterValue(registers, mOp3.first);
            return f;
        }
    };

    class GenericBinaryOP : public Instruction_4<TypedRegister, BinaryOperator::BinaryOp, TypedRegister, TypedRegister> {
    public:
        /* dest, op, source1, source2 */
        GenericBinaryOP (TypedRegister aOp1, BinaryOperator::BinaryOp aOp2, TypedRegister aOp3, TypedRegister aOp4) :
            Instruction_4<TypedRegister, BinaryOperator::BinaryOp, TypedRegister, TypedRegister>
            (GENERIC_BINARY_OP, aOp1, aOp2, aOp3, aOp4) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[GENERIC_BINARY_OP] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3 << ", " << mOp4;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp3.first) << ", " << getRegisterValue(registers, mOp4.first);
            return f;
        }
    };

    class GetElement : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, base, index */
        GetElement (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (GET_ELEMENT, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[GET_ELEMENT] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first) << ", " << getRegisterValue(registers, mOp3.first);
            return f;
        }
    };

    class GetMethod : public Instruction_3<TypedRegister, TypedRegister, uint32> {
    public:
        /* result, target base, index */
        GetMethod (TypedRegister aOp1, TypedRegister aOp2, uint32 aOp3) :
            Instruction_3<TypedRegister, TypedRegister, uint32>
            (GET_METHOD, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[GET_METHOD] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class GetProp : public Instruction_3<TypedRegister, TypedRegister, const StringAtom*> {
    public:
        /* dest, object, prop name */
        GetProp (TypedRegister aOp1, TypedRegister aOp2, const StringAtom* aOp3) :
            Instruction_3<TypedRegister, TypedRegister, const StringAtom*>
            (GET_PROP, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[GET_PROP] << "\t" << mOp1 << ", " << mOp2 << ", " << "'" << *mOp3 << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class GetSlot : public Instruction_3<TypedRegister, TypedRegister, uint32> {
    public:
        /* dest, object, slot number */
        GetSlot (TypedRegister aOp1, TypedRegister aOp2, uint32 aOp3) :
            Instruction_3<TypedRegister, TypedRegister, uint32>
            (GET_SLOT, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[GET_SLOT] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class GetStatic : public Instruction_3<TypedRegister, JSClass*, uint32> {
    public:
        /* dest, class, index */
        GetStatic (TypedRegister aOp1, JSClass* aOp2, uint32 aOp3) :
            Instruction_3<TypedRegister, JSClass*, uint32>
            (GET_STATIC, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[GET_STATIC] << "\t" << mOp1 << ", " << mOp2->getName() << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class Instanceof : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        Instanceof (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (INSTANCEOF, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
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
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            return f;
        }
    };

    class LoadBoolean : public Instruction_2<TypedRegister, bool> {
    public:
        /* dest, immediate value (boolean) */
        LoadBoolean (TypedRegister aOp1, bool aOp2) :
            Instruction_2<TypedRegister, bool>
            (LOAD_BOOLEAN, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[LOAD_BOOLEAN] << "\t" << mOp1 << ", " << "'" << ((mOp2) ? "true" : "false") << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class LoadImmediate : public Instruction_2<TypedRegister, double> {
    public:
        /* dest, immediate value (double) */
        LoadImmediate (TypedRegister aOp1, double aOp2) :
            Instruction_2<TypedRegister, double>
            (LOAD_IMMEDIATE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[LOAD_IMMEDIATE] << "\t" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class LoadName : public Instruction_2<TypedRegister, const StringAtom*> {
    public:
        /* dest, name */
        LoadName (TypedRegister aOp1, const StringAtom* aOp2) :
            Instruction_2<TypedRegister, const StringAtom*>
            (LOAD_NAME, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[LOAD_NAME] << "\t" << mOp1 << ", " << "'" << *mOp2 << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class LoadString : public Instruction_2<TypedRegister, JSString*> {
    public:
        /* dest, immediate value (string) */
        LoadString (TypedRegister aOp1, JSString* aOp2) :
            Instruction_2<TypedRegister, JSString*>
            (LOAD_STRING, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[LOAD_STRING] << "\t" << mOp1 << ", " << "'" << *mOp2 << "'";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class Move : public Instruction_2<TypedRegister, TypedRegister> {
    public:
        /* dest, source */
        Move (TypedRegister aOp1, TypedRegister aOp2) :
            Instruction_2<TypedRegister, TypedRegister>
            (MOVE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[MOVE] << "\t" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class Multiply : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Multiply (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (MULTIPLY, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class NameXcr : public Instruction_3<TypedRegister, const StringAtom*, double> {
    public:
        /* dest, name, value */
        NameXcr (TypedRegister aOp1, const StringAtom* aOp2, double aOp3) :
            Instruction_3<TypedRegister, const StringAtom*, double>
            (NAME_XCR, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NAME_XCR] << "\t" << mOp1 << ", " << "'" << *mOp2 << "'" << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class Negate : public Instruction_2<TypedRegister, TypedRegister> {
    public:
        /* dest, source */
        Negate (TypedRegister aOp1, TypedRegister aOp2) :
            Instruction_2<TypedRegister, TypedRegister>
            (NEGATE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NEGATE] << "\t" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class NewArray : public Instruction_1<TypedRegister> {
    public:
        /* dest */
        NewArray (TypedRegister aOp1) :
            Instruction_1<TypedRegister>
            (NEW_ARRAY, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NEW_ARRAY] << "\t" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class NewClass : public Instruction_2<TypedRegister, JSClass*> {
    public:
        /* dest, class */
        NewClass (TypedRegister aOp1, JSClass* aOp2) :
            Instruction_2<TypedRegister, JSClass*>
            (NEW_CLASS, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NEW_CLASS] << "\t" << mOp1 << ", " << mOp2->getName();
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class NewFunction : public Instruction_3<TypedRegister, ICodeModule*, FunctionDefinition*> {
    public:
        /* dest, ICodeModule, Function Definition */
        NewFunction (TypedRegister aOp1, ICodeModule* aOp2, FunctionDefinition* aOp3) :
            Instruction_3<TypedRegister, ICodeModule*, FunctionDefinition*>
            (NEW_FUNCTION, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NEW_FUNCTION] << "\t" << mOp1 << ", " << "ICodeModule" << ", " << "FunctionDefinition";
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class NewObject : public Instruction_2<TypedRegister, TypedRegister> {
    public:
        /* dest, constructor */
        NewObject (TypedRegister aOp1, TypedRegister aOp2) :
            Instruction_2<TypedRegister, TypedRegister>
            (NEW_OBJECT, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NEW_OBJECT] << "\t" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
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
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            return f;
        }
    };

    class Not : public Instruction_2<TypedRegister, TypedRegister> {
    public:
        /* dest, source */
        Not (TypedRegister aOp1, TypedRegister aOp2) :
            Instruction_2<TypedRegister, TypedRegister>
            (NOT, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[NOT] << "\t" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class Or : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Or (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (OR, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Posate : public Instruction_2<TypedRegister, TypedRegister> {
    public:
        /* dest, source */
        Posate (TypedRegister aOp1, TypedRegister aOp2) :
            Instruction_2<TypedRegister, TypedRegister>
            (POSATE, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[POSATE] << "\t" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class PropXcr : public Instruction_4<TypedRegister, TypedRegister, const StringAtom*, double> {
    public:
        /* dest, source, name, value */
        PropXcr (TypedRegister aOp1, TypedRegister aOp2, const StringAtom* aOp3, double aOp4) :
            Instruction_4<TypedRegister, TypedRegister, const StringAtom*, double>
            (PROP_XCR, aOp1, aOp2, aOp3, aOp4) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[PROP_XCR] << "\t" << mOp1 << ", " << mOp2 << ", " << "'" << *mOp3 << "'" << ", " << mOp4;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class Remainder : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Remainder (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (REMAINDER, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Return : public Instruction_1<TypedRegister> {
    public:
        /* return value */
        Return (TypedRegister aOp1) :
            Instruction_1<TypedRegister>
            (RETURN, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[RETURN] << "\t" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
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
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
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
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            return f;
        }
    };

    class SaveName : public Instruction_2<const StringAtom*, TypedRegister> {
    public:
        /* name, source */
        SaveName (const StringAtom* aOp1, TypedRegister aOp2) :
            Instruction_2<const StringAtom*, TypedRegister>
            (SAVE_NAME, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SAVE_NAME] << "\t" << "'" << *mOp1 << "'" << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class SetElement : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* base, index, value */
        SetElement (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (SET_ELEMENT, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SET_ELEMENT] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first) << ", " << getRegisterValue(registers, mOp3.first);
            return f;
        }
    };

    class SetProp : public Instruction_3<TypedRegister, const StringAtom*, TypedRegister> {
    public:
        /* object, name, source */
        SetProp (TypedRegister aOp1, const StringAtom* aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, const StringAtom*, TypedRegister>
            (SET_PROP, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SET_PROP] << "\t" << mOp1 << ", " << "'" << *mOp2 << "'" << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp3.first);
            return f;
        }
    };

    class SetSlot : public Instruction_3<TypedRegister, uint32, TypedRegister> {
    public:
        /* object, slot number, source */
        SetSlot (TypedRegister aOp1, uint32 aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, uint32, TypedRegister>
            (SET_SLOT, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SET_SLOT] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp3.first);
            return f;
        }
    };

    class SetStatic : public Instruction_3<JSClass*, uint32, TypedRegister> {
    public:
        /* class, index, source */
        SetStatic (JSClass* aOp1, uint32 aOp2, TypedRegister aOp3) :
            Instruction_3<JSClass*, uint32, TypedRegister>
            (SET_STATIC, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SET_STATIC] << "\t" << mOp1->getName() << ", " << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp3.first);
            return f;
        }
    };

    class Shiftleft : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Shiftleft (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (SHIFTLEFT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Shiftright : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Shiftright (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (SHIFTRIGHT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class SlotXcr : public Instruction_4<TypedRegister, TypedRegister, uint32, double> {
    public:
        /* dest, source, slot number, value */
        SlotXcr (TypedRegister aOp1, TypedRegister aOp2, uint32 aOp3, double aOp4) :
            Instruction_4<TypedRegister, TypedRegister, uint32, double>
            (SLOT_XCR, aOp1, aOp2, aOp3, aOp4) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SLOT_XCR] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3 << ", " << mOp4;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class StaticXcr : public Instruction_4<TypedRegister, JSClass*, uint32, double> {
    public:
        /* dest, class, index, value */
        StaticXcr (TypedRegister aOp1, JSClass* aOp2, uint32 aOp3, double aOp4) :
            Instruction_4<TypedRegister, JSClass*, uint32, double>
            (STATIC_XCR, aOp1, aOp2, aOp3, aOp4) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[STATIC_XCR] << "\t" << mOp1 << ", " << mOp2->getName() << ", " << mOp3 << ", " << mOp4;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class StrictEQ : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        StrictEQ (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (STRICT_EQ, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class StrictNE : public Instruction_3<TypedRegister, TypedRegister, TypedRegister> {
    public:
        /* dest, source1, source2 */
        StrictNE (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Instruction_3<TypedRegister, TypedRegister, TypedRegister>
            (STRICT_NE, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Instruction_3<TypedRegister, TypedRegister, TypedRegister> */
    };

    class Subtract : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Subtract (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (SUBTRACT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class Super : public Instruction_1<TypedRegister> {
    public:
        /* dest */
        Super (TypedRegister aOp1) :
            Instruction_1<TypedRegister>
            (SUPER, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[SUPER] << "\t" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
            return f;
        }
    };

    class Test : public Instruction_2<TypedRegister, TypedRegister> {
    public:
        /* dest, source */
        Test (TypedRegister aOp1, TypedRegister aOp2) :
            Instruction_2<TypedRegister, TypedRegister>
            (TEST, aOp1, aOp2) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[TEST] << "\t" << mOp1 << ", " << mOp2;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class Throw : public Instruction_1<TypedRegister> {
    public:
        /* exception value */
        Throw (TypedRegister aOp1) :
            Instruction_1<TypedRegister>
            (THROW, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[THROW] << "\t" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
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
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
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
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            return f;
        }
    };

    class Ushiftright : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Ushiftright (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (USHIFTRIGHT, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

    class VarXcr : public Instruction_3<TypedRegister, TypedRegister, double> {
    public:
        /* dest, source, value */
        VarXcr (TypedRegister aOp1, TypedRegister aOp2, double aOp3) :
            Instruction_3<TypedRegister, TypedRegister, double>
            (VAR_XCR, aOp1, aOp2, aOp3) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[VAR_XCR] << "\t" << mOp1 << ", " << mOp2 << ", " << mOp3;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first) << ", " << getRegisterValue(registers, mOp2.first);
            return f;
        }
    };

    class Within : public Instruction_1<TypedRegister> {
    public:
        /* within this object */
        Within (TypedRegister aOp1) :
            Instruction_1<TypedRegister>
            (WITHIN, aOp1) {};
        virtual Formatter& print(Formatter& f) {
            f << opcodeNames[WITHIN] << "\t" << mOp1;
            return f;
        }
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            f << getRegisterValue(registers, mOp1.first);
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
        virtual Formatter& printOperands(Formatter& f, const JSValues& registers) {
            return f;
        }
    };

    class Xor : public Arithmetic {
    public:
        /* dest, source1, source2 */
        Xor (TypedRegister aOp1, TypedRegister aOp2, TypedRegister aOp3) :
            Arithmetic
            (XOR, aOp1, aOp2, aOp3) {};
        /* print() and printOperands() inherited from Arithmetic */
    };

#else

    char *opcodeNames[] = {
        "ADD              ",
        "AND              ",
        "BITNOT           ",
        "BRANCH           ",
        "BRANCH_FALSE     ",
        "BRANCH_TRUE      ",
        "CALL             ",
        "CAST             ",
        "COMPARE_EQ       ",
        "COMPARE_GE       ",
        "COMPARE_GT       ",
        "COMPARE_IN       ",
        "COMPARE_LE       ",
        "COMPARE_LT       ",
        "COMPARE_NE       ",
        "DEBUGGER         ",
        "DELETE_PROP      ",
        "DIRECT_CALL      ",
        "DIVIDE           ",
        "ELEM_XCR         ",
        "GENERIC_BINARY_OP",
        "GET_ELEMENT      ",
        "GET_METHOD       ",
        "GET_PROP         ",
        "GET_SLOT         ",
        "GET_STATIC       ",
        "INSTANCEOF       ",
        "JSR              ",
        "LOAD_BOOLEAN     ",
        "LOAD_IMMEDIATE   ",
        "LOAD_NAME        ",
        "LOAD_STRING      ",
        "MOVE             ",
        "MULTIPLY         ",
        "NAME_XCR         ",
        "NEGATE           ",
        "NEW_ARRAY        ",
        "NEW_CLASS        ",
        "NEW_FUNCTION     ",
        "NEW_OBJECT       ",
        "NOP              ",
        "NOT              ",
        "OR               ",
        "POSATE           ",
        "PROP_XCR         ",
        "REMAINDER        ",
        "RETURN           ",
        "RETURN_VOID      ",
        "RTS              ",
        "SAVE_NAME        ",
        "SET_ELEMENT      ",
        "SET_PROP         ",
        "SET_SLOT         ",
        "SET_STATIC       ",
        "SHIFTLEFT        ",
        "SHIFTRIGHT       ",
        "SLOT_XCR         ",
        "STATIC_XCR       ",
        "STRICT_EQ        ",
        "STRICT_NE        ",
        "SUBTRACT         ",
        "SUPER            ",
        "TEST             ",
        "THROW            ",
        "TRYIN            ",
        "TRYOUT           ",
        "USHIFTRIGHT      ",
        "VAR_XCR          ",
        "WITHIN           ",
        "WITHOUT          ",
        "XOR              ",
    };

#endif

