/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/BinASTParserPerTokenizer.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/Maybe.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Vector.h"

#include <utility>

#include "frontend/BinAST-macros.h"
#include "frontend/BinASTParser.h"
#include "frontend/BinASTTokenReaderContext.h"
#include "frontend/BinASTTokenReaderMultipart.h"
#include "frontend/FullParseHandler.h"
#include "frontend/ParseNode.h"
#include "frontend/Parser.h"
#include "frontend/SharedContext.h"
#include "js/Result.h"
#include "vm/RegExpObject.h"

#include "frontend/ParseContext-inl.h"
#include "frontend/SharedContext-inl.h"
#include "vm/JSContext-inl.h"

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
// Normally, this implementation validates all properties of the AST *except*
// the order of fields, which is partially constrained by the AST spec (e.g. in
// a block, field `scope` must appear before field `body`, etc.).
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

namespace js::frontend {

using UsedNamePtr = UsedNameTracker::UsedNameMap::Ptr;

// ------------- Toplevel constructions

template <typename Tok>
BinASTParserPerTokenizer<Tok>::BinASTParserPerTokenizer(
    JSContext* cx, CompilationInfo& compilationInfo,
    const JS::ReadOnlyCompileOptions& options,
    HandleScriptSourceObject sourceObject,
    Handle<BaseScript*> lazyScript /* = nullptr */)
    : BinASTParserBase(cx, compilationInfo, sourceObject),
      options_(options),
      lazyScript_(cx, lazyScript),
      handler_(cx, compilationInfo.allocScope.alloc(), nullptr,
               SourceKind::Binary),
      variableDeclarationKind_(VariableDeclarationKind::Var),
      treeHolder_(cx) {
  MOZ_ASSERT_IF(lazyScript_, lazyScript_->isBinAST());
}

template <typename Tok>
JS::Result<ParseNode*> BinASTParserPerTokenizer<Tok>::parse(
    GlobalSharedContext* globalsc, const Vector<uint8_t>& data,
    BinASTSourceMetadata** metadataPtr) {
  return parse(globalsc, data.begin(), data.length(), metadataPtr);
}

template <typename Tok>
JS::Result<ParseNode*> BinASTParserPerTokenizer<Tok>::parse(
    GlobalSharedContext* globalsc, const uint8_t* start, const size_t length,
    BinASTSourceMetadata** metadataPtr) {
  auto result = parseAux(globalsc, start, length, metadataPtr);
  poison();  // Make sure that the parser is never used again accidentally.
  return result;
}

template <typename Tok>
JS::Result<ParseNode*> BinASTParserPerTokenizer<Tok>::parseAux(
    GlobalSharedContext* globalsc, const uint8_t* start, const size_t length,
    BinASTSourceMetadata** metadataPtr) {
  MOZ_ASSERT(globalsc);

  tokenizer_.emplace(cx_, this, start, length);

  BinASTParseContext globalpc(cx_, this, globalsc,
                              /* newDirectives = */ nullptr);
  if (MOZ_UNLIKELY(!globalpc.init())) {
    return cx_->alreadyReportedError();
  }

  ParseContext::VarScope varScope(cx_, &globalpc, usedNames_);
  if (MOZ_UNLIKELY(!varScope.init(&globalpc))) {
    return cx_->alreadyReportedError();
  }

  MOZ_TRY(tokenizer_->readHeader());

  ParseNode* result(nullptr);
  const auto topContext = RootContext();
  MOZ_TRY_VAR(result, asFinalParser()->parseProgram(topContext));

  MOZ_TRY(tokenizer_->readTreeFooter());

  mozilla::Maybe<GlobalScope::Data*> bindings =
      NewGlobalScopeData(cx_, varScope, alloc_, pc_);
  if (MOZ_UNLIKELY(!bindings)) {
    return cx_->alreadyReportedError();
  }
  globalsc->bindings = *bindings;

  if (metadataPtr) {
    *metadataPtr = tokenizer_->takeMetadata();
  }

  return result;  // Magic conversion to Ok.
}

template <typename Tok>
JS::Result<FunctionNode*> BinASTParserPerTokenizer<Tok>::parseLazyFunction(
    ScriptSource* scriptSource, const size_t firstOffset) {
  MOZ_ASSERT(lazyScript_);
  MOZ_ASSERT(scriptSource->length() > firstOffset);

  tokenizer_.emplace(cx_, this, scriptSource->binASTSource(),
                     scriptSource->length());

  MOZ_TRY(tokenizer_->initFromScriptSource(scriptSource));

  tokenizer_->seek(firstOffset);

  // For now, only function declarations and function expression are supported.
  RootedFunction func(cx_, lazyScript_->function());
  bool isExpr = func->isLambda();
  MOZ_ASSERT(func->kind() == FunctionFlags::FunctionKind::NormalFunction);

  // Poison the tokenizer when we leave to ensure that it's not used again by
  // accident.
  auto onExit = mozilla::MakeScopeExit([&]() { poison(); });

  // TODO: This should be actually shared with the auto-generated version.

  auto syntaxKind =
      isExpr ? FunctionSyntaxKind::Expression : FunctionSyntaxKind::Statement;
  BINJS_MOZ_TRY_DECL(
      funbox, buildFunctionBox(lazyScript_->generatorKind(),
                               lazyScript_->asyncKind(), syntaxKind, nullptr));

  // Push a new ParseContext. It will be used to parse `scope`, the arguments,
  // the function.
  BinASTParseContext funpc(cx_, this, funbox, /* newDirectives = */ nullptr);
  BINJS_TRY(funpc.init());
  pc_->functionScope().useAsVarScope(pc_);
  MOZ_ASSERT(pc_->isFunctionBox());

  ParseContext::Scope lexicalScope(cx_, pc_, usedNames_);
  BINJS_TRY(lexicalScope.init(pc_));
  ListNode* params;
  ListNode* tmpBody;
  auto parseFunc = isExpr ? &FinalParser::parseFunctionExpressionContents
                          : &FinalParser::parseFunctionOrMethodContents;

  // Inject a toplevel context (i.e. no parent) to parse the lazy content.
  // In the future, we may move this to a more specific context.
  const auto context = FieldOrRootContext(RootContext());
  MOZ_TRY(
      (asFinalParser()->*parseFunc)(func->nargs(), &params, &tmpBody, context));

  uint32_t nargs = params->count();

  BINJS_TRY_DECL(lexicalScopeData,
                 NewLexicalScopeData(cx_, lexicalScope, alloc_, pc_));
  BINJS_TRY_DECL(body, handler_.newLexicalScope(*lexicalScopeData, tmpBody));

  BINJS_MOZ_TRY_DECL(result,
                     makeEmptyFunctionNode(firstOffset, syntaxKind, funbox));
  MOZ_TRY(setFunctionParametersAndBody(result, params, body));
  MOZ_TRY(finishEagerFunction(funbox, nargs));
  return result;
}

template <typename Tok>
void BinASTParserPerTokenizer<Tok>::forceStrictIfNecessary(
    SharedContext* sc, ListNode* directives) {
  JSAtom* useStrict = cx_->names().useStrict;

  for (const ParseNode* directive : directives->contents()) {
    if (directive->as<NameNode>().atom() == useStrict) {
      sc->setStrictScript();
      break;
    }
  }
}

template <typename Tok>
JS::Result<FunctionBox*> BinASTParserPerTokenizer<Tok>::buildFunctionBox(
    GeneratorKind generatorKind, FunctionAsyncKind functionAsyncKind,
    FunctionSyntaxKind syntax, ParseNode* name) {
  MOZ_ASSERT_IF(!pc_, lazyScript_);

  RootedAtom atom(cx_);

  // Name might be any of {Identifier,ComputedPropertyName,LiteralPropertyName}
  if (name && name->is<NameNode>()) {
    atom = name->as<NameNode>().atom();
  }

  if (pc_ && syntax == FunctionSyntaxKind::Statement) {
    auto ptr = pc_->varScope().lookupDeclaredName(atom);
    if (MOZ_UNLIKELY(!ptr)) {
      return raiseError(
          "FunctionDeclaration without corresponding AssertedDeclaredName.");
    }

    DeclarationKind declaredKind = ptr->value()->kind();
    if (DeclarationKindIsVar(declaredKind)) {
      if (MOZ_UNLIKELY(!pc_->atBodyLevel())) {
        return raiseError(
            "body-level FunctionDeclaration inside non-body-level context.");
      }
      RedeclareVar(ptr, DeclarationKind::BodyLevelFunction);
    }
  }

  // Allocate the function before walking down the tree.
  RootedFunction fun(cx_);
  if (pc_) {
    Rooted<FunctionCreationData> fcd(
        cx_,
        FunctionCreationData(atom, syntax, generatorKind, functionAsyncKind));
    BINJS_TRY_VAR(fun, AllocNewFunction(cx_, fcd));
    MOZ_ASSERT(fun->explicitName() == atom);
  } else {
    BINJS_TRY_VAR(fun, lazyScript_->function());
  }

  mozilla::Maybe<Directives> directives;
  if (pc_) {
    directives.emplace(pc_);
  } else {
    directives.emplace(lazyScript_->strict());
  }

  size_t index = this->getCompilationInfo().funcData.length();
  if (!this->getCompilationInfo().funcData.emplaceBack(
          mozilla::AsVariant(fun.get()))) {
    return nullptr;
  }

  // This extent will be further filled in during parse.
  SourceExtent extent;
  auto* funbox = alloc_.new_<FunctionBox>(
      cx_, traceListHead_, extent, getCompilationInfo(), *directives,
      generatorKind, functionAsyncKind, fun->displayAtom(), fun->flags(),
      index);
  if (MOZ_UNLIKELY(!funbox)) {
    return raiseOOM();
  }

  traceListHead_ = funbox;
  if (pc_) {
    funbox->initWithEnclosingParseContext(pc_, fun, syntax);
  } else {
    funbox->initFromLazyFunction(fun);
    funbox->initWithEnclosingScope(fun);
  }
  return funbox;
}

template <typename Tok>
JS::Result<FunctionNode*> BinASTParserPerTokenizer<Tok>::makeEmptyFunctionNode(
    const size_t start, const FunctionSyntaxKind syntaxKind,
    FunctionBox* funbox) {
  // Lazy script compilation requires basically none of the fields filled out.
  TokenPos pos = tokenizer_->pos(start);

  BINJS_TRY_DECL(result, handler_.newFunction(syntaxKind, pos));

  funbox->setStart(start, 0, pos.begin);
  funbox->setEnd(pos.end);
  handler_.setFunctionBox(result, funbox);

  return result;
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::setFunctionParametersAndBody(
    FunctionNode* fun, ListNode* params, ParseNode* body) {
  params->appendWithoutOrderAssumption(body);
  handler_.setFunctionFormalParametersAndBody(fun, params);
  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::finishEagerFunction(
    FunctionBox* funbox, uint32_t nargs) {
  // If this is delazification of a canonical function, the JSFunction object
  // already has correct `nargs_`.
  if (!lazyScript_ || lazyScript_->function() != funbox->function()) {
    funbox->setArgCount(nargs);
    funbox->synchronizeArgCount();
  } else {
    MOZ_ASSERT(funbox->function()->nargs() == nargs);
    funbox->setArgCount(nargs);
  }

  // BCE will need to generate bytecode for this.
  funbox->emitBytecode = true;

  const bool canSkipLazyClosedOverBindings = false;
  BINJS_TRY(pc_->declareFunctionArgumentsObject(usedNames_,
                                                canSkipLazyClosedOverBindings));
  BINJS_TRY(
      pc_->declareFunctionThis(usedNames_, canSkipLazyClosedOverBindings));

  // Check all our bindings after maybe adding function metavars.
  MOZ_TRY(checkFunctionClosedVars());

  BINJS_TRY_DECL(bindings,
                 NewFunctionScopeData(cx_, pc_->functionScope(),
                                      /* hasParameterExprs = */ false,
                                      IsFieldInitializer::No, alloc_, pc_));

  funbox->functionScopeBindings().set(*bindings);

  // See: JSFunction::needsCallObject()
  if (FunctionScopeHasClosedOverBindings(pc_) ||
      funbox->needsCallObjectRegardlessOfBindings()) {
    funbox->setNeedsFunctionEnvironmentObjects();
  }

  if (funbox->isNamedLambda()) {
    BINJS_TRY_DECL(
        recursiveBinding,
        NewLexicalScopeData(cx_, pc_->namedLambdaScope(), alloc_, pc_));

    funbox->namedLambdaBindings().set(*recursiveBinding);

    // See: JSFunction::needsNamedLambdaEnvironment()
    if (LexicalScopeHasClosedOverBindings(pc_, pc_->namedLambdaScope())) {
      funbox->setNeedsFunctionEnvironmentObjects();
    }
  }

  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::finishLazyFunction(
    FunctionBox* funbox, uint32_t nargs, size_t start, size_t end) {
  RootedFunction fun(cx_, funbox->function());

  funbox->setArgCount(nargs);
  funbox->synchronizeArgCount();

  using ImmutableFlags = ImmutableScriptFlagsEnum;
  ImmutableScriptFlags immutableFlags = funbox->immutableFlags();

  // Compute the flags that frontend doesn't directly compute.
  immutableFlags.setFlag(ImmutableFlags::ForceStrict,
                         options().forceStrictMode());
  immutableFlags.setFlag(ImmutableFlags::HasMappedArgsObj,
                         funbox->hasMappedArgsObj());

  SourceExtent extent(start, end, start, end,
                      /* lineno = */ 0, start);

  // NOTE: Lazy functions generated by BinAST do not track inner functions or
  //       closed-over-binding data on the VM. Instead they use out-of-band data
  //       from the BinAST stream during delazification.
  BINJS_TRY_DECL(
      lazy, BaseScript::CreateRawLazy(cx_, /* ngcthings = */ 0, fun,
                                      sourceObject_, extent, immutableFlags));

  MOZ_ASSERT(lazy->isBinAST());
  funbox->initLazyScript(lazy);

  MOZ_TRY(tokenizer_->registerLazyScript(lazy));

  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::addScopeName(
    AssertedScopeKind scopeKind, HandleAtom name, ParseContext::Scope* scope,
    DeclarationKind declKind, bool isCaptured, bool allowDuplicateName) {
  auto ptr = scope->lookupDeclaredNameForAdd(name);
  if (ptr) {
    if (allowDuplicateName) {
      return Ok();
    }
    return raiseError("Variable redeclaration");
  }

  BINJS_TRY(scope->addDeclaredName(pc_, ptr, name.get(), declKind,
                                   tokenizer_->offset()));

  if (isCaptured) {
    auto declaredPtr = scope->lookupDeclaredName(name);
    MOZ_ASSERT(declaredPtr);
    declaredPtr->value()->setClosedOver();
  }

  return Ok();
}

template <typename Tok>
void BinASTParserPerTokenizer<Tok>::captureFunctionName() {
  MOZ_ASSERT(pc_->isFunctionBox());
  MOZ_ASSERT(pc_->functionBox()->isNamedLambda());

  RootedAtom funName(cx_, pc_->functionBox()->explicitName());
  MOZ_ASSERT(funName);

  auto ptr = pc_->namedLambdaScope().lookupDeclaredName(funName);
  MOZ_ASSERT(ptr);
  ptr->value()->setClosedOver();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::getDeclaredScope(
    AssertedScopeKind scopeKind, AssertedDeclaredKind kind,
    ParseContext::Scope*& scope, DeclarationKind& declKind) {
  MOZ_ASSERT(scopeKind == AssertedScopeKind::Block ||
             scopeKind == AssertedScopeKind::Global ||
             scopeKind == AssertedScopeKind::Var);
  switch (kind) {
    case AssertedDeclaredKind::Var:
      if (MOZ_UNLIKELY(scopeKind == AssertedScopeKind::Block)) {
        return raiseError("AssertedBlockScope cannot contain 'var' binding");
      }
      declKind = DeclarationKind::Var;
      scope = &pc_->varScope();
      break;
    case AssertedDeclaredKind::NonConstLexical:
      declKind = DeclarationKind::Let;
      scope = pc_->innermostScope();
      break;
    case AssertedDeclaredKind::ConstLexical:
      declKind = DeclarationKind::Const;
      scope = pc_->innermostScope();
      break;
  }

  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::getBoundScope(
    AssertedScopeKind scopeKind, ParseContext::Scope*& scope,
    DeclarationKind& declKind) {
  MOZ_ASSERT(scopeKind == AssertedScopeKind::Catch ||
             scopeKind == AssertedScopeKind::Parameter);
  switch (scopeKind) {
    case AssertedScopeKind::Catch:
      declKind = DeclarationKind::CatchParameter;
      scope = pc_->innermostScope();
      break;
    case AssertedScopeKind::Parameter:
      MOZ_ASSERT(pc_->isFunctionBox());
      declKind = DeclarationKind::PositionalFormalParameter;
      scope = &pc_->functionScope();
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected AssertedScopeKind");
      break;
  }

  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::checkBinding(JSAtom* name) {
  // Check that the variable appears in the corresponding scope.
  ParseContext::Scope& scope =
      variableDeclarationKind_ == VariableDeclarationKind::Var
          ? pc_->varScope()
          : *pc_->innermostScope();

  auto ptr = scope.lookupDeclaredName(name->asPropertyName());
  if (MOZ_UNLIKELY(!ptr)) {
    return raiseMissingVariableInAssertedScope(name);
  }

  return Ok();
}

// Binary AST (revision 8eab67e0c434929a66ff6abe99ff790bca087dda)
// 3.1.5 CheckPositionalParameterIndices.
template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::checkPositionalParameterIndices(
    Handle<GCVector<JSAtom*>> positionalParams, ListNode* params) {
  // positionalParams should have the corresponding entry up to the last
  // positional parameter.

  // `positionalParams` corresponds to `expectedParams` parameter in the spec.
  // `params` corresponds to `parseTree` in 3.1.9 CheckAssertedScope, and
  // `positionalParamNames` parameter

  // Steps 1-3.
  // PositionalParameterNames (3.1.9 CheckAssertedScope step 5.d) and
  // CreatePositionalParameterIndices (3.1.5 CheckPositionalParameterIndices
  // step 1) are done implicitly.
  uint32_t i = 0;
  const bool hasRest = pc_->functionBox()->hasRest();
  for (ParseNode* param : params->contents()) {
    if (param->isKind(ParseNodeKind::AssignExpr)) {
      param = param->as<AssignmentNode>().left();
    }

    // At this point, function body is not part of params list.
    const bool isRest = hasRest && !param->pn_next;
    if (isRest) {
      // Rest parameter

      // Step 3.
      if (i >= positionalParams.get().length()) {
        continue;
      }

      if (MOZ_UNLIKELY(positionalParams.get()[i])) {
        return raiseError(
            "Expected positional parameter per "
            "AssertedParameterScope.paramNames, got rest parameter");
      }
    } else if (param->isKind(ParseNodeKind::Name)) {
      // Simple or default parameter.

      // Step 2.a.
      if (MOZ_UNLIKELY(i >= positionalParams.get().length())) {
        return raiseError(
            "AssertedParameterScope.paramNames doesn't have corresponding "
            "entry to positional parameter");
      }

      JSAtom* name = positionalParams.get()[i];
      if (MOZ_UNLIKELY(!name)) {
        // Step 2.a.ii.1.
        return raiseError(
            "Expected destructuring/rest parameter per "
            "AssertedParameterScope.paramNames, got positional parameter");
      }

      // Step 2.a.i.
      if (MOZ_UNLIKELY(param->as<NameNode>().name() != name)) {
        // Step 2.a.ii.1.
        return raiseError(
            "Name mismatch between AssertedPositionalParameterName in "
            "AssertedParameterScope.paramNames and actual parameter");
      }

      // Step 2.a.i.1.
      // Implicitly done.
    } else {
      // Destructuring parameter.

      MOZ_ASSERT(param->isKind(ParseNodeKind::ObjectExpr) ||
                 param->isKind(ParseNodeKind::ArrayExpr));

      // Step 3.
      if (i >= positionalParams.get().length()) {
        continue;
      }

      if (MOZ_UNLIKELY(positionalParams.get()[i])) {
        return raiseError(
            "Expected positional parameter per "
            "AssertedParameterScope.paramNames, got destructuring parameter");
      }
    }

    i++;
  }

  // Step 3.
  if (MOZ_UNLIKELY(positionalParams.get().length() > params->count())) {
    // `positionalParams` has unchecked entries.
    return raiseError(
        "AssertedParameterScope.paramNames has unmatching items than the "
        "actual parameters");
  }

  return Ok();
}

// Binary AST (revision 8eab67e0c434929a66ff6abe99ff790bca087dda)
// 3.1.13 CheckFunctionLength.
template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::checkFunctionLength(
    uint32_t expectedLength) {
  if (MOZ_UNLIKELY(pc_->functionBox()->length != expectedLength)) {
    return raiseError("Function length does't match");
  }
  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::checkClosedVars(
    ParseContext::Scope& scope) {
  for (ParseContext::Scope::BindingIter bi = scope.bindings(pc_); bi; bi++) {
    if (UsedNamePtr p = usedNames_.lookup(bi.name())) {
      bool closedOver;
      p->value().noteBoundInScope(pc_->scriptId(), scope.id(), &closedOver);
      if (MOZ_UNLIKELY(closedOver && !bi.closedOver())) {
        return raiseInvalidClosedVar(bi.name());
      }
    }
  }

  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::checkFunctionClosedVars() {
  MOZ_ASSERT(pc_->isFunctionBox());

  MOZ_TRY(checkClosedVars(*pc_->innermostScope()));
  MOZ_TRY(checkClosedVars(pc_->functionScope()));
  if (pc_->functionBox()->isNamedLambda()) {
    MOZ_TRY(checkClosedVars(pc_->namedLambdaScope()));
  }

  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::prependDirectivesToBody(
    ListNode* body, ListNode* directives) {
  if (!directives) {
    return Ok();
  }

  if (directives->empty()) {
    return Ok();
  }

  MOZ_TRY(prependDirectivesImpl(body, directives->head()));

  return Ok();
}

template <typename Tok>
JS::Result<Ok> BinASTParserPerTokenizer<Tok>::prependDirectivesImpl(
    ListNode* body, ParseNode* directive) {
  BINJS_TRY(CheckRecursionLimit(cx_));

  // Prepend in the reverse order by using stack, so that the directives are
  // prepended in the original order.
  if (ParseNode* next = directive->pn_next) {
    MOZ_TRY(prependDirectivesImpl(body, next));
  }

  BINJS_TRY_DECL(statement,
                 handler_.newExprStatement(directive, directive->pn_pos.end));
  body->prependAndUpdatePos(statement);

  return Ok();
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseInvalidClosedVar(JSAtom* name) {
  return raiseError("Captured variable was not declared as captured");
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseMissingVariableInAssertedScope(
    JSAtom* name) {
  // For the moment, we don't trust inputs sufficiently to put the name
  // in an error message.
  return raiseError("Missing variable in AssertedScope");
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseMissingDirectEvalInAssertedScope() {
  return raiseError("Direct call to `eval` was not declared in AssertedScope");
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseInvalidKind(const char* superKind,
                                                const BinASTKind kind) {
  Sprinter out(cx_);
  BINJS_TRY(out.init());
  BINJS_TRY(out.printf("In %s, invalid kind %s", superKind,
                       describeBinASTKind(kind)));
  return raiseError(out.string());
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseInvalidVariant(const char* kind,
                                                   const BinASTVariant value) {
  Sprinter out(cx_);
  BINJS_TRY(out.init());
  BINJS_TRY(out.printf("In %s, invalid variant '%s'", kind,
                       describeBinASTVariant(value)));

  return raiseError(out.string());
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseMissingField(const char* kind,
                                                 const BinASTField field) {
  Sprinter out(cx_);
  BINJS_TRY(out.init());
  BINJS_TRY(out.printf("In %s, missing field '%s'", kind,
                       describeBinASTField(field)));

  return raiseError(out.string());
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseEmpty(const char* description) {
  Sprinter out(cx_);
  BINJS_TRY(out.init());
  BINJS_TRY(out.printf("Empty %s", description));

  return raiseError(out.string());
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseOOM() {
  return tokenizer_->raiseOOM();
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseError(BinASTKind kind,
                                          const char* description) {
  Sprinter out(cx_);
  BINJS_TRY(out.init());
  BINJS_TRY(out.printf("In %s, %s", describeBinASTKind(kind), description));
  return tokenizer_->raiseError(out.string());
}

template <typename Tok>
mozilla::GenericErrorResult<JS::Error&>
BinASTParserPerTokenizer<Tok>::raiseError(const char* description) {
  return tokenizer_->raiseError(description);
}

template <typename Tok>
void BinASTParserPerTokenizer<Tok>::poison() {
  tokenizer_.reset();
}

template <typename Tok>
bool BinASTParserPerTokenizer<Tok>::computeErrorMetadata(
    ErrorMetadata* err, const ErrorOffset& errorOffset) {
  err->filename = getFilename();
  err->lineNumber = 0;
  if (errorOffset.is<uint32_t>()) {
    err->columnNumber = errorOffset.as<uint32_t>();
  } else if (errorOffset.is<Current>()) {
    err->columnNumber = offset();
  } else {
    errorOffset.is<NoOffset>();
    err->columnNumber = 0;
  }

  err->isMuted = options().mutedErrors();
  return true;
}

void TraceBinASTParser(JSTracer* trc, JS::AutoGCRooter* parser) {
  static_cast<BinASTParserBase*>(parser)->trace(trc);
}

template <typename Tok>
void BinASTParserPerTokenizer<Tok>::doTrace(JSTracer* trc) {
  if (tokenizer_) {
    tokenizer_->traceMetadata(trc);
  }
}

template <typename Tok>
inline typename BinASTParserPerTokenizer<Tok>::FinalParser*
BinASTParserPerTokenizer<Tok>::asFinalParser() {
  // Same as GeneralParser::asFinalParser, verify the inheritance to
  // make sure the static downcast works.
  static_assert(std::is_base_of_v<BinASTParserPerTokenizer<Tok>, FinalParser>,
                "inheritance relationship required by the static_cast<> below");

  return static_cast<FinalParser*>(this);
}

template <typename Tok>
inline const typename BinASTParserPerTokenizer<Tok>::FinalParser*
BinASTParserPerTokenizer<Tok>::asFinalParser() const {
  static_assert(std::is_base_of_v<BinASTParserPerTokenizer<Tok>, FinalParser>,
                "inheritance relationship required by the static_cast<> below");

  return static_cast<const FinalParser*>(this);
}

// Force class instantiation.
// This ensures that the symbols are built, without having to export all our
// code (and its baggage of #include and macros) in the header.
template class BinASTParserPerTokenizer<BinASTTokenReaderContext>;
template class BinASTParserPerTokenizer<BinASTTokenReaderMultipart>;

}  // namespace js::frontend
