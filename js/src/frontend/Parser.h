/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JS parser. */

#ifndef frontend_Parser_h
#define frontend_Parser_h

#include "mozilla/Array.h"
#include "mozilla/Maybe.h"
#include "mozilla/TypeTraits.h"

#include "jsiter.h"
#include "jspubtd.h"

#include "ds/Nestable.h"
#include "frontend/BytecodeCompiler.h"
#include "frontend/FullParseHandler.h"
#include "frontend/LanguageExtensions.h"
#include "frontend/NameAnalysisTypes.h"
#include "frontend/NameCollections.h"
#include "frontend/SharedContext.h"
#include "frontend/SyntaxParseHandler.h"

namespace js {

class ModuleObject;

namespace frontend {

class ParserBase;

template <template <typename CharT> class ParseHandler, typename CharT> class Parser;

/*
 * The struct ParseContext stores information about the current parsing context,
 * which is part of the parser state (see the field Parser::pc). The current
 * parsing context is either the global context, or the function currently being
 * parsed. When the parser encounters a function definition, it creates a new
 * ParseContext, makes it the new current context.
 */
class ParseContext : public Nestable<ParseContext>
{
  public:
    // The intra-function statement stack.
    //
    // Used for early error checking that depend on the nesting structure of
    // statements, such as continue/break targets, labels, and unbraced
    // lexical declarations.
    class Statement : public Nestable<Statement>
    {
        StatementKind kind_;

      public:
        using Nestable<Statement>::enclosing;
        using Nestable<Statement>::findNearest;

        Statement(ParseContext* pc, StatementKind kind)
          : Nestable<Statement>(&pc->innermostStatement_),
            kind_(kind)
        { }

        template <typename T> inline bool is() const;
        template <typename T> inline T& as();

        StatementKind kind() const {
            return kind_;
        }

        void refineForKind(StatementKind newForKind) {
            MOZ_ASSERT(kind_ == StatementKind::ForLoop);
            MOZ_ASSERT(newForKind == StatementKind::ForInLoop ||
                       newForKind == StatementKind::ForOfLoop);
            kind_ = newForKind;
        }
    };

    class LabelStatement : public Statement
    {
        RootedAtom label_;

      public:
        LabelStatement(ParseContext* pc, JSAtom* label)
          : Statement(pc, StatementKind::Label),
            label_(pc->sc_->context, label)
        { }

        HandleAtom label() const {
            return label_;
        }
    };

    struct ClassStatement : public Statement
    {
        FunctionBox* constructorBox;

        explicit ClassStatement(ParseContext* pc)
          : Statement(pc, StatementKind::Class),
            constructorBox(nullptr)
        { }
    };

    // The intra-function scope stack.
    //
    // Tracks declared and used names within a scope.
    class Scope : public Nestable<Scope>
    {
        // Names declared in this scope. Corresponds to the union of
        // VarDeclaredNames and LexicallyDeclaredNames in the ES spec.
        //
        // A 'var' declared name is a member of the declared name set of every
        // scope in its scope contour.
        //
        // A lexically declared name is a member only of the declared name set of
        // the scope in which it is declared.
        PooledMapPtr<DeclaredNameMap> declared_;

        // Monotonically increasing id.
        uint32_t id_;

        bool maybeReportOOM(ParseContext* pc, bool result) {
            if (!result)
                ReportOutOfMemory(pc->sc()->context);
            return result;
        }

      public:
        using DeclaredNamePtr = DeclaredNameMap::Ptr;
        using AddDeclaredNamePtr = DeclaredNameMap::AddPtr;

        using Nestable<Scope>::enclosing;

        explicit inline Scope(ParserBase* parser);

        void dump(ParseContext* pc);

        uint32_t id() const {
            return id_;
        }

        MOZ_MUST_USE bool init(ParseContext* pc) {
            if (id_ == UINT32_MAX) {
                pc->tokenStream_.reportErrorNoOffset(JSMSG_NEED_DIET, js_script_str);
                return false;
            }

            return declared_.acquire(pc->sc()->context);
        }

        DeclaredNamePtr lookupDeclaredName(JSAtom* name) {
            return declared_->lookup(name);
        }

        AddDeclaredNamePtr lookupDeclaredNameForAdd(JSAtom* name) {
            return declared_->lookupForAdd(name);
        }

        MOZ_MUST_USE bool addDeclaredName(ParseContext* pc, AddDeclaredNamePtr& p, JSAtom* name,
                                          DeclarationKind kind, uint32_t pos)
        {
            return maybeReportOOM(pc, declared_->add(p, name, DeclaredNameInfo(kind, pos)));
        }

        // Remove all VarForAnnexBLexicalFunction declarations of a certain
        // name from all scopes in pc's scope stack.
        static void removeVarForAnnexBLexicalFunction(ParseContext* pc, JSAtom* name);

        // Add and remove catch parameter names. Used to implement the odd
        // semantics of catch bodies.
        bool addCatchParameters(ParseContext* pc, Scope& catchParamScope);
        void removeCatchParameters(ParseContext* pc, Scope& catchParamScope);

        void useAsVarScope(ParseContext* pc) {
            MOZ_ASSERT(!pc->varScope_);
            pc->varScope_ = this;
        }

        // An iterator for the set of names a scope binds: the set of all
        // declared names for 'var' scopes, and the set of lexically declared
        // names for non-'var' scopes.
        class BindingIter
        {
            friend class Scope;

            DeclaredNameMap::Range declaredRange_;
            mozilla::DebugOnly<uint32_t> count_;
            bool isVarScope_;

            BindingIter(Scope& scope, bool isVarScope)
              : declaredRange_(scope.declared_->all()),
                count_(0),
                isVarScope_(isVarScope)
            {
                settle();
            }

            void settle() {
                // Both var and lexically declared names are binding in a var
                // scope.
                if (isVarScope_)
                    return;

                // Otherwise, pop only lexically declared names are
                // binding. Pop the range until we find such a name.
                while (!declaredRange_.empty()) {
                    if (BindingKindIsLexical(kind()))
                        break;
                    declaredRange_.popFront();
                }
            }

          public:
            bool done() const {
                return declaredRange_.empty();
            }

            explicit operator bool() const {
                return !done();
            }

            JSAtom* name() {
                MOZ_ASSERT(!done());
                return declaredRange_.front().key();
            }

            DeclarationKind declarationKind() {
                MOZ_ASSERT(!done());
                return declaredRange_.front().value()->kind();
            }

            BindingKind kind() {
                return DeclarationKindToBindingKind(declarationKind());
            }

            bool closedOver() {
                MOZ_ASSERT(!done());
                return declaredRange_.front().value()->closedOver();
            }

            void setClosedOver() {
                MOZ_ASSERT(!done());
                return declaredRange_.front().value()->setClosedOver();
            }

            void operator++(int) {
                MOZ_ASSERT(!done());
                MOZ_ASSERT(count_ != UINT32_MAX);
                declaredRange_.popFront();
                settle();
            }
        };

        inline BindingIter bindings(ParseContext* pc);
    };

    class VarScope : public Scope
    {
      public:
        explicit inline VarScope(ParserBase* parser);
    };

