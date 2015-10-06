/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2014 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "asmjs/AsmJSValidate.h"

#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"

#ifdef MOZ_VTUNE
# include "vtune/VTuneWrapper.h"
#endif

#include "jsmath.h"
#include "jsprf.h"
#include "jsutil.h"

#include "asmjs/AsmJSCompile.h"
#include "asmjs/AsmJSGlobals.h"
#include "asmjs/AsmJSLink.h"
#include "asmjs/AsmJSSignalHandlers.h"
#include "builtin/SIMD.h"
#include "frontend/Parser.h"
#include "jit/AtomicOperations.h"
#include "jit/CodeGenerator.h"
#include "jit/CompileWrappers.h"
#include "jit/MIR.h"
#include "jit/MIRGraph.h"
#ifdef JS_ION_PERF
# include "jit/PerfSpewer.h"
#endif
#include "vm/HelperThreads.h"
#include "vm/Interpreter.h"
#include "vm/Time.h"

#include "jsobjinlines.h"

#include "frontend/ParseNode-inl.h"
#include "frontend/Parser-inl.h"
#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::frontend;
using namespace js::jit;
using namespace js::wasm;

using mozilla::AddToHash;
using mozilla::ArrayLength;
using mozilla::CountLeadingZeroes32;
using mozilla::DebugOnly;
using mozilla::HashGeneric;
using mozilla::IsNaN;
using mozilla::IsNegativeZero;
using mozilla::Maybe;
using mozilla::Move;
using mozilla::PositiveInfinity;
using mozilla::UniquePtr;
using JS::GenericNaN;

/*****************************************************************************/
// ParseNode utilities

static inline ParseNode*
NextNode(ParseNode* pn)
{
    return pn->pn_next;
}

static inline ParseNode*
UnaryKid(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_UNARY));
    return pn->pn_kid;
}

static inline ParseNode*
BinaryRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_BINARY));
    return pn->pn_right;
}

static inline ParseNode*
BinaryLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_BINARY));
    return pn->pn_left;
}

static inline ParseNode*
ReturnExpr(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_RETURN));
    return BinaryLeft(pn);
}

static inline ParseNode*
TernaryKid1(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid1;
}

static inline ParseNode*
TernaryKid2(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid2;
}

static inline ParseNode*
TernaryKid3(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_TERNARY));
    return pn->pn_kid3;
}

static inline ParseNode*
ListHead(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_LIST));
    return pn->pn_head;
}

static inline unsigned
ListLength(ParseNode* pn)
{
    MOZ_ASSERT(pn->isArity(PN_LIST));
    return pn->pn_count;
}

static inline ParseNode*
CallCallee(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_CALL));
    return ListHead(pn);
}

static inline unsigned
CallArgListLength(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_CALL));
    MOZ_ASSERT(ListLength(pn) >= 1);
    return ListLength(pn) - 1;
}

static inline ParseNode*
CallArgList(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_CALL));
    return NextNode(ListHead(pn));
}

static inline ParseNode*
VarListHead(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_VAR) || pn->isKind(PNK_CONST));
    return ListHead(pn);
}

static inline ParseNode*
CaseExpr(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_CASE) || pn->isKind(PNK_DEFAULT));
    return BinaryLeft(pn);
}

static inline ParseNode*
CaseBody(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_CASE) || pn->isKind(PNK_DEFAULT));
    return BinaryRight(pn);
}

static inline ParseNode*
BinaryOpLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isBinaryOperation());
    MOZ_ASSERT(pn->isArity(PN_LIST));
    MOZ_ASSERT(pn->pn_count == 2);
    return ListHead(pn);
}

static inline ParseNode*
BinaryOpRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isBinaryOperation());
    MOZ_ASSERT(pn->isArity(PN_LIST));
    MOZ_ASSERT(pn->pn_count == 2);
    return NextNode(ListHead(pn));
}

static inline ParseNode*
BitwiseLeft(ParseNode* pn)
{
    return BinaryOpLeft(pn);
}

static inline ParseNode*
BitwiseRight(ParseNode* pn)
{
    return BinaryOpRight(pn);
}

static inline ParseNode*
MultiplyLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_STAR));
    return BinaryOpLeft(pn);
}

static inline ParseNode*
MultiplyRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_STAR));
    return BinaryOpRight(pn);
}

static inline ParseNode*
AddSubLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_ADD) || pn->isKind(PNK_SUB));
    return BinaryOpLeft(pn);
}

static inline ParseNode*
AddSubRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_ADD) || pn->isKind(PNK_SUB));
    return BinaryOpRight(pn);
}

static inline ParseNode*
DivOrModLeft(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_DIV) || pn->isKind(PNK_MOD));
    return BinaryOpLeft(pn);
}

static inline ParseNode*
DivOrModRight(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_DIV) || pn->isKind(PNK_MOD));
    return BinaryOpRight(pn);
}

static inline ParseNode*
ComparisonLeft(ParseNode* pn)
{
    return BinaryOpLeft(pn);
}

static inline ParseNode*
ComparisonRight(ParseNode* pn)
{
    return BinaryOpRight(pn);
}

static inline ParseNode*
AndOrLeft(ParseNode* pn)
{
    return BinaryOpLeft(pn);
}

static inline ParseNode*
AndOrRight(ParseNode* pn)
{
    return BinaryOpRight(pn);
}

static inline ParseNode*
RelationalLeft(ParseNode* pn)
{
    return BinaryOpLeft(pn);
}

static inline ParseNode*
RelationalRight(ParseNode* pn)
{
    return BinaryOpRight(pn);
}

static inline bool
IsExpressionStatement(ParseNode* pn)
{
    return pn->isKind(PNK_SEMI);
}

static inline ParseNode*
ExpressionStatementExpr(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_SEMI));
    return UnaryKid(pn);
}

static inline PropertyName*
LoopControlMaybeLabel(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_BREAK) || pn->isKind(PNK_CONTINUE));
    MOZ_ASSERT(pn->isArity(PN_NULLARY));
    return pn->as<LoopControlStatement>().label();
}

static inline PropertyName*
LabeledStatementLabel(ParseNode* pn)
{
    return pn->as<LabeledStatement>().label();
}

static inline ParseNode*
LabeledStatementStatement(ParseNode* pn)
{
    return pn->as<LabeledStatement>().statement();
}

static double
NumberNodeValue(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_NUMBER));
    return pn->pn_dval;
}

static bool
NumberNodeHasFrac(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_NUMBER));
    return pn->pn_u.number.decimalPoint == HasDecimal;
}

static ParseNode*
DotBase(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_DOT));
    MOZ_ASSERT(pn->isArity(PN_NAME));
    return pn->expr();
}

static PropertyName*
DotMember(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_DOT));
    MOZ_ASSERT(pn->isArity(PN_NAME));
    return pn->pn_atom->asPropertyName();
}

static ParseNode*
ElemBase(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_ELEM));
    return BinaryLeft(pn);
}

static ParseNode*
ElemIndex(ParseNode* pn)
{
    MOZ_ASSERT(pn->isKind(PNK_ELEM));
    return BinaryRight(pn);
}

static inline JSFunction*
FunctionObject(ParseNode* fn)
{
    MOZ_ASSERT(fn->isKind(PNK_FUNCTION));
    MOZ_ASSERT(fn->isArity(PN_CODE));
    return fn->pn_funbox->function();
}

static inline PropertyName*
FunctionName(ParseNode* fn)
{
    if (JSAtom* atom = FunctionObject(fn)->atom())
        return atom->asPropertyName();
    return nullptr;
}

static inline ParseNode*
FunctionStatementList(ParseNode* fn)
{
    MOZ_ASSERT(fn->pn_body->isKind(PNK_ARGSBODY));
    ParseNode* last = fn->pn_body->last();
    MOZ_ASSERT(last->isKind(PNK_STATEMENTLIST));
    return last;
}

static inline bool
IsNormalObjectField(ExclusiveContext* cx, ParseNode* pn)
{
    return pn->isKind(PNK_COLON) &&
           pn->getOp() == JSOP_INITPROP &&
           BinaryLeft(pn)->isKind(PNK_OBJECT_PROPERTY_NAME);
}

static inline PropertyName*
ObjectNormalFieldName(ExclusiveContext* cx, ParseNode* pn)
{
    MOZ_ASSERT(IsNormalObjectField(cx, pn));
    MOZ_ASSERT(BinaryLeft(pn)->isKind(PNK_OBJECT_PROPERTY_NAME));
    return BinaryLeft(pn)->pn_atom->asPropertyName();
}

static inline ParseNode*
ObjectNormalFieldInitializer(ExclusiveContext* cx, ParseNode* pn)
{
    MOZ_ASSERT(IsNormalObjectField(cx, pn));
    return BinaryRight(pn);
}

static inline bool
IsDefinition(ParseNode* pn)
{
    return pn->isKind(PNK_NAME) && pn->isDefn();
}

static inline ParseNode*
MaybeDefinitionInitializer(ParseNode* pn)
{
    MOZ_ASSERT(IsDefinition(pn));
    return pn->expr();
}

static inline bool
IsUseOfName(ParseNode* pn, PropertyName* name)
{
    return pn->isKind(PNK_NAME) && pn->name() == name;
}

static inline bool
IsIgnoredDirectiveName(ExclusiveContext* cx, JSAtom* atom)
{
    return atom != cx->names().useStrict;
}

static inline bool
IsIgnoredDirective(ExclusiveContext* cx, ParseNode* pn)
{
    return pn->isKind(PNK_SEMI) &&
           UnaryKid(pn) &&
           UnaryKid(pn)->isKind(PNK_STRING) &&
           IsIgnoredDirectiveName(cx, UnaryKid(pn)->pn_atom);
}

static inline bool
IsEmptyStatement(ParseNode* pn)
{
    return pn->isKind(PNK_SEMI) && !UnaryKid(pn);
}

static inline ParseNode*
SkipEmptyStatements(ParseNode* pn)
{
    while (pn && IsEmptyStatement(pn))
        pn = pn->pn_next;
    return pn;
}

static inline ParseNode*
NextNonEmptyStatement(ParseNode* pn)
{
    return SkipEmptyStatements(pn->pn_next);
}

static bool
GetToken(AsmJSParser& parser, TokenKind* tkp)
{
    TokenStream& ts = parser.tokenStream;
    TokenKind tk;
    while (true) {
        if (!ts.getToken(&tk, TokenStream::Operand))
            return false;
        if (tk != TOK_SEMI)
            break;
    }
    *tkp = tk;
    return true;
}

static bool
PeekToken(AsmJSParser& parser, TokenKind* tkp)
{
    TokenStream& ts = parser.tokenStream;
    TokenKind tk;
    while (true) {
        if (!ts.peekToken(&tk, TokenStream::Operand))
            return false;
        if (tk != TOK_SEMI)
            break;
        ts.consumeKnownToken(TOK_SEMI, TokenStream::Operand);
    }
    *tkp = tk;
    return true;
}

static bool
ParseVarOrConstStatement(AsmJSParser& parser, ParseNode** var)
{
    TokenKind tk;
    if (!PeekToken(parser, &tk))
        return false;
    if (tk != TOK_VAR && tk != TOK_CONST) {
        *var = nullptr;
        return true;
    }

    *var = parser.statement(YieldIsName);
    if (!*var)
        return false;

    MOZ_ASSERT((*var)->isKind(PNK_VAR) || (*var)->isKind(PNK_CONST));
    return true;
}

/*****************************************************************************/

// Implements <: (subtype) operator when the rhs is a VarType
static inline bool
operator<=(Type lhs, VarType rhs)
{
    switch (rhs.which()) {
      case VarType::Int:       return lhs.isInt();
      case VarType::Double:    return lhs.isDouble();
      case VarType::Float:     return lhs.isFloat();
      case VarType::Int32x4:   return lhs.isInt32x4();
      case VarType::Float32x4: return lhs.isFloat32x4();
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Unexpected rhs type");
}

/*****************************************************************************/

static
bool operator==(const Signature& lhs, const Signature& rhs)
{
    if (lhs.retType() != rhs.retType())
        return false;
    if (lhs.args().length() != rhs.args().length())
        return false;
    for (unsigned i = 0; i < lhs.args().length(); i++) {
        if (lhs.arg(i) != rhs.arg(i))
            return false;
    }
    return true;
}

static inline
bool operator!=(const Signature& lhs, const Signature& rhs)
{
    return !(lhs == rhs);
}

/*****************************************************************************/
// Typed array utilities

static Type
TypedArrayLoadType(Scalar::Type viewType)
{
    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Int16:
      case Scalar::Int32:
      case Scalar::Uint8:
      case Scalar::Uint16:
      case Scalar::Uint32:
        return Type::Intish;
      case Scalar::Float32:
        return Type::MaybeFloat;
      case Scalar::Float64:
        return Type::MaybeDouble;
      default:;
    }
    MOZ_CRASH("Unexpected array type");
}

namespace {

// The ModuleValidator encapsulates the entire validation of an asm.js module.
// Its lifetime goes from the validation of the top components of an asm.js
// module (all the globals), the emission of bytecode for all the functions in
// the module and the validation of function's pointer tables. It also finishes
// the compilation of all the module's stubs.
//
// Rooting note: ModuleValidator is a stack class that contains unrooted
// PropertyName (JSAtom) pointers.  This is safe because it cannot be
// constructed without a TokenStream reference.  TokenStream is itself a stack
// class that cannot be constructed without an AutoKeepAtoms being live on the
// stack, which prevents collection of atoms.
//
// ModuleValidator is marked as rooted in the rooting analysis.  Don't add
// non-JSAtom pointers, or this will break!
class MOZ_STACK_CLASS ModuleValidator
{
  public:
    class Func
    {
        LifoSignature* sig_;
        PropertyName* name_;
        uint32_t firstUseOffset_;
        uint32_t funcIndex_;
        uint32_t srcBegin_;
        uint32_t srcEnd_;
        uint32_t compileTime_;
        bool defined_;

      public:
        Func(PropertyName* name, uint32_t firstUseOffset, LifoSignature* sig, uint32_t funcIndex)
          : sig_(sig), name_(name), firstUseOffset_(firstUseOffset), funcIndex_(funcIndex),
            srcBegin_(0), srcEnd_(0), compileTime_(0), defined_(false)
        {}

        PropertyName* name() const { return name_; }
        uint32_t firstUseOffset() const { return firstUseOffset_; }
        bool defined() const { return defined_; }
        uint32_t funcIndex() const { return funcIndex_; }

        void define(ParseNode* fn) {
            MOZ_ASSERT(!defined_);
            defined_ = true;
            srcBegin_ = fn->pn_pos.begin;
            srcEnd_ = fn->pn_pos.end;
        }

        uint32_t srcBegin() const { MOZ_ASSERT(defined_); return srcBegin_; }
        uint32_t srcEnd() const { MOZ_ASSERT(defined_); return srcEnd_; }
        LifoSignature& sig() { return *sig_; }
        const LifoSignature& sig() const { return *sig_; }
        uint32_t compileTime() const { return compileTime_; }
        void accumulateCompileTime(uint32_t ms) { compileTime_ += ms; }
    };

    typedef Vector<const Func*, 0, LifoAllocPolicy<Fallible>> ConstFuncVector;
    typedef Vector<Func*> FuncVector;

    class FuncPtrTable
    {
        LifoSignature *sig_;
        ConstFuncVector elems_;
        PropertyName* name_;
        uint32_t firstUseOffset_;
        uint32_t mask_;
        uint32_t globalDataOffset_;
        uint32_t tableIndex_;

        FuncPtrTable(FuncPtrTable&& rhs) = delete;

      public:
        FuncPtrTable(LifoAlloc& lifo, PropertyName* name, uint32_t firstUseOffset,
                     LifoSignature* sig, uint32_t mask, uint32_t gdo, uint32_t tableIndex)
          : sig_(sig), elems_(lifo), name_(name), firstUseOffset_(firstUseOffset), mask_(mask),
            globalDataOffset_(gdo), tableIndex_(tableIndex)
        {}

        LifoSignature& sig() { return *sig_; }
        const LifoSignature& sig() const { return *sig_; }
        PropertyName* name() const { return name_; }
        uint32_t firstUseOffset() const { return firstUseOffset_; }
        unsigned mask() const { return mask_; }
        unsigned globalDataOffset() const { return globalDataOffset_; }
        unsigned tableIndex() const { return tableIndex_; }

        bool initialized() const { return !elems_.empty(); }
        void initElems(ConstFuncVector&& elems) { elems_ = Move(elems); MOZ_ASSERT(initialized()); }
        unsigned numElems() const { MOZ_ASSERT(initialized()); return elems_.length(); }
        const Func& elem(unsigned i) const { return *elems_[i]; }
    };

    typedef Vector<FuncPtrTable*> FuncPtrTableVector;

    class Global
    {
      public:
        enum Which {
            Variable,
            ConstantLiteral,
            ConstantImport,
            Function,
            FuncPtrTable,
            FFI,
            ArrayView,
            ArrayViewCtor,
            MathBuiltinFunction,
            AtomicsBuiltinFunction,
            SimdCtor,
            SimdOperation,
            ByteLength,
            ChangeHeap
        };

      private:
        Which which_;
        union {
            struct {
                Type::Which type_;
                uint32_t index_;
                AsmJSNumLit literalValue_;
            } varOrConst;
            uint32_t funcIndex_;
            uint32_t funcPtrTableIndex_;
            uint32_t ffiIndex_;
            struct {
                Scalar::Type viewType_;
                bool isSharedView_;
            } viewInfo;
            AsmJSMathBuiltinFunction mathBuiltinFunc_;
            AsmJSAtomicsBuiltinFunction atomicsBuiltinFunc_;
            AsmJSSimdType simdCtorType_;
            struct {
                AsmJSSimdType type_;
                AsmJSSimdOperation which_;
            } simdOp;
            struct {
                uint32_t srcBegin_;
                uint32_t srcEnd_;
            } changeHeap;
        } u;

        friend class ModuleValidator;
        friend class js::LifoAlloc;

        explicit Global(Which which) : which_(which) {}

      public:
        Which which() const {
            return which_;
        }
        Type varOrConstType() const {
            MOZ_ASSERT(which_ == Variable || which_ == ConstantLiteral || which_ == ConstantImport);
            return u.varOrConst.type_;
        }
        uint32_t varOrConstIndex() const {
            MOZ_ASSERT(which_ == Variable || which_ == ConstantImport);
            return u.varOrConst.index_;
        }
        bool isConst() const {
            return which_ == ConstantLiteral || which_ == ConstantImport;
        }
        AsmJSNumLit constLiteralValue() const {
            MOZ_ASSERT(which_ == ConstantLiteral);
            return u.varOrConst.literalValue_;
        }
        uint32_t funcIndex() const {
            MOZ_ASSERT(which_ == Function);
            return u.funcIndex_;
        }
        uint32_t funcPtrTableIndex() const {
            MOZ_ASSERT(which_ == FuncPtrTable);
            return u.funcPtrTableIndex_;
        }
        unsigned ffiIndex() const {
            MOZ_ASSERT(which_ == FFI);
            return u.ffiIndex_;
        }
        bool isAnyArrayView() const {
            return which_ == ArrayView || which_ == ArrayViewCtor;
        }
        Scalar::Type viewType() const {
            MOZ_ASSERT(isAnyArrayView());
            return u.viewInfo.viewType_;
        }
        bool viewIsSharedView() const {
            MOZ_ASSERT(isAnyArrayView());
            return u.viewInfo.isSharedView_;
        }
        bool isMathFunction() const {
            return which_ == MathBuiltinFunction;
        }
        AsmJSMathBuiltinFunction mathBuiltinFunction() const {
            MOZ_ASSERT(which_ == MathBuiltinFunction);
            return u.mathBuiltinFunc_;
        }
        bool isAtomicsFunction() const {
            return which_ == AtomicsBuiltinFunction;
        }
        AsmJSAtomicsBuiltinFunction atomicsBuiltinFunction() const {
            MOZ_ASSERT(which_ == AtomicsBuiltinFunction);
            return u.atomicsBuiltinFunc_;
        }
        bool isSimdCtor() const {
            return which_ == SimdCtor;
        }
        AsmJSSimdType simdCtorType() const {
            MOZ_ASSERT(which_ == SimdCtor);
            return u.simdCtorType_;
        }
        bool isSimdOperation() const {
            return which_ == SimdOperation;
        }
        AsmJSSimdOperation simdOperation() const {
            MOZ_ASSERT(which_ == SimdOperation);
            return u.simdOp.which_;
        }
        AsmJSSimdType simdOperationType() const {
            MOZ_ASSERT(which_ == SimdOperation);
            return u.simdOp.type_;
        }
        uint32_t changeHeapSrcBegin() const {
            MOZ_ASSERT(which_ == ChangeHeap);
            return u.changeHeap.srcBegin_;
        }
        uint32_t changeHeapSrcEnd() const {
            MOZ_ASSERT(which_ == ChangeHeap);
            return u.changeHeap.srcEnd_;
        }
    };

    struct MathBuiltin
    {
        enum Kind { Function, Constant };
        Kind kind;

        union {
            double cst;
            AsmJSMathBuiltinFunction func;
        } u;

        MathBuiltin() : kind(Kind(-1)) {}
        explicit MathBuiltin(double cst) : kind(Constant) {
            u.cst = cst;
        }
        explicit MathBuiltin(AsmJSMathBuiltinFunction func) : kind(Function) {
            u.func = func;
        }
    };

    struct ArrayView
    {
        ArrayView(PropertyName* name, Scalar::Type type)
          : name(name), type(type)
        {}

        PropertyName* name;
        Scalar::Type type;
    };

    class ExitDescriptor
    {
        PropertyName* name_;
        const LifoSignature* sig_;

      public:
        ExitDescriptor(PropertyName* name, const LifoSignature* sig)
          : name_(name), sig_(sig)
        {}

        PropertyName* name() const {
            return name_;
        }
        const LifoSignature& sig() const {
            return *sig_;
        }

        // ExitDescriptor is a HashPolicy:
        struct Lookup {
            PropertyName* name_;
            const Signature* sig_;
            Lookup(PropertyName* name, const Signature* sig)
              : name_(name), sig_(sig)
            {}
        };
        static HashNumber hash(const Lookup& d) {
            HashNumber hn = HashGeneric(d.name_, d.sig_->retType().which());
            const VarTypeVector& args = d.sig_->args();
            for (unsigned i = 0; i < args.length(); i++)
                hn = AddToHash(hn, args[i].which());
            return hn;
        }
        static bool match(const ExitDescriptor& lhs, const Lookup& rhs) {
            return lhs.name_ == rhs.name_ && *lhs.sig_ == *rhs.sig_;
        }
    };

  private:
    typedef HashMap<PropertyName*, Global*> GlobalMap;
    typedef HashMap<PropertyName*, MathBuiltin> MathNameMap;
    typedef HashMap<PropertyName*, AsmJSAtomicsBuiltinFunction> AtomicsNameMap;
    typedef HashMap<PropertyName*, AsmJSSimdOperation> SimdOperationNameMap;
    typedef Vector<ArrayView> ArrayViewVector;

  public:
    typedef HashMap<ExitDescriptor, unsigned, ExitDescriptor> ExitMap;

  private:
    ExclusiveContext*                       cx_;
    AsmJSParser&                            parser_;

    ScopedJSDeletePtr<AsmJSModule>          module_;
    LifoAlloc                               moduleLifo_;

    FuncVector                              functions_;
    FuncPtrTableVector                      funcPtrTables_;
    GlobalMap                               globals_;
    ArrayViewVector                         arrayViews_;
    ExitMap                                 exits_;

    MathNameMap                             standardLibraryMathNames_;
    AtomicsNameMap                          standardLibraryAtomicsNames_;
    SimdOperationNameMap                    standardLibrarySimdOpNames_;

    ParseNode*                              moduleFunctionNode_;
    PropertyName*                           moduleFunctionName_;

    UniquePtr<char[], JS::FreePolicy>       errorString_;
    uint32_t                                errorOffset_;
    bool                                    errorOverRecursed_;

    bool                                    canValidateChangeHeap_;
    bool                                    hasChangeHeap_;
    bool                                    supportsSimd_;

    ScopedJSDeletePtr<ModuleCompileResults> compileResults_;
    DebugOnly<bool>                         finishedFunctionBodies_;

  public:
    ModuleValidator(ExclusiveContext* cx, AsmJSParser& parser)
      : cx_(cx),
        parser_(parser),
        moduleLifo_(LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
        functions_(cx),
        funcPtrTables_(cx),
        globals_(cx),
        arrayViews_(cx),
        exits_(cx),
        standardLibraryMathNames_(cx),
        standardLibraryAtomicsNames_(cx),
        standardLibrarySimdOpNames_(cx),
        moduleFunctionNode_(parser.pc->maybeFunction),
        moduleFunctionName_(nullptr),
        errorString_(nullptr),
        errorOffset_(UINT32_MAX),
        errorOverRecursed_(false),
        canValidateChangeHeap_(false),
        hasChangeHeap_(false),
        supportsSimd_(cx->jitSupportsSimd()),
        compileResults_(nullptr),
        finishedFunctionBodies_(false)
    {
        MOZ_ASSERT(moduleFunctionNode_->pn_funbox == parser.pc->sc->asFunctionBox());
    }

    ~ModuleValidator() {
        if (errorString_) {
            MOZ_ASSERT(errorOffset_ != UINT32_MAX);
            tokenStream().reportAsmJSError(errorOffset_,
                                           JSMSG_USE_ASM_TYPE_FAIL,
                                           errorString_.get());
        }
        if (errorOverRecursed_)
            ReportOverRecursed(cx_);
    }

  private:

    // Helpers
    bool addStandardLibraryMathName(const char* name, AsmJSMathBuiltinFunction func) {
        JSAtom* atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        MathBuiltin builtin(func);
        return standardLibraryMathNames_.putNew(atom->asPropertyName(), builtin);
    }
    bool addStandardLibraryMathName(const char* name, double cst) {
        JSAtom* atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        MathBuiltin builtin(cst);
        return standardLibraryMathNames_.putNew(atom->asPropertyName(), builtin);
    }
    bool addStandardLibraryAtomicsName(const char* name, AsmJSAtomicsBuiltinFunction func) {
        JSAtom* atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        return standardLibraryAtomicsNames_.putNew(atom->asPropertyName(), func);
    }
    bool addStandardLibrarySimdOpName(const char* name, AsmJSSimdOperation op) {
        JSAtom* atom = Atomize(cx_, name, strlen(name));
        if (!atom)
            return false;
        return standardLibrarySimdOpNames_.putNew(atom->asPropertyName(), op);
    }

  public:

    bool init() {
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
            !addStandardLibraryMathName("imul", AsmJSMathBuiltin_imul) ||
            !addStandardLibraryMathName("clz32", AsmJSMathBuiltin_clz32) ||
            !addStandardLibraryMathName("fround", AsmJSMathBuiltin_fround) ||
            !addStandardLibraryMathName("min", AsmJSMathBuiltin_min) ||
            !addStandardLibraryMathName("max", AsmJSMathBuiltin_max) ||
            !addStandardLibraryMathName("E", M_E) ||
            !addStandardLibraryMathName("LN10", M_LN10) ||
            !addStandardLibraryMathName("LN2", M_LN2) ||
            !addStandardLibraryMathName("LOG2E", M_LOG2E) ||
            !addStandardLibraryMathName("LOG10E", M_LOG10E) ||
            !addStandardLibraryMathName("PI", M_PI) ||
            !addStandardLibraryMathName("SQRT1_2", M_SQRT1_2) ||
            !addStandardLibraryMathName("SQRT2", M_SQRT2))
        {
            return false;
        }

        if (!standardLibraryAtomicsNames_.init() ||
            !addStandardLibraryAtomicsName("compareExchange", AsmJSAtomicsBuiltin_compareExchange) ||
            !addStandardLibraryAtomicsName("exchange", AsmJSAtomicsBuiltin_exchange) ||
            !addStandardLibraryAtomicsName("load", AsmJSAtomicsBuiltin_load) ||
            !addStandardLibraryAtomicsName("store", AsmJSAtomicsBuiltin_store) ||
            !addStandardLibraryAtomicsName("fence", AsmJSAtomicsBuiltin_fence) ||
            !addStandardLibraryAtomicsName("add", AsmJSAtomicsBuiltin_add) ||
            !addStandardLibraryAtomicsName("sub", AsmJSAtomicsBuiltin_sub) ||
            !addStandardLibraryAtomicsName("and", AsmJSAtomicsBuiltin_and) ||
            !addStandardLibraryAtomicsName("or", AsmJSAtomicsBuiltin_or) ||
            !addStandardLibraryAtomicsName("xor", AsmJSAtomicsBuiltin_xor) ||
            !addStandardLibraryAtomicsName("isLockFree", AsmJSAtomicsBuiltin_isLockFree))
        {
            return false;
        }

#define ADDSTDLIBSIMDOPNAME(op) || !addStandardLibrarySimdOpName(#op, AsmJSSimdOperation_##op)
        if (!standardLibrarySimdOpNames_.init()
            FORALL_SIMD_OP(ADDSTDLIBSIMDOPNAME))
        {
            return false;
        }
#undef ADDSTDLIBSIMDOPNAME

        uint32_t srcStart = parser_.pc->maybeFunction->pn_body->pn_pos.begin;
        uint32_t srcBodyStart = tokenStream().currentToken().pos.end;

        // "use strict" should be added to the source if we are in an implicit
        // strict context, see also comment above addUseStrict in
        // js::FunctionToString.
        bool strict = parser_.pc->sc->strict() && !parser_.pc->sc->hasExplicitUseStrict();
        module_ = cx_->new_<AsmJSModule>(parser_.ss, srcStart, srcBodyStart, strict,
                                         cx_->canUseSignalHandlers());
        return !!module_;
    }

    // Mutable interface.
    void initModuleFunctionName(PropertyName* name) { moduleFunctionName_ = name; }
    void initGlobalArgumentName(PropertyName* n)    { module_->initGlobalArgumentName(n); }
    void initImportArgumentName(PropertyName* n)    { module_->initImportArgumentName(n); }
    void initBufferArgumentName(PropertyName* n)    { module_->initBufferArgumentName(n); }

    bool addGlobalVarInit(PropertyName* varName, const AsmJSNumLit& lit, bool isConst) {
        // The type of a const is the exact type of the literal (since its value
        // cannot change) which is more precise than the corresponding vartype.
        Type type = isConst ? Type::Of(lit) : VarType::Of(lit).toType();
        uint32_t globalIndex;
        if (!module_->addGlobalVarInit(lit, &globalIndex))
            return false;
        Global::Which which = isConst ? Global::ConstantLiteral : Global::Variable;
        Global* global = moduleLifo_.new_<Global>(which);
        if (!global)
            return false;
        global->u.varOrConst.index_ = globalIndex;
        global->u.varOrConst.type_ = type.which();
        if (isConst)
            global->u.varOrConst.literalValue_ = lit;
        return globals_.putNew(varName, global);
    }
    bool addGlobalVarImport(PropertyName* varName, PropertyName* fieldName, AsmJSCoercion coercion,
                            bool isConst)
    {
        uint32_t globalIndex;
        if (!module_->addGlobalVarImport(fieldName, coercion, &globalIndex))
            return false;
        Global::Which which = isConst ? Global::ConstantImport : Global::Variable;
        Global* global = moduleLifo_.new_<Global>(which);
        if (!global)
            return false;
        global->u.varOrConst.index_ = globalIndex;
        global->u.varOrConst.type_ = VarType(coercion).toType().which();
        return globals_.putNew(varName, global);
    }
    bool addArrayView(PropertyName* varName, Scalar::Type vt, PropertyName* maybeField,
                      bool isSharedView)
    {
        if (!arrayViews_.append(ArrayView(varName, vt)))
            return false;
        Global* global = moduleLifo_.new_<Global>(Global::ArrayView);
        if (!global)
            return false;
        if (!module_->addArrayView(vt, maybeField, isSharedView))
            return false;
        global->u.viewInfo.viewType_ = vt;
        global->u.viewInfo.isSharedView_ = isSharedView;
        return globals_.putNew(varName, global);
    }
    bool addMathBuiltinFunction(PropertyName* varName, AsmJSMathBuiltinFunction func,
                                PropertyName* fieldName)
    {
        if (!module_->addMathBuiltinFunction(func, fieldName))
            return false;
        Global* global = moduleLifo_.new_<Global>(Global::MathBuiltinFunction);
        if (!global)
            return false;
        global->u.mathBuiltinFunc_ = func;
        return globals_.putNew(varName, global);
    }
  private:
    bool addGlobalDoubleConstant(PropertyName* varName, double constant) {
        Global* global = moduleLifo_.new_<Global>(Global::ConstantLiteral);
        if (!global)
            return false;
        global->u.varOrConst.type_ = Type::Double;
        global->u.varOrConst.literalValue_ = AsmJSNumLit::Create(AsmJSNumLit::Double,
                                                                 DoubleValue(constant));
        return globals_.putNew(varName, global);
    }
  public:
    bool addMathBuiltinConstant(PropertyName* varName, double constant, PropertyName* fieldName) {
        if (!module_->addMathBuiltinConstant(constant, fieldName))
            return false;
        return addGlobalDoubleConstant(varName, constant);
    }
    bool addGlobalConstant(PropertyName* varName, double constant, PropertyName* fieldName) {
        if (!module_->addGlobalConstant(constant, fieldName))
            return false;
        return addGlobalDoubleConstant(varName, constant);
    }
    bool addAtomicsBuiltinFunction(PropertyName* varName, AsmJSAtomicsBuiltinFunction func,
                                   PropertyName* fieldName)
    {
        if (!module_->addAtomicsBuiltinFunction(func, fieldName))
            return false;
        Global* global = moduleLifo_.new_<Global>(Global::AtomicsBuiltinFunction);
        if (!global)
            return false;
        global->u.atomicsBuiltinFunc_ = func;
        return globals_.putNew(varName, global);
    }
    bool addSimdCtor(PropertyName* varName, AsmJSSimdType type, PropertyName* fieldName) {
        if (!module_->addSimdCtor(type, fieldName))
            return false;
        Global* global = moduleLifo_.new_<Global>(Global::SimdCtor);
        if (!global)
            return false;
        global->u.simdCtorType_ = type;
        return globals_.putNew(varName, global);
    }
    bool addSimdOperation(PropertyName* varName, AsmJSSimdType type, AsmJSSimdOperation op,
                          PropertyName* typeVarName, PropertyName* opName)
    {
        if (!module_->addSimdOperation(type, op, opName))
            return false;
        Global* global = moduleLifo_.new_<Global>(Global::SimdOperation);
        if (!global)
            return false;
        global->u.simdOp.type_ = type;
        global->u.simdOp.which_ = op;
        return globals_.putNew(varName, global);
    }
    bool addByteLength(PropertyName* name) {
        canValidateChangeHeap_ = true;
        if (!module_->addByteLength())
            return false;
        Global* global = moduleLifo_.new_<Global>(Global::ByteLength);
        return global && globals_.putNew(name, global);
    }
    bool addChangeHeap(PropertyName* name, ParseNode* fn, uint32_t mask, uint32_t min, uint32_t max) {
        hasChangeHeap_ = true;
        module_->addChangeHeap(mask, min, max);
        Global* global = moduleLifo_.new_<Global>(Global::ChangeHeap);
        if (!global)
            return false;
        global->u.changeHeap.srcBegin_ = fn->pn_pos.begin;
        global->u.changeHeap.srcEnd_ = fn->pn_pos.end;
        return globals_.putNew(name, global);
    }
    bool addArrayViewCtor(PropertyName* varName, Scalar::Type vt, PropertyName* fieldName, bool isSharedView) {
        Global* global = moduleLifo_.new_<Global>(Global::ArrayViewCtor);
        if (!global)
            return false;
        if (!module_->addArrayViewCtor(vt, fieldName, isSharedView))
            return false;
        global->u.viewInfo.viewType_ = vt;
        global->u.viewInfo.isSharedView_ = isSharedView;
        return globals_.putNew(varName, global);
    }
    bool addFFI(PropertyName* varName, PropertyName* field) {
        Global* global = moduleLifo_.new_<Global>(Global::FFI);
        if (!global)
            return false;
        uint32_t index;
        if (!module_->addFFI(field, &index))
            return false;
        global->u.ffiIndex_ = index;
        return globals_.putNew(varName, global);
    }
    bool addExportedFunction(const Func& func, PropertyName* maybeFieldName) {
        AsmJSModule::ArgCoercionVector argCoercions;
        const VarTypeVector& args = func.sig().args();
        if (!argCoercions.resize(args.length()))
            return false;
        for (unsigned i = 0; i < args.length(); i++)
            argCoercions[i] = args[i].toCoercion();
        AsmJSModule::ReturnType retType = func.sig().retType().toModuleReturnType();
        return module_->addExportedFunction(func.name(), func.srcBegin(), func.srcEnd(),
                                            maybeFieldName, Move(argCoercions), retType);
    }
    bool addExportedChangeHeap(PropertyName* name, const Global& g, PropertyName* maybeFieldName) {
        return module_->addExportedChangeHeap(name, g.changeHeapSrcBegin(), g.changeHeapSrcEnd(),
                                              maybeFieldName);
    }
    bool addFunction(PropertyName* name, uint32_t firstUseOffset, Signature&& sig, Func** func) {
        uint32_t funcIndex = numFunctions();
        Global* global = moduleLifo_.new_<Global>(Global::Function);
        if (!global)
            return false;
        global->u.funcIndex_ = funcIndex;
        if (!globals_.putNew(name, global))
            return false;
        LifoSignature* lifoSig = LifoSignature::new_(moduleLifo_, Move(sig));
        if (!lifoSig)
            return false;
        *func = moduleLifo_.new_<Func>(name, firstUseOffset, lifoSig, funcIndex);
        if (!*func)
            return false;
        return functions_.append(*func);
    }
    bool addFuncPtrTable(PropertyName* name, uint32_t offset, Signature&& sig, uint32_t mask,
                         ModuleValidator::FuncPtrTable** table)
    {
        uint32_t tableIndex = numFuncPtrTables();
        Global* global = moduleLifo_.new_<Global>(Global::FuncPtrTable);
        if (!global)
            return false;
        global->u.funcPtrTableIndex_ = tableIndex;
        if (!globals_.putNew(name, global))
            return false;
        uint32_t globalDataOffset;
        if (!module_->addFuncPtrTable(/* numElems = */ mask + 1, &globalDataOffset))
            return false;
        LifoSignature* lifoSig = LifoSignature::new_(moduleLifo_, Move(sig));
        if (!lifoSig)
            return false;
        *table = moduleLifo_.new_<ModuleValidator::FuncPtrTable>(moduleLifo_, name, offset, lifoSig, mask,
                                                                 globalDataOffset, tableIndex);
        return *table && funcPtrTables_.append(*table);
    }
    bool addExit(unsigned ffiIndex, PropertyName* name, Signature&& sig, unsigned* exitIndex,
                 const LifoSignature** lifoSig)
    {
        ExitDescriptor::Lookup lookup(name, &sig);
        ExitMap::AddPtr p = exits_.lookupForAdd(lookup);
        if (p) {
            *lifoSig = &p->key().sig();
            *exitIndex = p->value();
            return true;
        }
        LifoSignature* signature = LifoSignature::new_(moduleLifo_, Move(sig));
        if (!signature)
            return false;
        *lifoSig = signature;
        if (!module_->addExit(ffiIndex, exitIndex))
            return false;
        ExitDescriptor key(name, *lifoSig);
        return exits_.add(p, Move(key), *exitIndex);
    }

