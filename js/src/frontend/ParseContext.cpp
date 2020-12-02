/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParseContext-inl.h"

#include "js/friend/ErrorMessages.h"  // JSMSG_*

#include "vm/EnvironmentObject-inl.h"

using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

namespace js {
namespace frontend {

using AddDeclaredNamePtr = ParseContext::Scope::AddDeclaredNamePtr;
using DeclaredNamePtr = ParseContext::Scope::DeclaredNamePtr;

const char* DeclarationKindString(DeclarationKind kind) {
  switch (kind) {
    case DeclarationKind::PositionalFormalParameter:
    case DeclarationKind::FormalParameter:
      return "formal parameter";
    case DeclarationKind::CoverArrowParameter:
      return "cover arrow parameter";
    case DeclarationKind::Var:
      return "var";
    case DeclarationKind::Let:
      return "let";
    case DeclarationKind::Const:
      return "const";
    case DeclarationKind::Class:
      return "class";
    case DeclarationKind::Import:
      return "import";
    case DeclarationKind::BodyLevelFunction:
    case DeclarationKind::ModuleBodyLevelFunction:
    case DeclarationKind::LexicalFunction:
    case DeclarationKind::SloppyLexicalFunction:
      return "function";
    case DeclarationKind::VarForAnnexBLexicalFunction:
      return "annex b var";
    case DeclarationKind::SimpleCatchParameter:
    case DeclarationKind::CatchParameter:
      return "catch parameter";
    case DeclarationKind::PrivateName:
      return "private name";
  }

  MOZ_CRASH("Bad DeclarationKind");
}

bool DeclarationKindIsVar(DeclarationKind kind) {
  return kind == DeclarationKind::Var ||
         kind == DeclarationKind::BodyLevelFunction ||
         kind == DeclarationKind::VarForAnnexBLexicalFunction;
}

bool DeclarationKindIsParameter(DeclarationKind kind) {
  return kind == DeclarationKind::PositionalFormalParameter ||
         kind == DeclarationKind::FormalParameter;
}

bool UsedNameTracker::noteUse(JSContext* cx, const ParserAtom* name,
                              NameVisibility visibility, uint32_t scriptId,
                              uint32_t scopeId,
                              mozilla::Maybe<TokenPos> tokenPosition) {
  if (UsedNameMap::AddPtr p = map_.lookupForAdd(name)) {
    if (!p->value().noteUsedInScope(scriptId, scopeId)) {
      return false;
    }
  } else {
    // We need a token position precisely where we have private visibility.
    MOZ_ASSERT(tokenPosition.isSome() ==
               (visibility == NameVisibility::Private));

    if (visibility == NameVisibility::Private) {
      // We have seen at least one private name
      hasPrivateNames_ = true;
    }

    UsedNameInfo info(cx, visibility, tokenPosition);

    if (!info.noteUsedInScope(scriptId, scopeId)) {
      return false;
    }
    if (!map_.add(p, name, std::move(info))) {
      return false;
    }
  }

  return true;
}

bool UsedNameTracker::getUnboundPrivateNames(
    Vector<UnboundPrivateName, 8>& unboundPrivateNames) {
  // We never saw any private names, so can just return early
  if (!hasPrivateNames_) {
    return true;
  }

  for (auto iter = map_.iter(); !iter.done(); iter.next()) {
    // Don't care about public;
    if (iter.get().value().isPublic()) {
      continue;
    }

    // empty list means all bound
    if (iter.get().value().empty()) {
      continue;
    }

    if (!unboundPrivateNames.emplaceBack(iter.get().key(),
                                         *iter.get().value().pos())) {
      return false;
    }
  }

  // Return a sorted list in ascendng order of position.
  auto comparePosition = [](const auto& a, const auto& b) {
    return a.position < b.position;
  };
  std::sort(unboundPrivateNames.begin(), unboundPrivateNames.end(),
            comparePosition);

  return true;
}

bool UsedNameTracker::hasUnboundPrivateNames(
    JSContext* cx, mozilla::Maybe<UnboundPrivateName>& maybeUnboundName) {
  // We never saw any private names, so can just return early
  if (!hasPrivateNames_) {
    return true;
  }

  Vector<UnboundPrivateName, 8> unboundPrivateNames(cx);
  if (!getUnboundPrivateNames(unboundPrivateNames)) {
    return false;
  }

  if (unboundPrivateNames.empty()) {
    return true;
  }

  // GetUnboundPrivateNames returns the list sorted.
  maybeUnboundName.emplace(unboundPrivateNames[0]);
  return true;
}

void UsedNameTracker::UsedNameInfo::resetToScope(uint32_t scriptId,
                                                 uint32_t scopeId) {
  while (!uses_.empty()) {
    Use& innermost = uses_.back();
    if (innermost.scopeId < scopeId) {
      break;
    }
    MOZ_ASSERT(innermost.scriptId >= scriptId);
    uses_.popBack();
  }
}

void UsedNameTracker::rewind(RewindToken token) {
  scriptCounter_ = token.scriptId;
  scopeCounter_ = token.scopeId;

  for (UsedNameMap::Range r = map_.all(); !r.empty(); r.popFront()) {
    r.front().value().resetToScope(token.scriptId, token.scopeId);
  }
}

void ParseContext::Scope::dump(ParseContext* pc) {
  JSContext* cx = pc->sc()->cx_;

  fprintf(stdout, "ParseScope %p", this);

  fprintf(stdout, "\n  decls:\n");
  for (DeclaredNameMap::Range r = declared_->all(); !r.empty(); r.popFront()) {
    UniqueChars bytes = QuoteString(cx, r.front().key());
    if (!bytes) {
      return;
    }
    DeclaredNameInfo& info = r.front().value().wrapped;
    fprintf(stdout, "    %s %s%s\n", DeclarationKindString(info.kind()),
            bytes.get(), info.closedOver() ? " (closed over)" : "");
  }

  fprintf(stdout, "\n");
}

bool ParseContext::Scope::addPossibleAnnexBFunctionBox(ParseContext* pc,
                                                       FunctionBox* funbox) {
  if (!possibleAnnexBFunctionBoxes_) {
    if (!possibleAnnexBFunctionBoxes_.acquire(pc->sc()->cx_)) {
      return false;
    }
  }

  return maybeReportOOM(pc, possibleAnnexBFunctionBoxes_->append(funbox));
}

bool ParseContext::Scope::propagateAndMarkAnnexBFunctionBoxes(
    ParseContext* pc) {
  // Strict mode doesn't have wack Annex B function semantics.
  if (pc->sc()->strict() || !possibleAnnexBFunctionBoxes_ ||
      possibleAnnexBFunctionBoxes_->empty()) {
    return true;
  }

  if (this == &pc->varScope()) {
    // Base case: actually declare the Annex B vars and mark applicable
    // function boxes as Annex B.
    Maybe<DeclarationKind> redeclaredKind;
    uint32_t unused;
    for (FunctionBox* funbox : *possibleAnnexBFunctionBoxes_) {
      bool annexBApplies;
      if (!pc->computeAnnexBAppliesToLexicalFunctionInInnermostScope(
              funbox, &annexBApplies)) {
        return false;
      }
      if (annexBApplies) {
        const ParserName* name = funbox->explicitName()->asName();
        if (!pc->tryDeclareVar(
                name, DeclarationKind::VarForAnnexBLexicalFunction,
                DeclaredNameInfo::npos, &redeclaredKind, &unused)) {
          return false;
        }

        MOZ_ASSERT(!redeclaredKind);
        funbox->isAnnexB = true;
      }
    }
  } else {
    // Inner scope case: propagate still applicable function boxes to the
    // enclosing scope.
    for (FunctionBox* funbox : *possibleAnnexBFunctionBoxes_) {
      bool annexBApplies;
      if (!pc->computeAnnexBAppliesToLexicalFunctionInInnermostScope(
              funbox, &annexBApplies)) {
        return false;
      }
      if (annexBApplies) {
        if (!enclosing()->addPossibleAnnexBFunctionBox(pc, funbox)) {
          return false;
        }
      }
    }
  }

  return true;
}

static bool DeclarationKindIsCatchParameter(DeclarationKind kind) {
  return kind == DeclarationKind::SimpleCatchParameter ||
         kind == DeclarationKind::CatchParameter;
}

bool ParseContext::Scope::addCatchParameters(ParseContext* pc,
                                             Scope& catchParamScope) {
  if (pc->useAsmOrInsideUseAsm()) {
    return true;
  }

  for (DeclaredNameMap::Range r = catchParamScope.declared_->all(); !r.empty();
       r.popFront()) {
    DeclarationKind kind = r.front().value()->kind();
    uint32_t pos = r.front().value()->pos();
    MOZ_ASSERT(DeclarationKindIsCatchParameter(kind));
    const ParserAtom* name = r.front().key();
    AddDeclaredNamePtr p = lookupDeclaredNameForAdd(name);
    MOZ_ASSERT(!p);
    if (!addDeclaredName(pc, p, name, kind, pos)) {
      return false;
    }
  }

  return true;
}

void ParseContext::Scope::removeCatchParameters(ParseContext* pc,
                                                Scope& catchParamScope) {
  if (pc->useAsmOrInsideUseAsm()) {
    return;
  }

  for (DeclaredNameMap::Range r = catchParamScope.declared_->all(); !r.empty();
       r.popFront()) {
    DeclaredNamePtr p = declared_->lookup(r.front().key());
    MOZ_ASSERT(p);

    // This check is needed because the catch body could have declared
    // vars, which would have been added to catchParamScope.
    if (DeclarationKindIsCatchParameter(r.front().value()->kind())) {
      declared_->remove(p);
    }
  }
}

ParseContext::ParseContext(JSContext* cx, ParseContext*& parent,
                           SharedContext* sc, ErrorReporter& errorReporter,
                           CompilationState& compilationState,
                           Directives* newDirectives, bool isFull)
    : Nestable<ParseContext>(&parent),
      traceLog_(sc->cx_,
                isFull ? TraceLogger_ParsingFull : TraceLogger_ParsingSyntax,
                errorReporter),
      sc_(sc),
      errorReporter_(errorReporter),
      innermostStatement_(nullptr),
      innermostScope_(nullptr),
      varScope_(nullptr),
      positionalFormalParameterNames_(cx->frontendCollectionPool()),
      closedOverBindingsForLazy_(cx->frontendCollectionPool()),
      innerFunctionIndexesForLazy(cx),
      newDirectives(newDirectives),
      lastYieldOffset(NoYieldOffset),
      lastAwaitOffset(NoAwaitOffset),
      scriptId_(compilationState.usedNames.nextScriptId()),
      superScopeNeedsHomeObject_(false) {
  if (isFunctionBox()) {
    if (functionBox()->isNamedLambda()) {
      namedLambdaScope_.emplace(cx, parent, compilationState.usedNames);
    }
    functionScope_.emplace(cx, parent, compilationState.usedNames);
  }
}

bool ParseContext::init() {
  if (scriptId_ == UINT32_MAX) {
    errorReporter_.errorNoOffset(JSMSG_NEED_DIET, js_script_str);
    return false;
  }

  JSContext* cx = sc()->cx_;

  if (isFunctionBox()) {
    // Named lambdas always need a binding for their own name. If this
    // binding is closed over when we finish parsing the function in
    // finishFunctionScopes, the function box needs to be marked as
    // needing a dynamic DeclEnv object.
    if (functionBox()->isNamedLambda()) {
      if (!namedLambdaScope_->init(this)) {
        return false;
      }
      AddDeclaredNamePtr p = namedLambdaScope_->lookupDeclaredNameForAdd(
          functionBox()->explicitName());
      MOZ_ASSERT(!p);
      if (!namedLambdaScope_->addDeclaredName(
              this, p, functionBox()->explicitName(), DeclarationKind::Const,
              DeclaredNameInfo::npos)) {
        return false;
      }
    }

    if (!functionScope_->init(this)) {
      return false;
    }

    if (!positionalFormalParameterNames_.acquire(cx)) {
      return false;
    }
  }

  if (!closedOverBindingsForLazy_.acquire(cx)) {
    return false;
  }

  return true;
}

bool ParseContext::computeAnnexBAppliesToLexicalFunctionInInnermostScope(
    FunctionBox* funbox, bool* annexBApplies) {
  MOZ_ASSERT(!sc()->strict());

  const ParserName* name = funbox->explicitName()->asName();
  Maybe<DeclarationKind> redeclaredKind;
  if (!isVarRedeclaredInInnermostScope(
          name, DeclarationKind::VarForAnnexBLexicalFunction,
          &redeclaredKind)) {
    return false;
  }

  if (!redeclaredKind && isFunctionBox()) {
    Scope& funScope = functionScope();
    if (&funScope != &varScope()) {
      // Annex B.3.3.1 disallows redeclaring parameter names. In the
      // presence of parameter expressions, parameter names are on the
      // function scope, which encloses the var scope. This means the
      // isVarRedeclaredInInnermostScope call above would not catch this
      // case, so test it manually.
      if (DeclaredNamePtr p = funScope.lookupDeclaredName(name)) {
        DeclarationKind declaredKind = p->value()->kind();
        if (DeclarationKindIsParameter(declaredKind)) {
          redeclaredKind = Some(declaredKind);
        } else {
          MOZ_ASSERT(FunctionScope::isSpecialName(sc()->cx_, name));
        }
      }
    }
  }

  // If an early error would have occurred already, this function should not
  // exhibit Annex B.3.3 semantics.
  *annexBApplies = !redeclaredKind;
  return true;
}

bool ParseContext::isVarRedeclaredInInnermostScope(
    const ParserName* name, DeclarationKind kind,
    mozilla::Maybe<DeclarationKind>* out) {
  uint32_t unused;
  return tryDeclareVarHelper<DryRunInnermostScopeOnly>(
      name, kind, DeclaredNameInfo::npos, out, &unused);
}

bool ParseContext::isVarRedeclaredInEval(const ParserName* name,
                                         DeclarationKind kind,
                                         Maybe<DeclarationKind>* out) {
  MOZ_ASSERT(out);
  MOZ_ASSERT(DeclarationKindIsVar(kind));
  MOZ_ASSERT(sc()->isEvalContext());

  // TODO-Stencil: After scope snapshotting, this can be done away with.
  JSAtom* nameAtom =
      name->toJSAtom(sc()->cx_, sc()->compilationInfo().input.atomCache);
  if (!nameAtom) {
    return false;
  }

  // In the case of eval, we also need to check enclosing VM scopes to see
  // if the var declaration is allowed in the context.
  js::Scope* enclosingScope = sc()->compilationInfo().input.enclosingScope;
  js::Scope* varScope = EvalScope::nearestVarScopeForDirectEval(enclosingScope);
  MOZ_ASSERT(varScope);
  for (ScopeIter si(enclosingScope); si; si++) {
    for (js::BindingIter bi(si.scope()); bi; bi++) {
      if (bi.name() != nameAtom) {
        continue;
      }

      switch (bi.kind()) {
        case BindingKind::Let: {
          // Annex B.3.5 allows redeclaring simple (non-destructured)
          // catch parameters with var declarations.
          bool annexB35Allowance = si.kind() == ScopeKind::SimpleCatch;
          if (!annexB35Allowance) {
            *out = Some(ScopeKindIsCatch(si.kind())
                            ? DeclarationKind::CatchParameter
                            : DeclarationKind::Let);
            return true;
          }
          break;
        }

        case BindingKind::Const:
          *out = Some(DeclarationKind::Const);
          return true;

        case BindingKind::Import:
        case BindingKind::FormalParameter:
        case BindingKind::Var:
        case BindingKind::NamedLambdaCallee:
          break;
      }
    }

    if (si.scope() == varScope) {
      break;
    }
  }

  *out = Nothing();
  return true;
}

bool ParseContext::tryDeclareVar(const ParserName* name, DeclarationKind kind,
                                 uint32_t beginPos,
                                 Maybe<DeclarationKind>* redeclaredKind,
                                 uint32_t* prevPos) {
  return tryDeclareVarHelper<NotDryRun>(name, kind, beginPos, redeclaredKind,
                                        prevPos);
}

template <ParseContext::DryRunOption dryRunOption>
bool ParseContext::tryDeclareVarHelper(const ParserName* name,
                                       DeclarationKind kind, uint32_t beginPos,
                                       Maybe<DeclarationKind>* redeclaredKind,
                                       uint32_t* prevPos) {
  MOZ_ASSERT(DeclarationKindIsVar(kind));

  // It is an early error if a 'var' declaration appears inside a
  // scope contour that has a lexical declaration of the same name. For
  // example, the following are early errors:
  //
  //   { let x; var x; }
  //   { { var x; } let x; }
  //
  // And the following are not:
  //
  //   { var x; var x; }
  //   { { let x; } var x; }

  for (ParseContext::Scope* scope = innermostScope();
       scope != varScope().enclosing(); scope = scope->enclosing()) {
    if (AddDeclaredNamePtr p = scope->lookupDeclaredNameForAdd(name)) {
      DeclarationKind declaredKind = p->value()->kind();
      if (DeclarationKindIsVar(declaredKind)) {
        if (dryRunOption == NotDryRun) {
          RedeclareVar(p, kind);
        }
      } else if (!DeclarationKindIsParameter(declaredKind)) {
        // Annex B.3.5 allows redeclaring simple (non-destructured)
        // catch parameters with var declarations.
        bool annexB35Allowance =
            declaredKind == DeclarationKind::SimpleCatchParameter;

        // Annex B.3.3 allows redeclaring functions in the same block.
        bool annexB33Allowance =
            declaredKind == DeclarationKind::SloppyLexicalFunction &&
            kind == DeclarationKind::VarForAnnexBLexicalFunction &&
            scope == innermostScope();

        if (!annexB35Allowance && !annexB33Allowance) {
          *redeclaredKind = Some(declaredKind);
          *prevPos = p->value()->pos();
          return true;
        }
      } else if (kind == DeclarationKind::VarForAnnexBLexicalFunction) {
        MOZ_ASSERT(DeclarationKindIsParameter(declaredKind));

        // Annex B.3.3.1 disallows redeclaring parameter names.
        // We don't need to set *prevPos here since this case is not
        // an error.
        *redeclaredKind = Some(declaredKind);
        return true;
      }
    } else if (dryRunOption == NotDryRun) {
      if (!scope->addDeclaredName(this, p, name, kind, beginPos)) {
        return false;
      }
    }

    // DryRunOption is used for propagating Annex B functions: we don't
    // want to declare the synthesized Annex B vars until we exit the var
    // scope and know that no early errors would have occurred. In order
    // to avoid quadratic search, we only check for var redeclarations in
    // the innermost scope when doing a dry run.
    if (dryRunOption == DryRunInnermostScopeOnly) {
      break;
    }
  }

  if (!sc()->strict() && sc()->isEvalContext() &&
      (dryRunOption == NotDryRun || innermostScope() == &varScope())) {
    if (!isVarRedeclaredInEval(name, kind, redeclaredKind)) {
      return false;
    }
    // We don't have position information at runtime.
    *prevPos = DeclaredNameInfo::npos;
  }

  return true;
}

bool ParseContext::hasUsedName(const UsedNameTracker& usedNames,
                               const ParserName* name) {
  if (auto p = usedNames.lookup(name)) {
    return p->value().isUsedInScript(scriptId());
  }
  return false;
}

bool ParseContext::hasUsedFunctionSpecialName(const UsedNameTracker& usedNames,
                                              const ParserName* name) {
  MOZ_ASSERT(name == sc()->cx_->parserNames().arguments ||
             name == sc()->cx_->parserNames().dotThis);
  return hasUsedName(usedNames, name) ||
         functionBox()->bindingsAccessedDynamically();
}

bool ParseContext::declareFunctionThis(const UsedNameTracker& usedNames,
                                       bool canSkipLazyClosedOverBindings) {
  // The asm.js validator does all its own symbol-table management so, as an
  // optimization, avoid doing any work here.
  if (useAsmOrInsideUseAsm()) {
    return true;
  }

  // Derived class constructors emit JSOp::CheckReturn, which requires
  // '.this' to be bound.
  FunctionBox* funbox = functionBox();
  const ParserName* dotThis = sc()->cx_->parserNames().dotThis;

  bool declareThis;
  if (canSkipLazyClosedOverBindings) {
    declareThis = funbox->functionHasThisBinding();
  } else {
    declareThis = hasUsedFunctionSpecialName(usedNames, dotThis) ||
                  funbox->isClassConstructor();
  }

  if (declareThis) {
    ParseContext::Scope& funScope = functionScope();
    AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotThis);
    MOZ_ASSERT(!p);
    if (!funScope.addDeclaredName(this, p, dotThis, DeclarationKind::Var,
                                  DeclaredNameInfo::npos)) {
      return false;
    }
    funbox->setFunctionHasThisBinding();
  }