  private:
    // Trace logging of parsing time.
    AutoFrontendTraceLog traceLog_;

    // Context shared between parsing and bytecode generation.
    SharedContext* sc_;

    // TokenStream used for error reporting.
    TokenStreamAnyChars& tokenStream_;

    // The innermost statement, i.e., top of the statement stack.
    Statement* innermostStatement_;

    // The innermost scope, i.e., top of the scope stack.
    //
    // The outermost scope in the stack is usually varScope_. In the case of
    // functions, the outermost scope is functionScope_, which may be
    // varScope_. See comment above functionScope_.
    Scope* innermostScope_;

    // If isFunctionBox() and the function is a named lambda, the DeclEnv
    // scope for named lambdas.
    mozilla::Maybe<Scope> namedLambdaScope_;

    // If isFunctionBox(), the scope for the function. If there are no
    // parameter expressions, this is scope for the entire function. If there
    // are parameter expressions, this holds the special function names
    // ('.this', 'arguments') and the formal parameers.
    mozilla::Maybe<Scope> functionScope_;

    // The body-level scope. This always exists, but not necessarily at the
    // beginning of parsing the script in the case of functions with parameter
    // expressions.
    Scope* varScope_;

    // Inner function boxes in this context to try Annex B.3.3 semantics
    // on. Only used when full parsing.
    PooledVectorPtr<FunctionBoxVector> innerFunctionBoxesForAnnexB_;

    // Simple formal parameter names, in order of appearance. Only used when
    // isFunctionBox().
    PooledVectorPtr<AtomVector> positionalFormalParameterNames_;

    // Closed over binding names, in order of appearance. Null-delimited
    // between scopes. Only used when syntax parsing.
    PooledVectorPtr<AtomVector> closedOverBindingsForLazy_;

    // Monotonically increasing id.
    uint32_t scriptId_;

    // Set when compiling a function using Parser::standaloneFunctionBody via
    // the Function or Generator constructor.
    bool isStandaloneFunctionBody_;

    // Set when encountering a super.property inside a method. We need to mark
    // the nearest super scope as needing a home object.
    bool superScopeNeedsHomeObject_;

  public:
    // lastYieldOffset stores the offset of the last yield that was parsed.
    // NoYieldOffset is its initial value.
    static const uint32_t NoYieldOffset = UINT32_MAX;
    uint32_t lastYieldOffset;

    // lastAwaitOffset stores the offset of the last await that was parsed.
    // NoAwaitOffset is its initial value.
    static const uint32_t NoAwaitOffset = UINT32_MAX;
    uint32_t         lastAwaitOffset;

    // All inner functions in this context. Only used when syntax parsing.
    Rooted<GCVector<JSFunction*, 8>> innerFunctionsForLazy;

    // In a function context, points to a Directive struct that can be updated
    // to reflect new directives encountered in the Directive Prologue that
    // require reparsing the function. In global/module/generator-tail contexts,
    // we don't need to reparse when encountering a DirectivePrologue so this
    // pointer may be nullptr.
    Directives* newDirectives;

    // Set when parsing a function and it has 'return <expr>;'
    bool funHasReturnExpr;

    // Set when parsing a function and it has 'return;'
    bool funHasReturnVoid;

  public:
    template <template <typename CharT> class ParseHandler, typename CharT>
    ParseContext(Parser<ParseHandler, CharT>* prs, SharedContext* sc, Directives* newDirectives)
      : Nestable<ParseContext>(&prs->pc),
        traceLog_(sc->context,
                  mozilla::IsSame<ParseHandler<CharT>, FullParseHandler<CharT>>::value
                  ? TraceLogger_ParsingFull
                  : TraceLogger_ParsingSyntax,
                  prs->tokenStream),
        sc_(sc),
        tokenStream_(prs->tokenStream),
        innermostStatement_(nullptr),
        innermostScope_(nullptr),
        varScope_(nullptr),
        innerFunctionBoxesForAnnexB_(prs->context->frontendCollectionPool()),
        positionalFormalParameterNames_(prs->context->frontendCollectionPool()),
        closedOverBindingsForLazy_(prs->context->frontendCollectionPool()),
        scriptId_(prs->usedNames.nextScriptId()),
        isStandaloneFunctionBody_(false),
        superScopeNeedsHomeObject_(false),
        lastYieldOffset(NoYieldOffset),
        lastAwaitOffset(NoAwaitOffset),
        innerFunctionsForLazy(prs->context, GCVector<JSFunction*, 8>(prs->context)),
        newDirectives(newDirectives),
        funHasReturnExpr(false),
        funHasReturnVoid(false)
    {
        if (isFunctionBox()) {
            if (functionBox()->function()->isNamedLambda())
                namedLambdaScope_.emplace(prs);
            functionScope_.emplace(prs);
        }
    }

    ~ParseContext();

    MOZ_MUST_USE bool init();

    SharedContext* sc() {
        return sc_;
    }

    bool isFunctionBox() const {
        return sc_->isFunctionBox();
    }

    FunctionBox* functionBox() {
        return sc_->asFunctionBox();
    }

    Statement* innermostStatement() {
        return innermostStatement_;
    }

    Scope* innermostScope() {
        // There is always at least one scope: the 'var' scope.
        MOZ_ASSERT(innermostScope_);
        return innermostScope_;
    }

    Scope& namedLambdaScope() {
        MOZ_ASSERT(functionBox()->function()->isNamedLambda());
        return *namedLambdaScope_;
    }

    Scope& functionScope() {
        MOZ_ASSERT(isFunctionBox());
        return *functionScope_;
    }

    Scope& varScope() {
        MOZ_ASSERT(varScope_);
        return *varScope_;
    }

    bool isFunctionExtraBodyVarScopeInnermost() {
        return isFunctionBox() && functionBox()->hasParameterExprs &&
               innermostScope() == varScope_;
    }

    template <typename Predicate /* (Statement*) -> bool */>
    Statement* findInnermostStatement(Predicate predicate) {
        return Statement::findNearest(innermostStatement_, predicate);
    }

    template <typename T, typename Predicate /* (Statement*) -> bool */>
    T* findInnermostStatement(Predicate predicate) {
        return Statement::findNearest<T>(innermostStatement_, predicate);
    }

    template <typename T>
    T* findInnermostStatement() {
        return Statement::findNearest<T>(innermostStatement_);
    }

    AtomVector& positionalFormalParameterNames() {
        return *positionalFormalParameterNames_;
    }

    AtomVector& closedOverBindingsForLazy() {
        return *closedOverBindingsForLazy_;
    }

    MOZ_MUST_USE bool addInnerFunctionBoxForAnnexB(FunctionBox* funbox);
    void removeInnerFunctionBoxesForAnnexB(JSAtom* name);
    void finishInnerFunctionBoxesForAnnexB();

    // True if we are at the topmost level of a entire script or function body.
    // For example, while parsing this code we would encounter f1 and f2 at
    // body level, but we would not encounter f3 or f4 at body level:
    //
    //   function f1() { function f2() { } }
    //   if (cond) { function f3() { if (cond) { function f4() { } } } }
    //
    bool atBodyLevel() {
        return !innermostStatement_;
    }

    bool atGlobalLevel() {
        return atBodyLevel() && sc_->isGlobalContext();
    }

