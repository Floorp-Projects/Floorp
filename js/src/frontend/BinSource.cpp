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

// Evaluate an expression, checking that the result is not 0.
//
// Throw `cx->alreadyReportedError()` if it returns 0/nullptr.
#define TRY(EXPR) \
    do { \
        if (!EXPR) \
            return cx_->alreadyReportedError(); \
    } while(false)


#define TRY_VAR(VAR, EXPR) \
    do { \
        VAR = EXPR; \
        if (!VAR) \
            return cx_->alreadyReportedError(); \
    } while (false)

#define TRY_DECL(VAR, EXPR) \
    auto VAR = EXPR; \
    if (!VAR) \
       return cx_->alreadyReportedError();

#define TRY_EMPL(VAR, EXPR) \
    do { \
        auto _tryEmplResult = EXPR; \
        if (!_tryEmplResult) \
            return cx_->alreadyReportedError(); \
        VAR.emplace(_tryEmplResult.unwrap()); \
    } while (false)

#define MOZ_TRY_EMPLACE(VAR, EXPR) \
    do { \
        auto _tryEmplResult = EXPR; \
        if (_tryEmplResult.isErr()) \
            return ::mozilla::Err(_tryEmplResult.unwrapErr()); \
        VAR.emplace(_tryEmplResult.unwrap()); \
    } while (false)

using namespace mozilla;

namespace js {
namespace frontend {

using AutoList = BinTokenReaderTester::AutoList;
using AutoTaggedTuple = BinTokenReaderTester::AutoTaggedTuple;
using AutoTuple = BinTokenReaderTester::AutoTuple;
using BinFields = BinTokenReaderTester::BinFields;
using Chars = BinTokenReaderTester::Chars;
using NameBag = GCHashSet<JSString*>;
using Names = GCVector<JSString*, 8>;
using UsedNamePtr = UsedNameTracker::UsedNameMap::Ptr;

// ------------- Toplevel constructions

JS::Result<ParseNode*>
BinASTParser::parse(const Vector<uint8_t>& data)
{
    return parse(data.begin(), data.length());
}

JS::Result<ParseNode*>
BinASTParser::parse(const uint8_t* start, const size_t length)
{
    auto result = parseAux(start, length);
    poison(); // Make sure that the parser is never used again accidentally.
    return result;
}


JS::Result<ParseNode*>
BinASTParser::parseAux(const uint8_t* start, const size_t length)
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

    ParseNode* result(nullptr);
    MOZ_TRY_VAR(result, parseProgram());

    Maybe<GlobalScope::Data*> bindings = NewGlobalScopeData(cx_, varScope, alloc_, parseContext_);
    if (!bindings)
        return cx_->alreadyReportedError();
    globalsc.bindings = *bindings;

