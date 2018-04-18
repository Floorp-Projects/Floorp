/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinSource.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/Maybe.h"
#include "mozilla/Move.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Vector.h"

#include "frontend/BinSource-macros.h"
#include "frontend/BinTokenReaderTester.h"
#include "frontend/FullParseHandler.h"
#include "frontend/Parser.h"
#include "frontend/SharedContext.h"

#include "vm/RegExpObject.h"

#include "frontend/ParseContext-inl.h"
#include "frontend/ParseNode-inl.h"


// # About compliance with EcmaScript
//
// For the moment, this parser implements ES5. Future versions will be extended
// to ES6 and further on.
//
// By design, it does NOT implement Annex B.3.3. If possible, we would like
// to avoid going down that rabbit hole.
//
//
// # About the AST
//
// At this stage of experimentation, the AST specifications change often. This
// version of the parser attempts to implement
// https://gist.github.com/Yoric/2390f0367515c079172be2526349b294
//
//
// # About validating the AST
//
// Normally, this implementation validates all properties of the AST *except* the
// order of fields, which is partially constrained by the AST spec (e.g. in a block,
// field `scope` must appear before field `body`, etc.).
//
//
// # About names and scopes
//
// One of the key objectives of the BinAST syntax is to be able to entirely skip
// parsing inner functions until they are needed. With a purely syntactic AST,
// this is generally impossible, as we would need to walk the AST to find
// lexically-bound/var-bound variables, instances of direct eval, etc.
//
// To achieve this, BinAST files contain scope data, as instances of
// `BinJS:Scope` nodes. Rather than walking the AST to assign bindings
// to scopes, we extract data from the `BinJS:Scope` and check it lazily,
// once we actually need to walk the AST.
//
// WARNING: The current implementation DOES NOT perform the check yet. It
// is therefore unsafe.
//
// # About directives
//
// Currently, directives are ignored and treated as regular strings.
//
// They should be treated lazily (whenever we open a subscope), like bindings.

using namespace mozilla;

namespace js {
namespace frontend {

using UsedNamePtr = UsedNameTracker::UsedNameMap::Ptr;

// ------------- Toplevel constructions

template<typename Tok> JS::Result<ParseNode*>
BinASTParser<Tok>::parse(const Vector<uint8_t>& data)
{
    return parse(data.begin(), data.length());
}

template<typename Tok> JS::Result<ParseNode*>
BinASTParser<Tok>::parse(const uint8_t* start, const size_t length)
{
    auto result = parseAux(start, length);
    poison(); // Make sure that the parser is never used again accidentally.
    return result;
}


template<typename Tok> JS::Result<ParseNode*>
BinASTParser<Tok>::parseAux(const uint8_t* start, const size_t length)
{
    tokenizer_.emplace(cx_, start, length);

    Directives directives(options().strictOption);
    GlobalSharedContext globalsc(cx_, ScopeKind::Global,
                                 directives, options().extraWarningsOption);
    BinParseContext globalpc(cx_, this, &globalsc, /* newDirectives = */ nullptr);
    if (!globalpc.init())
        return cx_->alreadyReportedError();

    ParseContext::VarScope varScope(cx_, &globalpc, usedNames_);
    if (!varScope.init(&globalpc))
        return cx_->alreadyReportedError();

    MOZ_TRY(tokenizer_->readHeader());

    ParseNode* result(nullptr);
    MOZ_TRY_VAR(result, parseProgram());

    Maybe<GlobalScope::Data*> bindings = NewGlobalScopeData(cx_, varScope, alloc_, parseContext_);
    if (!bindings)
        return cx_->alreadyReportedError();
    globalsc.bindings = *bindings;

    return result; // Magic conversion to Ok.
}

template<typename Tok> JS::Result<FunctionBox*>
BinASTParser<Tok>::buildFunctionBox(GeneratorKind generatorKind,
    FunctionAsyncKind functionAsyncKind,
    FunctionSyntaxKind syntax)
{
    // Allocate the function before walking down the tree.
    RootedFunction fun(cx_);
    BINJS_TRY_VAR(fun, NewFunctionWithProto(cx_,
            /* native = */ nullptr,
            /* nargs placeholder = */ 0,
            JSFunction::INTERPRETED_NORMAL,
            /* enclosingEnv = */ nullptr,
            /* name (placeholder) = */ nullptr,
            /* proto = */ nullptr,
            gc::AllocKind::FUNCTION,
            TenuredObject
    ));
    BINJS_TRY_DECL(funbox, alloc_.new_<FunctionBox>(cx_,
        traceListHead_,
        fun,
        /* toStringStart = */ 0,
        Directives(parseContext_),
        /* extraWarning = */ false,
        generatorKind,
        functionAsyncKind));

    traceListHead_ = funbox;
    funbox->initWithEnclosingParseContext(parseContext_, syntax);
    return funbox;
}

template<typename Tok> JS::Result<ParseNode*>
BinASTParser<Tok>::buildFunction(const size_t start, const BinKind kind, ParseNode* name,
                            ParseNode* params, ParseNode* body, FunctionBox* funbox)
{
    TokenPos pos = tokenizer_->pos(start);

    RootedAtom atom((cx_));
    if (name)
        atom = name->pn_atom;


    funbox->function()->setArgCount(params ? uint16_t(params->pn_count) : 0);
    funbox->function()->initAtom(atom);

    // ParseNode represents the body as concatenated after the params.
    params->appendWithoutOrderAssumption(body);

    BINJS_TRY_DECL(result, kind == BinKind::FunctionDeclaration
                     ? factory_.newFunctionStatement(pos)
                     : factory_.newFunctionExpression(pos));

    factory_.setFunctionBox(result, funbox);
    factory_.setFunctionFormalParametersAndBody(result, params);

    HandlePropertyName dotThis = cx_->names().dotThis;
    const bool declareThis = hasUsedName(dotThis) ||
                             funbox->bindingsAccessedDynamically() ||
                             funbox->isDerivedClassConstructor();

    if (declareThis) {
        ParseContext::Scope& funScope = parseContext_->functionScope();
        ParseContext::Scope::AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotThis);
        MOZ_ASSERT(!p);
        BINJS_TRY(funScope.addDeclaredName(parseContext_, p, dotThis, DeclarationKind::Var,
                                     DeclaredNameInfo::npos));
        funbox->setHasThisBinding();
    }

