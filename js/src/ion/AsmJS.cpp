/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsmath.h"
#include "jsworkers.h"

#include "frontend/ParseNode.h"
#include "ion/AsmJS.h"
#include "ion/AsmJSModule.h"

#include "frontend/ParseNode-inl.h"

using namespace js;
using namespace js::frontend;
using namespace mozilla;

#include "ion/CodeGenerator.h"
#include "ion/MIR.h"
#include "ion/MIRGraph.h"

using namespace js::ion;

#ifdef JS_ASMJS

/*****************************************************************************/
// ParseNode utilities

static inline ParseNode *
NextNode(ParseNode *pn)
{
    return pn->pn_next;
}

static inline ParseNode *
UnaryKid(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_UNARY));
    return pn->pn_kid;
}

static inline ParseNode *
BinaryRight(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_BINARY));
    return pn->pn_right;
}

static inline ParseNode *
BinaryLeft(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_BINARY));
    return pn->pn_left;
}

static inline ParseNode *
TernaryKid1(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid1;
}

static inline ParseNode *
TernaryKid2(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid2;
}

static inline ParseNode *
TernaryKid3(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid3;
}

static inline ParseNode *
ListHead(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    return pn->pn_head;
}

static inline unsigned
ListLength(ParseNode *pn)
{
    JS_ASSERT(pn->isArity(PN_LIST));
    return pn->pn_count;
}

static inline ParseNode *
CallCallee(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CALL));
    return ListHead(pn);
}

static inline unsigned
CallArgListLength(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CALL));
    JS_ASSERT(ListLength(pn) >= 1);
    return ListLength(pn) - 1;
}

static inline ParseNode *
CallArgList(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CALL));
    return NextNode(ListHead(pn));
}

static inline ParseNode *
VarListHead(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_VAR));
    return ListHead(pn);
}

static inline ParseNode *
CaseExpr(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CASE) || pn->isKind(PNK_DEFAULT));
    return BinaryLeft(pn);
}

static inline ParseNode *
CaseBody(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_CASE) || pn->isKind(PNK_DEFAULT));
    return BinaryRight(pn);
}

static inline JSAtom *
StringAtom(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_STRING));
    return pn->pn_atom;
}

static inline bool
IsExpressionStatement(ParseNode *pn)
{
    return pn->isKind(PNK_SEMI);
}

static inline ParseNode *
ExpressionStatementExpr(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_SEMI));
    return UnaryKid(pn);
}

static inline PropertyName *
LoopControlMaybeLabel(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_BREAK) || pn->isKind(PNK_CONTINUE));
    JS_ASSERT(pn->isArity(PN_NULLARY));
    return pn->as<LoopControlStatement>().label();
}

static inline PropertyName *
LabeledStatementLabel(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_COLON));
    return pn->pn_atom->asPropertyName();
}

static inline ParseNode *
LabeledStatementStatement(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_COLON));
    return pn->expr();
}

static double
NumberNodeValue(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_NUMBER));
    return pn->pn_dval;
}

static bool
NumberNodeHasFrac(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_NUMBER));
    return pn->pn_u.number.decimalPoint == HasDecimal;
}

static ParseNode *
DotBase(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_DOT));
    JS_ASSERT(pn->isArity(PN_NAME));
    return pn->expr();
}

static PropertyName *
DotMember(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_DOT));
    JS_ASSERT(pn->isArity(PN_NAME));
    return pn->pn_atom->asPropertyName();
}

static ParseNode *
ElemBase(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_ELEM));
    return BinaryLeft(pn);
}

static ParseNode *
ElemIndex(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_ELEM));
    return BinaryRight(pn);
}

static inline JSFunction *
FunctionObject(ParseNode *fn)
{
    JS_ASSERT(fn->isKind(PNK_FUNCTION));
    JS_ASSERT(fn->isArity(PN_CODE));
    return fn->pn_funbox->function();
}

static inline PropertyName *
FunctionName(ParseNode *fn)
{
    if (JSAtom *atom = FunctionObject(fn)->atom())
        return atom->asPropertyName();
    return NULL;
}

static inline ParseNode *
FunctionArgsList(ParseNode *fn, unsigned *numFormals)
{
    JS_ASSERT(fn->isKind(PNK_FUNCTION));
    ParseNode *argsBody = fn->pn_body;
    JS_ASSERT(argsBody->isKind(PNK_ARGSBODY));
    *numFormals = argsBody->pn_count - 1;
    return ListHead(argsBody);
}

static inline unsigned
FunctionNumFormals(ParseNode *fn)
{
    unsigned numFormals;
    FunctionArgsList(fn, &numFormals);
    return numFormals;
}

static inline bool
FunctionHasStatementList(ParseNode *fn)
{
    JS_ASSERT(fn->isKind(PNK_FUNCTION));
    ParseNode *argsBody = fn->pn_body;
    JS_ASSERT(argsBody->isKind(PNK_ARGSBODY));
    ParseNode *body = argsBody->last();
    return body->isKind(PNK_STATEMENTLIST);
}

static inline ParseNode *
FunctionStatementList(ParseNode *fn)
{
    JS_ASSERT(FunctionHasStatementList(fn));
    return fn->pn_body->last();
}

static inline ParseNode *
FunctionLastStatementOrNull(ParseNode *fn)
{
    ParseNode *list = FunctionStatementList(fn);
    return list->pn_count == 0 ? NULL : list->last();
}

static inline bool
IsNormalObjectField(JSContext *cx, ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_COLON));
    return pn->getOp() == JSOP_INITPROP &&
           BinaryLeft(pn)->isKind(PNK_NAME) &&
           BinaryLeft(pn)->name() != cx->names().proto;
}

static inline PropertyName *
ObjectNormalFieldName(JSContext *cx, ParseNode *pn)
{
    JS_ASSERT(IsNormalObjectField(cx, pn));
    return BinaryLeft(pn)->name();
}

static inline ParseNode *
ObjectFieldInitializer(ParseNode *pn)
{
    JS_ASSERT(pn->isKind(PNK_COLON));
    return BinaryRight(pn);
}

static inline bool
IsDefinition(ParseNode *pn)
{
    return pn->isKind(PNK_NAME) && pn->isDefn();
}

static inline ParseNode *
MaybeDefinitionInitializer(ParseNode *pn)
{
    JS_ASSERT(IsDefinition(pn));
    return pn->expr();
}

static inline bool
IsUseOfName(ParseNode *pn, PropertyName *name)
{
    return pn->isKind(PNK_NAME) && pn->name() == name;
}

static inline ParseNode *
SkipEmptyStatements(ParseNode *pn)
{
    while (pn && pn->isKind(PNK_SEMI) && !UnaryKid(pn))
        pn = pn->pn_next;
    return pn;
}

static inline ParseNode *
NextNonEmptyStatement(ParseNode *pn)
{
    return SkipEmptyStatements(pn->pn_next);
}

/*****************************************************************************/

// Respresents the type of a general asm.js expression.
class Type
{
  public:
    enum Which {
        Double,
        Doublish,
        Fixnum,
        Int,
        Signed,
        Unsigned,
        Intish,
        Void
    };

  private:
    Which which_;

  public:
    Type() : which_(Which(-1)) {}
    Type(Which w) : which_(w) {}

    bool operator==(Type rhs) const { return which_ == rhs.which_; }
    bool operator!=(Type rhs) const { return which_ != rhs.which_; }

    bool isSigned() const {
        return which_ == Signed || which_ == Fixnum;
    }

    bool isUnsigned() const {
        return which_ == Unsigned || which_ == Fixnum;
    }

    bool isInt() const {
        return isSigned() || isUnsigned() || which_ == Int;
    }

    bool isIntish() const {
        return isInt() || which_ == Intish;
    }

    bool isDouble() const {
        return which_ == Double;
    }

    bool isDoublish() const {
        return isDouble() || which_ == Doublish;
    }

    bool isVoid() const {
        return which_ == Void;
    }

    bool isExtern() const {
        return isDouble() || isSigned();
    }

    MIRType toMIRType() const {
        switch (which_) {
          case Double:
          case Doublish:
            return MIRType_Double;
          case Fixnum:
          case Int:
          case Signed:
          case Unsigned:
          case Intish:
            return MIRType_Int32;
          case Void:
            return MIRType_None;
        }
        JS_NOT_REACHED("Invalid Type");
        return MIRType_None;
    }
};

// Represents the subset of Type that can be used as the return type of a
// function.
class RetType
{
  public:
    enum Which {
        Void = Type::Void,
        Signed = Type::Signed,
        Double = Type::Double
    };

  private:
    Which which_;

  public:
    RetType() {}
    RetType(Which w) : which_(w) {}
    RetType(AsmJSCoercion coercion) {
        switch (coercion) {
          case AsmJS_ToInt32: which_ = Signed; break;
          case AsmJS_ToNumber: which_ = Double; break;
        }
    }
    Which which() const {
        return which_;
    }
    Type toType() const {
        return Type::Which(which_);
    }
    AsmJSModule::ReturnType toModuleReturnType() const {
        switch (which_) {
          case Void: return AsmJSModule::Return_Void;
          case Signed: return AsmJSModule::Return_Int32;
          case Double: return AsmJSModule::Return_Double;
        }
        JS_NOT_REACHED("Unexpected return type");
        return AsmJSModule::Return_Void;
    }
    MIRType toMIRType() const {
        switch (which_) {
          case Void: return MIRType_None;
          case Signed: return MIRType_Int32;
          case Double: return MIRType_Double;
        }
        JS_NOT_REACHED("Unexpected return type");
        return MIRType_None;
    }
    bool operator==(RetType rhs) const { return which_ == rhs.which_; }
    bool operator!=(RetType rhs) const { return which_ != rhs.which_; }
};

// Implements <: (subtype) operator when the rhs is an RetType
static inline bool
operator<=(Type lhs, RetType rhs)
{
    switch (rhs.which()) {
      case RetType::Signed: return lhs.isSigned();
      case RetType::Double: return lhs == Type::Double;
      case RetType::Void:   return lhs == Type::Void;
    }
    JS_NOT_REACHED("Unexpected rhs type");
    return false;
}

// Represents the subset of Type that can be used as a variable or
// argument's type. Note: AsmJSCoercion and VarType are kept separate to
// make very clear the signed/int distinction: a coercion may explicitly sign
// an *expression* but, when stored as a variable, this signedness information
// is explicitly thrown away by the asm.js type system. E.g., in
//
//   function f(i) {
//     i = i | 0;             (1)
//     if (...)
//         i = foo() >>> 0;
//     else
//         i = bar() | 0;
//     return i | 0;          (2)
//
// the AsmJSCoercion of (1) is Signed (since | performs ToInt32) but, when
// translated to an VarType, the result is a plain Int since, as shown, it
// is legal to assign both Signed and Unsigned (or some other Int) values to
// it. For (2), the AsmJSCoercion is also Signed but, when translated to an
// RetType, the result is Signed since callers (asm.js and non-asm.js) can
// rely on the return value being Signed.
class VarType
{
  public:
    enum Which {
        Int = Type::Int,
        Double = Type::Double
    };

  private:
    Which which_;

  public:
    VarType()
      : which_(Which(-1)) {}
    VarType(Which w)
      : which_(w) {}
    VarType(AsmJSCoercion coercion) {
        switch (coercion) {
          case AsmJS_ToInt32: which_ = Int; break;
          case AsmJS_ToNumber: which_ = Double; break;
        }
    }
    Which which() const {
        return which_;
    }
    Type toType() const {
        return Type::Which(which_);
    }
    MIRType toMIRType() const {
        return which_ == Int ? MIRType_Int32 : MIRType_Double;
    }
    AsmJSCoercion toCoercion() const {
        return which_ == Int ? AsmJS_ToInt32 : AsmJS_ToNumber;
    }
    static VarType FromMIRType(MIRType type) {
        JS_ASSERT(type == MIRType_Int32 || type == MIRType_Double);
        return type == MIRType_Int32 ? Int : Double;
    }
    bool operator==(VarType rhs) const { return which_ == rhs.which_; }
    bool operator!=(VarType rhs) const { return which_ != rhs.which_; }
};

// Implements <: (subtype) operator when the rhs is an VarType
static inline bool
operator<=(Type lhs, VarType rhs)
{
    switch (rhs.which()) {
      case VarType::Int:    return lhs.isInt();
      case VarType::Double: return lhs.isDouble();
    }
    JS_NOT_REACHED("Unexpected rhs type");
    return false;
}

// Passed from parent expressions to child expressions to indicate if and how
// the child expression's result will be coerced. While most type checking
// occurs bottom-up (with child expressions returning the type of the result
// and parents checking these types), FFI calls naturally want to know the
// parent's context to determine the appropriate result type. If a parent
// passes NoCoercion to an FFI all, then the FFI's return type will be "Void"
// which will cause a type error if the result is used.
//
// The other application of Use is to support the asm.js type rule which
// allows (a-b+c-d+e)|0 without intermediate conversions. The type system has
// only binary +/- nodes so we simulate the n-ary expression by having the
// outer parent +/- expression pass in Use::AddOrSub so that the inner
// expression knows to return type Int instead of Intish.
class Use
{
  public:
    enum Which {
        NoCoercion,
        ToInt32,
        ToNumber,
        AddOrSub
    };

  private:
    Which which_;
    unsigned *pcount_;

  public:
    Use()
      : which_(Which(-1)), pcount_(NULL) {}
    Use(Which w)
      : which_(w), pcount_(NULL) { JS_ASSERT(w != AddOrSub); }
    Use(unsigned *pcount)
      : which_(AddOrSub), pcount_(pcount) {}
    Which which() const {
        return which_;
    }
    unsigned &addOrSubCount() const {
        JS_ASSERT(which_ == AddOrSub);
        return *pcount_;
    }
    Type toFFIReturnType() const {
        switch (which_) {
          case NoCoercion: return Type::Void;
          case ToInt32: return Type::Intish;
          case ToNumber: return Type::Doublish;
          case AddOrSub: return Type::Void;
        }
        JS_NOT_REACHED("unexpected use type");
        return Type::Void;
    }
    MIRType toMIRType() const {
        switch (which_) {
          case NoCoercion: return MIRType_None;
          case ToInt32: return MIRType_Int32;
          case ToNumber: return MIRType_Double;
          case AddOrSub: return MIRType_None;
        }
        JS_NOT_REACHED("unexpected use type");
        return MIRType_None;
    }
    bool operator==(Use rhs) const { return which_ == rhs.which_; }
    bool operator!=(Use rhs) const { return which_ != rhs.which_; }
};

/*****************************************************************************/
// Numeric literal utilities

// Represents the type and value of an asm.js numeric literal.
//
// A literal is a double iff the literal contains an exponent or decimal point
// (even if the fractional part is 0). Otherwise, integers may be classified:
//  fixnum: [0, 2^31)
//  negative int: [-2^31, 0)
//  big unsigned: [2^31, 2^32)
//  out of range: otherwise
class NumLit
{
  public:
    enum Which {
        Fixnum = Type::Fixnum,
        NegativeInt = Type::Signed,
        BigUnsigned = Type::Unsigned,
        Double = Type::Double,
        OutOfRangeInt = -1
    };

  private:
    Which which_;
    Value v_;

  public:
    NumLit(Which w, Value v)
      : which_(w), v_(v)
    {}

    Which which() const {
        return which_;
    }

    int32_t toInt32() const {
        JS_ASSERT(which_ == Fixnum || which_ == NegativeInt || which_ == BigUnsigned);
        return v_.toInt32();
    }

    double toDouble() const {
        return v_.toDouble();
    }

    Type type() const {
        JS_ASSERT(which_ != OutOfRangeInt);
        return Type::Which(which_);
    }

    Value value() const {
        JS_ASSERT(which_ != OutOfRangeInt);
        return v_;
    }
};

// Note: '-' is never rolled into the number; numbers are always positive and
// negations must be applied manually.
static bool
IsNumericLiteral(ParseNode *pn)
{
    return pn->isKind(PNK_NUMBER) ||
           (pn->isKind(PNK_NEG) && UnaryKid(pn)->isKind(PNK_NUMBER));
}

static NumLit
ExtractNumericLiteral(ParseNode *pn)
{
    JS_ASSERT(IsNumericLiteral(pn));
    ParseNode *numberNode;
    double d;
    if (pn->isKind(PNK_NEG)) {
        numberNode = UnaryKid(pn);
        d = -NumberNodeValue(numberNode);
    } else {
        numberNode = pn;
        d = NumberNodeValue(numberNode);
    }

    if (NumberNodeHasFrac(numberNode))
        return NumLit(NumLit::Double, DoubleValue(d));

    int64_t i64 = int64_t(d);
    if (d != double(i64))
        return NumLit(NumLit::OutOfRangeInt, UndefinedValue());

    if (i64 >= 0) {
        if (i64 <= INT32_MAX)
            return NumLit(NumLit::Fixnum, Int32Value(i64));
        if (i64 <= UINT32_MAX)
            return NumLit(NumLit::BigUnsigned, Int32Value(uint32_t(i64)));
        return NumLit(NumLit::OutOfRangeInt, UndefinedValue());
    }
    if (i64 >= INT32_MIN)
        return NumLit(NumLit::NegativeInt, Int32Value(i64));
    return NumLit(NumLit::OutOfRangeInt, UndefinedValue());
}

static inline bool
IsLiteralUint32(ParseNode *pn, uint32_t *u32)
{
    if (!IsNumericLiteral(pn))
        return false;

    NumLit literal = ExtractNumericLiteral(pn);
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::BigUnsigned:
        *u32 = uint32_t(literal.toInt32());
        return true;
      case NumLit::NegativeInt:
      case NumLit::Double:
      case NumLit::OutOfRangeInt:
        return false;
    }

    JS_NOT_REACHED("Bad literal type");
}

static inline bool
IsBits32(ParseNode *pn, int32_t i)
{
    if (!IsNumericLiteral(pn))
        return false;

    NumLit literal = ExtractNumericLiteral(pn);
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::BigUnsigned:
      case NumLit::NegativeInt:
        return literal.toInt32() == i;
      case NumLit::Double:
      case NumLit::OutOfRangeInt:
        return false;
    }

    JS_NOT_REACHED("Bad literal type");
}

/*****************************************************************************/
// Typed array utilities

static Type
TypedArrayLoadType(ArrayBufferView::ViewType viewType)
{
    switch (viewType) {
      case ArrayBufferView::TYPE_INT8:
      case ArrayBufferView::TYPE_INT16:
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT8:
      case ArrayBufferView::TYPE_UINT16:
      case ArrayBufferView::TYPE_UINT32:
        return Type::Intish;
      case ArrayBufferView::TYPE_FLOAT32:
      case ArrayBufferView::TYPE_FLOAT64:
        return Type::Doublish;
      default:;
    }
    JS_NOT_REACHED("Unexpected array type");
    return Type();
}

enum ArrayStoreEnum {
    ArrayStore_Intish,
    ArrayStore_Double
};

static ArrayStoreEnum
TypedArrayStoreType(ArrayBufferView::ViewType viewType)
{
    switch (viewType) {
      case ArrayBufferView::TYPE_INT8:
      case ArrayBufferView::TYPE_INT16:
      case ArrayBufferView::TYPE_INT32:
      case ArrayBufferView::TYPE_UINT8:
      case ArrayBufferView::TYPE_UINT16:
      case ArrayBufferView::TYPE_UINT32:
        return ArrayStore_Intish;
      case ArrayBufferView::TYPE_FLOAT32:
      case ArrayBufferView::TYPE_FLOAT64:
        return ArrayStore_Double;
      default:;
    }
    JS_NOT_REACHED("Unexpected array type");
    return ArrayStore_Double;
}

/*****************************************************************************/

typedef Vector<PropertyName*,1> LabelVector;
typedef Vector<MBasicBlock*,16> CaseVector;

// ModuleCompiler encapsulates the compilation of an entire asm.js module. Over
// the course of an ModuleCompiler object's lifetime, many FunctionCompiler
// objects will be created and destroyed in sequence, one for each function in
// the module.
//
// *** asm.js FFI calls ***
//
// asm.js allows calling out to non-asm.js via "FFI calls". The asm.js type
// system does not place any constraints on the FFI call. In particular:
//  - an FFI call's target is not known or speculated at module-compile time;
//  - a single external function can be called with different signatures.
//
// If performance didn't matter, all FFI calls could simply box their arguments
// and call js::Invoke. However, we'd like to be able to specialize FFI calls
// to be more efficient in several cases:
//
//  - for calls to JS functions which have been jitted, we'd like to call
//    directly into JIT code without going through C++.
//
//  - for calls to certain builtins, we'd like to be call directly into the C++
//    code for the builtin without going through the general call path.
//
// All of this requires dynamic specialization techniques which must happen
// after module compilation. To support this, at module-compilation time, each
// FFI call generates a call signature according to the system ABI, as if the
// callee was a C++ function taking/returning the same types as the caller was
// passing/expecting. The callee is loaded from a fixed offset in the global
// data array which allows the callee to change at runtime. Initially, the
// callee is stub which boxes its arguments and calls js::Invoke.
//
// To do this, we need to generate a callee stub for each pairing of FFI callee
// and signature. We call this pairing an "exit". For example, this code has
// two external functions and three exits:
//
//  function f(global, imports) {
//    "use asm";
//    var foo = imports.foo;
//    var bar = imports.bar;
//    function g() {
//      foo(1);      // Exit #1: (int) -> void
//      foo(1.5);    // Exit #2: (double) -> void
//      bar(1)|0;    // Exit #3: (int) -> int
//      bar(2)|0;    // Exit #3: (int) -> int
//    }
//
// The ModuleCompiler maintains a hash table (ExitMap) which allows a call site
// to add a new exit or reuse an existing one. The key is an ExitDescriptor
// (which holds the exit pairing) and the value is an index into the
// Vector<Exit> stored in the AsmJSModule.
class ModuleCompiler
{
  public:
    class Func
    {
        ParseNode *fn_;
        ParseNode *body_;
        MIRTypeVector argTypes_;
        RetType returnType_;
        mutable Label code_;

      public:
        Func(ParseNode *fn, ParseNode *body, MoveRef<MIRTypeVector> types, RetType returnType)
          : fn_(fn),
            body_(body),
            argTypes_(types),
            returnType_(returnType),
            code_()
        {}

        Func(MoveRef<Func> rhs)
          : fn_(rhs->fn_),
            body_(rhs->body_),
            argTypes_(Move(rhs->argTypes_)),
            returnType_(rhs->returnType_),
            code_(rhs->code_)
        {}

        ~Func()
        {
            // Avoid spurious Label assertions on compilation failure.
            if (!code_.bound())
                code_.bind(0);
        }

        ParseNode *fn() const { return fn_; }
        ParseNode *body() const { return body_; }
        unsigned numArgs() const { return argTypes_.length(); }
        VarType argType(unsigned i) const { return VarType::FromMIRType(argTypes_[i]); }
        const MIRTypeVector &argMIRTypes() const { return argTypes_; }
        RetType returnType() const { return returnType_; }
        Label *codeLabel() const { return &code_; }
    };