    bool tryOnceToValidateChangeHeap() {
        bool ret = canValidateChangeHeap_;
        canValidateChangeHeap_ = false;
        return ret;
    }
    bool hasChangeHeap() const {
        return hasChangeHeap_;
    }
    bool tryRequireHeapLengthToBeAtLeast(uint32_t len) {
        return module_->tryRequireHeapLengthToBeAtLeast(len);
    }
    uint32_t minHeapLength() const {
        return module_->minHeapLength();
    }

    // Error handling.
    bool hasAlreadyFailed() const {
        return !!errorString_;
    }

    bool failOffset(uint32_t offset, const char* str) {
        MOZ_ASSERT(!hasAlreadyFailed());
        MOZ_ASSERT(errorOffset_ == UINT32_MAX);
        MOZ_ASSERT(str);
        errorOffset_ = offset;
        errorString_ = DuplicateString(cx_, str);
        return false;
    }

    bool fail(ParseNode* pn, const char* str) {
        return failOffset(pn->pn_pos.begin, str);
    }

    bool failfVAOffset(uint32_t offset, const char* fmt, va_list ap) {
        MOZ_ASSERT(!hasAlreadyFailed());
        MOZ_ASSERT(errorOffset_ == UINT32_MAX);
        MOZ_ASSERT(fmt);
        errorOffset_ = offset;
        errorString_.reset(JS_vsmprintf(fmt, ap));
        return false;
    }

    bool failfOffset(uint32_t offset, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        failfVAOffset(offset, fmt, ap);
        va_end(ap);
        return false;
    }

    bool failf(ParseNode* pn, const char* fmt, ...) {
        va_list ap;
        va_start(ap, fmt);
        failfVAOffset(pn->pn_pos.begin, fmt, ap);
        va_end(ap);
        return false;
    }

    bool failNameOffset(uint32_t offset, const char* fmt, PropertyName* name) {
        // This function is invoked without the caller properly rooting its locals.
        gc::AutoSuppressGC suppress(cx_);
        JSAutoByteString bytes;
        if (AtomToPrintableString(cx_, name, &bytes))
            failfOffset(offset, fmt, bytes.ptr());
        return false;
    }

    bool failName(ParseNode* pn, const char* fmt, PropertyName* name) {
        return failNameOffset(pn->pn_pos.begin, fmt, name);
    }

    bool failOverRecursed() {
        errorOverRecursed_ = true;
        return false;
    }

    // Read-only interface
    ExclusiveContext* cx() const             { return cx_; }
    ParseNode* moduleFunctionNode() const    { return moduleFunctionNode_; }
    PropertyName* moduleFunctionName() const { return moduleFunctionName_; }
    AsmJSModule& module()                    { return *module_.get(); }
    const AsmJSModule& module() const        { return *module_.get(); }
    AsmJSParser& parser() const              { return parser_; }
    TokenStream& tokenStream() const         { return parser_.tokenStream; }
    bool supportsSimd() const                { return supportsSimd_; }
    LifoAlloc& lifo()                        { return moduleLifo_; }

    unsigned numArrayViews() const {
        return arrayViews_.length();
    }
    const ArrayView& arrayView(unsigned i) const {
        return arrayViews_[i];
    }
    unsigned numFunctions() const {
        return functions_.length();
    }
    Func& function(unsigned i) const {
        return *functions_[i];
    }
    unsigned numFuncPtrTables() const {
        return funcPtrTables_.length();
    }
    FuncPtrTable& funcPtrTable(unsigned i) const {
        return *funcPtrTables_[i];
    }

    const Global* lookupGlobal(PropertyName* name) const {
        if (GlobalMap::Ptr p = globals_.lookup(name))
            return p->value();
        return nullptr;
    }

    Func* lookupFunction(PropertyName* name) {
        if (GlobalMap::Ptr p = globals_.lookup(name)) {
            Global* value = p->value();
            if (value->which() == Global::Function)
                return functions_[value->funcIndex()];
        }
        return nullptr;
    }

    bool lookupStandardLibraryMathName(PropertyName* name, MathBuiltin* mathBuiltin) const {
        if (MathNameMap::Ptr p = standardLibraryMathNames_.lookup(name)) {
            *mathBuiltin = p->value();
            return true;
        }
        return false;
    }
    bool lookupStandardLibraryAtomicsName(PropertyName* name, AsmJSAtomicsBuiltinFunction* atomicsBuiltin) const {
        if (AtomicsNameMap::Ptr p = standardLibraryAtomicsNames_.lookup(name)) {
            *atomicsBuiltin = p->value();
            return true;
        }
        return false;
    }
    bool lookupStandardSimdOpName(PropertyName* name, AsmJSSimdOperation* op) const {
        if (SimdOperationNameMap::Ptr p = standardLibrarySimdOpNames_.lookup(name)) {
            *op = p->value();
            return true;
        }
        return false;
    }

    // End-of-compilation utils
    MacroAssembler& masm()           { return compileResults_->masm(); }
    Label& stackOverflowLabel()      { return compileResults_->stackOverflowLabel(); }
    Label& asyncInterruptLabel()     { return compileResults_->asyncInterruptLabel(); }
    Label& syncInterruptLabel()      { return compileResults_->syncInterruptLabel(); }
    Label& onDetachedLabel()         { return compileResults_->onDetachedLabel(); }
    Label& onOutOfBoundsLabel()      { return compileResults_->onOutOfBoundsLabel(); }
    Label& onConversionErrorLabel()  { return compileResults_->onConversionErrorLabel(); }
    Label* functionEntry(unsigned i) { return compileResults_->functionEntry(i); }
    ExitMap::Range allExits() const  { return exits_.all(); }

    bool finishGeneratingEntry(unsigned exportIndex, Label* begin) {
        MOZ_ASSERT(finishedFunctionBodies_);
        module_->exportedFunction(exportIndex).initCodeOffset(begin->offset());
        uint32_t end = masm().currentOffset();
        return module_->addCodeRange(AsmJSModule::CodeRange::Entry, begin->offset(), end);
    }
    bool finishGeneratingInterpExit(unsigned exitIndex, Label* begin, Label* profilingReturn) {
        MOZ_ASSERT(finishedFunctionBodies_);
        uint32_t beg = begin->offset();
        module_->exit(exitIndex).initInterpOffset(beg);
        uint32_t pret = profilingReturn->offset();
        uint32_t end = masm().currentOffset();
        return module_->addCodeRange(AsmJSModule::CodeRange::SlowFFI, beg, pret, end);
    }
    bool finishGeneratingJitExit(unsigned exitIndex, Label* begin, Label* profilingReturn) {
        MOZ_ASSERT(finishedFunctionBodies_);
        uint32_t beg = begin->offset();
        module_->exit(exitIndex).initJitOffset(beg);
        uint32_t pret = profilingReturn->offset();
        uint32_t end = masm().currentOffset();
        return module_->addCodeRange(AsmJSModule::CodeRange::JitFFI, beg, pret, end);
    }
    bool finishGeneratingInlineStub(Label* begin) {
        MOZ_ASSERT(finishedFunctionBodies_);
        uint32_t end = masm().currentOffset();
        return module_->addCodeRange(AsmJSModule::CodeRange::Inline, begin->offset(), end);
    }
    bool finishGeneratingInterrupt(Label* begin, Label* profilingReturn) {
        MOZ_ASSERT(finishedFunctionBodies_);
        uint32_t beg = begin->offset();
        uint32_t pret = profilingReturn->offset();
        uint32_t end = masm().currentOffset();
        return module_->addCodeRange(AsmJSModule::CodeRange::Interrupt, beg, pret, end);
    }
    bool finishGeneratingBuiltinThunk(AsmJSExit::BuiltinKind builtin, Label* begin, Label* pret) {
        MOZ_ASSERT(finishedFunctionBodies_);
        uint32_t end = masm().currentOffset();
        return module_->addBuiltinThunkCodeRange(builtin, begin->offset(), pret->offset(), end);
    }

    bool finish(ScopedJSDeletePtr<AsmJSModule>* module) {
        masm().finish();
        if (masm().oom())
            return false;

        if (!module_->finish(cx_, tokenStream(), masm(), asyncInterruptLabel(), onOutOfBoundsLabel()))
            return false;

        // Finally, convert all the function-pointer table elements into
        // RelativeLinks that will be patched by AsmJSModule::staticallyLink.
        for (unsigned tableIndex = 0; tableIndex < numFuncPtrTables(); tableIndex++) {
            ModuleValidator::FuncPtrTable& table = funcPtrTable(tableIndex);
            unsigned tableBaseOffset = module_->offsetOfGlobalData() + table.globalDataOffset();
            for (unsigned elemIndex = 0; elemIndex < table.numElems(); elemIndex++) {
                AsmJSModule::RelativeLink link(AsmJSModule::RelativeLink::RawPointer);
                link.patchAtOffset = tableBaseOffset + elemIndex * sizeof(uint8_t*);
                Label* entry = functionEntry(table.elem(elemIndex).funcIndex());
                link.targetOffset = masm().actualOffset(entry->offset());
                if (!module_->addRelativeLink(link))
                    return false;
            }
        }

        *module = module_.forget();
        return true;
    }

    void startFunctionBodies() {
        module_->startFunctionBodies();
    }
    bool finishFunctionBodies(ScopedJSDeletePtr<ModuleCompileResults>* compileResults) {
        // Take ownership of compilation results
        compileResults_ = compileResults->forget();

        // These must be done before the module is done with function bodies.
        for (size_t i = 0; i < compileResults_->numFunctionCounts(); ++i) {
            if (!module().addFunctionCounts(compileResults_->functionCount(i)))
                return false;
        }
#if defined(MOZ_VTUNE) || defined(JS_ION_PERF)
        for (size_t i = 0; i < compileResults_->numProfiledFunctions(); ++i) {
            if (!module().addProfiledFunction(Move(compileResults_->profiledFunction(i))))
                return false;
        }
#endif // defined(MOZ_VTUNE) || defined(JS_ION_PERF)

        // Hand in code ranges, script counts and perf profiling data to the AsmJSModule
        for (size_t i = 0; i < compileResults_->numCodeRanges(); ++i) {
            AsmJSModule::FunctionCodeRange& codeRange = compileResults_->codeRange(i);
            if (!module().addFunctionCodeRange(codeRange.name(), Move(codeRange)))
                return false;
        }

        // When an interrupt is triggered, all function code is mprotected and,
        // for sanity, stub code (particularly the interrupt stub) is not.
        // Protection works at page granularity, so we need to ensure that no
        // stub code gets into the function code pages.
        // TODO; this is no longer true and could be removed, see also
        // bug 1200609.
        MOZ_ASSERT(!finishedFunctionBodies_);
        masm().haltingAlign(AsmJSPageSize);
        module_->finishFunctionBodies(masm().currentOffset());
        finishedFunctionBodies_ = true;

        return true;
    }

    void buildCompilationTimeReport(JS::AsmJSCacheResult cacheResult, ScopedJSFreePtr<char>* out) {
#ifndef JS_MORE_DETERMINISTIC
        ScopedJSFreePtr<char> slowFuns;
        int64_t usecAfter = PRMJ_Now();
        int msTotal = (usecAfter - compileResults_->usecBefore()) / PRMJ_USEC_PER_MSEC;
        ModuleCompileResults::SlowFunctionVector& slowFunctions = compileResults_->slowFunctions();
        if (!slowFunctions.empty()) {
            slowFuns.reset(JS_smprintf("; %d functions compiled slowly: ", slowFunctions.length()));
            if (!slowFuns)
                return;
            for (unsigned i = 0; i < slowFunctions.length(); i++) {
                ModuleCompileResults::SlowFunction& func = slowFunctions[i];
                JSAutoByteString name;
                if (!AtomToPrintableString(cx_, func.name, &name))
                    return;
                slowFuns.reset(JS_smprintf("%s%s:%u:%u (%ums)%s", slowFuns.get(),
                                           name.ptr(), func.line, func.column, func.ms,
                                           i+1 < slowFunctions.length() ? ", " : ""));
                if (!slowFuns)
                    return;
            }
        }
        const char* cacheString = "";
        switch (cacheResult) {
          case JS::AsmJSCache_Success:
            cacheString = "stored in cache";
            break;
          case JS::AsmJSCache_ModuleTooSmall:
            cacheString = "not stored in cache (too small to benefit)";
            break;
          case JS::AsmJSCache_SynchronousScript:
            cacheString = "unable to cache asm.js in synchronous scripts; try loading "
                          "asm.js via <script async> or createElement('script')";
            break;
          case JS::AsmJSCache_QuotaExceeded:
            cacheString = "not enough temporary storage quota to store in cache";
            break;
          case JS::AsmJSCache_StorageInitFailure:
            cacheString = "storage initialization failed (consider filing a bug)";
            break;
          case JS::AsmJSCache_Disabled_Internal:
            cacheString = "caching disabled by internal configuration (consider filing a bug)";
            break;
          case JS::AsmJSCache_Disabled_ShellFlags:
            cacheString = "caching disabled by missing command-line arguments";
            break;
          case JS::AsmJSCache_Disabled_JitInspector:
            cacheString = "caching disabled by active JIT inspector";
            break;
          case JS::AsmJSCache_InternalError:
            cacheString = "unable to store in cache due to internal error (consider filing a bug)";
            break;
          case JS::AsmJSCache_LIMIT:
            MOZ_CRASH("bad AsmJSCacheResult");
            break;
        }
        out->reset(JS_smprintf("total compilation time %dms; %s%s",
                               msTotal, cacheString, slowFuns ? slowFuns.get() : ""));
#endif
    }

    ModuleCompileInputs compileInputs() const {
        CompileCompartment* compartment = CompileCompartment::get(cx()->compartment());
        return ModuleCompileInputs(compartment,
                                   compartment->runtime(),
                                   module().usesSignalHandlersForOOB());
    }
};

} // namespace

/*****************************************************************************/
// Numeric literal utilities

static bool
IsNumericNonFloatLiteral(ParseNode* pn)
{
    // Note: '-' is never rolled into the number; numbers are always positive
    // and negations must be applied manually.
    return pn->isKind(PNK_NUMBER) ||
           (pn->isKind(PNK_NEG) && UnaryKid(pn)->isKind(PNK_NUMBER));
}

static bool
IsCallToGlobal(ModuleValidator& m, ParseNode* pn, const ModuleValidator::Global** global)
{
    if (!pn->isKind(PNK_CALL))
        return false;

    ParseNode* callee = CallCallee(pn);
    if (!callee->isKind(PNK_NAME))
        return false;

    *global = m.lookupGlobal(callee->name());
    return !!*global;
}

static bool
IsCoercionCall(ModuleValidator& m, ParseNode* pn, AsmJSCoercion* coercion, ParseNode** coercedExpr)
{
    const ModuleValidator::Global* global;
    if (!IsCallToGlobal(m, pn, &global))
        return false;

    if (CallArgListLength(pn) != 1)
        return false;

    if (coercedExpr)
        *coercedExpr = CallArgList(pn);

    if (global->isMathFunction() && global->mathBuiltinFunction() == AsmJSMathBuiltin_fround) {
        *coercion = AsmJS_FRound;
        return true;
    }

    if (global->isSimdOperation() && global->simdOperation() == AsmJSSimdOperation_check) {
        switch (global->simdOperationType()) {
          case AsmJSSimdType_int32x4:
            *coercion = AsmJS_ToInt32x4;
            return true;
          case AsmJSSimdType_float32x4:
            *coercion = AsmJS_ToFloat32x4;
            return true;
        }
    }

    return false;
}

static bool
IsFloatLiteral(ModuleValidator& m, ParseNode* pn)
{
    ParseNode* coercedExpr;
    AsmJSCoercion coercion;
    if (!IsCoercionCall(m, pn, &coercion, &coercedExpr))
        return false;
    // Don't fold into || to avoid clang/memcheck bug (bug 1077031).
    if (coercion != AsmJS_FRound)
        return false;
    return IsNumericNonFloatLiteral(coercedExpr);
}

static unsigned
SimdTypeToLength(AsmJSSimdType type)
{
    switch (type) {
      case AsmJSSimdType_float32x4:
      case AsmJSSimdType_int32x4:
        return 4;
    }
    MOZ_CRASH("unexpected SIMD type");
}

static bool
IsSimdTuple(ModuleValidator& m, ParseNode* pn, AsmJSSimdType* type)
{
    const ModuleValidator::Global* global;
    if (!IsCallToGlobal(m, pn, &global))
        return false;

    if (!global->isSimdCtor())
        return false;

    if (CallArgListLength(pn) != SimdTypeToLength(global->simdCtorType()))
        return false;

    *type = global->simdCtorType();
    return true;
}

static bool
IsNumericLiteral(ModuleValidator& m, ParseNode* pn);

static AsmJSNumLit
ExtractNumericLiteral(ModuleValidator& m, ParseNode* pn);

static inline bool
IsLiteralInt(ModuleValidator& m, ParseNode* pn, uint32_t* u32);

static bool
IsSimdLiteral(ModuleValidator& m, ParseNode* pn)
{
    AsmJSSimdType type;
    if (!IsSimdTuple(m, pn, &type))
        return false;

    ParseNode* arg = CallArgList(pn);
    unsigned length = SimdTypeToLength(type);
    for (unsigned i = 0; i < length; i++) {
        if (!IsNumericLiteral(m, arg))
            return false;

        uint32_t _;
        switch (type) {
          case AsmJSSimdType_int32x4:
            if (!IsLiteralInt(m, arg, &_))
                return false;
          case AsmJSSimdType_float32x4:
            if (!IsNumericNonFloatLiteral(arg))
                return false;
        }

        arg = NextNode(arg);
    }

    MOZ_ASSERT(arg == nullptr);
    return true;
}

static bool
IsNumericLiteral(ModuleValidator& m, ParseNode* pn)
{
    return IsNumericNonFloatLiteral(pn) ||
           IsFloatLiteral(m, pn) ||
           IsSimdLiteral(m, pn);
}

// The JS grammar treats -42 as -(42) (i.e., with separate grammar
// productions) for the unary - and literal 42). However, the asm.js spec
// recognizes -42 (modulo parens, so -(42) and -((42))) as a single literal
// so fold the two potential parse nodes into a single double value.
static double
ExtractNumericNonFloatValue(ParseNode* pn, ParseNode** out = nullptr)
{
    MOZ_ASSERT(IsNumericNonFloatLiteral(pn));

    if (pn->isKind(PNK_NEG)) {
        pn = UnaryKid(pn);
        if (out)
            *out = pn;
        return -NumberNodeValue(pn);
    }

    return NumberNodeValue(pn);
}

static AsmJSNumLit
ExtractSimdValue(ModuleValidator& m, ParseNode* pn)
{
    MOZ_ASSERT(IsSimdLiteral(m, pn));

    AsmJSSimdType type = AsmJSSimdType_int32x4;
    JS_ALWAYS_TRUE(IsSimdTuple(m, pn, &type));

    ParseNode* arg = CallArgList(pn);
    switch (type) {
      case AsmJSSimdType_int32x4: {
        MOZ_ASSERT(SimdTypeToLength(type) == 4);
        int32_t val[4];
        for (size_t i = 0; i < 4; i++, arg = NextNode(arg)) {
            uint32_t u32;
            JS_ALWAYS_TRUE(IsLiteralInt(m, arg, &u32));
            val[i] = int32_t(u32);
        }
        MOZ_ASSERT(arg== nullptr);
        return AsmJSNumLit::Create(AsmJSNumLit::Int32x4, SimdConstant::CreateX4(val));
      }
      case AsmJSSimdType_float32x4: {
        MOZ_ASSERT(SimdTypeToLength(type) == 4);
        float val[4];
        for (size_t i = 0; i < 4; i++, arg = NextNode(arg))
            val[i] = float(ExtractNumericNonFloatValue(arg));
        MOZ_ASSERT(arg == nullptr);
        return AsmJSNumLit::Create(AsmJSNumLit::Float32x4, SimdConstant::CreateX4(val));
      }
    }

    MOZ_CRASH("Unexpected SIMD type.");
}

static AsmJSNumLit
ExtractNumericLiteral(ModuleValidator& m, ParseNode* pn)
{
    MOZ_ASSERT(IsNumericLiteral(m, pn));

    if (pn->isKind(PNK_CALL)) {
        // Float literals are explicitly coerced and thus the coerced literal may be
        // any valid (non-float) numeric literal.
        if (CallArgListLength(pn) == 1) {
            pn = CallArgList(pn);
            double d = ExtractNumericNonFloatValue(pn);
            return AsmJSNumLit::Create(AsmJSNumLit::Float, DoubleValue(d));
        }

        MOZ_ASSERT(CallArgListLength(pn) == 4);
        return ExtractSimdValue(m, pn);
    }

    double d = ExtractNumericNonFloatValue(pn, &pn);

    // The asm.js spec syntactically distinguishes any literal containing a
    // decimal point or the literal -0 as having double type.
    if (NumberNodeHasFrac(pn) || IsNegativeZero(d))
        return AsmJSNumLit::Create(AsmJSNumLit::Double, DoubleValue(d));

    // The syntactic checks above rule out these double values.
    MOZ_ASSERT(!IsNegativeZero(d));
    MOZ_ASSERT(!IsNaN(d));

    // Although doubles can only *precisely* represent 53-bit integers, they
    // can *imprecisely* represent integers much bigger than an int64_t.
    // Furthermore, d may be inf or -inf. In both cases, casting to an int64_t
    // is undefined, so test against the integer bounds using doubles.
    if (d < double(INT32_MIN) || d > double(UINT32_MAX))
        return AsmJSNumLit::Create(AsmJSNumLit::OutOfRangeInt, UndefinedValue());

    // With the above syntactic and range limitations, d is definitely an
    // integer in the range [INT32_MIN, UINT32_MAX] range.
    int64_t i64 = int64_t(d);
    if (i64 >= 0) {
        if (i64 <= INT32_MAX)
            return AsmJSNumLit::Create(AsmJSNumLit::Fixnum, Int32Value(i64));
        MOZ_ASSERT(i64 <= UINT32_MAX);
        return AsmJSNumLit::Create(AsmJSNumLit::BigUnsigned, Int32Value(uint32_t(i64)));
    }
    MOZ_ASSERT(i64 >= INT32_MIN);
    return AsmJSNumLit::Create(AsmJSNumLit::NegativeInt, Int32Value(i64));
}

static inline bool
IsLiteralInt(AsmJSNumLit lit, uint32_t* u32)
{
    switch (lit.which()) {
      case AsmJSNumLit::Fixnum:
      case AsmJSNumLit::BigUnsigned:
      case AsmJSNumLit::NegativeInt:
        *u32 = uint32_t(lit.toInt32());
        return true;
      case AsmJSNumLit::Double:
      case AsmJSNumLit::Float:
      case AsmJSNumLit::OutOfRangeInt:
      case AsmJSNumLit::Int32x4:
      case AsmJSNumLit::Float32x4:
        return false;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Bad literal type");
}

static inline bool
IsLiteralInt(ModuleValidator& m, ParseNode* pn, uint32_t* u32)
{
    return IsNumericLiteral(m, pn) &&
           IsLiteralInt(ExtractNumericLiteral(m, pn), u32);
}

/*****************************************************************************/

namespace {

// Encapsulates the building of an asm bytecode function from an asm.js function
// source code, packing the asm.js code into the asm bytecode form that can
// be decoded and compiled with a FunctionCompiler.
class FunctionValidator
{
  public:
    struct Local
    {
        VarType type;
        unsigned slot;
        Local(VarType t, unsigned slot) : type(t), slot(slot) {}
    };

  private:
    typedef HashMap<PropertyName*, Local> LocalMap;
    typedef HashMap<PropertyName*, uint32_t> LabelMap;

    ModuleValidator &      m_;
    ParseNode *            fn_;

    AsmFunction &          func_;

    LocalMap               locals_;
    LabelMap               labels_;

    unsigned               heapExpressionDepth_;

    bool                   hasAlreadyReturned_;

  public:
    FunctionValidator(ModuleValidator& m, AsmFunction& func, ParseNode* fn)
      : m_(m),
        fn_(fn),
        func_(func),
        locals_(m.cx()),
        labels_(m.cx()),
        heapExpressionDepth_(0),
        hasAlreadyReturned_(false)
    {}

    ModuleValidator& m() const        { return m_; }
    const AsmJSModule& module() const { return m_.module(); }
    ExclusiveContext* cx() const      { return m_.cx(); }
    ParseNode* fn() const             { return fn_; }

    bool init()
    {
        return locals_.init() &&
               labels_.init();
    }

    bool fail(ParseNode* pn, const char* str)
    {
        return m_.fail(pn, str);
    }

    bool failf(ParseNode* pn, const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        m_.failfVAOffset(pn->pn_pos.begin, fmt, ap);
        va_end(ap);
        return false;
    }

    bool failName(ParseNode* pn, const char* fmt, PropertyName* name)
    {
        return m_.failName(pn, fmt, name);
    }

    /***************************************************** Local scope setup */

    bool addFormal(ParseNode* pn, PropertyName* name, VarType type)
    {
        LocalMap::AddPtr p = locals_.lookupForAdd(name);
        if (p)
            return failName(pn, "duplicate local name '%s' not allowed", name);
        return locals_.add(p, name, Local(type, locals_.count()));
    }

    bool addVariable(ParseNode* pn, PropertyName* name, const AsmJSNumLit& init)
    {
        LocalMap::AddPtr p = locals_.lookupForAdd(name);
        if (p)
            return failName(pn, "duplicate local name '%s' not allowed", name);
        if (!locals_.add(p, name, Local(VarType::Of(init), locals_.count())))
            return false;
        return func_.addVariable(init);
    }

    /*************************************************************************/

    void enterHeapExpression() {
        heapExpressionDepth_++;
    }
    void leaveHeapExpression() {
        MOZ_ASSERT(heapExpressionDepth_ > 0);
        heapExpressionDepth_--;
    }
    bool canCall() const {
        return heapExpressionDepth_ == 0 || !m_.hasChangeHeap();
    }

    /****************************** For consistency of returns in a function */

    bool hasAlreadyReturned() const {
        return hasAlreadyReturned_;
    }

    RetType returnedType() const {
        return func_.returnedType();
    }

    void setReturnedType(RetType retType) {
        func_.setReturnedType(retType);
        hasAlreadyReturned_ = true;
    }

    /**************************************************************** Labels */

    uint32_t lookupLabel(PropertyName* label) const {
        if (auto p = labels_.lookup(label))
            return p->value();
        return -1;
    }

    bool addLabel(PropertyName* label, uint32_t* id) {
        *id = labels_.count();
        return labels_.putNew(label, *id);
    }

    void removeLabel(PropertyName* label) {
        auto p = labels_.lookup(label);
        MOZ_ASSERT(!!p);
        labels_.remove(p);
    }

    /*************************************************** Read-only interface */

    const Local* lookupLocal(PropertyName* name) const {
        if (auto p = locals_.lookup(name))
            return &p->value();
        return nullptr;
    }

    const ModuleValidator::Global* lookupGlobal(PropertyName* name) const {
        if (locals_.has(name))
            return nullptr;
        return m_.lookupGlobal(name);
    }

    size_t numLocals() const { return locals_.count(); }

    /************************************************* Packing interface */

    bool startedPacking() const {
        return func_.size() != 0;
    }

    template<class T>
    size_t writeOp(T op) {
        static_assert(sizeof(T) == sizeof(uint8_t), "opcodes must be uint8");
        return func_.writeU8(uint8_t(op));
    }

    void writeDebugCheckPoint() {
#ifdef DEBUG
        writeOp(Stmt::DebugCheckPoint);
#endif
    }

    size_t writeU8(uint8_t u) {
        return func_.writeU8(u);
    }
    size_t writeU32(uint32_t u) {
        return func_.writeU32(u);
    }
    size_t writeI32(int32_t u) {
        return func_.writeI32(u);
    }

    void writeInt32Lit(int32_t i) {
        writeOp(I32::Literal);
        func_.writeI32(i);
    }

    void writeLit(AsmJSNumLit lit) {
        switch (lit.which()) {
          case AsmJSNumLit::Fixnum:
          case AsmJSNumLit::NegativeInt:
          case AsmJSNumLit::BigUnsigned:
            writeInt32Lit(lit.toInt32());
            return;
          case AsmJSNumLit::Float:
            writeOp(F32::Literal);
            func_.writeF32(lit.toFloat());
            return;
          case AsmJSNumLit::Double:
            writeOp(F64::Literal);
            func_.writeF64(lit.toDouble());
            return;
          case AsmJSNumLit::Int32x4:
            writeOp(I32X4::Literal);
            func_.writeI32X4(lit.simdValue().asInt32x4());
            return;
          case AsmJSNumLit::Float32x4:
            writeOp(F32X4::Literal);
            func_.writeF32X4(lit.simdValue().asFloat32x4());
            return;
          case AsmJSNumLit::OutOfRangeInt:
            break;
        }
        MOZ_CRASH("unexpected literal type");
    }

    template<class T>
    void patchOp(size_t pos, T stmt) {
        static_assert(sizeof(T) == sizeof(uint8_t), "opcodes must be uint8");
        func_.patchU8(pos, uint8_t(stmt));
    }
    void patchU8(size_t pos, uint8_t u8) {
        func_.patchU8(pos, u8);
    }
    template<class T>
    void patch32(size_t pos, T val) {
        static_assert(sizeof(T) == sizeof(uint32_t), "patch32 is used for 4-bytes long ops");
        func_.patch32(pos, val);
    }
    void patchSignature(size_t pos, const LifoSignature* ptr) {
        func_.patchSignature(pos, ptr);
    }

    size_t tempU8() {
        return func_.writeU8(uint8_t(Stmt::Bad));
    }
    size_t tempOp() {
        return tempU8();
    }
    size_t temp32() {
        size_t ret = func_.writeU8(uint8_t(Stmt::Bad));
        for (size_t i = 1; i < 4; i++)
            func_.writeU8(uint8_t(Stmt::Bad));
        return ret;
    }
    size_t tempPtr() {
        size_t ret = func_.writeU8(uint8_t(Stmt::Bad));
        for (size_t i = 1; i < sizeof(intptr_t); i++)
            func_.writeU8(uint8_t(Stmt::Bad));
        return ret;
    }
    /************************************************** End of build helpers */
};

} /* anonymous namespace */

/*****************************************************************************/
// asm.js type-checking and code-generation algorithm

static bool
CheckIdentifier(ModuleValidator& m, ParseNode* usepn, PropertyName* name)
{
    if (name == m.cx()->names().arguments || name == m.cx()->names().eval)
        return m.failName(usepn, "'%s' is not an allowed identifier", name);
    return true;
}

static bool
CheckModuleLevelName(ModuleValidator& m, ParseNode* usepn, PropertyName* name)
{
    if (!CheckIdentifier(m, usepn, name))
        return false;

    if (name == m.moduleFunctionName() ||
        name == m.module().globalArgumentName() ||
        name == m.module().importArgumentName() ||
        name == m.module().bufferArgumentName() ||
        m.lookupGlobal(name))
    {
        return m.failName(usepn, "duplicate name '%s' not allowed", name);
    }

    return true;
}

static bool
CheckFunctionHead(ModuleValidator& m, ParseNode* fn)
{
    JSFunction* fun = FunctionObject(fn);
    if (fun->hasRest())
        return m.fail(fn, "rest args not allowed");
    if (fun->isExprBody())
        return m.fail(fn, "expression closures not allowed");
    if (fn->pn_funbox->hasDestructuringArgs)
        return m.fail(fn, "destructuring args not allowed");
    return true;
}

static bool
CheckArgument(ModuleValidator& m, ParseNode* arg, PropertyName** name)
{
    if (!IsDefinition(arg))
        return m.fail(arg, "duplicate argument name not allowed");

    if (arg->isKind(PNK_ASSIGN))
        return m.fail(arg, "default arguments not allowed");

    if (!CheckIdentifier(m, arg, arg->name()))
        return false;

    *name = arg->name();
    return true;
}

static bool
CheckModuleArgument(ModuleValidator& m, ParseNode* arg, PropertyName** name)
{
    if (!CheckArgument(m, arg, name))
        return false;

    if (!CheckModuleLevelName(m, arg, *name))
        return false;

    return true;
}

static bool
CheckModuleArguments(ModuleValidator& m, ParseNode* fn)
{
    unsigned numFormals;
    ParseNode* arg1 = FunctionArgsList(fn, &numFormals);
    ParseNode* arg2 = arg1 ? NextNode(arg1) : nullptr;
    ParseNode* arg3 = arg2 ? NextNode(arg2) : nullptr;

    if (numFormals > 3)
        return m.fail(fn, "asm.js modules takes at most 3 argument");

    PropertyName* arg1Name = nullptr;
    if (numFormals >= 1 && !CheckModuleArgument(m, arg1, &arg1Name))
        return false;
    m.initGlobalArgumentName(arg1Name);

    PropertyName* arg2Name = nullptr;
    if (numFormals >= 2 && !CheckModuleArgument(m, arg2, &arg2Name))
        return false;
    m.initImportArgumentName(arg2Name);

    PropertyName* arg3Name = nullptr;
    if (numFormals >= 3 && !CheckModuleArgument(m, arg3, &arg3Name))
        return false;
    m.initBufferArgumentName(arg3Name);

    return true;
}

static bool
CheckPrecedingStatements(ModuleValidator& m, ParseNode* stmtList)
{
    MOZ_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));

    ParseNode* stmt = ListHead(stmtList);
    for (unsigned i = 0, n = ListLength(stmtList); i < n; i++) {
        if (!IsIgnoredDirective(m.cx(), stmt))
            return m.fail(stmt, "invalid asm.js statement");
    }

    return true;
}