  return true;
}

bool ParseContext::declareFunctionArgumentsObject(
    const UsedNameTracker& usedNames, bool canSkipLazyClosedOverBindings) {
  FunctionBox* funbox = functionBox();
  ParseContext::Scope& funScope = functionScope();
  ParseContext::Scope& _varScope = varScope();

  bool usesArguments = false;
  bool hasExtraBodyVarScope = &funScope != &_varScope;

  // Time to implement the odd semantics of 'arguments'.
  const ParserName* argumentsName = sc()->cx_->parserNames().arguments;

  bool tryDeclareArguments;
  if (canSkipLazyClosedOverBindings) {
    tryDeclareArguments = funbox->shouldDeclareArguments();
  } else {
    tryDeclareArguments = hasUsedFunctionSpecialName(usedNames, argumentsName);
  }

  // ES 9.2.12 steps 19 and 20 say formal parameters, lexical bindings,
  // and body-level functions named 'arguments' shadow the arguments
  // object.
  //
  // So even if there wasn't a free use of 'arguments' but there is a var
  // binding of 'arguments', we still might need the arguments object.
  //
  // If we have an extra var scope due to parameter expressions and the body
  // declared 'var arguments', we still need to declare 'arguments' in the
  // function scope.
  DeclaredNamePtr p = _varScope.lookupDeclaredName(argumentsName);
  if (p && p->value()->kind() == DeclarationKind::Var) {
    if (hasExtraBodyVarScope) {
      tryDeclareArguments = true;
    } else {
      usesArguments = true;
    }
  }

  if (tryDeclareArguments) {
    AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(argumentsName);
    if (!p) {
      if (!funScope.addDeclaredName(this, p, argumentsName,
                                    DeclarationKind::Var,
                                    DeclaredNameInfo::npos)) {
        return false;
      }
      funbox->setShouldDeclareArguments();
      usesArguments = true;
    } else if (hasExtraBodyVarScope) {
      // Formal parameters shadow the arguments object.
      return true;
    }
  }

  if (usesArguments) {
    // There is an 'arguments' binding. Is the arguments object definitely
    // needed?
    //
    // Also see the flags' comments in ContextFlags.
    funbox->setArgumentsHasVarBinding();

    // Dynamic scope access destroys all hope of optimization.
    if (sc()->bindingsAccessedDynamically()) {
      funbox->setAlwaysNeedsArgsObj();
    }
  }

  return true;
}

bool ParseContext::declareDotGeneratorName() {
  // The special '.generator' binding must be on the function scope, and must
  // be marked closed-over, as generators expect to find it on the CallObject.
  ParseContext::Scope& funScope = functionScope();
  const ParserName* dotGenerator = sc()->cx_->parserNames().dotGenerator;
  AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotGenerator);
  if (!p) {
    if (!funScope.addDeclaredName(this, p, dotGenerator, DeclarationKind::Var,
                                  DeclaredNameInfo::npos, ClosedOver::Yes)) {
      return false;
    }
  }
  return true;
}

}  // namespace frontend

}  // namespace js