    // True if we are at the topmost level of a module only.
    bool atModuleLevel() {
        return atBodyLevel() && sc_->isModuleContext();
    }

    void setIsStandaloneFunctionBody() {
        isStandaloneFunctionBody_ = true;
    }

    bool isStandaloneFunctionBody() const {
        return isStandaloneFunctionBody_;
    }

    void setSuperScopeNeedsHomeObject() {
        MOZ_ASSERT(sc_->allowSuperProperty());
        superScopeNeedsHomeObject_ = true;
    }

    bool superScopeNeedsHomeObject() const {
        return superScopeNeedsHomeObject_;
    }

    bool useAsmOrInsideUseAsm() const {
        return sc_->isFunctionBox() && sc_->asFunctionBox()->useAsmOrInsideUseAsm();
    }

    // Most functions start off being parsed as non-generators.
    // Non-generators transition to LegacyGenerator on parsing "yield" in JS 1.7.
    // An ES6 generator is marked as a "star generator" before its body is parsed.
    GeneratorKind generatorKind() const {
        return sc_->isFunctionBox() ? sc_->asFunctionBox()->generatorKind() : NotGenerator;
    }

    bool isLegacyGenerator() const {
        return generatorKind() == LegacyGenerator;
    }

    bool isStarGenerator() const {
        return generatorKind() == StarGenerator;
    }

    bool isAsync() const {
        return sc_->isFunctionBox() && sc_->asFunctionBox()->isAsync();
    }

    bool needsDotGeneratorName() const {
        return isStarGenerator() || isLegacyGenerator() || isAsync();
    }

    FunctionAsyncKind asyncKind() const {
        return isAsync() ? AsyncFunction : SyncFunction;
    }

    bool isArrowFunction() const {
        return sc_->isFunctionBox() && sc_->asFunctionBox()->function()->isArrow();
    }

    bool isMethod() const {
        return sc_->isFunctionBox() && sc_->asFunctionBox()->function()->isMethod();
    }

    uint32_t scriptId() const {
        return scriptId_;
    }
};

template <>
inline bool
ParseContext::Statement::is<ParseContext::LabelStatement>() const
{
    return kind_ == StatementKind::Label;
}

template <>
inline bool
ParseContext::Statement::is<ParseContext::ClassStatement>() const
{
    return kind_ == StatementKind::Class;
}

template <typename T>
inline T&
ParseContext::Statement::as()
{
    MOZ_ASSERT(is<T>());
    return static_cast<T&>(*this);
}

inline ParseContext::Scope::BindingIter
ParseContext::Scope::bindings(ParseContext* pc)
{
    // In function scopes with parameter expressions, function special names
    // (like '.this') are declared as vars in the function scope, despite its
    // not being the var scope.
    return BindingIter(*this, pc->varScope_ == this || pc->functionScope_.ptrOr(nullptr) == this);
}

inline
Directives::Directives(ParseContext* parent)
  : strict_(parent->sc()->strict()),
    asmJS_(parent->useAsmOrInsideUseAsm())
{}

enum VarContext { HoistVars, DontHoistVars };
enum PropListType { ObjectLiteral, ClassBody, DerivedClassBody };
enum class PropertyType {
    Normal,
    Shorthand,
    CoverInitializedName,
    Getter,
    GetterNoExpressionClosure,
    Setter,
    SetterNoExpressionClosure,
    Method,
    GeneratorMethod,
    AsyncMethod,
    AsyncGeneratorMethod,
    Constructor,
    DerivedConstructor
};

// Specify a value for an ES6 grammar parametrization.  We have no enum for
// [Return] because its behavior is exactly equivalent to checking whether
// we're in a function box -- easier and simpler than passing an extra
// parameter everywhere.
enum YieldHandling { YieldIsName, YieldIsKeyword };
enum InHandling { InAllowed, InProhibited };
enum DefaultHandling { NameRequired, AllowDefaultName };
enum TripledotHandling { TripledotAllowed, TripledotProhibited };

// A data structure for tracking used names per parsing session in order to
// compute which bindings are closed over. Scripts and scopes are numbered
// monotonically in textual order and name uses are tracked by lists of
// (script id, scope id) pairs of their use sites.
//
// Intuitively, in a pair (P,S), P tracks the most nested function that has a
// use of u, and S tracks the most nested scope that is still being parsed.
//
// P is used to answer the question "is u used by a nested function?"
// S is used to answer the question "is u used in any scopes currently being
//                                   parsed?"
//
// The algorithm:
//
// Let Used by a map of names to lists.
//
// 1. Number all scopes in monotonic increasing order in textual order.
// 2. Number all scripts in monotonic increasing order in textual order.
// 3. When an identifier u is used in scope numbered S in script numbered P,
//    and u is found in Used,
//   a. Append (P,S) to Used[u].
//   b. Otherwise, assign the the list [(P,S)] to Used[u].
// 4. When we finish parsing a scope S in script P, for each declared name d in
//    Declared(S):
//   a. If d is found in Used, mark d as closed over if there is a value
//     (P_d, S_d) in Used[d] such that P_d > P and S_d > S.
//   b. Remove all values (P_d, S_d) in Used[d] such that S_d are >= S.
//
// Steps 1 and 2 are implemented by UsedNameTracker::next{Script,Scope}Id.
// Step 3 is implemented by UsedNameTracker::noteUsedInScope.
// Step 4 is implemented by UsedNameTracker::noteBoundInScope and
// Parser::propagateFreeNamesAndMarkClosedOverBindings.
class UsedNameTracker
{
  public:
    struct Use
    {
        uint32_t scriptId;
        uint32_t scopeId;
    };

    class UsedNameInfo
    {
        friend class UsedNameTracker;

        Vector<Use, 6> uses_;

        void resetToScope(uint32_t scriptId, uint32_t scopeId);

      public:
        explicit UsedNameInfo(JSContext* cx)
          : uses_(cx)
        { }

        UsedNameInfo(UsedNameInfo&& other)
          : uses_(mozilla::Move(other.uses_))
        { }

        bool noteUsedInScope(uint32_t scriptId, uint32_t scopeId) {
            if (uses_.empty() || uses_.back().scopeId < scopeId)
                return uses_.append(Use { scriptId, scopeId });
            return true;
        }

        void noteBoundInScope(uint32_t scriptId, uint32_t scopeId, bool* closedOver) {
            *closedOver = false;
            while (!uses_.empty()) {
                Use& innermost = uses_.back();
                if (innermost.scopeId < scopeId)
                    break;
                if (innermost.scriptId > scriptId)
                    *closedOver = true;
                uses_.popBack();
            }
        }

        bool isUsedInScript(uint32_t scriptId) const {
            return !uses_.empty() && uses_.back().scriptId >= scriptId;
        }
    };

    using UsedNameMap = HashMap<JSAtom*,
                                UsedNameInfo,
                                DefaultHasher<JSAtom*>>;

  private:
    // The map of names to chains of uses.
    UsedNameMap map_;

    // Monotonically increasing id for all nested scripts.
    uint32_t scriptCounter_;

    // Monotonically increasing id for all nested scopes.
    uint32_t scopeCounter_;