static bool
CheckGlobalVariableInitConstant(ModuleValidator& m, PropertyName* varName, ParseNode* initNode,
                                bool isConst)
{
    AsmJSNumLit literal = ExtractNumericLiteral(m, initNode);
    if (!literal.hasType())
        return m.fail(initNode, "global initializer is out of representable integer range");

    return m.addGlobalVarInit(varName, literal, isConst);
}

static bool
CheckTypeAnnotation(ModuleValidator& m, ParseNode* coercionNode, AsmJSCoercion* coercion,
                    ParseNode** coercedExpr = nullptr)
{
    switch (coercionNode->getKind()) {
      case PNK_BITOR: {
        ParseNode* rhs = BitwiseRight(coercionNode);
        uint32_t i;
        if (!IsLiteralInt(m, rhs, &i) || i != 0)
            return m.fail(rhs, "must use |0 for argument/return coercion");
        *coercion = AsmJS_ToInt32;
        if (coercedExpr)
            *coercedExpr = BitwiseLeft(coercionNode);
        return true;
      }
      case PNK_POS: {
        *coercion = AsmJS_ToNumber;
        if (coercedExpr)
            *coercedExpr = UnaryKid(coercionNode);
        return true;
      }
      case PNK_CALL: {
        if (IsCoercionCall(m, coercionNode, coercion, coercedExpr))
            return true;
      }
      default:;
    }

    return m.fail(coercionNode, "must be of the form +x, x|0, fround(x), or a SIMD check(x)");
}

static bool
CheckGlobalVariableImportExpr(ModuleValidator& m, PropertyName* varName, AsmJSCoercion coercion,
                              ParseNode* coercedExpr, bool isConst)
{
    if (!coercedExpr->isKind(PNK_DOT))
        return m.failName(coercedExpr, "invalid import expression for global '%s'", varName);

    ParseNode* base = DotBase(coercedExpr);
    PropertyName* field = DotMember(coercedExpr);

    PropertyName* importName = m.module().importArgumentName();
    if (!importName)
        return m.fail(coercedExpr, "cannot import without an asm.js foreign parameter");
    if (!IsUseOfName(base, importName))
        return m.failName(coercedExpr, "base of import expression must be '%s'", importName);

    return m.addGlobalVarImport(varName, field, coercion, isConst);
}

static bool
CheckGlobalVariableInitImport(ModuleValidator& m, PropertyName* varName, ParseNode* initNode,
                              bool isConst)
{
    AsmJSCoercion coercion;
    ParseNode* coercedExpr;
    if (!CheckTypeAnnotation(m, initNode, &coercion, &coercedExpr))
        return false;
    return CheckGlobalVariableImportExpr(m, varName, coercion, coercedExpr, isConst);
}

static bool
IsArrayViewCtorName(ModuleValidator& m, PropertyName* name, Scalar::Type* type, bool* shared)
{
    JSAtomState& names = m.cx()->names();
    *shared = false;
    if (name == names.Int8Array) {
        *type = Scalar::Int8;
    } else if (name == names.Uint8Array) {
        *type = Scalar::Uint8;
    } else if (name == names.Int16Array) {
        *type = Scalar::Int16;
    } else if (name == names.Uint16Array) {
        *type = Scalar::Uint16;
    } else if (name == names.Int32Array) {
        *type = Scalar::Int32;
    } else if (name == names.Uint32Array) {
        *type = Scalar::Uint32;
    } else if (name == names.Float32Array) {
        *type = Scalar::Float32;
    } else if (name == names.Float64Array) {
        *type = Scalar::Float64;
    } else if (name == names.SharedInt8Array) {
        *shared = true;
        *type = Scalar::Int8;
    } else if (name == names.SharedUint8Array) {
        *shared = true;
        *type = Scalar::Uint8;
    } else if (name == names.SharedInt16Array) {
        *shared = true;
        *type = Scalar::Int16;
    } else if (name == names.SharedUint16Array) {
        *shared = true;
        *type = Scalar::Uint16;
    } else if (name == names.SharedInt32Array) {
        *shared = true;
        *type = Scalar::Int32;
    } else if (name == names.SharedUint32Array) {
        *shared = true;
        *type = Scalar::Uint32;
    } else if (name == names.SharedFloat32Array) {
        *shared = true;
        *type = Scalar::Float32;
    } else if (name == names.SharedFloat64Array) {
        *shared = true;
        *type = Scalar::Float64;
    } else {
        return false;
    }
    return true;
}

static bool
CheckNewArrayViewArgs(ModuleValidator& m, ParseNode* ctorExpr, PropertyName* bufferName)
{
    ParseNode* bufArg = NextNode(ctorExpr);
    if (!bufArg || NextNode(bufArg) != nullptr)
        return m.fail(ctorExpr, "array view constructor takes exactly one argument");

    if (!IsUseOfName(bufArg, bufferName))
        return m.failName(bufArg, "argument to array view constructor must be '%s'", bufferName);

    return true;
}

static bool
CheckNewArrayView(ModuleValidator& m, PropertyName* varName, ParseNode* newExpr)
{
    PropertyName* globalName = m.module().globalArgumentName();
    if (!globalName)
        return m.fail(newExpr, "cannot create array view without an asm.js global parameter");

    PropertyName* bufferName = m.module().bufferArgumentName();
    if (!bufferName)
        return m.fail(newExpr, "cannot create array view without an asm.js heap parameter");

    ParseNode* ctorExpr = ListHead(newExpr);

    PropertyName* field;
    Scalar::Type type;
    bool shared = false;
    if (ctorExpr->isKind(PNK_DOT)) {
        ParseNode* base = DotBase(ctorExpr);

        if (!IsUseOfName(base, globalName))
            return m.failName(base, "expecting '%s.*Array", globalName);

        field = DotMember(ctorExpr);
        if (!IsArrayViewCtorName(m, field, &type, &shared))
            return m.fail(ctorExpr, "could not match typed array name");
    } else {
        if (!ctorExpr->isKind(PNK_NAME))
            return m.fail(ctorExpr, "expecting name of imported array view constructor");

        PropertyName* globalName = ctorExpr->name();
        const ModuleValidator::Global* global = m.lookupGlobal(globalName);
        if (!global)
            return m.failName(ctorExpr, "%s not found in module global scope", globalName);

        if (global->which() != ModuleValidator::Global::ArrayViewCtor)
            return m.failName(ctorExpr, "%s must be an imported array view constructor", globalName);

        field = nullptr;
        type = global->viewType();
        shared = global->viewIsSharedView();
    }

#if !defined(ENABLE_SHARED_ARRAY_BUFFER)
    if (shared)
        return m.fail(ctorExpr, "shared views not supported by this build");
#endif

    if (!CheckNewArrayViewArgs(m, ctorExpr, bufferName))
        return false;

    if (!m.module().isValidViewSharedness(shared))
        return m.failName(ctorExpr, "%s has different sharedness than previous view constructors", globalName);

    return m.addArrayView(varName, type, field, shared);
}

static bool
IsSimdTypeName(ModuleValidator& m, PropertyName* name, AsmJSSimdType* type)
{
    if (name == m.cx()->names().int32x4) {
        *type = AsmJSSimdType_int32x4;
        return true;
    }
    if (name == m.cx()->names().float32x4) {
        *type = AsmJSSimdType_float32x4;
        return true;
    }
    return false;
}

static bool
IsSimdValidOperationType(AsmJSSimdType type, AsmJSSimdOperation op)
{
    switch (op) {
#define CASE(op) case AsmJSSimdOperation_##op:
      FOREACH_COMMONX4_SIMD_OP(CASE)
        return true;
      FOREACH_INT32X4_SIMD_OP(CASE)
        return type == AsmJSSimdType_int32x4;
      FOREACH_FLOAT32X4_SIMD_OP(CASE)
        return type == AsmJSSimdType_float32x4;
#undef CASE
    }
    return false;
}

static bool
CheckGlobalMathImport(ModuleValidator& m, ParseNode* initNode, PropertyName* varName,
                      PropertyName* field)
{
    // Math builtin, with the form glob.Math.[[builtin]]
    ModuleValidator::MathBuiltin mathBuiltin;
    if (!m.lookupStandardLibraryMathName(field, &mathBuiltin))
        return m.failName(initNode, "'%s' is not a standard Math builtin", field);

    switch (mathBuiltin.kind) {
      case ModuleValidator::MathBuiltin::Function:
        return m.addMathBuiltinFunction(varName, mathBuiltin.u.func, field);
      case ModuleValidator::MathBuiltin::Constant:
        return m.addMathBuiltinConstant(varName, mathBuiltin.u.cst, field);
      default:
        break;
    }
    MOZ_CRASH("unexpected or uninitialized math builtin type");
}

static bool
CheckGlobalAtomicsImport(ModuleValidator& m, ParseNode* initNode, PropertyName* varName,
                         PropertyName* field)
{
    // Atomics builtin, with the form glob.Atomics.[[builtin]]
    AsmJSAtomicsBuiltinFunction func;
    if (!m.lookupStandardLibraryAtomicsName(field, &func))
        return m.failName(initNode, "'%s' is not a standard Atomics builtin", field);

    return m.addAtomicsBuiltinFunction(varName, func, field);
}

static bool
CheckGlobalSimdImport(ModuleValidator& m, ParseNode* initNode, PropertyName* varName,
                      PropertyName* field)
{
    if (!m.supportsSimd())
        return m.fail(initNode, "SIMD is not supported on this platform");

    // SIMD constructor, with the form glob.SIMD.[[type]]
    AsmJSSimdType simdType;
    if (!IsSimdTypeName(m, field, &simdType))
        return m.failName(initNode, "'%s' is not a standard SIMD type", field);
    return m.addSimdCtor(varName, simdType, field);
}

static bool
CheckGlobalSimdOperationImport(ModuleValidator& m, const ModuleValidator::Global* global,
                               ParseNode* initNode, PropertyName* varName, PropertyName* ctorVarName,
                               PropertyName* opName)
{
    AsmJSSimdType simdType = global->simdCtorType();
    AsmJSSimdOperation simdOp;
    if (!m.lookupStandardSimdOpName(opName, &simdOp))
        return m.failName(initNode, "'%s' is not a standard SIMD operation", opName);
    if (!IsSimdValidOperationType(simdType, simdOp))
        return m.failName(initNode, "'%s' is not an operation supported by the SIMD type", opName);
    return m.addSimdOperation(varName, simdType, simdOp, ctorVarName, opName);
}

static bool
CheckGlobalDotImport(ModuleValidator& m, PropertyName* varName, ParseNode* initNode)
{
    ParseNode* base = DotBase(initNode);
    PropertyName* field = DotMember(initNode);

    if (base->isKind(PNK_DOT)) {
        ParseNode* global = DotBase(base);
        PropertyName* mathOrAtomicsOrSimd = DotMember(base);

        PropertyName* globalName = m.module().globalArgumentName();
        if (!globalName)
            return m.fail(base, "import statement requires the module have a stdlib parameter");

        if (!IsUseOfName(global, globalName)) {
            if (global->isKind(PNK_DOT)) {
                return m.failName(base, "imports can have at most two dot accesses "
                                        "(e.g. %s.Math.sin)", globalName);
            }
            return m.failName(base, "expecting %s.*", globalName);
        }

        if (mathOrAtomicsOrSimd == m.cx()->names().Math)
            return CheckGlobalMathImport(m, initNode, varName, field);
        if (mathOrAtomicsOrSimd == m.cx()->names().Atomics)
            return CheckGlobalAtomicsImport(m, initNode, varName, field);
        if (mathOrAtomicsOrSimd == m.cx()->names().SIMD)
            return CheckGlobalSimdImport(m, initNode, varName, field);
        return m.failName(base, "expecting %s.{Math|SIMD}", globalName);
    }

    if (!base->isKind(PNK_NAME))
        return m.fail(base, "expected name of variable or parameter");

    if (base->name() == m.module().globalArgumentName()) {
        if (field == m.cx()->names().NaN)
            return m.addGlobalConstant(varName, GenericNaN(), field);
        if (field == m.cx()->names().Infinity)
            return m.addGlobalConstant(varName, PositiveInfinity<double>(), field);
        if (field == m.cx()->names().byteLength)
            return m.addByteLength(varName);

        Scalar::Type type;
        bool shared = false;
        if (IsArrayViewCtorName(m, field, &type, &shared)) {
#if !defined(ENABLE_SHARED_ARRAY_BUFFER)
            if (shared)
                return m.fail(initNode, "shared views not supported by this build");
#endif
            if (!m.module().isValidViewSharedness(shared))
                return m.failName(initNode, "'%s' has different sharedness than previous view constructors", field);
            return m.addArrayViewCtor(varName, type, field, shared);
        }

        return m.failName(initNode, "'%s' is not a standard constant or typed array name", field);
    }

    if (base->name() == m.module().importArgumentName())
        return m.addFFI(varName, field);

    const ModuleValidator::Global* global = m.lookupGlobal(base->name());
    if (!global)
        return m.failName(initNode, "%s not found in module global scope", base->name());

    if (!global->isSimdCtor())
        return m.failName(base, "expecting SIMD constructor name, got %s", field);

    return CheckGlobalSimdOperationImport(m, global, initNode, varName, base->name(), field);
}

static bool
CheckModuleGlobal(ModuleValidator& m, ParseNode* var, bool isConst)
{
    if (!IsDefinition(var))
        return m.fail(var, "import variable names must be unique");

    if (!CheckModuleLevelName(m, var, var->name()))
        return false;

    ParseNode* initNode = MaybeDefinitionInitializer(var);
    if (!initNode)
        return m.fail(var, "module import needs initializer");

    if (IsNumericLiteral(m, initNode))
        return CheckGlobalVariableInitConstant(m, var->name(), initNode, isConst);

    if (initNode->isKind(PNK_BITOR) || initNode->isKind(PNK_POS) || initNode->isKind(PNK_CALL))
        return CheckGlobalVariableInitImport(m, var->name(), initNode, isConst);

    if (initNode->isKind(PNK_NEW))
        return CheckNewArrayView(m, var->name(), initNode);

    if (initNode->isKind(PNK_DOT))
        return CheckGlobalDotImport(m, var->name(), initNode);

    return m.fail(initNode, "unsupported import expression");
}

static bool
CheckModuleProcessingDirectives(ModuleValidator& m)
{
    TokenStream& ts = m.parser().tokenStream;
    while (true) {
        bool matched;
        if (!ts.matchToken(&matched, TOK_STRING, TokenStream::Operand))
            return false;
        if (!matched)
            return true;

        if (!IsIgnoredDirectiveName(m.cx(), ts.currentToken().atom()))
            return m.failOffset(ts.currentToken().pos.begin, "unsupported processing directive");

        TokenKind tt;
        if (!ts.getToken(&tt))
            return false;
        if (tt != TOK_SEMI) {
            return m.failOffset(ts.currentToken().pos.begin,
                                "expected semicolon after string literal");
        }
    }
}

static bool
CheckModuleGlobals(ModuleValidator& m)
{
    while (true) {
        ParseNode* varStmt;
        if (!ParseVarOrConstStatement(m.parser(), &varStmt))
            return false;
        if (!varStmt)
            break;
        for (ParseNode* var = VarListHead(varStmt); var; var = NextNode(var)) {
            if (!CheckModuleGlobal(m, var, varStmt->isKind(PNK_CONST)))
                return false;
        }
    }

    return true;
}

static bool
ArgFail(FunctionValidator& f, PropertyName* argName, ParseNode* stmt)
{
    return f.failName(stmt, "expecting argument type declaration for '%s' of the "
                      "form 'arg = arg|0' or 'arg = +arg' or 'arg = fround(arg)'", argName);
}

static bool
CheckArgumentType(FunctionValidator& f, ParseNode* stmt, PropertyName* name, VarType* type)
{
    if (!stmt || !IsExpressionStatement(stmt))
        return ArgFail(f, name, stmt ? stmt : f.fn());

    ParseNode* initNode = ExpressionStatementExpr(stmt);
    if (!initNode || !initNode->isKind(PNK_ASSIGN))
        return ArgFail(f, name, stmt);

    ParseNode* argNode = BinaryLeft(initNode);
    ParseNode* coercionNode = BinaryRight(initNode);

    if (!IsUseOfName(argNode, name))
        return ArgFail(f, name, stmt);

    ParseNode* coercedExpr;
    AsmJSCoercion coercion;
    if (!CheckTypeAnnotation(f.m(), coercionNode, &coercion, &coercedExpr))
        return false;

    if (!IsUseOfName(coercedExpr, name))
        return ArgFail(f, name, stmt);

    *type = VarType(coercion);
    return true;
}

static bool
CheckProcessingDirectives(ModuleValidator& m, ParseNode** stmtIter)
{
    ParseNode* stmt = *stmtIter;

    while (stmt && IsIgnoredDirective(m.cx(), stmt))
        stmt = NextNode(stmt);

    *stmtIter = stmt;
    return true;
}

static bool
CheckArguments(FunctionValidator& f, ParseNode** stmtIter, VarTypeVector* argTypes)
{
    ParseNode* stmt = *stmtIter;

    unsigned numFormals;
    ParseNode* argpn = FunctionArgsList(f.fn(), &numFormals);

    for (unsigned i = 0; i < numFormals; i++, argpn = NextNode(argpn), stmt = NextNode(stmt)) {
        PropertyName* name;
        if (!CheckArgument(f.m(), argpn, &name))
            return false;

        VarType type;
        if (!CheckArgumentType(f, stmt, name, &type))
            return false;

        if (!argTypes->append(type))
            return false;

        if (!f.addFormal(argpn, name, type))
            return false;
    }

    *stmtIter = stmt;
    return true;
}

static bool
IsLiteralOrConst(FunctionValidator& f, ParseNode* pn, AsmJSNumLit* lit)
{
    if (pn->isKind(PNK_NAME)) {
        const ModuleValidator::Global* global = f.lookupGlobal(pn->name());
        if (!global || global->which() != ModuleValidator::Global::ConstantLiteral)
            return false;

        *lit = global->constLiteralValue();
        return true;
    }

    if (!IsNumericLiteral(f.m(), pn))
        return false;

    *lit = ExtractNumericLiteral(f.m(), pn);
    return true;
}

static bool
CheckFinalReturn(FunctionValidator& f, ParseNode* lastNonEmptyStmt)
{
    if (!f.hasAlreadyReturned()) {
        f.setReturnedType(RetType::Void);
        f.writeOp(Stmt::Ret);
        return true;
    }

    if (!lastNonEmptyStmt->isKind(PNK_RETURN)) {
        if (f.returnedType() != RetType::Void)
            return f.fail(lastNonEmptyStmt, "void incompatible with previous return type");

        f.writeOp(Stmt::Ret);
        return true;
    }

    return true;
}

static bool
CheckVariable(FunctionValidator& f, ParseNode* var)
{
    if (!IsDefinition(var))
        return f.fail(var, "local variable names must not restate argument names");

    PropertyName* name = var->name();

    if (!CheckIdentifier(f.m(), var, name))
        return false;

    ParseNode* initNode = MaybeDefinitionInitializer(var);
    if (!initNode)
        return f.failName(var, "var '%s' needs explicit type declaration via an initial value", name);

    AsmJSNumLit lit;
    if (!IsLiteralOrConst(f, initNode, &lit))
        return f.failName(var, "var '%s' initializer must be literal or const literal", name);

    if (!lit.hasType())
        return f.failName(var, "var '%s' initializer out of range", name);

    return f.addVariable(var, name, lit);
}

static bool
CheckVariables(FunctionValidator& f, ParseNode** stmtIter)
{
    ParseNode* stmt = *stmtIter;

    for (; stmt && stmt->isKind(PNK_VAR); stmt = NextNonEmptyStatement(stmt)) {
        for (ParseNode* var = VarListHead(stmt); var; var = NextNode(var)) {
            if (!CheckVariable(f, var))
                return false;
        }
    }

    *stmtIter = stmt;
    return true;
}

static bool
CheckExpr(FunctionValidator& f, ParseNode* expr, Type* type);

static bool
CheckNumericLiteral(FunctionValidator& f, ParseNode* num, Type* type)
{
    AsmJSNumLit literal = ExtractNumericLiteral(f.m(), num);
    if (!literal.hasType())
        return f.fail(num, "numeric literal out of representable integer range");
    f.writeLit(literal);
    *type = Type::Of(literal);
    return true;
}

static bool
CheckVarRef(FunctionValidator& f, ParseNode* varRef, Type* type)
{
    PropertyName* name = varRef->name();

    if (const FunctionValidator::Local* local = f.lookupLocal(name)) {
        switch (local->type.which()) {
          case VarType::Int:       f.writeOp(I32::GetLocal);   break;
          case VarType::Double:    f.writeOp(F64::GetLocal);   break;
          case VarType::Float:     f.writeOp(F32::GetLocal);   break;
          case VarType::Int32x4:   f.writeOp(I32X4::GetLocal); break;
          case VarType::Float32x4: f.writeOp(F32X4::GetLocal); break;
        }
        f.writeU32(local->slot);
        *type = local->type.toType();
        return true;
    }

    if (const ModuleValidator::Global* global = f.lookupGlobal(name)) {
        switch (global->which()) {
          case ModuleValidator::Global::ConstantLiteral:
            f.writeLit(global->constLiteralValue());
            *type = global->varOrConstType();
            break;
          case ModuleValidator::Global::ConstantImport:
          case ModuleValidator::Global::Variable: {
            switch (global->varOrConstType().which()) {
              case Type::Int:       f.writeOp(I32::GetGlobal);   break;
              case Type::Double:    f.writeOp(F64::GetGlobal);   break;
              case Type::Float:     f.writeOp(F32::GetGlobal);   break;
              case Type::Int32x4:   f.writeOp(I32X4::GetGlobal); break;
              case Type::Float32x4: f.writeOp(F32X4::GetGlobal); break;
              default: MOZ_CRASH("unexpected global type");
            }

            uint32_t globalIndex = global->varOrConstIndex();
            // Write the global data offset
            if (global->varOrConstType().isSimd())
                f.writeU32(f.module().globalSimdVarIndexToGlobalDataOffset(globalIndex));
            else
                f.writeU32(f.module().globalScalarVarIndexToGlobalDataOffset(globalIndex));
            f.writeU8(uint8_t(global->isConst()));
            *type = global->varOrConstType();
            break;
          }
          case ModuleValidator::Global::Function:
          case ModuleValidator::Global::FFI:
          case ModuleValidator::Global::MathBuiltinFunction:
          case ModuleValidator::Global::AtomicsBuiltinFunction:
          case ModuleValidator::Global::FuncPtrTable:
          case ModuleValidator::Global::ArrayView:
          case ModuleValidator::Global::ArrayViewCtor:
          case ModuleValidator::Global::SimdCtor:
          case ModuleValidator::Global::SimdOperation:
          case ModuleValidator::Global::ByteLength:
          case ModuleValidator::Global::ChangeHeap:
            return f.failName(varRef, "'%s' may not be accessed by ordinary expressions", name);
        }
        return true;
    }

    return f.failName(varRef, "'%s' not found in local or asm.js module scope", name);
}

static inline bool
IsLiteralOrConstInt(FunctionValidator& f, ParseNode* pn, uint32_t* u32)
{
    AsmJSNumLit lit;
    if (!IsLiteralOrConst(f, pn, &lit))
        return false;

    return IsLiteralInt(lit, u32);
}

static bool
FoldMaskedArrayIndex(FunctionValidator& f, ParseNode** indexExpr, int32_t* mask,
                     NeedsBoundsCheck* needsBoundsCheck)
{
    MOZ_ASSERT((*indexExpr)->isKind(PNK_BITAND));

    ParseNode* indexNode = BitwiseLeft(*indexExpr);
    ParseNode* maskNode = BitwiseRight(*indexExpr);

    uint32_t mask2;
    if (IsLiteralOrConstInt(f, maskNode, &mask2)) {
        // Flag the access to skip the bounds check if the mask ensures that an
        // 'out of bounds' access can not occur based on the current heap length
        // constraint. The unsigned maximum of a masked index is the mask
        // itself, so check that the mask is not negative and compare the mask
        // to the known minimum heap length.
        if (int32_t(mask2) >= 0 && mask2 < f.m().minHeapLength())
            *needsBoundsCheck = NO_BOUNDS_CHECK;
        *mask &= mask2;
        *indexExpr = indexNode;
        return true;
    }

    return false;
}

static const int32_t NoMask = -1;

static bool
CheckArrayAccess(FunctionValidator& f, ParseNode* viewName, ParseNode* indexExpr,
                 Scalar::Type* viewType, NeedsBoundsCheck* needsBoundsCheck, int32_t* mask)
{
    *needsBoundsCheck = NEEDS_BOUNDS_CHECK;

    if (!viewName->isKind(PNK_NAME))
        return f.fail(viewName, "base of array access must be a typed array view name");

    const ModuleValidator::Global* global = f.lookupGlobal(viewName->name());
    if (!global || !global->isAnyArrayView())
        return f.fail(viewName, "base of array access must be a typed array view name");

    *viewType = global->viewType();

    uint32_t index;
    if (IsLiteralOrConstInt(f, indexExpr, &index)) {
        uint64_t byteOffset = uint64_t(index) << TypedArrayShift(*viewType);
        if (byteOffset > INT32_MAX)
            return f.fail(indexExpr, "constant index out of range");

        unsigned elementSize = TypedArrayElemSize(*viewType);
        if (!f.m().tryRequireHeapLengthToBeAtLeast(byteOffset + elementSize)) {
            return f.failf(indexExpr, "constant index outside heap size range declared by the "
                                      "change-heap function (0x%x - 0x%x)",
                                      f.m().minHeapLength(), f.m().module().maxHeapLength());
        }

        *mask = NoMask;
        *needsBoundsCheck = NO_BOUNDS_CHECK;
        f.writeInt32Lit(byteOffset);
        return true;
    }

    // Mask off the low bits to account for the clearing effect of a right shift
    // followed by the left shift implicit in the array access. E.g., H32[i>>2]
    // loses the low two bits.
    *mask = ~(TypedArrayElemSize(*viewType) - 1);

    if (indexExpr->isKind(PNK_RSH)) {
        ParseNode* shiftAmountNode = BitwiseRight(indexExpr);

        uint32_t shift;
        if (!IsLiteralInt(f.m(), shiftAmountNode, &shift))
            return f.failf(shiftAmountNode, "shift amount must be constant");

        unsigned requiredShift = TypedArrayShift(*viewType);
        if (shift != requiredShift)
            return f.failf(shiftAmountNode, "shift amount must be %u", requiredShift);

        ParseNode* pointerNode = BitwiseLeft(indexExpr);

        if (pointerNode->isKind(PNK_BITAND))
            FoldMaskedArrayIndex(f, &pointerNode, mask, needsBoundsCheck);

        f.enterHeapExpression();

        Type pointerType;
        if (!CheckExpr(f, pointerNode, &pointerType))
            return false;

        f.leaveHeapExpression();

        if (!pointerType.isIntish())
            return f.failf(pointerNode, "%s is not a subtype of int", pointerType.toChars());
    } else {
        // For legacy compatibility, accept Int8/Uint8 accesses with no shift.
        if (TypedArrayShift(*viewType) != 0)
            return f.fail(indexExpr, "index expression isn't shifted; must be an Int8/Uint8 access");

        MOZ_ASSERT(*mask == NoMask);
        bool folded = false;

        ParseNode* pointerNode = indexExpr;

        if (pointerNode->isKind(PNK_BITAND))
            folded = FoldMaskedArrayIndex(f, &pointerNode, mask, needsBoundsCheck);

        f.enterHeapExpression();

        Type pointerType;
        if (!CheckExpr(f, pointerNode, &pointerType))
            return false;

        f.leaveHeapExpression();

        if (folded) {
            if (!pointerType.isIntish())
                return f.failf(pointerNode, "%s is not a subtype of intish", pointerType.toChars());
        } else {
            if (!pointerType.isInt())
                return f.failf(pointerNode, "%s is not a subtype of int", pointerType.toChars());
        }
    }

    return true;
}

static bool
CheckAndPrepareArrayAccess(FunctionValidator& f, ParseNode* viewName, ParseNode* indexExpr,
                           Scalar::Type* viewType, NeedsBoundsCheck* needsBoundsCheck, int32_t* mask)
{
    size_t prepareAt = f.tempOp();

    if (!CheckArrayAccess(f, viewName, indexExpr, viewType, needsBoundsCheck, mask))
        return false;

    // Don't generate the mask op if there is no need for it which could happen for
    // a shift of zero or a SIMD access.
    if (*mask != NoMask) {
        f.patchOp(prepareAt, I32::BitAnd);
        f.writeInt32Lit(*mask);
    } else {
        f.patchOp(prepareAt, I32::Id);
    }

    return true;
}

static bool
CheckLoadArray(FunctionValidator& f, ParseNode* elem, Type* type)
{
    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;

    size_t opcodeAt = f.tempOp();
    size_t needsBoundsCheckAt = f.tempU8();

    if (!CheckAndPrepareArrayAccess(f, ElemBase(elem), ElemIndex(elem), &viewType, &needsBoundsCheck, &mask))
        return false;

    switch (viewType) {
      case Scalar::Int8:    f.patchOp(opcodeAt, I32::SLoad8);  break;
      case Scalar::Int16:   f.patchOp(opcodeAt, I32::SLoad16); break;
      case Scalar::Int32:   f.patchOp(opcodeAt, I32::SLoad32); break;
      case Scalar::Uint8:   f.patchOp(opcodeAt, I32::ULoad8);  break;
      case Scalar::Uint16:  f.patchOp(opcodeAt, I32::ULoad16); break;
      case Scalar::Uint32:  f.patchOp(opcodeAt, I32::ULoad32); break;
      case Scalar::Float32: f.patchOp(opcodeAt, F32::Load);    break;
      case Scalar::Float64: f.patchOp(opcodeAt, F64::Load);    break;
      default: MOZ_CRASH("unexpected scalar type");
    }

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));

    *type = TypedArrayLoadType(viewType);
    return true;
}

static bool
CheckDotAccess(FunctionValidator& f, ParseNode* elem, Type* type)
{
    MOZ_ASSERT(elem->isKind(PNK_DOT));

    size_t opcodeAt = f.tempOp();

    ParseNode* base = DotBase(elem);
    Type baseType;
    if (!CheckExpr(f, base, &baseType))
        return false;
    if (!baseType.isSimd())
        return f.failf(base, "expected SIMD type, got %s", baseType.toChars());

    ModuleValidator& m = f.m();
    PropertyName* field = DotMember(elem);

    JSAtomState& names = m.cx()->names();

    if (field != names.signMask)
        return f.fail(base, "dot access field must be signMask");

    *type = Type::Signed;
    switch (baseType.simdType()) {
      case AsmJSSimdType_int32x4:   f.patchOp(opcodeAt, I32::I32X4SignMask); break;
      case AsmJSSimdType_float32x4: f.patchOp(opcodeAt, I32::F32X4SignMask); break;
    }

    return true;
}

static bool
CheckStoreArray(FunctionValidator& f, ParseNode* lhs, ParseNode* rhs, Type* type)
{
    size_t opcodeAt = f.tempOp();
    size_t needsBoundsCheckAt = f.tempU8();

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckAndPrepareArrayAccess(f, ElemBase(lhs), ElemIndex(lhs), &viewType, &needsBoundsCheck, &mask))
        return false;

    f.enterHeapExpression();

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    f.leaveHeapExpression();

    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Int16:
      case Scalar::Int32:
      case Scalar::Uint8:
      case Scalar::Uint16:
      case Scalar::Uint32:
        if (!rhsType.isIntish())
            return f.failf(lhs, "%s is not a subtype of intish", rhsType.toChars());
        break;
      case Scalar::Float32:
        if (!rhsType.isMaybeDouble() && !rhsType.isFloatish())
            return f.failf(lhs, "%s is not a subtype of double? or floatish", rhsType.toChars());
        break;
      case Scalar::Float64:
        if (!rhsType.isMaybeFloat() && !rhsType.isMaybeDouble())
            return f.failf(lhs, "%s is not a subtype of float? or double?", rhsType.toChars());
        break;
      default:
        MOZ_CRASH("Unexpected view type");
    }

    switch (viewType) {
      case Scalar::Int8:
      case Scalar::Uint8:
        f.patchOp(opcodeAt, I32::Store8);
        break;
      case Scalar::Int16:
      case Scalar::Uint16:
        f.patchOp(opcodeAt, I32::Store16);
        break;
      case Scalar::Int32:
      case Scalar::Uint32:
        f.patchOp(opcodeAt, I32::Store32);
        break;
      case Scalar::Float32:
        if (rhsType.isFloatish())
            f.patchOp(opcodeAt, F32::StoreF32);
        else
            f.patchOp(opcodeAt, F64::StoreF32);
        break;
      case Scalar::Float64:
        if (rhsType.isFloatish())
            f.patchOp(opcodeAt, F32::StoreF64);
        else
            f.patchOp(opcodeAt, F64::StoreF64);
        break;
      default: MOZ_CRASH("unexpected scalar type");
    }

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));

    *type = rhsType;
    return true;
}