    class Global
    {
      public:
        enum Which { Variable, Function, FuncPtrTable, FFI, ArrayView, MathBuiltin, Constant };

      private:
        Which which_;
        union {
            struct {
                uint32_t index_;
                VarType::Which type_;
            } var;
            uint32_t funcIndex_;
            uint32_t funcPtrTableIndex_;
            uint32_t ffiIndex_;
            ArrayBufferView::ViewType viewType_;
            AsmJSMathBuiltin mathBuiltin_;
            double constant_;
        } u;

        friend class ModuleCompiler;

        Global(Which which) : which_(which) {}

      public:
        Which which() const {
            return which_;
        }
        VarType varType() const {
            JS_ASSERT(which_ == Variable);
            return VarType(u.var.type_);
        }
        uint32_t varIndex() const {
            JS_ASSERT(which_ == Variable);
            return u.var.index_;
        }
        uint32_t funcIndex() const {
            JS_ASSERT(which_ == Function);
            return u.funcIndex_;
        }
        uint32_t funcPtrTableIndex() const {
            JS_ASSERT(which_ == FuncPtrTable);
            return u.funcPtrTableIndex_;
        }
        unsigned ffiIndex() const {
            JS_ASSERT(which_ == FFI);
            return u.ffiIndex_;
        }
        ArrayBufferView::ViewType viewType() const {
            JS_ASSERT(which_ == ArrayView);
            return u.viewType_;
        }
        AsmJSMathBuiltin mathBuiltin() const {
            JS_ASSERT(which_ == MathBuiltin);
            return u.mathBuiltin_;
        }
        double constant() const {
            JS_ASSERT(which_ == Constant);
            return u.constant_;
        }
    };

    typedef Vector<const Func*> FuncPtrVector;

    class FuncPtrTable
    {
        FuncPtrVector elems_;
        unsigned baseIndex_;

      public:
        FuncPtrTable(MoveRef<FuncPtrVector> elems, unsigned baseIndex)
          : elems_(elems), baseIndex_(baseIndex) {}
        FuncPtrTable(MoveRef<FuncPtrTable> rhs)
          : elems_(Move(rhs->elems_)), baseIndex_(rhs->baseIndex_) {}

        const Func &sig() const { return *elems_[0]; }
        unsigned numElems() const { return elems_.length(); }
        const Func &elem(unsigned i) const { return *elems_[i]; }
        unsigned baseIndex() const { return baseIndex_; }
        unsigned mask() const { JS_ASSERT(IsPowerOfTwo(numElems())); return numElems() - 1; }
    };

    typedef Vector<FuncPtrTable> FuncPtrTableVector;

    class ExitDescriptor
    {
        PropertyName *name_;
        MIRTypeVector argTypes_;
        Use use_;

      public:
        ExitDescriptor(PropertyName *name, MoveRef<MIRTypeVector> argTypes, Use use)
          : name_(name),
            argTypes_(argTypes),
            use_(use)
        {}
        ExitDescriptor(MoveRef<ExitDescriptor> rhs)
          : name_(rhs->name_),
            argTypes_(Move(rhs->argTypes_)),
            use_(rhs->use_)
        {}
        const MIRTypeVector &argTypes() const {
            return argTypes_;
        }
        Use use() const {
            return use_;
        }

        // ExitDescriptor is a HashPolicy:
        typedef ExitDescriptor Lookup;
        static HashNumber hash(const ExitDescriptor &d) {
            HashNumber hn = HashGeneric(d.name_, d.use_.which());
            for (unsigned i = 0; i < d.argTypes_.length(); i++)
                hn = AddToHash(hn, d.argTypes_[i]);
            return hn;
        }
        static bool match(const ExitDescriptor &lhs, const ExitDescriptor &rhs) {
            if (lhs.name_ != rhs.name_ ||
                lhs.argTypes_.length() != rhs.argTypes_.length() ||
                lhs.use_ != rhs.use_)
            {
                return false;
            }
            for (unsigned i = 0; i < lhs.argTypes_.length(); i++) {
                if (lhs.argTypes_[i] != rhs.argTypes_[i])
                    return false;
            }
            return true;
        }
    };

    typedef HashMap<ExitDescriptor, unsigned, ExitDescriptor, ContextAllocPolicy> ExitMap;

  private:
    typedef HashMap<PropertyName*, AsmJSMathBuiltin> MathNameMap;
    typedef HashMap<PropertyName*, Global> GlobalMap;
    typedef Vector<Func> FuncVector;
    typedef Vector<AsmJSGlobalAccess> GlobalAccessVector;

    JSContext *                    cx_;
    IonContext                     ictx_;
    MacroAssembler                 masm_;

    ScopedJSDeletePtr<AsmJSModule> module_;

    PropertyName *                 moduleFunctionName_;

    GlobalMap                      globals_;
    FuncVector                     functions_;
    FuncPtrTableVector             funcPtrTables_;
    ExitMap                        exits_;
    MathNameMap                    standardLibraryMathNames_;
    GlobalAccessVector             globalAccesses_;
    Label                          stackOverflowLabel_;
    Label                          operationCallbackLabel_;

    const char *                   errorString_;
    ParseNode *                    errorNode_;
    TokenStream &                  tokenStream_;

    DebugOnly<int>                 currentPass_;

    bool addStandardLibraryMathName(const char *name, AsmJSMathBuiltin builtin) {
        JSAtom *atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        return standardLibraryMathNames_.putNew(atom->asPropertyName(), builtin);
    }

  public:
    ModuleCompiler(JSContext *cx, TokenStream &ts)
      : cx_(cx),
        ictx_(cx->runtime),
        masm_(cx),
        moduleFunctionName_(NULL),
        globals_(cx),
        functions_(cx),
        funcPtrTables_(cx),
        exits_(cx),
        standardLibraryMathNames_(cx),
        globalAccesses_(cx),
        errorString_(NULL),
        errorNode_(NULL),
        tokenStream_(ts),
        currentPass_(1)
    {}

    ~ModuleCompiler() {
        if (errorString_)
            tokenStream_.reportAsmJSError(errorNode_->pn_pos.begin, JSMSG_USE_ASM_TYPE_FAIL,
                                          errorString_);

        // Avoid spurious Label assertions on compilation failure.
        if (!stackOverflowLabel_.bound())
            stackOverflowLabel_.bind(0);
        if (!operationCallbackLabel_.bound())
            operationCallbackLabel_.bind(0);
    }

    bool init() {
        if (!cx_->compartment->ensureIonCompartmentExists(cx_))
            return false;

        if (!globals_.init() || !exits_.init())
            return false;

        if (!standardLibraryMathNames_.init() ||
            !addStandardLibraryMathName("sin", AsmJSMathBuiltin_sin) ||
            !addStandardLibraryMathName("cos", AsmJSMathBuiltin_cos) ||
            !addStandardLibraryMathName("tan", AsmJSMathBuiltin_tan) ||
            !addStandardLibraryMathName("asin", AsmJSMathBuiltin_asin) ||
            !addStandardLibraryMathName("acos", AsmJSMathBuiltin_acos) ||
            !addStandardLibraryMathName("atan", AsmJSMathBuiltin_atan) ||
            !addStandardLibraryMathName("ceil", AsmJSMathBuiltin_ceil) ||
            !addStandardLibraryMathName("floor", AsmJSMathBuiltin_floor) ||
            !addStandardLibraryMathName("exp", AsmJSMathBuiltin_exp) ||
            !addStandardLibraryMathName("log", AsmJSMathBuiltin_log) ||
            !addStandardLibraryMathName("pow", AsmJSMathBuiltin_pow) ||
            !addStandardLibraryMathName("sqrt", AsmJSMathBuiltin_sqrt) ||
            !addStandardLibraryMathName("abs", AsmJSMathBuiltin_abs) ||
            !addStandardLibraryMathName("atan2", AsmJSMathBuiltin_atan2) ||
            !addStandardLibraryMathName("imul", AsmJSMathBuiltin_imul))
        {
            return false;
        }

        module_ = cx_->new_<AsmJSModule>(cx_);
        if (!module_)
            return false;

        return true;
    }

    bool fail(const char *str, ParseNode *pn) {
        JS_ASSERT(!errorString_);
        JS_ASSERT(!errorNode_);
        JS_ASSERT(str);
        JS_ASSERT(pn);
        errorString_ = str;
        errorNode_ = pn;
        return false;
    }

    /*************************************************** Read-only interface */

    JSContext *cx() const { return cx_; }
    MacroAssembler &masm() { return masm_; }
    Label &stackOverflowLabel() { return stackOverflowLabel_; }
    Label &operationCallbackLabel() { return operationCallbackLabel_; }
    bool hasError() const { return errorString_ != NULL; }
    const AsmJSModule &module() const { return *module_.get(); }

    PropertyName *moduleFunctionName() const { return moduleFunctionName_; }

    const Global *lookupGlobal(PropertyName *name) const {
        if (GlobalMap::Ptr p = globals_.lookup(name))
            return &p->value;
        return NULL;
    }
    const FuncPtrTable *lookupFuncPtrTable(PropertyName *name) const {
        if (GlobalMap::Ptr p = globals_.lookup(name)) {
            if (p->value.which() == Global::FuncPtrTable)
                return &funcPtrTables_[p->value.funcPtrTableIndex()];
        }
        return NULL;
    }
    const Func *lookupFunction(PropertyName *name) const {
        if (GlobalMap::Ptr p = globals_.lookup(name)) {
            if (p->value.which() == Global::Function)
                return &functions_[p->value.funcIndex()];
        }
        return NULL;
    }
    unsigned numFunctions() const {
        return functions_.length();
    }
    const Func &function(unsigned i) const {
        return functions_[i];
    }
    bool lookupStandardLibraryMathName(PropertyName *name, AsmJSMathBuiltin *mathBuiltin) const {
        if (MathNameMap::Ptr p = standardLibraryMathNames_.lookup(name)) {
            *mathBuiltin = p->value;
            return true;
        }
        return false;
    }
    ExitMap::Range allExits() const {
        return exits_.all();
    }

    /***************************************************** Mutable interface */

    void initModuleFunctionName(PropertyName *n) { moduleFunctionName_ = n; }

    void initGlobalArgumentName(PropertyName *n) { module_->initGlobalArgumentName(n); }
    void initImportArgumentName(PropertyName *n) { module_->initImportArgumentName(n); }
    void initBufferArgumentName(PropertyName *n) { module_->initBufferArgumentName(n); }

    bool addGlobalVarInitConstant(PropertyName *varName, VarType type, const Value &v) {
        JS_ASSERT(currentPass_ == 1);
        Global g(Global::Variable);
        uint32_t index;
        if (!module_->addGlobalVarInitConstant(v, &index))
            return false;
        g.u.var.index_ = index;
        g.u.var.type_ = type.which();
        return globals_.putNew(varName, g);
    }
    bool addGlobalVarImport(PropertyName *varName, PropertyName *fieldName, AsmJSCoercion coercion)
    {
        JS_ASSERT(currentPass_ == 1);
        Global g(Global::Variable);
        uint32_t index;
        if (!module_->addGlobalVarImport(fieldName, coercion, &index))
            return false;
        g.u.var.index_ = index;
        g.u.var.type_ = VarType(coercion).which();
        return globals_.putNew(varName, g);
    }
    bool addFunction(MoveRef<Func> func) {
        JS_ASSERT(currentPass_ == 1);
        Global g(Global::Function);
        g.u.funcIndex_ = functions_.length();
        if (!globals_.putNew(FunctionName(func->fn()), g))
            return false;
        return functions_.append(func);
    }
    bool addFuncPtrTable(PropertyName *varName, MoveRef<FuncPtrVector> funcPtrs) {
        JS_ASSERT(currentPass_ == 1);
        Global g(Global::FuncPtrTable);
        g.u.funcPtrTableIndex_ = funcPtrTables_.length();
        if (!globals_.putNew(varName, g))
            return false;
        FuncPtrTable table(funcPtrs, module_->numFuncPtrTableElems());
        if (!module_->incrementNumFuncPtrTableElems(table.numElems()))
            return false;
        return funcPtrTables_.append(Move(table));
    }
    bool addFFI(PropertyName *varName, PropertyName *field) {
        JS_ASSERT(currentPass_ == 1);
        Global g(Global::FFI);
        uint32_t index;
        if (!module_->addFFI(field, &index))
            return false;
        g.u.ffiIndex_ = index;
        return globals_.putNew(varName, g);
    }
    bool addArrayView(PropertyName *varName, ArrayBufferView::ViewType vt, PropertyName *fieldName) {
        JS_ASSERT(currentPass_ == 1);
        Global g(Global::ArrayView);
        if (!module_->addArrayView(vt, fieldName))
            return false;
        g.u.viewType_ = vt;
        return globals_.putNew(varName, g);
    }
    bool addMathBuiltin(PropertyName *varName, AsmJSMathBuiltin mathBuiltin, PropertyName *fieldName) {
        JS_ASSERT(currentPass_ == 1);
        if (!module_->addMathBuiltin(mathBuiltin, fieldName))
            return false;
        Global g(Global::MathBuiltin);
        g.u.mathBuiltin_ = mathBuiltin;
        return globals_.putNew(varName, g);
    }
    bool addGlobalConstant(PropertyName *varName, double constant, PropertyName *fieldName) {
        JS_ASSERT(currentPass_ == 1);
        if (!module_->addGlobalConstant(constant, fieldName))
            return false;
        Global g(Global::Constant);
        g.u.constant_ = constant;
        return globals_.putNew(varName, g);
    }
    bool collectAccesses(MIRGenerator &gen) {
#ifdef JS_CPU_ARM
        if (!module_->addBoundsChecks(gen.asmBoundsChecks()))
            return false;
#else
        if (!module_->addHeapAccesses(gen.heapAccesses()))
            return false;
#endif
        for (unsigned i = 0; i < gen.globalAccesses().length(); i++) {
            if (!globalAccesses_.append(gen.globalAccesses()[i]))
                return false;
        }
        return true;
    }
    bool addGlobalAccess(AsmJSGlobalAccess access) {
        return globalAccesses_.append(access);
    }
    bool addExportedFunction(const Func *func, PropertyName *maybeFieldName) {
        JS_ASSERT(currentPass_ == 1);
        AsmJSModule::ArgCoercionVector argCoercions;
        if (!argCoercions.resize(func->numArgs()))
            return false;
        for (unsigned i = 0; i < func->numArgs(); i++)
            argCoercions[i] = func->argType(i).toCoercion();
        AsmJSModule::ReturnType returnType = func->returnType().toModuleReturnType();
        return module_->addExportedFunction(FunctionObject(func->fn()), maybeFieldName,
                                            Move(argCoercions), returnType);
    }

    void setFirstPassComplete() {
        JS_ASSERT(currentPass_ == 1);
        currentPass_ = 2;
    }

    Func &function(unsigned funcIndex) {
        JS_ASSERT(currentPass_ == 2);
        return functions_[funcIndex];
    }
    bool addExit(unsigned ffiIndex, PropertyName *name, MoveRef<MIRTypeVector> argTypes, Use use,
                 unsigned *exitIndex)
    {
        JS_ASSERT(currentPass_ == 2);
        ExitDescriptor exitDescriptor(name, argTypes, use);
        ExitMap::AddPtr p = exits_.lookupForAdd(exitDescriptor);
        if (p) {
            *exitIndex = p->value;
            return true;
        }
        if (!module_->addExit(ffiIndex, exitIndex))
            return false;
        return exits_.add(p, Move(exitDescriptor), *exitIndex);
    }


    void setSecondPassComplete() {
        JS_ASSERT(currentPass_ == 2);
        masm_.align(gc::PageSize);
        module_->setFunctionBytes(masm_.size());
        currentPass_ = 3;
    }

    void setExitOffset(unsigned exitIndex) {
        JS_ASSERT(currentPass_ == 3);
#if defined(JS_CPU_ARM)
        masm_.flush();
#endif
        module_->exit(exitIndex).initCodeOffset(masm_.size());
    }
    void setEntryOffset(unsigned exportIndex) {
        JS_ASSERT(currentPass_ == 3);
#if defined(JS_CPU_ARM)
        masm_.flush();
#endif
        module_->exportedFunction(exportIndex).initCodeOffset(masm_.size());
    }

    bool finish(ScopedJSDeletePtr<AsmJSModule> *module) {
        // After finishing, the only valid operation on an ModuleCompiler is
        // destruction.
        JS_ASSERT(currentPass_ == 3);
        currentPass_ = -1;

        // Finish the code section.
        masm_.finish();
        if (masm_.oom())
            return false;

        // The global data section sits immediately after the executable (and
        // other) data allocated by the MacroAssembler. Round up bytesNeeded so
        // that doubles/pointers stay aligned.
        size_t codeBytes = AlignBytes(masm_.bytesNeeded(), sizeof(double));
        size_t totalBytes = codeBytes + module_->globalDataBytes();

        // The code must be page aligned, so include extra space so that we can
        // AlignBytes the allocation result below.
        size_t allocedBytes = totalBytes + gc::PageSize;

        // Allocate the slab of memory.
        JSC::ExecutableAllocator *execAlloc = cx_->compartment->ionCompartment()->execAlloc();
        JSC::ExecutablePool *pool;
        uint8_t *unalignedBytes = (uint8_t*)execAlloc->alloc(allocedBytes, &pool, JSC::ASMJS_CODE);
        if (!unalignedBytes)
            return false;
        uint8_t *code = (uint8_t*)AlignBytes((uintptr_t)unalignedBytes, gc::PageSize);

        // The ExecutablePool owns the memory and must be released by the AsmJSModule.
        module_->takeOwnership(pool, code, codeBytes, totalBytes);

        // Copy the buffer into executable memory (c.f. IonCode::copyFrom).
        masm_.executableCopy(code);
        masm_.processCodeLabels(code);
        JS_ASSERT(masm_.jumpRelocationTableBytes() == 0);
        JS_ASSERT(masm_.dataRelocationTableBytes() == 0);
        JS_ASSERT(masm_.preBarrierTableBytes() == 0);
        JS_ASSERT(!masm_.hasEnteredExitFrame());

        // Patch everything that needs an absolute address:

        // Entry points
        for (unsigned i = 0; i < module_->numExportedFunctions(); i++)
            module_->exportedFunction(i).patch(code);

        // Exit points
        for (unsigned i = 0; i < module_->numExits(); i++) {
            module_->exit(i).patch(code);
            module_->exitIndexToGlobalDatum(i).exit = module_->exit(i).code();
            module_->exitIndexToGlobalDatum(i).fun = NULL;
        }
        module_->setOperationCallbackExit(code + masm_.actualOffset(operationCallbackLabel_.offset()));

        // Function-pointer table entries
        unsigned elemIndex = 0;
        for (unsigned i = 0; i < funcPtrTables_.length(); i++) {
            FuncPtrTable &table = funcPtrTables_[i];
            JS_ASSERT(elemIndex == table.baseIndex());
            for (unsigned j = 0; j < table.numElems(); j++) {
                uint8_t *funcPtr = code + masm_.actualOffset(table.elem(j).codeLabel()->offset());
                module_->funcPtrIndexToGlobalDatum(elemIndex++) = funcPtr;
            }
            JS_ASSERT(elemIndex == table.baseIndex() + table.numElems());
        }
        JS_ASSERT(elemIndex == module_->numFuncPtrTableElems());

        // Global accesses in function bodies
#ifdef JS_CPU_ARM
        JS_ASSERT(globalAccesses_.length() == 0);
        // The AsmJSHeapAccess offsets need to be updated to reflect the
        // "actualOffset" (an ARM distinction).
        module_->convertBoundsChecksToActualOffset(masm_);

#else

        for (unsigned i = 0; i < globalAccesses_.length(); i++) {
            AsmJSGlobalAccess access = globalAccesses_[i];
            masm_.patchAsmJSGlobalAccess(access.offset, code, codeBytes, access.globalDataOffset);
        }
#endif
        // The AsmJSHeapAccess offsets need to be updated to reflect the
        // "actualOffset" (an ARM distinction).
        for (unsigned i = 0; i < module_->numHeapAccesses(); i++) {
            AsmJSHeapAccess &access = module_->heapAccess(i);
            access.updateOffset(masm_.actualOffset(access.offset()));
        }

        *module = module_.forget();
        return true;
    }
};

/*****************************************************************************/

// Encapsulates the compilation of a single function in an asm.js module. The
// function compiler handles the creation and final backend compilation of the
// MIR graph. Also see ModuleCompiler comment.
class FunctionCompiler
{
  public:
    struct Local
    {
        enum Which { Var, Arg } which;
        VarType type;
        unsigned slot;
        Value initialValue;

        Local(VarType t, unsigned slot)
          : which(Arg), type(t), slot(slot), initialValue(MagicValue(JS_GENERIC_MAGIC)) {}
        Local(VarType t, unsigned slot, const Value &init)
          : which(Var), type(t), slot(slot), initialValue(init) {}
    };
    typedef HashMap<PropertyName*, Local> LocalMap;

  private:
    typedef Vector<MBasicBlock*, 2> BlockVector;
    typedef HashMap<PropertyName*, BlockVector> LabeledBlockMap;
    typedef HashMap<ParseNode*, BlockVector> UnlabeledBlockMap;
    typedef Vector<ParseNode*, 4> NodeStack;

    ModuleCompiler &       m_;
    ModuleCompiler::Func & func_;
    LocalMap               locals_;

    MIRGenerator *         mirGen_;
    AutoFlushCache         autoFlushCache_;

    MBasicBlock *          curBlock_;
    NodeStack              loopStack_;
    NodeStack              breakableStack_;
    UnlabeledBlockMap      unlabeledBreaks_;
    UnlabeledBlockMap      unlabeledContinues_;
    LabeledBlockMap        labeledBreaks_;
    LabeledBlockMap        labeledContinues_;

  public:
    FunctionCompiler(ModuleCompiler &m, ModuleCompiler::Func &func,
                     MoveRef<LocalMap> locals, MIRGenerator *mirGen)
      : m_(m),
        func_(func),
        locals_(locals),
        mirGen_(mirGen),
        autoFlushCache_("asm.js"),
        curBlock_(NULL),
        loopStack_(m.cx()),
        breakableStack_(m.cx()),
        unlabeledBreaks_(m.cx()),
        unlabeledContinues_(m.cx()),
        labeledBreaks_(m.cx()),
        labeledContinues_(m.cx())
    {}

    bool init()
    {
        if (!unlabeledBreaks_.init() ||
            !unlabeledContinues_.init() ||
            !labeledBreaks_.init() ||
            !labeledContinues_.init())
        {
            return false;
        }

        if (!newBlock(/* pred = */ NULL, &curBlock_))
            return false;

        curBlock_->add(MAsmJSCheckOverRecursed::New(&m_.stackOverflowLabel()));

        for (ABIArgIter i(func_.argMIRTypes()); !i.done(); i++) {
            MAsmJSParameter *ins = MAsmJSParameter::New(*i, i.mirType());
            curBlock_->add(ins);
            curBlock_->initSlot(info().localSlot(i.index()), ins);
        }

        for (LocalMap::Range r = locals_.all(); !r.empty(); r.popFront()) {
            const Local &local = r.front().value;
            if (local.which == Local::Var) {
                MConstant *ins = MConstant::New(local.initialValue);
                curBlock_->add(ins);
                curBlock_->initSlot(info().localSlot(local.slot), ins);
            }
        }

        return true;
    }