  public:
    explicit UsedNameTracker(JSContext* cx)
      : map_(cx),
        scriptCounter_(0),
        scopeCounter_(0)
    { }

    MOZ_MUST_USE bool init() {
        return map_.init();
    }

    uint32_t nextScriptId() {
        MOZ_ASSERT(scriptCounter_ != UINT32_MAX,
                   "ParseContext::Scope::init should have prevented wraparound");
        return scriptCounter_++;
    }

    uint32_t nextScopeId() {
        MOZ_ASSERT(scopeCounter_ != UINT32_MAX);
        return scopeCounter_++;
    }

    UsedNameMap::Ptr lookup(JSAtom* name) const {
        return map_.lookup(name);
    }

    MOZ_MUST_USE bool noteUse(JSContext* cx, JSAtom* name,
                              uint32_t scriptId, uint32_t scopeId);

    struct RewindToken
    {
      private:
        friend class UsedNameTracker;
        uint32_t scriptId;
        uint32_t scopeId;
    };

    RewindToken getRewindToken() const {
        RewindToken token;
        token.scriptId = scriptCounter_;
        token.scopeId = scopeCounter_;
        return token;
    }

    // Resets state so that scriptId and scopeId are the innermost script and
    // scope, respectively. Used for rewinding state on syntax parse failure.
    void rewind(RewindToken token);

    // Resets state to beginning of compilation.
    void reset() {
        map_.clear();
        RewindToken token;
        token.scriptId = 0;
        token.scopeId = 0;
        rewind(token);
    }
};

template <class Parser>
class AutoAwaitIsKeyword;

class ParserBase : public StrictModeGetter
{
  private:
    ParserBase* thisForCtor() { return this; }

  public:
    JSContext* const context;

    LifoAlloc& alloc;

    TokenStream tokenStream;
    LifoAlloc::Mark tempPoolMark;

    /* list of parsed objects for GC tracing */
    ObjectBox* traceListHead;

    /* innermost parse context (stack-allocated) */
    ParseContext* pc;

    // For tracking used names in this parsing session.
    UsedNameTracker& usedNames;

    ScriptSource*       ss;

    /* Root atoms and objects allocated for the parsed tree. */
    AutoKeepAtoms       keepAtoms;

    /* Perform constant-folding; must be true when interfacing with the emitter. */
    const bool          foldConstants:1;

  protected:
#if DEBUG
    /* Our fallible 'checkOptions' member function has been called. */
    bool checkOptionsCalled:1;
#endif

    /*
     * Not all language constructs can be handled during syntax parsing. If it
     * is not known whether the parse succeeds or fails, this bit is set and
     * the parse will return false.
     */
    bool abortedSyntaxParse:1;

    /* Unexpected end of input, i.e. TOK_EOF not at top-level. */
    bool isUnexpectedEOF_:1;

    bool awaitIsKeyword_:1;

  public:
    bool awaitIsKeyword() const {
      return awaitIsKeyword_;
    }

    ParserBase(JSContext* cx, LifoAlloc& alloc, const ReadOnlyCompileOptions& options,
               const char16_t* chars, size_t length, bool foldConstants,
               UsedNameTracker& usedNames, LazyScript* lazyOuterFunction);
    ~ParserBase();

    const char* getFilename() const { return tokenStream.getFilename(); }
    JSVersion versionNumber() const { return tokenStream.versionNumber(); }
    TokenPos pos() const { return tokenStream.currentToken().pos; }

    // Determine whether |yield| is a valid name in the current context, or
    // whether it's prohibited due to strictness, JS version, or occurrence
    // inside a star generator.
    bool yieldExpressionsSupported() {
        return (versionNumber() >= JSVERSION_1_7 && !pc->isAsync()) ||
               pc->isStarGenerator() ||
               pc->isLegacyGenerator();
    }

    bool asyncIterationSupported() {
#ifdef RELEASE_OR_BETA
        return false;
#else
        // Expose Async Iteration only to web content until the spec proposal
        // gets stable.
        return !options().isProbablySystemOrAddonCode;
#endif
    }

    virtual bool strictMode() { return pc->sc()->strict(); }
    bool setLocalStrictMode(bool strict) {
        MOZ_ASSERT(tokenStream.debugHasNoLookahead());
        return pc->sc()->setLocalStrictMode(strict);
    }

    const ReadOnlyCompileOptions& options() const {
        return tokenStream.options();
    }

    bool hadAbortedSyntaxParse() {
        return abortedSyntaxParse;
    }
    void clearAbortedSyntaxParse() {
        abortedSyntaxParse = false;
    }

    bool isUnexpectedEOF() const { return isUnexpectedEOF_; }

    bool reportNoOffset(ParseReportKind kind, bool strict, unsigned errorNumber, ...);

    /* Report the given error at the current offset. */
    void error(unsigned errorNumber, ...);
    void errorWithNotes(UniquePtr<JSErrorNotes> notes, unsigned errorNumber, ...);

    /* Report the given error at the given offset. */
    void errorAt(uint32_t offset, unsigned errorNumber, ...);
    void errorWithNotesAt(UniquePtr<JSErrorNotes> notes, uint32_t offset,
                          unsigned errorNumber, ...);

    /*
     * Handle a strict mode error at the current offset.  Report an error if in
     * strict mode code, or warn if not, using the given error number and
     * arguments.
     */
    MOZ_MUST_USE bool strictModeError(unsigned errorNumber, ...);

    /*
     * Handle a strict mode error at the given offset.  Report an error if in
     * strict mode code, or warn if not, using the given error number and
     * arguments.
     */
    MOZ_MUST_USE bool strictModeErrorAt(uint32_t offset, unsigned errorNumber, ...);

    /* Report the given warning at the current offset. */
    MOZ_MUST_USE bool warning(unsigned errorNumber, ...);

    /* Report the given warning at the given offset. */
    MOZ_MUST_USE bool warningAt(uint32_t offset, unsigned errorNumber, ...);

    /*
     * If extra warnings are enabled, report the given warning at the current
     * offset.
     */
    MOZ_MUST_USE bool extraWarning(unsigned errorNumber, ...);

    /*
     * If extra warnings are enabled, report the given warning at the given
     * offset.
     */
    MOZ_MUST_USE bool extraWarningAt(uint32_t offset, unsigned errorNumber, ...);

    bool isValidStrictBinding(PropertyName* name);

    void addTelemetry(DeprecatedLanguageExtension e);

    bool warnOnceAboutExprClosure();
    bool warnOnceAboutForEach();

    bool allowsForEachIn() {
#if !JS_HAS_FOR_EACH_IN
        return false;
#else
        return options().forEachStatementOption && versionNumber() >= JSVERSION_1_6;
#endif
    }

    bool hasValidSimpleStrictParameterNames();


    /*
     * Create a new function object given a name (which is optional if this is
     * a function expression).
     */
    JSFunction* newFunction(HandleAtom atom, FunctionSyntaxKind kind,
                            GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                            HandleObject proto);

