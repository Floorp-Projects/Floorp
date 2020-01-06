/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Stencil.h"

#include "frontend/SharedContext.h"
#include "js/TracingAPI.h"
#include "vm/EnvironmentObject.h"
#include "vm/Scope.h"

using namespace js;
using namespace js::frontend;

Scope* ScopeCreationData::createScope(JSContext* cx) {
  // If we've already created a scope, best just return it.
  if (scope_) {
    return scope_;
  }

  Scope* scope = nullptr;
  switch (kind()) {
    case ScopeKind::Function: {
      scope = createSpecificScope<FunctionScope>(cx);
      break;
    }
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical: {
      scope = createSpecificScope<LexicalScope>(cx);
      break;
    }
    case ScopeKind::FunctionBodyVar:
    case ScopeKind::ParameterExpressionVar: {
      scope = createSpecificScope<VarScope>(cx);
      break;
    }
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      scope = createSpecificScope<GlobalScope>(cx);
      break;
    }
    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      scope = createSpecificScope<EvalScope>(cx);
      break;
    }
    case ScopeKind::Module: {
      scope = createSpecificScope<ModuleScope>(cx);
      break;
    }
    case ScopeKind::With: {
      scope = createSpecificScope<WithScope>(cx);
      break;
    }
    default: {
      MOZ_CRASH("Unexpected deferred type");
    }
  }
  return scope;
}

void ScopeCreationData::trace(JSTracer* trc) {
  if (enclosing_) {
    enclosing_.trace(trc);
  }
  if (environmentShape_) {
    TraceEdge(trc, &environmentShape_, "ScopeCreationData Environment Shape");
  }
  if (scope_) {
    TraceEdge(trc, &scope_, "ScopeCreationData Scope");
  }
  if (funbox_) {
    funbox_->trace(trc);
  }

  // Trace Datas
  if (data_) {
    switch (kind()) {
      case ScopeKind::Function: {
        data<FunctionScope>().trace(trc);
        break;
      }
      case ScopeKind::Lexical:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
      case ScopeKind::NamedLambda:
      case ScopeKind::StrictNamedLambda:
      case ScopeKind::FunctionLexical: {
        data<LexicalScope>().trace(trc);
        break;
      }
      case ScopeKind::FunctionBodyVar:
      case ScopeKind::ParameterExpressionVar: {
        data<VarScope>().trace(trc);
        break;
      }
      case ScopeKind::Global:
      case ScopeKind::NonSyntactic: {
        data<GlobalScope>().trace(trc);
        break;
      }
      case ScopeKind::Eval:
      case ScopeKind::StrictEval: {
        data<EvalScope>().trace(trc);
        break;
      }
      case ScopeKind::Module: {
        data<ModuleScope>().trace(trc);
        break;
      }
      case ScopeKind::With:
      default:
        MOZ_CRASH("Unexpected data type");
    }
  }
}

uint32_t ScopeCreationData::nextFrameSlot() const {
  switch (kind()) {
    case ScopeKind::Function:
      return nextFrameSlot<FunctionScope>();
    case ScopeKind::FunctionBodyVar:
    case ScopeKind::ParameterExpressionVar:
      return nextFrameSlot<VarScope>();
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::FunctionLexical:
      return nextFrameSlot<LexicalScope>();
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
      // Named lambda scopes cannot have frame slots.
      return 0;
    case ScopeKind::Eval:
    case ScopeKind::StrictEval:
      return nextFrameSlot<EvalScope>();
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic:
      return 0;
    case ScopeKind::Module:
      return nextFrameSlot<ModuleScope>();
    case ScopeKind::WasmInstance:
    case ScopeKind::WasmFunction:
    case ScopeKind::With:
      MOZ_CRASH(
          "With, WasmInstance and WasmFunction Scopes don't get "
          "nextFrameSlot()");
      return 0;
  }
  MOZ_CRASH("Not an enclosing intra-frame scope");
}

bool ScopeCreationData::isArrow() const { return funbox_->isArrow(); }

JSFunction* ScopeCreationData::canonicalFunction() const {
  return funbox_->function();
}
