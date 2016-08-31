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

#include "jspubtd.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/FullParseHandler.h"
#include "frontend/NameCollections.h"
#include "frontend/SharedContext.h"
#include "frontend/SyntaxParseHandler.h"

namespace js {

class ModuleObject;

namespace frontend {

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

        template <typename ParseHandler>
        explicit Scope(Parser<ParseHandler>* parser)
          : Nestable<Scope>(&parser->pc->innermostScope_),
            declared_(parser->context->frontendCollectionPool()),
            id_(parser->usedNames.nextScopeId())
        { }

        void dump(ParseContext* pc);

        uint32_t id() const {
            return id_;
        }

        MOZ_MUST_USE bool init(ParseContext* pc) {
            if (id_ == UINT32_MAX) {
                pc->tokenStream_.reportError(JSMSG_NEED_DIET, js_script_str);
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
                                          DeclarationKind kind)
        {
            return maybeReportOOM(pc, declared_->add(p, name, DeclaredNameInfo(kind)));
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
        template <typename ParseHandler>
        explicit VarScope(Parser<ParseHandler>* parser)
          : Scope(parser)
        {
            useAsVarScope(parser->pc);
        }
    };

  private:
    // Context shared between parsing and bytecode generation.
    SharedContext* sc_;

    // TokenStream used for error reporting.
    TokenStream& tokenStream_;

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

    // All inner functions in this context. Only used when syntax parsing.
    Rooted<GCVector<JSFunction*, 8>> innerFunctionsForLazy;

    // In a function context, points to a Directive struct that can be updated
    // to reflect new directives encountered in the Directive Prologue that
    // require reparsing the function. In global/module/generator-tail contexts,
    // we don't need to reparse when encountering a DirectivePrologue so this
    // pointer may be nullptr.
    Directives* newDirectives;

    // Set when parsing a declaration-like destructuring pattern.  This flag
    // causes PrimaryExpr to create PN_NAME parse nodes for variable references
    // which are not hooked into any definition's use chain, added to any tree
    // context's AtomList, etc. etc.  checkDestructuring will do that work
    // later.
    //
    // The comments atop checkDestructuring explain the distinction between
    // assignment-like and declaration-like destructuring patterns, and why
    // they need to be treated differently.
    mozilla::Maybe<DeclarationKind> inDestructuringDecl;

    // Set when parsing a function and it has 'return <expr>;'
    bool funHasReturnExpr;

    // Set when parsing a function and it has 'return;'
    bool funHasReturnVoid;

  public:
    template <typename ParseHandler>
    ParseContext(Parser<ParseHandler>* prs, SharedContext* sc, Directives* newDirectives)
      : Nestable<ParseContext>(&prs->pc),
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

    bool isGenerator() const {
        return generatorKind() != NotGenerator;
    }

    bool isLegacyGenerator() const {
        return generatorKind() == LegacyGenerator;
    }

    bool isStarGenerator() const {
        return generatorKind() == StarGenerator;
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
        explicit UsedNameInfo(ExclusiveContext* cx)
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
    explicit UsedNameTracker(ExclusiveContext* cx)
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

    MOZ_MUST_USE bool noteUse(ExclusiveContext* cx, JSAtom* name,
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

template <typename ParseHandler>
class Parser final : private JS::AutoGCRooter, public StrictModeGetter
{
  private:
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
     * When using PossibleError one should set a pending error at the location
     * where an error occurs. From that point, the error may be resolved
     * (invalidated) or left until the PossibleError is checked.
     *
     * Ex:
     *   PossibleError possibleError(*this);
     *   possibleError.setPending(ParseError, JSMSG_BAD_PROP_ID, false);
     *   // A JSMSG_BAD_PROP_ID ParseError is reported, returns false.
     *   possibleError.checkForExprErrors();
     *
     *   PossibleError possibleError(*this);
     *   possibleError.setPending(ParseError, JSMSG_BAD_PROP_ID, false);
     *   possibleError.setResolved();
     *   // Returns true, no error is reported.
     *   possibleError.checkForExprErrors();
     *
     *   PossibleError possibleError(*this);
     *   // Returns true, no error is reported.
     *   possibleError.checkForExprErrors();
     */
    class MOZ_STACK_CLASS PossibleError
    {
      protected:
        enum ErrorState { None, Pending };
        ErrorState state_;

        // Error reporting fields.
        uint32_t offset_;
        unsigned errorNumber_;
        ParseReportKind reportKind_;
        Parser<ParseHandler>& parser_;
        bool strict_;

      public:
        explicit PossibleError(Parser<ParseHandler>& parser);

        // Set a pending error. Only a single error may be set per instance.
        // Returns true on success or false on failure.
        bool setPending(ParseReportKind kind, unsigned errorNumber, bool strict);

        // Resolve any pending error.
        void setResolved();

        // Return true if an error is pending without reporting
        bool hasError();

        // If there is a pending error report it and return false, otherwise return
        // true.
        bool checkForExprErrors();

        // Pass pending errors between possible error instances. This is useful
        // for extending the lifetime of a pending error beyond the scope of
        // the PossibleError where it was initially set (keeping in mind that
        // PossibleError is a MOZ_STACK_CLASS).
        void transferErrorTo(PossibleError* other);
    };

  public:
    ExclusiveContext* const context;

    LifoAlloc& alloc;

    TokenStream tokenStream;
    LifoAlloc::Mark tempPoolMark;

    /* list of parsed objects for GC tracing */
    ObjectBox* traceListHead;

    /* innermost parse context (stack-allocated) */
    ParseContext* pc;

    // For tracking used names in this parsing session.
    UsedNameTracker& usedNames;

    /* Compression token for aborting. */
    SourceCompressionTask* sct;

    ScriptSource*       ss;

    /* Root atoms and objects allocated for the parsed tree. */
    AutoKeepAtoms       keepAtoms;

    /* Perform constant-folding; must be true when interfacing with the emitter. */
    const bool          foldConstants:1;

  private:
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

    typedef typename ParseHandler::Node Node;

  public:
    /* State specific to the kind of parse being performed. */
    ParseHandler handler;

    void prepareNodeForMutation(Node node) { handler.prepareNodeForMutation(node); }
    void freeTree(Node node) { handler.freeTree(node); }

  private:
    bool reportHelper(ParseReportKind kind, bool strict, uint32_t offset,
                      unsigned errorNumber, va_list args);
  public:
    bool report(ParseReportKind kind, bool strict, Node pn, unsigned errorNumber, ...);
    bool reportNoOffset(ParseReportKind kind, bool strict, unsigned errorNumber, ...);
    bool reportWithOffset(ParseReportKind kind, bool strict, uint32_t offset, unsigned errorNumber,
                          ...);

    Parser(ExclusiveContext* cx, LifoAlloc& alloc, const ReadOnlyCompileOptions& options,
           const char16_t* chars, size_t length, bool foldConstants, UsedNameTracker& usedNames,
           Parser<SyntaxParseHandler>* syntaxParser, LazyScript* lazyOuterFunction);
    ~Parser();

    bool checkOptions();

    // A Parser::Mark is the extension of the LifoAlloc::Mark to the entire
    // Parser's state. Note: clients must still take care that any ParseContext
    // that points into released ParseNodes is destroyed.
    class Mark
    {
        friend class Parser;
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

    friend void js::frontend::MarkParser(JSTracer* trc, JS::AutoGCRooter* parser);

    const char* getFilename() const { return tokenStream.getFilename(); }
    JSVersion versionNumber() const { return tokenStream.versionNumber(); }

    /*
     * Parse a top-level JS script.
     */
    Node parse();

    /*
     * Allocate a new parsed object or function container from
     * cx->tempLifoAlloc.
     */
    ObjectBox* newObjectBox(JSObject* obj);
    FunctionBox* newFunctionBox(Node fn, JSFunction* fun, Directives directives,
                                GeneratorKind generatorKind, bool tryAnnexB);

    /*
     * Create a new function object given a name (which is optional if this is
     * a function expression).
     */
    JSFunction* newFunction(HandleAtom atom, FunctionSyntaxKind kind, GeneratorKind generatorKind,
                            HandleObject proto);

    void trace(JSTracer* trc);

    bool hadAbortedSyntaxParse() {
        return abortedSyntaxParse;
    }
    void clearAbortedSyntaxParse() {
        abortedSyntaxParse = false;
    }

    bool isUnexpectedEOF() const { return isUnexpectedEOF_; }

    bool checkUnescapedName();

  private:
    Parser* thisForCtor() { return this; }

    JSAtom* stopStringCompression();

    Node stringLiteral();
    Node noSubstitutionTemplate();
    Node templateLiteral(YieldHandling yieldHandling);
    bool taggedTemplate(YieldHandling yieldHandling, Node nodeList, TokenKind tt);
    bool appendToCallSiteObj(Node callSiteObj);
    bool addExprAndGetNextTemplStrToken(YieldHandling yieldHandling, Node nodeList,
                                        TokenKind* ttp);
    bool checkStatementsEOF();

    inline Node newName(PropertyName* name);
    inline Node newName(PropertyName* name, TokenPos pos);
    inline Node newYieldExpression(uint32_t begin, Node expr, bool isYieldStar = false);

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

    // Parse a function, given only its body. Used for the Function and
    // Generator constructors.
    Node standaloneFunctionBody(HandleFunction fun, HandleScope enclosingScope,
                                Handle<PropertyNameVector> formals, GeneratorKind generatorKind,
                                Directives inheritedDirectives, Directives* newDirectives);

    // Parse a function, given only its arguments and body. Used for lazily
    // parsed functions.
    Node standaloneLazyFunction(HandleFunction fun, bool strict, GeneratorKind generatorKind);

    // Parse an inner function given an enclosing ParseContext and a
    // FunctionBox for the inner function.
    bool innerFunction(Node pn, ParseContext* outerpc, FunctionBox* funbox,
                       InHandling inHandling, FunctionSyntaxKind kind, GeneratorKind generatorKind,
                       Directives inheritedDirectives, Directives* newDirectives);

    // Parse a function's formal parameters and its body assuming its function
    // ParseContext is already on the stack.
    bool functionFormalParametersAndBody(InHandling inHandling, YieldHandling yieldHandling,
                                         Node pn, FunctionSyntaxKind kind);

    // Determine whether |yield| is a valid name in the current context, or
    // whether it's prohibited due to strictness, JS version, or occurrence
    // inside a star generator.
    bool checkYieldNameValidity();
    bool yieldExpressionsSupported() {
        return versionNumber() >= JSVERSION_1_7 || pc->isGenerator();
    }

    virtual bool strictMode() { return pc->sc()->strict(); }
    bool setLocalStrictMode(bool strict) {
        MOZ_ASSERT(tokenStream.debugHasNoLookahead());
        return pc->sc()->setLocalStrictMode(strict);
    }

    const ReadOnlyCompileOptions& options() const {
        return tokenStream.options();
    }

  private:
    enum InvokedPrediction { PredictUninvoked = false, PredictInvoked = true };
    enum ForInitLocation { InForInit, NotInForInit };

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
    Node functionStmt(YieldHandling yieldHandling, DefaultHandling defaultHandling);
    Node functionExpr(InvokedPrediction invoked = PredictUninvoked);

    Node statementList(YieldHandling yieldHandling);

    Node blockStatement(YieldHandling yieldHandling,
                        unsigned errorNumber = JSMSG_CURLY_IN_COMPOUND);
    Node doWhileStatement(YieldHandling yieldHandling);
    Node whileStatement(YieldHandling yieldHandling);

    Node forStatement(YieldHandling yieldHandling);
    bool forHeadStart(YieldHandling yieldHandling,
                      ParseNodeKind* forHeadKind,
                      Node* forInitialPart,
                      mozilla::Maybe<ParseContext::Scope>& forLetImpliedScope,
                      Node* forInOrOfExpression);
    bool validateForInOrOfLHSExpression(Node target);
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

    Node lexicalDeclaration(YieldHandling yieldHandling, bool isConst);

    Node importDeclaration();
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
              TripledotHandling tripledotHandling,
              PossibleError* possibleError,
              InvokedPrediction invoked = PredictUninvoked);
    Node expr(InHandling inHandling, YieldHandling yieldHandling,
              TripledotHandling tripledotHandling,
              InvokedPrediction invoked = PredictUninvoked);
    Node assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                    TripledotHandling tripledotHandling,
                    PossibleError* possibleError,
                    InvokedPrediction invoked = PredictUninvoked);
    Node assignExpr(InHandling inHandling, YieldHandling yieldHandling,
                    TripledotHandling tripledotHandling,
                    InvokedPrediction invoked = PredictUninvoked);
    Node assignExprWithoutYield(YieldHandling yieldHandling, unsigned err);
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
                   PossibleError* possibleError,
                   InvokedPrediction invoked = PredictUninvoked);
    Node memberExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                    PossibleError* possibleError, TokenKind tt,
                    bool allowCallSyntax, InvokedPrediction invoked = PredictUninvoked);
    Node memberExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling, TokenKind tt,
                    bool allowCallSyntax, InvokedPrediction invoked = PredictUninvoked);
    Node primaryExpr(YieldHandling yieldHandling, TripledotHandling tripledotHandling,
                     PossibleError* possibleError, TokenKind tt,
                     InvokedPrediction invoked = PredictUninvoked);
    Node exprInParens(InHandling inHandling, YieldHandling yieldHandling,
                      TripledotHandling tripledotHandling,
                      PossibleError* possibleError);
    Node exprInParens(InHandling inHandling, YieldHandling yieldHandling,
                      TripledotHandling tripledotHandling);