static bool
CheckAssignName(FunctionValidator& f, ParseNode* lhs, ParseNode* rhs, Type* type)
{
    RootedPropertyName name(f.cx(), lhs->name());

    size_t opcodeAt = f.tempOp();
    size_t indexAt = f.temp32();

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (const FunctionValidator::Local* lhsVar = f.lookupLocal(name)) {
        if (!(rhsType <= lhsVar->type)) {
            return f.failf(lhs, "%s is not a subtype of %s",
                           rhsType.toChars(), lhsVar->type.toType().toChars());
        }

        switch (lhsVar->type.which()) {
          case VarType::Int:       f.patchOp(opcodeAt, I32::SetLocal);   break;
          case VarType::Float:     f.patchOp(opcodeAt, F32::SetLocal);   break;
          case VarType::Double:    f.patchOp(opcodeAt, F64::SetLocal);   break;
          case VarType::Int32x4:   f.patchOp(opcodeAt, I32X4::SetLocal); break;
          case VarType::Float32x4: f.patchOp(opcodeAt, F32X4::SetLocal); break;
        }

        f.patch32(indexAt, lhsVar->slot);
        *type = rhsType;
        return true;
    }

    if (const ModuleValidator::Global* global = f.lookupGlobal(name)) {
        if (global->which() != ModuleValidator::Global::Variable)
            return f.failName(lhs, "'%s' is not a mutable variable", name);

        if (!(rhsType <= global->varOrConstType())) {
            return f.failf(lhs, "%s is not a subtype of %s",
                           rhsType.toChars(), global->varOrConstType().toChars());
        }

        switch (global->varOrConstType().which()) {
          case Type::Int:       f.patchOp(opcodeAt, I32::SetGlobal);   break;
          case Type::Float:     f.patchOp(opcodeAt, F32::SetGlobal);   break;
          case Type::Double:    f.patchOp(opcodeAt, F64::SetGlobal);   break;
          case Type::Int32x4:   f.patchOp(opcodeAt, I32X4::SetGlobal); break;
          case Type::Float32x4: f.patchOp(opcodeAt, F32X4::SetGlobal); break;
          default: MOZ_CRASH("unexpected global type");
        }

        unsigned globalIndex = global->varOrConstIndex();
        // Global data offset
        if (global->varOrConstType().isSimd())
            f.patch32(indexAt, f.module().globalSimdVarIndexToGlobalDataOffset(globalIndex));
        else
            f.patch32(indexAt, f.module().globalScalarVarIndexToGlobalDataOffset(globalIndex));
        *type = rhsType;
        return true;
    }

    return f.failName(lhs, "'%s' not found in local or asm.js module scope", name);
}

static bool
CheckAssign(FunctionValidator& f, ParseNode* assign, Type* type)
{
    MOZ_ASSERT(assign->isKind(PNK_ASSIGN));

    ParseNode* lhs = BinaryLeft(assign);
    ParseNode* rhs = BinaryRight(assign);

    if (lhs->getKind() == PNK_ELEM)
        return CheckStoreArray(f, lhs, rhs, type);

    if (lhs->getKind() == PNK_NAME)
        return CheckAssignName(f, lhs, rhs, type);

    return f.fail(assign, "left-hand side of assignment must be a variable or array access");
}

static bool
CheckMathIMul(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 2)
        return f.fail(call, "Math.imul must be passed 2 arguments");

    ParseNode* lhs = CallArgList(call);
    ParseNode* rhs = NextNode(lhs);

    f.writeOp(I32::Mul);

    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (!lhsType.isIntish())
        return f.failf(lhs, "%s is not a subtype of intish", lhsType.toChars());
    if (!rhsType.isIntish())
        return f.failf(rhs, "%s is not a subtype of intish", rhsType.toChars());

    *type = Type::Signed;
    return true;
}

static bool
CheckMathClz32(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Math.clz32 must be passed 1 argument");

    f.writeOp(I32::Clz);

    ParseNode* arg = CallArgList(call);

    Type argType;
    if (!CheckExpr(f, arg, &argType))
        return false;

    if (!argType.isIntish())
        return f.failf(arg, "%s is not a subtype of intish", argType.toChars());

    *type = Type::Fixnum;
    return true;
}

static bool
CheckMathAbs(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Math.abs must be passed 1 argument");

    ParseNode* arg = CallArgList(call);

    size_t opcodeAt = f.tempOp();

    Type argType;
    if (!CheckExpr(f, arg, &argType))
        return false;

    if (argType.isSigned()) {
        f.patchOp(opcodeAt, I32::Abs);
        *type = Type::Unsigned;
        return true;
    }

    if (argType.isMaybeDouble()) {
        f.patchOp(opcodeAt, F64::Abs);
        *type = Type::Double;
        return true;
    }

    if (argType.isMaybeFloat()) {
        f.patchOp(opcodeAt, F32::Abs);
        *type = Type::Floatish;
        return true;
    }

    return f.failf(call, "%s is not a subtype of signed, float? or double?", argType.toChars());
}

static bool
CheckMathSqrt(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Math.sqrt must be passed 1 argument");

    ParseNode* arg = CallArgList(call);

    size_t opcodeAt = f.tempOp();

    Type argType;
    if (!CheckExpr(f, arg, &argType))
        return false;

    if (argType.isMaybeDouble()) {
        f.patchOp(opcodeAt, F64::Sqrt);
        *type = Type::Double;
        return true;
    }

    if (argType.isMaybeFloat()) {
        f.patchOp(opcodeAt, F32::Sqrt);
        *type = Type::Floatish;
        return true;
    }

    return f.failf(call, "%s is neither a subtype of double? nor float?", argType.toChars());
}

static bool
CheckMathMinMax(FunctionValidator& f, ParseNode* callNode, bool isMax, Type* type)
{
    if (CallArgListLength(callNode) < 2)
        return f.fail(callNode, "Math.min/max must be passed at least 2 arguments");

    size_t opcodeAt = f.tempOp();
    size_t numArgsAt = f.tempU8();

    ParseNode* firstArg = CallArgList(callNode);
    Type firstType;
    if (!CheckExpr(f, firstArg, &firstType))
        return false;

    if (firstType.isMaybeDouble()) {
        *type = Type::Double;
        firstType = Type::MaybeDouble;
        f.patchOp(opcodeAt, isMax ? F64::Max : F64::Min);
    } else if (firstType.isMaybeFloat()) {
        *type = Type::Float;
        firstType = Type::MaybeFloat;
        f.patchOp(opcodeAt, isMax ? F32::Max : F32::Min);
    } else if (firstType.isSigned()) {
        *type = Type::Signed;
        firstType = Type::Signed;
        f.patchOp(opcodeAt, isMax ? I32::Max : I32::Min);
    } else {
        return f.failf(firstArg, "%s is not a subtype of double?, float? or signed",
                       firstType.toChars());
    }

    unsigned numArgs = CallArgListLength(callNode);
    f.patchU8(numArgsAt, numArgs);

    ParseNode* nextArg = NextNode(firstArg);
    for (unsigned i = 1; i < numArgs; i++, nextArg = NextNode(nextArg)) {
        Type nextType;
        if (!CheckExpr(f, nextArg, &nextType))
            return false;
        if (!(nextType <= firstType))
            return f.failf(nextArg, "%s is not a subtype of %s", nextType.toChars(), firstType.toChars());
    }

    return true;
}

static bool
CheckSharedArrayAtomicAccess(FunctionValidator& f, ParseNode* viewName, ParseNode* indexExpr,
                             Scalar::Type* viewType, NeedsBoundsCheck* needsBoundsCheck,
                             int32_t* mask)
{
    if (!CheckAndPrepareArrayAccess(f, viewName, indexExpr, viewType, needsBoundsCheck, mask))
        return false;

    // Atomic accesses may be made on shared integer arrays only.

    // The global will be sane, CheckArrayAccess checks it.
    const ModuleValidator::Global* global = f.lookupGlobal(viewName->name());
    if (global->which() != ModuleValidator::Global::ArrayView || !f.m().module().isSharedView())
        return f.fail(viewName, "base of array access must be a shared typed array view name");

    switch (*viewType) {
      case Scalar::Int8:
      case Scalar::Int16:
      case Scalar::Int32:
      case Scalar::Uint8:
      case Scalar::Uint16:
      case Scalar::Uint32:
        return true;
      default:
        return f.failf(viewName, "not an integer array");
    }

    return true;
}

static bool
CheckAtomicsFence(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 0)
        return f.fail(call, "Atomics.fence must be passed 0 arguments");

    f.writeOp(Stmt::AtomicsFence);
    *type = Type::Void;
    return true;
}

static bool
CheckAtomicsLoad(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 2)
        return f.fail(call, "Atomics.load must be passed 2 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);

    f.writeOp(I32::AtomicsLoad);
    size_t needsBoundsCheckAt = f.tempU8();
    size_t viewTypeAt = f.tempU8();

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsStore(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 3)
        return f.fail(call, "Atomics.store must be passed 3 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);
    ParseNode* valueArg = NextNode(indexArg);

    f.writeOp(I32::AtomicsStore);
    size_t needsBoundsCheckAt = f.tempU8();
    size_t viewTypeAt = f.tempU8();

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    Type rhsType;
    if (!CheckExpr(f, valueArg, &rhsType))
        return false;

    if (!rhsType.isIntish())
        return f.failf(arrayArg, "%s is not a subtype of intish", rhsType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = rhsType;
    return true;
}

static bool
CheckAtomicsBinop(FunctionValidator& f, ParseNode* call, Type* type, js::jit::AtomicOp op)
{
    if (CallArgListLength(call) != 3)
        return f.fail(call, "Atomics binary operator must be passed 3 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);
    ParseNode* valueArg = NextNode(indexArg);

    f.writeOp(I32::AtomicsBinOp);
    size_t needsBoundsCheckAt = f.tempU8();
    size_t viewTypeAt = f.tempU8();
    f.writeU8(uint8_t(op));

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    Type valueArgType;
    if (!CheckExpr(f, valueArg, &valueArgType))
        return false;

    if (!valueArgType.isIntish())
        return f.failf(valueArg, "%s is not a subtype of intish", valueArgType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsIsLockFree(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 1)
        return f.fail(call, "Atomics.isLockFree must be passed 1 argument");

    ParseNode* sizeArg = CallArgList(call);

    uint32_t size;
    if (!IsLiteralInt(f.m(), sizeArg, &size))
        return f.fail(sizeArg, "Atomics.isLockFree requires an integer literal argument");

    f.writeInt32Lit(AtomicOperations::isLockfree(size));
    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsCompareExchange(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 4)
        return f.fail(call, "Atomics.compareExchange must be passed 4 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);
    ParseNode* oldValueArg = NextNode(indexArg);
    ParseNode* newValueArg = NextNode(oldValueArg);

    f.writeOp(I32::AtomicsCompareExchange);
    size_t needsBoundsCheckAt = f.tempU8();
    size_t viewTypeAt = f.tempU8();

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    Type oldValueArgType;
    if (!CheckExpr(f, oldValueArg, &oldValueArgType))
        return false;

    Type newValueArgType;
    if (!CheckExpr(f, newValueArg, &newValueArgType))
        return false;

    if (!oldValueArgType.isIntish())
        return f.failf(oldValueArg, "%s is not a subtype of intish", oldValueArgType.toChars());

    if (!newValueArgType.isIntish())
        return f.failf(newValueArg, "%s is not a subtype of intish", newValueArgType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsExchange(FunctionValidator& f, ParseNode* call, Type* type)
{
    if (CallArgListLength(call) != 3)
        return f.fail(call, "Atomics.exchange must be passed 3 arguments");

    ParseNode* arrayArg = CallArgList(call);
    ParseNode* indexArg = NextNode(arrayArg);
    ParseNode* valueArg = NextNode(indexArg);

    f.writeOp(I32::AtomicsExchange);
    size_t needsBoundsCheckAt = f.tempU8();
    size_t viewTypeAt = f.tempU8();

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    int32_t mask;
    if (!CheckSharedArrayAtomicAccess(f, arrayArg, indexArg, &viewType, &needsBoundsCheck, &mask))
        return false;

    Type valueArgType;
    if (!CheckExpr(f, valueArg, &valueArgType))
        return false;

    if (!valueArgType.isIntish())
        return f.failf(arrayArg, "%s is not a subtype of intish", valueArgType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = Type::Int;
    return true;
}

static bool
CheckAtomicsBuiltinCall(FunctionValidator& f, ParseNode* callNode, AsmJSAtomicsBuiltinFunction func,
                        Type* resultType)
{
    switch (func) {
      case AsmJSAtomicsBuiltin_compareExchange:
        return CheckAtomicsCompareExchange(f, callNode, resultType);
      case AsmJSAtomicsBuiltin_exchange:
        return CheckAtomicsExchange(f, callNode, resultType);
      case AsmJSAtomicsBuiltin_load:
        return CheckAtomicsLoad(f, callNode, resultType);
      case AsmJSAtomicsBuiltin_store:
        return CheckAtomicsStore(f, callNode, resultType);
      case AsmJSAtomicsBuiltin_fence:
        return CheckAtomicsFence(f, callNode, resultType);
      case AsmJSAtomicsBuiltin_add:
        return CheckAtomicsBinop(f, callNode, resultType, AtomicFetchAddOp);
      case AsmJSAtomicsBuiltin_sub:
        return CheckAtomicsBinop(f, callNode, resultType, AtomicFetchSubOp);
      case AsmJSAtomicsBuiltin_and:
        return CheckAtomicsBinop(f, callNode, resultType, AtomicFetchAndOp);
      case AsmJSAtomicsBuiltin_or:
        return CheckAtomicsBinop(f, callNode, resultType, AtomicFetchOrOp);
      case AsmJSAtomicsBuiltin_xor:
        return CheckAtomicsBinop(f, callNode, resultType, AtomicFetchXorOp);
      case AsmJSAtomicsBuiltin_isLockFree:
        return CheckAtomicsIsLockFree(f, callNode, resultType);
      default:
        MOZ_CRASH("unexpected atomicsBuiltin function");
    }
}

typedef bool (*CheckArgType)(FunctionValidator& f, ParseNode* argNode, Type type);

static bool
CheckCallArgs(FunctionValidator& f, ParseNode* callNode, CheckArgType checkArg, Signature& signature)
{
    ParseNode* argNode = CallArgList(callNode);
    for (unsigned i = 0; i < CallArgListLength(callNode); i++, argNode = NextNode(argNode)) {
        Type type;
        if (!CheckExpr(f, argNode, &type))
            return false;

        if (!checkArg(f, argNode, type))
            return false;

        if (!signature.appendArg(VarType::FromCheckedType(type)))
            return false;
    }
    return true;
}

static bool
CheckSignatureAgainstExisting(ModuleValidator& m, ParseNode* usepn, const Signature& sig,
                              const Signature& existing)
{
    if (sig.args().length() != existing.args().length()) {
        return m.failf(usepn, "incompatible number of arguments (%u here vs. %u before)",
                       sig.args().length(), existing.args().length());
    }

    for (unsigned i = 0; i < sig.args().length(); i++) {
        if (sig.arg(i) != existing.arg(i)) {
            return m.failf(usepn, "incompatible type for argument %u: (%s here vs. %s before)",
                           i, sig.arg(i).toType().toChars(), existing.arg(i).toType().toChars());
        }
    }

    if (sig.retType() != existing.retType()) {
        return m.failf(usepn, "%s incompatible with previous return of type %s",
                       sig.retType().toType().toChars(), existing.retType().toType().toChars());
    }

    MOZ_ASSERT(sig == existing);
    return true;
}

static bool
CheckFunctionSignature(ModuleValidator& m, ParseNode* usepn, Signature&& sig, PropertyName* name,
                       ModuleValidator::Func** func)
{
    ModuleValidator::Func* existing = m.lookupFunction(name);
    if (!existing) {
        if (!CheckModuleLevelName(m, usepn, name))
            return false;
        return m.addFunction(name, usepn->pn_pos.begin, Move(sig), func);
    }

    if (!CheckSignatureAgainstExisting(m, usepn, sig, existing->sig()))
        return false;

    *func = existing;
    return true;
}

static bool
CheckIsVarType(FunctionValidator& f, ParseNode* argNode, Type type)
{
    if (!type.isVarType())
        return f.failf(argNode, "%s is not a subtype of int, float or double", type.toChars());
    return true;
}

static void
WriteCallLineCol(FunctionValidator& f, ParseNode* pn)
{
    uint32_t line, column;
    f.m().tokenStream().srcCoords.lineNumAndColumnIndex(pn->pn_pos.begin, &line, &column);
    f.writeU32(line);
    f.writeU32(column);
}

static bool
CheckInternalCall(FunctionValidator& f, ParseNode* callNode, PropertyName* calleeName,
                  RetType retType, Type* type)
{
    if (!f.canCall()) {
        return f.fail(callNode, "call expressions may not be nested inside heap expressions "
                                "when the module contains a change-heap function");
    }

    switch (retType.which()) {
        case RetType::Void:      f.writeOp(Stmt::CallInternal);  break;
        case RetType::Signed:    f.writeOp(I32::CallInternal);   break;
        case RetType::Double:    f.writeOp(F64::CallInternal);   break;
        case RetType::Float:     f.writeOp(F32::CallInternal);   break;
        case RetType::Int32x4:   f.writeOp(I32X4::CallInternal); break;
        case RetType::Float32x4: f.writeOp(F32X4::CallInternal); break;
    }

    // Function's index, to find out the function's entry
    size_t funcIndexAt = f.temp32();
    // Function's signature in lifo
    size_t signatureAt = f.tempPtr();
    // Call node position (asm.js specific)
    WriteCallLineCol(f, callNode);

    Signature signature(f.m().lifo(), retType);
    if (!CheckCallArgs(f, callNode, CheckIsVarType, signature))
        return false;

    ModuleValidator::Func* callee;
    if (!CheckFunctionSignature(f.m(), callNode, Move(signature), calleeName, &callee))
        return false;

    f.patch32(funcIndexAt, callee->funcIndex());
    f.patchSignature(signatureAt, &callee->sig());
    *type = retType.toType();
    return true;
}

static bool
CheckFuncPtrTableAgainstExisting(ModuleValidator& m, ParseNode* usepn,
                                 PropertyName* name, Signature&& sig, unsigned mask,
                                 ModuleValidator::FuncPtrTable** tableOut)
{
    if (const ModuleValidator::Global* existing = m.lookupGlobal(name)) {
        if (existing->which() != ModuleValidator::Global::FuncPtrTable)
            return m.failName(usepn, "'%s' is not a function-pointer table", name);

        ModuleValidator::FuncPtrTable& table = m.funcPtrTable(existing->funcPtrTableIndex());
        if (mask != table.mask())
            return m.failf(usepn, "mask does not match previous value (%u)", table.mask());

        if (!CheckSignatureAgainstExisting(m, usepn, sig, table.sig()))
            return false;

        *tableOut = &table;
        return true;
    }

    if (!CheckModuleLevelName(m, usepn, name))
        return false;

    return m.addFuncPtrTable(name, usepn->pn_pos.begin, Move(sig), mask, tableOut);
}

static bool
CheckFuncPtrCall(FunctionValidator& f, ParseNode* callNode, RetType retType, Type* type)
{
    if (!f.canCall()) {
        return f.fail(callNode, "function-pointer call expressions may not be nested inside heap "
                                "expressions when the module contains a change-heap function");
    }

    ParseNode* callee = CallCallee(callNode);
    ParseNode* tableNode = ElemBase(callee);
    ParseNode* indexExpr = ElemIndex(callee);

    if (!tableNode->isKind(PNK_NAME))
        return f.fail(tableNode, "expecting name of function-pointer array");

    PropertyName* name = tableNode->name();
    if (const ModuleValidator::Global* existing = f.lookupGlobal(name)) {
        if (existing->which() != ModuleValidator::Global::FuncPtrTable)
            return f.failName(tableNode, "'%s' is not the name of a function-pointer array", name);
    }

    if (!indexExpr->isKind(PNK_BITAND))
        return f.fail(indexExpr, "function-pointer table index expression needs & mask");

    ParseNode* indexNode = BitwiseLeft(indexExpr);
    ParseNode* maskNode = BitwiseRight(indexExpr);

    uint32_t mask;
    if (!IsLiteralInt(f.m(), maskNode, &mask) || mask == UINT32_MAX || !IsPowerOfTwo(mask + 1))
        return f.fail(maskNode, "function-pointer table index mask value must be a power of two minus 1");

    // Opcode
    switch (retType.which()) {
        case RetType::Void:      f.writeOp(Stmt::CallIndirect);  break;
        case RetType::Signed:    f.writeOp(I32::CallIndirect);   break;
        case RetType::Double:    f.writeOp(F64::CallIndirect);   break;
        case RetType::Float:     f.writeOp(F32::CallIndirect);   break;
        case RetType::Int32x4:   f.writeOp(I32X4::CallIndirect); break;
        case RetType::Float32x4: f.writeOp(F32X4::CallIndirect); break;
    }

    // Table's mask
    f.writeU32(mask);
    // Global data offset
    size_t globalDataOffsetAt = f.temp32();
    // Signature
    size_t signatureAt = f.tempPtr();
    // Call node position (asm.js specific)
    WriteCallLineCol(f, callNode);

    Type indexType;
    if (!CheckExpr(f, indexNode, &indexType))
        return false;

    if (!indexType.isIntish())
        return f.failf(indexNode, "%s is not a subtype of intish", indexType.toChars());

    Signature sig(f.m().lifo(), retType);
    if (!CheckCallArgs(f, callNode, CheckIsVarType, sig))
        return false;

    ModuleValidator::FuncPtrTable* table;
    if (!CheckFuncPtrTableAgainstExisting(f.m(), tableNode, name, Move(sig), mask, &table))
        return false;

    f.patch32(globalDataOffsetAt, table->globalDataOffset());
    f.patchSignature(signatureAt, &table->sig());

    *type = retType.toType();
    return true;
}

static bool
CheckIsExternType(FunctionValidator& f, ParseNode* argNode, Type type)
{
    if (!type.isExtern())
        return f.failf(argNode, "%s is not a subtype of extern", type.toChars());
    return true;
}

static bool
CheckFFICall(FunctionValidator& f, ParseNode* callNode, unsigned ffiIndex, RetType retType,
             Type* type)
{
    if (!f.canCall()) {
        return f.fail(callNode, "FFI call expressions may not be nested inside heap "
                                "expressions when the module contains a change-heap function");
    }

    PropertyName* calleeName = CallCallee(callNode)->name();

    if (retType == RetType::Float)
        return f.fail(callNode, "FFI calls can't return float");
    if (retType.toType().isSimd())
        return f.fail(callNode, "FFI calls can't return SIMD values");

    switch (retType.which()) {
        case RetType::Void:      f.writeOp(Stmt::CallImport);  break;
        case RetType::Signed:    f.writeOp(I32::CallImport);   break;
        case RetType::Double:    f.writeOp(F64::CallImport);   break;
        case RetType::Float:     f.writeOp(F32::CallImport);   break;
        case RetType::Int32x4:   f.writeOp(I32X4::CallImport); break;
        case RetType::Float32x4: f.writeOp(F32X4::CallImport); break;
    }

    // Global data offset
    size_t offsetAt = f.temp32();
    // Pointer to the exit's signature in the module's lifo
    size_t sigAt = f.tempPtr();
    // Call node position (asm.js specific)
    WriteCallLineCol(f, callNode);

    Signature signature(f.m().lifo(), retType);
    if (!CheckCallArgs(f, callNode, CheckIsExternType, signature))
        return false;

    unsigned exitIndex;
    const LifoSignature* lifoSig = nullptr;
    if (!f.m().addExit(ffiIndex, calleeName, Move(signature), &exitIndex, &lifoSig))
        return false;

    MOZ_ASSERT(!!lifoSig);

    JS_STATIC_ASSERT(offsetof(AsmJSModule::ExitDatum, exit) == 0);
    f.patch32(offsetAt, f.module().exitIndexToGlobalDataOffset(exitIndex));
    f.patchSignature(sigAt, lifoSig);
    *type = retType.toType();
    return true;
}

static bool
CheckFloatCoercionArg(FunctionValidator& f, ParseNode* inputNode, Type inputType,
                      size_t opcodeAt)
{
    if (inputType.isMaybeDouble()) {
        f.patchOp(opcodeAt, F32::FromF64);
        return true;
    }
    if (inputType.isSigned()) {
        f.patchOp(opcodeAt, F32::FromS32);
        return true;
    }
    if (inputType.isUnsigned()) {
        f.patchOp(opcodeAt, F32::FromU32);
        return true;
    }
    if (inputType.isFloatish()) {
        f.patchOp(opcodeAt, F32::Id);
        return true;
    }

    return f.failf(inputNode, "%s is not a subtype of signed, unsigned, double? or floatish",
                   inputType.toChars());
}

static bool
CheckCoercedCall(FunctionValidator& f, ParseNode* call, RetType retType, Type* type);

static bool
CheckCoercionArg(FunctionValidator& f, ParseNode* arg, AsmJSCoercion expected, Type* type)
{
    RetType retType(expected);
    if (arg->isKind(PNK_CALL))
        return CheckCoercedCall(f, arg, retType, type);

    size_t opcodeAt = f.tempOp();

    Type argType;
    if (!CheckExpr(f, arg, &argType))
        return false;

    switch (expected) {
      case AsmJS_FRound:
        if (!CheckFloatCoercionArg(f, arg, argType, opcodeAt))
            return false;
        break;
      case AsmJS_ToInt32x4:
        if (!argType.isInt32x4())
            return f.fail(arg, "argument to SIMD int32x4 coercion isn't int32x4");
        f.patchOp(opcodeAt, I32X4::Id);
        break;
      case AsmJS_ToFloat32x4:
        if (!argType.isFloat32x4())
            return f.fail(arg, "argument to SIMD float32x4 coercion isn't float32x4");
        f.patchOp(opcodeAt, F32X4::Id);
        break;
      case AsmJS_ToInt32:
      case AsmJS_ToNumber:
        MOZ_CRASH("not call coercions");
    }

    *type = retType.toType();
    return true;
}

static bool
CheckMathFRound(FunctionValidator& f, ParseNode* callNode, Type* type)
{
    if (CallArgListLength(callNode) != 1)
        return f.fail(callNode, "Math.fround must be passed 1 argument");

    ParseNode* argNode = CallArgList(callNode);
    Type argType;
    if (!CheckCoercionArg(f, argNode, AsmJS_FRound, &argType))
        return false;

    MOZ_ASSERT(argType == Type::Float);
    *type = Type::Float;
    return true;
}

static bool
CheckMathBuiltinCall(FunctionValidator& f, ParseNode* callNode, AsmJSMathBuiltinFunction func,
                     Type* type)
{
    unsigned arity = 0;
    F32 f32;
    F64 f64;
    switch (func) {
      case AsmJSMathBuiltin_imul:   return CheckMathIMul(f, callNode, type);
      case AsmJSMathBuiltin_clz32:  return CheckMathClz32(f, callNode, type);
      case AsmJSMathBuiltin_abs:    return CheckMathAbs(f, callNode, type);
      case AsmJSMathBuiltin_sqrt:   return CheckMathSqrt(f, callNode, type);
      case AsmJSMathBuiltin_fround: return CheckMathFRound(f, callNode, type);
      case AsmJSMathBuiltin_min:    return CheckMathMinMax(f, callNode, /* isMax = */ false, type);
      case AsmJSMathBuiltin_max:    return CheckMathMinMax(f, callNode, /* isMax = */ true, type);
      case AsmJSMathBuiltin_ceil:   arity = 1; f64 = F64::Ceil;  f32 = F32::Ceil;  break;
      case AsmJSMathBuiltin_floor:  arity = 1; f64 = F64::Floor; f32 = F32::Floor; break;
      case AsmJSMathBuiltin_sin:    arity = 1; f64 = F64::Sin;   f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_cos:    arity = 1; f64 = F64::Cos;   f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_tan:    arity = 1; f64 = F64::Tan;   f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_asin:   arity = 1; f64 = F64::Asin;  f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_acos:   arity = 1; f64 = F64::Acos;  f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_atan:   arity = 1; f64 = F64::Atan;  f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_exp:    arity = 1; f64 = F64::Exp;   f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_log:    arity = 1; f64 = F64::Log;   f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_pow:    arity = 2; f64 = F64::Pow;   f32 = F32::Bad;   break;
      case AsmJSMathBuiltin_atan2:  arity = 2; f64 = F64::Atan2; f32 = F32::Bad;   break;
      default: MOZ_CRASH("unexpected mathBuiltin function");
    }

    unsigned actualArity = CallArgListLength(callNode);
    if (actualArity != arity)
        return f.failf(callNode, "call passed %u arguments, expected %u", actualArity, arity);

    size_t opcodeAt = f.tempOp();
    // Call node position (asm.js specific)
    WriteCallLineCol(f, callNode);

    Type firstType;
    ParseNode* argNode = CallArgList(callNode);
    if (!CheckExpr(f, argNode, &firstType))
        return false;

    if (!firstType.isMaybeFloat() && !firstType.isMaybeDouble())
        return f.fail(argNode, "arguments to math call should be a subtype of double? or float?");

    bool opIsDouble = firstType.isMaybeDouble();
    if (!opIsDouble && f32 == F32::Bad)
        return f.fail(callNode, "math builtin cannot be used as float");

    if (opIsDouble)
        f.patchOp(opcodeAt, f64);
    else
        f.patchOp(opcodeAt, f32);

    if (arity == 2) {
        Type secondType;
        argNode = NextNode(argNode);
        if (!CheckExpr(f, argNode, &secondType))
            return false;

        if (firstType.isMaybeDouble() && !secondType.isMaybeDouble())
            return f.fail(argNode, "both arguments to math builtin call should be the same type");
        if (firstType.isMaybeFloat() && !secondType.isMaybeFloat())
            return f.fail(argNode, "both arguments to math builtin call should be the same type");
    }

    *type = opIsDouble ? Type::Double : Type::Floatish;
    return true;
}

namespace {
// Include CheckSimdCallArgs in unnamed namespace to avoid MSVC name lookup bug.

template<class CheckArgOp>
static bool
CheckSimdCallArgs(FunctionValidator& f, ParseNode* call, unsigned expectedArity,
                  const CheckArgOp& checkArg)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != expectedArity)
        return f.failf(call, "expected %u arguments to SIMD call, got %u", expectedArity, numArgs);

    ParseNode* arg = CallArgList(call);
    for (size_t i = 0; i < numArgs; i++, arg = NextNode(arg)) {
        MOZ_ASSERT(!!arg);
        Type argType;
        if (!CheckExpr(f, arg, &argType))
            return false;
        if (!checkArg(f, arg, i, argType))
            return false;
    }

    return true;
}

template<class CheckArgOp>
static bool
CheckSimdCallArgsPatchable(FunctionValidator& f, ParseNode* call, unsigned expectedArity,
                           const CheckArgOp& checkArg)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != expectedArity)
        return f.failf(call, "expected %u arguments to SIMD call, got %u", expectedArity, numArgs);

    ParseNode* arg = CallArgList(call);
    for (size_t i = 0; i < numArgs; i++, arg = NextNode(arg)) {
        MOZ_ASSERT(!!arg);
        Type argType;
        size_t patchAt = f.tempOp();
        if (!CheckExpr(f, arg, &argType))
            return false;
        if (!checkArg(f, arg, i, argType, patchAt))
            return false;
    }

    return true;
}


class CheckArgIsSubtypeOf
{
    Type formalType_;

  public:
    explicit CheckArgIsSubtypeOf(AsmJSSimdType t) : formalType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType) const
    {
        if (!(actualType <= formalType_)) {
            return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                           formalType_.toChars());
        }
        return true;
    }
};

static inline Type
SimdToCoercedScalarType(AsmJSSimdType t)
{
    switch (t) {
      case AsmJSSimdType_int32x4:
        return Type::Intish;
      case AsmJSSimdType_float32x4:
        return Type::Floatish;
    }
    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected SIMD type");
}

class CheckSimdScalarArgs
{
    AsmJSSimdType simdType_;
    Type formalType_;

  public:
    explicit CheckSimdScalarArgs(AsmJSSimdType simdType)
      : simdType_(simdType), formalType_(SimdToCoercedScalarType(simdType))
    {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType,
                    size_t patchAt) const
    {
        if (!(actualType <= formalType_)) {
            // As a special case, accept doublelit arguments to float32x4 ops by
            // re-emitting them as float32 constants.
            if (simdType_ != AsmJSSimdType_float32x4 || !actualType.isDoubleLit()) {
                return f.failf(arg, "%s is not a subtype of %s%s",
                               actualType.toChars(), formalType_.toChars(),
                               simdType_ == AsmJSSimdType_float32x4 ? " or doublelit" : "");
            }

            // We emitted a double literal and actually want a float32.
            MOZ_ASSERT(patchAt != size_t(-1));
            f.patchOp(patchAt, F32::FromF64);
            return true;
        }

        if (patchAt == size_t(-1))
            return true;

        switch (simdType_) {
          case AsmJSSimdType_int32x4:   f.patchOp(patchAt, I32::Id); return true;
          case AsmJSSimdType_float32x4: f.patchOp(patchAt, F32::Id); return true;
        }

        MOZ_CRASH("unexpected simd type");
    }
};

class CheckSimdSelectArgs
{
    Type formalType_;

  public:
    explicit CheckSimdSelectArgs(AsmJSSimdType t) : formalType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType) const
    {
        if (argIndex == 0) {
            // First argument of select is an int32x4 mask.
            if (!(actualType <= Type::Int32x4))
                return f.failf(arg, "%s is not a subtype of Int32x4", actualType.toChars());
            return true;
        }

        if (!(actualType <= formalType_)) {
            return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                           formalType_.toChars());
        }
        return true;
    }
};

class CheckSimdVectorScalarArgs
{
    AsmJSSimdType formalSimdType_;

  public:
    explicit CheckSimdVectorScalarArgs(AsmJSSimdType t) : formalSimdType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType,
                    size_t patchAt = -1) const
    {
        MOZ_ASSERT(argIndex < 2);
        if (argIndex == 0) {
            // First argument is the vector
            if (!(actualType <= Type(formalSimdType_))) {
                return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                               Type(formalSimdType_).toChars());
            }

            if (patchAt == size_t(-1))
                return true;

            switch (formalSimdType_) {
              case AsmJSSimdType_int32x4:   f.patchOp(patchAt, I32X4::Id); return true;
              case AsmJSSimdType_float32x4: f.patchOp(patchAt, F32X4::Id); return true;
            }

            MOZ_CRASH("unexpected simd type");
        }

        // Second argument is the scalar
        return CheckSimdScalarArgs(formalSimdType_)(f, arg, argIndex, actualType, patchAt);
    }
};

class CheckSimdExtractLaneArgs
{
    AsmJSSimdType formalSimdType_;

  public:
    explicit CheckSimdExtractLaneArgs(AsmJSSimdType t) : formalSimdType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType) const
    {
        MOZ_ASSERT(argIndex < 2);
        if (argIndex == 0) {
            // First argument is the vector
            if (!(actualType <= Type(formalSimdType_))) {
                return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                               Type(formalSimdType_).toChars());
            }
            return true;
        }

        uint32_t laneIndex;
        // Second argument is the lane < vector length
        if (!IsLiteralOrConstInt(f, arg, &laneIndex))
            return f.failf(arg, "lane selector should be a constant integer literal");
        if (laneIndex >= SimdTypeToLength(formalSimdType_))
            return f.failf(arg, "lane selector should be in bounds");
        return true;
    }
};

class CheckSimdReplaceLaneArgs
{
    AsmJSSimdType formalSimdType_;

  public:
    explicit CheckSimdReplaceLaneArgs(AsmJSSimdType t) : formalSimdType_(t) {}