    bool fail(const char *str, ParseNode *pn)
    {
        m_.fail(str, pn);
        return false;
    }

    ~FunctionCompiler()
    {
        if (!m().hasError() && !cx()->isExceptionPending()) {
            JS_ASSERT(loopStack_.empty());
            JS_ASSERT(unlabeledBreaks_.empty());
            JS_ASSERT(unlabeledContinues_.empty());
            JS_ASSERT(labeledBreaks_.empty());
            JS_ASSERT(labeledContinues_.empty());
            JS_ASSERT(curBlock_ == NULL);
        }
    }

    /*************************************************** Read-only interface */

    JSContext *            cx() const { return m_.cx(); }
    ModuleCompiler &       m() const { return m_; }
    const AsmJSModule &    module() const { return m_.module(); }
    ModuleCompiler::Func & func() const { return func_; }
    MIRGenerator &         mirGen() { return *mirGen_; }
    MIRGraph &             mirGraph() { return mirGen_->graph(); }
    CompileInfo &          info() { return mirGen_->info(); }

    const Local *lookupLocal(PropertyName *name) const
    {
        if (LocalMap::Ptr p = locals_.lookup(name))
            return &p->value;
        return NULL;
    }

    MDefinition *getLocalDef(const Local &local)
    {
        if (!curBlock_)
            return NULL;
        return curBlock_->getSlot(info().localSlot(local.slot));
    }

    const ModuleCompiler::Func *lookupFunction(PropertyName *name) const
    {
        if (locals_.has(name))
            return NULL;
        if (const ModuleCompiler::Func *func = m_.lookupFunction(name))
            return func;
        return NULL;
    }

    const ModuleCompiler::Global *lookupGlobal(PropertyName *name) const
    {
        if (locals_.has(name))
            return NULL;
        return m_.lookupGlobal(name);
    }

    /************************************************* Expression generation */

    MDefinition *constant(const Value &v)
    {
        if (!curBlock_)
            return NULL;
        JS_ASSERT(v.isNumber());
        MConstant *constant = MConstant::New(v);
        curBlock_->add(constant);
        return constant;
    }