    bool tryNewTarget(Node& newTarget);
    bool checkAndMarkSuperScope();

    Node methodDefinition(YieldHandling yieldHandling, PropertyType propType, HandleAtom funName);

    /*
     * Additional JS parsers.
     */
    bool functionArguments(YieldHandling yieldHandling, FunctionSyntaxKind kind,
                           Node funcpn);

    Node functionDefinition(InHandling inHandling, YieldHandling yieldHandling, HandleAtom name,
                            FunctionSyntaxKind kind, GeneratorKind generatorKind,
                            InvokedPrediction invoked = PredictUninvoked);

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

    bool argumentList(YieldHandling yieldHandling, Node listNode, bool* isSpread);
    Node destructuringDeclaration(DeclarationKind kind, YieldHandling yieldHandling,
                                  TokenKind tt);
    Node destructuringDeclarationWithoutYield(DeclarationKind kind, YieldHandling yieldHandling,
                                              TokenKind tt, unsigned msg);

    bool namedImportsOrNamespaceImport(TokenKind tt, Node importSpecSet);
    bool checkExportedName(JSAtom* exportName);
    bool checkExportedNamesForDeclaration(Node node);

    enum ClassContext { ClassStatement, ClassExpression };
    Node classDefinition(YieldHandling yieldHandling, ClassContext classContext,
                         DefaultHandling defaultHandling);