    // A Parser::Mark is the extension of the LifoAlloc::Mark to the entire
    // Parser's state. Note: clients must still take care that any ParseContext
    // that points into released ParseNodes is destroyed.
    class Mark
    {
        friend class ParserBase;
        LifoAlloc::Mark mark;
        ObjectBox* traceListHead;
    };
    Mark mark() const {
        Mark m;
        m.mark = alloc.mark();
        m.traceListHead = traceListHead;
        return m;
    }
    void release(Mark m) {
        alloc.release(m.mark);
        traceListHead = m.traceListHead;
    }

    ObjectBox* newObjectBox(JSObject* obj);

    mozilla::Maybe<GlobalScope::Data*> newGlobalScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<ModuleScope::Data*> newModuleScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<EvalScope::Data*> newEvalScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<FunctionScope::Data*> newFunctionScopeData(ParseContext::Scope& scope,
                                                              bool hasParameterExprs);
    mozilla::Maybe<VarScope::Data*> newVarScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<LexicalScope::Data*> newLexicalScopeData(ParseContext::Scope& scope);

  protected:
    enum InvokedPrediction { PredictUninvoked = false, PredictInvoked = true };
    enum ForInitLocation { InForInit, NotInForInit };
};

inline
ParseContext::Scope::Scope(ParserBase* parser)
  : Nestable<Scope>(&parser->pc->innermostScope_),
    declared_(parser->context->frontendCollectionPool()),
    id_(parser->usedNames.nextScopeId())
{ }

inline
ParseContext::VarScope::VarScope(ParserBase* parser)
  : Scope(parser)
{
    useAsVarScope(parser->pc);
}

template <template<typename CharT> class ParseHandler, typename CharT>
class Parser final : public ParserBase, private JS::AutoGCRooter
{
  private:
    using Node = typename ParseHandler<CharT>::Node;

    /*
     * A class for temporarily stashing errors while parsing continues.
     *
     * The ability to stash an error is useful for handling situations where we
     * aren't able to verify that an error has occurred until later in the parse.
     * For instance | ({x=1}) | is always parsed as an object literal with
     * a SyntaxError, however, in the case where it is followed by '=>' we rewind
     * and reparse it as a valid arrow function. Here a PossibleError would be
     * set to 'pending' when the initial SyntaxError was encountered then 'resolved'
     * just before rewinding the parser.
     *
     * There are currently two kinds of PossibleErrors: Expression and
     * Destructuring errors. Expression errors are used to mark a possible
     * syntax error when a grammar production is used in an expression context.
     * For example in |{x = 1}|, we mark the CoverInitializedName |x = 1| as a
     * possible expression error, because CoverInitializedName productions
     * are disallowed when an actual ObjectLiteral is expected.
     * Destructuring errors are used to record possible syntax errors in
     * destructuring contexts. For example in |[...rest, ] = []|, we initially
     * mark the trailing comma after the spread expression as a possible
     * destructuring error, because the ArrayAssignmentPattern grammar
     * production doesn't allow a trailing comma after the rest element.
     *
     * When using PossibleError one should set a pending error at the location
     * where an error occurs. From that point, the error may be resolved
     * (invalidated) or left until the PossibleError is checked.
     *
     * Ex:
     *   PossibleError possibleError(*this);
     *   possibleError.setPendingExpressionErrorAt(pos, JSMSG_BAD_PROP_ID);
     *   // A JSMSG_BAD_PROP_ID ParseError is reported, returns false.
     *   if (!possibleError.checkForExpressionError())
     *       return false; // we reach this point with a pending exception
     *
     *   PossibleError possibleError(*this);
     *   possibleError.setPendingExpressionErrorAt(pos, JSMSG_BAD_PROP_ID);
     *   // Returns true, no error is reported.
     *   if (!possibleError.checkForDestructuringError())
     *       return false; // not reached, no pending exception
     *
     *   PossibleError possibleError(*this);
     *   // Returns true, no error is reported.
     *   if (!possibleError.checkForExpressionError())
     *       return false; // not reached, no pending exception
     */
    class MOZ_STACK_CLASS PossibleError
    {
      private:
        enum class ErrorKind { Expression, Destructuring, DestructuringWarning };

        enum class ErrorState { None, Pending };

        struct Error {
            ErrorState state_ = ErrorState::None;

            // Error reporting fields.
            uint32_t offset_;
            unsigned errorNumber_;
        };

        Parser<ParseHandler, CharT>& parser_;
        Error exprError_;
        Error destructuringError_;
        Error destructuringWarning_;

        // Returns the error report.
        Error& error(ErrorKind kind);

        // Return true if an error is pending without reporting.
        bool hasError(ErrorKind kind);

        // Resolve any pending error.
        void setResolved(ErrorKind kind);

        // Set a pending error. Only a single error may be set per instance and
        // error kind.
        void setPending(ErrorKind kind, const TokenPos& pos, unsigned errorNumber);

        // If there is a pending error, report it and return false, otherwise
        // return true.
        MOZ_MUST_USE bool checkForError(ErrorKind kind);

        // If there is a pending warning, report it and return either false or
        // true depending on the werror option, otherwise return true.
        MOZ_MUST_USE bool checkForWarning(ErrorKind kind);

        // Transfer an existing error to another instance.
        void transferErrorTo(ErrorKind kind, PossibleError* other);

      public:
        explicit PossibleError(Parser<ParseHandler, CharT>& parser);

        // Return true if a pending destructuring error is present.
        bool hasPendingDestructuringError();

        // Set a pending destructuring error. Only a single error may be set
        // per instance, i.e. subsequent calls to this method are ignored and
        // won't overwrite the existing pending error.
        void setPendingDestructuringErrorAt(const TokenPos& pos, unsigned errorNumber);

        // Set a pending destructuring warning. Only a single warning may be
        // set per instance, i.e. subsequent calls to this method are ignored
        // and won't overwrite the existing pending warning.
        void setPendingDestructuringWarningAt(const TokenPos& pos, unsigned errorNumber);

        // Set a pending expression error. Only a single error may be set per
        // instance, i.e. subsequent calls to this method are ignored and won't
        // overwrite the existing pending error.
        void setPendingExpressionErrorAt(const TokenPos& pos, unsigned errorNumber);

        // If there is a pending destructuring error or warning, report it and
        // return false, otherwise return true. Clears any pending expression
        // error.
        MOZ_MUST_USE bool checkForDestructuringErrorOrWarning();

        // If there is a pending expression error, report it and return false,
        // otherwise return true. Clears any pending destructuring error or
        // warning.
        MOZ_MUST_USE bool checkForExpressionError();

        // Pass pending errors between possible error instances. This is useful
        // for extending the lifetime of a pending error beyond the scope of
        // the PossibleError where it was initially set (keeping in mind that
        // PossibleError is a MOZ_STACK_CLASS).
        void transferErrorsTo(PossibleError* other);
    };

  public:
    /* State specific to the kind of parse being performed. */
    ParseHandler<CharT> handler;

    void prepareNodeForMutation(Node node) { handler.prepareNodeForMutation(node); }
    void freeTree(Node node) { handler.freeTree(node); }

  public:
    Parser(JSContext* cx, LifoAlloc& alloc, const ReadOnlyCompileOptions& options,
           const CharT* chars, size_t length, bool foldConstants, UsedNameTracker& usedNames,
           Parser<SyntaxParseHandler, CharT>* syntaxParser, LazyScript* lazyOuterFunction);
    ~Parser();