    template <class T>
    MDefinition *unary(MDefinition *op)
    {
        if (!curBlock_)
            return NULL;
        T *ins = T::NewAsmJS(op);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *unary(MDefinition *op, MIRType type)
    {
        if (!curBlock_)
            return NULL;
        T *ins = T::NewAsmJS(op, type);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *binary(MDefinition *lhs, MDefinition *rhs)
    {
        if (!curBlock_)
            return NULL;
        T *ins = T::New(lhs, rhs);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *binary(MDefinition *lhs, MDefinition *rhs, MIRType type)
    {
        if (!curBlock_)
            return NULL;
        T *ins = T::NewAsmJS(lhs, rhs, type);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition *mul(MDefinition *lhs, MDefinition *rhs, MIRType type, MMul::Mode mode)
    {
        if (!curBlock_)
            return NULL;
        MMul *ins = MMul::New(lhs, rhs, type, mode);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *bitwise(MDefinition *lhs, MDefinition *rhs)
    {
        if (!curBlock_)
            return NULL;
        T *ins = T::NewAsmJS(lhs, rhs);
        curBlock_->add(ins);
        return ins;
    }

    template <class T>
    MDefinition *bitwise(MDefinition *op)
    {
        if (!curBlock_)
            return NULL;
        T *ins = T::NewAsmJS(op);
        curBlock_->add(ins);
        return ins;
    }

    MDefinition *compare(MDefinition *lhs, MDefinition *rhs, JSOp op, MCompare::CompareType type)
    {
        if (!curBlock_)
            return NULL;
        MCompare *ins = MCompare::NewAsmJS(lhs, rhs, op, type);
        curBlock_->add(ins);
        return ins;
    }

    void assign(const Local &local, MDefinition *def)
    {
        if (!curBlock_)
            return;
        curBlock_->setSlot(info().localSlot(local.slot), def);
    }

    MDefinition *loadHeap(ArrayBufferView::ViewType vt, MDefinition *ptr)
    {
        if (!curBlock_)
            return NULL;
        MAsmJSLoadHeap *load = MAsmJSLoadHeap::New(vt, ptr);
        curBlock_->add(load);
        return load;
    }

    void storeHeap(ArrayBufferView::ViewType vt, MDefinition *ptr, MDefinition *v)
    {
        if (!curBlock_)
            return;
        curBlock_->add(MAsmJSStoreHeap::New(vt, ptr, v));
    }

    MDefinition *loadGlobalVar(const ModuleCompiler::Global &global)
    {
        if (!curBlock_)
            return NULL;
        MIRType type = global.varType().toMIRType();
        unsigned globalDataOffset = module().globalVarIndexToGlobalDataOffset(global.varIndex());
        MAsmJSLoadGlobalVar *load = MAsmJSLoadGlobalVar::New(type, globalDataOffset);
        curBlock_->add(load);
        return load;
    }

    void storeGlobalVar(const ModuleCompiler::Global &global, MDefinition *v)
    {
        if (!curBlock_)
            return;
        unsigned globalDataOffset = module().globalVarIndexToGlobalDataOffset(global.varIndex());
        curBlock_->add(MAsmJSStoreGlobalVar::New(globalDataOffset, v));
    }

    /***************************************************************** Calls */

    // The IonMonkey backend maintains a single stack offset (from the stack
    // pointer to the base of the frame) by adding the total amount of spill
    // space required plus the maximum stack required for argument passing.
    // Since we do not use IonMonkey's MPrepareCall/MPassArg/MCall, we must
    // manually accumulate, for the entire function, the maximum required stack
    // space for argument passing. (This is passed to the CodeGenerator via
    // MIRGenerator::maxAsmJSStackArgBytes.) Naively, this would just be the
    // maximum of the stack space required for each individual call (as
    // determined by the call ABI). However, as an optimization, arguments are
    // stored to the stack immediately after evaluation (to decrease live
    // ranges and reduce spilling). This introduces the complexity that,
    // between evaluating an argument and making the call, another argument
    // evaluation could perform a call that also needs to store to the stack.
    // When this occurs childClobbers_ = true and the parent expression's
    // arguments are stored above the maximum depth clobbered by a child
    // expression.

    class Args
    {
        ABIArgGenerator abi_;
        uint32_t prevMaxStackBytes_;
        uint32_t maxChildStackBytes_;
        uint32_t spIncrement_;
        Vector<Type, 8> types_;
        MAsmJSCall::Args regArgs_;
        Vector<MAsmJSPassStackArg*> stackArgs_;
        bool childClobbers_;

        friend class FunctionCompiler;

      public:
        Args(FunctionCompiler &f)
          : prevMaxStackBytes_(0),
            maxChildStackBytes_(0),
            spIncrement_(0),
            types_(f.cx()),
            regArgs_(f.cx()),
            stackArgs_(f.cx()),
            childClobbers_(false)
        {}
        unsigned length() const {
            return types_.length();
        }
        Type type(unsigned i) const {
            return types_[i];
        }
    };

    void startCallArgs(Args *args)
    {
        if (!curBlock_)
            return;
        args->prevMaxStackBytes_ = mirGen().resetAsmJSMaxStackArgBytes();
    }

    bool passArg(MDefinition *argDef, Type type, Args *args)
    {
        if (!args->types_.append(type))
            return false;

        if (!curBlock_)
            return true;

        uint32_t childStackBytes = mirGen().resetAsmJSMaxStackArgBytes();
        args->maxChildStackBytes_ = Max(args->maxChildStackBytes_, childStackBytes);
        if (childStackBytes > 0 && !args->stackArgs_.empty())
            args->childClobbers_ = true;

        ABIArg arg = args->abi_.next(type.toMIRType());
        if (arg.kind() == ABIArg::Stack) {
            MAsmJSPassStackArg *mir = MAsmJSPassStackArg::New(arg.offsetFromArgBase(), argDef);
            curBlock_->add(mir);
            if (!args->stackArgs_.append(mir))
                return false;
        } else {
            if (!args->regArgs_.append(MAsmJSCall::Arg(arg.reg(), argDef)))
                return false;
        }
        return true;
    }

    void finishCallArgs(Args *args)
    {
        if (!curBlock_)
            return;
        uint32_t parentStackBytes = args->abi_.stackBytesConsumedSoFar();
        uint32_t newStackBytes;
        if (args->childClobbers_) {
            args->spIncrement_ = AlignBytes(args->maxChildStackBytes_, StackAlignment);
            for (unsigned i = 0; i < args->stackArgs_.length(); i++)
                args->stackArgs_[i]->incrementOffset(args->spIncrement_);
            newStackBytes = Max(args->prevMaxStackBytes_,
                                args->spIncrement_ + parentStackBytes);
        } else {
            args->spIncrement_ = 0;
            newStackBytes = Max(args->prevMaxStackBytes_,
                                Max(args->maxChildStackBytes_, parentStackBytes));
        }
        mirGen_->setAsmJSMaxStackArgBytes(newStackBytes);
    }

  private:
    bool call(MAsmJSCall::Callee callee, const Args &args, MIRType returnType, MDefinition **def)
    {
        if (!curBlock_) {
            *def = NULL;
            return true;
        }
        MAsmJSCall *ins = MAsmJSCall::New(callee, args.regArgs_, returnType, args.spIncrement_);
        if (!ins)
            return false;
        curBlock_->add(ins);
        *def = ins;
        return true;
    }

  public:
    bool internalCall(const ModuleCompiler::Func &func, const Args &args, MDefinition **def)
    {
        MIRType returnType = func.returnType().toMIRType();
        return call(MAsmJSCall::Callee(func.codeLabel()), args, returnType, def);
    }

    bool funcPtrCall(const ModuleCompiler::FuncPtrTable &funcPtrTable, MDefinition *index,
                     const Args &args, MDefinition **def)
    {
        if (!curBlock_) {
            *def = NULL;
            return true;
        }

        MConstant *mask = MConstant::New(Int32Value(funcPtrTable.mask()));
        curBlock_->add(mask);
        MBitAnd *maskedIndex = MBitAnd::NewAsmJS(index, mask);
        curBlock_->add(maskedIndex);
        unsigned globalDataOffset = module().funcPtrIndexToGlobalDataOffset(funcPtrTable.baseIndex());
        MAsmJSLoadFuncPtr *ptrFun = MAsmJSLoadFuncPtr::New(globalDataOffset, maskedIndex);
        curBlock_->add(ptrFun);

        MIRType returnType = funcPtrTable.sig().returnType().toMIRType();
        return call(MAsmJSCall::Callee(ptrFun), args, returnType, def);
    }

    bool ffiCall(unsigned exitIndex, const Args &args, MIRType returnType, MDefinition **def)
    {
        if (!curBlock_) {
            *def = NULL;
            return true;
        }

        JS_STATIC_ASSERT(offsetof(AsmJSModule::ExitDatum, exit) == 0);
        unsigned globalDataOffset = module().exitIndexToGlobalDataOffset(exitIndex);

        MAsmJSLoadFFIFunc *ptrFun = MAsmJSLoadFFIFunc::New(globalDataOffset);
        curBlock_->add(ptrFun);

        return call(MAsmJSCall::Callee(ptrFun), args, returnType, def);
    }

    bool builtinCall(void *builtin, const Args &args, MIRType returnType, MDefinition **def)
    {
        return call(MAsmJSCall::Callee(builtin), args, returnType, def);
    }

    /*********************************************** Control flow generation */

    void returnExpr(MDefinition *expr)
    {
        if (!curBlock_)
            return;
        MAsmJSReturn *ins = MAsmJSReturn::New(expr);
        curBlock_->end(ins);
        curBlock_ = NULL;
    }

    void returnVoid()
    {
        if (!curBlock_)
            return;
        MAsmJSVoidReturn *ins = MAsmJSVoidReturn::New();
        curBlock_->end(ins);
        curBlock_ = NULL;
    }

    bool branchAndStartThen(MDefinition *cond, MBasicBlock **thenBlock, MBasicBlock **elseBlock)
    {
        if (!curBlock_) {
            *thenBlock = NULL;
            *elseBlock = NULL;
            return true;
        }
        if (!newBlock(curBlock_, thenBlock) || !newBlock(curBlock_, elseBlock))
            return false;
        curBlock_->end(MTest::New(cond, *thenBlock, *elseBlock));
        curBlock_ = *thenBlock;
        return true;
    }

    void joinIf(MBasicBlock *joinBlock)
    {
        if (!joinBlock)
            return;
        if (curBlock_) {
            curBlock_->end(MGoto::New(joinBlock));
            joinBlock->addPredecessor(curBlock_);
        }
        curBlock_ = joinBlock;
        mirGraph().moveBlockToEnd(curBlock_);
    }

    MBasicBlock *switchToElse(MBasicBlock *elseBlock)
    {
        if (!elseBlock)
            return NULL;
        MBasicBlock *thenEnd = curBlock_;
        curBlock_ = elseBlock;
        mirGraph().moveBlockToEnd(curBlock_);
        return thenEnd;
    }

    bool joinIfElse(MBasicBlock *thenEnd)
    {
        if (!curBlock_ && !thenEnd)
            return true;
        MBasicBlock *pred = curBlock_ ? curBlock_ : thenEnd;
        MBasicBlock *join;
        if (!newBlock(pred, &join))
            return false;
        if (curBlock_)
            curBlock_->end(MGoto::New(join));
        if (thenEnd)
            thenEnd->end(MGoto::New(join));
        if (curBlock_ && thenEnd)
            join->addPredecessor(thenEnd);
        curBlock_ = join;
        return true;
    }

    void pushPhiInput(MDefinition *def)
    {
        if (!curBlock_)
            return;
        JS_ASSERT(curBlock_->stackDepth() == info().firstStackSlot());
        curBlock_->push(def);
    }

    MDefinition *popPhiOutput()
    {
        if (!curBlock_)
            return NULL;
        JS_ASSERT(curBlock_->stackDepth() == info().firstStackSlot() + 1);
        return curBlock_->pop();
    }

    bool startPendingLoop(ParseNode *pn, MBasicBlock **loopEntry)
    {
        if (!loopStack_.append(pn) || !breakableStack_.append(pn))
            return false;
        JS_ASSERT_IF(curBlock_, curBlock_->loopDepth() == loopStack_.length() - 1);
        if (!curBlock_) {
            *loopEntry = NULL;
            return true;
        }
        *loopEntry = MBasicBlock::NewPendingLoopHeader(mirGraph(), info(), curBlock_, NULL);
        if (!*loopEntry)
            return false;
        mirGraph().addBlock(*loopEntry);
        (*loopEntry)->setLoopDepth(loopStack_.length());
        curBlock_->end(MGoto::New(*loopEntry));
        curBlock_ = *loopEntry;
        return true;
    }

    bool branchAndStartLoopBody(MDefinition *cond, MBasicBlock **afterLoop)
    {
        if (!curBlock_) {
            *afterLoop = NULL;
            return true;
        }
        JS_ASSERT(curBlock_->loopDepth() > 0);
        MBasicBlock *body;
        if (!newBlock(curBlock_, &body))
            return false;
        if (cond->isConstant() && ToBoolean(cond->toConstant()->value())) {
            *afterLoop = NULL;
            curBlock_->end(MGoto::New(body));
        } else {
            if (!newBlockWithDepth(curBlock_, curBlock_->loopDepth() - 1, afterLoop))
                return false;
            curBlock_->end(MTest::New(cond, body, *afterLoop));
        }
        curBlock_ = body;
        return true;
    }

  private:
    ParseNode *popLoop()
    {
        ParseNode *pn = loopStack_.back();
        JS_ASSERT(!unlabeledContinues_.has(pn));
        loopStack_.popBack();
        breakableStack_.popBack();
        return pn;
    }

  public:
    bool closeLoop(MBasicBlock *loopEntry, MBasicBlock *afterLoop)
    {
        ParseNode *pn = popLoop();
        if (!loopEntry) {
            JS_ASSERT(!afterLoop);
            JS_ASSERT(!curBlock_);
            JS_ASSERT(!unlabeledBreaks_.has(pn));
            return true;
        }
        JS_ASSERT(loopEntry->loopDepth() == loopStack_.length() + 1);
        JS_ASSERT_IF(afterLoop, afterLoop->loopDepth() == loopStack_.length());
        if (curBlock_) {
            JS_ASSERT(curBlock_->loopDepth() == loopStack_.length() + 1);
            curBlock_->end(MGoto::New(loopEntry));
            loopEntry->setBackedge(curBlock_);
        }
        curBlock_ = afterLoop;
        if (curBlock_)
            mirGraph().moveBlockToEnd(curBlock_);
        return bindUnlabeledBreaks(pn);
    }

    bool branchAndCloseDoWhileLoop(MDefinition *cond, MBasicBlock *loopEntry)
    {
        ParseNode *pn = popLoop();
        if (!loopEntry) {
            JS_ASSERT(!curBlock_);
            JS_ASSERT(!unlabeledBreaks_.has(pn));
            return true;
        }
        JS_ASSERT(loopEntry->loopDepth() == loopStack_.length() + 1);
        if (curBlock_) {
            JS_ASSERT(curBlock_->loopDepth() == loopStack_.length() + 1);
            if (cond->isConstant()) {
                if (ToBoolean(cond->toConstant()->value())) {
                    curBlock_->end(MGoto::New(loopEntry));
                    loopEntry->setBackedge(curBlock_);
                    curBlock_ = NULL;
                } else {
                    MBasicBlock *afterLoop;
                    if (!newBlock(curBlock_, &afterLoop))
                        return false;
                    curBlock_->end(MGoto::New(afterLoop));
                    curBlock_ = afterLoop;
                }
            } else {
                MBasicBlock *afterLoop;
                if (!newBlock(curBlock_, &afterLoop))
                    return false;
                curBlock_->end(MTest::New(cond, loopEntry, afterLoop));
                loopEntry->setBackedge(curBlock_);
                curBlock_ = afterLoop;
            }
        }
        return bindUnlabeledBreaks(pn);
    }

    bool bindContinues(ParseNode *pn, const LabelVector *maybeLabels)
    {
        if (UnlabeledBlockMap::Ptr p = unlabeledContinues_.lookup(pn)) {
            if (!bindBreaksOrContinues(&p->value))
                return false;
            unlabeledContinues_.remove(p);
        }
        return bindLabeledBreaksOrContinues(maybeLabels, &labeledContinues_);
    }

    bool bindLabeledBreaks(const LabelVector *maybeLabels)
    {
        return bindLabeledBreaksOrContinues(maybeLabels, &labeledBreaks_);
    }

    bool addBreak(PropertyName *maybeLabel) {
        if (maybeLabel)
            return addBreakOrContinue(maybeLabel, &labeledBreaks_);
        return addBreakOrContinue(breakableStack_.back(), &unlabeledBreaks_);
    }

    bool addContinue(PropertyName *maybeLabel) {
        if (maybeLabel)
            return addBreakOrContinue(maybeLabel, &labeledContinues_);
        return addBreakOrContinue(loopStack_.back(), &unlabeledContinues_);
    }

    bool startSwitch(ParseNode *pn, MDefinition *expr, int32_t low, int32_t high,
                     MBasicBlock **switchBlock)
    {
        if (!breakableStack_.append(pn))
            return false;
        if (!curBlock_) {
            *switchBlock = NULL;
            return true;
        }
        curBlock_->end(MTableSwitch::New(expr, low, high));
        *switchBlock = curBlock_;
        curBlock_ = NULL;
        return true;
    }

    bool startSwitchCase(MBasicBlock *switchBlock, MBasicBlock **next)
    {
        if (!switchBlock) {
            *next = NULL;
            return true;
        }
        if (!newBlock(switchBlock, next))
            return false;
        if (curBlock_) {
            curBlock_->end(MGoto::New(*next));
            (*next)->addPredecessor(curBlock_);
        }
        curBlock_ = *next;
        return true;
    }

    bool startSwitchDefault(MBasicBlock *switchBlock, CaseVector *cases, MBasicBlock **defaultBlock)
    {
        if (!startSwitchCase(switchBlock, defaultBlock))
            return false;
        if (!*defaultBlock)
            return true;
        for (unsigned i = 0; i < cases->length(); i++) {
            if (!(*cases)[i]) {
                MBasicBlock *bb;
                if (!newBlock(switchBlock, &bb))
                    return false;
                bb->end(MGoto::New(*defaultBlock));
                (*defaultBlock)->addPredecessor(bb);
                (*cases)[i] = bb;
            }
        }
        mirGraph().moveBlockToEnd(*defaultBlock);
        return true;
    }

    bool joinSwitch(MBasicBlock *switchBlock, const CaseVector &cases, MBasicBlock *defaultBlock)
    {
        ParseNode *pn = breakableStack_.popCopy();
        if (!switchBlock)
            return true;
        MTableSwitch *mir = switchBlock->lastIns()->toTableSwitch();
        mir->addDefault(defaultBlock);
        for (unsigned i = 0; i < cases.length(); i++)
            mir->addCase(cases[i]);
        if (curBlock_) {
            MBasicBlock *next;
            if (!newBlock(curBlock_, &next))
                return false;
            curBlock_->end(MGoto::New(next));
            curBlock_ = next;
        }
        return bindUnlabeledBreaks(pn);
    }

    /*************************************************************************/
  private:
    bool newBlockWithDepth(MBasicBlock *pred, unsigned loopDepth, MBasicBlock **block)
    {
        *block = MBasicBlock::New(mirGraph(), info(), pred, /* pc = */ NULL, MBasicBlock::NORMAL);
        if (!*block)
            return false;
        mirGraph().addBlock(*block);
        (*block)->setLoopDepth(loopDepth);
        return true;
    }

    bool newBlock(MBasicBlock *pred, MBasicBlock **block)
    {
        return newBlockWithDepth(pred, loopStack_.length(), block);
    }

    bool bindBreaksOrContinues(BlockVector *preds)
    {
        for (unsigned i = 0; i < preds->length(); i++) {
            MBasicBlock *pred = (*preds)[i];
            if (curBlock_ && curBlock_->begin() == curBlock_->end()) {
                pred->end(MGoto::New(curBlock_));
                curBlock_->addPredecessor(pred);
            } else {
                MBasicBlock *next;
                if (!newBlock(pred, &next))
                    return false;
                pred->end(MGoto::New(next));
                if (curBlock_) {
                    curBlock_->end(MGoto::New(next));
                    next->addPredecessor(curBlock_);
                }
                curBlock_ = next;
            }
            JS_ASSERT(curBlock_->begin() == curBlock_->end());
        }
        preds->clear();
        return true;
    }

    bool bindLabeledBreaksOrContinues(const LabelVector *maybeLabels, LabeledBlockMap *map)
    {
        if (!maybeLabels)
            return true;
        const LabelVector &labels = *maybeLabels;
        for (unsigned i = 0; i < labels.length(); i++) {
            if (LabeledBlockMap::Ptr p = map->lookup(labels[i])) {
                if (!bindBreaksOrContinues(&p->value))
                    return false;
                map->remove(p);
            }
        }
        return true;
    }

    template <class Key, class Map>
    bool addBreakOrContinue(Key key, Map *map)
    {
        if (!curBlock_)
            return true;
        typename Map::AddPtr p = map->lookupForAdd(key);
        if (!p) {
            BlockVector empty(m().cx());
            if (!map->add(p, key, Move(empty)))
                return false;
        }
        if (!p->value.append(curBlock_))
            return false;
        curBlock_ = NULL;
        return true;
    }

    bool bindUnlabeledBreaks(ParseNode *pn)
    {
        if (UnlabeledBlockMap::Ptr p = unlabeledBreaks_.lookup(pn)) {
            if (!bindBreaksOrContinues(&p->value))
                return false;
            unlabeledBreaks_.remove(p);
        }
        return true;
    }
};

/*****************************************************************************/
// An AsmJSModule contains the persistent results of asm.js module compilation,
// viz., the jit code and dynamic link information.
//
// An AsmJSModule object is created at the end of module compilation and
// subsequently owned by an AsmJSModuleClass JSObject.

static void AsmJSModuleObject_finalize(FreeOp *fop, RawObject obj);
static void AsmJSModuleObject_trace(JSTracer *trc, JSRawObject obj);

static const unsigned ASM_CODE_RESERVED_SLOT = 0;
static const unsigned ASM_CODE_NUM_RESERVED_SLOTS = 1;

static Class AsmJSModuleClass = {
    "AsmJSModuleObject",
    JSCLASS_IS_ANONYMOUS | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(ASM_CODE_NUM_RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    NULL,                    /* convert     */
    AsmJSModuleObject_finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* hasInstance */
    NULL,                    /* construct   */
    AsmJSModuleObject_trace
};

AsmJSModule &
js::AsmJSModuleObjectToModule(JSObject *obj)
{
    JS_ASSERT(obj->getClass() == &AsmJSModuleClass);
    return *(AsmJSModule *)obj->getReservedSlot(ASM_CODE_RESERVED_SLOT).toPrivate();
}

static const unsigned ASM_MODULE_FUNCTION_MODULE_OBJECT_SLOT = 0;

JSObject &
js::AsmJSModuleObject(JSFunction *moduleFun)
{
    return moduleFun->getExtendedSlot(ASM_MODULE_FUNCTION_MODULE_OBJECT_SLOT).toObject();
}

void
js::SetAsmJSModuleObject(JSFunction *moduleFun, JSObject *moduleObj)
{
    moduleFun->setExtendedSlot(ASM_MODULE_FUNCTION_MODULE_OBJECT_SLOT, OBJECT_TO_JSVAL(moduleObj));
}

static void
AsmJSModuleObject_finalize(FreeOp *fop, RawObject obj)
{
    fop->delete_(&AsmJSModuleObjectToModule(obj));
}

static void
AsmJSModuleObject_trace(JSTracer *trc, JSRawObject obj)
{
    AsmJSModuleObjectToModule(obj).trace(trc);
}

static JSObject *
NewAsmJSModuleObject(JSContext *cx, ScopedJSDeletePtr<AsmJSModule> *module)
{
    JSObject *obj = NewObjectWithGivenProto(cx, &AsmJSModuleClass, NULL, NULL);
    if (!obj)
        return NULL;

    obj->setReservedSlot(ASM_CODE_RESERVED_SLOT, PrivateValue(module->forget()));
    return obj;
}

/*****************************************************************************/
// asm.js type-checking and code-generation algorithm

static bool
CheckIdentifier(ModuleCompiler &m, PropertyName *name, ParseNode *nameNode)
{
    if (name == m.cx()->names().arguments || name == m.cx()->names().eval)
        return m.fail("disallowed asm.js parameter name", nameNode);
    return true;
}

static bool
CheckModuleLevelName(ModuleCompiler &m, PropertyName *name, ParseNode *nameNode)
{
    if (!CheckIdentifier(m, name, nameNode))
        return false;

    if (name == m.moduleFunctionName() ||
        name == m.module().globalArgumentName() ||
        name == m.module().importArgumentName() ||
        name == m.module().bufferArgumentName() ||
        m.lookupGlobal(name))
    {
        return m.fail("Duplicate names not allowed", nameNode);
    }

    return true;
}

static bool
CheckFunctionHead(ModuleCompiler &m, ParseNode *fn, ParseNode **stmtIter)
{
    if (FunctionObject(fn)->hasRest())
        return m.fail("rest args not allowed in asm.js", fn);
    if (!FunctionHasStatementList(fn))
        return m.fail("expression closures not allowed in asm.js", fn);

    *stmtIter = ListHead(FunctionStatementList(fn));
    return true;
}

static bool
CheckArgument(ModuleCompiler &m, ParseNode *arg, PropertyName **name)
{
    if (!IsDefinition(arg))
        return m.fail("overlapping argument names not allowed", arg);

    if (MaybeDefinitionInitializer(arg))
        return m.fail("default arguments not allowed", arg);

    if (!CheckIdentifier(m, arg->name(), arg))
        return false;

    *name = arg->name();
    return true;
}

static bool
CheckModuleArgument(ModuleCompiler &m, ParseNode *arg, PropertyName **name)
{
    if (!CheckArgument(m, arg, name))
        return false;

    if (!CheckModuleLevelName(m, *name, arg))
        return false;

    return true;
}

static bool
CheckModuleArguments(ModuleCompiler &m, ParseNode *fn)
{
    unsigned numFormals;
    ParseNode *arg1 = FunctionArgsList(fn, &numFormals);
    ParseNode *arg2 = arg1 ? NextNode(arg1) : NULL;
    ParseNode *arg3 = arg2 ? NextNode(arg2) : NULL;

    if (numFormals > 3)
        return m.fail("asm.js modules takes at most 3 argument.", fn);

    PropertyName *arg1Name = NULL;
    if (numFormals >= 1 && !CheckModuleArgument(m, arg1, &arg1Name))
        return false;
    m.initGlobalArgumentName(arg1Name);

    PropertyName *arg2Name = NULL;
    if (numFormals >= 2 && !CheckModuleArgument(m, arg2, &arg2Name))
        return false;
    m.initImportArgumentName(arg2Name);

    PropertyName *arg3Name = NULL;
    if (numFormals >= 3 && !CheckModuleArgument(m, arg3, &arg3Name))
        return false;
    m.initBufferArgumentName(arg3Name);

    return true;
}

static bool
SkipUseAsmDirective(ModuleCompiler &m, ParseNode **stmtIter)
{
    ParseNode *firstStatement = *stmtIter;

    if (!IsExpressionStatement(firstStatement))
        return m.fail("No funny stuff before the 'use asm' directive", firstStatement);

    ParseNode *expr = ExpressionStatementExpr(firstStatement);
    if (!expr || !expr->isKind(PNK_STRING))
        return m.fail("No funny stuff before the 'use asm' directive", firstStatement);

    if (StringAtom(expr) != m.cx()->names().useAsm)
        return m.fail("asm.js precludes other directives", firstStatement);

    *stmtIter = NextNode(firstStatement);
    return true;
}

static bool
CheckGlobalVariableInitConstant(ModuleCompiler &m, PropertyName *varName, ParseNode *initNode)
{
    NumLit literal = ExtractNumericLiteral(initNode);
    VarType type;
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
      case NumLit::BigUnsigned:
        type = VarType::Int;
        break;
      case NumLit::Double:
        type = VarType::Double;
        break;
      case NumLit::OutOfRangeInt:
        return m.fail("Global initializer is out of representable integer range", initNode);
    }
    return m.addGlobalVarInitConstant(varName, type, literal.value());
}

static bool
CheckTypeAnnotation(ModuleCompiler &m, ParseNode *coercionNode, AsmJSCoercion *coercion,
                    ParseNode **coercedExpr = NULL)
{
    switch (coercionNode->getKind()) {
      case PNK_BITOR: {
        ParseNode *rhs = BinaryRight(coercionNode);

        if (!IsNumericLiteral(rhs))
            return m.fail("Must use |0 for argument/return coercion.", rhs);

        NumLit rhsLiteral = ExtractNumericLiteral(rhs);
        if (rhsLiteral.which() != NumLit::Fixnum || rhsLiteral.toInt32() != 0)
            return m.fail("Must use |0 for argument/return coercion.", rhs);

        *coercion = AsmJS_ToInt32;
        if (coercedExpr)
            *coercedExpr = BinaryLeft(coercionNode);
        return true;
      }
      case PNK_POS: {
        *coercion = AsmJS_ToNumber;
        if (coercedExpr)
            *coercedExpr = UnaryKid(coercionNode);
        return true;
      }
      default:;
    }

    return m.fail("in coercion expression, the expression must be of the form +x or x|0", coercionNode);
}

static bool
CheckGlobalVariableInitImport(ModuleCompiler &m, PropertyName *varName, ParseNode *initNode)
{
    AsmJSCoercion coercion;
    ParseNode *coercedExpr;
    if (!CheckTypeAnnotation(m, initNode, &coercion, &coercedExpr))
        return false;

    if (!coercedExpr->isKind(PNK_DOT))
        return m.fail("Bad global variable import expression", coercedExpr);

    ParseNode *base = DotBase(coercedExpr);
    PropertyName *field = DotMember(coercedExpr);

    if (!IsUseOfName(base, m.module().importArgumentName()))
        return m.fail("Expecting c.y where c is the import parameter", coercedExpr);

    return m.addGlobalVarImport(varName, field, coercion);
}

static bool
CheckNewArrayView(ModuleCompiler &m, PropertyName *varName, ParseNode *newExpr, bool first)
{
    ParseNode *ctorExpr = ListHead(newExpr);
    if (!ctorExpr->isKind(PNK_DOT))
        return m.fail("Only valid 'new' import is 'new global.XYZArray(buf)'", ctorExpr);

    ParseNode *base = DotBase(ctorExpr);
    PropertyName *field = DotMember(ctorExpr);

    if (!IsUseOfName(base, m.module().globalArgumentName()))
        return m.fail("Expecting global.y", base);

    ParseNode *bufArg = NextNode(ctorExpr);
    if (!bufArg)
        return m.fail("Constructor needs an argument", ctorExpr);

    if (NextNode(bufArg) != NULL)
        return m.fail("Only one argument may be passed to a typed array constructor", bufArg);

    if (!IsUseOfName(bufArg, m.module().bufferArgumentName()))
        return m.fail("Argument to typed array constructor must be ArrayBuffer name", bufArg);

    JSAtomState &names = m.cx()->names();
    ArrayBufferView::ViewType type;
    if (field == names.Int8Array)
        type = ArrayBufferView::TYPE_INT8;
    else if (field == names.Uint8Array)
        type = ArrayBufferView::TYPE_UINT8;
    else if (field == names.Int16Array)
        type = ArrayBufferView::TYPE_INT16;
    else if (field == names.Uint16Array)
        type = ArrayBufferView::TYPE_UINT16;
    else if (field == names.Int32Array)
        type = ArrayBufferView::TYPE_INT32;
    else if (field == names.Uint32Array)
        type = ArrayBufferView::TYPE_UINT32;
    else if (field == names.Float32Array)
        type = ArrayBufferView::TYPE_FLOAT32;
    else if (field == names.Float64Array)
        type = ArrayBufferView::TYPE_FLOAT64;
    else
        return m.fail("could not match typed array name", ctorExpr);

    return m.addArrayView(varName, type, field);
}

static bool
CheckGlobalDotImport(ModuleCompiler &m, PropertyName *varName, ParseNode *initNode)
{
    ParseNode *base = DotBase(initNode);
    PropertyName *field = DotMember(initNode);

    if (base->isKind(PNK_DOT)) {
        ParseNode *global = DotBase(base);
        PropertyName *math = DotMember(base);
        if (!IsUseOfName(global, m.module().globalArgumentName()) || math != m.cx()->names().Math)
            return m.fail("Expecting global.Math", base);

        AsmJSMathBuiltin mathBuiltin;
        if (!m.lookupStandardLibraryMathName(field, &mathBuiltin))
            return m.fail("Does not match a standard Math builtin", initNode);

        return m.addMathBuiltin(varName, mathBuiltin, field);
    }

    if (IsUseOfName(base, m.module().globalArgumentName())) {
        if (field == m.cx()->names().NaN)
            return m.addGlobalConstant(varName, js_NaN, field);
        if (field == m.cx()->names().Infinity)
            return m.addGlobalConstant(varName, js_PositiveInfinity, field);
        return m.fail("Does not match a standard global constant", initNode);
    }

    if (IsUseOfName(base, m.module().importArgumentName()))
        return m.addFFI(varName, field);

    return m.fail("Expecting c.y where c is either the global or import parameter", initNode);
}

static bool
CheckModuleGlobal(ModuleCompiler &m, ParseNode *var, bool first)
{
    if (!IsDefinition(var))
        return m.fail("Import variable names must be unique", var);

    if (!CheckModuleLevelName(m, var->name(), var))
        return false;

    ParseNode *initNode = MaybeDefinitionInitializer(var);
    if (!initNode)
        return m.fail("Module import needs initializer", var);

    if (IsNumericLiteral(initNode))
        return CheckGlobalVariableInitConstant(m, var->name(), initNode);

    if (initNode->isKind(PNK_BITOR) || initNode->isKind(PNK_POS))
        return CheckGlobalVariableInitImport(m, var->name(), initNode);

    if (initNode->isKind(PNK_NEW))
        return CheckNewArrayView(m, var->name(), initNode, first);

    if (initNode->isKind(PNK_DOT))
        return CheckGlobalDotImport(m, var->name(), initNode);

    return m.fail("Unsupported import expression", initNode);

}

static bool
CheckModuleGlobals(ModuleCompiler &m, ParseNode **stmtIter)
{
    ParseNode *stmt = SkipEmptyStatements(*stmtIter);

    bool first = true;

    for (; stmt && stmt->isKind(PNK_VAR); stmt = NextNonEmptyStatement(stmt)) {
        for (ParseNode *var = VarListHead(stmt); var; var = NextNode(var)) {
            if (!CheckModuleGlobal(m, var, first))
                return false;
            first = false;
        }
    }

    *stmtIter = stmt;
    return true;
}

static bool
CheckArgumentType(ModuleCompiler &m, ParseNode *fn, PropertyName *argName, ParseNode *stmt,
                  VarType *type)
{
    if (!stmt)
        return m.fail("Missing parameter type declaration statements", fn);

    if (!IsExpressionStatement(stmt))
        return m.fail("Expecting expression statement type of the form 'arg = coercion;'", stmt);

    ParseNode *initNode = ExpressionStatementExpr(stmt);
    if (!initNode || !initNode->isKind(PNK_ASSIGN))
        return m.fail("Expecting expression statement type of the form 'arg = coercion;'", stmt);

    ParseNode *argNode = BinaryLeft(initNode);
    ParseNode *coercionNode = BinaryRight(initNode);

    if (!IsUseOfName(argNode, argName))
        return m.fail("left-hand side of 'arg = expr;' must be the name of an argument.", argNode);

    ParseNode *coercedExpr;
    AsmJSCoercion coercion;
    if (!CheckTypeAnnotation(m, coercionNode, &coercion, &coercedExpr))
        return false;

    if (!IsUseOfName(coercedExpr, argName))
        return m.fail("For argument type declaration, need 'x = coercion(x)'", coercedExpr);

    *type = VarType(coercion);
    return true;
}

static bool
CheckArguments(ModuleCompiler &m, ParseNode *fn, MIRTypeVector *argTypes, ParseNode **stmtIter)
{
    ParseNode *stmt = *stmtIter;

    unsigned numFormals;
    ParseNode *argpn = FunctionArgsList(fn, &numFormals);

    HashSet<PropertyName*> dupSet(m.cx());
    if (!dupSet.init())
        return false;

    for (unsigned i = 0; i < numFormals; i++, argpn = NextNode(argpn), stmt = NextNode(stmt)) {
        PropertyName *argName;
        if (!CheckArgument(m, argpn, &argName))
            return false;

        if (dupSet.has(argName))
            return m.fail("asm.js arguments must have distinct names", argpn);
        if (!dupSet.putNew(argName))
            return false;

        VarType argType;
        if (!CheckArgumentType(m, fn, argName, stmt, &argType))
            return false;

        if (!argTypes->append(argType.toMIRType()))
            return false;
    }

    *stmtIter = stmt;
    return true;
}

static bool
CheckReturnType(ModuleCompiler &m, ParseNode *fn, RetType *returnType)
{
    ParseNode *stmt = FunctionLastStatementOrNull(fn);
    if (!stmt || !stmt->isKind(PNK_RETURN) || !UnaryKid(stmt)) {
        *returnType = RetType::Void;
        return true;
    }

    ParseNode *coercionNode = UnaryKid(stmt);

    if (IsNumericLiteral(coercionNode)) {
        switch (ExtractNumericLiteral(coercionNode).which()) {
          case NumLit::BigUnsigned:
          case NumLit::OutOfRangeInt:
            return m.fail("Returned literal is out of integer range", coercionNode);
          case NumLit::Fixnum:
          case NumLit::NegativeInt:
            *returnType = RetType::Signed;
            break;
          case NumLit::Double:
            *returnType = RetType::Double;
            break;
        }
    } else {
        AsmJSCoercion coercion;
        if (!CheckTypeAnnotation(m, coercionNode, &coercion))
            return false;
        *returnType = RetType(coercion);
    }

    JS_ASSERT(returnType->toType().isExtern());
    return true;
}

static bool
CheckFunctionSignature(ModuleCompiler &m, ParseNode *fn)
{
    PropertyName *name = FunctionName(fn);
    if (!CheckModuleLevelName(m, name, fn))
        return false;

    ParseNode *stmtIter = NULL;

    if (!CheckFunctionHead(m, fn, &stmtIter))
        return false;

    MIRTypeVector argTypes(m.cx());
    if (!CheckArguments(m, fn, &argTypes, &stmtIter))
        return false;

    RetType returnType;
    if (!CheckReturnType(m, fn, &returnType))
        return false;

    ModuleCompiler::Func func(fn, stmtIter, Move(argTypes), returnType);
    if (!m.addFunction(Move(func)))
        return false;

    return true;
}

static bool
CheckFunctionSignatures(ModuleCompiler &m, ParseNode **stmtIter)
{
    ParseNode *fn = SkipEmptyStatements(*stmtIter);

    for (; fn && fn->isKind(PNK_FUNCTION); fn = NextNonEmptyStatement(fn)) {
        if (!CheckFunctionSignature(m, fn))
            return false;
    }

    if (fn && fn->isKind(PNK_NOP))
        return m.fail("duplicate function names are not allowed", fn);

    *stmtIter = fn;
    return true;
}

static bool
SameSignature(const ModuleCompiler::Func &a, const ModuleCompiler::Func &b)
{
    if (a.numArgs() != b.numArgs() || a.returnType() != b.returnType())
        return false;
    for (unsigned i = 0; i < a.numArgs(); i++) {
        if (a.argType(i) != b.argType(i))
            return false;
    }
    return true;
}

static bool
CheckFuncPtrTable(ModuleCompiler &m, ParseNode *var)
{
    if (!IsDefinition(var))
        return m.fail("Function-pointer table name must be unique", var);

    PropertyName *name = var->name();

    if (!CheckModuleLevelName(m, name, var))
        return false;

    ParseNode *arrayLiteral = MaybeDefinitionInitializer(var);
    if (!arrayLiteral || !arrayLiteral->isKind(PNK_ARRAY))
        return m.fail("Function-pointer table's initializer must be an array literal", var);

    unsigned length = ListLength(arrayLiteral);

    if (!IsPowerOfTwo(length))
        return m.fail("Function-pointer table's length must be a power of 2", arrayLiteral);

    ModuleCompiler::FuncPtrVector funcPtrs(m.cx());
    const ModuleCompiler::Func *firstFunction = NULL;

    for (ParseNode *elem = ListHead(arrayLiteral); elem; elem = NextNode(elem)) {
        if (!elem->isKind(PNK_NAME))
            return m.fail("Function-pointer table's elements must be names of functions", elem);

        PropertyName *funcName = elem->name();
        const ModuleCompiler::Func *func = m.lookupFunction(funcName);
        if (!func)
            return m.fail("Function-pointer table's elements must be names of functions", elem);

        if (firstFunction) {
            if (!SameSignature(*firstFunction, *func))
                return m.fail("All functions in table must have same signature", elem);
        } else {
            firstFunction = func;
        }

        if (!funcPtrs.append(func))
            return false;
    }

    return m.addFuncPtrTable(name, Move(funcPtrs));
}

static bool
CheckFuncPtrTables(ModuleCompiler &m, ParseNode **stmtIter)
{
    ParseNode *stmt = SkipEmptyStatements(*stmtIter);

    for (; stmt && stmt->isKind(PNK_VAR); stmt = NextNonEmptyStatement(stmt)) {
        for (ParseNode *var = VarListHead(stmt); var; var = NextNode(var)) {
            if (!CheckFuncPtrTable(m, var))
                return false;
        }
    }

    *stmtIter = stmt;
    return true;
}

static bool
CheckModuleExportFunction(ModuleCompiler &m, ParseNode *returnExpr)
{
    if (!returnExpr->isKind(PNK_NAME))
        return m.fail("an asm.js export statement must be of the form 'return name'", returnExpr);

    PropertyName *funcName = returnExpr->name();

    const ModuleCompiler::Func *func = m.lookupFunction(funcName);
    if (!func)
        return m.fail("exported function name not found", returnExpr);

    return m.addExportedFunction(func, /* maybeFieldName = */ NULL);
}

static bool
CheckModuleExportObject(ModuleCompiler &m, ParseNode *object)
{
    JS_ASSERT(object->isKind(PNK_OBJECT));

    for (ParseNode *pn = ListHead(object); pn; pn = NextNode(pn)) {
        if (!IsNormalObjectField(m.cx(), pn))
            return m.fail("Only normal object properties may be used in the export object literal", pn);

        PropertyName *fieldName = ObjectNormalFieldName(m.cx(), pn);

        ParseNode *initNode = ObjectFieldInitializer(pn);
        if (!initNode->isKind(PNK_NAME))
            return m.fail("Initializer of exported object literal must be name of function", initNode);

        PropertyName *funcName = initNode->name();

        const ModuleCompiler::Func *func = m.lookupFunction(funcName);
        if (!func)
            return m.fail("exported function name not found", initNode);

        if (!m.addExportedFunction(func, fieldName))
            return false;
    }

    return true;
}

static bool
CheckModuleExports(ModuleCompiler &m, ParseNode *fn, ParseNode **stmtIter)
{
    ParseNode *returnNode = SkipEmptyStatements(*stmtIter);

    if (!returnNode || !returnNode->isKind(PNK_RETURN))
        return m.fail("asm.js must end with a return export statement", fn);

    ParseNode *returnExpr = UnaryKid(returnNode);

    if (!returnExpr)
        return m.fail("an asm.js export statement must return something", returnNode);

    if (returnExpr->isKind(PNK_OBJECT)) {
        if (!CheckModuleExportObject(m, returnExpr))
            return false;
    } else {
        if (!CheckModuleExportFunction(m, returnExpr))
            return false;
    }

    *stmtIter = NextNonEmptyStatement(returnNode);
    return true;
}

static bool
CheckExpr(FunctionCompiler &f, ParseNode *expr, Use use, MDefinition **def, Type *type);

static bool
CheckNumericLiteral(FunctionCompiler &f, ParseNode *num, MDefinition **def, Type *type)
{
    JS_ASSERT(IsNumericLiteral(num));
    NumLit literal = ExtractNumericLiteral(num);

    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
      case NumLit::BigUnsigned:
      case NumLit::Double:
        break;
      case NumLit::OutOfRangeInt:
        return f.fail("Numeric literal out of representable integer range", num);
    }

    *type = literal.type();
    *def = f.constant(literal.value());
    return true;
}

static bool
CheckVarRef(FunctionCompiler &f, ParseNode *varRef, MDefinition **def, Type *type)
{
    PropertyName *name = varRef->name();

    if (const FunctionCompiler::Local *local = f.lookupLocal(name)) {
        *def = f.getLocalDef(*local);
        *type = local->type.toType();
        return true;
    }

    if (const ModuleCompiler::Global *global = f.lookupGlobal(name)) {
        switch (global->which()) {
          case ModuleCompiler::Global::Constant:
            *def = f.constant(DoubleValue(global->constant()));
            *type = Type::Double;
            break;
          case ModuleCompiler::Global::Variable:
            *def = f.loadGlobalVar(*global);
            *type = global->varType().toType();
            break;
          case ModuleCompiler::Global::Function:
          case ModuleCompiler::Global::FFI:
          case ModuleCompiler::Global::MathBuiltin:
          case ModuleCompiler::Global::FuncPtrTable:
          case ModuleCompiler::Global::ArrayView:
            return f.fail("Global may not be accessed by ordinary expressions", varRef);
        }
        return true;
    }

    return f.fail("Name not found in scope", varRef);
}

static bool
CheckArrayAccess(FunctionCompiler &f, ParseNode *elem, ArrayBufferView::ViewType *viewType,
                 MDefinition **def)
{
    ParseNode *viewName = ElemBase(elem);
    ParseNode *indexExpr = ElemIndex(elem);

    if (!viewName->isKind(PNK_NAME))
        return f.fail("Left-hand side of x[y] must be a name", viewName);

    const ModuleCompiler::Global *global = f.lookupGlobal(viewName->name());
    if (!global || global->which() != ModuleCompiler::Global::ArrayView)
        return f.fail("Left-hand side of x[y] must be typed array view name", viewName);

    *viewType = global->viewType();

    uint32_t pointer;
    if (IsLiteralUint32(indexExpr, &pointer)) {
        pointer <<= TypedArrayShift(*viewType);
        *def = f.constant(Int32Value(pointer));
        return true;
    }

    MDefinition *pointerDef;
    if (indexExpr->isKind(PNK_RSH)) {
        ParseNode *shiftNode = BinaryRight(indexExpr);
        ParseNode *pointerNode = BinaryLeft(indexExpr);

        uint32_t shift;
        if (!IsLiteralUint32(shiftNode, &shift) || shift != TypedArrayShift(*viewType))
            return f.fail("The shift amount must be a constant matching the array "
                          "element size", shiftNode);

        Type pointerType;
        if (!CheckExpr(f, pointerNode, Use::ToInt32, &pointerDef, &pointerType))
            return false;

        if (!pointerType.isIntish())
            return f.fail("Pointer input must be intish", pointerNode);
    } else {
        if (TypedArrayShift(*viewType) != 0)
            return f.fail("The shift amount is 0 so this must be a Int8/Uint8 array", indexExpr);

        Type pointerType;
        if (!CheckExpr(f, indexExpr, Use::ToInt32, &pointerDef, &pointerType))
            return false;

        if (!pointerType.isInt())
            return f.fail("Pointer input must be int", indexExpr);
    }

    // Mask off the low bits to account for clearing effect of a right shift
    // followed by the left shift implicit in the array access. E.g., H32[i>>2]
    // loses the low two bits.
    int32_t mask = ~((uint32_t(1) << TypedArrayShift(*viewType)) - 1);
    *def = f.bitwise<MBitAnd>(pointerDef, f.constant(Int32Value(mask)));
    return true;
}

static bool
CheckArrayLoad(FunctionCompiler &f, ParseNode *elem, MDefinition **def, Type *type)
{
    ArrayBufferView::ViewType viewType;
    MDefinition *pointerDef;
    if (!CheckArrayAccess(f, elem, &viewType, &pointerDef))
        return false;

    *def = f.loadHeap(viewType, pointerDef);
    *type = TypedArrayLoadType(viewType);
    return true;
}

static bool
CheckStoreArray(FunctionCompiler &f, ParseNode *lhs, ParseNode *rhs, MDefinition **def, Type *type)
{
    ArrayBufferView::ViewType viewType;
    MDefinition *pointerDef;
    if (!CheckArrayAccess(f, lhs, &viewType, &pointerDef))
        return false;

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, Use::NoCoercion, &rhsDef, &rhsType))
        return false;

    switch (TypedArrayStoreType(viewType)) {
      case ArrayStore_Intish:
        if (!rhsType.isIntish())
            return f.fail("Right-hand side of store must be intish", lhs);
        break;
      case ArrayStore_Double:
        if (rhsType != Type::Double)
            return f.fail("Right-hand side of store must be double", lhs);
        break;
    }

    f.storeHeap(viewType, pointerDef, rhsDef);

    *def = rhsDef;
    *type = rhsType;
    return true;
}

static bool
CheckAssignName(FunctionCompiler &f, ParseNode *lhs, ParseNode *rhs, MDefinition **def, Type *type)
{
    PropertyName *name = lhs->name();

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, Use::NoCoercion, &rhsDef, &rhsType))
        return false;