    Node identifierName(YieldHandling yieldHandling);

    bool matchLabel(YieldHandling yieldHandling, MutableHandle<PropertyName*> label);

    bool allowsForEachIn() {
#if !JS_HAS_FOR_EACH_IN
        return false;
#else
        return versionNumber() >= JSVERSION_1_6;
#endif
    }

    enum AssignmentFlavor {
        PlainAssignment,
        CompoundAssignment,
        KeyedDestructuringAssignment,
        IncrementAssignment,
        DecrementAssignment,
        ForInOrOfTarget
    };

    bool checkAndMarkAsAssignmentLhs(Node pn, AssignmentFlavor flavor,
                                     PossibleError* possibleError=nullptr);
    bool matchInOrOf(bool* isForInp, bool* isForOfp);

    bool hasUsedFunctionSpecialName(HandlePropertyName name);
    bool declareFunctionArgumentsObject();
    bool declareFunctionThis();
    Node newInternalDotName(HandlePropertyName name);
    Node newThisName();
    Node newDotGeneratorName();
    bool declareDotGeneratorName();

    bool checkFunctionDefinition(HandleAtom funAtom, Node pn, FunctionSyntaxKind kind,
                                 GeneratorKind generatorKind, bool* tryAnnexB);
    bool skipLazyInnerFunction(Node pn, bool tryAnnexB);
    bool innerFunction(Node pn, ParseContext* outerpc, HandleFunction fun,
                       InHandling inHandling, FunctionSyntaxKind kind,
                       GeneratorKind generatorKind, bool tryAnnexB,
                       Directives inheritedDirectives, Directives* newDirectives);
    bool trySyntaxParseInnerFunction(Node pn, HandleFunction fun, InHandling inHandling,
                                     FunctionSyntaxKind kind, GeneratorKind generatorKind,
                                     bool tryAnnexB, Directives inheritedDirectives,
                                     Directives* newDirectives);
    bool finishFunctionScopes();
    bool finishFunction();
    bool leaveInnerFunction(ParseContext* outerpc);