    BINJS_TRY_DECL(bindings,
             NewFunctionScopeData(cx_, parseContext_->functionScope(),
                                  /* hasParameterExprs = */ false, alloc_, parseContext_));

    funbox->functionScopeBindings().set(*bindings);

    return result;
}

template<typename Tok> JS::Result<Ok>
BinASTParser<Tok>::parseAndUpdateCapturedNames()
{
    // For the moment, we do not attempt to validate the list of captured names.
    AutoList guard(*tokenizer_);
    uint32_t length = 0;

    MOZ_TRY(tokenizer_->enterList(length, guard));
    RootedAtom name(cx_);
    for (uint32_t i = 0; i < length; ++i) {
        name = nullptr;

        MOZ_TRY_VAR(name, tokenizer_->readAtom());
    }
    MOZ_TRY(guard.done());
    return Ok();
}

template<typename Tok> JS::Result<Ok>
BinASTParser<Tok>::parseAndUpdateScopeNames(ParseContext::Scope& scope, DeclarationKind kind)
{
    AutoList guard(*tokenizer_);
    uint32_t length = 0;

    MOZ_TRY(tokenizer_->enterList(length, guard));
    RootedAtom name(cx_);
    for (uint32_t i = 0; i < length; ++i) {
        name = nullptr;

        MOZ_TRY_VAR(name, tokenizer_->readAtom());
        auto ptr = scope.lookupDeclaredNameForAdd(name);
        if (ptr)
            return raiseError("Variable redeclaration");

        BINJS_TRY(scope.addDeclaredName(parseContext_, ptr, name.get(), kind, tokenizer_->offset()));
    }
    MOZ_TRY(guard.done());
    return Ok();
}

template<typename Tok> JS::Result<Ok>
BinASTParser<Tok>::checkBinding(JSAtom* name)
{
    // Check that the variable appears in the corresponding scope.
    ParseContext::Scope& scope =
        variableDeclarationKind_ == VariableDeclarationKind::Var
        ? parseContext_->varScope()
        : *parseContext_->innermostScope();

    auto ptr = scope.lookupDeclaredName(name->asPropertyName());
    if (!ptr)
        return raiseMissingVariableInAssertedScope(name);

    return Ok();
}

template<typename Tok> JS::Result<ParseNode*>
BinASTParser<Tok>::appendDirectivesToBody(ParseNode* body, ParseNode* directives)
{
    ParseNode* result = body;
    if (directives && directives->pn_count >= 1) {
        MOZ_ASSERT(directives->isArity(PN_LIST));

        // Convert directive list to a list of strings.
        BINJS_TRY_DECL(prefix, factory_.newStatementList(directives->pn_head->pn_pos));
        for (ParseNode* iter = directives->pn_head; iter != nullptr; iter = iter->pn_next) {
            BINJS_TRY_DECL(statement, factory_.newExprStatement(iter, iter->pn_pos.end));
            prefix->appendWithoutOrderAssumption(statement);
        }

        // Prepend to the body.
        ParseNode* iter = body->pn_head;
        while (iter) {
            ParseNode* next = iter->pn_next;
            prefix->appendWithoutOrderAssumption(iter);
            iter = next;
        }
        prefix->setKind(body->getKind());
        prefix->setOp(body->getOp());
        result = prefix;
#if defined(DEBUG)
        result->checkListConsistency();
#endif // defined(DEBUG)
    }

    return result;
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseMissingVariableInAssertedScope(JSAtom* name)
{
    // For the moment, we don't trust inputs sufficiently to put the name
    // in an error message.
    return raiseError("Missing variable in AssertedScope");
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseMissingDirectEvalInAssertedScope()
{
    return raiseError("Direct call to `eval` was not declared in AssertedScope");
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseInvalidKind(const char* superKind, const BinKind kind)
{
    Sprinter out(cx_);
    BINJS_TRY(out.init());
    BINJS_TRY(out.printf("In %s, invalid kind %s", superKind, describeBinKind(kind)));
    return raiseError(out.string());
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseInvalidVariant(const char* kind, const BinVariant value)
{
    Sprinter out(cx_);
    BINJS_TRY(out.init());
    BINJS_TRY(out.printf("In %s, invalid variant '%s'", kind, describeBinVariant(value)));

    return raiseError(out.string());
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseMissingField(const char* kind, const BinField field)
{
    Sprinter out(cx_);
    BINJS_TRY(out.init());
    BINJS_TRY(out.printf("In %s, missing field '%s'", kind, describeBinField(field)));

    return raiseError(out.string());
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseEmpty(const char* description)
{
    Sprinter out(cx_);
    BINJS_TRY(out.init());
    BINJS_TRY(out.printf("Empty %s", description));

    return raiseError(out.string());
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseOOM()
{
    return tokenizer_->raiseOOM();
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseError(BinKind kind, const char* description)
{
    Sprinter out(cx_);
    BINJS_TRY(out.init());
    BINJS_TRY(out.printf("In %s, ", description));
    return tokenizer_->raiseError(out.string());
}

template<typename Tok> mozilla::GenericErrorResult<JS::Error&>
BinASTParser<Tok>::raiseError(const char* description)
{
    return tokenizer_->raiseError(description);
}

template<typename Tok> void
BinASTParser<Tok>::poison()
{
    tokenizer_.reset();
}

template<typename Tok> void
BinASTParser<Tok>::reportErrorNoOffsetVA(unsigned errorNumber, va_list args)
{
    ErrorMetadata metadata;
    metadata.filename = getFilename();
    metadata.lineNumber = 0;
    metadata.columnNumber = offset();
    ReportCompileError(cx_, Move(metadata), nullptr, JSREPORT_ERROR, errorNumber, args);
}

bool
BinASTParserBase::hasUsedName(HandlePropertyName name)
{
    if (UsedNamePtr p = usedNames_.lookup(name))
        return p->value().isUsedInScript(parseContext_->scriptId());

    return false;
}

void
TraceBinParser(JSTracer* trc, AutoGCRooter* parser)
{
    static_cast<BinASTParserBase*>(parser)->trace(trc);
}


// Force class instantiation.
// This ensures that the symbols are built, without having to export all our
// code (and its baggage of #include and macros) in the header.
template class BinASTParser<BinTokenReaderMultipart>;
template class BinASTParser<BinTokenReaderTester>;

} // namespace frontend
} // namespace js