    if (const FunctionCompiler::Local *lhsVar = f.lookupLocal(name)) {
        if (!(rhsType <= lhsVar->type))
            return f.fail("Right-hand side of assignment must be subtype of left-hand side", lhs);
        f.assign(*lhsVar, rhsDef);
    } else if (const ModuleCompiler::Global *global = f.lookupGlobal(name)) {
        if (global->which() != ModuleCompiler::Global::Variable)
            return f.fail("Only global variables are mutable, not FFI functions etc", lhs);
        if (!(rhsType <= global->varType()))
            return f.fail("Right-hand side of assignment must be subtype of left-hand side", lhs);
        f.storeGlobalVar(*global, rhsDef);
    } else {
        return f.fail("Variable name in left-hand side of assignment not found", lhs);
    }

    *def = rhsDef;
    *type = rhsType;
    return true;
}

static bool
CheckAssign(FunctionCompiler &f, ParseNode *assign, MDefinition **def, Type *type)
{
    JS_ASSERT(assign->isKind(PNK_ASSIGN));
    ParseNode *lhs = BinaryLeft(assign);
    ParseNode *rhs = BinaryRight(assign);

    if (lhs->getKind() == PNK_ELEM)
        return CheckStoreArray(f, lhs, rhs, def, type);

    if (lhs->getKind() == PNK_NAME)
        return CheckAssignName(f, lhs, rhs, def, type);

    return f.fail("Left-hand side of assignment must be a variable or heap", assign);
}

static bool
CheckMathIMul(FunctionCompiler &f, ParseNode *call, MDefinition **def, Type *type)
{
    if (CallArgListLength(call) != 2)
        return f.fail("Math.imul must be passed 2 arguments", call);

    ParseNode *lhs = CallArgList(call);
    ParseNode *rhs = NextNode(lhs);

    MDefinition *lhsDef;
    Type lhsType;
    if (!CheckExpr(f, lhs, Use::ToInt32, &lhsDef, &lhsType))
        return false;

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, Use::ToInt32, &rhsDef, &rhsType))
        return false;

    if (!lhsType.isIntish() || !rhsType.isIntish())
        return f.fail("Math.imul calls must be passed 2 intish arguments", call);

    *def = f.mul(lhsDef, rhsDef, MIRType_Int32, MMul::Integer);
    *type = Type::Signed;
    return true;
}

static bool
CheckMathAbs(FunctionCompiler &f, ParseNode *call, MDefinition **def, Type *type)
{
    if (CallArgListLength(call) != 1)
        return f.fail("Math.abs must be passed 1 argument", call);

    ParseNode *arg = CallArgList(call);

    MDefinition *argDef;
    Type argType;
    if (!CheckExpr(f, arg, Use::ToNumber, &argDef, &argType))
        return false;

    if (argType.isSigned()) {
        *def = f.unary<MAbs>(argDef, MIRType_Int32);
        *type = Type::Unsigned;
        return true;
    }

    if (argType.isDoublish()) {
        *def = f.unary<MAbs>(argDef, MIRType_Double);
        *type = Type::Double;
        return true;
    }

    return f.fail("Math.abs must be passed either a signed or doublish argument", call);
}

static bool
CheckCallArgs(FunctionCompiler &f, ParseNode *callNode, Use use, FunctionCompiler::Args *args)
{
    f.startCallArgs(args);

    ParseNode *argNode = CallArgList(callNode);
    for (unsigned i = 0; i < CallArgListLength(callNode); i++, argNode = NextNode(argNode)) {
        MDefinition *argDef;
        Type argType;
        if (!CheckExpr(f, argNode, use, &argDef, &argType))
            return false;

        if (argType.isVoid())
            return f.fail("Void cannot be passed as an argument", argNode);

        if (!f.passArg(argDef, argType, args))
            return false;
    }

    f.finishCallArgs(args);
    return true;
}

static bool
CheckInternalCall(FunctionCompiler &f, ParseNode *callNode, const ModuleCompiler::Func &callee,
                  MDefinition **def, Type *type)
{
    FunctionCompiler::Args args(f);

    if (!CheckCallArgs(f, callNode, Use::NoCoercion, &args))
        return false;

    if (args.length() != callee.numArgs())
        return f.fail("Wrong number of arguments", callNode);

    for (unsigned i = 0; i < args.length(); i++) {
        if (!(args.type(i) <= callee.argType(i)))
            return f.fail("actual arg type is not subtype of formal arg type", callNode);
    }

    if (!f.internalCall(callee, args, def))
        return false;

    *type = callee.returnType().toType();
    return true;
}

static bool
CheckFuncPtrCall(FunctionCompiler &f, ParseNode *callNode, MDefinition **def, Type *type)
{
    ParseNode *callee = CallCallee(callNode);
    ParseNode *elemBase = ElemBase(callee);
    ParseNode *indexExpr = ElemIndex(callee);

    if (!elemBase->isKind(PNK_NAME))
        return f.fail("Expecting name (of function-pointer array)", elemBase);

    const ModuleCompiler::FuncPtrTable *table = f.m().lookupFuncPtrTable(elemBase->name());
    if (!table)
        return f.fail("Expecting name of function-pointer array", elemBase);

    if (!indexExpr->isKind(PNK_BITAND))
        return f.fail("Function-pointer table index expression needs & mask", indexExpr);

    ParseNode *indexNode = BinaryLeft(indexExpr);
    ParseNode *maskNode = BinaryRight(indexExpr);

    uint32_t mask;
    if (!IsLiteralUint32(maskNode, &mask) || mask != table->mask())
        return f.fail("Function-pointer table index mask must be the table length minus 1", maskNode);

    MDefinition *indexDef;
    Type indexType;
    if (!CheckExpr(f, indexNode, Use::ToInt32, &indexDef, &indexType))
        return false;

    if (!indexType.isIntish())
        return f.fail("Function-pointer table index expression must be intish", indexNode);

    FunctionCompiler::Args args(f);

    if (!CheckCallArgs(f, callNode, Use::NoCoercion, &args))
        return false;

    if (args.length() != table->sig().numArgs())
        return f.fail("Wrong number of arguments", callNode);

    for (unsigned i = 0; i < args.length(); i++) {
        if (!(args.type(i) <= table->sig().argType(i)))
            return f.fail("actual arg type is not subtype of formal arg type", callNode);
    }

    if (!f.funcPtrCall(*table, indexDef, args, def))
        return false;

    *type = table->sig().returnType().toType();
    return true;
}

static bool
CheckFFICall(FunctionCompiler &f, ParseNode *callNode, unsigned ffiIndex, Use use,
             MDefinition **def, Type *type)
{
    FunctionCompiler::Args args(f);

    if (!CheckCallArgs(f, callNode, Use::NoCoercion, &args))
        return false;

    MIRTypeVector argMIRTypes(f.cx());
    for (unsigned i = 0; i < args.length(); i++) {
        if (!args.type(i).isExtern())
            return f.fail("args to FFI call must be <: extern", callNode);
        if (!argMIRTypes.append(args.type(i).toMIRType()))
            return false;
    }

    unsigned exitIndex;
    if (!f.m().addExit(ffiIndex, CallCallee(callNode)->name(), Move(argMIRTypes), use, &exitIndex))
        return false;

    if (!f.ffiCall(exitIndex, args, use.toMIRType(), def))
        return false;

    *type = use.toFFIReturnType();
    return true;
}

static inline void *
UnaryMathFunCast(double (*pf)(double))
{
    return JS_FUNC_TO_DATA_PTR(void*, pf);
}

static inline void *
BinaryMathFunCast(double (*pf)(double, double))
{
    return JS_FUNC_TO_DATA_PTR(void*, pf);
}

static bool
CheckMathBuiltinCall(FunctionCompiler &f, ParseNode *callNode, AsmJSMathBuiltin mathBuiltin,
                     MDefinition **def, Type *type)
{
    unsigned arity = 0;
    void *callee = NULL;
    switch (mathBuiltin) {
      case AsmJSMathBuiltin_imul:  return CheckMathIMul(f, callNode, def, type);
      case AsmJSMathBuiltin_abs:   return CheckMathAbs(f, callNode, def, type);
      case AsmJSMathBuiltin_sin:   arity = 1; callee = UnaryMathFunCast(sin);        break;
      case AsmJSMathBuiltin_cos:   arity = 1; callee = UnaryMathFunCast(cos);        break;
      case AsmJSMathBuiltin_tan:   arity = 1; callee = UnaryMathFunCast(tan);        break;
      case AsmJSMathBuiltin_asin:  arity = 1; callee = UnaryMathFunCast(asin);       break;
      case AsmJSMathBuiltin_acos:  arity = 1; callee = UnaryMathFunCast(acos);       break;
      case AsmJSMathBuiltin_atan:  arity = 1; callee = UnaryMathFunCast(atan);       break;
      case AsmJSMathBuiltin_ceil:  arity = 1; callee = UnaryMathFunCast(ceil);       break;
      case AsmJSMathBuiltin_floor: arity = 1; callee = UnaryMathFunCast(floor);      break;
      case AsmJSMathBuiltin_exp:   arity = 1; callee = UnaryMathFunCast(exp);        break;
      case AsmJSMathBuiltin_log:   arity = 1; callee = UnaryMathFunCast(log);        break;
      case AsmJSMathBuiltin_sqrt:  arity = 1; callee = UnaryMathFunCast(sqrt);       break;
      case AsmJSMathBuiltin_pow:   arity = 2; callee = BinaryMathFunCast(ecmaPow);   break;
      case AsmJSMathBuiltin_atan2: arity = 2; callee = BinaryMathFunCast(ecmaAtan2); break;
    }

    FunctionCompiler::Args args(f);

    if (!CheckCallArgs(f, callNode, Use::ToNumber, &args))
        return false;

    if (args.length() != arity)
        return f.fail("Math builtin call passed wrong number of argument", callNode);

    for (unsigned i = 0; i < args.length(); i++) {
        if (!args.type(i).isDoublish())
            return f.fail("Builtin calls must be passed 1 doublish argument", callNode);
    }

    if (!f.builtinCall(callee, args, MIRType_Double, def))
        return false;

    *type = Type::Double;
    return true;
}

static bool
CheckCall(FunctionCompiler &f, ParseNode *call, Use use, MDefinition **def, Type *type)
{
    ParseNode *callee = CallCallee(call);

    if (callee->isKind(PNK_ELEM))
        return CheckFuncPtrCall(f, call, def, type);

    if (!callee->isKind(PNK_NAME))
        return f.fail("Unexpected callee expression type", callee);

    if (const ModuleCompiler::Global *global = f.lookupGlobal(callee->name())) {
        switch (global->which()) {
          case ModuleCompiler::Global::Function:
            return CheckInternalCall(f, call, f.m().function(global->funcIndex()), def, type);
          case ModuleCompiler::Global::FFI:
            return CheckFFICall(f, call, global->ffiIndex(), use, def, type);
          case ModuleCompiler::Global::MathBuiltin:
            return CheckMathBuiltinCall(f, call, global->mathBuiltin(), def, type);
          case ModuleCompiler::Global::Constant:
          case ModuleCompiler::Global::Variable:
          case ModuleCompiler::Global::FuncPtrTable:
          case ModuleCompiler::Global::ArrayView:
            return f.fail("Global is not callable function", callee);
        }
    }

    return f.fail("Call target not found in global scope", callee);
}

static bool
CheckPos(FunctionCompiler &f, ParseNode *pos, MDefinition **def, Type *type)
{
    JS_ASSERT(pos->isKind(PNK_POS));
    ParseNode *operand = UnaryKid(pos);

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, Use::ToNumber, &operandDef, &operandType))
        return false;

    if (operandType.isSigned())
        *def = f.unary<MToDouble>(operandDef);
    else if (operandType.isUnsigned())
        *def = f.unary<MAsmJSUnsignedToDouble>(operandDef);
    else if (operandType.isDoublish())
        *def = operandDef;
    else
        return f.fail("Operand to unary + must be signed, unsigned or doubleish", operand);

    *type = Type::Double;
    return true;
}

static bool
CheckNot(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_NOT));
    ParseNode *operand = UnaryKid(expr);

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, Use::NoCoercion, &operandDef, &operandType))
        return false;

    if (!operandType.isInt())
        return f.fail("Operand to ! must be int", operand);

    *def = f.unary<MNot>(operandDef);
    *type = Type::Int;
    return true;
}

static bool
CheckNeg(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_NEG));
    ParseNode *operand = UnaryKid(expr);

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, Use::ToNumber, &operandDef, &operandType))
        return false;

    if (operandType.isInt()) {
        *def = f.unary<MAsmJSNeg>(operandDef, MIRType_Int32);
        *type = Type::Intish;
        return true;
    }

    if (operandType.isDoublish()) {
        *def = f.unary<MAsmJSNeg>(operandDef, MIRType_Double);
        *type = Type::Double;
        return true;
    }

    return f.fail("Operand to unary - must be an int", operand);
}

static bool
CheckBitNot(FunctionCompiler &f, ParseNode *neg, MDefinition **def, Type *type)
{
    JS_ASSERT(neg->isKind(PNK_BITNOT));
    ParseNode *operand = UnaryKid(neg);

    if (operand->isKind(PNK_BITNOT)) {
        MDefinition *operandDef;
        Type operandType;
        if (!CheckExpr(f, UnaryKid(operand), Use::NoCoercion, &operandDef, &operandType))
            return false;

        if (operandType.isDouble()) {
            *def = f.unary<MTruncateToInt32>(operandDef);
            *type = Type::Signed;
            return true;
        }
    }

    MDefinition *operandDef;
    Type operandType;
    if (!CheckExpr(f, operand, Use::ToInt32, &operandDef, &operandType))
        return false;

    if (!operandType.isIntish())
        return f.fail("Operand to ~ must be intish", operand);

    *def = f.bitwise<MBitNot>(operandDef);
    *type = Type::Signed;
    return true;
}

static bool
CheckComma(FunctionCompiler &f, ParseNode *comma, Use use, MDefinition **def, Type *type)
{
    JS_ASSERT(comma->isKind(PNK_COMMA));
    ParseNode *operands = ListHead(comma);

    ParseNode *pn = operands;
    for (; NextNode(pn); pn = NextNode(pn)) {
        if (!CheckExpr(f, pn, Use::NoCoercion, def, type))
            return false;
    }

    if (!CheckExpr(f, pn, use, def, type))
        return false;

    return true;
}

static bool
CheckConditional(FunctionCompiler &f, ParseNode *ternary, MDefinition **def, Type *type)
{
    JS_ASSERT(ternary->isKind(PNK_CONDITIONAL));
    ParseNode *cond = TernaryKid1(ternary);
    ParseNode *thenExpr = TernaryKid2(ternary);
    ParseNode *elseExpr = TernaryKid3(ternary);

    MDefinition *condDef;
    Type condType;
    if (!CheckExpr(f, cond, Use::NoCoercion, &condDef, &condType))
        return false;

    if (!condType.isInt())
        return f.fail("Condition of if must be boolish", cond);

    MBasicBlock *thenBlock, *elseBlock;
    if (!f.branchAndStartThen(condDef, &thenBlock, &elseBlock))
        return false;

    MDefinition *thenDef;
    Type thenType;
    if (!CheckExpr(f, thenExpr, Use::NoCoercion, &thenDef, &thenType))
        return false;

    f.pushPhiInput(thenDef);
    MBasicBlock *thenEnd = f.switchToElse(elseBlock);

    MDefinition *elseDef;
    Type elseType;
    if (!CheckExpr(f, elseExpr, Use::NoCoercion, &elseDef, &elseType))
        return false;

    f.pushPhiInput(elseDef);
    if (!f.joinIfElse(thenEnd))
        return false;
    *def = f.popPhiOutput();

    if (thenType.isInt() && elseType.isInt())
        *type = Type::Int;
    else if (thenType.isDouble() && elseType.isDouble())
        *type = Type::Double;
    else
        return f.fail("Then/else branches of conditional must both be int or double", ternary);

    return true;
}

static bool
IsValidIntMultiplyConstant(ParseNode *expr)
{
    if (!IsNumericLiteral(expr))
        return false;

    NumLit literal = ExtractNumericLiteral(expr);
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
        if (abs(literal.toInt32()) < (1<<20))
            return true;
        return false;
      case NumLit::BigUnsigned:
      case NumLit::Double:
      case NumLit::OutOfRangeInt:
        return false;
    }

    JS_NOT_REACHED("Bad literal");
    return false;
}

static bool
CheckMultiply(FunctionCompiler &f, ParseNode *star, MDefinition **def, Type *type)
{
    JS_ASSERT(star->isKind(PNK_STAR));
    ParseNode *lhs = BinaryLeft(star);
    ParseNode *rhs = BinaryRight(star);

    MDefinition *lhsDef;
    Type lhsType;
    if (!CheckExpr(f, lhs, Use::ToNumber, &lhsDef, &lhsType))
        return false;

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, Use::ToNumber, &rhsDef, &rhsType))
        return false;

    if (lhsType.isInt() && rhsType.isInt()) {
        if (!IsValidIntMultiplyConstant(lhs) && !IsValidIntMultiplyConstant(rhs))
            return f.fail("One arg to int multiply must be small (-2^20, 2^20) int literal", star);
        *def = f.mul(lhsDef, rhsDef, MIRType_Int32, MMul::Integer);
        *type = Type::Signed;
        return true;
    }

    if (lhsType.isDoublish() && rhsType.isDoublish()) {
        *def = f.mul(lhsDef, rhsDef, MIRType_Double, MMul::Normal);
        *type = Type::Double;
        return true;
    }

    return f.fail("Arguments to * must both be doubles", star);
}

