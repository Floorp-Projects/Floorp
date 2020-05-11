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
    case ScopeKind::FunctionBodyVar: {
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

  environmentShape_.trace(trc);

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
bool ScopeCreationData::isClassConstructor() const {
  return funbox_->isClassConstructor();
}
const FieldInitializers& ScopeCreationData::fieldInitializers() const {
  return *funbox_->fieldInitializers;
}

void ScriptStencilBase::trace(JSTracer* trc) {
  for (ScriptThingVariant& thing : gcThings) {
    if (thing.is<ClosedOverBinding>()) {
      JSAtom* atom = thing.as<ClosedOverBinding>();
      TraceRoot(trc, &atom, "closed-over-binding");
      MOZ_ASSERT(atom == thing.as<ClosedOverBinding>(),
                 "Atoms should be unmovable");
    }
  }
}

JSScript* ScriptStencil::intoScript(JSContext* cx,
                                    CompilationInfo& compilationInfo,
                                    SourceExtent extent) {
  MOZ_ASSERT(immutableScriptData,
             "Need generated bytecode to use ScriptStencil::intoScript");

  RootedObject functionOrGlobal(cx, cx->global());
  if (functionIndex) {
    functionOrGlobal =
        compilationInfo.funcData[*functionIndex].as<JSFunction*>();
  }

  RootedScript script(
      cx, JSScript::Create(cx, functionOrGlobal, compilationInfo.sourceObject,
                           extent, immutableFlags));
  if (!script) {
    return nullptr;
  }

  if (!JSScript::fullyInitFromStencil(cx, compilationInfo, script, *this)) {
    return nullptr;
  }

  return script;
}