    bool operator()(FunctionValidator& f, ParseNode* arg, unsigned argIndex, Type actualType,
                    size_t patchAt) const
    {
        MOZ_ASSERT(argIndex < 3);
        uint32_t u32;
        switch (argIndex) {
          case 0:
            // First argument is the vector
            if (!(actualType <= Type(formalSimdType_))) {
                return f.failf(arg, "%s is not a subtype of %s", actualType.toChars(),
                               Type(formalSimdType_).toChars());
            }
            switch (formalSimdType_) {
              case AsmJSSimdType_int32x4:   f.patchOp(patchAt, I32X4::Id); break;
              case AsmJSSimdType_float32x4: f.patchOp(patchAt, F32X4::Id); break;
            }
            return true;
          case 1:
            // Second argument is the lane (< vector length).
            if (!IsLiteralOrConstInt(f, arg, &u32))
                return f.failf(arg, "lane selector should be a constant integer literal");
            if (u32 >= SimdTypeToLength(formalSimdType_))
                return f.failf(arg, "lane selector should be in bounds");
            f.patchOp(patchAt, I32::Id);
            return true;
          case 2:
            // Third argument is the scalar
            return CheckSimdScalarArgs(formalSimdType_)(f, arg, argIndex, actualType, patchAt);
        }
        return false;
    }
};

} // namespace

static void
SwitchPackOp(FunctionValidator& f, AsmJSSimdType type, I32X4 i32x4, F32X4 f32x4)
{
    switch (type) {
      case AsmJSSimdType_int32x4:   f.writeOp(i32x4); return;
      case AsmJSSimdType_float32x4: f.writeOp(f32x4); return;
    }
    MOZ_CRASH("unexpected simd type");
}

static bool
CheckSimdUnary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
               MSimdUnaryArith::Operation op, Type* type)
{
    SwitchPackOp(f, opType, I32X4::Unary, F32X4::Unary);
    f.writeU8(uint8_t(op));
    if (!CheckSimdCallArgs(f, call, 1, CheckArgIsSubtypeOf(opType)))
        return false;
    *type = opType;
    return true;
}

template<class OpKind>
inline bool
CheckSimdBinaryGuts(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, OpKind op,
                    Type* type)
{
    f.writeU8(uint8_t(op));
    if (!CheckSimdCallArgs(f, call, 2, CheckArgIsSubtypeOf(opType)))
        return false;
    *type = opType;
    return true;
}

static bool
CheckSimdBinary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                MSimdBinaryArith::Operation op, Type* type)
{
    SwitchPackOp(f, opType, I32X4::Binary, F32X4::Binary);
    return CheckSimdBinaryGuts(f, call, opType, op, type);
}

static bool
CheckSimdBinary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                MSimdBinaryBitwise::Operation op, Type* type)
{
    SwitchPackOp(f, opType, I32X4::BinaryBitwise, F32X4::BinaryBitwise);
    return CheckSimdBinaryGuts(f, call, opType, op, type);
}

static bool
CheckSimdBinary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                MSimdBinaryComp::Operation op, Type* type)
{
    switch (opType) {
      case AsmJSSimdType_int32x4:   f.writeOp(I32X4::BinaryCompI32X4); break;
      case AsmJSSimdType_float32x4: f.writeOp(I32X4::BinaryCompF32X4); break;
    }
    f.writeU8(uint8_t(op));
    if (!CheckSimdCallArgs(f, call, 2, CheckArgIsSubtypeOf(opType)))
        return false;
    *type = Type::Int32x4;
    return true;
}

static bool
CheckSimdBinary(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                MSimdShift::Operation op, Type* type)
{
    f.writeOp(I32X4::BinaryShift);
    f.writeU8(uint8_t(op));
    if (!CheckSimdCallArgs(f, call, 2, CheckSimdVectorScalarArgs(opType)))
        return false;
    *type = Type::Int32x4;
    return true;
}

static bool
CheckSimdExtractLane(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    switch (opType) {
      case AsmJSSimdType_int32x4:
        f.writeOp(I32::I32X4ExtractLane);
        *type = Type::Signed;
        break;
      case AsmJSSimdType_float32x4:
        f.writeOp(F32::F32X4ExtractLane);
        *type = Type::Float;
        break;
    }
    return CheckSimdCallArgs(f, call, 2, CheckSimdExtractLaneArgs(opType));
}

static bool
CheckSimdReplaceLane(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    SwitchPackOp(f, opType, I32X4::ReplaceLane, F32X4::ReplaceLane);
    if (!CheckSimdCallArgsPatchable(f, call, 3, CheckSimdReplaceLaneArgs(opType)))
        return false;
    *type = opType;
    return true;
}

typedef bool IsBitCast;

namespace {
// Include CheckSimdCast in unnamed namespace to avoid MSVC name lookup bug (due to the use of Type).

static bool
CheckSimdCast(FunctionValidator& f, ParseNode* call, AsmJSSimdType fromType, AsmJSSimdType toType,
              bool bitcast, Type* type)
{
    SwitchPackOp(f, toType,
                 bitcast ? I32X4::FromF32X4Bits : I32X4::FromF32X4,
                 bitcast ? F32X4::FromI32X4Bits : F32X4::FromI32X4);
    if (!CheckSimdCallArgs(f, call, 1, CheckArgIsSubtypeOf(fromType)))
        return false;
    *type = toType;
    return true;
}

} // namespace

static bool
CheckSimdShuffleSelectors(FunctionValidator& f, ParseNode* lane, int32_t lanes[4], uint32_t maxLane)
{
    for (unsigned i = 0; i < 4; i++, lane = NextNode(lane)) {
        uint32_t u32;
        if (!IsLiteralInt(f.m(), lane, &u32))
            return f.failf(lane, "lane selector should be a constant integer literal");
        if (u32 >= maxLane)
            return f.failf(lane, "lane selector should be less than %u", maxLane);
        lanes[i] = int32_t(u32);
    }
    return true;
}

static bool
CheckSimdSwizzle(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != 5)
        return f.failf(call, "expected 5 arguments to SIMD swizzle, got %u", numArgs);

    SwitchPackOp(f, opType, I32X4::Swizzle, F32X4::Swizzle);

    Type retType = opType;
    ParseNode* vec = CallArgList(call);
    Type vecType;
    if (!CheckExpr(f, vec, &vecType))
        return false;
    if (!(vecType <= retType))
        return f.failf(vec, "%s is not a subtype of %s", vecType.toChars(), retType.toChars());

    int32_t lanes[4];
    if (!CheckSimdShuffleSelectors(f, NextNode(vec), lanes, 4))
        return false;

    for (unsigned i = 0; i < 4; i++)
        f.writeU8(uint8_t(lanes[i]));

    *type = retType;
    return true;
}

static bool
CheckSimdShuffle(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != 6)
        return f.failf(call, "expected 6 arguments to SIMD shuffle, got %u", numArgs);

    SwitchPackOp(f, opType, I32X4::Shuffle, F32X4::Shuffle);

    Type retType = opType;
    ParseNode* arg = CallArgList(call);
    for (unsigned i = 0; i < 2; i++, arg = NextNode(arg)) {
        Type type;
        if (!CheckExpr(f, arg, &type))
            return false;
        if (!(type <= retType))
            return f.failf(arg, "%s is not a subtype of %s", type.toChars(), retType.toChars());
    }

    int32_t lanes[4];
    if (!CheckSimdShuffleSelectors(f, arg, lanes, 8))
        return false;

    for (unsigned i = 0; i < 4; i++)
        f.writeU8(uint8_t(lanes[i]));

    *type = retType;
    return true;
}

static bool
CheckSimdLoadStoreArgs(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
                       Scalar::Type* viewType, NeedsBoundsCheck* needsBoundsCheck)
{
    ParseNode* view = CallArgList(call);
    if (!view->isKind(PNK_NAME))
        return f.fail(view, "expected Uint8Array view as SIMD.*.load/store first argument");

    const ModuleValidator::Global* global = f.lookupGlobal(view->name());
    if (!global ||
        global->which() != ModuleValidator::Global::ArrayView ||
        global->viewType() != Scalar::Uint8)
    {
        return f.fail(view, "expected Uint8Array view as SIMD.*.load/store first argument");
    }

    *needsBoundsCheck = NEEDS_BOUNDS_CHECK;

    switch (opType) {
      case AsmJSSimdType_int32x4:   *viewType = Scalar::Int32x4;   break;
      case AsmJSSimdType_float32x4: *viewType = Scalar::Float32x4; break;
    }

    ParseNode* indexExpr = NextNode(view);
    uint32_t indexLit;
    if (IsLiteralOrConstInt(f, indexExpr, &indexLit)) {
        if (indexLit > INT32_MAX)
            return f.fail(indexExpr, "constant index out of range");

        if (!f.m().tryRequireHeapLengthToBeAtLeast(indexLit + Simd128DataSize)) {
            return f.failf(indexExpr, "constant index outside heap size range declared by the "
                                      "change-heap function (0x%x - 0x%x)",
                                      f.m().minHeapLength(), f.m().module().maxHeapLength());
        }

        *needsBoundsCheck = NO_BOUNDS_CHECK;
        f.writeInt32Lit(indexLit);
        return true;
    }

    f.enterHeapExpression();

    Type indexType;
    if (!CheckExpr(f, indexExpr, &indexType))
        return false;
    if (!indexType.isIntish())
        return f.failf(indexExpr, "%s is not a subtype of intish", indexType.toChars());

    f.leaveHeapExpression();

    return true;
}

static bool
CheckSimdLoad(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
              unsigned numElems, Type* type)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != 2)
        return f.failf(call, "expected 2 arguments to SIMD load, got %u", numArgs);

    SwitchPackOp(f, opType, I32X4::Load, F32X4::Load);
    size_t viewTypeAt = f.tempU8();
    size_t needsBoundsCheckAt = f.tempU8();
    f.writeU8(numElems);

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    if (!CheckSimdLoadStoreArgs(f, call, opType, &viewType, &needsBoundsCheck))
        return false;

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = opType;
    return true;
}

static bool
CheckSimdStore(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType,
               unsigned numElems, Type* type)
{
    unsigned numArgs = CallArgListLength(call);
    if (numArgs != 3)
        return f.failf(call, "expected 3 arguments to SIMD store, got %u", numArgs);

    SwitchPackOp(f, opType, I32X4::Store, F32X4::Store);
    size_t viewTypeAt = f.tempU8();
    size_t needsBoundsCheckAt = f.tempU8();
    f.writeU8(numElems);

    Scalar::Type viewType;
    NeedsBoundsCheck needsBoundsCheck;
    if (!CheckSimdLoadStoreArgs(f, call, opType, &viewType, &needsBoundsCheck))
        return false;

    Type retType = opType;
    ParseNode* vecExpr = NextNode(NextNode(CallArgList(call)));
    Type vecType;
    if (!CheckExpr(f, vecExpr, &vecType))
        return false;
    if (!(vecType <= retType))
        return f.failf(vecExpr, "%s is not a subtype of %s", vecType.toChars(), retType.toChars());

    f.patchU8(needsBoundsCheckAt, uint8_t(needsBoundsCheck));
    f.patchU8(viewTypeAt, uint8_t(viewType));

    *type = vecType;
    return true;
}

static bool
CheckSimdSelect(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, bool isElementWise,
                Type* type)
{
    SwitchPackOp(f, opType,
                 isElementWise ? I32X4::Select : I32X4::BitSelect,
                 isElementWise ? F32X4::Select : F32X4::BitSelect);
    if (!CheckSimdCallArgs(f, call, 3, CheckSimdSelectArgs(opType)))
        return false;
    *type = opType;
    return true;
}

static bool
CheckSimdCheck(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    AsmJSCoercion coercion;
    ParseNode* argNode;
    if (!IsCoercionCall(f.m(), call, &coercion, &argNode))
        return f.failf(call, "expected 1 argument in call to check");
    return CheckCoercionArg(f, argNode, coercion, type);
}

static bool
CheckSimdSplat(FunctionValidator& f, ParseNode* call, AsmJSSimdType opType, Type* type)
{
    SwitchPackOp(f, opType, I32X4::Splat, F32X4::Splat);
    if (!CheckSimdCallArgsPatchable(f, call, 1, CheckSimdScalarArgs(opType)))
        return false;
    *type = opType;
    return true;
}

static bool
CheckSimdOperationCall(FunctionValidator& f, ParseNode* call, const ModuleValidator::Global* global,
                       Type* type)
{
    MOZ_ASSERT(global->isSimdOperation());

    AsmJSSimdType opType = global->simdOperationType();

    switch (global->simdOperation()) {
      case AsmJSSimdOperation_check:
        return CheckSimdCheck(f, call, opType, type);

#define OP_CHECK_CASE_LIST_(OP)                                                         \
      case AsmJSSimdOperation_##OP:                                                     \
        return CheckSimdBinary(f, call, opType, MSimdBinaryArith::Op_##OP, type);
      ARITH_COMMONX4_SIMD_OP(OP_CHECK_CASE_LIST_)
      BINARY_ARITH_FLOAT32X4_SIMD_OP(OP_CHECK_CASE_LIST_)
#undef OP_CHECK_CASE_LIST_

      case AsmJSSimdOperation_lessThan:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::lessThan, type);
      case AsmJSSimdOperation_lessThanOrEqual:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::lessThanOrEqual, type);
      case AsmJSSimdOperation_equal:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::equal, type);
      case AsmJSSimdOperation_notEqual:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::notEqual, type);
      case AsmJSSimdOperation_greaterThan:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::greaterThan, type);
      case AsmJSSimdOperation_greaterThanOrEqual:
        return CheckSimdBinary(f, call, opType, MSimdBinaryComp::greaterThanOrEqual, type);

      case AsmJSSimdOperation_and:
        return CheckSimdBinary(f, call, opType, MSimdBinaryBitwise::and_, type);
      case AsmJSSimdOperation_or:
        return CheckSimdBinary(f, call, opType, MSimdBinaryBitwise::or_, type);
      case AsmJSSimdOperation_xor:
        return CheckSimdBinary(f, call, opType, MSimdBinaryBitwise::xor_, type);

      case AsmJSSimdOperation_extractLane:
        return CheckSimdExtractLane(f, call, opType, type);
      case AsmJSSimdOperation_replaceLane:
        return CheckSimdReplaceLane(f, call, opType, type);

      case AsmJSSimdOperation_fromInt32x4:
        return CheckSimdCast(f, call, AsmJSSimdType_int32x4, opType, IsBitCast(false), type);
      case AsmJSSimdOperation_fromFloat32x4:
        return CheckSimdCast(f, call, AsmJSSimdType_float32x4, opType, IsBitCast(false), type);
      case AsmJSSimdOperation_fromInt32x4Bits:
        return CheckSimdCast(f, call, AsmJSSimdType_int32x4, opType, IsBitCast(true), type);
      case AsmJSSimdOperation_fromFloat32x4Bits:
        return CheckSimdCast(f, call, AsmJSSimdType_float32x4, opType, IsBitCast(true), type);

      case AsmJSSimdOperation_shiftLeftByScalar:
        return CheckSimdBinary(f, call, opType, MSimdShift::lsh, type);
      case AsmJSSimdOperation_shiftRightArithmeticByScalar:
        return CheckSimdBinary(f, call, opType, MSimdShift::rsh, type);
      case AsmJSSimdOperation_shiftRightLogicalByScalar:
        return CheckSimdBinary(f, call, opType, MSimdShift::ursh, type);

      case AsmJSSimdOperation_abs:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::abs, type);
      case AsmJSSimdOperation_neg:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::neg, type);
      case AsmJSSimdOperation_not:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::not_, type);
      case AsmJSSimdOperation_sqrt:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::sqrt, type);
      case AsmJSSimdOperation_reciprocalApproximation:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::reciprocalApproximation, type);
      case AsmJSSimdOperation_reciprocalSqrtApproximation:
        return CheckSimdUnary(f, call, opType, MSimdUnaryArith::reciprocalSqrtApproximation, type);

      case AsmJSSimdOperation_swizzle:
        return CheckSimdSwizzle(f, call, opType, type);
      case AsmJSSimdOperation_shuffle:
        return CheckSimdShuffle(f, call, opType, type);

      case AsmJSSimdOperation_load:
        return CheckSimdLoad(f, call, opType, 4, type);
      case AsmJSSimdOperation_load1:
        return CheckSimdLoad(f, call, opType, 1, type);
      case AsmJSSimdOperation_load2:
        return CheckSimdLoad(f, call, opType, 2, type);
      case AsmJSSimdOperation_load3:
        return CheckSimdLoad(f, call, opType, 3, type);
      case AsmJSSimdOperation_store:
        return CheckSimdStore(f, call, opType, 4, type);
      case AsmJSSimdOperation_store1:
        return CheckSimdStore(f, call, opType, 1, type);
      case AsmJSSimdOperation_store2:
        return CheckSimdStore(f, call, opType, 2, type);
      case AsmJSSimdOperation_store3:
        return CheckSimdStore(f, call, opType, 3, type);

      case AsmJSSimdOperation_selectBits:
        return CheckSimdSelect(f, call, opType, /*isElementWise */ false, type);
      case AsmJSSimdOperation_select:
        return CheckSimdSelect(f, call, opType, /*isElementWise */ true, type);

      case AsmJSSimdOperation_splat:
        return CheckSimdSplat(f, call, opType, type);
    }
    MOZ_CRASH("unexpected simd operation in CheckSimdOperationCall");
}

static bool
CheckSimdCtorCall(FunctionValidator& f, ParseNode* call, const ModuleValidator::Global* global,
                  Type* type)
{
    MOZ_ASSERT(call->isKind(PNK_CALL));

    AsmJSSimdType simdType = global->simdCtorType();
    SwitchPackOp(f, simdType, I32X4::Ctor, F32X4::Ctor);

    unsigned length = SimdTypeToLength(simdType);
    if (!CheckSimdCallArgsPatchable(f, call, length, CheckSimdScalarArgs(simdType)))
        return false;

    *type = simdType;
    return true;
}

static bool
CheckUncoercedCall(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_CALL));

    const ModuleValidator::Global* global;
    if (IsCallToGlobal(f.m(), expr, &global)) {
        if (global->isMathFunction())
            return CheckMathBuiltinCall(f, expr, global->mathBuiltinFunction(), type);
        if (global->isAtomicsFunction())
            return CheckAtomicsBuiltinCall(f, expr, global->atomicsBuiltinFunction(), type);
        if (global->isSimdCtor())
            return CheckSimdCtorCall(f, expr, global, type);
        if (global->isSimdOperation())
            return CheckSimdOperationCall(f, expr, global, type);
    }

    return f.fail(expr, "all function calls must either be calls to standard lib math functions, "
                        "standard atomic functions, standard SIMD constructors or operations, "
                        "ignored (via f(); or comma-expression), coerced to signed (via f()|0), "
                        "coerced to float (via fround(f())) or coerced to double (via +f())");
}

static bool
CoerceResult(FunctionValidator& f, ParseNode* expr, RetType expected, Type resultType,
             size_t patchAt, Type* type)
{
    // At this point, the bytecode resembles this:
    //      | patchAt | the thing we wanted to coerce | current position |>
    switch (expected.which()) {
      case RetType::Void: {
        if (resultType.isIntish())
            f.patchOp(patchAt, Stmt::I32Expr);
        else if (resultType.isFloatish())
            f.patchOp(patchAt, Stmt::F32Expr);
        else if (resultType.isMaybeDouble())
            f.patchOp(patchAt, Stmt::F64Expr);
        else if (resultType.isInt32x4())
            f.patchOp(patchAt, Stmt::I32X4Expr);
        else if (resultType.isFloat32x4())
            f.patchOp(patchAt, Stmt::F32X4Expr);
        else if (resultType.isVoid())
            f.patchOp(patchAt, Stmt::Id);
        else
            MOZ_CRASH("unhandled return type");
        *type = Type::Void;
        return true;
      }

      case RetType::Signed: {
        if (!resultType.isIntish())
            return f.failf(expr, "%s is not a subtype of intish", resultType.toChars());
        f.patchOp(patchAt, I32::Id);
        *type = Type::Signed;
        return true;
      }

      case RetType::Double: {
        *type = Type::Double;
        if (resultType.isMaybeDouble()) {
            f.patchOp(patchAt, F64::Id);
            return true;
        }
        if (resultType.isMaybeFloat()) {
            f.patchOp(patchAt, F64::FromF32);
            return true;
        }
        if (resultType.isSigned()) {
            f.patchOp(patchAt, F64::FromS32);
            return true;
        }
        if (resultType.isUnsigned()) {
            f.patchOp(patchAt, F64::FromU32);
            return true;
        }
        return f.failf(expr, "%s is not a subtype of double?, float?, signed or unsigned",
                       resultType.toChars());
      }

      case RetType::Float: {
        if (!CheckFloatCoercionArg(f, expr, resultType, patchAt))
            return false;
        *type = Type::Float;
        return true;
      }

      case RetType::Int32x4: {
        if (!resultType.isInt32x4())
            return f.failf(expr, "%s is not a subtype of int32x4", resultType.toChars());
        f.patchOp(patchAt, I32X4::Id);
        *type = Type::Int32x4;
        return true;
      }

      case RetType::Float32x4: {
        if (!resultType.isFloat32x4())
            return f.failf(expr, "%s is not a subtype of float32x4", resultType.toChars());
        f.patchOp(patchAt, F32X4::Id);
        *type = Type::Float32x4;
        return true;
      }
    }

    return true;
}

static bool
CheckCoercedMathBuiltinCall(FunctionValidator& f, ParseNode* callNode, AsmJSMathBuiltinFunction func,
                            RetType retType, Type* type)
{
    size_t opcodeAt = f.tempOp();
    Type resultType;
    if (!CheckMathBuiltinCall(f, callNode, func, &resultType))
        return false;
    return CoerceResult(f, callNode, retType, resultType, opcodeAt, type);
}

static bool
CheckCoercedSimdCall(FunctionValidator& f, ParseNode* call, const ModuleValidator::Global* global,
                     RetType retType, Type* type)
{
    size_t opcodeAt = f.tempOp();

    if (global->isSimdCtor()) {
        if (!CheckSimdCtorCall(f, call, global, type))
            return false;
        MOZ_ASSERT(type->isSimd());
    } else {
        MOZ_ASSERT(global->isSimdOperation());
        if (!CheckSimdOperationCall(f, call, global, type))
            return false;
        MOZ_ASSERT_IF(global->simdOperation() != AsmJSSimdOperation_extractLane, type->isSimd());
    }

    return CoerceResult(f, call, retType, *type, opcodeAt, type);
}

static bool
CheckCoercedAtomicsBuiltinCall(FunctionValidator& f, ParseNode* callNode,
                               AsmJSAtomicsBuiltinFunction func, RetType retType,
                               Type* resultType)
{
    size_t opcodeAt = f.tempOp();
    Type actualRetType;
    if (!CheckAtomicsBuiltinCall(f, callNode, func, &actualRetType))
        return false;
    return CoerceResult(f, callNode, retType, actualRetType, opcodeAt, resultType);
}

static bool
CheckCoercedCall(FunctionValidator& f, ParseNode* call, RetType retType, Type* type)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    if (IsNumericLiteral(f.m(), call)) {
        size_t coerceOp = f.tempOp();
        AsmJSNumLit literal = ExtractNumericLiteral(f.m(), call);
        f.writeLit(literal);
        return CoerceResult(f, call, retType, Type::Of(literal), coerceOp, type);
    }

    ParseNode* callee = CallCallee(call);

    if (callee->isKind(PNK_ELEM))
        return CheckFuncPtrCall(f, call, retType, type);

    if (!callee->isKind(PNK_NAME))
        return f.fail(callee, "unexpected callee expression type");

    PropertyName* calleeName = callee->name();

    if (const ModuleValidator::Global* global = f.lookupGlobal(calleeName)) {
        switch (global->which()) {
          case ModuleValidator::Global::FFI:
            return CheckFFICall(f, call, global->ffiIndex(), retType, type);
          case ModuleValidator::Global::MathBuiltinFunction:
            return CheckCoercedMathBuiltinCall(f, call, global->mathBuiltinFunction(), retType, type);
          case ModuleValidator::Global::AtomicsBuiltinFunction:
            return CheckCoercedAtomicsBuiltinCall(f, call, global->atomicsBuiltinFunction(), retType, type);
          case ModuleValidator::Global::ConstantLiteral:
          case ModuleValidator::Global::ConstantImport:
          case ModuleValidator::Global::Variable:
          case ModuleValidator::Global::FuncPtrTable:
          case ModuleValidator::Global::ArrayView:
          case ModuleValidator::Global::ArrayViewCtor:
          case ModuleValidator::Global::ByteLength:
          case ModuleValidator::Global::ChangeHeap:
            return f.failName(callee, "'%s' is not callable function", callee->name());
          case ModuleValidator::Global::SimdCtor:
          case ModuleValidator::Global::SimdOperation:
            return CheckCoercedSimdCall(f, call, global, retType, type);
          case ModuleValidator::Global::Function:
            break;
        }
    }

    return CheckInternalCall(f, call, calleeName, retType, type);
}

static bool
CheckPos(FunctionValidator& f, ParseNode* pos, Type* type)
{
    MOZ_ASSERT(pos->isKind(PNK_POS));
    ParseNode* operand = UnaryKid(pos);

    if (operand->isKind(PNK_CALL))
        return CheckCoercedCall(f, operand, RetType::Double, type);

    size_t opcodeAt = f.tempOp();
    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    return CoerceResult(f, operand, RetType::Double, operandType, opcodeAt, type);
}

static bool
CheckNot(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_NOT));
    ParseNode* operand = UnaryKid(expr);

    f.writeOp(I32::Not);

    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    if (!operandType.isInt())
        return f.failf(operand, "%s is not a subtype of int", operandType.toChars());

    *type = Type::Int;
    return true;
}

static bool
CheckNeg(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_NEG));
    ParseNode* operand = UnaryKid(expr);

    size_t opcodeAt = f.tempOp();

    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    if (operandType.isInt()) {
        f.patchOp(opcodeAt, I32::Neg);
        *type = Type::Intish;
        return true;
    }

    if (operandType.isMaybeDouble()) {
        f.patchOp(opcodeAt, F64::Neg);
        *type = Type::Double;
        return true;
    }

    if (operandType.isMaybeFloat()) {
        f.patchOp(opcodeAt, F32::Neg);
        *type = Type::Floatish;
        return true;
    }

    return f.failf(operand, "%s is not a subtype of int, float? or double?", operandType.toChars());
}

static bool
CheckCoerceToInt(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_BITNOT));
    ParseNode* operand = UnaryKid(expr);

    size_t opcodeAt = f.tempOp();

    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    if (operandType.isMaybeDouble() || operandType.isMaybeFloat()) {
        f.patchOp(opcodeAt, operandType.isMaybeDouble() ? I32::FromF64 : I32::FromF32);
        *type = Type::Signed;
        return true;
    }

    if (!operandType.isIntish())
        return f.failf(operand, "%s is not a subtype of double?, float? or intish", operandType.toChars());

    f.patchOp(opcodeAt, I32::Id);
    *type = Type::Signed;
    return true;
}

static bool
CheckBitNot(FunctionValidator& f, ParseNode* neg, Type* type)
{
    MOZ_ASSERT(neg->isKind(PNK_BITNOT));
    ParseNode* operand = UnaryKid(neg);

    if (operand->isKind(PNK_BITNOT))
        return CheckCoerceToInt(f, operand, type);

    f.writeOp(I32::BitNot);

    Type operandType;
    if (!CheckExpr(f, operand, &operandType))
        return false;

    if (!operandType.isIntish())
        return f.failf(operand, "%s is not a subtype of intish", operandType.toChars());

    *type = Type::Signed;
    return true;
}

static bool
CheckAsExprStatement(FunctionValidator& f, ParseNode* exprStmt);

static bool
CheckComma(FunctionValidator& f, ParseNode* comma, Type* type)
{
    MOZ_ASSERT(comma->isKind(PNK_COMMA));
    ParseNode* operands = ListHead(comma);

    size_t commaAt = f.tempOp();
    f.writeU32(ListLength(comma));

    ParseNode* pn = operands;
    for (; NextNode(pn); pn = NextNode(pn)) {
        if (!CheckAsExprStatement(f, pn))
            return false;
    }

    if (!CheckExpr(f, pn, type))
        return false;

    if (type->isIntish())
        f.patchOp(commaAt, I32::Comma);
    else if (type->isFloatish())
        f.patchOp(commaAt, F32::Comma);
    else if (type->isMaybeDouble())
        f.patchOp(commaAt, F64::Comma);
    else if (type->isInt32x4())
        f.patchOp(commaAt, I32X4::Comma);
    else if (type->isFloat32x4())
        f.patchOp(commaAt, F32X4::Comma);
    else
        MOZ_CRASH("unexpected or unimplemented expression statement");

    return true;
}

static bool
CheckConditional(FunctionValidator& f, ParseNode* ternary, Type* type)
{
    MOZ_ASSERT(ternary->isKind(PNK_CONDITIONAL));

    size_t opcodeAt = f.tempOp();

    ParseNode* cond = TernaryKid1(ternary);
    ParseNode* thenExpr = TernaryKid2(ternary);
    ParseNode* elseExpr = TernaryKid3(ternary);

    Type condType;
    if (!CheckExpr(f, cond, &condType))
        return false;

    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    Type thenType;
    if (!CheckExpr(f, thenExpr, &thenType))
        return false;

    Type elseType;
    if (!CheckExpr(f, elseExpr, &elseType))
        return false;

    if (thenType.isInt() && elseType.isInt()) {
        f.patchOp(opcodeAt, I32::Conditional);
        *type = Type::Int;
    } else if (thenType.isDouble() && elseType.isDouble()) {
        f.patchOp(opcodeAt, F64::Conditional);
        *type = Type::Double;
    } else if (thenType.isFloat() && elseType.isFloat()) {
        f.patchOp(opcodeAt, F32::Conditional);
        *type = Type::Float;
    } else if (elseType.isInt32x4() && thenType.isInt32x4()) {
        f.patchOp(opcodeAt, I32X4::Conditional);
        *type = Type::Int32x4;
    } else if (elseType.isFloat32x4() && thenType.isFloat32x4()) {
        f.patchOp(opcodeAt, F32X4::Conditional);
        *type = Type::Float32x4;
    } else {
        return f.failf(ternary, "then/else branches of conditional must both produce int, float, "
                       "double or SIMD types, current types are %s and %s",
                       thenType.toChars(), elseType.toChars());
    }

    return true;
}

static bool
IsValidIntMultiplyConstant(ModuleValidator& m, ParseNode* expr)
{
    if (!IsNumericLiteral(m, expr))
        return false;

    AsmJSNumLit literal = ExtractNumericLiteral(m, expr);
    switch (literal.which()) {
      case AsmJSNumLit::Fixnum:
      case AsmJSNumLit::NegativeInt:
        if (abs(literal.toInt32()) < (1<<20))
            return true;
        return false;
      case AsmJSNumLit::BigUnsigned:
      case AsmJSNumLit::Double:
      case AsmJSNumLit::Float:
      case AsmJSNumLit::OutOfRangeInt:
      case AsmJSNumLit::Int32x4:
      case AsmJSNumLit::Float32x4:
        return false;
    }

    MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("Bad literal");
}

static bool
CheckMultiply(FunctionValidator& f, ParseNode* star, Type* type)
{
    MOZ_ASSERT(star->isKind(PNK_STAR));
    ParseNode* lhs = MultiplyLeft(star);
    ParseNode* rhs = MultiplyRight(star);

    size_t opcodeAt = f.tempOp();

    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (lhsType.isInt() && rhsType.isInt()) {
        if (!IsValidIntMultiplyConstant(f.m(), lhs) && !IsValidIntMultiplyConstant(f.m(), rhs))
            return f.fail(star, "one arg to int multiply must be a small (-2^20, 2^20) int literal");
        f.patchOp(opcodeAt, I32::Mul);
        *type = Type::Intish;
        return true;
    }

    if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        f.patchOp(opcodeAt, F64::Mul);
        *type = Type::Double;
        return true;
    }

    if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        f.patchOp(opcodeAt, F32::Mul);
        *type = Type::Floatish;
        return true;
    }

    return f.fail(star, "multiply operands must be both int, both double? or both float?");
}

static bool
CheckAddOrSub(FunctionValidator& f, ParseNode* expr, Type* type, unsigned* numAddOrSubOut = nullptr)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    MOZ_ASSERT(expr->isKind(PNK_ADD) || expr->isKind(PNK_SUB));
    ParseNode* lhs = AddSubLeft(expr);
    ParseNode* rhs = AddSubRight(expr);

    Type lhsType, rhsType;
    unsigned lhsNumAddOrSub, rhsNumAddOrSub;

    size_t opcodeAt = f.tempOp();

    if (lhs->isKind(PNK_ADD) || lhs->isKind(PNK_SUB)) {
        if (!CheckAddOrSub(f, lhs, &lhsType, &lhsNumAddOrSub))
            return false;
        if (lhsType == Type::Intish)
            lhsType = Type::Int;
    } else {
        if (!CheckExpr(f, lhs, &lhsType))
            return false;
        lhsNumAddOrSub = 0;
    }

    if (rhs->isKind(PNK_ADD) || rhs->isKind(PNK_SUB)) {
        if (!CheckAddOrSub(f, rhs, &rhsType, &rhsNumAddOrSub))
            return false;
        if (rhsType == Type::Intish)
            rhsType = Type::Int;
    } else {
        if (!CheckExpr(f, rhs, &rhsType))
            return false;
        rhsNumAddOrSub = 0;
    }

    unsigned numAddOrSub = lhsNumAddOrSub + rhsNumAddOrSub + 1;
    if (numAddOrSub > (1<<20))
        return f.fail(expr, "too many + or - without intervening coercion");

    if (lhsType.isInt() && rhsType.isInt()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_ADD) ? I32::Add : I32::Sub);
        *type = Type::Intish;
    } else if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_ADD) ? F64::Add : F64::Sub);
        *type = Type::Double;
    } else if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_ADD) ? F32::Add : F32::Sub);
        *type = Type::Floatish;
    } else {
        return f.failf(expr, "operands to + or - must both be int, float? or double?, got %s and %s",
                       lhsType.toChars(), rhsType.toChars());
    }

    if (numAddOrSubOut)
        *numAddOrSubOut = numAddOrSub;
    return true;
}

static bool
CheckDivOrMod(FunctionValidator& f, ParseNode* expr, Type* type)
{
    MOZ_ASSERT(expr->isKind(PNK_DIV) || expr->isKind(PNK_MOD));

    size_t opcodeAt = f.tempOp();

    ParseNode* lhs = DivOrModLeft(expr);
    ParseNode* rhs = DivOrModRight(expr);

    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (lhsType.isMaybeDouble() && rhsType.isMaybeDouble()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_DIV) ? F64::Div : F64::Mod);
        *type = Type::Double;
        return true;
    }

    if (lhsType.isMaybeFloat() && rhsType.isMaybeFloat()) {
        if (expr->isKind(PNK_DIV))
            f.patchOp(opcodeAt, F32::Div);
        else
            return f.fail(expr, "modulo cannot receive float arguments");
        *type = Type::Floatish;
        return true;
    }

    if (lhsType.isSigned() && rhsType.isSigned()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_DIV) ? I32::SDiv : I32::SMod);
        *type = Type::Intish;
        return true;
    }

    if (lhsType.isUnsigned() && rhsType.isUnsigned()) {
        f.patchOp(opcodeAt, expr->isKind(PNK_DIV) ? I32::UDiv : I32::UMod);
        *type = Type::Intish;
        return true;
    }

    return f.failf(expr, "arguments to / or %% must both be double?, float?, signed, or unsigned; "
                   "%s and %s are given", lhsType.toChars(), rhsType.toChars());
}