static bool
CheckAddOrSub(FunctionCompiler &f, ParseNode *expr, Use use, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_ADD) || expr->isKind(PNK_SUB));
    ParseNode *lhs = BinaryLeft(expr);
    ParseNode *rhs = BinaryRight(expr);

    Use argUse;
    unsigned addOrSubCount = 1;
    if (use.which() == Use::AddOrSub) {
        if (++use.addOrSubCount() > (1<<20))
            return f.fail("Too many + or - without intervening coercion", expr);
        argUse = use;
    } else {
        argUse = Use(&addOrSubCount);
    }

    MDefinition *lhsDef, *rhsDef;
    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, argUse, &lhsDef, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, argUse, &rhsDef, &rhsType))
        return false;

    if (lhsType.isInt() && rhsType.isInt()) {
        *def = expr->isKind(PNK_ADD)
               ? f.binary<MAdd>(lhsDef, rhsDef, MIRType_Int32)
               : f.binary<MSub>(lhsDef, rhsDef, MIRType_Int32);
        if (use.which() == Use::AddOrSub)
            *type = Type::Int;
        else
            *type = Type::Intish;
        return true;
    }

    if (expr->isKind(PNK_ADD) && lhsType.isDouble() && rhsType.isDouble()) {
        *def = f.binary<MAdd>(lhsDef, rhsDef, MIRType_Double);
        *type = Type::Double;
        return true;
    }

    if (expr->isKind(PNK_SUB) && lhsType.isDoublish() && rhsType.isDoublish()) {
        *def = f.binary<MSub>(lhsDef, rhsDef, MIRType_Double);
        *type = Type::Double;
        return true;
    }

    return f.fail("Arguments to + or - must both be ints or doubles", expr);
}

static bool
CheckDivOrMod(FunctionCompiler &f, ParseNode *expr, MDefinition **def, Type *type)
{
    JS_ASSERT(expr->isKind(PNK_DIV) || expr->isKind(PNK_MOD));
    ParseNode *lhs = BinaryLeft(expr);
    ParseNode *rhs = BinaryRight(expr);

    MDefinition *lhsDef, *rhsDef;
    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, Use::ToNumber, &lhsDef, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, Use::ToNumber, &rhsDef, &rhsType))
        return false;

    if (lhsType.isDoublish() && rhsType.isDoublish()) {
        *def = expr->isKind(PNK_DIV)
               ? f.binary<MDiv>(lhsDef, rhsDef, MIRType_Double)
               : f.binary<MMod>(lhsDef, rhsDef, MIRType_Double);
        *type = Type::Double;
        return true;
    }

    if (lhsType.isSigned() && rhsType.isSigned()) {
        if (expr->isKind(PNK_DIV)) {
            *def = f.binary<MDiv>(lhsDef, rhsDef, MIRType_Int32);
            *type = Type::Intish;
        } else {
            *def = f.binary<MMod>(lhsDef, rhsDef, MIRType_Int32);
            *type = Type::Int;
        }
        return true;
    }

    if (lhsType.isUnsigned() && rhsType.isUnsigned()) {
        if (expr->isKind(PNK_DIV)) {
            *def = f.binary<MAsmJSUDiv>(lhsDef, rhsDef);
            *type = Type::Intish;
        } else {
            *def = f.binary<MAsmJSUMod>(lhsDef, rhsDef);
            *type = Type::Int;
        }
        return true;
    }

    return f.fail("Arguments to / or % must both be double, signed, or unsigned", expr);
}

static bool
CheckComparison(FunctionCompiler &f, ParseNode *comp, MDefinition **def, Type *type)
{
    JS_ASSERT(comp->isKind(PNK_LT) || comp->isKind(PNK_LE) || comp->isKind(PNK_GT) ||
              comp->isKind(PNK_GE) || comp->isKind(PNK_EQ) || comp->isKind(PNK_NE));
    ParseNode *lhs = BinaryLeft(comp);
    ParseNode *rhs = BinaryRight(comp);

    MDefinition *lhsDef, *rhsDef;
    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, Use::NoCoercion, &lhsDef, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, Use::NoCoercion, &rhsDef, &rhsType))
        return false;

    if ((lhsType.isSigned() && rhsType.isSigned()) || (lhsType.isUnsigned() && rhsType.isUnsigned())) {
        MCompare::CompareType compareType = lhsType.isUnsigned()
                                            ? MCompare::Compare_UInt32
                                            : MCompare::Compare_Int32;
        *def = f.compare(lhsDef, rhsDef, comp->getOp(), compareType);
        *type = Type::Int;
        return true;
    }

    if (lhsType.isDouble() && rhsType.isDouble()) {
        *def = f.compare(lhsDef, rhsDef, comp->getOp(), MCompare::Compare_Double);
        *type = Type::Int;
        return true;
    }

    return f.fail("The arguments to a comparison must both be signed, unsigned or doubles", comp);
}

static bool
CheckBitwise(FunctionCompiler &f, ParseNode *bitwise, MDefinition **def, Type *type)
{
    ParseNode *lhs = BinaryLeft(bitwise);
    ParseNode *rhs = BinaryRight(bitwise);

    int32_t identityElement;
    bool onlyOnRight;
    switch (bitwise->getKind()) {
      case PNK_BITOR:  identityElement = 0;  onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_BITAND: identityElement = -1; onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_BITXOR: identityElement = 0;  onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_LSH:    identityElement = 0;  onlyOnRight = true;  *type = Type::Signed;   break;
      case PNK_RSH:    identityElement = 0;  onlyOnRight = true;  *type = Type::Signed;   break;
      case PNK_URSH:   identityElement = 0;  onlyOnRight = true;  *type = Type::Unsigned; break;
      default: JS_NOT_REACHED("not a bitwise op");
    }

    if (!onlyOnRight && IsBits32(lhs, identityElement)) {
        Type rhsType;
        if (!CheckExpr(f, rhs, Use::ToInt32, def, &rhsType))
            return false;
        if (!rhsType.isIntish())
            return f.fail("Operands to bitwise ops must be intish", bitwise);
        return true;
    }

    if (IsBits32(rhs, identityElement)) {
        Type lhsType;
        if (!CheckExpr(f, lhs, Use::ToInt32, def, &lhsType))
            return false;
        if (!lhsType.isIntish())
            return f.fail("Operands to bitwise ops must be intish", bitwise);
        return true;
    }

    MDefinition *lhsDef;
    Type lhsType;
    if (!CheckExpr(f, lhs, Use::ToInt32, &lhsDef, &lhsType))
        return false;

    MDefinition *rhsDef;
    Type rhsType;
    if (!CheckExpr(f, rhs, Use::ToInt32, &rhsDef, &rhsType))
        return false;

    if (!lhsType.isIntish() || !rhsType.isIntish())
        return f.fail("Operands to bitwise ops must be intish", bitwise);

    switch (bitwise->getKind()) {
      case PNK_BITOR:  *def = f.bitwise<MBitOr>(lhsDef, rhsDef); break;
      case PNK_BITAND: *def = f.bitwise<MBitAnd>(lhsDef, rhsDef); break;
      case PNK_BITXOR: *def = f.bitwise<MBitXor>(lhsDef, rhsDef); break;
      case PNK_LSH:    *def = f.bitwise<MLsh>(lhsDef, rhsDef); break;
      case PNK_RSH:    *def = f.bitwise<MRsh>(lhsDef, rhsDef); break;
      case PNK_URSH:   *def = f.bitwise<MUrsh>(lhsDef, rhsDef); break;
      default: JS_NOT_REACHED("not a bitwise op");
    }

    return true;
}

static bool
CheckExpr(FunctionCompiler &f, ParseNode *expr, Use use, MDefinition **def, Type *type)
{
    JS_CHECK_RECURSION(f.cx(), return false);

    if (!f.mirGen().ensureBallast())
        return false;

    if (IsNumericLiteral(expr))
        return CheckNumericLiteral(f, expr, def, type);

    switch (expr->getKind()) {
      case PNK_NAME:        return CheckVarRef(f, expr, def, type);
      case PNK_ELEM:        return CheckArrayLoad(f, expr, def, type);
      case PNK_ASSIGN:      return CheckAssign(f, expr, def, type);
      case PNK_CALL:        return CheckCall(f, expr, use, def, type);
      case PNK_POS:         return CheckPos(f, expr, def, type);
      case PNK_NOT:         return CheckNot(f, expr, def, type);
      case PNK_NEG:         return CheckNeg(f, expr, def, type);
      case PNK_BITNOT:      return CheckBitNot(f, expr, def, type);
      case PNK_COMMA:       return CheckComma(f, expr, use, def, type);
      case PNK_CONDITIONAL: return CheckConditional(f, expr, def, type);

      case PNK_STAR:        return CheckMultiply(f, expr, def, type);

      case PNK_ADD:
      case PNK_SUB:         return CheckAddOrSub(f, expr, use, def, type);

      case PNK_DIV:
      case PNK_MOD:         return CheckDivOrMod(f, expr, def, type);

      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_EQ:
      case PNK_NE:          return CheckComparison(f, expr, def, type);

      case PNK_BITOR:
      case PNK_BITAND:
      case PNK_BITXOR:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:        return CheckBitwise(f, expr, def, type);

      default:;
    }

    return f.fail("Unsupported expression", expr);
}

static bool
CheckStatement(FunctionCompiler &f, ParseNode *stmt, LabelVector *maybeLabels = NULL);

static bool
CheckExprStatement(FunctionCompiler &f, ParseNode *exprStmt)
{
    JS_ASSERT(exprStmt->isKind(PNK_SEMI));
    ParseNode *expr = UnaryKid(exprStmt);

    if (!expr)
        return true;

    MDefinition *_1;
    Type _2;
    if (!CheckExpr(f, UnaryKid(exprStmt), Use::NoCoercion, &_1, &_2))
        return false;

    return true;
}

static bool
CheckWhile(FunctionCompiler &f, ParseNode *whileStmt, const LabelVector *maybeLabels)
{
    JS_ASSERT(whileStmt->isKind(PNK_WHILE));
    ParseNode *cond = BinaryLeft(whileStmt);
    ParseNode *body = BinaryRight(whileStmt);

    MBasicBlock *loopEntry;
    if (!f.startPendingLoop(whileStmt, &loopEntry))
        return false;

    MDefinition *condDef;
    Type condType;
    if (!CheckExpr(f, cond, Use::NoCoercion, &condDef, &condType))
        return false;

    if (!condType.isInt())
        return f.fail("Condition of while loop must be boolish", cond);

    MBasicBlock *afterLoop;
    if (!f.branchAndStartLoopBody(condDef, &afterLoop))
        return false;

    if (!CheckStatement(f, body))
        return false;

    if (!f.bindContinues(whileStmt, maybeLabels))
        return false;

    return f.closeLoop(loopEntry, afterLoop);
}

static bool
CheckFor(FunctionCompiler &f, ParseNode *forStmt, const LabelVector *maybeLabels)
{
    JS_ASSERT(forStmt->isKind(PNK_FOR));
    ParseNode *forHead = BinaryLeft(forStmt);
    ParseNode *body = BinaryRight(forStmt);

    if (!forHead->isKind(PNK_FORHEAD))
        return f.fail("Unsupported for-loop statement", forHead);

    ParseNode *maybeInit = TernaryKid1(forHead);
    ParseNode *maybeCond = TernaryKid2(forHead);
    ParseNode *maybeInc = TernaryKid3(forHead);

    if (maybeInit) {
        MDefinition *_1;
        Type _2;
        if (!CheckExpr(f, maybeInit, Use::NoCoercion, &_1, &_2))
            return false;
    }

    MBasicBlock *loopEntry;
    if (!f.startPendingLoop(forStmt, &loopEntry))
        return false;

    MDefinition *condDef;
    if (maybeCond) {
        Type condType;
        if (!CheckExpr(f, maybeCond, Use::NoCoercion, &condDef, &condType))
            return false;

        if (!condType.isInt())
            return f.fail("Condition of while loop must be boolish", maybeCond);
    } else {
        condDef = f.constant(Int32Value(1));
    }

    MBasicBlock *afterLoop;
    if (!f.branchAndStartLoopBody(condDef, &afterLoop))
        return false;

    if (!CheckStatement(f, body))
        return false;

    if (!f.bindContinues(forStmt, maybeLabels))
        return false;

    if (maybeInc) {
        MDefinition *_1;
        Type _2;
        if (!CheckExpr(f, maybeInc, Use::NoCoercion, &_1, &_2))
            return false;
    }

    return f.closeLoop(loopEntry, afterLoop);
}

static bool
CheckDoWhile(FunctionCompiler &f, ParseNode *whileStmt, const LabelVector *maybeLabels)
{
    JS_ASSERT(whileStmt->isKind(PNK_DOWHILE));
    ParseNode *body = BinaryLeft(whileStmt);
    ParseNode *cond = BinaryRight(whileStmt);

    MBasicBlock *loopEntry;
    if (!f.startPendingLoop(whileStmt, &loopEntry))
        return false;

    if (!CheckStatement(f, body))
        return false;

    if (!f.bindContinues(whileStmt, maybeLabels))
        return false;

    MDefinition *condDef;
    Type condType;
    if (!CheckExpr(f, cond, Use::NoCoercion, &condDef, &condType))
        return false;

    if (!condType.isInt())
        return f.fail("Condition of while loop must be boolish", cond);

    return f.branchAndCloseDoWhileLoop(condDef, loopEntry);
}

static bool
CheckLabel(FunctionCompiler &f, ParseNode *labeledStmt, LabelVector *maybeLabels)
{
    JS_ASSERT(labeledStmt->isKind(PNK_COLON));
    PropertyName *label = LabeledStatementLabel(labeledStmt);
    ParseNode *stmt = LabeledStatementStatement(labeledStmt);

    if (maybeLabels) {
        if (!maybeLabels->append(label))
            return false;
        if (!CheckStatement(f, stmt, maybeLabels))
            return false;
        return true;
    }

    LabelVector labels(f.cx());
    if (!labels.append(label))
        return false;

    if (!CheckStatement(f, stmt, &labels))
        return false;

    return f.bindLabeledBreaks(&labels);
}

static bool
CheckIf(FunctionCompiler &f, ParseNode *ifStmt)
{
    JS_ASSERT(ifStmt->isKind(PNK_IF));
    ParseNode *cond = TernaryKid1(ifStmt);
    ParseNode *thenStmt = TernaryKid2(ifStmt);
    ParseNode *elseStmt = TernaryKid3(ifStmt);

    MDefinition *condDef;
    Type condType;
    if (!CheckExpr(f, cond, Use::NoCoercion, &condDef, &condType))
        return false;

    if (!condType.isInt())
        return f.fail("Condition of if must be boolish", cond);

    MBasicBlock *thenBlock, *elseBlock;
    if (!f.branchAndStartThen(condDef, &thenBlock, &elseBlock))
        return false;

    if (!CheckStatement(f, thenStmt))
        return false;

    if (!elseStmt) {
        f.joinIf(elseBlock);
    } else {
        MBasicBlock *thenEnd = f.switchToElse(elseBlock);
        if (!CheckStatement(f, elseStmt))
            return false;
        if (!f.joinIfElse(thenEnd))
            return false;
    }

    return true;
}

static bool
CheckCaseExpr(FunctionCompiler &f, ParseNode *caseExpr, int32_t *value)
{
    if (!IsNumericLiteral(caseExpr))
        return f.fail("Switch case expression must be an integer literal", caseExpr);

    NumLit literal = ExtractNumericLiteral(caseExpr);
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
        *value = literal.toInt32();
        break;
      case NumLit::OutOfRangeInt:
      case NumLit::BigUnsigned:
        return f.fail("Switch case expression out of integer range", caseExpr);
      case NumLit::Double:
        return f.fail("Switch case expression must be an integer literal", caseExpr);
    }

    return true;
}

static bool
CheckDefaultAtEnd(FunctionCompiler &f, ParseNode *stmt)
{
    for (; stmt; stmt = NextNode(stmt)) {
        JS_ASSERT(stmt->isKind(PNK_CASE) || stmt->isKind(PNK_DEFAULT));
        if (stmt->isKind(PNK_DEFAULT) && NextNode(stmt) != NULL)
            return f.fail("default label must be at the end", stmt);
    }

    return true;
}

static bool
CheckSwitchRange(FunctionCompiler &f, ParseNode *stmt, int32_t *low, int32_t *high,
                 int32_t *tableLength)
{
    if (stmt->isKind(PNK_DEFAULT)) {
        *low = 0;
        *high = -1;
        *tableLength = 0;
        return true;
    }

    int32_t i = 0;
    if (!CheckCaseExpr(f, CaseExpr(stmt), &i))
        return false;

    *low = *high = i;

    for (stmt = NextNode(stmt); stmt && stmt->isKind(PNK_CASE); stmt = NextNode(stmt)) {
        int32_t i = 0;
        if (!CheckCaseExpr(f, CaseExpr(stmt), &i))
            return false;

        *low = Min(*low, i);
        *high = Max(*high, i);
    }

    int64_t i64 = (int64_t(*high) - int64_t(*low)) + 1;
    if (i64 > 512*1024*1024)
        return f.fail("All switch statements generate tables; this table would be bigger than 512MiB", stmt);

    *tableLength = int32_t(i64);
    return true;
}

static bool
CheckSwitch(FunctionCompiler &f, ParseNode *switchStmt)
{
    JS_ASSERT(switchStmt->isKind(PNK_SWITCH));
    ParseNode *switchExpr = BinaryLeft(switchStmt);
    ParseNode *switchBody = BinaryRight(switchStmt);

    if (!switchBody->isKind(PNK_STATEMENTLIST))
        return f.fail("Switch body may not contain 'let' declarations", switchBody);

    MDefinition *exprDef;
    Type exprType;
    if (!CheckExpr(f, switchExpr, Use::NoCoercion, &exprDef, &exprType))
        return false;

    if (!exprType.isSigned())
        return f.fail("Switch expression must be a signed integer", switchExpr);

    ParseNode *stmt = ListHead(switchBody);

    if (!CheckDefaultAtEnd(f, stmt))
        return false;

    if (!stmt)
        return true;

    int32_t low = 0, high = 0, tableLength = 0;
    if (!CheckSwitchRange(f, stmt, &low, &high, &tableLength))
        return false;

    CaseVector cases(f.cx());
    if (!cases.resize(tableLength))
        return false;

    MBasicBlock *switchBlock;
    if (!f.startSwitch(switchStmt, exprDef, low, high, &switchBlock))
        return false;

    for (; stmt && stmt->isKind(PNK_CASE); stmt = NextNode(stmt)) {
        int32_t caseValue = ExtractNumericLiteral(CaseExpr(stmt)).toInt32();
        unsigned caseIndex = caseValue - low;

        if (cases[caseIndex])
            return f.fail("No duplicate case labels", stmt);

        if (!f.startSwitchCase(switchBlock, &cases[caseIndex]))
            return false;

        if (!CheckStatement(f, CaseBody(stmt)))
            return false;
    }

    MBasicBlock *defaultBlock;
    if (!f.startSwitchDefault(switchBlock, &cases, &defaultBlock))
        return false;

    if (stmt && stmt->isKind(PNK_DEFAULT)) {
        if (!CheckStatement(f, CaseBody(stmt)))
            return false;
    }

    return f.joinSwitch(switchBlock, cases, defaultBlock);
}

static bool
CheckReturn(FunctionCompiler &f, ParseNode *returnStmt)
{
    JS_ASSERT(returnStmt->isKind(PNK_RETURN));
    ParseNode *expr = UnaryKid(returnStmt);

    if (!expr) {
        if (f.func().returnType().which() != RetType::Void)
            return f.fail("All return statements must return void", returnStmt);

        f.returnVoid();
        return true;
    }

    MDefinition *def;
    Type type;
    if (!CheckExpr(f, expr, Use::NoCoercion, &def, &type))
        return false;

    if (!(type <= f.func().returnType()))
        return f.fail("All returns must return the same type", expr);

    if (f.func().returnType().which() == RetType::Void)
        f.returnVoid();
    else
        f.returnExpr(def);
    return true;
}

static bool
CheckStatements(FunctionCompiler &f, ParseNode *stmtHead)
{
    for (ParseNode *stmt = stmtHead; stmt; stmt = NextNode(stmt)) {
        if (!CheckStatement(f, stmt))
            return false;
    }

    return true;
}

static bool
CheckStatementList(FunctionCompiler &f, ParseNode *stmt)
{
    JS_ASSERT(stmt->isKind(PNK_STATEMENTLIST));
    return CheckStatements(f, ListHead(stmt));
}

static bool
CheckStatement(FunctionCompiler &f, ParseNode *stmt, LabelVector *maybeLabels)
{
    JS_CHECK_RECURSION(f.cx(), return false);

    if (!f.mirGen().ensureBallast())
        return false;

    switch (stmt->getKind()) {
      case PNK_SEMI:          return CheckExprStatement(f, stmt);
      case PNK_WHILE:         return CheckWhile(f, stmt, maybeLabels);
      case PNK_FOR:           return CheckFor(f, stmt, maybeLabels);
      case PNK_DOWHILE:       return CheckDoWhile(f, stmt, maybeLabels);
      case PNK_COLON:         return CheckLabel(f, stmt, maybeLabels);
      case PNK_IF:            return CheckIf(f, stmt);
      case PNK_SWITCH:        return CheckSwitch(f, stmt);
      case PNK_RETURN:        return CheckReturn(f, stmt);
      case PNK_STATEMENTLIST: return CheckStatementList(f, stmt);
      case PNK_BREAK:         return f.addBreak(LoopControlMaybeLabel(stmt));
      case PNK_CONTINUE:      return f.addContinue(LoopControlMaybeLabel(stmt));
      default:;
    }

    return f.fail("Unexpected statement kind", stmt);
}

static bool
CheckVariableDecl(ModuleCompiler &m, ParseNode *var, FunctionCompiler::LocalMap *locals)
{
    if (!IsDefinition(var))
        return m.fail("Local variable names must not restate argument names", var);

    PropertyName *name = var->name();

    if (!CheckIdentifier(m, name, var))
        return false;

    ParseNode *initNode = MaybeDefinitionInitializer(var);
    if (!initNode)
        return m.fail("Variable needs explicit type declaration via an initial value", var);

    if (!IsNumericLiteral(initNode))
        return m.fail("Variable initialization value needs to be a numeric literal", initNode);

    NumLit literal = ExtractNumericLiteral(initNode);
    VarType type;
    switch (literal.which()) {
      case NumLit::Fixnum:
      case NumLit::NegativeInt:
      case NumLit::BigUnsigned:
        type = VarType::Int;
        break;
      case NumLit::Double:
        type = VarType::Double;
        break;
      case NumLit::OutOfRangeInt:
        return m.fail("Variable initializer is out of representable integer range", initNode);
    }

    FunctionCompiler::LocalMap::AddPtr p = locals->lookupForAdd(name);
    if (p)
        return m.fail("Local names must be unique", initNode);

    unsigned slot = locals->count();
    if (!locals->add(p, name, FunctionCompiler::Local(type, slot, literal.value())))
        return false;

    return true;
}