    friend class AutoAwaitIsKeyword<Parser>;
    void setAwaitIsKeyword(bool isKeyword);

    bool checkOptions();

    friend void js::frontend::TraceParser(JSTracer* trc, JS::AutoGCRooter* parser);

    /*
     * Parse a top-level JS script.
     */
    Node parse();

    FunctionBox* newFunctionBox(Node fn, JSFunction* fun, uint32_t toStringStart,
                                Directives directives,
                                GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                                bool tryAnnexB);

    void trace(JSTracer* trc);

  private:
    Parser* thisForCtor() { return this; }

    Node stringLiteral();
    Node noSubstitutionTaggedTemplate();
    Node noSubstitutionUntaggedTemplate();
    Node templateLiteral(YieldHandling yieldHandling);
    bool taggedTemplate(YieldHandling yieldHandling, Node nodeList, TokenKind tt);
    bool appendToCallSiteObj(Node callSiteObj);
    bool addExprAndGetNextTemplStrToken(YieldHandling yieldHandling, Node nodeList,
                                        TokenKind* ttp);
    bool checkStatementsEOF();

    inline Node newName(PropertyName* name);
    inline Node newName(PropertyName* name, TokenPos pos);

    inline bool abortIfSyntaxParser();

  public:
    /* Public entry points for parsing. */
    Node statement(YieldHandling yieldHandling);
    Node statementListItem(YieldHandling yieldHandling, bool canHaveDirectives = false);

    bool maybeParseDirective(Node list, Node pn, bool* cont);

    // Parse the body of an eval.
    //
    // Eval scripts are distinguished from global scripts in that in ES6, per
    // 18.2.1.1 steps 9 and 10, all eval scripts are executed under a fresh
    // lexical scope.
    Node evalBody(EvalSharedContext* evalsc);

    // Parse the body of a global script.
    Node globalBody(GlobalSharedContext* globalsc);

    // Parse a module.
    Node moduleBody(ModuleSharedContext* modulesc);

    // Parse a function, used for the Function, GeneratorFunction, and
    // AsyncFunction constructors.
    Node standaloneFunction(HandleFunction fun, HandleScope enclosingScope,
                            const mozilla::Maybe<uint32_t>& parameterListEnd,
                            GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                            Directives inheritedDirectives, Directives* newDirectives);

    // Parse a function, given only its arguments and body. Used for lazily
    // parsed functions.
    Node standaloneLazyFunction(HandleFunction fun, bool strict,
                                GeneratorKind generatorKind, FunctionAsyncKind asyncKind);

    // Parse an inner function given an enclosing ParseContext and a
    // FunctionBox for the inner function.
    bool innerFunction(Node pn, ParseContext* outerpc, FunctionBox* funbox, uint32_t toStringStart,
                       InHandling inHandling, YieldHandling yieldHandling,
                       FunctionSyntaxKind kind,
                       Directives inheritedDirectives, Directives* newDirectives);

    // Parse a function's formal parameters and its body assuming its function
    // ParseContext is already on the stack.
    bool functionFormalParametersAndBody(InHandling inHandling, YieldHandling yieldHandling,
                                         Node pn, FunctionSyntaxKind kind,
                                         const mozilla::Maybe<uint32_t>& parameterListEnd = mozilla::Nothing(),
                                         bool isStandaloneFunction = false);

    // Match the current token against the BindingIdentifier production with
    // the given Yield parameter.  If there is no match, report a syntax
    // error.
    PropertyName* bindingIdentifier(YieldHandling yieldHandling);

  private:
    /*
     * JS parsers, from lowest to highest precedence.
     *
     * Each parser must be called during the dynamic scope of a ParseContext
     * object, pointed to by this->pc.
     *
     * Each returns a parse node tree or null on error.
     *
     * Parsers whose name has a '1' suffix leave the TokenStream state
     * pointing to the token one past the end of the parsed fragment.  For a
     * number of the parsers this is convenient and avoids a lot of
     * unnecessary ungetting and regetting of tokens.
     *
     * Some parsers have two versions:  an always-inlined version (with an 'i'
     * suffix) and a never-inlined version (with an 'n' suffix).
     */
    Node functionStmt(uint32_t toStringStart,
                      YieldHandling yieldHandling, DefaultHandling defaultHandling,
                      FunctionAsyncKind asyncKind = SyncFunction);
    Node functionExpr(uint32_t toStringStart, InvokedPrediction invoked = PredictUninvoked,
                      FunctionAsyncKind asyncKind = SyncFunction);

    Node statementList(YieldHandling yieldHandling);

    Node blockStatement(YieldHandling yieldHandling,
                        unsigned errorNumber = JSMSG_CURLY_IN_COMPOUND);
    Node doWhileStatement(YieldHandling yieldHandling);
    Node whileStatement(YieldHandling yieldHandling);

    Node forStatement(YieldHandling yieldHandling);
    bool forHeadStart(YieldHandling yieldHandling,
                      IteratorKind iterKind,
                      ParseNodeKind* forHeadKind,
                      Node* forInitialPart,
                      mozilla::Maybe<ParseContext::Scope>& forLetImpliedScope,
                      Node* forInOrOfExpression);
    Node expressionAfterForInOrOf(ParseNodeKind forHeadKind, YieldHandling yieldHandling);

    Node switchStatement(YieldHandling yieldHandling);
    Node continueStatement(YieldHandling yieldHandling);
    Node breakStatement(YieldHandling yieldHandling);
    Node returnStatement(YieldHandling yieldHandling);
    Node withStatement(YieldHandling yieldHandling);
    Node throwStatement(YieldHandling yieldHandling);
    Node tryStatement(YieldHandling yieldHandling);
    Node catchBlockStatement(YieldHandling yieldHandling, ParseContext::Scope& catchParamScope);
    Node debuggerStatement();

    Node variableStatement(YieldHandling yieldHandling);

    Node labeledStatement(YieldHandling yieldHandling);
    Node labeledItem(YieldHandling yieldHandling);

    Node ifStatement(YieldHandling yieldHandling);
    Node consequentOrAlternative(YieldHandling yieldHandling);

    // While on a |let| TOK_NAME token, examine |next|.  Indicate whether
    // |next|, the next token already gotten with modifier TokenStream::None,
    // continues a LexicalDeclaration.
    bool nextTokenContinuesLetDeclaration(TokenKind next, YieldHandling yieldHandling);

    Node lexicalDeclaration(YieldHandling yieldHandling, DeclarationKind kind);

    Node importDeclaration();

    bool processExport(Node node);
    bool processExportFrom(Node node);

    Node exportFrom(uint32_t begin, Node specList);
    Node exportBatch(uint32_t begin);
    bool checkLocalExportNames(Node node);
    Node exportClause(uint32_t begin);
    Node exportFunctionDeclaration(uint32_t begin, uint32_t toStringStart,
                                   FunctionAsyncKind asyncKind = SyncFunction);
    Node exportVariableStatement(uint32_t begin);
    Node exportClassDeclaration(uint32_t begin);
    Node exportLexicalDeclaration(uint32_t begin, DeclarationKind kind);
    Node exportDefaultFunctionDeclaration(uint32_t begin, uint32_t toStringStart,
                                          FunctionAsyncKind asyncKind = SyncFunction);
    Node exportDefaultClassDeclaration(uint32_t begin);
    Node exportDefaultAssignExpr(uint32_t begin);
    Node exportDefault(uint32_t begin);
    Node exportDeclaration();