static bool
CheckComparison(FunctionValidator& f, ParseNode* comp, Type* type)
{
    MOZ_ASSERT(comp->isKind(PNK_LT) || comp->isKind(PNK_LE) || comp->isKind(PNK_GT) ||
               comp->isKind(PNK_GE) || comp->isKind(PNK_EQ) || comp->isKind(PNK_NE));

    size_t opcodeAt = f.tempOp();

    ParseNode* lhs = ComparisonLeft(comp);
    ParseNode* rhs = ComparisonRight(comp);

    Type lhsType, rhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (!(lhsType.isSigned() && rhsType.isSigned()) &&
        !(lhsType.isUnsigned() && rhsType.isUnsigned()) &&
        !(lhsType.isDouble() && rhsType.isDouble()) &&
        !(lhsType.isFloat() && rhsType.isFloat()))
    {
        return f.failf(comp, "arguments to a comparison must both be signed, unsigned, floats or doubles; "
                       "%s and %s are given", lhsType.toChars(), rhsType.toChars());
    }

    I32 stmt;
    if (lhsType.isSigned() && rhsType.isSigned()) {
        switch (comp->getOp()) {
          case JSOP_EQ: stmt = I32::EqI32;  break;
          case JSOP_NE: stmt = I32::NeI32;  break;
          case JSOP_LT: stmt = I32::SLtI32; break;
          case JSOP_LE: stmt = I32::SLeI32; break;
          case JSOP_GT: stmt = I32::SGtI32; break;
          case JSOP_GE: stmt = I32::SGeI32; break;
          default: MOZ_CRASH("unexpected comparison op");
        }
    } else if (lhsType.isUnsigned() && rhsType.isUnsigned()) {
        switch (comp->getOp()) {
          case JSOP_EQ: stmt = I32::EqI32;  break;
          case JSOP_NE: stmt = I32::NeI32;  break;
          case JSOP_LT: stmt = I32::ULtI32; break;
          case JSOP_LE: stmt = I32::ULeI32; break;
          case JSOP_GT: stmt = I32::UGtI32; break;
          case JSOP_GE: stmt = I32::UGeI32; break;
          default: MOZ_CRASH("unexpected comparison op");
        }
    } else if (lhsType.isDouble()) {
        switch (comp->getOp()) {
          case JSOP_EQ: stmt = I32::EqF64;  break;
          case JSOP_NE: stmt = I32::NeF64;  break;
          case JSOP_LT: stmt = I32::LtF64; break;
          case JSOP_LE: stmt = I32::LeF64; break;
          case JSOP_GT: stmt = I32::GtF64; break;
          case JSOP_GE: stmt = I32::GeF64; break;
          default: MOZ_CRASH("unexpected comparison op");
        }
    } else if (lhsType.isFloat()) {
        switch (comp->getOp()) {
          case JSOP_EQ: stmt = I32::EqF32;  break;
          case JSOP_NE: stmt = I32::NeF32;  break;
          case JSOP_LT: stmt = I32::LtF32; break;
          case JSOP_LE: stmt = I32::LeF32; break;
          case JSOP_GT: stmt = I32::GtF32; break;
          case JSOP_GE: stmt = I32::GeF32; break;
          default: MOZ_CRASH("unexpected comparison op");
        }
    } else {
        MOZ_CRASH("unexpected type");
    }

    f.patchOp(opcodeAt, stmt);
    *type = Type::Int;
    return true;
}

static bool
CheckBitwise(FunctionValidator& f, ParseNode* bitwise, Type* type)
{
    ParseNode* lhs = BitwiseLeft(bitwise);
    ParseNode* rhs = BitwiseRight(bitwise);

    int32_t identityElement;
    bool onlyOnRight;
    switch (bitwise->getKind()) {
      case PNK_BITOR:  identityElement = 0;  onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_BITAND: identityElement = -1; onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_BITXOR: identityElement = 0;  onlyOnRight = false; *type = Type::Signed;   break;
      case PNK_LSH:    identityElement = 0;  onlyOnRight = true;  *type = Type::Signed;   break;
      case PNK_RSH:    identityElement = 0;  onlyOnRight = true;  *type = Type::Signed;   break;
      case PNK_URSH:   identityElement = 0;  onlyOnRight = true;  *type = Type::Unsigned; break;
      default: MOZ_CRASH("not a bitwise op");
    }

    uint32_t i;
    if (!onlyOnRight && IsLiteralInt(f.m(), lhs, &i) && i == uint32_t(identityElement)) {
        Type rhsType;
        if (!CheckExpr(f, rhs, &rhsType))
            return false;
        if (!rhsType.isIntish())
            return f.failf(bitwise, "%s is not a subtype of intish", rhsType.toChars());
        return true;
    }

    if (IsLiteralInt(f.m(), rhs, &i) && i == uint32_t(identityElement)) {
        if (bitwise->isKind(PNK_BITOR) && lhs->isKind(PNK_CALL))
            return CheckCoercedCall(f, lhs, RetType::Signed, type);

        Type lhsType;
        if (!CheckExpr(f, lhs, &lhsType))
            return false;
        if (!lhsType.isIntish())
            return f.failf(bitwise, "%s is not a subtype of intish", lhsType.toChars());
        return true;
    }

    switch (bitwise->getKind()) {
      case PNK_BITOR:  f.writeOp(I32::BitOr); break;
      case PNK_BITAND: f.writeOp(I32::BitAnd); break;
      case PNK_BITXOR: f.writeOp(I32::BitXor); break;
      case PNK_LSH:    f.writeOp(I32::Lsh); break;
      case PNK_RSH:    f.writeOp(I32::ArithRsh); break;
      case PNK_URSH:   f.writeOp(I32::LogicRsh); break;
      default: MOZ_CRASH("not a bitwise op");
    }

    Type lhsType;
    if (!CheckExpr(f, lhs, &lhsType))
        return false;

    Type rhsType;
    if (!CheckExpr(f, rhs, &rhsType))
        return false;

    if (!lhsType.isIntish())
        return f.failf(lhs, "%s is not a subtype of intish", lhsType.toChars());
    if (!rhsType.isIntish())
        return f.failf(rhs, "%s is not a subtype of intish", rhsType.toChars());

    return true;
}

static bool
CheckExpr(FunctionValidator& f, ParseNode* expr, Type* type)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    if (IsNumericLiteral(f.m(), expr))
        return CheckNumericLiteral(f, expr, type);

    switch (expr->getKind()) {
      case PNK_NAME:        return CheckVarRef(f, expr, type);
      case PNK_ELEM:        return CheckLoadArray(f, expr, type);
      case PNK_DOT:         return CheckDotAccess(f, expr, type);
      case PNK_ASSIGN:      return CheckAssign(f, expr, type);
      case PNK_POS:         return CheckPos(f, expr, type);
      case PNK_NOT:         return CheckNot(f, expr, type);
      case PNK_NEG:         return CheckNeg(f, expr, type);
      case PNK_BITNOT:      return CheckBitNot(f, expr, type);
      case PNK_COMMA:       return CheckComma(f, expr, type);
      case PNK_CONDITIONAL: return CheckConditional(f, expr, type);
      case PNK_STAR:        return CheckMultiply(f, expr, type);
      case PNK_CALL:        return CheckUncoercedCall(f, expr, type);

      case PNK_ADD:
      case PNK_SUB:         return CheckAddOrSub(f, expr, type);

      case PNK_DIV:
      case PNK_MOD:         return CheckDivOrMod(f, expr, type);

      case PNK_LT:
      case PNK_LE:
      case PNK_GT:
      case PNK_GE:
      case PNK_EQ:
      case PNK_NE:          return CheckComparison(f, expr, type);

      case PNK_BITOR:
      case PNK_BITAND:
      case PNK_BITXOR:
      case PNK_LSH:
      case PNK_RSH:
      case PNK_URSH:        return CheckBitwise(f, expr, type);

      default:;
    }

    return f.fail(expr, "unsupported expression");
}

static bool
CheckStatement(FunctionValidator& f, ParseNode* stmt);

static bool
CheckAsExprStatement(FunctionValidator& f, ParseNode* expr)
{
    Type type;
    if (expr->isKind(PNK_CALL))
        return CheckCoercedCall(f, expr, RetType::Void, &type);

    size_t opcodeAt = f.tempOp();

    if (!CheckExpr(f, expr, &type))
        return false;

    if (type.isIntish())
        f.patchOp(opcodeAt, Stmt::I32Expr);
    else if (type.isFloatish())
        f.patchOp(opcodeAt, Stmt::F32Expr);
    else if (type.isMaybeDouble())
        f.patchOp(opcodeAt, Stmt::F64Expr);
    else if (type.isInt32x4())
        f.patchOp(opcodeAt, Stmt::I32X4Expr);
    else if (type.isFloat32x4())
        f.patchOp(opcodeAt, Stmt::F32X4Expr);
    else
        MOZ_CRASH("unexpected or unimplemented expression statement");

    return true;
}

static bool
CheckExprStatement(FunctionValidator& f, ParseNode* exprStmt)
{
    MOZ_ASSERT(exprStmt->isKind(PNK_SEMI));
    ParseNode* expr = UnaryKid(exprStmt);

    if (!expr) {
        f.writeOp(Stmt::Noop);
        return true;
    }

    return CheckAsExprStatement(f, expr);
}

enum class InterruptCheckPosition {
    Head,
    Loop
};

static void
MaybeAddInterruptCheck(FunctionValidator& f, InterruptCheckPosition pos, ParseNode* pn)
{
    if (f.m().module().usesSignalHandlersForInterrupt())
        return;

    switch (pos) {
      case InterruptCheckPosition::Head: f.writeOp(Stmt::InterruptCheckHead); break;
      case InterruptCheckPosition::Loop: f.writeOp(Stmt::InterruptCheckLoop); break;
    }

    unsigned lineno = 0, column = 0;
    f.m().tokenStream().srcCoords.lineNumAndColumnIndex(pn->pn_pos.begin, &lineno, &column);
    f.writeU32(lineno);
    f.writeU32(column);
}

static bool
CheckWhile(FunctionValidator& f, ParseNode* whileStmt)
{
    MOZ_ASSERT(whileStmt->isKind(PNK_WHILE));
    ParseNode* cond = BinaryLeft(whileStmt);
    ParseNode* body = BinaryRight(whileStmt);

    f.writeOp(Stmt::While);

    Type condType;
    if (!CheckExpr(f, cond, &condType))
        return false;
    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    MaybeAddInterruptCheck(f, InterruptCheckPosition::Loop, whileStmt);

    return CheckStatement(f, body);
}

static bool
CheckFor(FunctionValidator& f, ParseNode* forStmt)
{
    MOZ_ASSERT(forStmt->isKind(PNK_FOR));
    ParseNode* forHead = BinaryLeft(forStmt);
    ParseNode* body = BinaryRight(forStmt);

    if (!forHead->isKind(PNK_FORHEAD))
        return f.fail(forHead, "unsupported for-loop statement");

    ParseNode* maybeInit = TernaryKid1(forHead);
    ParseNode* maybeCond = TernaryKid2(forHead);
    ParseNode* maybeInc = TernaryKid3(forHead);

    f.writeOp(maybeInit ? (maybeInc ? Stmt::ForInitInc   : Stmt::ForInitNoInc)
                        : (maybeInc ? Stmt::ForNoInitInc : Stmt::ForNoInitNoInc));

    if (maybeInit && !CheckAsExprStatement(f, maybeInit))
        return false;

    if (maybeCond) {
        Type condType;
        if (!CheckExpr(f, maybeCond, &condType))
            return false;
        if (!condType.isInt())
            return f.failf(maybeCond, "%s is not a subtype of int", condType.toChars());
    } else {
        f.writeInt32Lit(1);
    }

    MaybeAddInterruptCheck(f, InterruptCheckPosition::Loop, forStmt);

    if (!CheckStatement(f, body))
        return false;

    if (maybeInc && !CheckAsExprStatement(f, maybeInc))
        return false;

    f.writeDebugCheckPoint();
    return true;
}

static bool
CheckDoWhile(FunctionValidator& f, ParseNode* whileStmt)
{
    MOZ_ASSERT(whileStmt->isKind(PNK_DOWHILE));
    ParseNode* body = BinaryLeft(whileStmt);
    ParseNode* cond = BinaryRight(whileStmt);

    f.writeOp(Stmt::DoWhile);

    MaybeAddInterruptCheck(f, InterruptCheckPosition::Loop, cond);

    if (!CheckStatement(f, body))
        return false;

    Type condType;
    if (!CheckExpr(f, cond, &condType))
        return false;
    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    return true;
}

static bool
CheckLabel(FunctionValidator& f, ParseNode* labeledStmt)
{
    MOZ_ASSERT(labeledStmt->isKind(PNK_LABEL));
    PropertyName* label = LabeledStatementLabel(labeledStmt);
    ParseNode* stmt = LabeledStatementStatement(labeledStmt);

    f.writeOp(Stmt::Label);

    uint32_t labelId;
    if (!f.addLabel(label, &labelId))
        return false;

    f.writeU32(labelId);

    if (!CheckStatement(f, stmt))
        return false;

    f.removeLabel(label);
    return true;
}

static bool
CheckIf(FunctionValidator& f, ParseNode* ifStmt)
{
  recurse:
    size_t opcodeAt = f.tempOp();

    MOZ_ASSERT(ifStmt->isKind(PNK_IF));
    ParseNode* cond = TernaryKid1(ifStmt);
    ParseNode* thenStmt = TernaryKid2(ifStmt);
    ParseNode* elseStmt = TernaryKid3(ifStmt);

    Type condType;
    if (!CheckExpr(f, cond, &condType))
        return false;
    if (!condType.isInt())
        return f.failf(cond, "%s is not a subtype of int", condType.toChars());

    if (!CheckStatement(f, thenStmt))
        return false;

    if (!elseStmt) {
        f.patchOp(opcodeAt, Stmt::IfThen);
    } else {
        f.patchOp(opcodeAt, Stmt::IfElse);

        if (elseStmt->isKind(PNK_IF)) {
            ifStmt = elseStmt;
            goto recurse;
        }

        if (!CheckStatement(f, elseStmt))
            return false;
    }

    return true;
}

static bool
CheckCaseExpr(FunctionValidator& f, ParseNode* caseExpr, int32_t* value)
{
    if (!IsNumericLiteral(f.m(), caseExpr))
        return f.fail(caseExpr, "switch case expression must be an integer literal");

    AsmJSNumLit literal = ExtractNumericLiteral(f.m(), caseExpr);
    switch (literal.which()) {
      case AsmJSNumLit::Fixnum:
      case AsmJSNumLit::NegativeInt:
        *value = literal.toInt32();
        break;
      case AsmJSNumLit::OutOfRangeInt:
      case AsmJSNumLit::BigUnsigned:
        return f.fail(caseExpr, "switch case expression out of integer range");
      case AsmJSNumLit::Double:
      case AsmJSNumLit::Float:
      case AsmJSNumLit::Int32x4:
      case AsmJSNumLit::Float32x4:
        return f.fail(caseExpr, "switch case expression must be an integer literal");
    }

    return true;
}

static bool
CheckDefaultAtEnd(FunctionValidator& f, ParseNode* stmt)
{
    for (; stmt; stmt = NextNode(stmt)) {
        MOZ_ASSERT(stmt->isKind(PNK_CASE) || stmt->isKind(PNK_DEFAULT));
        if (stmt->isKind(PNK_DEFAULT) && NextNode(stmt) != nullptr)
            return f.fail(stmt, "default label must be at the end");
    }

    return true;
}

static bool
CheckSwitchRange(FunctionValidator& f, ParseNode* stmt, int32_t* low, int32_t* high,
                 int32_t* tableLength)
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

    ParseNode* initialStmt = stmt;
    for (stmt = NextNode(stmt); stmt && stmt->isKind(PNK_CASE); stmt = NextNode(stmt)) {
        int32_t i = 0;
        if (!CheckCaseExpr(f, CaseExpr(stmt), &i))
            return false;

        *low = Min(*low, i);
        *high = Max(*high, i);
    }

    int64_t i64 = (int64_t(*high) - int64_t(*low)) + 1;
    if (i64 > 4*1024*1024)
        return f.fail(initialStmt, "all switch statements generate tables; this table would be too big");

    *tableLength = int32_t(i64);
    return true;
}

void
PatchSwitch(FunctionValidator& f,
            size_t hasDefaultAt, bool hasDefault,
            size_t lowAt, int32_t low,
            size_t highAt, int32_t high,
            size_t numCasesAt, uint32_t numCases)
{
    f.patchU8(hasDefaultAt, uint8_t(hasDefault));
    f.patch32(lowAt, low);
    f.patch32(highAt, high);
    f.patch32(numCasesAt, numCases);
}

static bool
CheckSwitch(FunctionValidator& f, ParseNode* switchStmt)
{
    MOZ_ASSERT(switchStmt->isKind(PNK_SWITCH));

    f.writeOp(Stmt::Switch);
    // Has default
    size_t hasDefaultAt = f.tempU8();
    // Low / High / Num cases
    size_t lowAt = f.temp32();
    size_t highAt = f.temp32();
    size_t numCasesAt = f.temp32();

    ParseNode* switchExpr = BinaryLeft(switchStmt);
    ParseNode* switchBody = BinaryRight(switchStmt);

    if (!switchBody->isKind(PNK_STATEMENTLIST))
        return f.fail(switchBody, "switch body may not contain 'let' declarations");

    Type exprType;
    if (!CheckExpr(f, switchExpr, &exprType))
        return false;

    if (!exprType.isSigned())
        return f.failf(switchExpr, "%s is not a subtype of signed", exprType.toChars());

    ParseNode* stmt = ListHead(switchBody);

    if (!CheckDefaultAtEnd(f, stmt))
        return false;

    if (!stmt) {
        PatchSwitch(f, hasDefaultAt, false, lowAt, 0, highAt, 0, numCasesAt, 0);
        return true;
    }

    int32_t low = 0, high = 0, tableLength = 0;
    if (!CheckSwitchRange(f, stmt, &low, &high, &tableLength))
        return false;

    Vector<bool, 8> cases(f.cx());
    if (!cases.resize(tableLength))
        return false;

    uint32_t numCases = 0;
    for (; stmt && stmt->isKind(PNK_CASE); stmt = NextNode(stmt)) {
        int32_t caseValue = ExtractNumericLiteral(f.m(), CaseExpr(stmt)).toInt32();
        unsigned caseIndex = caseValue - low;

        if (cases[caseIndex])
            return f.fail(stmt, "no duplicate case labels");

        cases[caseIndex] = true;
        numCases += 1;
        f.writeI32(caseValue);

        if (!CheckStatement(f, CaseBody(stmt)))
            return false;

    }

    bool hasDefault = false;
    if (stmt && stmt->isKind(PNK_DEFAULT)) {
        hasDefault = true;
        if (!CheckStatement(f, CaseBody(stmt)))
            return false;
    }

    PatchSwitch(f, hasDefaultAt, hasDefault, lowAt, low, highAt, high, numCasesAt, numCases);
    return true;
}

static bool
CheckReturnType(FunctionValidator& f, ParseNode* usepn, RetType retType)
{
    if (!f.hasAlreadyReturned()) {
        f.setReturnedType(retType);
        return true;
    }

    if (f.returnedType() != retType) {
        return f.failf(usepn, "%s incompatible with previous return of type %s",
                       retType.toType().toChars(), f.returnedType().toType().toChars());
    }

    return true;
}

static bool
CheckReturn(FunctionValidator& f, ParseNode* returnStmt)
{
    ParseNode* expr = ReturnExpr(returnStmt);

    f.writeOp(Stmt::Ret);

    if (!expr)
        return CheckReturnType(f, returnStmt, RetType::Void);

    Type type;
    if (!CheckExpr(f, expr, &type))
        return false;

    RetType retType;
    if (type.isSigned())
        retType = RetType::Signed;
    else if (type.isDouble())
        retType = RetType::Double;
    else if (type.isFloat())
        retType = RetType::Float;
    else if (type.isInt32x4())
        retType = RetType::Int32x4;
    else if (type.isFloat32x4())
        retType = RetType::Float32x4;
    else if (type.isVoid())
        retType = RetType::Void;
    else
        return f.failf(expr, "%s is not a valid return type", type.toChars());

    return CheckReturnType(f, expr, retType);
}

static bool
CheckStatementList(FunctionValidator& f, ParseNode* stmtList)
{
    MOZ_ASSERT(stmtList->isKind(PNK_STATEMENTLIST));

    f.writeOp(Stmt::Block);
    f.writeU32(ListLength(stmtList));

    for (ParseNode* stmt = ListHead(stmtList); stmt; stmt = NextNode(stmt)) {
        if (!CheckStatement(f, stmt))
            return false;
    }

    f.writeDebugCheckPoint();
    return true;
}

static bool
CheckBreakOrContinue(FunctionValidator& f, PropertyName* maybeLabel,
                     Stmt withoutLabel, Stmt withLabel)
{
    if (!maybeLabel) {
        f.writeOp(withoutLabel);
        return true;
    }

    f.writeOp(withLabel);

    uint32_t labelId = f.lookupLabel(maybeLabel);
    MOZ_ASSERT(labelId != uint32_t(-1));

    f.writeU32(labelId);
    return true;
}

static bool
CheckStatement(FunctionValidator& f, ParseNode* stmt)
{
    JS_CHECK_RECURSION_DONT_REPORT(f.cx(), return f.m().failOverRecursed());

    switch (stmt->getKind()) {
      case PNK_SEMI:          return CheckExprStatement(f, stmt);
      case PNK_WHILE:         return CheckWhile(f, stmt);
      case PNK_FOR:           return CheckFor(f, stmt);
      case PNK_DOWHILE:       return CheckDoWhile(f, stmt);
      case PNK_LABEL:         return CheckLabel(f, stmt);
      case PNK_IF:            return CheckIf(f, stmt);
      case PNK_SWITCH:        return CheckSwitch(f, stmt);
      case PNK_RETURN:        return CheckReturn(f, stmt);
      case PNK_STATEMENTLIST: return CheckStatementList(f, stmt);
      case PNK_BREAK:         return CheckBreakOrContinue(f, LoopControlMaybeLabel(stmt),
                                                          Stmt::Break, Stmt::BreakLabel);
      case PNK_CONTINUE:      return CheckBreakOrContinue(f, LoopControlMaybeLabel(stmt),
                                                          Stmt::Continue, Stmt::ContinueLabel);
      default:;
    }

    return f.fail(stmt, "unexpected statement kind");
}

static bool
CheckByteLengthCall(ModuleValidator& m, ParseNode* pn, PropertyName* newBufferName)
{
    if (!pn->isKind(PNK_CALL) || !CallCallee(pn)->isKind(PNK_NAME))
        return m.fail(pn, "expecting call to imported byteLength");

    const ModuleValidator::Global* global = m.lookupGlobal(CallCallee(pn)->name());
    if (!global || global->which() != ModuleValidator::Global::ByteLength)
        return m.fail(pn, "expecting call to imported byteLength");

    if (CallArgListLength(pn) != 1 || !IsUseOfName(CallArgList(pn), newBufferName))
        return m.failName(pn, "expecting %s as argument to byteLength call", newBufferName);

    return true;
}

static bool
CheckHeapLengthCondition(ModuleValidator& m, ParseNode* cond, PropertyName* newBufferName,
                         uint32_t* mask, uint32_t* minLength, uint32_t* maxLength)
{
    if (!cond->isKind(PNK_OR) || !AndOrLeft(cond)->isKind(PNK_OR))
        return m.fail(cond, "expecting byteLength & K || byteLength <= L || byteLength > M");

    ParseNode* cond1 = AndOrLeft(AndOrLeft(cond));
    ParseNode* cond2 = AndOrRight(AndOrLeft(cond));
    ParseNode* cond3 = AndOrRight(cond);

    if (!cond1->isKind(PNK_BITAND))
        return m.fail(cond1, "expecting byteLength & K");

    if (!CheckByteLengthCall(m, BitwiseLeft(cond1), newBufferName))
        return false;

    ParseNode* maskNode = BitwiseRight(cond1);
    if (!IsLiteralInt(m, maskNode, mask))
        return m.fail(maskNode, "expecting integer literal mask");
    if (*mask == UINT32_MAX)
        return m.fail(maskNode, "invalid mask value");
    if ((*mask & 0xffffff) != 0xffffff)
        return m.fail(maskNode, "mask value must have the bits 0xffffff set");

    if (!cond2->isKind(PNK_LE))
        return m.fail(cond2, "expecting byteLength <= L");

    if (!CheckByteLengthCall(m, RelationalLeft(cond2), newBufferName))
        return false;

    ParseNode* minLengthNode = RelationalRight(cond2);
    uint32_t minLengthExclusive;
    if (!IsLiteralInt(m, minLengthNode, &minLengthExclusive))
        return m.fail(minLengthNode, "expecting integer literal");
    if (minLengthExclusive < 0xffffff || minLengthExclusive == UINT32_MAX)
        return m.fail(minLengthNode, "literal must be >= 0xffffff and < 0xffffffff");

    // Add one to convert from exclusive (the branch rejects if ==) to inclusive.
    *minLength = minLengthExclusive + 1;

    if (!cond3->isKind(PNK_GT))
        return m.fail(cond3, "expecting byteLength > M");

    if (!CheckByteLengthCall(m, RelationalLeft(cond3), newBufferName))
        return false;

    ParseNode* maxLengthNode = RelationalRight(cond3);
    if (!IsLiteralInt(m, maxLengthNode, maxLength))
        return m.fail(maxLengthNode, "expecting integer literal");
    if (*maxLength > 0x80000000)
        return m.fail(maxLengthNode, "literal must be <= 0x80000000");

    if (*maxLength < *minLength)
        return m.fail(maxLengthNode, "maximum length must be greater or equal to minimum length");

    return true;
}

static bool
CheckReturnBoolLiteral(ModuleValidator& m, ParseNode* stmt, bool retval)
{
    if (stmt->isKind(PNK_STATEMENTLIST)) {
        ParseNode* next = SkipEmptyStatements(ListHead(stmt));
        if (!next)
            return m.fail(stmt, "expected return statement");
        stmt = next;
        if (NextNonEmptyStatement(stmt))
            return m.fail(stmt, "expected single return statement");
    }

    if (!stmt->isKind(PNK_RETURN))
        return m.fail(stmt, "expected return statement");

    ParseNode* returnExpr = ReturnExpr(stmt);
    if (!returnExpr || !returnExpr->isKind(retval ? PNK_TRUE : PNK_FALSE))
        return m.failf(stmt, "expected 'return %s;'", retval ? "true" : "false");

    return true;
}

static bool
CheckReassignmentTo(ModuleValidator& m, ParseNode* stmt, PropertyName* lhsName, ParseNode** rhs)
{
    if (!stmt->isKind(PNK_SEMI))
        return m.fail(stmt, "missing reassignment");

    ParseNode* assign = UnaryKid(stmt);
    if (!assign || !assign->isKind(PNK_ASSIGN))
        return m.fail(stmt, "missing reassignment");

    ParseNode* lhs = BinaryLeft(assign);
    if (!IsUseOfName(lhs, lhsName))
        return m.failName(lhs, "expecting reassignment of %s", lhsName);

    *rhs = BinaryRight(assign);
    return true;
}

static bool
CheckChangeHeap(ModuleValidator& m, ParseNode* fn, bool* validated)
{
    MOZ_ASSERT(fn->isKind(PNK_FUNCTION));

    // We don't yet know whether this is a change-heap function.
    // The point at which we know we have a change-heap function is once we see
    // whether the argument is coerced according to the normal asm.js rules. If
    // it is coerced, it's not change-heap and must validate according to normal
    // rules; otherwise it must validate as a change-heap function.
    *validated = false;

    PropertyName* changeHeapName = FunctionName(fn);
    if (!CheckModuleLevelName(m, fn, changeHeapName))
        return false;

    unsigned numFormals;
    ParseNode* arg = FunctionArgsList(fn, &numFormals);
    if (numFormals != 1)
        return true;

    PropertyName* newBufferName;
    if (!CheckArgument(m, arg, &newBufferName))
        return false;

    ParseNode* stmtIter = SkipEmptyStatements(ListHead(FunctionStatementList(fn)));
    if (!stmtIter || !stmtIter->isKind(PNK_IF))
        return true;

    // We can now issue validation failures if we see something that isn't a
    // valid change-heap function.
    *validated = true;

    PropertyName* bufferName = m.module().bufferArgumentName();
    if (!bufferName)
        return m.fail(fn, "to change heaps, the module must have a buffer argument");

    ParseNode* cond = TernaryKid1(stmtIter);
    ParseNode* thenStmt = TernaryKid2(stmtIter);
    if (ParseNode* elseStmt = TernaryKid3(stmtIter))
        return m.fail(elseStmt, "unexpected else statement");

    uint32_t mask, min = 0, max;  // initialize min to silence GCC warning
    if (!CheckHeapLengthCondition(m, cond, newBufferName, &mask, &min, &max))
        return false;

    if (!CheckReturnBoolLiteral(m, thenStmt, false))
        return false;

    ParseNode* next = NextNonEmptyStatement(stmtIter);

    for (unsigned i = 0; i < m.numArrayViews(); i++, next = NextNonEmptyStatement(stmtIter)) {
        if (!next)
            return m.failOffset(stmtIter->pn_pos.end, "missing reassignment");
        stmtIter = next;

        const ModuleValidator::ArrayView& view = m.arrayView(i);

        ParseNode* rhs;
        if (!CheckReassignmentTo(m, stmtIter, view.name, &rhs))
            return false;

        if (!rhs->isKind(PNK_NEW))
            return m.failName(rhs, "expecting assignment of new array view to %s", view.name);

        ParseNode* ctorExpr = ListHead(rhs);
        if (!ctorExpr->isKind(PNK_NAME))
            return m.fail(rhs, "expecting name of imported typed array constructor");

        const ModuleValidator::Global* global = m.lookupGlobal(ctorExpr->name());
        if (!global || global->which() != ModuleValidator::Global::ArrayViewCtor)
            return m.fail(rhs, "expecting name of imported typed array constructor");
        if (global->viewType() != view.type)
            return m.fail(rhs, "can't change the type of a global view variable");

        if (!CheckNewArrayViewArgs(m, ctorExpr, newBufferName))
            return false;
    }

    if (!next)
        return m.failOffset(stmtIter->pn_pos.end, "missing reassignment");
    stmtIter = next;

    ParseNode* rhs;
    if (!CheckReassignmentTo(m, stmtIter, bufferName, &rhs))
        return false;
    if (!IsUseOfName(rhs, newBufferName))
        return m.failName(stmtIter, "expecting assignment of new buffer to %s", bufferName);

    next = NextNonEmptyStatement(stmtIter);
    if (!next)
        return m.failOffset(stmtIter->pn_pos.end, "expected return statement");
    stmtIter = next;

    if (!CheckReturnBoolLiteral(m, stmtIter, true))
        return false;

    stmtIter = NextNonEmptyStatement(stmtIter);
    if (stmtIter)
        return m.fail(stmtIter, "expecting end of function");

    return m.addChangeHeap(changeHeapName, fn, mask, min, max);
}

static bool
ParseFunction(ModuleValidator& m, ParseNode** fnOut)
{
    TokenStream& tokenStream = m.tokenStream();

    tokenStream.consumeKnownToken(TOK_FUNCTION, TokenStream::Operand);

    RootedPropertyName name(m.cx());

    TokenKind tk;
    if (!tokenStream.getToken(&tk, TokenStream::Operand))
        return false;
    if (tk == TOK_NAME) {
        name = tokenStream.currentName();
    } else if (tk == TOK_YIELD) {
        if (!m.parser().checkYieldNameValidity())
            return false;
        name = m.cx()->names().yield;
    } else {
        return false;  // The regular parser will throw a SyntaxError, no need to m.fail.
    }

    ParseNode* fn = m.parser().handler.newFunctionDefinition();
    if (!fn)
        return false;

    // This flows into FunctionBox, so must be tenured.
    RootedFunction fun(m.cx(),
                       NewScriptedFunction(m.cx(), 0, JSFunction::INTERPRETED,
                                           name, gc::AllocKind::FUNCTION,
                                           TenuredObject));
    if (!fun)
        return false;

    AsmJSParseContext* outerpc = m.parser().pc;

    Directives directives(outerpc);
    FunctionBox* funbox = m.parser().newFunctionBox(fn, fun, outerpc, directives, NotGenerator);
    if (!funbox)
        return false;

    Directives newDirectives = directives;
    AsmJSParseContext funpc(&m.parser(), outerpc, fn, funbox, &newDirectives,
                            /* blockScopeDepth = */ 0);
    if (!funpc.init(m.parser()))
        return false;

    if (!m.parser().functionArgsAndBodyGeneric(InAllowed, YieldIsName, fn, fun, Statement)) {
        if (tokenStream.hadError() || directives == newDirectives)
            return false;

        return m.fail(fn, "encountered new directive in function");
    }

    MOZ_ASSERT(!tokenStream.hadError());
    MOZ_ASSERT(directives == newDirectives);

    fn->pn_blockid = outerpc->blockid();

    *fnOut = fn;
    return true;
}

static bool
CheckFunction(ModuleValidator& m, LifoAlloc& lifo, AsmFunction** funcOut)
{
    int64_t before = PRMJ_Now();

    // asm.js modules can be quite large when represented as parse trees so pop
    // the backing LifoAlloc after parsing/compiling each function.
    AsmJSParser::Mark mark = m.parser().mark();

    ParseNode* fn = nullptr;  // initialize to silence GCC warning
    if (!ParseFunction(m, &fn))
        return false;

    if (!CheckFunctionHead(m, fn))
        return false;

    if (m.tryOnceToValidateChangeHeap()) {
        bool validated;
        if (!CheckChangeHeap(m, fn, &validated))
            return false;
        if (validated) {
            *funcOut = nullptr;
            return true;
        }
    }

    AsmFunction* asmFunc = lifo.new_<AsmFunction>(lifo);
    FunctionValidator f(m, *asmFunc, fn);
    if (!f.init())
        return false;

    ParseNode* stmtIter = ListHead(FunctionStatementList(fn));

    if (!CheckProcessingDirectives(m, &stmtIter))
        return false;

    VarTypeVector argTypes(m.lifo());
    if (!CheckArguments(f, &stmtIter, &argTypes))
        return false;

    if (!CheckVariables(f, &stmtIter))
        return false;

    MOZ_ASSERT(!f.startedPacking(), "No bytecode should be written at this point.");
    MaybeAddInterruptCheck(f, InterruptCheckPosition::Head, fn);

    ParseNode* lastNonEmptyStmt = nullptr;
    for (; stmtIter; stmtIter = NextNode(stmtIter)) {
        if (!CheckStatement(f, stmtIter))
            return false;
        if (!IsEmptyStatement(stmtIter))
            lastNonEmptyStmt = stmtIter;
    }

    if (!CheckFinalReturn(f, lastNonEmptyStmt))
        return false;

    Signature sig(Move(argTypes), f.returnedType());
    ModuleValidator::Func* func = nullptr;
    if (!CheckFunctionSignature(m, fn, Move(sig), FunctionName(fn), &func))
        return false;

    if (func->defined())
        return m.failName(fn, "function '%s' already defined", FunctionName(fn));

    func->define(fn);

    func->accumulateCompileTime((PRMJ_Now() - before) / PRMJ_USEC_PER_MSEC);

    unsigned lineno, column;
    m.tokenStream().srcCoords.lineNumAndColumnIndex(func->srcBegin(), &lineno, &column);
    if (!asmFunc->finish(func->sig().args(), func->name(), func->funcIndex(),
                         func->srcBegin(), lineno, column, func->compileTime()))
    {
        return false;
    }

    m.parser().release(mark);

    *funcOut = asmFunc;
    return true;
}

static bool
CheckAllFunctionsDefined(ModuleValidator& m)
{
    for (unsigned i = 0; i < m.numFunctions(); i++) {
        ModuleValidator::Func& f = m.function(i);
        if (!f.defined()) {
            return m.failNameOffset(f.firstUseOffset(),
                                    "missing definition of function %s", f.name());
        }
    }

    return true;
}