static bool
CheckVariableDecls(ModuleCompiler &m, FunctionCompiler::LocalMap *locals, ParseNode **stmtIter)
{
    ParseNode *stmt = *stmtIter;

    for (; stmt && stmt->isKind(PNK_VAR); stmt = NextNode(stmt)) {
        for (ParseNode *var = VarListHead(stmt); var; var = NextNode(var)) {
            if (!CheckVariableDecl(m, var, locals))
                return false;
        }
    }

    *stmtIter = stmt;
    return true;
}

static MIRGenerator *
CheckFunctionBody(ModuleCompiler &m, ModuleCompiler::Func &func, LifoAlloc &lifo)
{
    // CheckFunctionSignature already has already checked the
    // function head as well as argument type declarations. The ParseNode*
    // stored in f.body points to the first non-argument statement.
    ParseNode *stmtIter = func.body();

    FunctionCompiler::LocalMap locals(m.cx());
    if (!locals.init())
        return NULL;

    unsigned numFormals;
    ParseNode *arg = FunctionArgsList(func.fn(), &numFormals);
    for (unsigned i = 0; i < numFormals; i++, arg = NextNode(arg)) {
        if (!locals.putNew(arg->name(), FunctionCompiler::Local(func.argType(i), i)))
            return NULL;
    }

    if (!CheckVariableDecls(m, &locals, &stmtIter))
        return NULL;

    // Force Ion allocations to occur in the LifoAlloc while in scope.
    TempAllocator *tempAlloc = lifo.new_<TempAllocator>(&lifo);
    IonContext icx(m.cx()->compartment, tempAlloc);

    // Allocate objects required for MIR generation.
    // Memory for the objects is provided by the LifoAlloc argument,
    // which may be explicitly tracked by the caller.
    MIRGraph *graph = lifo.new_<MIRGraph>(tempAlloc);
    CompileInfo *info = lifo.new_<CompileInfo>(locals.count());
    MIRGenerator *mirGen = lifo.new_<MIRGenerator>(m.cx()->compartment, tempAlloc, graph, info);
    JS_ASSERT(tempAlloc && graph && info && mirGen);

    FunctionCompiler f(m, func, Move(locals), mirGen);
    if (!f.init())
        return NULL;

    if (!CheckStatements(f, stmtIter))
        return NULL;

    f.returnVoid();
    JS_ASSERT(!tempAlloc->rootList());

    return mirGen;
}

static bool
GenerateAsmJSCode(ModuleCompiler &m, ModuleCompiler::Func &func,
                  MIRGenerator &mirGen, LIRGraph &lir)
{
    m.masm().bind(func.codeLabel());

    ScopedJSDeletePtr<CodeGenerator> codegen(GenerateCode(&mirGen, &lir, &m.masm()));
    if (!codegen)
        return m.fail("Internal codegen failure (probably out of memory)", func.fn());

    if (!m.collectAccesses(mirGen))
        return false;

    // A single MacroAssembler is reused for all function compilations so
    // that there is a single linear code segment for each module. To avoid
    // spiking memory, a LifoAllocScope in the caller frees all MIR/LIR
    // after each function is compiled. This method is responsible for cleaning
    // out any dangling pointers that the MacroAssembler may have kept.
    m.masm().resetForNewCodeGenerator();

    // Align internal function headers.
    m.masm().align(CodeAlignment);

    // Unlike regular IonMonkey which links and generates a new IonCode for
    // every function, we accumulate all the functions in the module in a
    // single MacroAssembler and link at end. Linking asm.js doesn't require a
    // CodeGenerator so we can destroy it now.
    return true;
}

static const size_t LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 1 << 12;

static bool
CheckFunctionBodiesSequential(ModuleCompiler &m)
{
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);

    for (unsigned i = 0; i < m.numFunctions(); i++) {
        ModuleCompiler::Func &func = m.function(i);

        // Use the scoped LifoAlloc for all temporaries,
        // including the MIRGenerator, MIRGraph, and LIRGraph.
        LifoAllocScope scope(&lifo);

        MIRGenerator *mirGen = CheckFunctionBody(m, func, lifo);
        if (!mirGen)
            return false;

        if (!OptimizeMIR(mirGen))
            return m.fail("Internal compiler failure (probably out of memory)", func.fn());

        LIRGraph *lir = GenerateLIR(mirGen);
        if (!lir)
            return m.fail("Internal compiler failure (probably out of memory)", func.fn());

        if (!GenerateAsmJSCode(m, func, *mirGen, *lir))
            return false;
    }

    return true;
}

#ifdef JS_PARALLEL_COMPILATION
// State of compilation as tracked and updated by the main thread.
struct ParallelGroupState
{
    WorkerThreadState &state;
    Vector<AsmJSParallelTask> &tasks;
    int32_t outstandingJobs; // Good work, jobs!
    uint32_t compiledJobs;

    ParallelGroupState(WorkerThreadState &state, Vector<AsmJSParallelTask> &tasks)
      : state(state), tasks(tasks), outstandingJobs(0), compiledJobs(0)
    { }
};

// Block until a worker-assigned LifoAlloc becomes finished.
static AsmJSParallelTask *
GetFinishedCompilation(ModuleCompiler &m, ParallelGroupState &group)
{
    AutoLockWorkerThreadState lock(m.cx()->runtime);

    while (!group.state.asmJSWorkerFailed()) {
        if (!group.state.asmJSFinishedList.empty()) {
            group.outstandingJobs--;
            return group.state.asmJSFinishedList.popCopy();
        }
        group.state.wait(WorkerThreadState::MAIN);
    }

    return NULL;
}

static bool
GenerateCodeForFinishedJob(ModuleCompiler &m, ParallelGroupState &group, AsmJSParallelTask **outTask)
{
    // Block until a used LifoAlloc becomes available.
    AsmJSParallelTask *task = GetFinishedCompilation(m, group);
    if (!task)
        return false;

    // Perform code generation on the main thread.
    if (!GenerateAsmJSCode(m, m.function(task->funcNum), *task->mir, *task->lir))
        return false;
    group.compiledJobs++;

    // Clear the LifoAlloc for use by another worker.
    TempAllocator &tempAlloc = task->mir->temp();
    tempAlloc.TempAllocator::~TempAllocator();
    task->lifo.releaseAll();

    *outTask = task;
    return true;
}

static inline bool
GetUnusedTask(ParallelGroupState &group, uint32_t funcNum, AsmJSParallelTask **outTask)
{
    // Since functions are dispatched in order, if fewer than |numLifos| functions
    // have been generated, then the |funcNum'th| LifoAlloc must never have been
    // assigned to a worker thread.
    if (funcNum >= group.tasks.length())
        return false;
    *outTask = &group.tasks[funcNum];
    return true;
}

static bool
CheckFunctionBodiesParallelImpl(ModuleCompiler &m, ParallelGroupState &group)
{
    JS_ASSERT(group.state.asmJSWorklist.empty());
    JS_ASSERT(group.state.asmJSFinishedList.empty());
    group.state.resetAsmJSFailureState();

    // Dispatch work for each function.
    for (uint32_t i = 0; i < m.numFunctions(); i++) {
        ModuleCompiler::Func &func = m.function(i);

        // Get exclusive access to an empty LifoAlloc from the thread group's pool.
        AsmJSParallelTask *task = NULL;
        if (!GetUnusedTask(group, i, &task) && !GenerateCodeForFinishedJob(m, group, &task))
            return false;

        // Generate MIR into the LifoAlloc on the main thread.
        MIRGenerator *mir = CheckFunctionBody(m, func, task->lifo);
        if (!mir)
            return false;

        // Perform optimizations and LIR generation on a worker thread.
        task->init(i, mir);
        if (!StartOffThreadAsmJSCompile(m.cx(), task))
            return false;

        group.outstandingJobs++;
    }

    // Block for all outstanding workers to complete.
    while (group.outstandingJobs > 0) {
        AsmJSParallelTask *ignored = NULL;
        if (!GenerateCodeForFinishedJob(m, group, &ignored))
            return false;
    }

    JS_ASSERT(group.outstandingJobs == 0);
    JS_ASSERT(group.compiledJobs == m.numFunctions());
    JS_ASSERT(group.state.asmJSWorklist.empty());
    JS_ASSERT(group.state.asmJSFinishedList.empty());
    JS_ASSERT(!group.state.asmJSWorkerFailed());

    return true;
}

static void
CancelOutstandingJobs(ModuleCompiler &m, ParallelGroupState &group)
{
    // This is failure-handling code, so it's not allowed to fail.
    // The problem is that all memory for compilation is stored in LifoAllocs
    // maintained in the scope of CheckFunctionBodiesParallel() -- so in order
    // for that function to safely return, and thereby remove the LifoAllocs,
    // none of that memory can be in use or reachable by workers.

    JS_ASSERT(group.outstandingJobs >= 0);
    if (!group.outstandingJobs)
        return;

    AutoLockWorkerThreadState lock(m.cx()->runtime);

    // From the compiling tasks, eliminate those waiting for worker assignation.
    group.outstandingJobs -= group.state.asmJSWorklist.length();
    group.state.asmJSWorklist.clear();

    // From the compiling tasks, eliminate those waiting for codegen.
    group.outstandingJobs -= group.state.asmJSFinishedList.length();
    group.state.asmJSFinishedList.clear();

    // Eliminate tasks that failed without adding to the finished list.
    group.outstandingJobs -= group.state.harvestFailedAsmJSJobs();

    // Any remaining tasks are therefore undergoing active compilation.
    JS_ASSERT(group.outstandingJobs >= 0);
    while (group.outstandingJobs > 0) {
        group.state.wait(WorkerThreadState::MAIN);

        group.outstandingJobs -= group.state.harvestFailedAsmJSJobs();
        group.outstandingJobs -= group.state.asmJSFinishedList.length();
        group.state.asmJSFinishedList.clear();
    }

    JS_ASSERT(group.outstandingJobs == 0);
    JS_ASSERT(group.state.asmJSWorklist.empty());
    JS_ASSERT(group.state.asmJSFinishedList.empty());
}

static const size_t LIFO_ALLOC_PARALLEL_CHUNK_SIZE = 1 << 12;

static bool
CheckFunctionBodiesParallel(ModuleCompiler &m)
{
    // Saturate all worker threads plus the main thread.
    WorkerThreadState &state = *m.cx()->runtime->workerThreadState;
    size_t numParallelJobs = state.numThreads + 1;

    // Allocate scoped AsmJSParallelTask objects. Each contains a unique
    // LifoAlloc that provides all necessary memory for compilation.
    Vector<AsmJSParallelTask, 0> tasks(m.cx());
    if (!tasks.initCapacity(numParallelJobs))
        return false;

    for (size_t i = 0; i < numParallelJobs; i++)
        tasks.infallibleAppend(LIFO_ALLOC_PARALLEL_CHUNK_SIZE);

    // With compilation memory in-scope, dispatch worker threads.
    ParallelGroupState group(state, tasks);
    if (!CheckFunctionBodiesParallelImpl(m, group)) {
        CancelOutstandingJobs(m, group);

        // If failure was triggered by a worker thread, report error.
        int32_t maybeFailureIndex = state.maybeGetAsmJSFailedFunctionIndex();
        if (maybeFailureIndex >= 0) {
            ParseNode *fn = m.function(maybeFailureIndex).fn();
            return m.fail("Internal compiler failure (probably out of memory)", fn);
        }

        // Otherwise, the error occurred on the main thread and was already reported.
        return false;
    }
    return true;
}
#endif // JS_PARALLEL_COMPILATION

static RegisterSet AllRegs = RegisterSet(GeneralRegisterSet(Registers::AllMask),
                                         FloatRegisterSet(FloatRegisters::AllMask));
static RegisterSet NonVolatileRegs = RegisterSet(GeneralRegisterSet(Registers::NonVolatileMask),
                                                 FloatRegisterSet(FloatRegisters::NonVolatileMask));

static void
LoadAsmJSActivationIntoRegister(MacroAssembler &masm, Register reg)
{
    masm.movePtr(ImmWord(GetIonContext()->compartment->rt), reg);
    size_t offset = offsetof(JSRuntime, mainThread) +
                    PerThreadData::offsetOfAsmJSActivationStackReadOnly();
    masm.loadPtr(Address(reg, offset), reg);
}

static void
LoadJSContextFromActivation(MacroAssembler &masm, Register activation, Register dest)
{
    masm.loadPtr(Address(activation, AsmJSActivation::offsetOfContext()), dest);
}

static void
AssertStackAlignment(MacroAssembler &masm)
{
    JS_ASSERT((AlignmentAtPrologue + masm.framePushed()) % StackAlignment == 0);
#ifdef DEBUG
    Label ok;
    JS_ASSERT(IsPowerOfTwo(StackAlignment));
    masm.branchTestPtr(Assembler::Zero, StackPointer, Imm32(StackAlignment - 1), &ok);
    masm.breakpoint();
    masm.bind(&ok);
#endif
}

static unsigned
StackArgBytes(const MIRTypeVector &argTypes)
{
    ABIArgIter iter(argTypes);
    while (!iter.done())
        iter++;
    return iter.stackBytesConsumedSoFar();
}

static unsigned
StackDecrementForCall(MacroAssembler &masm, const MIRTypeVector &argTypes, unsigned extraBytes = 0)
{
    // Include extra padding so that, after pushing the arguments and
    // extraBytes, the stack is aligned for a call instruction.
    unsigned argBytes = StackArgBytes(argTypes);
    unsigned alreadyPushed = AlignmentAtPrologue + masm.framePushed();
    return AlignBytes(alreadyPushed + extraBytes + argBytes, StackAlignment) - alreadyPushed;
}

static const unsigned FramePushedAfterSave = NonVolatileRegs.gprs().size() * STACK_SLOT_SIZE +
                                             NonVolatileRegs.fpus().size() * sizeof(double);
#ifndef JS_CPU_ARM
static bool
GenerateEntry(ModuleCompiler &m, const AsmJSModule::ExportedFunction &exportedFunc)
{
    MacroAssembler &masm = m.masm();

    // In constrast to the system ABI, the Ion convention is that all registers
    // are clobbered by calls. Thus, we must save the caller's non-volatile
    // registers.
    //
    // NB: GenerateExits assumes that masm.framePushed() == 0 before
    // PushRegsInMask(NonVolatileRegs).
    masm.setFramePushed(0);
    masm.PushRegsInMask(NonVolatileRegs);

    // Remember the stack pointer in the current AsmJSActivation. This will be
    // used by error exit paths to set the stack pointer back to what it was
    // right after the (C++) caller's non-volatile registers were saved so that
    // they can be restored.
    JS_ASSERT(masm.framePushed() == FramePushedAfterSave);
    Register activation = ABIArgGenerator::NonArgReturnVolatileReg1;
    LoadAsmJSActivationIntoRegister(masm, activation);
    masm.movePtr(StackPointer, Operand(activation, AsmJSActivation::offsetOfErrorRejoinSP()));

#if defined(JS_CPU_X64)
    // Install the heap pointer into the globally-pinned HeapReg. The heap
    // pointer is stored in the global data section and is patched at dynamic
    // link time.
    CodeOffsetLabel label = masm.loadRipRelativeInt64(HeapReg);
    m.addGlobalAccess(AsmJSGlobalAccess(label.offset(), m.module().heapOffset()));
#endif

    Register argv = ABIArgGenerator::NonArgReturnVolatileReg1;
    Register scratch = ABIArgGenerator::NonArgReturnVolatileReg2;
#if defined(JS_CPU_X86)
    masm.movl(Operand(StackPointer, NativeFrameSize + masm.framePushed()), argv);
#elif defined(JS_CPU_X64)
    masm.movq(IntArgReg0, argv);
    masm.Push(argv);
#endif

    // Bump the stack for the call.
    const ModuleCompiler::Func &func = *m.lookupFunction(exportedFunc.name());
    unsigned stackDec = StackDecrementForCall(masm, func.argMIRTypes());
    masm.reserveStack(stackDec);

    for (ABIArgIter iter(func.argMIRTypes()); !iter.done(); iter++) {
        Operand src(argv, iter.index() * sizeof(uint64_t));
        switch (iter->kind()) {
          case ABIArg::GPR:
            masm.load32(src, iter->gpr());
            break;
          case ABIArg::FPU:
            masm.loadDouble(src, iter->fpu());
            break;
          case ABIArg::Stack:
            if (iter.mirType() == MIRType_Int32) {
                masm.load32(src, scratch);
                masm.storePtr(scratch, Operand(StackPointer, iter->offsetFromArgBase()));
            } else {
                JS_ASSERT(iter.mirType() == MIRType_Double);
                masm.loadDouble(src, ScratchFloatReg);
                masm.storeDouble(ScratchFloatReg, Operand(StackPointer, iter->offsetFromArgBase()));
            }
            break;
        }
    }

    AssertStackAlignment(masm);
    masm.call(func.codeLabel());

    masm.freeStack(stackDec);

#if defined(JS_CPU_X86)
    masm.movl(Operand(StackPointer, NativeFrameSize + masm.framePushed()), argv);
#elif defined(JS_CPU_X64)
    masm.Pop(argv);
#endif

    // Store return value in argv[0]
    switch (func.returnType().which()) {
      case RetType::Void:
        break;
      case RetType::Signed:
        masm.storeValue(JSVAL_TYPE_INT32, ReturnReg, Address(argv, 0));
        break;
      case RetType::Double:
        masm.canonicalizeDouble(ReturnFloatReg);
        masm.storeDouble(ReturnFloatReg, Address(argv, 0));
        break;
    }

    // Restore clobbered registers.
    masm.PopRegsInMask(NonVolatileRegs);
    JS_ASSERT(masm.framePushed() == 0);

    masm.move32(Imm32(true), ReturnReg);
    masm.ret();
    return true;
}
#else
static bool
GenerateEntry(ModuleCompiler &m, const AsmJSModule::ExportedFunction &exportedFunc)
{
    const ModuleCompiler::Func &func = *m.lookupFunction(exportedFunc.name());

    MacroAssembler &masm = m.masm();

    // In constrast to the X64 system ABI, the Ion convention is that all
    // registers are clobbered by calls. Thus, we must save the caller's
    // non-volatile registers.
    //
    // NB: GenerateExits assumes that masm.framePushed() == 0 before
    // PushRegsInMask(NonVolatileRegs).
    masm.setFramePushed(0);
    masm.PushRegsInMask(NonVolatileRegs);
    JS_ASSERT(masm.framePushed() == FramePushedAfterSave);
    JS_ASSERT(masm.framePushed() % 8 == 0);

    // Remember the stack pointer in the current AsmJSActivation. This will be
    // used by error exit paths to set the stack pointer back to what it was
    // right after the (C++) caller's non-volatile registers were saved so that
    // they can be restored.

    LoadAsmJSActivationIntoRegister(masm, r9);
    masm.ma_str(StackPointer, Address(r9, AsmJSActivation::offsetOfErrorRejoinSP()));
    //    masm.storeErrorRejoinSp();

    // Move the parameters into non-argument registers since we are about to
    // clobber these registers with the contents of argv.
    Register argv = r9;
    masm.movePtr(IntArgReg1, GlobalReg);  // globalData
    masm.movePtr(IntArgReg0, argv);       // argv

    masm.ma_ldr(Operand(GlobalReg, Imm32(m.module().heapOffset())), HeapReg);
    // Remember argv so that we can load argv[0] after the call.
    JS_ASSERT(masm.framePushed() % 8 == 0);
    masm.Push(argv);
    JS_ASSERT(masm.framePushed() % 8 == 4);

    // Determine how many stack slots we need to hold arguments that don't fit
    // in registers.
    unsigned numStackArgs = 0;
    for (ABIArgIter iter(func.argMIRTypes()); !iter.done(); iter++) {
        if (iter->kind() == ABIArg::Stack)
            numStackArgs++;
    }

    // Before calling, we must ensure sp % 16 == 0. Since (sp % 16) = 8 on
    // entry, we need to push 8 (mod 16) bytes.
    //JS_ASSERT(AlignmentAtPrologue == 8);
    JS_ASSERT(masm.framePushed() % 8 == 4);
    unsigned stackDec = numStackArgs * sizeof(double) + (masm.framePushed() >> 2) % 2 * sizeof(uint32_t);
    masm.reserveStack(stackDec);
    //JS_ASSERT(masm.framePushed() % 8 == 0);
    if(getenv("GDB_BREAK")) {
        masm.breakpoint(js::ion::Assembler::Always);
    }
    // Copy parameters out of argv into the registers/stack-slots specified by
    // the system ABI.
    for (ABIArgIter iter(func.argMIRTypes()); !iter.done(); iter++) {
        unsigned argOffset = iter.index() * sizeof(uint64_t);
        switch (iter->kind()) {
          case ABIArg::GPR:
            masm.ma_ldr(Operand(argv, argOffset), iter->gpr());
            break;
          case ABIArg::FPU:
#if defined(JS_CPU_ARM_HARDFP)
            masm.ma_vldr(Operand(argv, argOffset), iter->fpu());
#else
            // The ABI is expecting a double value in a pair of gpr's.  Figure out which gprs it is,
            // and use them explicityl.
            masm.ma_dataTransferN(IsLoad, 64, true, argv, Imm32(argOffset), Register::FromCode(iter->fpu().code()*2));
#endif
            break;
          case ABIArg::Stack:
            if (iter.mirType() == MIRType_Int32) {
                masm.memMove32(Address(argv, argOffset), Address(StackPointer, iter->offsetFromArgBase()));
            } else {
                masm.memMove64(Address(argv, argOffset), Address(StackPointer, iter->offsetFromArgBase()));
            }
            break;
        }
    }
    masm.ma_vimm(js_NaN, NANReg);
    masm.call(func.codeLabel());

    // Recover argv.
    masm.freeStack(stackDec);
    masm.Pop(argv);

    // Store the result in argv[0].
    switch (func.returnType().which()) {
      case RetType::Void:
        break;
      case RetType::Signed:
        masm.storeValue(JSVAL_TYPE_INT32, ReturnReg, Address(argv, 0));
        break;
      case RetType::Double:
#ifndef JS_CPU_ARM_HARDFP
        masm.ma_vxfer(r0, r1, d0);
#endif
        masm.canonicalizeDouble(ReturnFloatReg);
        masm.storeDouble(ReturnFloatReg, Address(argv, 0));
        break;
    }

    masm.PopRegsInMask(NonVolatileRegs);

    masm.ma_mov(Imm32(true), ReturnReg);
    masm.abiret();
    return true;
}
#endif

static bool
GenerateEntries(ModuleCompiler &m)
{
    for (unsigned i = 0; i < m.module().numExportedFunctions(); i++) {
        m.setEntryOffset(i);
        if (!GenerateEntry(m, m.module().exportedFunction(i)))
            return false;
    }

    return true;
}

