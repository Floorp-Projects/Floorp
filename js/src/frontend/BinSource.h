/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BinSource_h
#define frontend_BinSource_h

/**
 * A Binary AST parser.
 *
 * At the time of this writing, this parser implements the grammar of ES5
 * and trusts its input (in particular, variable declarations).
 */

#include "mozilla/Maybe.h"

#include "frontend/BCEParserHandle.h"
#include "frontend/BinToken.h"
#include "frontend/BinTokenReaderMultipart.h"
#include "frontend/BinTokenReaderTester.h"
#include "frontend/FullParseHandler.h"
#include "frontend/ParseContext.h"
#include "frontend/ParseNode.h"
#include "frontend/SharedContext.h"

#include "js/CompileOptions.h"
#include "js/GCHashTable.h"
#include "js/GCVector.h"
#include "js/Result.h"

namespace js {
namespace frontend {

class BinASTParserBase: private JS::AutoGCRooter
{
  public:
    BinASTParserBase(JSContext* cx, LifoAlloc& alloc, UsedNameTracker& usedNames,
                     HandleScriptSourceObject sourceObject, Handle<LazyScript*> lazyScript);
    ~BinASTParserBase();

  public:
    // Names


    bool hasUsedName(HandlePropertyName name);

    // --- GC.

    virtual void doTrace(JSTracer* trc) {}

    void trace(JSTracer* trc) {
        ObjectBox::TraceList(trc, traceListHead_);
        doTrace(trc);
    }


  public:
    ParseNode* allocParseNode(size_t size) {
        MOZ_ASSERT(size == sizeof(ParseNode));
        return static_cast<ParseNode*>(nodeAlloc_.allocNode());
    }

    JS_DECLARE_NEW_METHODS(new_, allocParseNode, inline)

    // Needs access to AutoGCRooter.
    friend void TraceBinParser(JSTracer* trc, JS::AutoGCRooter* parser);

  protected:
    JSContext* cx_;

    // ---- Memory-related stuff
  protected:
    LifoAlloc& alloc_;
    ObjectBox* traceListHead_;
    UsedNameTracker& usedNames_;
  private:
    LifoAlloc::Mark tempPoolMark_;
    ParseNodeAllocator nodeAlloc_;

    // ---- Parsing-related stuff
  protected:
    // Root atoms and objects allocated for the parse tree.
    AutoKeepAtoms keepAtoms_;

    RootedScriptSourceObject sourceObject_;
    Rooted<LazyScript*> lazyScript_;
    ParseContext* parseContext_;
    FullParseHandler factory_;

    friend class BinParseContext;
};

/**
 * The parser for a Binary AST.
 *
 * By design, this parser never needs to backtrack or look ahead. Errors are not
 * recoverable.
 */
template<typename Tok>
class BinASTParser : public BinASTParserBase, public ErrorReporter, public BCEParserHandle
{
  public:
    using Tokenizer = Tok;

    using AutoList = typename Tokenizer::AutoList;
    using AutoTaggedTuple = typename Tokenizer::AutoTaggedTuple;
    using AutoTuple = typename Tokenizer::AutoTuple;
    using BinFields = typename Tokenizer::BinFields;
    using Chars = typename Tokenizer::Chars;

  public:
    BinASTParser(JSContext* cx, LifoAlloc& alloc, UsedNameTracker& usedNames, const JS::ReadOnlyCompileOptions& options,
                 HandleScriptSourceObject sourceObject, Handle<LazyScript*> lazyScript = nullptr)
        : BinASTParserBase(cx, alloc, usedNames, sourceObject, lazyScript)
        , options_(options)
        , variableDeclarationKind_(VariableDeclarationKind::Var)
    {
    }
    ~BinASTParser()
    {
    }

    /**
     * Parse a buffer, returning a node (which may be nullptr) in case of success
     * or Nothing() in case of error.
     *
     * The instance of `ParseNode` MAY NOT survive the `BinASTParser`. Indeed,
     * destruction of the `BinASTParser` will also destroy the `ParseNode`.
     *
     * In case of error, the parser reports the JS error.
     */
    JS::Result<ParseNode*> parse(GlobalSharedContext* globalsc,
                                 const uint8_t* start, const size_t length,
                                 BinASTSourceMetadata** metadataPtr = nullptr);
    JS::Result<ParseNode*> parse(GlobalSharedContext* globalsc, const Vector<uint8_t>& data,
                                 BinASTSourceMetadata** metadataPtr = nullptr);