  public:
    enum FunctionCallBehavior {
        PermitAssignmentToFunctionCalls,
        ForbidAssignmentToFunctionCalls
    };

    bool isValidSimpleAssignmentTarget(Node node,
                                       FunctionCallBehavior behavior = ForbidAssignmentToFunctionCalls);

  private:
    bool reportIfArgumentsEvalTarget(Node nameNode);
    bool reportIfNotValidSimpleAssignmentTarget(Node target, AssignmentFlavor flavor);

    bool checkAndMarkAsIncOperand(Node kid, AssignmentFlavor flavor);
    bool checkStrictAssignment(Node lhs);
    bool checkStrictBinding(PropertyName* name, TokenPos pos);

    void reportRedeclaration(HandlePropertyName name, DeclarationKind kind, TokenPos pos);
    bool notePositionalFormalParameter(Node fn, HandlePropertyName name,
                                       bool disallowDuplicateParams = false,
                                       bool* duplicatedParam = nullptr);
    bool noteDestructuredPositionalFormalParameter(Node fn, Node destruct);
    mozilla::Maybe<DeclarationKind> isVarRedeclaredInEval(HandlePropertyName name,
                                                          DeclarationKind kind);
    bool tryDeclareVar(HandlePropertyName name, DeclarationKind kind,
                       mozilla::Maybe<DeclarationKind>* redeclaredKind);
    bool tryDeclareVarForAnnexBLexicalFunction(HandlePropertyName name, bool* tryAnnexB);
    bool checkLexicalDeclarationDirectlyWithinBlock(ParseContext::Statement& stmt,
                                                    DeclarationKind kind, TokenPos pos);
    bool noteDeclaredName(HandlePropertyName name, DeclarationKind kind, TokenPos pos);
    bool noteUsedName(HandlePropertyName name);
    bool hasUsedName(HandlePropertyName name);