    return result; // Magic conversion to Ok.
}

JS::Result<FunctionBox*>
BinASTParser::buildFunctionBox(GeneratorKind generatorKind, FunctionAsyncKind functionAsyncKind)
{
    // Allocate the function before walking down the tree.
    RootedFunction fun(cx_);
    TRY_VAR(fun, NewFunctionWithProto(cx_,
            /* native = */ nullptr,
            /* nargs placeholder = */ 0,
            JSFunction::INTERPRETED_NORMAL,
            /* enclosingEnv = */ nullptr,
            /* name (placeholder) = */ nullptr,
            /* proto = */ nullptr,
            gc::AllocKind::FUNCTION,
            TenuredObject
    ));
    TRY_DECL(funbox, alloc_.new_<FunctionBox>(cx_,
        traceListHead_,
        fun,
        /* toStringStart = */ 0,
        Directives(parseContext_),
        /* extraWarning = */ false,
        generatorKind,
        functionAsyncKind));

    traceListHead_ = funbox;
    FunctionSyntaxKind syntax = Expression; // FIXME - What if we're assigning?
    // FIXME: The only thing we need to know is whether this is a
    // ClassConstructor/DerivedClassConstructor
    funbox->initWithEnclosingParseContext(parseContext_, syntax);
    return funbox;
}

JS::Result<ParseNode*>
BinASTParser::buildFunction(const size_t start, const BinKind kind, ParseNode* name,
                            ParseNode* params, ParseNode* body, FunctionBox* funbox)
{
    TokenPos pos = tokenizer_->pos(start);

    RootedAtom atom((cx_));
    if (name)
        atom = name->name();


    funbox->function()->setArgCount(uint16_t(params->pn_count));
    funbox->function()->initAtom(atom);

    // ParseNode represents the body as concatenated after the params.
    params->appendWithoutOrderAssumption(body);

    TRY_DECL(result, kind == BinKind::FunctionDeclaration
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
        TRY(funScope.addDeclaredName(parseContext_, p, dotThis, DeclarationKind::Var,
                                     DeclaredNameInfo::npos));
        funbox->setHasThisBinding();
    }

    TRY_DECL(bindings,
             NewFunctionScopeData(cx_, parseContext_->functionScope(),
                                  /* hasParameterExprs = */ false, alloc_, parseContext_));

    funbox->functionScopeBindings().set(*bindings);

    return result;
}

JS::Result<Ok>
BinASTParser::parseAndUpdateCapturedNames()
{
    // For the moment, we do not attempt to validate the list of captured names.
    AutoList guard(*tokenizer_);
    uint32_t length = 0;

    TRY(tokenizer_->enterList(length, guard));
    RootedAtom name(cx_);
    for (uint32_t i = 0; i < length; ++i) {
        name = nullptr;

        MOZ_TRY(readString(&name));
    }
    TRY(guard.done());
    return Ok();
}

JS::Result<Ok>
BinASTParser::parseAndUpdateScopeNames(ParseContext::Scope& scope, DeclarationKind kind)
{
    AutoList guard(*tokenizer_);
    uint32_t length = 0;

    TRY(tokenizer_->enterList(length, guard));
    RootedAtom name(cx_);
    for (uint32_t i = 0; i < length; ++i) {
        name = nullptr;

        MOZ_TRY(readString(&name));
        auto ptr = scope.lookupDeclaredNameForAdd(name);
        if (ptr)
            return raiseError("Variable redeclaration");

        TRY(scope.addDeclaredName(parseContext_, ptr, name.get(), kind, tokenizer_->offset()));
    }
    TRY(guard.done());
    return Ok();
}

JS::Result<Ok>
BinASTParser::checkBinding(JSAtom* name)
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

JS::Result<ParseNode*>
BinASTParser::appendDirectivesToBody(ParseNode* body, ParseNode* directives)
{
    ParseNode* result = body;
    if (directives && directives->pn_count >= 1) {
        MOZ_ASSERT(directives->isArity(PN_LIST));

        // Convert directive list to a list of strings.
        TRY_DECL(prefix, factory_.newStatementList(directives->pn_head->pn_pos));
        for (ParseNode* iter = directives->pn_head; iter != nullptr; iter = iter->pn_next) {
            TRY_DECL(statement, factory_.newExprStatement(iter, iter->pn_pos.end));
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

JS::Result<Ok>
BinASTParser::readString(MutableHandleAtom out)
{
    MOZ_ASSERT(!out);

<<<<<<< working copy
    Maybe<Chars> string;
    MOZ_TRY(readMaybeString(string));
    MOZ_ASSERT(string);
=======
    TokenPos pos = tokenizer_->pos(start);
    ParseNode* result;
    if (kind == BinKind::ContinueStatement) {
#if 0 // FIXME: We probably need to fix the AST before making this check.
        auto validity = parseContext_->checkContinueStatement(label ? label->name() : nullptr);
        if (validity.isErr()) {
            switch (validity.unwrapErr()) {
              case ParseContext::ContinueStatementError::NotInALoop:
                return raiseError(kind, "Not in a loop");
              case ParseContext::ContinueStatementError::LabelNotFound:
                return raiseError(kind, "Label not found");
            }
        }
#endif // 0
        // Ok, this is a valid continue statement.
        TRY_VAR(result, factory_.newContinueStatement(label ? label->name() : nullptr, pos));
    } else {
#if 0 // FIXME: We probably need to fix the AST before making this check.
        auto validity = parseContext_->checkBreakStatement(label ? label->name() : nullptr);
        if (validity.isErr()) {
            switch (validity.unwrapErr()) {
              case ParseContext::BreakStatementError::ToughBreak:
                 return raiseError(kind, "Not in a loop");
              case ParseContext::BreakStatementError::LabelNotFound:
                 return raiseError(kind, "Label not found");
            }
        }
#endif // 0
        // Ok, this is a valid break statement.
        TRY_VAR(result, factory_.newBreakStatement(label ? label->name() : nullptr, pos));
    }

    MOZ_ASSERT(result);

    return result;
}

JS::Result<ParseNode*>
BinASTParser::parseForInit()
{
    // This can be either a VarDecl or an Expression.
    BinFields fields(cx_);
    AutoTaggedTuple guard(*tokenizer_);
    BinKind kind;

    TRY(tokenizer_->enterTaggedTuple(kind, fields, guard));
    ParseNode* result(nullptr);

    switch (kind) {
      case BinKind::VariableDeclaration:
        MOZ_TRY_VAR(result, parseVariableDeclarationAux(kind, fields));
        break;
      default:
        // Parse as expression
        MOZ_TRY_VAR(result, parseExpressionAux(kind, fields));
        break;
    }

    TRY(guard.done());
    return result;
}

JS::Result<ParseNode*>
BinASTParser::parseForInInit()
{
    // This can be either a VarDecl or a Pattern.
    BinFields fields(cx_);
    AutoTaggedTuple guard(*tokenizer_);
    BinKind kind;

    TRY(tokenizer_->enterTaggedTuple(kind, fields, guard));
    ParseNode* result(nullptr);

    switch (kind) {
      case BinKind::VariableDeclaration:
        MOZ_TRY_VAR(result, parseVariableDeclarationAux(kind, fields));
        break;
      default:
        // Parse as expression. Not a joke: http://www.ecma-international.org/ecma-262/5.1/index.html#sec-12.6.4 .
        MOZ_TRY_VAR(result, parseExpressionAux(kind, fields));
        break;
    }

    TRY(guard.done());
    return result;
}

JS::Result<ParseNode*>
BinASTParser::parseFunctionAux(const BinKind kind, const BinFields& fields)
{
    MOZ_ASSERT(isMethodOrFunction(kind));

    const size_t start = tokenizer_->offset();

    ParseNode* id(nullptr);
    ParseNode* params(nullptr);
    ParseNode* body(nullptr);
    ParseNode* directives(nullptr); // Largely ignored for the moment.
    ParseNode* key(nullptr);  // Methods only

    // Allocate the function before walking down the tree.
    RootedFunction fun(cx_);
    TRY_VAR(fun, NewFunctionWithProto(cx_,
            /*native*/nullptr,
            /*nargs ?*/0,
            /*flags */ JSFunction::INTERPRETED_NORMAL,
            /*enclosing env*/nullptr,
            /*name*/ nullptr, // Will be known later
            /*proto*/ nullptr,
            /*alloc*/gc::AllocKind::FUNCTION,
            TenuredObject
    ));
    TRY_DECL(funbox, alloc_.new_<FunctionBox>(cx_,
        traceListHead_,
        fun,
        /* toStringStart = */0,
        /* directives = */Directives(parseContext_),
        /* extraWarning = */false,
        GeneratorKind::NotGenerator,
        FunctionAsyncKind::SyncFunction
    ));

    traceListHead_ = funbox;

    FunctionSyntaxKind syntax;
    switch (kind) {
      case BinKind::FunctionDeclaration:
        syntax = Statement;
        break;
      case BinKind::FunctionExpression:
        syntax = Expression; // FIXME: Probably doesn't work.
        break;
      case BinKind::ObjectMethod:
        syntax = Method;
        break;
      case BinKind::ObjectGetter:
        syntax = Getter;
        break;
      case BinKind::ObjectSetter:
        syntax = Setter;
        break;
      default:
        MOZ_CRASH("Invalid FunctionSyntaxKind"); // Checked above.
    }
    funbox->initWithEnclosingParseContext(parseContext_, syntax);

    // Container scopes.
    ParseContext::Scope& varScope = parseContext_->varScope();
    ParseContext::Scope* letScope = parseContext_->innermostScope();

    // Push a new ParseContext.
    BinParseContext funpc(cx_, this, funbox, /* newDirectives = */ nullptr);
    TRY(funpc.init());
    parseContext_->functionScope().useAsVarScope(parseContext_);
    MOZ_ASSERT(parseContext_->isFunctionBox());

    for (auto field : fields) {
        switch (field) {
          case BinField::Id:
            MOZ_TRY_VAR(id, parseIdentifier());
            break;
          case BinField::Params:
            MOZ_TRY_VAR(params, parseArgumentList());
            break;
          case BinField::BINJS_Scope:
            // This scope information affects the scopes contained in the function body. MUST appear before the `body`.
            MOZ_TRY(parseAndUpdateScope(varScope, *letScope));
            break;
          case BinField::Directives:
            MOZ_TRY_VAR(directives, parseDirectiveList());
            break;
          case BinField::Body:
            MOZ_TRY_VAR(body, parseBlockStatement());
            break;
          case BinField::Key:
            if (!isMethod(kind))
                return raiseInvalidField("Functions (unless defined as methods)", field);

            MOZ_TRY_VAR(key, parseObjectPropertyName());
            break;
          default:
            return raiseInvalidField("Function", field);
        }
    }

    // Inject default values for absent fields.
    if (!params)
        TRY_VAR(params, new_<ListNode>(ParseNodeKind::ParamsBody, tokenizer_->pos()));

    if (!body)
        TRY_VAR(body, factory_.newStatementList(tokenizer_->pos()));

    if (kind == BinKind::FunctionDeclaration && !id) {
        // The name is compulsory only for function declarations.
        return raiseMissingField("FunctionDeclaration", BinField::Id);
    }

    // Reject if required values are missing.
    if (isMethod(kind) && !key)
        return raiseMissingField("method", BinField::Key);

    if (id)
        fun->initAtom(id->pn_atom);

    MOZ_ASSERT(params->isArity(PN_LIST));

    if (!(body->isKind(ParseNodeKind::LexicalScope) &&
          body->pn_u.scope.body->isKind(ParseNodeKind::StatementList)))
    {
        // Promote to lexical scope + statement list.
        if (!body->isKind(ParseNodeKind::StatementList)) {
            TRY_DECL(list, factory_.newStatementList(tokenizer_->pos(start)));

            list->initList(body);
            body = list;
        }

        // Promote to lexical scope.
        TRY_VAR(body, factory_.newLexicalScope(nullptr, body));
    }
    MOZ_ASSERT(body->isKind(ParseNodeKind::LexicalScope));

    MOZ_TRY_VAR(body, appendDirectivesToBody(body, directives));

    params->appendWithoutOrderAssumption(body);

    TokenPos pos = tokenizer_->pos(start);
    TRY_DECL(function, kind == BinKind::FunctionDeclaration
                       ? factory_.newFunctionStatement(pos)
                       : factory_.newFunctionExpression(pos));

    factory_.setFunctionBox(function, funbox);
    factory_.setFunctionFormalParametersAndBody(function, params);

    ParseNode* result;
    if (kind == BinKind::ObjectMethod)
        TRY_VAR(result, factory_.newObjectMethodOrPropertyDefinition(key, function, AccessorType::None));
    else if (kind == BinKind::ObjectGetter)
        TRY_VAR(result, factory_.newObjectMethodOrPropertyDefinition(key, function, AccessorType::Getter));
    else if (kind == BinKind::ObjectSetter)
        TRY_VAR(result, factory_.newObjectMethodOrPropertyDefinition(key, function, AccessorType::Setter));
    else
        result = function;

    // Now handle bindings.
    HandlePropertyName dotThis = cx_->names().dotThis;
    const bool declareThis = hasUsedName(dotThis) || funbox->bindingsAccessedDynamically() || funbox->isDerivedClassConstructor();
>>>>>>> merge rev

    RootedAtom atom(cx_);
    TRY_VAR(atom, AtomizeUTF8Chars(cx_, (const char*)string->begin(), string->length()));

    out.set(Move(atom));
    return Ok();
}

JS::Result<Ok>
BinASTParser::readMaybeString(MutableHandleAtom out)
{
    MOZ_ASSERT(!out);

    Maybe<Chars> string;
    MOZ_TRY(readMaybeString(string));
    if (!string) {
        return Ok();
    }

    RootedAtom atom(cx_);
    TRY_VAR(atom, AtomizeUTF8Chars(cx_, (const char*)string->begin(), string->length()));

    out.set(Move(atom));
    return Ok();
}


JS::Result<Ok>
BinASTParser::readString(Chars& result)
{
    TRY(tokenizer_->readChars(result));
    return Ok();
}

JS::Result<Ok>
BinASTParser::readMaybeString(Maybe<Chars>& out)
{
    MOZ_ASSERT(out.isNothing());
    TRY(tokenizer_->readMaybeChars(out));
    return Ok();
}

JS::Result<double>
BinASTParser::readNumber()
{
    double result;
    TRY(tokenizer_->readDouble(result));

    return result;
}

JS::Result<bool>
BinASTParser::readBool()
{
    bool result;
    TRY(tokenizer_->readBool(result));

    return result;
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseMissingVariableInAssertedScope(JSAtom* name)
{
    // For the moment, we don't trust inputs sufficiently to put the name
    // in an error message.
    return raiseError("Missing variable in AssertedScope");
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseMissingDirectEvalInAssertedScope()
{
    return raiseError("Direct call to `eval` was not declared in AssertedScope");
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseInvalidKind(const char* superKind, const BinKind kind)
{
    Sprinter out(cx_);
    TRY(out.init());
    TRY(out.printf("In %s, invalid kind %s", superKind, describeBinKind(kind)));
    return raiseError(out.string());
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseInvalidField(const char* kind, const BinField field)
{
    Sprinter out(cx_);
    TRY(out.init());
    TRY(out.printf("In %s, invalid field '%s'", kind, describeBinField(field)));
    return raiseError(out.string());
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseInvalidNumberOfFields(const BinKind kind, const uint32_t expected, const uint32_t got)
{
    Sprinter out(cx_);
    TRY(out.init());
    TRY(out.printf("In %s, invalid number of fields: expected %u, got %u",
        describeBinKind(kind), expected, got));
    return raiseError(out.string());
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseInvalidEnum(const char* kind, const Chars& value)
{
    // We don't trust the actual chars of `value` to be properly formatted anything, so let's not use
    // them anywhere.
    return raiseError("Invalid enum");
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseMissingField(const char* kind, const BinField field)
{
    Sprinter out(cx_);
    TRY(out.init());
    TRY(out.printf("In %s, missing field '%s'", kind, describeBinField(field)));

    return raiseError(out.string());
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseEmpty(const char* description)
{
    Sprinter out(cx_);
    TRY(out.init());
    TRY(out.printf("Empty %s", description));

    return raiseError(out.string());
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseOOM()
{
    ReportOutOfMemory(cx_);
    return cx_->alreadyReportedError();
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseError(BinKind kind, const char* description)
{
    Sprinter out(cx_);
    TRY(out.init());
    TRY(out.printf("In %s, ", description));
    MOZ_ALWAYS_FALSE(tokenizer_->raiseError(out.string()));

    return cx_->alreadyReportedError();
}

mozilla::GenericErrorResult<JS::Error&>
BinASTParser::raiseError(const char* description)
{
    MOZ_ALWAYS_FALSE(tokenizer_->raiseError(description));
    return cx_->alreadyReportedError();
}

void
BinASTParser::poison()
{
    tokenizer_.reset();
}

void
BinASTParser::reportErrorNoOffsetVA(unsigned errorNumber, va_list args)
{
    ErrorMetadata metadata;
    metadata.filename = getFilename();
    metadata.lineNumber = 0;
    metadata.columnNumber = offset();
    ReportCompileError(cx_, Move(metadata), nullptr, JSREPORT_ERROR, errorNumber, args);
}

bool
BinASTParser::hasUsedName(HandlePropertyName name)
{
    if (UsedNamePtr p = usedNames_.lookup(name))
        return p->value().isUsedInScript(parseContext_->scriptId());

    return false;
}

void
TraceBinParser(JSTracer* trc, AutoGCRooter* parser)
{
    static_cast<BinASTParser*>(parser)->trace(trc);
}

} // namespace frontend
} // namespace js


// #undef everything, to avoid collisions with unified builds.

#undef TRY
#undef TRY_VAR
#undef TRY_DECL
#undef TRY_EMPL
#undef MOZ_TRY_EMPLACE
