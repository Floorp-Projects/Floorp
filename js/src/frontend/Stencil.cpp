/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Stencil.h"

#include "frontend/CompilationInfo.h"
#include "frontend/SharedContext.h"
#include "js/TracingAPI.h"
#include "vm/EnvironmentObject.h"
#include "vm/JSContext.h"
#include "vm/Scope.h"
#include "vm/StringType.h"

using namespace js;
using namespace js::frontend;

bool frontend::RegExpCreationData::init(JSContext* cx, JSAtom* pattern,
                                        JS::RegExpFlags flags) {
  length_ = pattern->length();
  buf_ = cx->make_pod_array<char16_t>(length_);
  if (!buf_) {
    return false;
  }
  js::CopyChars(buf_.get(), *pattern);
  flags_ = flags;
  return true;
}

bool frontend::EnvironmentShapeCreationData::createShape(
    JSContext* cx, MutableHandleShape shape) {
  struct Matcher {
    JSContext* cx;
    MutableHandleShape& shape;

    bool operator()(CreateEnvShapeData& data) {
      shape.set(CreateEnvironmentShape(cx, data.freshBi, data.cls,
                                       data.nextEnvironmentSlot,
                                       data.baseShapeFlags));
      return shape;
    }

    bool operator()(EmptyEnvShapeData& data) {
      shape.set(EmptyEnvironmentShape(cx, data.cls, JSSLOT_FREE(data.cls),
                                      data.baseShapeFlags));
      return shape;
    }

    bool operator()(mozilla::Nothing&) {
      shape.set(nullptr);
      return true;
    }
  };

  Matcher m{cx, shape};
  return data_.match(m);
}

// This is used during allocation of the scopes to ensure that we only
// allocate GC scopes with GC-enclosing scopes. This can recurse through
// the scope chain.
//
// Once all ScopeCreation for a compilation tree is centralized, this
// will go away, to be replaced with a single top down GC scope allocation.
//
// This uses an outparam to disambiguate between the case where we have a
// real nullptr scope and we failed to allocate a new scope because of OOM.
bool ScopeCreationData::getOrCreateEnclosingScope(JSContext* cx,
                                                  MutableHandleScope scope) {
  if (enclosing_.isScopeCreationData()) {
    ScopeCreationData& enclosingData = enclosing_.scopeCreationData().get();
    if (enclosingData.hasScope()) {
      scope.set(enclosingData.getScope());
      return true;
    }

    scope.set(enclosingData.createScope(cx, enclosing_.compilationInfo()));
    return !!scope;
  }

  scope.set(enclosing_.scope());
  return true;
}

JSFunction* ScopeCreationData::function(
    frontend::CompilationInfo& compilationInfo) {
  return compilationInfo.functions[*functionIndex_];
}

Scope* ScopeCreationData::createScope(JSContext* cx,
                                      CompilationInfo& compilationInfo) {
  // If we've already created a scope, best just return it.
  if (scope_) {
    return scope_;
  }

  Scope* scope = nullptr;
  switch (kind()) {
    case ScopeKind::Function: {
      scope = createSpecificScope<FunctionScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody: {
      scope = createSpecificScope<LexicalScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::FunctionBodyVar: {
      scope = createSpecificScope<VarScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      scope = createSpecificScope<GlobalScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      scope = createSpecificScope<EvalScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::Module: {
      scope = createSpecificScope<ModuleScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::With: {
      scope = createSpecificScope<WithScope>(cx, compilationInfo);
      break;
    }
    case ScopeKind::WasmFunction:
    case ScopeKind::WasmInstance: {
      MOZ_CRASH("Unexpected deferred type");
    }
  }
  return scope;
}

void ScopeCreationData::trace(JSTracer* trc) {
  if (enclosing_) {
    enclosing_.trace(trc);
  }

  environmentShape_.trace(trc);

  if (scope_) {
    TraceEdge(trc, &scope_, "ScopeCreationData Scope");
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
      case ScopeKind::FunctionLexical:
      case ScopeKind::ClassBody: {
        data<LexicalScope>().trace(trc);
        break;
      }
      case ScopeKind::FunctionBodyVar: {
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
      return nextFrameSlot<VarScope>();
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody:
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

void ScriptStencil::trace(JSTracer* trc) {
  for (ScriptThingVariant& thing : gcThings) {
    if (thing.is<ScriptAtom>()) {
      JSAtom* atom = thing.as<ScriptAtom>();
      TraceRoot(trc, &atom, "script-atom");
      MOZ_ASSERT(atom == thing.as<ScriptAtom>(), "Atoms should be unmovable");
    }
  }

  if (functionAtom) {
    TraceRoot(trc, &functionAtom, "script-atom");
  }
}