    Node expressionStatement(YieldHandling yieldHandling,
                             InvokedPrediction invoked = PredictUninvoked);

    // Declaration parsing.  The main entrypoint is Parser::declarationList,
    // with sub-functionality split out into the remaining methods.

    // |blockScope| may be non-null only when |kind| corresponds to a lexical
    // declaration (that is, PNK_LET or PNK_CONST).
    //
    // The for* parameters, for normal declarations, should be null/ignored.
    // They should be non-null only when Parser::forHeadStart parses a
    // declaration at the start of a for-loop head.
    //
    // In this case, on success |*forHeadKind| is PNK_FORHEAD, PNK_FORIN, or
    // PNK_FOROF, corresponding to the three for-loop kinds.  The precise value
    // indicates what was parsed.
    //
    // If parsing recognized a for(;;) loop, the next token is the ';' within
    // the loop-head that separates the init/test parts.
    //
    // Otherwise, for for-in/of loops, the next token is the ')' ending the
    // loop-head.  Additionally, the expression that the loop iterates over was
    // parsed into |*forInOrOfExpression|.
    Node declarationList(YieldHandling yieldHandling,
                         ParseNodeKind kind,
                         ParseNodeKind* forHeadKind = nullptr,
                         Node* forInOrOfExpression = nullptr);

    // The items in a declaration list are either patterns or names, with or
    // without initializers.  These two methods parse a single pattern/name and
    // any associated initializer -- and if parsing an |initialDeclaration|
    // will, if parsing in a for-loop head (as specified by |forHeadKind| being
    // non-null), consume additional tokens up to the closing ')' in a
    // for-in/of loop head, returning the iterated expression in
    // |*forInOrOfExpression|.  (An "initial declaration" is the first
    // declaration in a declaration list: |a| but not |b| in |var a, b|, |{c}|
    // but not |d| in |let {c} = 3, d|.)
    Node declarationPattern(Node decl, DeclarationKind declKind, TokenKind tt,
                            bool initialDeclaration, YieldHandling yieldHandling,
                            ParseNodeKind* forHeadKind, Node* forInOrOfExpression);
    Node declarationName(Node decl, DeclarationKind declKind, TokenKind tt,
                         bool initialDeclaration, YieldHandling yieldHandling,
                         ParseNodeKind* forHeadKind, Node* forInOrOfExpression);

    // Having parsed a name (not found in a destructuring pattern) declared by
    // a declaration, with the current token being the '=' separating the name
    // from its initializer, parse and bind that initializer -- and possibly
    // consume trailing in/of and subsequent expression, if so directed by
    // |forHeadKind|.
    bool initializerInNameDeclaration(Node decl, Node binding, Handle<PropertyName*> name,
                                      DeclarationKind declKind, bool initialDeclaration,
                                      YieldHandling yieldHandling, ParseNodeKind* forHeadKind,
                                      Node* forInOrOfExpression);