static bool
CheckFunctionsSequential(ModuleValidator& m, ScopedJSDeletePtr<ModuleCompileResults>* compileResults)
{
    // Use a single LifoAlloc to allocate all the temporary compiler IR.
    // All allocated LifoAlloc'd memory is released after compiling each
    // function by the LifoAllocScope inside the loop.
    LifoAlloc lifo(LIFO_ALLOC_PRIMARY_CHUNK_SIZE);

    AsmModuleCompilerScope scope;
    if (!CreateAsmModuleCompiler(m.compileInputs(), &scope))
        return false;
    ModuleCompiler& mc = scope.module();

    while (true) {
        TokenKind tk;
        if (!PeekToken(m.parser(), &tk))
            return false;
        if (tk != TOK_FUNCTION)
            break;

        LifoAllocScope scope(&lifo);

        AsmFunction* func;
        if (!CheckFunction(m, lifo, &func))
            return false;

        // In the case of the change-heap function, no function is produced.
        if (!func)
            continue;

        MIRGenerator* mir;
        if (!GenerateAsmFunctionMIR(mc, lifo, *func, &mir))
            return false;

        int64_t before = PRMJ_Now();

        LIRGraph* lir;
        {
            JitContext jcx(m.cx(), &mir->alloc());
            jit::AutoSpewEndFunction spewEndFunction(mir);

            if (!OptimizeMIR(mir))
                return m.failOffset(func->srcBegin(), "internal compiler failure (probably out of memory)");

            lir = GenerateLIR(mir);
            if (!lir)
                return m.failOffset(func->srcBegin(), "internal compiler failure (probably out of memory)");

            func->accumulateCompileTime((PRMJ_Now() - before) / PRMJ_USEC_PER_MSEC);
        }

        if (!GenerateAsmFunctionCode(mc, *func, *mir, *lir))
            return false;
    }

    if (!CheckAllFunctionsDefined(m))
        return false;

    FinishAsmModuleCompilation(mc, compileResults);
    return true;
}

// Currently, only one asm.js parallel compilation is allowed at a time.
// This RAII class attempts to claim this parallel compilation using atomic ops
// on the helper thread state's asmJSCompilationInProgress.
class ParallelCompilationGuard
{
    bool parallelState_;
  public:
    ParallelCompilationGuard() : parallelState_(false) {}
    ~ParallelCompilationGuard() {
        if (parallelState_) {
            MOZ_ASSERT(HelperThreadState().asmJSCompilationInProgress == true);
            HelperThreadState().asmJSCompilationInProgress = false;
        }
    }
    bool claim() {
        MOZ_ASSERT(!parallelState_);
        if (!HelperThreadState().asmJSCompilationInProgress.compareExchange(false, true))
            return false;
        parallelState_ = true;
        return true;
    }
};

static bool
ParallelCompilationEnabled(ExclusiveContext* cx)
{
    // If 'cx' isn't a JSContext, then we are already off the main thread so
    // off-thread compilation must be enabled. However, since there are a fixed
    // number of helper threads and one is already being consumed by this
    // parsing task, ensure that there another free thread to avoid deadlock.
    // (Note: there is at most one thread used for parsing so we don't have to
    // worry about general dining philosophers.)
    if (HelperThreadState().threadCount <= 1 || !CanUseExtraThreads())
        return false;

    if (!cx->isJSContext())
        return true;
    return cx->asJSContext()->runtime()->canUseOffthreadIonCompilation();
}

// State of compilation as tracked and updated by the main thread.
struct ParallelGroupState
{
    Vector<AsmJSParallelTask>& tasks;
    int32_t outstandingJobs; // Good work, jobs!
    uint32_t compiledJobs;

    explicit ParallelGroupState(Vector<AsmJSParallelTask>& tasks)
      : tasks(tasks), outstandingJobs(0), compiledJobs(0)
    { }
};

// Block until a helper-assigned LifoAlloc becomes finished.
static AsmJSParallelTask*
GetFinishedCompilation(ModuleCompiler& m, ParallelGroupState& group)
{
    AutoLockHelperThreadState lock;

    while (!HelperThreadState().asmJSFailed()) {
        if (!HelperThreadState().asmJSFinishedList().empty()) {
            group.outstandingJobs--;
            return HelperThreadState().asmJSFinishedList().popCopy();
        }
        HelperThreadState().wait(GlobalHelperThreadState::CONSUMER);
    }

    return nullptr;
}

static bool
GetUsedTask(ModuleCompiler& m, ParallelGroupState& group, AsmJSParallelTask** outTask)
{
    // Block until a used LifoAlloc becomes available.
    AsmJSParallelTask* task = GetFinishedCompilation(m, group);
    if (!task)
        return false;

    auto& func = *reinterpret_cast<AsmFunction*>(task->func);
    func.accumulateCompileTime(task->compileTime);

    // Perform code generation on the main thread.
    if (!GenerateAsmFunctionCode(m, func, *task->mir, *task->lir))
        return false;

    group.compiledJobs++;

    // Clear the LifoAlloc for use by another helper.
    TempAllocator& tempAlloc = task->mir->alloc();
    tempAlloc.TempAllocator::~TempAllocator();
    task->lifo.releaseAll();

    *outTask = task;
    return true;
}

static inline bool
GetUnusedTask(ParallelGroupState& group, uint32_t i, AsmJSParallelTask** outTask)
{
    // Since functions are dispatched in order, if fewer than |numLifos| functions
    // have been generated, then the |i'th| LifoAlloc must never have been
    // assigned to a helper thread.
    if (i >= group.tasks.length())
        return false;
    *outTask = &group.tasks[i];
    return true;
}

static bool
CheckFunctionsParallel(ModuleValidator& m, ParallelGroupState& group,
                       ScopedJSDeletePtr<ModuleCompileResults>* compileResults)
{
#ifdef DEBUG
    {
        AutoLockHelperThreadState lock;
        MOZ_ASSERT(HelperThreadState().asmJSWorklist().empty());
        MOZ_ASSERT(HelperThreadState().asmJSFinishedList().empty());
    }
#endif
    HelperThreadState().resetAsmJSFailureState();

    AsmModuleCompilerScope scope;
    if (!CreateAsmModuleCompiler(m.compileInputs(), &scope))
        return false;
    ModuleCompiler& mc = scope.module();

    AsmJSParallelTask* task = nullptr;
    for (unsigned i = 0;; i++) {
        TokenKind tk;
        if (!PeekToken(m.parser(), &tk))
            return false;
        if (tk != TOK_FUNCTION)
            break;

        if (!task && !GetUnusedTask(group, i, &task) && !GetUsedTask(mc, group, &task))
            return false;

        AsmFunction* func;
        if (!CheckFunction(m, task->lifo, &func))
            return false;

        // In the case of the change-heap function, no function is produced.
        if (!func)
            continue;

        // Generate MIR into the LifoAlloc on the main thread.
        MIRGenerator* mir;
        if (!GenerateAsmFunctionMIR(mc, task->lifo, *func, &mir))
            return false;

        // Perform optimizations and LIR generation on a helper thread.
        task->init(m.cx()->compartment()->runtimeFromAnyThread(), func, mir);
        if (!StartOffThreadAsmJSCompile(m.cx(), task))
            return false;

        group.outstandingJobs++;
        task = nullptr;
    }

    // Block for all outstanding helpers to complete.
    while (group.outstandingJobs > 0) {
        AsmJSParallelTask* ignored = nullptr;
        if (!GetUsedTask(mc, group, &ignored))
            return false;
    }

    if (!CheckAllFunctionsDefined(m))
        return false;

    MOZ_ASSERT(group.outstandingJobs == 0);
    MOZ_ASSERT(group.compiledJobs == m.numFunctions());
#ifdef DEBUG
    {
        AutoLockHelperThreadState lock;
        MOZ_ASSERT(HelperThreadState().asmJSWorklist().empty());
        MOZ_ASSERT(HelperThreadState().asmJSFinishedList().empty());
    }
#endif
    MOZ_ASSERT(!HelperThreadState().asmJSFailed());

    FinishAsmModuleCompilation(mc, compileResults);
    return true;
}

static void
CancelOutstandingJobs(ParallelGroupState& group)
{
    // This is failure-handling code, so it's not allowed to fail. The problem
    // is that all memory for compilation is stored in LifoAllocs maintained in
    // the scope of CheckFunctions() -- so in order for that function to safely
    // return, and thereby remove the LifoAllocs, none of that memory can be in
    // use or reachable by helpers.

    MOZ_ASSERT(group.outstandingJobs >= 0);
    if (!group.outstandingJobs)
        return;

    AutoLockHelperThreadState lock;

    // From the compiling tasks, eliminate those waiting for helper assignation.
    group.outstandingJobs -= HelperThreadState().asmJSWorklist().length();
    HelperThreadState().asmJSWorklist().clear();

    // From the compiling tasks, eliminate those waiting for codegen.
    group.outstandingJobs -= HelperThreadState().asmJSFinishedList().length();
    HelperThreadState().asmJSFinishedList().clear();

    // Eliminate tasks that failed without adding to the finished list.
    group.outstandingJobs -= HelperThreadState().harvestFailedAsmJSJobs();

    // Any remaining tasks are therefore undergoing active compilation.
    MOZ_ASSERT(group.outstandingJobs >= 0);
    while (group.outstandingJobs > 0) {
        HelperThreadState().wait(GlobalHelperThreadState::CONSUMER);

        group.outstandingJobs -= HelperThreadState().harvestFailedAsmJSJobs();
        group.outstandingJobs -= HelperThreadState().asmJSFinishedList().length();
        HelperThreadState().asmJSFinishedList().clear();
    }

    MOZ_ASSERT(group.outstandingJobs == 0);
    MOZ_ASSERT(HelperThreadState().asmJSWorklist().empty());
    MOZ_ASSERT(HelperThreadState().asmJSFinishedList().empty());
}

static const size_t LIFO_ALLOC_PARALLEL_CHUNK_SIZE = 1 << 12;

static bool
CheckFunctions(ModuleValidator& m, ScopedJSDeletePtr<ModuleCompileResults>* results)
{
    // If parallel compilation isn't enabled (not enough cores, disabled by
    // pref, etc) or another thread is currently compiling asm.js in parallel,
    // fall back to sequential compilation. (We could lift the latter
    // constraint by hoisting asmJS* state out of HelperThreadState so multiple
    // concurrent asm.js parallel compilations don't race.)
    ParallelCompilationGuard g;
    if (!ParallelCompilationEnabled(m.cx()) || !g.claim())
        return CheckFunctionsSequential(m, results);

    JitSpew(JitSpew_IonSyncLogs, "Can't log asm.js script. (Compiled on background thread.)");

    // Saturate all helper threads.
    size_t numParallelJobs = HelperThreadState().maxAsmJSCompilationThreads();

    // Allocate scoped AsmJSParallelTask objects. Each contains a unique
    // LifoAlloc that provides all necessary memory for compilation.
    Vector<AsmJSParallelTask, 0> tasks(m.cx());
    if (!tasks.initCapacity(numParallelJobs))
        return false;

    for (size_t i = 0; i < numParallelJobs; i++)
        tasks.infallibleAppend(LIFO_ALLOC_PARALLEL_CHUNK_SIZE);

    // With compilation memory in-scope, dispatch helper threads.
    ParallelGroupState group(tasks);
    if (!CheckFunctionsParallel(m, group, results)) {
        CancelOutstandingJobs(group);

        // If a validation error didn't occur on the main thread, either a
        // syntax error occurred and will be signalled by the regular parser,
        // or an error occurred on an helper thread.
        if (!m.hasAlreadyFailed()) {
            if (void* maybeFunc = HelperThreadState().maybeAsmJSFailedFunction()) {
                AsmFunction* func = reinterpret_cast<AsmFunction*>(maybeFunc);
                return m.failOffset(func->srcBegin(), "allocation failure during compilation");
            }
        }

        // Otherwise, the error occurred on the main thread and was already reported.
        return false;
    }
    return true;
}

static bool
CheckFuncPtrTable(ModuleValidator& m, ParseNode* var)
{
    if (!IsDefinition(var))
        return m.fail(var, "function-pointer table name must be unique");

    ParseNode* arrayLiteral = MaybeDefinitionInitializer(var);
    if (!arrayLiteral || !arrayLiteral->isKind(PNK_ARRAY))
        return m.fail(var, "function-pointer table's initializer must be an array literal");

    unsigned length = ListLength(arrayLiteral);

    if (!IsPowerOfTwo(length))
        return m.failf(arrayLiteral, "function-pointer table length must be a power of 2 (is %u)", length);

    unsigned mask = length - 1;

    ModuleValidator::ConstFuncVector elems(m.lifo());
    const Signature* firstSig = nullptr;

    for (ParseNode* elem = ListHead(arrayLiteral); elem; elem = NextNode(elem)) {
        if (!elem->isKind(PNK_NAME))
            return m.fail(elem, "function-pointer table's elements must be names of functions");

        PropertyName* funcName = elem->name();
        const ModuleValidator::Func* func = m.lookupFunction(funcName);
        if (!func)
            return m.fail(elem, "function-pointer table's elements must be names of functions");

        if (firstSig) {
            if (*firstSig != func->sig())
                return m.fail(elem, "all functions in table must have same signature");
        } else {
            firstSig = &func->sig();
        }

        if (!elems.append(func))
            return false;
    }

    Signature sig(m.lifo());
    if (!sig.copy(*firstSig))
        return false;

    ModuleValidator::FuncPtrTable* table;
    if (!CheckFuncPtrTableAgainstExisting(m, var, var->name(), Move(sig), mask, &table))
        return false;

    table->initElems(Move(elems));
    return true;
}

static bool
CheckFuncPtrTables(ModuleValidator& m)
{
    while (true) {
        ParseNode* varStmt;
        if (!ParseVarOrConstStatement(m.parser(), &varStmt))
            return false;
        if (!varStmt)
            break;
        for (ParseNode* var = VarListHead(varStmt); var; var = NextNode(var)) {
            if (!CheckFuncPtrTable(m, var))
                return false;
        }
    }

    for (unsigned i = 0; i < m.numFuncPtrTables(); i++) {
        ModuleValidator::FuncPtrTable& funcPtrTable = m.funcPtrTable(i);
        if (!funcPtrTable.initialized()) {
            return m.failNameOffset(funcPtrTable.firstUseOffset(),
                                    "function-pointer table %s wasn't defined",
                                    funcPtrTable.name());
        }
    }

    return true;
}

static bool
CheckModuleExportFunction(ModuleValidator& m, ParseNode* pn, PropertyName* maybeFieldName = nullptr)
{
    if (!pn->isKind(PNK_NAME))
        return m.fail(pn, "expected name of exported function");

    PropertyName* funcName = pn->name();
    const ModuleValidator::Global* global = m.lookupGlobal(funcName);
    if (!global)
        return m.failName(pn, "exported function name '%s' not found", funcName);

    if (global->which() == ModuleValidator::Global::Function)
        return m.addExportedFunction(m.function(global->funcIndex()), maybeFieldName);

    if (global->which() == ModuleValidator::Global::ChangeHeap)
        return m.addExportedChangeHeap(funcName, *global, maybeFieldName);

    return m.failName(pn, "'%s' is not a function", funcName);
}

static bool
CheckModuleExportObject(ModuleValidator& m, ParseNode* object)
{
    MOZ_ASSERT(object->isKind(PNK_OBJECT));

    for (ParseNode* pn = ListHead(object); pn; pn = NextNode(pn)) {
        if (!IsNormalObjectField(m.cx(), pn))
            return m.fail(pn, "only normal object properties may be used in the export object literal");

        PropertyName* fieldName = ObjectNormalFieldName(m.cx(), pn);

        ParseNode* initNode = ObjectNormalFieldInitializer(m.cx(), pn);
        if (!initNode->isKind(PNK_NAME))
            return m.fail(initNode, "initializer of exported object literal must be name of function");

        if (!CheckModuleExportFunction(m, initNode, fieldName))
            return false;
    }

    return true;
}

static bool
CheckModuleReturn(ModuleValidator& m)
{
    TokenKind tk;
    if (!GetToken(m.parser(), &tk))
        return false;
    TokenStream& ts = m.parser().tokenStream;
    if (tk != TOK_RETURN) {
        const char* msg = (tk == TOK_RC || tk == TOK_EOF)
                          ? "expecting return statement"
                          : "invalid asm.js. statement";
        return m.failOffset(ts.currentToken().pos.begin, msg);
    }
    ts.ungetToken();

    ParseNode* returnStmt = m.parser().statement(YieldIsName);
    if (!returnStmt)
        return false;

    ParseNode* returnExpr = ReturnExpr(returnStmt);
    if (!returnExpr)
        return m.fail(returnStmt, "export statement must return something");

    if (returnExpr->isKind(PNK_OBJECT)) {
        if (!CheckModuleExportObject(m, returnExpr))
            return false;
    } else {
        if (!CheckModuleExportFunction(m, returnExpr))
            return false;
    }

    // Function statements are not added to the lexical scope in ParseContext
    // (since cx->tempLifoAlloc is marked/released after each function
    // statement) and thus all the identifiers in the return statement will be
    // mistaken as free variables and added to lexdeps. Clear these now.
    m.parser().pc->lexdeps->clear();
    return true;
}

static void
AssertStackAlignment(MacroAssembler& masm, uint32_t alignment, uint32_t addBeforeAssert = 0)
{
    MOZ_ASSERT((sizeof(AsmJSFrame) + masm.framePushed() + addBeforeAssert) % alignment == 0);
    masm.assertStackAlignment(alignment, addBeforeAssert);
}

static unsigned
StackDecrementForCall(MacroAssembler& masm, uint32_t alignment, unsigned bytesToPush)
{
    return StackDecrementForCall(alignment, sizeof(AsmJSFrame) + masm.framePushed(), bytesToPush);
}

template <class VectorT>
static unsigned
StackArgBytes(const VectorT& argTypes)
{
    ABIArgIter<VectorT> iter(argTypes);
    while (!iter.done())
        iter++;
    return iter.stackBytesConsumedSoFar();
}

template <class VectorT>
static unsigned
StackDecrementForCall(MacroAssembler& masm, uint32_t alignment, const VectorT& argTypes,
                      unsigned extraBytes = 0)
{
    return StackDecrementForCall(masm, alignment, StackArgBytes(argTypes) + extraBytes);
}

#if defined(JS_CODEGEN_ARM)
// The ARM system ABI also includes d15 & s31 in the non volatile float registers.
// Also exclude lr (a.k.a. r14) as we preserve it manually)
static const LiveRegisterSet NonVolatileRegs =
    LiveRegisterSet(GeneralRegisterSet(Registers::NonVolatileMask&
                                       ~(uint32_t(1) << Registers::lr)),
                    FloatRegisterSet(FloatRegisters::NonVolatileMask
                                     | (1ULL << FloatRegisters::d15)
                                     | (1ULL << FloatRegisters::s31)));
#else
static const LiveRegisterSet NonVolatileRegs =
    LiveRegisterSet(GeneralRegisterSet(Registers::NonVolatileMask),
                    FloatRegisterSet(FloatRegisters::NonVolatileMask));
#endif

#if defined(JS_CODEGEN_MIPS32)
// Mips is using one more double slot due to stack alignment for double values.
// Look at MacroAssembler::PushRegsInMask(RegisterSet set)
static const unsigned FramePushedAfterSave = NonVolatileRegs.gprs().size() * sizeof(intptr_t) +
                                             NonVolatileRegs.fpus().getPushSizeInBytes() +
                                             sizeof(double);
#elif defined(JS_CODEGEN_NONE)
static const unsigned FramePushedAfterSave = 0;
#else
static const unsigned FramePushedAfterSave = NonVolatileRegs.gprs().size() * sizeof(intptr_t)
                                           + NonVolatileRegs.fpus().getPushSizeInBytes();
#endif
static const unsigned FramePushedForEntrySP = FramePushedAfterSave + sizeof(void*);

static bool
GenerateEntry(ModuleValidator& m, unsigned exportIndex)
{
    MacroAssembler& masm = m.masm();

    Label begin;
    masm.haltingAlign(CodeAlignment);
    masm.bind(&begin);

    // Save the return address if it wasn't already saved by the call insn.
#if defined(JS_CODEGEN_ARM)
    masm.push(lr);
#elif defined(JS_CODEGEN_MIPS32)
    masm.push(ra);
#elif defined(JS_CODEGEN_X86)
    static const unsigned EntryFrameSize = sizeof(void*);
#endif

    // Save all caller non-volatile registers before we clobber them here and in
    // the asm.js callee (which does not preserve non-volatile registers).
    masm.setFramePushed(0);
    masm.PushRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == FramePushedAfterSave);

    // ARM and MIPS have a globally-pinned GlobalReg (x64 uses RIP-relative
    // addressing, x86 uses immediates in effective addresses). For the
    // AsmJSGlobalRegBias addition, see Assembler-(mips,arm).h.
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32)
    masm.movePtr(IntArgReg1, GlobalReg);
    masm.addPtr(Imm32(AsmJSGlobalRegBias), GlobalReg);
#endif

    // ARM, MIPS and x64 have a globally-pinned HeapReg (x86 uses immediates in
    // effective addresses). Loading the heap register depends on the global
    // register already having been loaded.
    masm.loadAsmJSHeapRegisterFromGlobalData();

    // Put the 'argv' argument into a non-argument/return register so that we
    // can use 'argv' while we fill in the arguments for the asm.js callee.
    // Also, save 'argv' on the stack so that we can recover it after the call.
    // Use a second non-argument/return register as temporary scratch.
    Register argv = ABIArgGenerator::NonArgReturnReg0;
    Register scratch = ABIArgGenerator::NonArgReturnReg1;
#if defined(JS_CODEGEN_X86)
    masm.loadPtr(Address(masm.getStackPointer(), EntryFrameSize + masm.framePushed()), argv);
#else
    masm.movePtr(IntArgReg0, argv);
