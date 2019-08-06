/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/ParseContext-inl.h"
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

bool UsedNameTracker::noteUse(JSContext* cx, JSAtom* name, uint32_t scriptId,
                              uint32_t scopeId) {
  if (UsedNameMap::AddPtr p = map_.lookupForAdd(name)) {
    if (!p || !p->value().noteUsedInScope(scriptId, scopeId)) {
      return false;
    }
  } else {
    UsedNameInfo info(cx);
    if (!info.noteUsedInScope(scriptId, scopeId)) {
      return false;
    }
    if (!map_.add(p, name, std::move(info))) {
      return false;
    }
  }

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
    UniqueChars bytes = AtomToPrintableString(cx, r.front().key());
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
    RootedPropertyName name(pc->sc()->cx_);
    Maybe<DeclarationKind> redeclaredKind;
    uint32_t unused;
    for (FunctionBox* funbox : *possibleAnnexBFunctionBoxes_) {
      if (pc->annexBAppliesToLexicalFunctionInInnermostScope(funbox)) {
        name = funbox->explicitName()->asPropertyName();
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
      if (pc->annexBAppliesToLexicalFunctionInInnermostScope(funbox)) {
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
    JSAtom* name = r.front().key();
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
                           class UsedNameTracker& usedNames,
                           Directives* newDirectives,
                           FunctionTreeHolder* treeHolder, bool isFull)
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
      innerFunctionBoxesForLazy(cx),
      newDirectives(newDirectives),
      lastYieldOffset(NoYieldOffset),
      lastAwaitOffset(NoAwaitOffset),
      scriptId_(usedNames.nextScriptId()),
      isStandaloneFunctionBody_(false),
      superScopeNeedsHomeObject_(false) {
  if (isFunctionBox()) {
    // We exclude ASM bodies because they are always eager, and the
    // FunctionBoxes that get added to the tree in an AsmJS compilation
    // don't have a long enough lifespan, as the AsmJS parser is stack
    // allocated inside the ModuleValidator.
    if (treeHolder && !this->functionBox()->useAsmOrInsideUseAsm()) {
      tree.emplace(treeHolder);
    }
    if (functionBox()->isNamedLambda()) {
      namedLambdaScope_.emplace(cx, parent, usedNames);
    }
    functionScope_.emplace(cx, parent, usedNames);
  }
}

bool ParseContext::init() {
  if (scriptId_ == UINT32_MAX) {
    errorReporter_.errorNoOffset(JSMSG_NEED_DIET, js_script_str);
    return false;
  }

  JSContext* cx = sc()->cx_;

  if (isFunctionBox()) {
    if (tree && !tree->init(cx, this->functionBox())) {
      return false;
    }
    // Named lambdas always need a binding for their own name. If this
    // binding is closed over when we finish parsing the function in
    // finishExtraFunctionScopes, the function box needs to be marked as
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

bool ParseContext::annexBAppliesToLexicalFunctionInInnermostScope(
    FunctionBox* funbox) {
  MOZ_ASSERT(!sc()->strict());

  RootedPropertyName name(sc()->cx_, funbox->explicitName()->asPropertyName());
  Maybe<DeclarationKind> redeclaredKind = isVarRedeclaredInInnermostScope(
      name, DeclarationKind::VarForAnnexBLexicalFunction);

  if (!redeclaredKind && isFunctionBox()) {
    Scope& funScope = functionScope();
    if (&funScope != &varScope()) {
      // Annex B.3.3.1 disallows redeclaring parameter names. In the
      // presence of parameter expressions, parameter names are on the
      // function scope, which encloses the var scope. This means the
      // isVarRedeclaredInInnermostScope call above would not catch this
      // case, so test it manually.
      if (AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(name)) {
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
  return !redeclaredKind;
}

Maybe<DeclarationKind> ParseContext::isVarRedeclaredInInnermostScope(
    HandlePropertyName name, DeclarationKind kind) {
  Maybe<DeclarationKind> redeclaredKind;
  uint32_t unused;
  MOZ_ALWAYS_TRUE(tryDeclareVarHelper<DryRunInnermostScopeOnly>(
      name, kind, DeclaredNameInfo::npos, &redeclaredKind, &unused));
  return redeclaredKind;
}

Maybe<DeclarationKind> ParseContext::isVarRedeclaredInEval(
    HandlePropertyName name, DeclarationKind kind) {
  MOZ_ASSERT(DeclarationKindIsVar(kind));
  MOZ_ASSERT(sc()->isEvalContext());

  // In the case of eval, we also need to check enclosing VM scopes to see
  // if the var declaration is allowed in the context.
  //
  // This check is necessary in addition to
  // js::CheckEvalDeclarationConflicts because we only know during parsing
  // if a var is bound by for-of.
  js::Scope* enclosingScope = sc()->compilationEnclosingScope();
  js::Scope* varScope = EvalScope::nearestVarScopeForDirectEval(enclosingScope);
  MOZ_ASSERT(varScope);
  for (ScopeIter si(enclosingScope); si; si++) {
    for (js::BindingIter bi(si.scope()); bi; bi++) {
      if (bi.name() != name) {
        continue;
      }

      switch (bi.kind()) {
        case BindingKind::Let: {
          // Annex B.3.5 allows redeclaring simple (non-destructured)
          // catch parameters with var declarations.
          bool annexB35Allowance = si.kind() == ScopeKind::SimpleCatch;
          if (!annexB35Allowance) {
            return Some(ScopeKindIsCatch(si.kind())
                            ? DeclarationKind::CatchParameter
                            : DeclarationKind::Let);
          }
          break;
        }

        case BindingKind::Const:
          return Some(DeclarationKind::Const);

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

  return Nothing();
}

bool ParseContext::tryDeclareVar(HandlePropertyName name, DeclarationKind kind,
                                 uint32_t beginPos,
                                 Maybe<DeclarationKind>* redeclaredKind,
                                 uint32_t* prevPos) {
  return tryDeclareVarHelper<NotDryRun>(name, kind, beginPos, redeclaredKind,
                                        prevPos);
}

template <ParseContext::DryRunOption dryRunOption>
bool ParseContext::tryDeclareVarHelper(HandlePropertyName name,
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
    *redeclaredKind = isVarRedeclaredInEval(name, kind);
    // We don't have position information at runtime.
    *prevPos = DeclaredNameInfo::npos;
  }

  return true;
}

bool ParseContext::hasUsedName(const UsedNameTracker& usedNames,
                               HandlePropertyName name) {
  if (auto p = usedNames.lookup(name)) {
    return p->value().isUsedInScript(scriptId());
  }
  return false;
}

bool ParseContext::hasUsedFunctionSpecialName(const UsedNameTracker& usedNames,
                                              HandlePropertyName name) {
  MOZ_ASSERT(name == sc()->cx_->names().arguments ||
             name == sc()->cx_->names().dotThis);
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

  // Derived class constructors emit JSOP_CHECKRETURN, which requires
  // '.this' to be bound.
  FunctionBox* funbox = functionBox();
  HandlePropertyName dotThis = sc()->cx_->names().dotThis;

  bool declareThis;
  if (canSkipLazyClosedOverBindings) {
    declareThis = funbox->function()->lazyScript()->hasThisBinding();
  } else {
    declareThis =
        hasUsedFunctionSpecialName(usedNames, dotThis) ||
        funbox->kind() == FunctionFlags::FunctionKind::ClassConstructor;
  }

  if (declareThis) {
    ParseContext::Scope& funScope = functionScope();
    AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotThis);
    MOZ_ASSERT(!p);
    if (!funScope.addDeclaredName(this, p, dotThis, DeclarationKind::Var,
                                  DeclaredNameInfo::npos)) {
      return false;
    }
    funbox->setHasThisBinding();
  }

  return true;
}

bool ParseContext::declareFunctionArgumentsObject(
    const UsedNameTracker& usedNames, bool canSkipLazyClosedOverBindings) {
  FunctionBox* funbox = functionBox();
  ParseContext::Scope& funScope = functionScope();
  ParseContext::Scope& _varScope = varScope();

  bool hasExtraBodyVarScope = &funScope != &_varScope;

  // Time to implement the odd semantics of 'arguments'.
  HandlePropertyName argumentsName = sc()->cx_->names().arguments;

  bool tryDeclareArguments;
  if (canSkipLazyClosedOverBindings) {
    tryDeclareArguments =
        funbox->function()->lazyScript()->shouldDeclareArguments();
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
      funbox->usesArguments = true;
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
      funbox->declaredArguments = true;
      funbox->usesArguments = true;
    } else if (hasExtraBodyVarScope) {
      // Formal parameters shadow the arguments object.
      return true;
    }
  }

  // Compute if we need an arguments object.
  if (funbox->usesArguments) {
    // There is an 'arguments' binding. Is the arguments object definitely
    // needed?
    //
    // Also see the flags' comments in ContextFlags.
    funbox->setArgumentsHasLocalBinding();

    // Dynamic scope access destroys all hope of optimization.
    if (sc()->bindingsAccessedDynamically()) {
      funbox->setDefinitelyNeedsArgsObj();
    }
  }

  return true;
}

bool ParseContext::declareDotGeneratorName() {
  // The special '.generator' binding must be on the function scope, as
  // generators expect to find it on the CallObject.
  ParseContext::Scope& funScope = functionScope();
  HandlePropertyName dotGenerator = sc()->cx_->names().dotGenerator;
  AddDeclaredNamePtr p = funScope.lookupDeclaredNameForAdd(dotGenerator);
  if (!p &&
      !funScope.addDeclaredName(this, p, dotGenerator, DeclarationKind::Var,
                                DeclaredNameInfo::npos)) {
    return false;
  }
  return true;
}

}  // namespace frontend

}  // namespace js