    // Required on Scope exit.
    bool propagateFreeNamesAndMarkClosedOverBindings(ParseContext::Scope& scope);

    mozilla::Maybe<GlobalScope::Data*> newGlobalScopeData(ParseContext::Scope& scope,
                                                          uint32_t* functionBindingEnd);
    mozilla::Maybe<ModuleScope::Data*> newModuleScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<EvalScope::Data*> newEvalScopeData(ParseContext::Scope& scope,
                                                      uint32_t* functionBindingEnd);
    mozilla::Maybe<FunctionScope::Data*> newFunctionScopeData(ParseContext::Scope& scope,
                                                              bool hasParameterExprs);
    mozilla::Maybe<VarScope::Data*> newVarScopeData(ParseContext::Scope& scope);
    mozilla::Maybe<LexicalScope::Data*> newLexicalScopeData(ParseContext::Scope& scope);
    Node finishLexicalScope(ParseContext::Scope& scope, Node body);

    Node propertyName(YieldHandling yieldHandling, Node propList,
                      PropertyType* propType, MutableHandleAtom propAtom);
    Node computedPropertyName(YieldHandling yieldHandling, Node literal);
    Node arrayInitializer(YieldHandling yieldHandling);
    Node newRegExp();

    Node objectLiteral(YieldHandling yieldHandling, PossibleError* possibleError);

    // Top-level entrypoint into destructuring pattern checking/name-analyzing.
    bool checkDestructuringPattern(Node pattern,
                                   mozilla::Maybe<DeclarationKind> maybeDecl = mozilla::Nothing());

    // Recursive methods for checking/name-analyzing subcomponents of a
    // destructuring pattern.  The array/object methods *must* be passed arrays
    // or objects.  The name method may be passed anything but will report an
    // error if not passed a name.
    bool checkDestructuringArray(Node arrayPattern, mozilla::Maybe<DeclarationKind> maybeDecl);
    bool checkDestructuringObject(Node objectPattern, mozilla::Maybe<DeclarationKind> maybeDecl);
    bool checkDestructuringName(Node expr, mozilla::Maybe<DeclarationKind> maybeDecl);

    bool makeSetCall(Node node, unsigned errnum);

    Node newNumber(const Token& tok) {
        return handler.newNumber(tok.number(), tok.decimalPoint(), tok.pos);
    }

    static Node null() { return ParseHandler::null(); }

    bool reportBadReturn(Node pn, ParseReportKind kind, unsigned errnum, unsigned anonerrnum);

    JSAtom* prefixAccessorName(PropertyType propType, HandleAtom propAtom);

    TokenPos pos() const { return tokenStream.currentToken().pos; }

    bool asmJS(Node list);

    void addTelemetry(JSCompartment::DeprecatedLanguageExtension e);

    bool warnOnceAboutExprClosure();
};

} /* namespace frontend */
} /* namespace js */

/*
 * Convenience macro to access Parser.tokenStream as a pointer.
 */
#define TS(p) (&(p)->tokenStream)

#endif /* frontend_Parser_h */