    Node expr(InHandling inHandling, YieldHandling yieldHandling,
              TripledotHandling tripledotHandling, PossibleError* possibleError = nullptr,
              InvokedPrediction invoked = PredictUninvoked);
    Node assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                    TripledotHandling tripledotHandling, PossibleError* possibleError = nullptr,
                    InvokedPrediction invoked = PredictUninvoked);
    Node assignExprWithoutYieldOrAwait(YieldHandling yieldHandling);
    Node yieldExpression(InHandling inHandling);
    Node condExpr1(InHandling inHandling, YieldHandling yieldHandling,
                   TripledotHandling tripledotHandling,
                   PossibleError* possibleError,
                   InvokedPrediction invoked = PredictUninvoked);
    Node orExpr1(InHandling inHandling, YieldHandling yieldHandling,
                 TripledotHandling tripledotHandling,
                 PossibleError* possibleError,
                 InvokedPrediction invoked = PredictUninvoked);
    Node unaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                   PossibleError* possibleError = nullptr,
                   InvokedPrediction invoked = PredictUninvoked);
    Node memberExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                    TokenKind tt, bool allowCallSyntax = true,
                    PossibleError* possibleError = nullptr,
                    InvokedPrediction invoked = PredictUninvoked);
    Node primaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                     TokenKind tt, PossibleError* possibleError,
                     InvokedPrediction invoked = PredictUninvoked);
    Node exprInParens(InHandling inHandling, YieldHandling yieldHandling,
                      TripledotHandling tripledotHandling, PossibleError* possibleError = nullptr);

    bool tryNewTarget(Node& newTarget);
    bool checkAndMarkSuperScope();

    Node methodDefinition(uint32_t toStringStart, PropertyType propType, HandleAtom funName);

    /*
     * Additional JS parsers.
     */
    bool functionArguments(YieldHandling yieldHandling, FunctionSyntaxKind kind,
                           Node funcpn);

    Node functionDefinition(Node func, uint32_t toStringStart,
                            InHandling inHandling, YieldHandling yieldHandling,
                            HandleAtom name, FunctionSyntaxKind kind,
                            GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                            bool tryAnnexB = false);

    // Parse a function body.  Pass StatementListBody if the body is a list of
    // statements; pass ExpressionBody if the body is a single expression.
    enum FunctionBodyType { StatementListBody, ExpressionBody };
    Node functionBody(InHandling inHandling, YieldHandling yieldHandling, FunctionSyntaxKind kind,
                      FunctionBodyType type);

    Node unaryOpExpr(YieldHandling yieldHandling, ParseNodeKind kind, JSOp op, uint32_t begin);

    Node condition(InHandling inHandling, YieldHandling yieldHandling);

    /* comprehensions */
    Node generatorComprehensionLambda(unsigned begin);
    Node comprehensionFor(GeneratorKind comprehensionKind);
    Node comprehensionIf(GeneratorKind comprehensionKind);
    Node comprehensionTail(GeneratorKind comprehensionKind);
    Node comprehension(GeneratorKind comprehensionKind);
    Node arrayComprehension(uint32_t begin);
    Node generatorComprehension(uint32_t begin);

    bool argumentList(YieldHandling yieldHandling, Node listNode, bool* isSpread,
                      PossibleError* possibleError = nullptr);
    Node destructuringDeclaration(DeclarationKind kind, YieldHandling yieldHandling,
                                  TokenKind tt);
    Node destructuringDeclarationWithoutYieldOrAwait(DeclarationKind kind, YieldHandling yieldHandling,
                                                     TokenKind tt);

    bool namedImportsOrNamespaceImport(TokenKind tt, Node importSpecSet);
    bool checkExportedName(JSAtom* exportName);
    bool checkExportedNamesForDeclaration(Node node);
    bool checkExportedNameForClause(Node node);
    bool checkExportedNameForFunction(Node node);
    bool checkExportedNameForClass(Node node);

    enum ClassContext { ClassStatement, ClassExpression };
    Node classDefinition(YieldHandling yieldHandling, ClassContext classContext,
                         DefaultHandling defaultHandling);

    bool checkLabelOrIdentifierReference(PropertyName* ident,
                                         uint32_t offset,
                                         YieldHandling yieldHandling,
                                         TokenKind hint = TOK_LIMIT);

    bool checkLocalExportName(PropertyName* ident, uint32_t offset) {
        return checkLabelOrIdentifierReference(ident, offset, YieldIsName);
    }

    bool checkBindingIdentifier(PropertyName* ident,
                                uint32_t offset,
                                YieldHandling yieldHandling,
                                TokenKind hint = TOK_LIMIT);

    PropertyName* labelOrIdentifierReference(YieldHandling yieldHandling);

    PropertyName* labelIdentifier(YieldHandling yieldHandling) {
        return labelOrIdentifierReference(yieldHandling);
    }

    PropertyName* identifierReference(YieldHandling yieldHandling) {
        return labelOrIdentifierReference(yieldHandling);
    }

    PropertyName* importedBinding() {
        return bindingIdentifier(YieldIsName);
    }

    Node identifierReference(Handle<PropertyName*> name);

    bool matchLabel(YieldHandling yieldHandling, MutableHandle<PropertyName*> label);

    bool matchInOrOf(bool* isForInp, bool* isForOfp);

    bool hasUsedFunctionSpecialName(HandlePropertyName name);
    bool declareFunctionArgumentsObject();
    bool declareFunctionThis();
    Node newInternalDotName(HandlePropertyName name);
    Node newThisName();
    Node newDotGeneratorName();
    bool declareDotGeneratorName();

    bool skipLazyInnerFunction(Node pn, uint32_t toStringStart, FunctionSyntaxKind kind,
                               bool tryAnnexB);
    bool innerFunction(Node pn, ParseContext* outerpc, HandleFunction fun, uint32_t toStringStart,
                       InHandling inHandling, YieldHandling yieldHandling,
                       FunctionSyntaxKind kind,
                       GeneratorKind generatorKind, FunctionAsyncKind asyncKind, bool tryAnnexB,
                       Directives inheritedDirectives, Directives* newDirectives);
    bool trySyntaxParseInnerFunction(Node pn, HandleFunction fun, uint32_t toStringStart,
                                     InHandling inHandling, YieldHandling yieldHandling,
                                     FunctionSyntaxKind kind,
                                     GeneratorKind generatorKind, FunctionAsyncKind asyncKind,
                                     bool tryAnnexB,
                                     Directives inheritedDirectives, Directives* newDirectives);
    bool finishFunctionScopes(bool isStandaloneFunction);
    bool finishFunction(bool isStandaloneFunction = false);
    bool leaveInnerFunction(ParseContext* outerpc);

    bool matchOrInsertSemicolonHelper(TokenStream::Modifier modifier);
    bool matchOrInsertSemicolonAfterExpression();
    bool matchOrInsertSemicolonAfterNonExpression();

  public:
    enum FunctionCallBehavior {
        PermitAssignmentToFunctionCalls,
        ForbidAssignmentToFunctionCalls
    };

    bool isValidSimpleAssignmentTarget(Node node,
                                       FunctionCallBehavior behavior = ForbidAssignmentToFunctionCalls);

  private:
    bool checkIncDecOperand(Node operand, uint32_t operandOffset);
    bool checkStrictAssignment(Node lhs);

    void reportMissingClosing(unsigned errorNumber, unsigned noteNumber, uint32_t openedPos);

    void reportRedeclaration(HandlePropertyName name, DeclarationKind prevKind, TokenPos pos,
                             uint32_t prevPos);
    bool notePositionalFormalParameter(Node fn, HandlePropertyName name, uint32_t beginPos,
                                       bool disallowDuplicateParams, bool* duplicatedParam);
    bool noteDestructuredPositionalFormalParameter(Node fn, Node destruct);
    mozilla::Maybe<DeclarationKind> isVarRedeclaredInEval(HandlePropertyName name,
                                                          DeclarationKind kind);
    bool tryDeclareVar(HandlePropertyName name, DeclarationKind kind, uint32_t beginPos,
                       mozilla::Maybe<DeclarationKind>* redeclaredKind, uint32_t* prevPos);
    bool tryDeclareVarForAnnexBLexicalFunction(HandlePropertyName name, uint32_t beginPos,
                                               bool* tryAnnexB);
    bool checkLexicalDeclarationDirectlyWithinBlock(ParseContext::Statement& stmt,
                                                    DeclarationKind kind, TokenPos pos);
    bool noteDeclaredName(HandlePropertyName name, DeclarationKind kind, TokenPos pos);
    bool noteUsedName(HandlePropertyName name);
    bool hasUsedName(HandlePropertyName name);

    // Required on Scope exit.
    bool propagateFreeNamesAndMarkClosedOverBindings(ParseContext::Scope& scope);

    Node finishLexicalScope(ParseContext::Scope& scope, Node body);

    Node propertyName(YieldHandling yieldHandling,
                      const mozilla::Maybe<DeclarationKind>& maybeDecl, Node propList,
                      PropertyType* propType, MutableHandleAtom propAtom);
    Node computedPropertyName(YieldHandling yieldHandling,
                              const mozilla::Maybe<DeclarationKind>& maybeDecl, Node literal);
    Node arrayInitializer(YieldHandling yieldHandling, PossibleError* possibleError);
    Node newRegExp();

    Node objectLiteral(YieldHandling yieldHandling, PossibleError* possibleError);

    Node bindingInitializer(Node lhs, DeclarationKind kind, YieldHandling yieldHandling);
    Node bindingIdentifier(DeclarationKind kind, YieldHandling yieldHandling);
    Node bindingIdentifierOrPattern(DeclarationKind kind, YieldHandling yieldHandling,
                                    TokenKind tt);
    Node objectBindingPattern(DeclarationKind kind, YieldHandling yieldHandling);
    Node arrayBindingPattern(DeclarationKind kind, YieldHandling yieldHandling);

    void checkDestructuringAssignmentTarget(Node expr, TokenPos exprPos,
                                            PossibleError* possibleError);
    void checkDestructuringAssignmentElement(Node expr, TokenPos exprPos,
                                             PossibleError* possibleError);

    Node newNumber(const Token& tok) {
        return handler.newNumber(tok.number(), tok.decimalPoint(), tok.pos);
    }

    static Node null() { return ParseHandler<CharT>::null(); }

    JSAtom* prefixAccessorName(PropertyType propType, HandleAtom propAtom);

    bool asmJS(Node list);
};

template <class Parser>
class MOZ_STACK_CLASS AutoAwaitIsKeyword
{
  private:
    Parser* parser_;
    bool oldAwaitIsKeyword_;

  public:
    AutoAwaitIsKeyword(Parser* parser, bool awaitIsKeyword) {
        parser_ = parser;
        oldAwaitIsKeyword_ = parser_->awaitIsKeyword_;
        parser_->setAwaitIsKeyword(awaitIsKeyword);
    }

    ~AutoAwaitIsKeyword() {
        parser_->setAwaitIsKeyword(oldAwaitIsKeyword_);
    }
};

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_Parser_h */