    JS::Result<ParseNode*> parseLazyFunction(ScriptSource* src, const size_t firstOffset);

  private:
    MOZ_MUST_USE JS::Result<ParseNode*> parseAux(GlobalSharedContext* globalsc,
                                                 const uint8_t* start, const size_t length,
                                                 BinASTSourceMetadata** metadataPtr = nullptr);

    // --- Raise errors.
    //
    // These methods return a (failed) JS::Result for convenience.

    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseInvalidClosedVar(JSAtom* name);
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseMissingVariableInAssertedScope(JSAtom* name);
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseMissingDirectEvalInAssertedScope();
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseInvalidKind(const char* superKind,
        const BinKind kind);
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseInvalidVariant(const char* kind,
        const BinVariant value);
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseMissingField(const char* kind,
        const BinField field);
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseEmpty(const char* description);
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseOOM();
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseError(const char* description);
    MOZ_MUST_USE mozilla::GenericErrorResult<JS::Error&> raiseError(BinKind kind,
        const char* description);


    // Ensure that this parser will never be used again.
    void poison();

    // The owner or the target of Asserted*Scope.
    enum class AssertedScopeKind {
        Block,
        Catch,
        Global,
        Parameter,
        Var,
    };

    // Auto-generated methods
#include "frontend/BinSource-auto.h"

    // --- Auxiliary parsing functions

    // Build a function object for a function-producing production. Called AFTER creating the scope.
    JS::Result<CodeNode*>
    makeEmptyFunctionNode(const size_t start, const BinKind kind, FunctionBox* funbox);
    JS::Result<ParseNode*>
    buildFunction(const size_t start, const BinKind kind, ParseNode* name, ListNode* params,
                  ParseNode* body, FunctionBox* funbox);
    JS::Result<FunctionBox*>
    buildFunctionBox(GeneratorKind generatorKind, FunctionAsyncKind functionAsyncKind, FunctionSyntaxKind syntax, ParseNode* name);

    // Add name to a given scope.
    MOZ_MUST_USE JS::Result<Ok> addScopeName(AssertedScopeKind scopeKind, HandleAtom name,
                                             ParseContext::Scope* scope,
                                             DeclarationKind declKind,
                                             bool isCaptured,
                                             bool allowDuplicateName);

    void captureFunctionName();

    // Map AssertedScopeKind and AssertedDeclaredKind for single binding to
    // corresponding ParseContext::Scope to store the binding, and
    // DeclarationKind for the binding.
    MOZ_MUST_USE JS::Result<Ok> getDeclaredScope(AssertedScopeKind scopeKind,
                                                 AssertedDeclaredKind kind,
                                                 ParseContext::Scope*& scope,
                                                 DeclarationKind& declKind);
    MOZ_MUST_USE JS::Result<Ok> getBoundScope(AssertedScopeKind scopeKind,
                                              ParseContext::Scope*& scope,
                                              DeclarationKind& declKind);

    MOZ_MUST_USE JS::Result<Ok> checkBinding(JSAtom* name);

    MOZ_MUST_USE JS::Result<Ok> checkPositionalParameterIndices(Handle<GCVector<JSAtom*>> positionalParams,
                                                                ListNode* params);

    MOZ_MUST_USE JS::Result<Ok> checkFunctionLength(uint32_t expectedLength);

    // When leaving a scope, check that none of its bindings are known closed over and un-marked.
    MOZ_MUST_USE JS::Result<Ok> checkClosedVars(ParseContext::Scope& scope);

    // As a convenience, a helper that checks the body, parameter, and recursive binding scopes.
    MOZ_MUST_USE JS::Result<Ok> checkFunctionClosedVars();

    // --- Utilities.

    MOZ_MUST_USE JS::Result<Ok> prependDirectivesToBody(ListNode* body,
        ListNode* directives);

    MOZ_MUST_USE JS::Result<Ok> prependDirectivesImpl(ListNode* body,
        ParseNode* directive);