static int32_t
InvokeFromAsmJS_Ignore(JSContext *cx, AsmJSModule::ExitDatum *exitDatum, int32_t argc, Value *argv)
{
    RootedValue fval(cx, ObjectValue(*exitDatum->fun));
    RootedValue rval(cx);
    if (!Invoke(cx, UndefinedValue(), fval, argc, argv, rval.address()))
        return false;

    return true;
}

static int32_t
InvokeFromAsmJS_ToInt32(JSContext *cx, AsmJSModule::ExitDatum *exitDatum, int32_t argc, Value *argv)
{
    RootedValue fval(cx, ObjectValue(*exitDatum->fun));
    RootedValue rval(cx);
    if (!Invoke(cx, UndefinedValue(), fval, argc, argv, rval.address()))
        return false;

    int32_t i32;
    if (!ToInt32(cx, rval, &i32))
        return false;
    argv[0] = Int32Value(i32);

    return true;
}

static int32_t
InvokeFromAsmJS_ToNumber(JSContext *cx, AsmJSModule::ExitDatum *exitDatum, int32_t argc, Value *argv)
{
    RootedValue fval(cx, ObjectValue(*exitDatum->fun));
    RootedValue rval(cx);
    if (!Invoke(cx, UndefinedValue(), fval, argc, argv, rval.address()))
        return false;

    double dbl;
    if (!ToNumber(cx, rval, &dbl))
        return false;
    argv[0] = DoubleValue(dbl);

    return true;
}

// See "asm.js FFI calls" comment above.
static void
GenerateFFIExit(ModuleCompiler &m, const ModuleCompiler::ExitDescriptor &exit, unsigned exitIndex,
                Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    m.setExitOffset(exitIndex);
#if defined(JS_CPU_X86) || defined(JS_CPU_X64)
    MIRType typeArray[] = { MIRType_Pointer,   // cx
                            MIRType_Pointer,   // exitDatum
                            MIRType_Int32,     // argc
                            MIRType_Pointer }; // argv
    MIRTypeVector invokeArgTypes(m.cx());
    invokeArgTypes.infallibleAppend(typeArray, ArrayLength(typeArray));

    // Reserve space for a call to InvokeFromAsmJS_* and an array of values
    // passed to this FFI call.
    unsigned arraySize = Max<size_t>(1, exit.argTypes().length()) * sizeof(Value);
    unsigned stackDec = StackDecrementForCall(masm, invokeArgTypes, arraySize);
    masm.setFramePushed(0);
    masm.reserveStack(stackDec);

    // Fill the argument array.
    unsigned offsetToCallerStackArgs = NativeFrameSize + masm.framePushed();
    unsigned offsetToArgv = StackArgBytes(invokeArgTypes);
    for (ABIArgIter i(exit.argTypes()); !i.done(); i++) {
        Address dstAddr = Address(StackPointer, offsetToArgv + i.index() * sizeof(Value));
        switch (i->kind()) {
          case ABIArg::GPR:
            masm.storeValue(JSVAL_TYPE_INT32, i->gpr(), dstAddr);
            break;
          case ABIArg::FPU:
            masm.canonicalizeDouble(i->fpu());
            masm.storeDouble(i->fpu(), dstAddr);
            break;
          case ABIArg::Stack:
            if (i.mirType() == MIRType_Int32) {
                Address src(StackPointer, offsetToCallerStackArgs + i->offsetFromArgBase());
                Register scratch = ABIArgGenerator::NonArgReturnVolatileReg1;
                masm.load32(src, scratch);
                masm.storeValue(JSVAL_TYPE_INT32, scratch, dstAddr);
            } else {
                JS_ASSERT(i.mirType() == MIRType_Double);
                Address src(StackPointer, offsetToCallerStackArgs + i->offsetFromArgBase());
                masm.loadDouble(src, ScratchFloatReg);
                masm.canonicalizeDouble(ScratchFloatReg);
                masm.storeDouble(ScratchFloatReg, dstAddr);
            }
            break;
        }
    }

    // Prepare the arguments for the call to InvokeFromAsmJS_*.
    ABIArgIter i(invokeArgTypes);
    Register scratch = ABIArgGenerator::NonArgReturnVolatileReg1;
    Register activation = ABIArgGenerator::NonArgReturnVolatileReg2;
    LoadAsmJSActivationIntoRegister(masm, activation);

    // argument 0: cx
    if (i->kind() == ABIArg::GPR) {
        LoadJSContextFromActivation(masm, activation, i->gpr());
    } else {
        LoadJSContextFromActivation(masm, activation, scratch);
        masm.movePtr(scratch, Operand(StackPointer, i->offsetFromArgBase()));
    }
    i++;

    // argument 1: exitDatum
    CodeOffsetLabel label;
#if defined(JS_CPU_X64)
    label = masm.leaRipRelative(i->gpr());
#else
    if (i->kind() == ABIArg::GPR) {
        label = masm.movlWithPatch(Imm32(0), i->gpr());
    } else {
        label = masm.movlWithPatch(Imm32(0), scratch);
        masm.movl(scratch, Operand(StackPointer, i->offsetFromArgBase()));
    }
#endif
    unsigned globalDataOffset = m.module().exitIndexToGlobalDataOffset(exitIndex);
    m.addGlobalAccess(AsmJSGlobalAccess(label.offset(), globalDataOffset));
    i++;

    // argument 2: argc
    unsigned argc = exit.argTypes().length();
    if (i->kind() == ABIArg::GPR)
        masm.mov(Imm32(argc), i->gpr());
    else
        masm.move32(Imm32(argc), Operand(StackPointer, i->offsetFromArgBase()));
    i++;

    // argument 3: argv
    Address argv(StackPointer, offsetToArgv);
    if (i->kind() == ABIArg::GPR) {
        masm.computeEffectiveAddress(argv, i->gpr());
    } else {
        masm.computeEffectiveAddress(argv, scratch);
        masm.movePtr(scratch, Operand(StackPointer, i->offsetFromArgBase()));
    }
    i++;
    JS_ASSERT(i.done());

    // Make the call, test whether it succeeded, and extract the return value.
    AssertStackAlignment(masm);
    switch (exit.use().which()) {
      case Use::NoCoercion:
        masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, &InvokeFromAsmJS_Ignore)));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        break;
      case Use::ToInt32:
        masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, &InvokeFromAsmJS_ToInt32)));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.unboxInt32(argv, ReturnReg);
        break;
      case Use::ToNumber:
        masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, &InvokeFromAsmJS_ToNumber)));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.loadDouble(argv, ReturnFloatReg);
        break;
      case Use::AddOrSub:
        JS_NOT_REACHED("Should have been a type error");
    }

    // Note: the caller is IonMonkey code which means there are no non-volatile
    // registers to restore.
    masm.freeStack(stackDec);
    masm.ret();
#else
    const unsigned arrayLength = Max<size_t>(1, exit.argTypes().length());
    const unsigned arraySize = arrayLength * sizeof(Value);
    const unsigned reserveSize = AlignBytes(arraySize, StackAlignment) +
        ShadowStackSpace;
    const unsigned callerArgsOffset = reserveSize + NativeFrameSize + sizeof(int32_t);
    masm.setFramePushed(0);
    masm.Push(lr);
    masm.reserveStack(reserveSize + sizeof(int32_t));

    for (ABIArgIter i(exit.argTypes()); !i.done(); i++) {
        Address dstAddr = Address(StackPointer, ShadowStackSpace + i.index() * sizeof(Value));
        switch (i->kind()) {
          case ABIArg::GPR:
            masm.storeValue(JSVAL_TYPE_INT32, i->gpr(), dstAddr);
            break;
          case ABIArg::FPU: {
#ifndef JS_CPU_ARM_HARDFP
              FloatRegister fr = i->fpu();
              int srcId = fr.code() * 2;
              masm.ma_vxfer(Register::FromCode(srcId), Register::FromCode(srcId+1), fr);
#endif
              masm.canonicalizeDouble(i->fpu());
              masm.storeDouble(i->fpu(), dstAddr);
              break;
          }
          case ABIArg::Stack:
            if (i.mirType() == MIRType_Int32) {
                Address src(StackPointer, callerArgsOffset + i->offsetFromArgBase());
                masm.memIntToValue(src, dstAddr);
            } else {
                JS_ASSERT(i.mirType() == MIRType_Double);
                Address src(StackPointer, callerArgsOffset + i->offsetFromArgBase());
                masm.loadDouble(src, ScratchFloatReg);
                masm.canonicalizeDouble(ScratchFloatReg);
                masm.storeDouble(ScratchFloatReg, dstAddr);
            }
            break;
        }
    }

    // argument 0: cx
    Register activation = IntArgReg3;
    LoadAsmJSActivationIntoRegister(masm, activation);

    LoadJSContextFromActivation(masm, activation, IntArgReg0);

    // argument 1: exitDatum
    masm.lea(Operand(GlobalReg, m.module().exitIndexToGlobalDataOffset(exitIndex)), IntArgReg1);

    // argument 2: argc
    masm.mov(Imm32(exit.argTypes().length()), IntArgReg2);

    // argument 3: argv
    Address argv(StackPointer, ShadowStackSpace);
    masm.lea(Operand(argv), IntArgReg3);

    AssertStackAlignment(masm);
    switch (exit.use().which()) {
      case Use::NoCoercion:
        masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, &InvokeFromAsmJS_Ignore)));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        break;
      case Use::ToInt32:
        masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, &InvokeFromAsmJS_ToInt32)));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.unboxInt32(argv, ReturnReg);
        break;
      case Use::ToNumber:
        masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, &InvokeFromAsmJS_ToNumber)));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
#if defined(JS_CPU_ARM) && !defined(JS_CPU_ARM_HARDFP)
        masm.loadValue(argv, softfpReturnOperand);
#else
        masm.loadDouble(argv, ReturnFloatReg);
#endif
        break;
      case Use::AddOrSub:
        JS_NOT_REACHED("Should have been a type error");
    }

    masm.freeStack(reserveSize + sizeof(int32_t));
    masm.ret();
#endif
}

// The stack-overflow exit is called when the stack limit has definitely been
// exceeded. In this case, we can clobber everything since we are about to pop
// all the frames.
static void
GenerateStackOverflowExit(ModuleCompiler &m, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    masm.bind(&m.stackOverflowLabel());

#if defined(JS_CPU_X86)
    // Ensure that at least one slot is pushed for passing 'cx' below.
    masm.push(Imm32(0));
#endif

    // We know that StackPointer is word-aligned, but nothing past that. Thus,
    // we must align StackPointer dynamically. Don't worry about restoring
    // StackPointer since throwLabel will clobber StackPointer immediately.
    masm.andPtr(Imm32(~(StackAlignment - 1)), StackPointer);
    if (ShadowStackSpace)
        masm.subPtr(Imm32(ShadowStackSpace), StackPointer);

    // Prepare the arguments for the call to js_ReportOverRecursed.
#if defined(JS_CPU_X86)
    LoadAsmJSActivationIntoRegister(masm, eax);
    LoadJSContextFromActivation(masm, eax, eax);
    masm.storePtr(eax, Address(StackPointer, 0));
#elif defined(JS_CPU_X64)
    LoadAsmJSActivationIntoRegister(masm, IntArgReg0);
    LoadJSContextFromActivation(masm, IntArgReg0, IntArgReg0);
#else

    // on ARM, we should always be aligned, just do the context manipulation
    // and make the call.
    LoadAsmJSActivationIntoRegister(masm, IntArgReg0);
    LoadJSContextFromActivation(masm, IntArgReg0, IntArgReg0);

#endif
    void (*pf)(JSContext*) = js_ReportOverRecursed;
    masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, pf)));
    masm.jump(throwLabel);
}

// The operation-callback exit is called from arbitrarily-interrupted asm.js
// code. That means we must first save *all* registers and restore *all*
// registers when we resume. The address to resume to (assuming that
// js_HandleExecutionInterrupt doesn't indicate that the execution should be
// aborted) is stored in AsmJSActivation::resumePC_. Unfortunately, loading
// this requires a scratch register which we don't have after restoring all
// registers. To hack around this, push the resumePC on the stack so that it
// can be popped directly into PC.
static void
GenerateOperationCallbackExit(ModuleCompiler &m, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    masm.bind(&m.operationCallbackLabel());

#ifndef JS_CPU_ARM
    // Be very careful here not to perturb the machine state before saving it
    // to the stack. In particular, add/sub instructions may set conditions in
    // the flags register.
    masm.push(Imm32(0));            // space for resumePC
    masm.pushFlags();               // after this we are safe to use sub
    masm.setFramePushed(0);         // set to zero so we can use masm.framePushed() below
    masm.PushRegsInMask(AllRegs);   // save all GP/FP registers

    Register activation = ABIArgGenerator::NonArgReturnVolatileReg1;
    Register scratch = ABIArgGenerator::NonArgReturnVolatileReg2;

    // Store resumePC into the reserved space.
    LoadAsmJSActivationIntoRegister(masm, activation);
    masm.loadPtr(Address(activation, AsmJSActivation::offsetOfResumePC()), scratch);
    masm.storePtr(scratch, Address(StackPointer, masm.framePushed() + sizeof(void*)));

    // We know that StackPointer is word-aligned, but not necessarily
    // stack-aligned, so we need to align it dynamically.
    masm.mov(StackPointer, ABIArgGenerator::NonVolatileReg);
#if defined(JS_CPU_X86)
    // Ensure that at least one slot is pushed for passing 'cx' below.
    masm.push(Imm32(0));
#endif
    masm.andPtr(Imm32(~(StackAlignment - 1)), StackPointer);
    if (ShadowStackSpace)
        masm.subPtr(Imm32(ShadowStackSpace), StackPointer);

    // argument 0: cx
#if defined(JS_CPU_X86)
    LoadJSContextFromActivation(masm, activation, scratch);
    masm.storePtr(scratch, Address(StackPointer, 0));
#elif defined(JS_CPU_X64)
    LoadJSContextFromActivation(masm, activation, IntArgReg0);
#endif

    JSBool (*pf)(JSContext*) = js_HandleExecutionInterrupt;
    masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, pf)));
    masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);

    // Restore the StackPointer to it's position before the call.
    masm.mov(ABIArgGenerator::NonVolatileReg, StackPointer);

    // Restore the machine state to before the interrupt.
    masm.PopRegsInMask(AllRegs);  // restore all GP/FP registers
    masm.popFlags();              // after this, nothing that sets conditions
    masm.ret();                   // pop resumePC into PC
#else
    masm.setFramePushed(0);         // set to zero so we can use masm.framePushed() below
    masm.PushRegsInMask(RegisterSet(GeneralRegisterSet(Registers::AllMask & ~(1<<Registers::sp)), FloatRegisterSet(uint32_t(0))));   // save all GP registers,excep sp

    // Save both the APSR and FPSCR in non-volatile registers.
    masm.as_mrs(r4);
    masm.as_vmrs(r5);
    // Save the stack pointer in a non-volatile register.
    masm.mov(sp,r6);
    // Align the stack.
    masm.ma_and(Imm32(~7), sp, sp);

    // Store resumePC into the return PC stack slot.
    LoadAsmJSActivationIntoRegister(masm, IntArgReg0);
    masm.loadPtr(Address(IntArgReg0, AsmJSActivation::offsetOfResumePC()), IntArgReg1);
    masm.storePtr(IntArgReg1, Address(r6, 14 * sizeof(uint32_t*)));

    // argument 0: cx
    masm.loadPtr(Address(IntArgReg0, AsmJSActivation::offsetOfContext()), IntArgReg0);

    masm.PushRegsInMask(RegisterSet(GeneralRegisterSet(0), FloatRegisterSet(FloatRegisters::AllMask)));   // save all FP registers
    JSBool (*pf)(JSContext*) = js_HandleExecutionInterrupt;
    masm.call(ImmWord(JS_FUNC_TO_DATA_PTR(void*, pf)));
    masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);

    // Restore the machine state to before the interrupt. this will set the pc!
    masm.PopRegsInMask(RegisterSet(GeneralRegisterSet(0), FloatRegisterSet(FloatRegisters::AllMask)));   // restore all FP registers
    masm.mov(r6,sp);
    masm.as_vmsr(r5);
    masm.as_msr(r4);
    // Restore all GP registers
    masm.startDataTransferM(IsLoad, sp, IA, WriteBack);
    masm.transferReg(r0);
    masm.transferReg(r1);
    masm.transferReg(r2);
    masm.transferReg(r3);
    masm.transferReg(r4);
    masm.transferReg(r5);
    masm.transferReg(r6);
    masm.transferReg(r7);
    masm.transferReg(r8);
    masm.transferReg(r9);
    masm.transferReg(r10);
    masm.transferReg(r11);
    masm.transferReg(r12);
    masm.transferReg(lr);
    masm.finishDataTransfer();
    masm.ret();

#endif

}

// If an exception is thrown, simply pop all frames (since asm.js does not
// contain try/catch). To do this:
//  1. Restore 'sp' to it's value right after the PushRegsInMask in GenerateEntry.
//  2. PopRegsInMask to restore the caller's non-volatile registers.
//  3. Return (to CallAsmJS).
static void
GenerateThrowExit(ModuleCompiler &m, Label *throwLabel)
{
    MacroAssembler &masm = m.masm();
    masm.align(CodeAlignment);
    masm.bind(throwLabel);

    Register activation = ABIArgGenerator::NonArgReturnVolatileReg1;
    LoadAsmJSActivationIntoRegister(masm, activation);

    masm.setFramePushed(FramePushedAfterSave);
    masm.loadPtr(Address(activation, AsmJSActivation::offsetOfErrorRejoinSP()), StackPointer);

    masm.PopRegsInMask(NonVolatileRegs);
    JS_ASSERT(masm.framePushed() == 0);

    masm.mov(Imm32(0), ReturnReg);
    masm.abiret();

}

static bool
GenerateExits(ModuleCompiler &m)
{
    Label throwLabel;

    for (ModuleCompiler::ExitMap::Range r = m.allExits(); !r.empty(); r.popFront()) {
        GenerateFFIExit(m, r.front().key, r.front().value, &throwLabel);
        if (m.masm().oom())
            return false;
    }

    if (m.stackOverflowLabel().used())
        GenerateStackOverflowExit(m, &throwLabel);

    GenerateOperationCallbackExit(m, &throwLabel);

    GenerateThrowExit(m, &throwLabel);
    return true;
}

static bool
CheckModule(JSContext *cx, TokenStream &ts, ParseNode *fn, ScopedJSDeletePtr<AsmJSModule> *module)
{
    ModuleCompiler m(cx, ts);
    if (!m.init())
        return false;

    if (PropertyName *moduleFunctionName = FunctionName(fn)) {
        if (!CheckModuleLevelName(m, moduleFunctionName, fn))
            return false;
        m.initModuleFunctionName(moduleFunctionName);
    }

    ParseNode *stmtIter = NULL;

    if (!CheckFunctionHead(m, fn, &stmtIter))
        return false;

    if (!CheckModuleArguments(m, fn))
        return false;

    if (!SkipUseAsmDirective(m, &stmtIter))
        return false;

    if (!CheckModuleGlobals(m, &stmtIter))
        return false;

    if (!CheckFunctionSignatures(m, &stmtIter))
        return false;

    if (!CheckFuncPtrTables(m, &stmtIter))
        return false;

    if (!CheckModuleExports(m, fn, &stmtIter))
        return false;

    if (stmtIter)
        return m.fail("The top-level export (return) must be the last statement.", stmtIter);

    m.setFirstPassComplete();

#ifdef JS_PARALLEL_COMPILATION
    if (OffThreadCompilationEnabled(cx)) {
        if (!CheckFunctionBodiesParallel(m))
            return false;
    } else {
        if (!CheckFunctionBodiesSequential(m))
            return false;
    }
#else
    if (!CheckFunctionBodiesSequential(m))
        return false;
#endif

    m.setSecondPassComplete();

    if (!GenerateEntries(m))
        return false;

    if (!GenerateExits(m))
        return false;

    return m.finish(module);
}

#endif // defined(JS_ASMJS)

static bool
Warn(JSContext *cx, int code, const char *str = NULL)
{
    return JS_ReportErrorFlagsAndNumber(cx, JSREPORT_WARNING, js_GetErrorMessage,
                                        NULL, code, str);
}

extern bool
EnsureAsmJSSignalHandlersInstalled(JSRuntime *rt);

bool
js::CompileAsmJS(JSContext *cx, TokenStream &ts, ParseNode *fn, const CompileOptions &options,
                 ScriptSource *scriptSource, uint32_t bufStart, uint32_t bufEnd,
                 MutableHandleFunction moduleFun)
{
    if (!JSC::MacroAssembler().supportsFloatingPoint())
        return Warn(cx, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by lack of floating point support");

    if (!cx->hasOption(JSOPTION_ASMJS))
        return Warn(cx, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by javascript.options.experimental_asmjs "
                                                 "in about:config");

    if (cx->compartment->debugMode())
        return Warn(cx, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by debugger");

#ifdef JS_ASMJS
    if (!EnsureAsmJSSignalHandlersInstalled(cx->runtime))
        return Warn(cx, JSMSG_USE_ASM_TYPE_FAIL, "Platform missing signal handler support");

# ifdef JS_PARALLEL_COMPILATION
    if (OffThreadCompilationEnabled(cx)) {
        if (!EnsureParallelCompilationInitialized(cx->runtime))
            return Warn(cx, JSMSG_USE_ASM_TYPE_FAIL, "Failed compilation thread initialization");
    }
# endif

    ScopedJSDeletePtr<AsmJSModule> module;
    if (!CheckModule(cx, ts, fn, &module))
        return !cx->isExceptionPending();

    module->initPostLinkFailureInfo(options, scriptSource, bufStart, bufEnd);

    RootedObject moduleObj(cx, NewAsmJSModuleObject(cx, &module));
    if (!moduleObj)
        return false;

    // Replace the existing interpreted function representing the asm.js module
    // with a native function call to LinkAsmJS.  The native holds, in an
    // extended slot, a reference to the module object, which holds enough
    // information that we can recompile the function normally if linking
    // fails.
    RootedPropertyName name(cx, FunctionName(fn));
    moduleFun.set(NewFunction(cx, NullPtr(), LinkAsmJS, FunctionNumFormals(fn),
                              JSFunction::NATIVE_FUN, NullPtr(), name,
                              JSFunction::ExtendedFinalizeKind));
    if (!moduleFun)
        return false;

    SetAsmJSModuleObject(moduleFun, moduleObj);

    return Warn(cx, JSMSG_USE_ASM_TYPE_OK);
#else
    return Warn(cx, JSMSG_USE_ASM_TYPE_FAIL, "Platform not supported (yet)");
#endif
}

JSBool
js::IsAsmJSCompilationAvailable(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

#ifdef JS_ASMJS
    bool available = JSC::MacroAssembler().supportsFloatingPoint() &&
                     !cx->compartment->debugMode() &&
                     cx->hasOption(JSOPTION_ASMJS);
#else
    bool available = false;
#endif

    args.rval().set(BooleanValue(available));
    return true;
}