#endif
    masm.Push(argv);

    // Save the stack pointer to the saved non-volatile registers. We will use
    // this on two paths: normal return and exceptional return. Since
    // loadAsmJSActivation uses GlobalReg, we must do this after loading
    // GlobalReg.
    MOZ_ASSERT(masm.framePushed() == FramePushedForEntrySP);
    masm.loadAsmJSActivation(scratch);
    masm.storeStackPtr(Address(scratch, AsmJSActivation::offsetOfEntrySP()));

    // Dynamically align the stack since ABIStackAlignment is not necessarily
    // AsmJSStackAlignment. We'll use entrySP to recover the original stack
    // pointer on return.
    masm.andToStackPtr(Imm32(~(AsmJSStackAlignment - 1)));

    // Bump the stack for the call.
    PropertyName* funcName = m.module().exportedFunction(exportIndex).name();
    const ModuleValidator::Func& func = *m.lookupFunction(funcName);
    masm.reserveStack(AlignBytes(StackArgBytes(func.sig().args()), AsmJSStackAlignment));

    // Copy parameters out of argv and into the registers/stack-slots specified by
    // the system ABI.
    for (ABIArgTypeIter iter(func.sig().args()); !iter.done(); iter++) {
        unsigned argOffset = iter.index() * sizeof(AsmJSModule::EntryArg);
        Address src(argv, argOffset);
        MIRType type = iter.mirType();
        switch (iter->kind()) {
          case ABIArg::GPR:
            masm.load32(src, iter->gpr());
            break;
#ifdef JS_CODEGEN_REGISTER_PAIR
          case ABIArg::GPR_PAIR:
            MOZ_CRASH("AsmJS uses hardfp for function calls.");
            break;
#endif
          case ABIArg::FPU: {
            static_assert(sizeof(AsmJSModule::EntryArg) >= jit::Simd128DataSize,
                          "EntryArg must be big enough to store SIMD values");
            switch (type) {
              case MIRType_Int32x4:
                masm.loadUnalignedInt32x4(src, iter->fpu());
                break;
              case MIRType_Float32x4:
                masm.loadUnalignedFloat32x4(src, iter->fpu());
                break;
              case MIRType_Double:
                masm.loadDouble(src, iter->fpu());
                break;
              case MIRType_Float32:
                masm.loadFloat32(src, iter->fpu());
                break;
              default:
                MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected FPU type");
                break;
            }
            break;
          }
          case ABIArg::Stack:
            switch (type) {
              case MIRType_Int32:
                masm.load32(src, scratch);
                masm.storePtr(scratch, Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType_Double:
                masm.loadDouble(src, ScratchDoubleReg);
                masm.storeDouble(ScratchDoubleReg, Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType_Float32:
                masm.loadFloat32(src, ScratchFloat32Reg);
                masm.storeFloat32(ScratchFloat32Reg, Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType_Int32x4:
                masm.loadUnalignedInt32x4(src, ScratchSimdReg);
                masm.storeAlignedInt32x4(ScratchSimdReg,
                                         Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              case MIRType_Float32x4:
                masm.loadUnalignedFloat32x4(src, ScratchSimdReg);
                masm.storeAlignedFloat32x4(ScratchSimdReg,
                                           Address(masm.getStackPointer(), iter->offsetFromArgBase()));
                break;
              default:
                MOZ_MAKE_COMPILER_ASSUME_IS_UNREACHABLE("unexpected stack arg type");
            }
            break;
        }
    }

    // Call into the real function.
    masm.assertStackAlignment(AsmJSStackAlignment);
    masm.call(CallSiteDesc(CallSiteDesc::Relative), m.functionEntry(func.funcIndex()));

    // Recover the stack pointer value before dynamic alignment.
    masm.loadAsmJSActivation(scratch);
    masm.loadStackPtr(Address(scratch, AsmJSActivation::offsetOfEntrySP()));
    masm.setFramePushed(FramePushedForEntrySP);

    // Recover the 'argv' pointer which was saved before aligning the stack.
    masm.Pop(argv);

    // Store the return value in argv[0]
    switch (func.sig().retType().which()) {
      case RetType::Void:
        break;
      case RetType::Signed:
        masm.storeValue(JSVAL_TYPE_INT32, ReturnReg, Address(argv, 0));
        break;
      case RetType::Float:
        masm.convertFloat32ToDouble(ReturnFloat32Reg, ReturnDoubleReg);
        // Fall through as ReturnDoubleReg now contains a Double
      case RetType::Double:
        masm.canonicalizeDouble(ReturnDoubleReg);
        masm.storeDouble(ReturnDoubleReg, Address(argv, 0));
        break;
      case RetType::Int32x4:
        // We don't have control on argv alignment, do an unaligned access.
        masm.storeUnalignedInt32x4(ReturnInt32x4Reg, Address(argv, 0));
        break;
      case RetType::Float32x4:
        // We don't have control on argv alignment, do an unaligned access.
        masm.storeUnalignedFloat32x4(ReturnFloat32x4Reg, Address(argv, 0));
        break;
    }

    // Restore clobbered non-volatile registers of the caller.
    masm.PopRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == 0);

    masm.move32(Imm32(true), ReturnReg);
    masm.ret();

    return !masm.oom() && m.finishGeneratingEntry(exportIndex, &begin);
}

static void
FillArgumentArray(ModuleValidator& m, const VarTypeVector& argTypes,
                  unsigned offsetToArgs, unsigned offsetToCallerStackArgs,
                  Register scratch)
{
    MacroAssembler& masm = m.masm();

    for (ABIArgTypeIter i(argTypes); !i.done(); i++) {
        Address dstAddr(masm.getStackPointer(), offsetToArgs + i.index() * sizeof(Value));
        switch (i->kind()) {
          case ABIArg::GPR:
            masm.storeValue(JSVAL_TYPE_INT32, i->gpr(), dstAddr);
            break;
#ifdef JS_CODEGEN_REGISTER_PAIR
          case ABIArg::GPR_PAIR:
            MOZ_CRASH("AsmJS uses hardfp for function calls.");
            break;
#endif
          case ABIArg::FPU:
            masm.canonicalizeDouble(i->fpu());
            masm.storeDouble(i->fpu(), dstAddr);
            break;
          case ABIArg::Stack:
            if (i.mirType() == MIRType_Int32) {
                Address src(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
                masm.load32(src, scratch);
                masm.storeValue(JSVAL_TYPE_INT32, scratch, dstAddr);
#else
                masm.memIntToValue(src, dstAddr);
#endif
            } else {
                MOZ_ASSERT(i.mirType() == MIRType_Double);
                Address src(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
                masm.loadDouble(src, ScratchDoubleReg);
                masm.canonicalizeDouble(ScratchDoubleReg);
                masm.storeDouble(ScratchDoubleReg, dstAddr);
            }
            break;
        }
    }
}

// If an FFI detaches its heap (viz., via ArrayBuffer.transfer), it must
// call change-heap to another heap (viz., the new heap returned by transfer)
// before returning to asm.js code. If the application fails to do this (if the
// heap pointer is null), jump to a stub.
static void
GenerateCheckForHeapDetachment(ModuleValidator& m, Register scratch)
{
    if (!m.module().hasArrayView())
        return;

    MacroAssembler& masm = m.masm();
    MOZ_ASSERT(int(masm.framePushed()) >= int(ShadowStackSpace));
    AssertStackAlignment(masm, ABIStackAlignment);
#if defined(JS_CODEGEN_X86)
    CodeOffsetLabel label = masm.movlWithPatch(PatchedAbsoluteAddress(), scratch);
    masm.append(AsmJSGlobalAccess(label, AsmJSHeapGlobalDataOffset));
    masm.branchTestPtr(Assembler::Zero, scratch, scratch, &m.onDetachedLabel());
#else
    masm.branchTestPtr(Assembler::Zero, HeapReg, HeapReg, &m.onDetachedLabel());
#endif
}

static bool
GenerateFFIInterpExit(ModuleValidator& m, const Signature& sig, unsigned exitIndex,
                      Label* throwLabel)
{
    MacroAssembler& masm = m.masm();
    MOZ_ASSERT(masm.framePushed() == 0);

    // Argument types for InvokeFromAsmJS_*:
    static const MIRType typeArray[] = { MIRType_Pointer,   // exitDatum
                                         MIRType_Int32,     // argc
                                         MIRType_Pointer }; // argv
    MIRTypeVector invokeArgTypes(m.cx());
    if (!invokeArgTypes.append(typeArray, ArrayLength(typeArray)))
        return false;

    // At the point of the call, the stack layout shall be (sp grows to the left):
    //   | stack args | padding | Value argv[] | padding | retaddr | caller stack args |
    // The padding between stack args and argv ensures that argv is aligned. The
    // padding between argv and retaddr ensures that sp is aligned.
    unsigned offsetToArgv = AlignBytes(StackArgBytes(invokeArgTypes), sizeof(double));
    unsigned argvBytes = Max<size_t>(1, sig.args().length()) * sizeof(Value);
    unsigned framePushed = StackDecrementForCall(masm, ABIStackAlignment, offsetToArgv + argvBytes);

    Label begin;
    GenerateAsmJSExitPrologue(masm, framePushed, AsmJSExit::SlowFFI, &begin);

    // Fill the argument array.
    unsigned offsetToCallerStackArgs = sizeof(AsmJSFrame) + masm.framePushed();
    Register scratch = ABIArgGenerator::NonArgReturnReg0;
    FillArgumentArray(m, sig.args(), offsetToArgv, offsetToCallerStackArgs, scratch);

    // Prepare the arguments for the call to InvokeFromAsmJS_*.
    ABIArgMIRTypeIter i(invokeArgTypes);

    // argument 0: exitIndex
    if (i->kind() == ABIArg::GPR)
        masm.mov(ImmWord(exitIndex), i->gpr());
    else
        masm.store32(Imm32(exitIndex), Address(masm.getStackPointer(), i->offsetFromArgBase()));
    i++;

    // argument 1: argc
    unsigned argc = sig.args().length();
    if (i->kind() == ABIArg::GPR)
        masm.mov(ImmWord(argc), i->gpr());
    else
        masm.store32(Imm32(argc), Address(masm.getStackPointer(), i->offsetFromArgBase()));
    i++;

    // argument 2: argv
    Address argv(masm.getStackPointer(), offsetToArgv);
    if (i->kind() == ABIArg::GPR) {
        masm.computeEffectiveAddress(argv, i->gpr());
    } else {
        masm.computeEffectiveAddress(argv, scratch);
        masm.storePtr(scratch, Address(masm.getStackPointer(), i->offsetFromArgBase()));
    }
    i++;
    MOZ_ASSERT(i.done());

    // Make the call, test whether it succeeded, and extract the return value.
    AssertStackAlignment(masm, ABIStackAlignment);
    switch (sig.retType().which()) {
      case RetType::Void:
        masm.call(AsmJSImmPtr(AsmJSImm_InvokeFromAsmJS_Ignore));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        break;
      case RetType::Signed:
        masm.call(AsmJSImmPtr(AsmJSImm_InvokeFromAsmJS_ToInt32));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.unboxInt32(argv, ReturnReg);
        break;
      case RetType::Double:
        masm.call(AsmJSImmPtr(AsmJSImm_InvokeFromAsmJS_ToNumber));
        masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
        masm.loadDouble(argv, ReturnDoubleReg);
        break;
      case RetType::Float:
        MOZ_CRASH("Float32 shouldn't be returned from a FFI");
      case RetType::Int32x4:
      case RetType::Float32x4:
        MOZ_CRASH("SIMD types shouldn't be returned from a FFI");
    }

    // The heap pointer may have changed during the FFI, so reload it and test
    // for detachment.
    masm.loadAsmJSHeapRegisterFromGlobalData();
    GenerateCheckForHeapDetachment(m, ABIArgGenerator::NonReturn_VolatileReg0);

    Label profilingReturn;
    GenerateAsmJSExitEpilogue(masm, framePushed, AsmJSExit::SlowFFI, &profilingReturn);
    return !masm.oom() && m.finishGeneratingInterpExit(exitIndex, &begin, &profilingReturn);
}

#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32)
static const unsigned MaybeSavedGlobalReg = sizeof(void*);
#else
static const unsigned MaybeSavedGlobalReg = 0;
#endif

static bool
GenerateFFIIonExit(ModuleValidator& m, const Signature& sig, unsigned exitIndex,
                   Label* throwLabel)
{
    MacroAssembler& masm = m.masm();
    MOZ_ASSERT(masm.framePushed() == 0);

    // Ion calls use the following stack layout (sp grows to the left):
    //   | retaddr | descriptor | callee | argc | this | arg1..N |
    // After the Ion frame, the global register (if present) is saved since Ion
    // does not preserve non-volatile regs. Also, unlike most ABIs, Ion requires
    // that sp be JitStackAlignment-aligned *after* pushing the return address.
    static_assert(AsmJSStackAlignment >= JitStackAlignment, "subsumes");
    unsigned sizeOfRetAddr = sizeof(void*);
    unsigned ionFrameBytes = 3 * sizeof(void*) + (1 + sig.args().length()) * sizeof(Value);
    unsigned totalIonBytes = sizeOfRetAddr + ionFrameBytes + MaybeSavedGlobalReg;
    unsigned ionFramePushed = StackDecrementForCall(masm, JitStackAlignment, totalIonBytes) -
                              sizeOfRetAddr;

    Label begin;
    GenerateAsmJSExitPrologue(masm, ionFramePushed, AsmJSExit::JitFFI, &begin);

    // 1. Descriptor
    size_t argOffset = 0;
    uint32_t descriptor = MakeFrameDescriptor(ionFramePushed, JitFrame_Entry);
    masm.storePtr(ImmWord(uintptr_t(descriptor)), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 2. Callee
    Register callee = ABIArgGenerator::NonArgReturnReg0;   // live until call
    Register scratch = ABIArgGenerator::NonArgReturnReg1;  // repeatedly clobbered

    // 2.1. Get ExitDatum
    unsigned globalDataOffset = m.module().exitIndexToGlobalDataOffset(exitIndex);
#if defined(JS_CODEGEN_X64)
    m.masm().append(AsmJSGlobalAccess(masm.leaRipRelative(callee), globalDataOffset));
#elif defined(JS_CODEGEN_X86)
    m.masm().append(AsmJSGlobalAccess(masm.movlWithPatch(Imm32(0), callee), globalDataOffset));
#elif defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32)
    masm.computeEffectiveAddress(Address(GlobalReg, globalDataOffset - AsmJSGlobalRegBias), callee);
#endif

    // 2.2. Get callee
    masm.loadPtr(Address(callee, offsetof(AsmJSModule::ExitDatum, fun)), callee);

    // 2.3. Save callee
    masm.storePtr(callee, Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 2.4. Load callee executable entry point
    masm.loadPtr(Address(callee, JSFunction::offsetOfNativeOrScript()), callee);
    masm.loadBaselineOrIonNoArgCheck(callee, callee, nullptr);

    // 3. Argc
    unsigned argc = sig.args().length();
    masm.storePtr(ImmWord(uintptr_t(argc)), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(size_t);

    // 4. |this| value
    masm.storeValue(UndefinedValue(), Address(masm.getStackPointer(), argOffset));
    argOffset += sizeof(Value);

    // 5. Fill the arguments
    unsigned offsetToCallerStackArgs = ionFramePushed + sizeof(AsmJSFrame);
    FillArgumentArray(m, sig.args(), argOffset, offsetToCallerStackArgs, scratch);
    argOffset += sig.args().length() * sizeof(Value);
    MOZ_ASSERT(argOffset == ionFrameBytes);

    // 6. Jit code will clobber all registers, even non-volatiles. GlobalReg and
    //    HeapReg are removed from the general register set for asm.js code, so
    //    these will not have been saved by the caller like all other registers,
    //    so they must be explicitly preserved. Only save GlobalReg since
    //    HeapReg must be reloaded (from global data) after the call since the
    //    heap may change during the FFI call.
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32)
    static_assert(MaybeSavedGlobalReg == sizeof(void*), "stack frame accounting");
    masm.storePtr(GlobalReg, Address(masm.getStackPointer(), ionFrameBytes));
#endif

    {
        // Enable Activation.
        //
        // This sequence requires four registers, and needs to preserve the 'callee'
        // register, so there are five live registers.
        MOZ_ASSERT(callee == AsmJSIonExitRegCallee);
        Register reg0 = AsmJSIonExitRegE0;
        Register reg1 = AsmJSIonExitRegE1;
        Register reg2 = AsmJSIonExitRegE2;
        Register reg3 = AsmJSIonExitRegE3;

        // The following is inlined:
        //   JSContext* cx = activation->cx();
        //   Activation* act = cx->runtime()->activation();
        //   act.active_ = true;
        //   act.prevJitTop_ = cx->runtime()->jitTop;
        //   act.prevJitJSContext_ = cx->runtime()->jitJSContext;
        //   cx->runtime()->jitJSContext = cx;
        //   act.prevJitActivation_ = cx->runtime()->jitActivation;
        //   cx->runtime()->jitActivation = act;
        //   act.prevProfilingActivation_ = cx->runtime()->profilingActivation;
        //   cx->runtime()->profilingActivation_ = act;
        // On the ARM store8() uses the secondScratchReg (lr) as a temp.
        size_t offsetOfActivation = JSRuntime::offsetOfActivation();
        size_t offsetOfJitTop = offsetof(JSRuntime, jitTop);
        size_t offsetOfJitJSContext = offsetof(JSRuntime, jitJSContext);
        size_t offsetOfJitActivation = offsetof(JSRuntime, jitActivation);
        size_t offsetOfProfilingActivation = JSRuntime::offsetOfProfilingActivation();
        masm.loadAsmJSActivation(reg0);
        masm.loadPtr(Address(reg0, AsmJSActivation::offsetOfContext()), reg3);
        masm.loadPtr(Address(reg3, JSContext::offsetOfRuntime()), reg0);
        masm.loadPtr(Address(reg0, offsetOfActivation), reg1);

        //   act.active_ = true;
        masm.store8(Imm32(1), Address(reg1, JitActivation::offsetOfActiveUint8()));

        //   act.prevJitTop_ = cx->runtime()->jitTop;
        masm.loadPtr(Address(reg0, offsetOfJitTop), reg2);
        masm.storePtr(reg2, Address(reg1, JitActivation::offsetOfPrevJitTop()));

        //   act.prevJitJSContext_ = cx->runtime()->jitJSContext;
        masm.loadPtr(Address(reg0, offsetOfJitJSContext), reg2);
        masm.storePtr(reg2, Address(reg1, JitActivation::offsetOfPrevJitJSContext()));
        //   cx->runtime()->jitJSContext = cx;
        masm.storePtr(reg3, Address(reg0, offsetOfJitJSContext));

        //   act.prevJitActivation_ = cx->runtime()->jitActivation;
        masm.loadPtr(Address(reg0, offsetOfJitActivation), reg2);
        masm.storePtr(reg2, Address(reg1, JitActivation::offsetOfPrevJitActivation()));
        //   cx->runtime()->jitActivation = act;
        masm.storePtr(reg1, Address(reg0, offsetOfJitActivation));

        //   act.prevProfilingActivation_ = cx->runtime()->profilingActivation;
        masm.loadPtr(Address(reg0, offsetOfProfilingActivation), reg2);
        masm.storePtr(reg2, Address(reg1, Activation::offsetOfPrevProfiling()));
        //   cx->runtime()->profilingActivation_ = act;
        masm.storePtr(reg1, Address(reg0, offsetOfProfilingActivation));
    }

    AssertStackAlignment(masm, JitStackAlignment, sizeOfRetAddr);
    masm.callJitNoProfiler(callee);
    AssertStackAlignment(masm, JitStackAlignment, sizeOfRetAddr);

    {
        // Disable Activation.
        //
        // This sequence needs three registers, and must preserve the JSReturnReg_Data and
        // JSReturnReg_Type, so there are five live registers.
        MOZ_ASSERT(JSReturnReg_Data == AsmJSIonExitRegReturnData);
        MOZ_ASSERT(JSReturnReg_Type == AsmJSIonExitRegReturnType);
        Register reg0 = AsmJSIonExitRegD0;
        Register reg1 = AsmJSIonExitRegD1;
        Register reg2 = AsmJSIonExitRegD2;

        // The following is inlined:
        //   rt->profilingActivation = prevProfilingActivation_;
        //   rt->activation()->active_ = false;
        //   rt->jitTop = prevJitTop_;
        //   rt->jitJSContext = prevJitJSContext_;
        //   rt->jitActivation = prevJitActivation_;
        // On the ARM store8() uses the secondScratchReg (lr) as a temp.
        size_t offsetOfActivation = JSRuntime::offsetOfActivation();
        size_t offsetOfJitTop = offsetof(JSRuntime, jitTop);
        size_t offsetOfJitJSContext = offsetof(JSRuntime, jitJSContext);
        size_t offsetOfJitActivation = offsetof(JSRuntime, jitActivation);
        size_t offsetOfProfilingActivation = JSRuntime::offsetOfProfilingActivation();

        masm.movePtr(AsmJSImmPtr(AsmJSImm_Runtime), reg0);
        masm.loadPtr(Address(reg0, offsetOfActivation), reg1);

        //   rt->jitTop = prevJitTop_;
        masm.loadPtr(Address(reg1, JitActivation::offsetOfPrevJitTop()), reg2);
        masm.storePtr(reg2, Address(reg0, offsetOfJitTop));

        //   rt->profilingActivation = rt->activation()->prevProfiling_;
        masm.loadPtr(Address(reg1, Activation::offsetOfPrevProfiling()), reg2);
        masm.storePtr(reg2, Address(reg0, offsetOfProfilingActivation));

        //   rt->activation()->active_ = false;
        masm.store8(Imm32(0), Address(reg1, JitActivation::offsetOfActiveUint8()));

        //   rt->jitJSContext = prevJitJSContext_;
        masm.loadPtr(Address(reg1, JitActivation::offsetOfPrevJitJSContext()), reg2);
        masm.storePtr(reg2, Address(reg0, offsetOfJitJSContext));

        //   rt->jitActivation = prevJitActivation_;
        masm.loadPtr(Address(reg1, JitActivation::offsetOfPrevJitActivation()), reg2);
        masm.storePtr(reg2, Address(reg0, offsetOfJitActivation));
    }

    // Reload the global register since Ion code can clobber any register.
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS32)
    static_assert(MaybeSavedGlobalReg == sizeof(void*), "stack frame accounting");
    masm.loadPtr(Address(masm.getStackPointer(), ionFrameBytes), GlobalReg);
#endif

    // As explained above, the frame was aligned for Ion such that
    //   (sp + sizeof(void*)) % JitStackAlignment == 0
    // But now we possibly want to call one of several different C++ functions,
    // so subtract the sizeof(void*) so that sp is aligned for an ABI call.
    static_assert(ABIStackAlignment <= JitStackAlignment, "subsumes");
    masm.reserveStack(sizeOfRetAddr);
    unsigned nativeFramePushed = masm.framePushed();
    AssertStackAlignment(masm, ABIStackAlignment);

    masm.branchTestMagic(Assembler::Equal, JSReturnOperand, throwLabel);

    Label oolConvert;
    switch (sig.retType().which()) {
      case RetType::Void:
        break;
      case RetType::Signed:
        masm.convertValueToInt32(JSReturnOperand, ReturnDoubleReg, ReturnReg, &oolConvert,
                                 /* -0 check */ false);
        break;
      case RetType::Double:
        masm.convertValueToDouble(JSReturnOperand, ReturnDoubleReg, &oolConvert);
        break;
      case RetType::Float:
        MOZ_CRASH("Float shouldn't be returned from a FFI");
      case RetType::Int32x4:
      case RetType::Float32x4:
        MOZ_CRASH("SIMD types shouldn't be returned from a FFI");
    }

    Label done;
    masm.bind(&done);

    // The heap pointer has to be reloaded anyway since Ion could have clobbered
    // it. Additionally, the FFI may have detached the heap buffer.
    masm.loadAsmJSHeapRegisterFromGlobalData();
    GenerateCheckForHeapDetachment(m, ABIArgGenerator::NonReturn_VolatileReg0);

    Label profilingReturn;
    GenerateAsmJSExitEpilogue(masm, masm.framePushed(), AsmJSExit::JitFFI, &profilingReturn);

    if (oolConvert.used()) {
        masm.bind(&oolConvert);
        masm.setFramePushed(nativeFramePushed);

        // Coercion calls use the following stack layout (sp grows to the left):
        //   | args | padding | Value argv[1] | padding | exit AsmJSFrame |
        MIRTypeVector coerceArgTypes(m.cx());
        JS_ALWAYS_TRUE(coerceArgTypes.append(MIRType_Pointer));
        unsigned offsetToCoerceArgv = AlignBytes(StackArgBytes(coerceArgTypes), sizeof(Value));
        MOZ_ASSERT(nativeFramePushed >= offsetToCoerceArgv + sizeof(Value));
        AssertStackAlignment(masm, ABIStackAlignment);

        // Store return value into argv[0]
        masm.storeValue(JSReturnOperand, Address(masm.getStackPointer(), offsetToCoerceArgv));

        // argument 0: argv
        ABIArgMIRTypeIter i(coerceArgTypes);
        Address argv(masm.getStackPointer(), offsetToCoerceArgv);
        if (i->kind() == ABIArg::GPR) {
            masm.computeEffectiveAddress(argv, i->gpr());
        } else {
            masm.computeEffectiveAddress(argv, scratch);
            masm.storePtr(scratch, Address(masm.getStackPointer(), i->offsetFromArgBase()));
        }
        i++;
        MOZ_ASSERT(i.done());

        // Call coercion function
        AssertStackAlignment(masm, ABIStackAlignment);
        switch (sig.retType().which()) {
          case RetType::Signed:
            masm.call(AsmJSImmPtr(AsmJSImm_CoerceInPlace_ToInt32));
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
            masm.unboxInt32(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnReg);
            break;
          case RetType::Double:
            masm.call(AsmJSImmPtr(AsmJSImm_CoerceInPlace_ToNumber));
            masm.branchTest32(Assembler::Zero, ReturnReg, ReturnReg, throwLabel);
            masm.loadDouble(Address(masm.getStackPointer(), offsetToCoerceArgv), ReturnDoubleReg);
            break;
          default:
            MOZ_CRASH("Unsupported convert type");
        }

        masm.jump(&done);
        masm.setFramePushed(0);
    }

    MOZ_ASSERT(masm.framePushed() == 0);

    return !masm.oom() && m.finishGeneratingJitExit(exitIndex, &begin, &profilingReturn);
}

// See "asm.js FFI calls" comment above.
static bool
GenerateFFIExits(ModuleValidator& m, unsigned exitIndex, const Signature& signature,
                 Label* throwLabel)
{
    // Generate the slow path through the interpreter
    if (!GenerateFFIInterpExit(m, signature, exitIndex, throwLabel))
        return false;

    // Generate the fast path
    if (!GenerateFFIIonExit(m, signature, exitIndex, throwLabel))
        return false;

    return true;
}

// Generate a thunk that updates fp before calling the given builtin so that
// both the builtin and the calling function show up in profiler stacks. (This
// thunk is dynamically patched in when profiling is enabled.) Since the thunk
// pushes an AsmJSFrame on the stack, that means we must rebuild the stack
// frame. Fortunately, these are low arity functions and everything is passed in
// regs on everything but x86 anyhow.
//
// NB: Since this thunk is being injected at system ABI callsites, it must
//     preserve the argument registers (going in) and the return register
//     (coming out) and preserve non-volatile registers.
static bool
GenerateBuiltinThunk(ModuleValidator& m, AsmJSExit::BuiltinKind builtin)
{
    MacroAssembler& masm = m.masm();
    MOZ_ASSERT(masm.framePushed() == 0);

    MIRTypeVector argTypes(m.cx());
    if (!argTypes.reserve(5))
        return false;

    switch (builtin) {
      case AsmJSExit::Builtin_ToInt32:
        argTypes.infallibleAppend(MIRType_Int32);
        break;
#if defined(JS_CODEGEN_ARM)
      case AsmJSExit::Builtin_IDivMod:
      case AsmJSExit::Builtin_UDivMod:
        argTypes.infallibleAppend(MIRType_Int32);
        argTypes.infallibleAppend(MIRType_Int32);
        break;
      case AsmJSExit::Builtin_AtomicCmpXchg:
        argTypes.infallibleAppend(MIRType_Int32);
        argTypes.infallibleAppend(MIRType_Int32);
        argTypes.infallibleAppend(MIRType_Int32);
        argTypes.infallibleAppend(MIRType_Int32);
        break;
      case AsmJSExit::Builtin_AtomicXchg:
      case AsmJSExit::Builtin_AtomicFetchAdd:
      case AsmJSExit::Builtin_AtomicFetchSub:
      case AsmJSExit::Builtin_AtomicFetchAnd:
      case AsmJSExit::Builtin_AtomicFetchOr:
      case AsmJSExit::Builtin_AtomicFetchXor:
        argTypes.infallibleAppend(MIRType_Int32);
        argTypes.infallibleAppend(MIRType_Int32);
        argTypes.infallibleAppend(MIRType_Int32);
        break;
#endif
      case AsmJSExit::Builtin_SinD:
      case AsmJSExit::Builtin_CosD:
      case AsmJSExit::Builtin_TanD:
      case AsmJSExit::Builtin_ASinD:
      case AsmJSExit::Builtin_ACosD:
      case AsmJSExit::Builtin_ATanD:
      case AsmJSExit::Builtin_CeilD:
      case AsmJSExit::Builtin_FloorD:
      case AsmJSExit::Builtin_ExpD:
      case AsmJSExit::Builtin_LogD:
        argTypes.infallibleAppend(MIRType_Double);
        break;
      case AsmJSExit::Builtin_ModD:
      case AsmJSExit::Builtin_PowD:
      case AsmJSExit::Builtin_ATan2D:
        argTypes.infallibleAppend(MIRType_Double);
        argTypes.infallibleAppend(MIRType_Double);
        break;
      case AsmJSExit::Builtin_CeilF:
      case AsmJSExit::Builtin_FloorF:
        argTypes.infallibleAppend(MIRType_Float32);
        break;
      case AsmJSExit::Builtin_Limit:
        MOZ_CRASH("Bad builtin");
    }

    uint32_t framePushed = StackDecrementForCall(masm, ABIStackAlignment, argTypes);

    Label begin;
    GenerateAsmJSExitPrologue(masm, framePushed, AsmJSExit::Builtin(builtin), &begin);

    for (ABIArgMIRTypeIter i(argTypes); !i.done(); i++) {
        if (i->kind() != ABIArg::Stack)
            continue;
#if !defined(JS_CODEGEN_ARM)
        unsigned offsetToCallerStackArgs = sizeof(AsmJSFrame) + masm.framePushed();
        Address srcAddr(masm.getStackPointer(), offsetToCallerStackArgs + i->offsetFromArgBase());
        Address dstAddr(masm.getStackPointer(), i->offsetFromArgBase());
        if (i.mirType() == MIRType_Int32 || i.mirType() == MIRType_Float32) {
            masm.load32(srcAddr, ABIArgGenerator::NonArg_VolatileReg);
            masm.store32(ABIArgGenerator::NonArg_VolatileReg, dstAddr);
        } else {
            MOZ_ASSERT(i.mirType() == MIRType_Double);
            masm.loadDouble(srcAddr, ScratchDoubleReg);
            masm.storeDouble(ScratchDoubleReg, dstAddr);
        }
#else
        MOZ_CRASH("Architecture should have enough registers for all builtin calls");
#endif
    }

    AssertStackAlignment(masm, ABIStackAlignment);
    masm.call(BuiltinToImmKind(builtin));

    Label profilingReturn;
    GenerateAsmJSExitEpilogue(masm, framePushed, AsmJSExit::Builtin(builtin), &profilingReturn);
    return !masm.oom() && m.finishGeneratingBuiltinThunk(builtin, &begin, &profilingReturn);
}

static bool
GenerateStackOverflowExit(ModuleValidator& m, Label* throwLabel)
{
    MacroAssembler& masm = m.masm();
    GenerateAsmJSStackOverflowExit(masm, &m.stackOverflowLabel(), throwLabel);
    return !masm.oom() && m.finishGeneratingInlineStub(&m.stackOverflowLabel());
}

static bool
GenerateOnDetachedLabelExit(ModuleValidator& m, Label* throwLabel)
{
    MacroAssembler& masm = m.masm();
    masm.bind(&m.onDetachedLabel());
    masm.assertStackAlignment(ABIStackAlignment);

    // For now, OnDetached always throws (see OnDetached comment).
    masm.call(AsmJSImmPtr(AsmJSImm_OnDetached));
    masm.jump(throwLabel);

    return !masm.oom() && m.finishGeneratingInlineStub(&m.onDetachedLabel());
}

static bool
GenerateExceptionLabelExit(ModuleValidator& m, Label* throwLabel, Label* exit, AsmJSImmKind func)
{
    MacroAssembler& masm = m.masm();
    masm.bind(exit);

    // sp can be anything at this point, so ensure it is aligned when calling
    // into C++.  We unconditionally jump to throw so don't worry about restoring sp.
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));

    // OnOutOfBounds always throws.
    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(AsmJSImmPtr(func));
    masm.jump(throwLabel);

    return !masm.oom() && m.finishGeneratingInlineStub(exit);
}

static const LiveRegisterSet AllRegsExceptSP(
    GeneralRegisterSet(Registers::AllMask&
                       ~(uint32_t(1) << Registers::StackPointer)),
    FloatRegisterSet(FloatRegisters::AllMask));

// The async interrupt-callback exit is called from arbitrarily-interrupted asm.js
// code. That means we must first save *all* registers and restore *all*
// registers (except the stack pointer) when we resume. The address to resume to
// (assuming that js::HandleExecutionInterrupt doesn't indicate that the
// execution should be aborted) is stored in AsmJSActivation::resumePC_.
// Unfortunately, loading this requires a scratch register which we don't have
// after restoring all registers. To hack around this, push the resumePC on the
// stack so that it can be popped directly into PC.
static bool
GenerateAsyncInterruptExit(ModuleValidator& m, Label* throwLabel)
{
    MacroAssembler& masm = m.masm();
    masm.haltingAlign(CodeAlignment);
    masm.bind(&m.asyncInterruptLabel());

#if defined(JS_CODEGEN_X86) || defined(JS_CODEGEN_X64)
    // Be very careful here not to perturb the machine state before saving it
    // to the stack. In particular, add/sub instructions may set conditions in
    // the flags register.
    masm.push(Imm32(0));            // space for resumePC
    masm.pushFlags();               // after this we are safe to use sub
    masm.setFramePushed(0);         // set to zero so we can use masm.framePushed() below
    masm.PushRegsInMask(AllRegsExceptSP); // save all GP/FP registers (except SP)

    Register scratch = ABIArgGenerator::NonArgReturnReg0;

    // Store resumePC into the reserved space.
    masm.loadAsmJSActivation(scratch);
    masm.loadPtr(Address(scratch, AsmJSActivation::offsetOfResumePC()), scratch);
    masm.storePtr(scratch, Address(masm.getStackPointer(), masm.framePushed() + sizeof(void*)));

    // We know that StackPointer is word-aligned, but not necessarily
    // stack-aligned, so we need to align it dynamically.
    masm.moveStackPtrTo(ABIArgGenerator::NonVolatileReg);
    masm.andToStackPtr(Imm32(~(ABIStackAlignment - 1)));
    if (ShadowStackSpace)
        masm.subFromStackPtr(Imm32(ShadowStackSpace));

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(AsmJSImmPtr(AsmJSImm_HandleExecutionInterrupt));

    masm.branchIfFalseBool(ReturnReg, throwLabel);

    // Restore the StackPointer to its position before the call.
    masm.moveToStackPtr(ABIArgGenerator::NonVolatileReg);

    // Restore the machine state to before the interrupt.
    masm.PopRegsInMask(AllRegsExceptSP); // restore all GP/FP registers (except SP)
    masm.popFlags();              // after this, nothing that sets conditions
    masm.ret();                   // pop resumePC into PC
#elif defined(JS_CODEGEN_MIPS32)
    // Reserve space to store resumePC.
    masm.subFromStackPtr(Imm32(sizeof(intptr_t)));
    // set to zero so we can use masm.framePushed() below.
    masm.setFramePushed(0);
    // When this platform supports SIMD extensions, we'll need to push high lanes
    // of SIMD registers as well.
    JS_STATIC_ASSERT(!SupportsSimd);
    // save all registers,except sp. After this stack is alligned.
    masm.PushRegsInMask(AllRegsExceptSP);

    // Save the stack pointer in a non-volatile register.
    masm.moveStackPtrTo(s0);
    // Align the stack.
    masm.ma_and(StackPointer, StackPointer, Imm32(~(ABIStackAlignment - 1)));

    // Store resumePC into the reserved space.
    masm.loadAsmJSActivation(IntArgReg0);
    masm.loadPtr(Address(IntArgReg0, AsmJSActivation::offsetOfResumePC()), IntArgReg1);
    masm.storePtr(IntArgReg1, Address(s0, masm.framePushed()));

    // MIPS ABI requires rewserving stack for registes $a0 to $a3.
    masm.subFromStackPtr(Imm32(4 * sizeof(intptr_t)));

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(AsmJSImm_HandleExecutionInterrupt);

    masm.addToStackPtr(Imm32(4 * sizeof(intptr_t)));

    masm.branchIfFalseBool(ReturnReg, throwLabel);

    // This will restore stack to the address before the call.
    masm.moveToStackPtr(s0);
    masm.PopRegsInMask(AllRegsExceptSP);

    // Pop resumePC into PC. Clobber HeapReg to make the jump and restore it
    // during jump delay slot.
    masm.pop(HeapReg);
    masm.as_jr(HeapReg);
    masm.loadAsmJSHeapRegisterFromGlobalData();
#elif defined(JS_CODEGEN_ARM)
    masm.setFramePushed(0);         // set to zero so we can use masm.framePushed() below

    // Save all GPR, except the stack pointer.
    masm.PushRegsInMask(LiveRegisterSet(
                            GeneralRegisterSet(Registers::AllMask & ~(1<<Registers::sp)),
                            FloatRegisterSet(uint32_t(0))));

    // Save both the APSR and FPSCR in non-volatile registers.
    masm.as_mrs(r4);
    masm.as_vmrs(r5);
    // Save the stack pointer in a non-volatile register.
    masm.mov(sp,r6);
    // Align the stack.
    masm.ma_and(Imm32(~7), sp, sp);

    // Store resumePC into the return PC stack slot.
    masm.loadAsmJSActivation(IntArgReg0);
    masm.loadPtr(Address(IntArgReg0, AsmJSActivation::offsetOfResumePC()), IntArgReg1);
    masm.storePtr(IntArgReg1, Address(r6, 14 * sizeof(uint32_t*)));

    // When this platform supports SIMD extensions, we'll need to push and pop
    // high lanes of SIMD registers as well.

    // Save all FP registers
    JS_STATIC_ASSERT(!SupportsSimd);
    masm.PushRegsInMask(LiveRegisterSet(GeneralRegisterSet(0),
                                        FloatRegisterSet(FloatRegisters::AllDoubleMask)));

    masm.assertStackAlignment(ABIStackAlignment);
    masm.call(AsmJSImm_HandleExecutionInterrupt);

    masm.branchIfFalseBool(ReturnReg, throwLabel);

    // Restore the machine state to before the interrupt. this will set the pc!

    // Restore all FP registers
    masm.PopRegsInMask(LiveRegisterSet(GeneralRegisterSet(0),
                                       FloatRegisterSet(FloatRegisters::AllDoubleMask)));
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
#elif defined(JS_CODEGEN_ARM64)
    MOZ_CRASH();
#elif defined (JS_CODEGEN_NONE)
    MOZ_CRASH();
#else
# error "Unknown architecture!"
#endif

    return !masm.oom() && m.finishGeneratingInlineStub(&m.asyncInterruptLabel());
}

static bool
GenerateSyncInterruptExit(ModuleValidator& m, Label* throwLabel)
{
    MacroAssembler& masm = m.masm();
    masm.setFramePushed(0);

    unsigned framePushed = StackDecrementForCall(masm, ABIStackAlignment, ShadowStackSpace);

    GenerateAsmJSExitPrologue(masm, framePushed, AsmJSExit::Interrupt, &m.syncInterruptLabel());

    AssertStackAlignment(masm, ABIStackAlignment);
    masm.call(AsmJSImmPtr(AsmJSImm_HandleExecutionInterrupt));
    masm.branchIfFalseBool(ReturnReg, throwLabel);

    Label profilingReturn;
    GenerateAsmJSExitEpilogue(masm, framePushed, AsmJSExit::Interrupt, &profilingReturn);
    return !masm.oom() && m.finishGeneratingInterrupt(&m.syncInterruptLabel(), &profilingReturn);
}

// If an exception is thrown, simply pop all frames (since asm.js does not
// contain try/catch). To do this:
//  1. Restore 'sp' to it's value right after the PushRegsInMask in GenerateEntry.
//  2. PopRegsInMask to restore the caller's non-volatile registers.
//  3. Return (to CallAsmJS).
static bool
GenerateThrowStub(ModuleValidator& m, Label* throwLabel)
{
    MacroAssembler& masm = m.masm();
    masm.haltingAlign(CodeAlignment);
    masm.bind(throwLabel);

    // We are about to pop all frames in this AsmJSActivation. Set fp to null to
    // maintain the invariant that fp is either null or pointing to a valid
    // frame.
    Register scratch = ABIArgGenerator::NonArgReturnReg0;
    masm.loadAsmJSActivation(scratch);
    masm.storePtr(ImmWord(0), Address(scratch, AsmJSActivation::offsetOfFP()));

    masm.setFramePushed(FramePushedForEntrySP);
    masm.loadStackPtr(Address(scratch, AsmJSActivation::offsetOfEntrySP()));
    masm.Pop(scratch);
    masm.PopRegsInMask(NonVolatileRegs);
    MOZ_ASSERT(masm.framePushed() == 0);

    masm.mov(ImmWord(0), ReturnReg);
    masm.ret();

    return !masm.oom() && m.finishGeneratingInlineStub(throwLabel);
}

static bool
GenerateStubs(ModuleValidator& m)
{
    for (unsigned i = 0; i < m.module().numExportedFunctions(); i++) {
        if (m.module().exportedFunction(i).isChangeHeap())
            continue;
        if (!GenerateEntry(m, i))
           return false;
    }

    Label throwLabel;

    for (ModuleValidator::ExitMap::Range r = m.allExits(); !r.empty(); r.popFront()) {
        if (!GenerateFFIExits(m, r.front().value(), r.front().key().sig(), &throwLabel))
            return false;
    }

    if (m.stackOverflowLabel().used() && !GenerateStackOverflowExit(m, &throwLabel))
        return false;

    if (m.onDetachedLabel().used() && !GenerateOnDetachedLabelExit(m, &throwLabel))
        return false;

    if (!GenerateExceptionLabelExit(m, &throwLabel, &m.onOutOfBoundsLabel(), AsmJSImm_OnOutOfBounds))
        return false;
    if (!GenerateExceptionLabelExit(m, &throwLabel, &m.onConversionErrorLabel(), AsmJSImm_OnImpreciseConversion))
        return false;

    if (!GenerateAsyncInterruptExit(m, &throwLabel))
        return false;
    if (m.syncInterruptLabel().used() && !GenerateSyncInterruptExit(m, &throwLabel))
        return false;

    if (!GenerateThrowStub(m, &throwLabel))
        return false;

    for (unsigned i = 0; i < AsmJSExit::Builtin_Limit; i++) {
        if (!GenerateBuiltinThunk(m, AsmJSExit::BuiltinKind(i)))
            return false;
    }

    return true;
}

static bool
FinishModule(ModuleValidator& m, ScopedJSDeletePtr<AsmJSModule>* module)
{
    LifoAlloc lifo(TempAllocator::PreferredLifoChunkSize);
    TempAllocator alloc(&lifo);
    JitContext jitContext(m.cx(), &alloc);

    m.masm().resetForNewCodeGenerator(alloc);

    if (!GenerateStubs(m))
        return false;

    return m.finish(module);
}

static bool
CheckModule(ExclusiveContext* cx, AsmJSParser& parser, ParseNode* stmtList,
            ScopedJSDeletePtr<AsmJSModule>* moduleOut,
            ScopedJSFreePtr<char>* compilationTimeReport)
{
    if (!LookupAsmJSModuleInCache(cx, parser, moduleOut, compilationTimeReport))
        return false;
    if (*moduleOut)
        return true;

    ModuleValidator m(cx, parser);
    if (!m.init())
        return false;

    if (PropertyName* moduleFunctionName = FunctionName(m.moduleFunctionNode())) {
        if (!CheckModuleLevelName(m, m.moduleFunctionNode(), moduleFunctionName))
            return false;
        m.initModuleFunctionName(moduleFunctionName);
    }

    if (!CheckFunctionHead(m, m.moduleFunctionNode()))
        return false;

    if (!CheckModuleArguments(m, m.moduleFunctionNode()))
        return false;

    if (!CheckPrecedingStatements(m, stmtList))
        return false;

    if (!CheckModuleProcessingDirectives(m))
        return false;

    if (!CheckModuleGlobals(m))
        return false;

    m.startFunctionBodies();

#if !defined(ENABLE_SHARED_ARRAY_BUFFER)
    MOZ_ASSERT(!m.module().hasArrayView() || !m.module().isSharedView());
#endif

    ScopedJSDeletePtr<ModuleCompileResults> mcd;
    if (!CheckFunctions(m, &mcd))
        return false;

    if (!m.finishFunctionBodies(&mcd))
        return false;

    if (!CheckFuncPtrTables(m))
        return false;

    if (!CheckModuleReturn(m))
        return false;

    TokenKind tk;
    if (!GetToken(m.parser(), &tk))
        return false;
    TokenStream& ts = m.parser().tokenStream;
    if (tk == TOK_EOF || tk == TOK_RC) {
        ts.ungetToken();
    } else {
        return m.failOffset(ts.currentToken().pos.begin,
                            "top-level export (return) must be the last statement");
    }

    // Delay flushing until dynamic linking. The inhibited range is set by the
    // masm.executableCopy() called transitively by FinishModule.
    AutoFlushICache afc("CheckModule", /* inhibit = */ true);

    ScopedJSDeletePtr<AsmJSModule> module;
    if (!FinishModule(m, &module))
        return false;

    JS::AsmJSCacheResult cacheResult = StoreAsmJSModuleInCache(parser, *module, cx);
    module->staticallyLink(cx);

    m.buildCompilationTimeReport(cacheResult, compilationTimeReport);
    *moduleOut = module.forget();
    return true;
}

static bool
Warn(AsmJSParser& parser, int errorNumber, const char* str)
{
    ParseReportKind reportKind = parser.options().throwOnAsmJSValidationFailureOption &&
                                 errorNumber == JSMSG_USE_ASM_TYPE_FAIL
                                 ? ParseError
                                 : ParseWarning;
    parser.reportNoOffset(reportKind, /* strict = */ false, errorNumber, str ? str : "");
    return false;
}

static bool
EstablishPreconditions(ExclusiveContext* cx, AsmJSParser& parser)
{
#ifdef JS_CODEGEN_NONE
    return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by lack of a JIT compiler");
#endif

    if (!cx->jitSupportsFloatingPoint())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by lack of floating point support");

    if (cx->gcSystemPageSize() != AsmJSPageSize)
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by non 4KiB system page size");

    if (!parser.options().asmJSOption)
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by javascript.options.asmjs in about:config");

    if (cx->compartment()->debuggerObservesAsmJS())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by debugger");

    if (parser.pc->isGenerator())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by generator context");

    if (parser.pc->isArrowFunction())
        return Warn(parser, JSMSG_USE_ASM_TYPE_FAIL, "Disabled by arrow function context");

    return true;
}

static bool
NoExceptionPending(ExclusiveContext* cx)
{
    return !cx->isJSContext() || !cx->asJSContext()->isExceptionPending();
}

bool
js::ValidateAsmJS(ExclusiveContext* cx, AsmJSParser& parser, ParseNode* stmtList, bool* validated)
{
    *validated = false;

    if (!EstablishPreconditions(cx, parser))
        return NoExceptionPending(cx);

    ScopedJSFreePtr<char> compilationTimeReport;
    ScopedJSDeletePtr<AsmJSModule> module;
    if (!CheckModule(cx, parser, stmtList, &module, &compilationTimeReport))
        return NoExceptionPending(cx);

    RootedObject moduleObj(cx, AsmJSModuleObject::create(cx, &module));
    if (!moduleObj)
        return false;

    FunctionBox* funbox = parser.pc->maybeFunction->pn_funbox;
    RootedFunction moduleFun(cx, NewAsmJSModuleFunction(cx, funbox->function(), moduleObj));
    if (!moduleFun)
        return false;

    MOZ_ASSERT(funbox->function()->isInterpreted());
    funbox->object = moduleFun;

    *validated = true;
    Warn(parser, JSMSG_USE_ASM_TYPE_OK, compilationTimeReport.get());
    return NoExceptionPending(cx);
}

bool
js::IsAsmJSCompilationAvailable(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // See EstablishPreconditions.
#ifdef JS_CODEGEN_NONE
    bool available = false;
#else
    bool available = cx->jitSupportsFloatingPoint() &&
                     cx->gcSystemPageSize() == AsmJSPageSize &&
                     cx->runtime()->options().asmJS();
#endif

    args.rval().set(BooleanValue(available));
    return true;
}