    // Optionally force a strict context without restarting the parse when we see a strict
    // directive.
    void forceStrictIfNecessary(SharedContext* sc, ListNode* directives);

  private: // Implement ErrorReporter
    const JS::ReadOnlyCompileOptions& options_;

    const JS::ReadOnlyCompileOptions& options() const override {
        return this->options_;
    }

    void doTrace(JSTracer* trc) final;

  public:
    virtual ObjectBox* newObjectBox(JSObject* obj) override {
        MOZ_ASSERT(obj);

        /*
         * We use JSContext.tempLifoAlloc to allocate parsed objects and place them
         * on a list in this Parser to ensure GC safety. Thus the tempLifoAlloc
         * arenas containing the entries must be alive until we are done with
         * scanning, parsing and code generation for the whole script or top-level
         * function.
         */

         ObjectBox* objbox = alloc_.new_<ObjectBox>(obj, traceListHead_);
         if (!objbox) {
             ReportOutOfMemory(cx_);
             return nullptr;
        }

        traceListHead_ = objbox;

        return objbox;
    }


    virtual ErrorReporter& errorReporter() override {
        return *this;
    }
    virtual const ErrorReporter& errorReporter() const override {
        return *this;
    }

    virtual FullParseHandler& astGenerator() override {
        return factory_;
    }

    virtual void lineAndColumnAt(size_t offset, uint32_t* line, uint32_t* column) const override {
        *line = lineAt(offset);
        *column = columnAt(offset);
    }
    virtual uint32_t lineAt(size_t offset) const override {
        return 0;
    }
    virtual uint32_t columnAt(size_t offset) const override {
        return offset;
    }

    virtual bool isOnThisLine(size_t offset, uint32_t lineNum, bool *isOnSameLine) const override {
        if (lineNum != 0) {
            return false;
        }
        *isOnSameLine = true;
        return true;
    }

    virtual void currentLineAndColumn(uint32_t* line, uint32_t* column) const override {
        *line = 0;
        *column = offset();
    }
    size_t offset() const {
        if (tokenizer_.isSome()) {
            return tokenizer_->offset();
        }

        return 0;
    }
    virtual bool hasTokenizationStarted() const override {
        return tokenizer_.isSome();
    }
    virtual void reportErrorNoOffsetVA(unsigned errorNumber, va_list args) override;
    virtual void errorAtVA(uint32_t offset, unsigned errorNumber, va_list* args) override;
    virtual bool reportExtraWarningErrorNumberVA(UniquePtr<JSErrorNotes> notes, uint32_t offset, unsigned errorNumber, va_list* args) override;
    virtual const char* getFilename() const override {
        return this->options_.filename();
    }

  private: // Implement ErrorReporter
    mozilla::Maybe<Tokenizer> tokenizer_;
    VariableDeclarationKind variableDeclarationKind_;

    friend class BinParseContext;
    friend class AutoVariableDeclarationKind;

    // Helper class: Restore field `variableDeclarationKind` upon leaving a scope.
    class MOZ_RAII AutoVariableDeclarationKind {
      public:
        explicit AutoVariableDeclarationKind(BinASTParser<Tok>* parser
                                             MOZ_GUARD_OBJECT_NOTIFIER_PARAM)
            : parser_(parser)
            , kind(parser->variableDeclarationKind_)
        {
            MOZ_GUARD_OBJECT_NOTIFIER_INIT;
        }
        ~AutoVariableDeclarationKind() {
            parser_->variableDeclarationKind_ = kind;
        }
        private:
        BinASTParser<Tok>* parser_;
        BinASTParser<Tok>::VariableDeclarationKind kind;
        MOZ_DECL_USE_GUARD_OBJECT_NOTIFIER
    };
};

class BinParseContext : public ParseContext
{
  public:
    template<typename Tok>
    BinParseContext(JSContext* cx, BinASTParser<Tok>* parser, SharedContext* sc,
        Directives* newDirectives)
        : ParseContext(cx, parser->parseContext_, sc, *parser,
                       parser->usedNames_, newDirectives, /* isFull = */ true)
    { }
};


extern template class BinASTParser<BinTokenReaderMultipart>;
extern template class BinASTParser<BinTokenReaderTester>;

} // namespace frontend
} // namespace js

#endif // frontend_BinSource_h
