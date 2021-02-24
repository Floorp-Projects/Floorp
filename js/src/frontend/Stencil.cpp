/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/Stencil.h"

#include "mozilla/OperatorNewExtensions.h"  // mozilla::KnownNotNull
#include "mozilla/PodOperations.h"          // mozilla::PodCopy
#include "mozilla/RefPtr.h"                 // RefPtr
#include "mozilla/Sprintf.h"                // SprintfLiteral

#include "ds/LifoAlloc.h"                  // LifoAlloc
#include "frontend/AbstractScopePtr.h"     // ScopeIndex
#include "frontend/BytecodeCompilation.h"  // CanLazilyParse
#include "frontend/BytecodeSection.h"      // EmitScriptThingsVector
#include "frontend/CompilationStencil.h"  // CompilationStencil, ExtensibleCompilationStencil, CompilationGCOutput
#include "frontend/SharedContext.h"
#include "gc/AllocKind.h"               // gc::AllocKind
#include "gc/Rooting.h"                 // RootedAtom
#include "gc/Tracer.h"                  // TraceNullableRoot
#include "js/CallArgs.h"                // JSNative
#include "js/experimental/JSStencil.h"  // JS::Stencil
#include "js/GCAPI.h"                   // JS::AutoCheckCannotGC
#include "js/RootingAPI.h"              // Rooted
#include "js/Transcoding.h"             // JS::TranscodeBuffer
#include "js/Value.h"                   // ObjectValue
#include "js/WasmModule.h"              // JS::WasmModule
#include "vm/BindingKind.h"             // BindingKind
#include "vm/EnvironmentObject.h"
#include "vm/GeneratorAndAsyncKind.h"  // GeneratorKind, FunctionAsyncKind
#include "vm/JSContext.h"              // JSContext
#include "vm/JSFunction.h"  // JSFunction, GetFunctionPrototype, NewFunctionWithProto
#include "vm/JSObject.h"      // JSObject
#include "vm/JSONPrinter.h"   // js::JSONPrinter
#include "vm/JSScript.h"      // BaseScript, JSScript
#include "vm/ObjectGroup.h"   // TenuredObject
#include "vm/Printer.h"       // js::Fprinter
#include "vm/RegExpObject.h"  // js::RegExpObject
#include "vm/Scope.h"  // Scope, *Scope, ScopeKindString, ScopeIter, ScopeKindIsCatch, BindingIter
#include "vm/ScopeKind.h"     // ScopeKind
#include "vm/StencilEnums.h"  // ImmutableScriptFlagsEnum
#include "vm/StringType.h"    // JSAtom, js::CopyChars
#include "vm/Xdr.h"           // XDRMode, XDRResult, XDREncoder
#include "wasm/AsmJS.h"       // InstantiateAsmJS
#include "wasm/WasmModule.h"  // wasm::Module

#include "vm/EnvironmentObject-inl.h"  // JSObject::enclosingEnvironment
#include "vm/JSFunction-inl.h"         // JSFunction::create

using namespace js;
using namespace js::frontend;

bool ScopeContext::init(JSContext* cx, CompilationInput& input,
                        ParserAtomsTable& parserAtoms, InheritThis inheritThis,
                        JSObject* enclosingEnv) {
  Scope* maybeNonDefaultEnclosingScope = input.maybeNonDefaultEnclosingScope();

  // If this eval is in response to Debugger.Frame.eval, we may have an
  // incomplete scope chain. In order to provide a better debugging experience,
  // we inspect the (optional) environment chain to determine it's enclosing
  // FunctionScope if there is one. If there is no such scope, we use the
  // orignal scope provided.
  //
  // NOTE: This is used to compute the ThisBinding kind and to allow access to
  //       private fields, while other contextual information only uses the
  //       actual scope passed to the compile.
  JS::Rooted<Scope*> effectiveScope(
      cx, determineEffectiveScope(maybeNonDefaultEnclosingScope, enclosingEnv));

  if (inheritThis == InheritThis::Yes) {
    computeThisBinding(effectiveScope);
    computeThisEnvironment(maybeNonDefaultEnclosingScope);
  }
  computeInScope(maybeNonDefaultEnclosingScope);

  cacheEnclosingScope(input.enclosingScope);

  if (input.target == CompilationInput::CompilationTarget::Eval) {
    if (!cacheEnclosingScopeBindingForEval(cx, input, parserAtoms)) {
      return false;
    }
    if (!cachePrivateFieldsForEval(cx, input, effectiveScope, parserAtoms)) {
      return false;
    }
  }

  return true;
}

void ScopeContext::computeThisEnvironment(Scope* enclosingScope) {
  uint32_t envCount = 0;
  for (ScopeIter si(enclosingScope); si; si++) {
    if (si.kind() == ScopeKind::Function) {
      JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();

      // Arrow function inherit the "this" environment of the enclosing script,
      // so continue ignore them.
      if (!fun->isArrow()) {
        allowNewTarget = true;

        if (fun->allowSuperProperty()) {
          allowSuperProperty = true;
          enclosingThisEnvironmentHops = envCount;
        }

        if (fun->isClassConstructor()) {
          memberInitializers =
              fun->baseScript()->useMemberInitializers()
                  ? mozilla::Some(fun->baseScript()->getMemberInitializers())
                  : mozilla::Some(MemberInitializers::Empty());
          MOZ_ASSERT(memberInitializers->valid);
        } else {
          if (fun->isSyntheticFunction()) {
            allowArguments = false;
          }
        }

        if (fun->isDerivedClassConstructor()) {
          allowSuperCall = true;
        }

        // Found the effective "this" environment, so stop.
        return;
      }
    }

    if (si.scope()->hasEnvironment()) {
      envCount++;
    }
  }
}

void ScopeContext::computeThisBinding(Scope* scope) {
  // Inspect the scope-chain.
  for (ScopeIter si(scope); si; si++) {
    if (si.kind() == ScopeKind::Module) {
      thisBinding = ThisBinding::Module;
      return;
    }

    if (si.kind() == ScopeKind::Function) {
      JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();

      // Arrow functions don't have their own `this` binding.
      if (fun->isArrow()) {
        continue;
      }

      // Derived class constructors (and their nested arrow functions and evals)
      // use ThisBinding::DerivedConstructor, which ensures TDZ checks happen
      // when accessing |this|.
      if (fun->isDerivedClassConstructor()) {
        thisBinding = ThisBinding::DerivedConstructor;
      } else {
        thisBinding = ThisBinding::Function;
      }

      return;
    }
  }

  thisBinding = ThisBinding::Global;
}

void ScopeContext::computeInScope(Scope* enclosingScope) {
  for (ScopeIter si(enclosingScope); si; si++) {
    if (si.kind() == ScopeKind::ClassBody) {
      inClass = true;
    }

    if (si.kind() == ScopeKind::With) {
      inWith = true;
    }
  }
}

void ScopeContext::cacheEnclosingScope(Scope* enclosingScope) {
  if (!enclosingScope) {
    return;
  }

  enclosingScopeEnvironmentChainLength =
      enclosingScope->environmentChainLength();

  enclosingScopeKind = enclosingScope->kind();

  if (enclosingScope->is<FunctionScope>()) {
    MOZ_ASSERT(enclosingScope->as<FunctionScope>().canonicalFunction());
    enclosingScopeIsArrow =
        enclosingScope->as<FunctionScope>().canonicalFunction()->isArrow();
  }

  enclosingScopeHasEnvironment = enclosingScope->hasEnvironment();

#ifdef DEBUG
  hasNonSyntacticScopeOnChain =
      enclosingScope->hasOnChain(ScopeKind::NonSyntactic);

  // This computes a general answer for the query "does the enclosing scope
  // have a function scope that needs a home object?", but it's only asserted
  // if the parser parses eval body that contains `super` that needs a home
  // object.
  for (ScopeIter si(enclosingScope); si; si++) {
    if (si.kind() == ScopeKind::Function) {
      JSFunction* fun = si.scope()->as<FunctionScope>().canonicalFunction();
      if (fun->isArrow()) {
        continue;
      }
      if (fun->allowSuperProperty() && fun->baseScript()->needsHomeObject()) {
        hasFunctionNeedsHomeObjectOnChain = true;
      }
      break;
    }
  }
#endif
}

/* static */
Scope* ScopeContext::determineEffectiveScope(Scope* scope,
                                             JSObject* environment) {
  // If the scope-chain is non-syntactic, we may still determine a more precise
  // effective-scope to use instead.
  if (environment && scope->hasOnChain(ScopeKind::NonSyntactic)) {
    JSObject* env = environment;
    while (env) {
      // Look at target of any DebugEnvironmentProxy, but be sure to use
      // enclosingEnvironment() of the proxy itself.
      JSObject* unwrapped = env;
      if (env->is<DebugEnvironmentProxy>()) {
        unwrapped = &env->as<DebugEnvironmentProxy>().environment();
      }

      if (unwrapped->is<CallObject>()) {
        JSFunction* callee = &unwrapped->as<CallObject>().callee();
        return callee->nonLazyScript()->bodyScope();
      }

      env = env->enclosingEnvironment();
    }
  }

  return scope;
}

bool ScopeContext::cacheEnclosingScopeBindingForEval(
    JSContext* cx, CompilationInput& input, ParserAtomsTable& parserAtoms) {
  enclosingLexicalBindingCache_.emplace();

  js::Scope* varScope =
      EvalScope::nearestVarScopeForDirectEval(input.enclosingScope);
  MOZ_ASSERT(varScope);
  for (ScopeIter si(input.enclosingScope); si; si++) {
    for (js::BindingIter bi(si.scope()); bi; bi++) {
      switch (bi.kind()) {
        case BindingKind::Let: {
          // Annex B.3.5 allows redeclaring simple (non-destructured)
          // catch parameters with var declarations.
          bool annexB35Allowance = si.kind() == ScopeKind::SimpleCatch;
          if (!annexB35Allowance) {
            auto kind = ScopeKindIsCatch(si.kind())
                            ? EnclosingLexicalBindingKind::CatchParameter
                            : EnclosingLexicalBindingKind::Let;
            if (!addToEnclosingLexicalBindingCache(cx, input, parserAtoms,
                                                   bi.name(), kind)) {
              return false;
            }
          }
          break;
        }

        case BindingKind::Const:
          if (!addToEnclosingLexicalBindingCache(
                  cx, input, parserAtoms, bi.name(),
                  EnclosingLexicalBindingKind::Const)) {
            return false;
          }
          break;

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

  return true;
}

bool ScopeContext::addToEnclosingLexicalBindingCache(
    JSContext* cx, CompilationInput& input, ParserAtomsTable& parserAtoms,
    JSAtom* name, EnclosingLexicalBindingKind kind) {
  auto parserName = parserAtoms.internJSAtom(cx, input.atomCache, name);
  if (!parserName) {
    return false;
  }

  // Same lexical binding can appear multiple times across scopes.
  //
  // enclosingLexicalBindingCache_ map is used for detecting conflicting
  // `var` binding, and inner binding should be reported in the error.
  //
  // cacheEnclosingScopeBindingForEval iterates from inner scope, and
  // inner-most binding is added to the map first.
  //
  // Do not overwrite the value with outer bindings.
  auto p = enclosingLexicalBindingCache_->lookupForAdd(parserName);
  if (!p) {
    if (!enclosingLexicalBindingCache_->add(p, parserName, kind)) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  return true;
}

static bool IsPrivateField(JSAtom* atom) {
  MOZ_ASSERT(atom->length() > 0);

  JS::AutoCheckCannotGC nogc;
  if (atom->hasLatin1Chars()) {
    return atom->latin1Chars(nogc)[0] == '#';
  }

  return atom->twoByteChars(nogc)[0] == '#';
}

bool ScopeContext::cachePrivateFieldsForEval(JSContext* cx,
                                             CompilationInput& input,
                                             Scope* effectiveScope,
                                             ParserAtomsTable& parserAtoms) {
  if (!input.options.privateClassFields) {
    return true;
  }

  effectiveScopePrivateFieldCache_.emplace();

  for (ScopeIter si(effectiveScope); si; si++) {
    if (si.scope()->kind() != ScopeKind::ClassBody) {
      continue;
    }

    for (js::BindingIter bi(si.scope()); bi; bi++) {
      if (IsPrivateField(bi.name())) {
        auto parserName =
            parserAtoms.internJSAtom(cx, input.atomCache, bi.name());
        if (!parserName) {
          return false;
        }

        if (!effectiveScopePrivateFieldCache_->put(parserName)) {
          ReportOutOfMemory(cx);
          return false;
        }
      }
    }
  }

  return true;
}

#ifdef DEBUG
static bool NameIsOnEnvironment(Scope* scope, JSAtom* name) {
  for (BindingIter bi(scope); bi; bi++) {
    // If found, the name must already be on the environment or an import,
    // or else there is a bug in the closed-over name analysis in the
    // Parser.
    if (bi.name() == name) {
      BindingLocation::Kind kind = bi.location().kind();

      if (bi.hasArgumentSlot()) {
        JSScript* script = scope->as<FunctionScope>().script();
        if (script->functionAllowsParameterRedeclaration()) {
          // Check for duplicate positional formal parameters.
          for (BindingIter bi2(bi); bi2 && bi2.hasArgumentSlot(); bi2++) {
            if (bi2.name() == name) {
              kind = bi2.location().kind();
            }
          }
        }
      }

      return kind == BindingLocation::Kind::Global ||
             kind == BindingLocation::Kind::Environment ||
             kind == BindingLocation::Kind::Import;
    }
  }

  // If not found, assume it's on the global or dynamically accessed.
  return true;
}
#endif

/* static */
NameLocation ScopeContext::searchInDelazificationEnclosingScope(
    JSContext* cx, CompilationInput& input, ParserAtomsTable& parserAtoms,
    TaggedParserAtomIndex name, uint8_t hops) {
  MOZ_ASSERT(input.target ==
             CompilationInput::CompilationTarget::Delazification);

  // TODO-Stencil
  //   Here, we convert our name into a JSAtom*, and hard-crash on failure
  //   to allocate.  This conversion should not be required as we should be
  //   able to iterate up snapshotted scope chains that use parser atoms.
  //
  //   This will be fixed when the enclosing scopes are snapshotted.
  //
  //   See bug 1690277.
  JSAtom* jsname = nullptr;
  {
    AutoEnterOOMUnsafeRegion oomUnsafe;
    jsname = parserAtoms.toJSAtom(cx, name, input.atomCache);
    if (!jsname) {
      oomUnsafe.crash("EmitterScope::searchAndCache");
    }
  }

  for (ScopeIter si(input.enclosingScope); si; si++) {
    MOZ_ASSERT(NameIsOnEnvironment(si.scope(), jsname));

    bool hasEnv = si.hasSyntacticEnvironment();

    switch (si.kind()) {
      case ScopeKind::Function:
        if (hasEnv) {
          JSScript* script = si.scope()->as<FunctionScope>().script();
          if (script->funHasExtensibleScope()) {
            return NameLocation::Dynamic();
          }

          for (BindingIter bi(si.scope()); bi; bi++) {
            if (bi.name() != jsname) {
              continue;
            }

            BindingLocation bindLoc = bi.location();
            if (bi.hasArgumentSlot() &&
                script->functionAllowsParameterRedeclaration()) {
              // Check for duplicate positional formal parameters.
              for (BindingIter bi2(bi); bi2 && bi2.hasArgumentSlot(); bi2++) {
                if (bi2.name() == jsname) {
                  bindLoc = bi2.location();
                }
              }
            }

            MOZ_ASSERT(bindLoc.kind() == BindingLocation::Kind::Environment);
            return NameLocation::EnvironmentCoordinate(bi.kind(), hops,
                                                       bindLoc.slot());
          }
        }
        break;

      case ScopeKind::FunctionBodyVar:
      case ScopeKind::Lexical:
      case ScopeKind::NamedLambda:
      case ScopeKind::StrictNamedLambda:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
      case ScopeKind::FunctionLexical:
      case ScopeKind::ClassBody:
        if (hasEnv) {
          for (BindingIter bi(si.scope()); bi; bi++) {
            if (bi.name() != jsname) {
              continue;
            }

            // The name must already have been marked as closed
            // over. If this assertion is hit, there is a bug in the
            // name analysis.
            BindingLocation bindLoc = bi.location();
            MOZ_ASSERT(bindLoc.kind() == BindingLocation::Kind::Environment);
            return NameLocation::EnvironmentCoordinate(bi.kind(), hops,
                                                       bindLoc.slot());
          }
        }
        break;

      case ScopeKind::Module:
        // This case is used only when delazifying a function inside
        // module.
        // Initial compilation of module doesn't have enlcosing scope.
        if (hasEnv) {
          for (BindingIter bi(si.scope()); bi; bi++) {
            if (bi.name() != jsname) {
              continue;
            }

            BindingLocation bindLoc = bi.location();

            // Imports are on the environment but are indirect
            // bindings and must be accessed dynamically instead of
            // using an EnvironmentCoordinate.
            if (bindLoc.kind() == BindingLocation::Kind::Import) {
              MOZ_ASSERT(si.kind() == ScopeKind::Module);
              return NameLocation::Import();
            }

            MOZ_ASSERT(bindLoc.kind() == BindingLocation::Kind::Environment);
            return NameLocation::EnvironmentCoordinate(bi.kind(), hops,
                                                       bindLoc.slot());
          }
        }
        break;

      case ScopeKind::Eval:
      case ScopeKind::StrictEval:
        // As an optimization, if the eval doesn't have its own var
        // environment and its immediate enclosing scope is a global
        // scope, all accesses are global.
        if (!hasEnv && si.scope()->enclosing()->is<GlobalScope>()) {
          return NameLocation::Global(BindingKind::Var);
        }
        return NameLocation::Dynamic();

      case ScopeKind::Global:
        return NameLocation::Global(BindingKind::Var);

      case ScopeKind::With:
      case ScopeKind::NonSyntactic:
        return NameLocation::Dynamic();

      case ScopeKind::WasmInstance:
      case ScopeKind::WasmFunction:
        MOZ_CRASH("No direct eval inside wasm functions");
    }

    if (hasEnv) {
      MOZ_ASSERT(hops < ENVCOORD_HOPS_LIMIT - 1);
      hops++;
    }
  }

  MOZ_CRASH("Malformed scope chain");
}

mozilla::Maybe<ScopeContext::EnclosingLexicalBindingKind>
ScopeContext::lookupLexicalBindingInEnclosingScope(TaggedParserAtomIndex name) {
  auto p = enclosingLexicalBindingCache_->lookup(name);
  if (!p) {
    return mozilla::Nothing();
  }

  return mozilla::Some(p->value());
}

bool ScopeContext::effectiveScopePrivateFieldCacheHas(
    TaggedParserAtomIndex name) {
  return effectiveScopePrivateFieldCache_->has(name);
}

bool CompilationInput::initScriptSource(JSContext* cx) {
  source = do_AddRef(cx->new_<ScriptSource>());
  if (!source) {
    return false;
  }

  return source->initFromOptions(cx, options);
}

bool CompilationInput::initForStandaloneFunctionInNonSyntacticScope(
    JSContext* cx, HandleScope functionEnclosingScope) {
  MOZ_ASSERT(!functionEnclosingScope->as<GlobalScope>().isSyntactic());

  target = CompilationTarget::StandaloneFunctionInNonSyntacticScope;
  if (!initScriptSource(cx)) {
    return false;
  }
  enclosingScope = functionEnclosingScope;
  return true;
}

void CompilationInput::trace(JSTracer* trc) {
  atomCache.trace(trc);
  TraceNullableRoot(trc, &lazy, "compilation-input-lazy");
  TraceNullableRoot(trc, &enclosingScope, "compilation-input-enclosing-scope");
}

void CompilationAtomCache::trace(JSTracer* trc) { atoms_.trace(trc); }

void CompilationGCOutput::trace(JSTracer* trc) {
  TraceNullableRoot(trc, &script, "compilation-gc-output-script");
  TraceNullableRoot(trc, &module, "compilation-gc-output-module");
  TraceNullableRoot(trc, &sourceObject, "compilation-gc-output-source");
  functions.trace(trc);
  scopes.trace(trc);
}

RegExpObject* RegExpStencil::createRegExp(
    JSContext* cx, const CompilationAtomCache& atomCache) const {
  RootedAtom atom(cx, atomCache.getExistingAtomAt(cx, atom_));
  return RegExpObject::createSyntaxChecked(cx, atom, flags(), TenuredObject);
}

RegExpObject* RegExpStencil::createRegExpAndEnsureAtom(
    JSContext* cx, ParserAtomsTable& parserAtoms,
    CompilationAtomCache& atomCache) const {
  RootedAtom atom(cx, parserAtoms.toJSAtom(cx, atom_, atomCache));
  if (!atom) {
    return nullptr;
  }
  return RegExpObject::createSyntaxChecked(cx, atom, flags(), TenuredObject);
}

AbstractScopePtr ScopeStencil::enclosing(
    CompilationState& compilationState) const {
  if (hasEnclosing()) {
    return AbstractScopePtr(compilationState, enclosing());
  }

  return AbstractScopePtr::compilationEnclosingScope(compilationState);
}

Scope* ScopeStencil::enclosingExistingScope(
    const CompilationInput& input, const CompilationGCOutput& gcOutput) const {
  if (hasEnclosing()) {
    Scope* result = gcOutput.scopes[enclosing()];
    MOZ_ASSERT(result, "Scope must already exist to use this method");
    return result;
  }

  return input.enclosingScope;
}

Scope* ScopeStencil::createScope(JSContext* cx, CompilationInput& input,
                                 CompilationGCOutput& gcOutput,
                                 BaseParserScopeData* baseScopeData) const {
  RootedScope enclosingScope(cx, enclosingExistingScope(input, gcOutput));
  return createScope(cx, input.atomCache, enclosingScope, baseScopeData);
}

Scope* ScopeStencil::createScope(JSContext* cx, CompilationAtomCache& atomCache,
                                 HandleScope enclosingScope,
                                 BaseParserScopeData* baseScopeData) const {
  switch (kind()) {
    case ScopeKind::Function: {
      using ScopeType = FunctionScope;
      MOZ_ASSERT(matchScopeKind<ScopeType>(kind()));
      return createSpecificScope<ScopeType, CallObject>(
          cx, atomCache, enclosingScope, baseScopeData);
    }
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody: {
      using ScopeType = LexicalScope;
      MOZ_ASSERT(matchScopeKind<ScopeType>(kind()));
      return createSpecificScope<ScopeType, LexicalEnvironmentObject>(
          cx, atomCache, enclosingScope, baseScopeData);
    }
    case ScopeKind::FunctionBodyVar: {
      using ScopeType = VarScope;
      MOZ_ASSERT(matchScopeKind<ScopeType>(kind()));
      return createSpecificScope<ScopeType, VarEnvironmentObject>(
          cx, atomCache, enclosingScope, baseScopeData);
    }
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      using ScopeType = GlobalScope;
      MOZ_ASSERT(matchScopeKind<ScopeType>(kind()));
      return createSpecificScope<ScopeType, std::nullptr_t>(
          cx, atomCache, enclosingScope, baseScopeData);
    }
    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      using ScopeType = EvalScope;
      MOZ_ASSERT(matchScopeKind<ScopeType>(kind()));
      return createSpecificScope<ScopeType, VarEnvironmentObject>(
          cx, atomCache, enclosingScope, baseScopeData);
    }
    case ScopeKind::Module: {
      using ScopeType = ModuleScope;
      MOZ_ASSERT(matchScopeKind<ScopeType>(kind()));
      return createSpecificScope<ScopeType, ModuleEnvironmentObject>(
          cx, atomCache, enclosingScope, baseScopeData);
    }
    case ScopeKind::With: {
      using ScopeType = WithScope;
      MOZ_ASSERT(matchScopeKind<ScopeType>(kind()));
      return createSpecificScope<ScopeType, std::nullptr_t>(
          cx, atomCache, enclosingScope, baseScopeData);
    }
    case ScopeKind::WasmFunction:
    case ScopeKind::WasmInstance: {
      // ScopeStencil does not support WASM
      break;
    }
  }
  MOZ_CRASH();
}

bool CompilationState::prepareSharedDataStorage(JSContext* cx) {
  size_t allScriptCount = scriptData.length();
  size_t nonLazyScriptCount = nonLazyFunctionCount;
  if (!scriptData[0].isFunction()) {
    nonLazyScriptCount++;
  }
  return sharedData.prepareStorageFor(cx, nonLazyScriptCount, allScriptCount);
}

static bool CreateLazyScript(JSContext* cx, const CompilationInput& input,
                             const BaseCompilationStencil& stencil,
                             CompilationGCOutput& gcOutput,
                             const ScriptStencil& script,
                             const ScriptStencilExtra& scriptExtra,
                             ScriptIndex scriptIndex, HandleFunction function) {
  Rooted<ScriptSourceObject*> sourceObject(cx, gcOutput.sourceObject);

  size_t ngcthings = script.gcThingsLength;

  Rooted<BaseScript*> lazy(
      cx, BaseScript::CreateRawLazy(cx, ngcthings, function, sourceObject,
                                    scriptExtra.extent,
                                    scriptExtra.immutableFlags));
  if (!lazy) {
    return false;
  }

  if (ngcthings) {
    if (!EmitScriptThingsVector(cx, input, stencil, gcOutput,
                                script.gcthings(stencil),
                                lazy->gcthingsForInit())) {
      return false;
    }
  }

  if (scriptExtra.useMemberInitializers()) {
    lazy->setMemberInitializers(scriptExtra.memberInitializers());
  }

  function->initScript(lazy);

  return true;
}

// Parser-generated functions with the same prototype will share the same shape
// and group. By computing the correct values up front, we can save a lot of
// time in the Object creation code. For simplicity, we focus only on plain
// synchronous functions which are by far the most common.
//
// This bypasses the `NewObjectCache`, but callers are expected to retrieve a
// valid group and shape from the appropriate de-duplication tables.
//
// NOTE: Keep this in sync with `js::NewFunctionWithProto`.
static JSFunction* CreateFunctionFast(JSContext* cx, CompilationInput& input,
                                      HandleObjectGroup group,
                                      HandleShape shape,
                                      const ScriptStencil& script,
                                      const ScriptStencilExtra& scriptExtra) {
  MOZ_ASSERT(
      !scriptExtra.immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsAsync));
  MOZ_ASSERT(!scriptExtra.immutableFlags.hasFlag(
      ImmutableScriptFlagsEnum::IsGenerator));
  MOZ_ASSERT(!script.functionFlags.isAsmJSNative());

  FunctionFlags flags = script.functionFlags;
  gc::AllocKind allocKind = flags.isExtended()
                                ? gc::AllocKind::FUNCTION_EXTENDED
                                : gc::AllocKind::FUNCTION;

  JSFunction* fun;
  JS_TRY_VAR_OR_RETURN_NULL(
      cx, fun,
      JSFunction::create(cx, allocKind, gc::TenuredHeap, shape, group));

  fun->setArgCount(scriptExtra.nargs);
  fun->setFlags(flags);

  fun->initScript(nullptr);
  fun->initEnvironment(nullptr);

  if (script.functionAtom) {
    JSAtom* atom = input.atomCache.getExistingAtomAt(cx, script.functionAtom);
    MOZ_ASSERT(atom);
    fun->initAtom(atom);
  }

  if (flags.isExtended()) {
    fun->initializeExtended();
  }

  return fun;
}

static JSFunction* CreateFunction(JSContext* cx, CompilationInput& input,
                                  const BaseCompilationStencil& stencil,
                                  const ScriptStencil& script,
                                  const ScriptStencilExtra& scriptExtra,
                                  ScriptIndex functionIndex) {
  GeneratorKind generatorKind =
      scriptExtra.immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsGenerator)
          ? GeneratorKind::Generator
          : GeneratorKind::NotGenerator;
  FunctionAsyncKind asyncKind =
      scriptExtra.immutableFlags.hasFlag(ImmutableScriptFlagsEnum::IsAsync)
          ? FunctionAsyncKind::AsyncFunction
          : FunctionAsyncKind::SyncFunction;

  // Determine the new function's proto. This must be done for singleton
  // functions.
  RootedObject proto(cx);
  if (!GetFunctionPrototype(cx, generatorKind, asyncKind, &proto)) {
    return nullptr;
  }

  gc::AllocKind allocKind = script.functionFlags.isExtended()
                                ? gc::AllocKind::FUNCTION_EXTENDED
                                : gc::AllocKind::FUNCTION;
  bool isAsmJS = script.functionFlags.isAsmJSNative();

  JSNative maybeNative = isAsmJS ? InstantiateAsmJS : nullptr;

  RootedAtom displayAtom(cx);
  if (script.functionAtom) {
    displayAtom.set(input.atomCache.getExistingAtomAt(cx, script.functionAtom));
    MOZ_ASSERT(displayAtom);
  }
  RootedFunction fun(
      cx, NewFunctionWithProto(cx, maybeNative, scriptExtra.nargs,
                               script.functionFlags, nullptr, displayAtom,
                               proto, allocKind, TenuredObject));
  if (!fun) {
    return nullptr;
  }

  if (isAsmJS) {
    RefPtr<const JS::WasmModule> asmJS =
        stencil.asCompilationStencil()
            .asmJS->moduleMap.lookup(functionIndex)
            ->value();

    JSObject* moduleObj = asmJS->createObjectForAsmJS(cx);
    if (!moduleObj) {
      return nullptr;
    }

    fun->setExtendedSlot(FunctionExtended::ASMJS_MODULE_SLOT,
                         ObjectValue(*moduleObj));
  }

  return fun;
}

static bool InstantiateAtoms(JSContext* cx, CompilationInput& input,
                             const BaseCompilationStencil& stencil) {
  return InstantiateMarkedAtoms(cx, stencil.parserAtomData, input.atomCache);
}

static bool InstantiateScriptSourceObject(JSContext* cx,
                                          CompilationInput& input,
                                          const CompilationStencil& stencil,
                                          CompilationGCOutput& gcOutput) {
  MOZ_ASSERT(stencil.source);

  gcOutput.sourceObject = ScriptSourceObject::create(cx, stencil.source.get());
  if (!gcOutput.sourceObject) {
    return false;
  }

  // Off-thread compilations do all their GC heap allocation, including the
  // SSO, in a temporary compartment. Hence, for the SSO to refer to the
  // gc-heap-allocated values in |options|, it would need cross-compartment
  // wrappers from the temporary compartment to the real compartment --- which
  // would then be inappropriate once we merged the temporary and real
  // compartments.
  //
  // Instead, we put off populating those SSO slots in off-thread compilations
  // until after we've merged compartments.
  if (!cx->isHelperThreadContext()) {
    Rooted<ScriptSourceObject*> sourceObject(cx, gcOutput.sourceObject);
    if (!ScriptSourceObject::initFromOptions(cx, sourceObject, input.options)) {
      return false;
    }
  }

  return true;
}

// Instantiate ModuleObject. Further initialization is done after the associated
// BaseScript is instantiated in InstantiateTopLevel.
static bool InstantiateModuleObject(JSContext* cx, CompilationInput& input,
                                    const CompilationStencil& stencil,
                                    CompilationGCOutput& gcOutput) {
  MOZ_ASSERT(stencil.scriptExtra[CompilationStencil::TopLevelIndex].isModule());

  gcOutput.module = ModuleObject::create(cx);
  if (!gcOutput.module) {
    return false;
  }

  Rooted<ModuleObject*> module(cx, gcOutput.module);
  return stencil.moduleMetadata->initModule(cx, input.atomCache, module);
}

// Instantiate JSFunctions for each FunctionBox.
static bool InstantiateFunctions(JSContext* cx, CompilationInput& input,
                                 const BaseCompilationStencil& stencil,
                                 CompilationGCOutput& gcOutput) {
  using ImmutableFlags = ImmutableScriptFlagsEnum;

  if (!gcOutput.functions.resize(stencil.scriptData.size())) {
    ReportOutOfMemory(cx);
    return false;
  }

  // Most JSFunctions will be have the same Shape / Group so we can compute it
  // now to allow fast object creation. Generators / Async will use the slow
  // path instead.
  RootedObject proto(cx,
                     GlobalObject::getOrCreatePrototype(cx, JSProto_Function));
  if (!proto) {
    return false;
  }
  RootedObjectGroup group(cx, ObjectGroup::defaultNewGroup(
                                  cx, &JSFunction::class_, TaggedProto(proto)));
  if (!group) {
    return false;
  }
  RootedShape shape(
      cx, EmptyShape::getInitialShape(cx, &JSFunction::class_,
                                      TaggedProto(proto), /* nfixed = */ 0,
                                      /* objectFlags = */ 0));
  if (!shape) {
    return false;
  }

  for (auto item :
       CompilationStencil::functionScriptStencils(stencil, gcOutput)) {
    const auto& scriptStencil = item.script;
    const auto& scriptExtra = (*item.scriptExtra);
    auto index = item.index;

    MOZ_ASSERT(!item.function);

    // Plain functions can use a fast path.
    bool useFastPath =
        !scriptExtra.immutableFlags.hasFlag(ImmutableFlags::IsAsync) &&
        !scriptExtra.immutableFlags.hasFlag(ImmutableFlags::IsGenerator) &&
        !scriptStencil.functionFlags.isAsmJSNative();

    JSFunction* fun = useFastPath
                          ? CreateFunctionFast(cx, input, group, shape,
                                               scriptStencil, scriptExtra)
                          : CreateFunction(cx, input, stencil, scriptStencil,
                                           scriptExtra, index);
    if (!fun) {
      return false;
    }

    gcOutput.functions[index] = fun;
  }

  return true;
}

// Instantiate Scope for each ScopeStencil.
//
// This should be called after InstantiateFunctions, given FunctionScope needs
// associated JSFunction pointer, and also should be called before
// InstantiateScriptStencils, given JSScript needs Scope pointer in gc things.
static bool InstantiateScopes(JSContext* cx, CompilationInput& input,
                              const BaseCompilationStencil& stencil,
                              CompilationGCOutput& gcOutput) {
  // While allocating Scope object from ScopeStencil, Scope object for the
  // enclosing Scope should already be allocated.
  //
  // Enclosing scope of ScopeStencil can be either ScopeStencil or Scope*
  // pointer.
  //
  // If the enclosing scope is ScopeStencil, it's guaranteed to be earlier
  // element in stencil.scopeData, because enclosing_ field holds
  // index into it, and newly created ScopeStencil is pushed back to the vector.
  //
  // If the enclosing scope is Scope*, it's CompilationInput.enclosingScope.

  MOZ_ASSERT(stencil.scopeData.size() == stencil.scopeNames.size());
  size_t scopeCount = stencil.scopeData.size();
  for (size_t i = 0; i < scopeCount; i++) {
    Scope* scope = stencil.scopeData[i].createScope(cx, input, gcOutput,
                                                    stencil.scopeNames[i]);
    if (!scope) {
      return false;
    }
    gcOutput.scopes.infallibleAppend(scope);
  }

  return true;
}

// Instantiate js::BaseScripts from ScriptStencils for inner functions of the
// compilation. Note that standalone functions and functions being delazified
// are handled below with other top-levels.
static bool InstantiateScriptStencils(JSContext* cx, CompilationInput& input,
                                      const CompilationStencil& stencil,
                                      CompilationGCOutput& gcOutput) {
  MOZ_ASSERT(stencil.isInitialStencil());

  Rooted<JSFunction*> fun(cx);
  for (auto item :
       CompilationStencil::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    auto* scriptExtra = item.scriptExtra;
    fun = item.function;
    auto index = item.index;
    if (scriptStencil.hasSharedData()) {
      // If the function was not referenced by enclosing script's bytecode, we
      // do not generate a BaseScript for it. For example, `(function(){});`.
      //
      // `wasEmittedByEnclosingScript` is false also for standalone
      // functions. They are handled in InstantiateTopLevel.
      if (!scriptStencil.wasEmittedByEnclosingScript()) {
        continue;
      }

      RootedScript script(
          cx, JSScript::fromStencil(cx, input, stencil, gcOutput, index));
      if (!script) {
        return false;
      }

      if (scriptStencil.allowRelazify()) {
        MOZ_ASSERT(script->isRelazifiable());
        script->setAllowRelazify();
      }
    } else if (scriptStencil.functionFlags.isAsmJSNative()) {
      MOZ_ASSERT(fun->isAsmJSNative());
    } else {
      MOZ_ASSERT(fun->isIncomplete());
      if (!CreateLazyScript(cx, input, stencil, gcOutput, scriptStencil,
                            *scriptExtra, index, fun)) {
        return false;
      }
    }
  }

  return true;
}

// Instantiate the Stencil for the top-level script of the compilation. This
// includes standalone functions and functions being delazified.
static bool InstantiateTopLevel(JSContext* cx, CompilationInput& input,
                                const BaseCompilationStencil& stencil,
                                CompilationGCOutput& gcOutput) {
  const ScriptStencil& scriptStencil =
      stencil.scriptData[CompilationStencil::TopLevelIndex];

  // Top-level asm.js does not generate a JSScript.
  if (scriptStencil.functionFlags.isAsmJSNative()) {
    return true;
  }

  MOZ_ASSERT(scriptStencil.hasSharedData());
  MOZ_ASSERT(stencil.sharedData.get(CompilationStencil::TopLevelIndex));

  if (!stencil.isInitialStencil()) {
    MOZ_ASSERT(input.lazy);
    RootedScript script(cx, JSScript::CastFromLazy(input.lazy));
    if (!JSScript::fullyInitFromStencil(cx, input, stencil, gcOutput, script,
                                        CompilationStencil::TopLevelIndex)) {
      return false;
    }

    if (scriptStencil.allowRelazify()) {
      MOZ_ASSERT(script->isRelazifiable());
      script->setAllowRelazify();
    }

    gcOutput.script = script;
    return true;
  }

  gcOutput.script =
      JSScript::fromStencil(cx, input, stencil.asCompilationStencil(), gcOutput,
                            CompilationStencil::TopLevelIndex);
  if (!gcOutput.script) {
    return false;
  }

  if (scriptStencil.allowRelazify()) {
    MOZ_ASSERT(gcOutput.script->isRelazifiable());
    gcOutput.script->setAllowRelazify();
  }

  const ScriptStencilExtra& scriptExtra =
      stencil.asCompilationStencil()
          .scriptExtra[CompilationStencil::TopLevelIndex];

  // Finish initializing the ModuleObject if needed.
  if (scriptExtra.isModule()) {
    RootedScript script(cx, gcOutput.script);
    RootedModuleObject module(cx, gcOutput.module);

    script->outermostScope()->as<ModuleScope>().initModule(module);

    module->initScriptSlots(script);
    module->initStatusSlot();

    if (!ModuleObject::createEnvironment(cx, module)) {
      return false;
    }

    // Off-thread compilation with parseGlobal will freeze the module object
    // in GlobalHelperThreadState::finishSingleParseTask instead.
    if (!cx->isHelperThreadContext()) {
      if (!ModuleObject::Freeze(cx, module)) {
        return false;
      }
    }
  }

  return true;
}

// When a function is first referenced by enclosing script's bytecode, we need
// to update it with information determined by the BytecodeEmitter. This applies
// to both initial and delazification parses. The functions being update may or
// may not have bytecode at this point.
static void UpdateEmittedInnerFunctions(JSContext* cx, CompilationInput& input,
                                        const BaseCompilationStencil& stencil,
                                        CompilationGCOutput& gcOutput) {
  for (auto item :
       CompilationStencil::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    auto& fun = item.function;
    if (!scriptStencil.wasEmittedByEnclosingScript()) {
      continue;
    }

    if (scriptStencil.functionFlags.isAsmJSNative() ||
        fun->baseScript()->hasBytecode()) {
      // Non-lazy inner functions don't use the enclosingScope_ field.
      MOZ_ASSERT(!scriptStencil.hasLazyFunctionEnclosingScopeIndex());
    } else {
      // Apply updates from FunctionEmitter::emitLazy().
      BaseScript* script = fun->baseScript();

      ScopeIndex index = scriptStencil.lazyFunctionEnclosingScopeIndex();
      Scope* scope = gcOutput.scopes[index];
      script->setEnclosingScope(scope);

      // Inferred and Guessed names are computed by BytecodeEmitter and so may
      // need to be applied to existing JSFunctions during delazification.
      if (fun->displayAtom() == nullptr) {
        JSAtom* funcAtom = nullptr;
        if (scriptStencil.functionFlags.hasInferredName() ||
            scriptStencil.functionFlags.hasGuessedAtom()) {
          funcAtom =
              input.atomCache.getExistingAtomAt(cx, scriptStencil.functionAtom);
          MOZ_ASSERT(funcAtom);
        }
        if (scriptStencil.functionFlags.hasInferredName()) {
          fun->setInferredName(funcAtom);
        }
        if (scriptStencil.functionFlags.hasGuessedAtom()) {
          fun->setGuessedAtom(funcAtom);
        }
      }
    }
  }
}

// During initial parse we must link lazy-functions-inside-lazy-functions to
// their enclosing script.
static void LinkEnclosingLazyScript(const BaseCompilationStencil& stencil,
                                    CompilationGCOutput& gcOutput) {
  for (auto item :
       CompilationStencil::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    auto& fun = item.function;
    if (!scriptStencil.functionFlags.hasBaseScript()) {
      continue;
    }

    if (fun->baseScript()->hasBytecode()) {
      continue;
    }

    BaseScript* script = fun->baseScript();
    MOZ_ASSERT(!script->hasBytecode());

    for (auto inner : script->gcthings()) {
      if (!inner.is<JSObject>()) {
        continue;
      }
      inner.as<JSObject>().as<JSFunction>().setEnclosingLazyScript(script);
    }
  }
}

#ifdef DEBUG
// Some fields aren't used in delazification, given the target functions and
// scripts are already instantiated, but they still should match.
static void AssertDelazificationFieldsMatch(
    const BaseCompilationStencil& stencil, CompilationGCOutput& gcOutput) {
  for (auto item :
       CompilationStencil::functionScriptStencils(stencil, gcOutput)) {
    auto& scriptStencil = item.script;
    auto* scriptExtra = item.scriptExtra;
    auto& fun = item.function;

    MOZ_ASSERT(scriptExtra == nullptr);

    // Names are updated by UpdateInnerFunctions.
    constexpr uint16_t HAS_INFERRED_NAME =
        uint16_t(FunctionFlags::Flags::HAS_INFERRED_NAME);
    constexpr uint16_t HAS_GUESSED_ATOM =
        uint16_t(FunctionFlags::Flags::HAS_GUESSED_ATOM);
    constexpr uint16_t MUTABLE_FLAGS =
        uint16_t(FunctionFlags::Flags::MUTABLE_FLAGS);
    constexpr uint16_t acceptableDifferenceForFunction =
        HAS_INFERRED_NAME | HAS_GUESSED_ATOM | MUTABLE_FLAGS;

    MOZ_ASSERT((fun->flags().toRaw() | acceptableDifferenceForFunction) ==
               (scriptStencil.functionFlags.toRaw() |
                acceptableDifferenceForFunction));

    // Delazification shouldn't delazify inner scripts.
    MOZ_ASSERT_IF(item.index == CompilationStencil::TopLevelIndex,
                  scriptStencil.hasSharedData());
    MOZ_ASSERT_IF(item.index > CompilationStencil::TopLevelIndex,
                  !scriptStencil.hasSharedData());
  }
}
#endif  // DEBUG

// When delazifying, use the existing JSFunctions. The initial and delazifying
// parse are required to generate the same sequence of functions for lazy
// parsing to work at all.
static void FunctionsFromExistingLazy(CompilationInput& input,
                                      CompilationGCOutput& gcOutput) {
  MOZ_ASSERT(gcOutput.functions.empty());
  gcOutput.functions.infallibleAppend(input.lazy->function());

  for (JS::GCCellPtr elem : input.lazy->gcthings()) {
    if (!elem.is<JSObject>()) {
      continue;
    }
    JSFunction* fun = &elem.as<JSObject>().as<JSFunction>();
    gcOutput.functions.infallibleAppend(fun);
  }
}

/* static */
bool CompilationStencil::instantiateStencils(
    JSContext* cx, CompilationInput& input, const CompilationStencil& stencil,
    CompilationGCOutput& gcOutput,
    CompilationGCOutput* gcOutputForDelazification) {
  if (!prepareForInstantiate(cx, input, stencil, gcOutput,
                             gcOutputForDelazification)) {
    return false;
  }

  if (!instantiateBaseStencilAfterPreparation(cx, input, stencil, gcOutput)) {
    return false;
  }

  if (stencil.delazificationSet) {
    MOZ_ASSERT(gcOutputForDelazification);

    CompilationAtomCache::AtomCacheVector reusableAtomCache;
    input.atomCache.releaseBuffer(reusableAtomCache);

    size_t numDelazifications =
        stencil.delazificationSet->delazifications.length();
    for (size_t i = 0; i < numDelazifications; i++) {
      auto& delazification = stencil.delazificationSet->delazifications[i];
      auto index = stencil.delazificationSet->delazificationIndices[i];

      JSFunction* fun = gcOutput.functions[index];
      MOZ_ASSERT(fun);

      BaseScript* lazy = fun->baseScript();
      MOZ_ASSERT(!lazy->hasBytecode());

      if (!lazy->isReadyForDelazification()) {
        MOZ_ASSERT(false, "Delazification target is not ready. Bad XDR?");
        continue;
      }

      Rooted<CompilationInput> delazificationInput(
          cx, CompilationInput(input.options));
      delazificationInput.get().initFromLazy(lazy, input.source);

      delazificationInput.get().atomCache.stealBuffer(reusableAtomCache);

      if (!instantiateBaseStencilAfterPreparation(cx, delazificationInput.get(),
                                                  delazification,
                                                  *gcOutputForDelazification)) {
        return false;
      }

      // Destroy elements, without unreserving.
      gcOutputForDelazification->functions.clear();
      gcOutputForDelazification->scopes.clear();

      delazificationInput.get().atomCache.releaseBuffer(reusableAtomCache);
    }

    input.atomCache.stealBuffer(reusableAtomCache);
  }

  return true;
}

/* static */
bool CompilationStencil::instantiateBaseStencilAfterPreparation(
    JSContext* cx, CompilationInput& input,
    const BaseCompilationStencil& stencil, CompilationGCOutput& gcOutput) {
  // Distinguish between the initial (possibly lazy) compile and any subsequent
  // delazification compiles. Delazification will update existing GC things.
  bool isInitialParse = stencil.isInitialStencil();
  MOZ_ASSERT(stencil.isInitialStencil() == !input.lazy);

  // Phase 1: Instantate JSAtoms.
  if (!InstantiateAtoms(cx, input, stencil)) {
    return false;
  }

  // Phase 2: Instantiate ScriptSourceObject, ModuleObject, JSFunctions.
  if (isInitialParse) {
    const CompilationStencil& initialStencil = stencil.asCompilationStencil();

    if (!InstantiateScriptSourceObject(cx, input, initialStencil, gcOutput)) {
      return false;
    }

    if (initialStencil.moduleMetadata) {
      // The enclosing script of a module is always the global scope. Fetch the
      // scope of the current global and update input data.
      MOZ_ASSERT(input.enclosingScope == nullptr);
      input.enclosingScope = &cx->global()->emptyGlobalScope();
      MOZ_ASSERT(input.enclosingScope->environmentChainLength() ==
                 ModuleScope::EnclosingEnvironmentChainLength);

      if (!InstantiateModuleObject(cx, input, initialStencil, gcOutput)) {
        return false;
      }
    }

    if (!InstantiateFunctions(cx, input, stencil, gcOutput)) {
      return false;
    }
  } else {
    MOZ_ASSERT(
        stencil.scriptData[CompilationStencil::TopLevelIndex].isFunction());

    // FunctionKey is used when caching to map a delazification stencil to a
    // specific lazy script. It is not used by instantiation, but we should
    // ensure it is correctly defined.
    MOZ_ASSERT(stencil.functionKey ==
               BaseCompilationStencil::toFunctionKey(input.lazy->extent()));

    FunctionsFromExistingLazy(input, gcOutput);
    MOZ_ASSERT(gcOutput.functions.length() == stencil.scriptData.size());

#ifdef DEBUG
    AssertDelazificationFieldsMatch(stencil, gcOutput);
#endif
  }

  // Phase 3: Instantiate js::Scopes.
  if (!InstantiateScopes(cx, input, stencil, gcOutput)) {
    return false;
  }

  // Phase 4: Instantiate (inner) BaseScripts.
  if (isInitialParse) {
    if (!InstantiateScriptStencils(cx, input, stencil.asCompilationStencil(),
                                   gcOutput)) {
      return false;
    }
  }

  // Phase 5: Finish top-level handling
  if (!InstantiateTopLevel(cx, input, stencil, gcOutput)) {
    return false;
  }

  // !! Must be infallible from here forward !!

  // Phase 6: Update lazy scripts.
  if (CanLazilyParse(input.options)) {
    UpdateEmittedInnerFunctions(cx, input, stencil, gcOutput);

    if (isInitialParse) {
      LinkEnclosingLazyScript(stencil, gcOutput);
    }
  }

  return true;
}

bool StencilDelazificationSet::buildDelazificationIndices(
    JSContext* cx, const CompilationStencil& stencil) {
  // Standalone-functions are not supported by XDR.
  MOZ_ASSERT(!stencil.scriptData[0].isFunction());

  MOZ_ASSERT(!delazifications.empty());
  MOZ_ASSERT(delazificationIndices.empty());

  if (!delazificationIndices.resize(delazifications.length())) {
    ReportOutOfMemory(cx);
    return false;
  }

  HashMap<BaseCompilationStencil::FunctionKey, size_t> keyToIndex(cx);
  if (!keyToIndex.reserve(delazifications.length())) {
    return false;
  }

  for (size_t i = 0; i < delazifications.length(); i++) {
    const auto& delazification = delazifications[i];
    auto key = delazification.functionKey;
    keyToIndex.putNewInfallible(key, i);

    if (maxScriptDataLength < delazification.scriptData.size()) {
      maxScriptDataLength = delazification.scriptData.size();
    }
    if (maxScopeDataLength < delazification.scopeData.size()) {
      maxScopeDataLength = delazification.scopeData.size();
    }
    if (maxParserAtomDataLength < delazification.parserAtomData.size()) {
      maxParserAtomDataLength = delazification.parserAtomData.size();
    }
  }

  MOZ_ASSERT(keyToIndex.count() == delazifications.length());

  for (size_t i = 1; i < stencil.scriptData.size(); i++) {
    auto key =
        BaseCompilationStencil::toFunctionKey(stencil.scriptExtra[i].extent);
    auto ptr = keyToIndex.lookup(key);
    if (!ptr) {
      continue;
    }
    delazificationIndices[ptr->value()] = ScriptIndex(i);
  }

  return true;
}

/* static */
bool CompilationStencil::prepareForInstantiate(
    JSContext* cx, CompilationInput& input, const CompilationStencil& stencil,
    CompilationGCOutput& gcOutput,
    CompilationGCOutput* gcOutputForDelazification) {
  size_t maxParserAtomDataLength = stencil.parserAtomData.size();

  // Reserve the `gcOutput` vectors.
  if (!gcOutput.ensureReserved(cx, stencil.scriptData.size(),
                               stencil.scopeData.size())) {
    return false;
  }

  // Reserve the `gcOutputForDelazification` vectors.
  if (auto* data = stencil.delazificationSet.get()) {
    MOZ_ASSERT(data->hasDelazificationIndices());
    MOZ_ASSERT(gcOutputForDelazification);

    if (!gcOutputForDelazification->ensureReserved(
            cx, data->maxScriptDataLength, data->maxScopeDataLength)) {
      return false;
    }

    if (data->maxParserAtomDataLength > maxParserAtomDataLength) {
      maxParserAtomDataLength = data->maxParserAtomDataLength;
    }
  }

  // The `atomCache` is used for the base and delazification stencils.
  return input.atomCache.allocate(cx, maxParserAtomDataLength);
}

bool CompilationStencil::serializeStencils(JSContext* cx,
                                           CompilationInput& input,
                                           JS::TranscodeBuffer& buf,
                                           bool* succeededOut) const {
  if (succeededOut) {
    *succeededOut = false;
  }
  XDRIncrementalStencilEncoder encoder(cx);

  XDRResult res =
      encoder.codeStencil(input, const_cast<CompilationStencil&>(*this));
  if (res.isErr()) {
    if (JS::IsTranscodeFailureResult(res.unwrapErr())) {
      buf.clear();
      return true;
    }
    MOZ_ASSERT(res.unwrapErr() == JS::TranscodeResult::Throw);

    return false;
  }

  // Linearize the encoder, return empty buffer on failure.
  res = encoder.linearize(buf, source.get());
  if (res.isErr()) {
    MOZ_ASSERT(cx->isThrowingOutOfMemory());
    buf.clear();
    return false;
  }

  if (succeededOut) {
    *succeededOut = true;
  }
  return true;
}

bool CompilationStencil::deserializeStencils(JSContext* cx,
                                             CompilationInput& input,
                                             const JS::TranscodeRange& range,
                                             bool* succeededOut) {
  if (succeededOut) {
    *succeededOut = false;
  }
  MOZ_ASSERT(parserAtomData.empty());
  XDRStencilDecoder decoder(cx, &input.options, range);

  XDRResult res = decoder.codeStencils(input, *this);
  if (res.isErr()) {
    if (JS::IsTranscodeFailureResult(res.unwrapErr())) {
      return true;
    }
    MOZ_ASSERT(res.unwrapErr() == JS::TranscodeResult::Throw);

    return false;
  }

  if (succeededOut) {
    *succeededOut = true;
  }
  return true;
}

ExtensibleCompilationStencil::ExtensibleCompilationStencil(
    JSContext* cx, CompilationInput& input)
    : alloc(CompilationStencil::LifoAllocChunkSize),
      source(input.source),
      parserAtoms(cx->runtime(), alloc) {}

CompilationState::CompilationState(JSContext* cx,
                                   LifoAllocScope& frontendAllocScope,
                                   CompilationInput& input)
    : ExtensibleCompilationStencil(cx, input),
      directives(input.options.forceStrictMode()),
      usedNames(cx),
      allocScope(frontendAllocScope),
      input(input) {}

BorrowingCompilationStencil::BorrowingCompilationStencil(
    ExtensibleCompilationStencil& extensibleStencil)
    : CompilationStencil(extensibleStencil.source) {
  hasExternalDependency = true;

  functionKey = extensibleStencil.functionKey;

  // Borrow the vector content as span.
  scriptData = extensibleStencil.scriptData;
  gcThingData = extensibleStencil.gcThingData;

  scopeData = extensibleStencil.scopeData;
  scopeNames = extensibleStencil.scopeNames;

  regExpData = extensibleStencil.regExpData;
  bigIntData = extensibleStencil.bigIntData;
  objLiteralData = extensibleStencil.objLiteralData;

  scriptExtra = extensibleStencil.scriptExtra;

  // Borrow the parser atoms as span.
  parserAtomData = extensibleStencil.parserAtoms.entries_;

  // Share ref-counted data.
  source = extensibleStencil.source;
  asmJS = extensibleStencil.asmJS;
  moduleMetadata = extensibleStencil.moduleMetadata;

  // Borrow container.
  sharedData.setBorrow(&extensibleStencil.sharedData);
}

SharedDataContainer::~SharedDataContainer() {
  if (isEmpty()) {
    // Nothing to do.
  } else if (isSingle()) {
    asSingle()->Release();
  } else if (isVector()) {
    js_delete(asVector());
  } else if (isMap()) {
    js_delete(asMap());
  } else {
    MOZ_ASSERT(isBorrow());
    // Nothing to do.
  }
}

bool SharedDataContainer::initVector(JSContext* cx) {
  MOZ_ASSERT(isEmpty());

  auto* vec = js_new<SharedDataVector>();
  if (!vec) {
    ReportOutOfMemory(cx);
    return false;
  }
  data_ = uintptr_t(vec) | VectorTag;
  return true;
}

bool SharedDataContainer::initMap(JSContext* cx) {
  MOZ_ASSERT(isEmpty());

  auto* map = js_new<SharedDataMap>();
  if (!map) {
    ReportOutOfMemory(cx);
    return false;
  }
  data_ = uintptr_t(map) | MapTag;
  return true;
}

bool SharedDataContainer::prepareStorageFor(JSContext* cx,
                                            size_t nonLazyScriptCount,
                                            size_t allScriptCount) {
  MOZ_ASSERT(isEmpty());

  if (nonLazyScriptCount <= 1) {
    MOZ_ASSERT(isSingle());
    return true;
  }

  // If the ratio of scripts with bytecode is small, allocating the Vector
  // storage with the number of all scripts isn't space-efficient.
  // In that case use HashMap instead.
  //
  // In general, we expect either all scripts to contain bytecode (priviledge
  // and self-hosted), or almost none to (eg standard lazy parsing output).
  constexpr size_t thresholdRatio = 8;
  bool useHashMap = nonLazyScriptCount < allScriptCount / thresholdRatio;
  if (useHashMap) {
    if (!initMap(cx)) {
      return false;
    }
    if (!asMap()->reserve(nonLazyScriptCount)) {
      ReportOutOfMemory(cx);
      return false;
    }
  } else {
    if (!initVector(cx)) {
      return false;
    }
    if (!asVector()->resize(allScriptCount)) {
      ReportOutOfMemory(cx);
      return false;
    }
  }

  return true;
}

js::SharedImmutableScriptData* SharedDataContainer::get(
    ScriptIndex index) const {
  if (isSingle()) {
    if (index == CompilationStencil::TopLevelIndex) {
      return asSingle();
    }
    return nullptr;
  }

  if (isVector()) {
    auto& vec = *asVector();
    if (index.index < vec.length()) {
      return vec[index];
    }
    return nullptr;
  }

  if (isMap()) {
    auto& map = *asMap();
    auto p = map.lookup(index);
    if (p) {
      return p->value();
    }
    return nullptr;
  }

  MOZ_ASSERT(isBorrow());
  return asBorrow()->get(index);
}

bool SharedDataContainer::addAndShare(JSContext* cx, ScriptIndex index,
                                      js::SharedImmutableScriptData* data) {
  MOZ_ASSERT(!isBorrow());

  if (isSingle()) {
    MOZ_ASSERT(index == CompilationStencil::TopLevelIndex);
    RefPtr<SharedImmutableScriptData> ref(data);
    if (!SharedImmutableScriptData::shareScriptData(cx, ref)) {
      return false;
    }
    setSingle(ref.forget());
    return true;
  }

  if (isVector()) {
    auto& vec = *asVector();
    // Resized by SharedDataContainer::prepareStorageFor.
    vec[index] = data;
    return SharedImmutableScriptData::shareScriptData(cx, vec[index]);
  }

  MOZ_ASSERT(isMap());
  auto& map = *asMap();
  // Reserved by SharedDataContainer::prepareStorageFor.
  map.putNewInfallible(index, data);
  auto p = map.lookup(index);
  MOZ_ASSERT(p);
  return SharedImmutableScriptData::shareScriptData(cx, p->value());
}

#ifdef DEBUG
void CompilationStencil::assertNoExternalDependency() const {
  MOZ_ASSERT_IF(!regExpData.empty(), alloc.contains(regExpData.data()));

  MOZ_ASSERT_IF(!bigIntData.empty(), alloc.contains(bigIntData.data()));
  for (const auto& data : bigIntData) {
    MOZ_ASSERT(data.isContainedIn(alloc));
  }

  MOZ_ASSERT_IF(!objLiteralData.empty(), alloc.contains(objLiteralData.data()));
  for (const auto& data : objLiteralData) {
    MOZ_ASSERT(data.isContainedIn(alloc));
  }

  MOZ_ASSERT_IF(!scriptData.empty(), alloc.contains(scriptData.data()));
  MOZ_ASSERT_IF(!scriptExtra.empty(), alloc.contains(scriptExtra.data()));

  MOZ_ASSERT_IF(!scopeData.empty(), alloc.contains(scopeData.data()));
  MOZ_ASSERT_IF(!scopeNames.empty(), alloc.contains(scopeNames.data()));
  for (const auto* data : scopeNames) {
    MOZ_ASSERT_IF(data, alloc.contains(data));
  }

  MOZ_ASSERT(!sharedData.isBorrow());

  MOZ_ASSERT_IF(!parserAtomData.empty(), alloc.contains(parserAtomData.data()));
  for (const auto* data : parserAtomData) {
    MOZ_ASSERT_IF(data, alloc.contains(data));
  }

  // NOTE: We don't recursively check delazificationSet, but just prohibit
  //       having them to be no-external-dependency.
  //       Will be fixed by bug 1687095 that removes delazificationSet field.
  MOZ_ASSERT(!delazificationSet);
}

void ExtensibleCompilationStencil::assertNoExternalDependency() const {
  for (const auto& data : bigIntData) {
    MOZ_ASSERT(data.isContainedIn(alloc));
  }

  for (const auto& data : objLiteralData) {
    MOZ_ASSERT(data.isContainedIn(alloc));
  }

  for (const auto* data : scopeNames) {
    MOZ_ASSERT_IF(data, alloc.contains(data));
  }

  MOZ_ASSERT(!sharedData.isBorrow());

  for (const auto* data : parserAtoms.entries()) {
    MOZ_ASSERT_IF(data, alloc.contains(data));
  }
}
#endif  // DEBUG

template <typename T, typename VectorT>
bool CopyVectorToSpan(JSContext* cx, LifoAlloc& alloc, mozilla::Span<T>& span,
                      VectorT& vec) {
  auto len = vec.length();
  if (len == 0) {
    return true;
  }

  auto* p = alloc.newArrayUninitialized<T>(len);
  if (!p) {
    js::ReportOutOfMemory(cx);
    return false;
  }
  span = mozilla::Span(p, len);
  memcpy(span.data(), vec.begin(), sizeof(T) * len);
  return true;
}

bool ExtensibleCompilationStencil::finish(JSContext* cx,
                                          CompilationStencil& stencil) {
#ifdef DEBUG
  assertNoExternalDependency();
#endif

  MOZ_ASSERT(stencil.alloc.isEmpty());
  stencil.alloc.steal(&alloc);

  stencil.functionKey = functionKey;

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.regExpData, regExpData)) {
    return false;
  }

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.bigIntData, bigIntData)) {
    return false;
  }

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.objLiteralData,
                        objLiteralData)) {
    return false;
  }

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.scriptData, scriptData)) {
    return false;
  }

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.scriptExtra, scriptExtra)) {
    return false;
  }

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.scopeData, scopeData)) {
    return false;
  }

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.scopeNames, scopeNames)) {
    return false;
  }

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.parserAtomData,
                        parserAtoms.entries())) {
    return false;
  }

  if (!CopyVectorToSpan(cx, stencil.alloc, stencil.gcThingData, gcThingData)) {
    return false;
  }

  stencil.asmJS = std::move(asmJS);

  stencil.moduleMetadata = std::move(moduleMetadata);

  stencil.sharedData = std::move(sharedData);

#ifdef DEBUG
  stencil.assertNoExternalDependency();
#endif

  return true;
}

mozilla::Span<TaggedScriptThingIndex> ScriptStencil::gcthings(
    const BaseCompilationStencil& stencil) const {
  return stencil.gcThingData.Subspan(gcThingsOffset, gcThingsLength);
}

[[nodiscard]] bool BigIntStencil::init(JSContext* cx, LifoAlloc& alloc,
                                       const Vector<char16_t, 32>& buf) {
#ifdef DEBUG
  // Assert we have no separators; if we have a separator then the algorithm
  // used in BigInt::literalIsZero will be incorrect.
  for (char16_t c : buf) {
    MOZ_ASSERT(c != '_');
  }
#endif
  size_t length = buf.length();
  char16_t* p = alloc.template newArrayUninitialized<char16_t>(length);
  if (!p) {
    ReportOutOfMemory(cx);
    return false;
  }
  mozilla::PodCopy(p, buf.begin(), length);
  source_ = mozilla::Span(p, length);
  return true;
}

#ifdef DEBUG
bool BigIntStencil::isContainedIn(const LifoAlloc& alloc) const {
  return alloc.contains(source_.data());
}
#endif

#if defined(DEBUG) || defined(JS_JITSPEW)

void frontend::DumpTaggedParserAtomIndex(
    js::JSONPrinter& json, TaggedParserAtomIndex taggedIndex,
    const BaseCompilationStencil* stencil) {
  if (taggedIndex.isParserAtomIndex()) {
    json.property("tag", "AtomIndex");
    auto index = taggedIndex.toParserAtomIndex();
    if (stencil && stencil->parserAtomData[index]) {
      GenericPrinter& out = json.beginStringProperty("atom");
      stencil->parserAtomData[index]->dumpCharsNoQuote(out);
      json.endString();
    } else {
      json.property("index", size_t(index));
    }
    return;
  }

  if (taggedIndex.isWellKnownAtomId()) {
    json.property("tag", "WellKnown");
    auto index = taggedIndex.toWellKnownAtomId();
    switch (index) {
      case WellKnownAtomId::empty:
        json.property("atom", "");
        break;

#  define CASE_(_, name, _2) case WellKnownAtomId::name:
        FOR_EACH_NONTINY_COMMON_PROPERTYNAME(CASE_)
#  undef CASE_

#  define CASE_(name, _) case WellKnownAtomId::name:
        JS_FOR_EACH_PROTOTYPE(CASE_)
#  undef CASE_

        {
          GenericPrinter& out = json.beginStringProperty("atom");
          ParserAtomsTable::dumpCharsNoQuote(out, index);
          json.endString();
          break;
        }

      default:
        // This includes tiny WellKnownAtomId atoms, which is invalid.
        json.property("index", size_t(index));
        break;
    }
    return;
  }

  if (taggedIndex.isLength1StaticParserString()) {
    json.property("tag", "Length1Static");
    auto index = taggedIndex.toLength1StaticParserString();
    GenericPrinter& out = json.beginStringProperty("atom");
    ParserAtomsTable::dumpCharsNoQuote(out, index);
    json.endString();
    return;
  }

  if (taggedIndex.isLength2StaticParserString()) {
    json.property("tag", "Length2Static");
    auto index = taggedIndex.toLength2StaticParserString();
    GenericPrinter& out = json.beginStringProperty("atom");
    ParserAtomsTable::dumpCharsNoQuote(out, index);
    json.endString();
    return;
  }

  MOZ_ASSERT(taggedIndex.isNull());
  json.property("tag", "null");
}

void frontend::DumpTaggedParserAtomIndexNoQuote(
    GenericPrinter& out, TaggedParserAtomIndex taggedIndex,
    const BaseCompilationStencil* stencil) {
  if (taggedIndex.isParserAtomIndex()) {
    auto index = taggedIndex.toParserAtomIndex();
    if (stencil && stencil->parserAtomData[index]) {
      stencil->parserAtomData[index]->dumpCharsNoQuote(out);
    } else {
      out.printf("AtomIndex#%zu", size_t(index));
    }
    return;
  }

  if (taggedIndex.isWellKnownAtomId()) {
    auto index = taggedIndex.toWellKnownAtomId();
    switch (index) {
      case WellKnownAtomId::empty:
        out.put("#<zero-length name>");
        break;

#  define CASE_(_, name, _2) case WellKnownAtomId::name:
        FOR_EACH_NONTINY_COMMON_PROPERTYNAME(CASE_)
#  undef CASE_

#  define CASE_(name, _) case WellKnownAtomId::name:
        JS_FOR_EACH_PROTOTYPE(CASE_)
#  undef CASE_

        {
          ParserAtomsTable::dumpCharsNoQuote(out, index);
          break;
        }

      default:
        // This includes tiny WellKnownAtomId atoms, which is invalid.
        out.printf("WellKnown#%zu", size_t(index));
        break;
    }
    return;
  }

  if (taggedIndex.isLength1StaticParserString()) {
    auto index = taggedIndex.toLength1StaticParserString();
    ParserAtomsTable::dumpCharsNoQuote(out, index);
    return;
  }

  if (taggedIndex.isLength2StaticParserString()) {
    auto index = taggedIndex.toLength2StaticParserString();
    ParserAtomsTable::dumpCharsNoQuote(out, index);
    return;
  }

  MOZ_ASSERT(taggedIndex.isNull());
  out.put("#<null name>");
}

void RegExpStencil::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr);
}

void RegExpStencil::dump(js::JSONPrinter& json,
                         const BaseCompilationStencil* stencil) const {
  json.beginObject();
  dumpFields(json, stencil);
  json.endObject();
}

void RegExpStencil::dumpFields(js::JSONPrinter& json,
                               const BaseCompilationStencil* stencil) const {
  json.beginObjectProperty("pattern");
  DumpTaggedParserAtomIndex(json, atom_, stencil);
  json.endObject();

  GenericPrinter& out = json.beginStringProperty("flags");

  if (flags().global()) {
    out.put("g");
  }
  if (flags().ignoreCase()) {
    out.put("i");
  }
  if (flags().multiline()) {
    out.put("m");
  }
  if (flags().dotAll()) {
    out.put("s");
  }
  if (flags().unicode()) {
    out.put("u");
  }
  if (flags().sticky()) {
    out.put("y");
  }

  json.endStringProperty();
}

void BigIntStencil::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void BigIntStencil::dump(js::JSONPrinter& json) const {
  GenericPrinter& out = json.beginString();
  dumpCharsNoQuote(out);
  json.endString();
}

void BigIntStencil::dumpCharsNoQuote(GenericPrinter& out) const {
  for (char16_t c : source_) {
    out.putChar(char(c));
  }
}

void ScopeStencil::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr, nullptr);
}

void ScopeStencil::dump(js::JSONPrinter& json,
                        const BaseParserScopeData* baseScopeData,
                        const BaseCompilationStencil* stencil) const {
  json.beginObject();
  dumpFields(json, baseScopeData, stencil);
  json.endObject();
}

void ScopeStencil::dumpFields(js::JSONPrinter& json,
                              const BaseParserScopeData* baseScopeData,
                              const BaseCompilationStencil* stencil) const {
  json.property("kind", ScopeKindString(kind_));

  if (hasEnclosing()) {
    json.formatProperty("enclosing", "ScopeIndex(%zu)", size_t(enclosing()));
  }

  json.property("firstFrameSlot", firstFrameSlot_);

  if (hasEnvironmentShape()) {
    json.formatProperty("numEnvironmentSlots", "%zu",
                        size_t(numEnvironmentSlots_));
  }

  if (isFunction()) {
    json.formatProperty("functionIndex", "ScriptIndex(%zu)",
                        size_t(functionIndex_));
  }

  json.beginListProperty("flags");
  if (flags_ & HasEnclosing) {
    json.value("HasEnclosing");
  }
  if (flags_ & HasEnvironmentShape) {
    json.value("HasEnvironmentShape");
  }
  if (flags_ & IsArrow) {
    json.value("IsArrow");
  }
  json.endList();

  if (!baseScopeData) {
    return;
  }

  json.beginObjectProperty("data");

  const AbstractTrailingNamesArray<TaggedParserAtomIndex>* trailingNames =
      nullptr;
  uint32_t length = 0;

  switch (kind_) {
    case ScopeKind::Function: {
      const auto* data =
          static_cast<const FunctionScope::ParserData*>(baseScopeData);
      json.property("nextFrameSlot", data->slotInfo.nextFrameSlot);
      json.property("hasParameterExprs", data->slotInfo.hasParameterExprs());
      json.property("nonPositionalFormalStart",
                    data->slotInfo.nonPositionalFormalStart);
      json.property("varStart", data->slotInfo.varStart);

      trailingNames = &data->trailingNames;
      length = data->slotInfo.length;
      break;
    }

    case ScopeKind::FunctionBodyVar: {
      const auto* data =
          static_cast<const VarScope::ParserData*>(baseScopeData);
      json.property("nextFrameSlot", data->slotInfo.nextFrameSlot);

      trailingNames = &data->trailingNames;
      length = data->slotInfo.length;
      break;
    }

    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical:
    case ScopeKind::ClassBody: {
      const auto* data =
          static_cast<const LexicalScope::ParserData*>(baseScopeData);
      json.property("nextFrameSlot", data->slotInfo.nextFrameSlot);
      json.property("constStart", data->slotInfo.constStart);

      trailingNames = &data->trailingNames;
      length = data->slotInfo.length;
      break;
    }

    case ScopeKind::With: {
      break;
    }

    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      const auto* data =
          static_cast<const EvalScope::ParserData*>(baseScopeData);
      json.property("nextFrameSlot", data->slotInfo.nextFrameSlot);

      trailingNames = &data->trailingNames;
      length = data->slotInfo.length;
      break;
    }

    case ScopeKind::Global:
    case ScopeKind::NonSyntactic: {
      const auto* data =
          static_cast<const GlobalScope::ParserData*>(baseScopeData);
      json.property("letStart", data->slotInfo.letStart);
      json.property("constStart", data->slotInfo.constStart);

      trailingNames = &data->trailingNames;
      length = data->slotInfo.length;
      break;
    }

    case ScopeKind::Module: {
      const auto* data =
          static_cast<const ModuleScope::ParserData*>(baseScopeData);
      json.property("nextFrameSlot", data->slotInfo.nextFrameSlot);
      json.property("varStart", data->slotInfo.varStart);
      json.property("letStart", data->slotInfo.letStart);
      json.property("constStart", data->slotInfo.constStart);

      trailingNames = &data->trailingNames;
      length = data->slotInfo.length;
      break;
    }

    case ScopeKind::WasmInstance: {
      const auto* data =
          static_cast<const WasmInstanceScope::ParserData*>(baseScopeData);
      json.property("nextFrameSlot", data->slotInfo.nextFrameSlot);
      json.property("globalsStart", data->slotInfo.globalsStart);

      trailingNames = &data->trailingNames;
      length = data->slotInfo.length;
      break;
    }

    case ScopeKind::WasmFunction: {
      const auto* data =
          static_cast<const WasmFunctionScope::ParserData*>(baseScopeData);
      json.property("nextFrameSlot", data->slotInfo.nextFrameSlot);

      trailingNames = &data->trailingNames;
      length = data->slotInfo.length;
      break;
    }

    default: {
      MOZ_CRASH("Unexpected ScopeKind");
      break;
    }
  }

  if (trailingNames) {
    char index[64];
    json.beginObjectProperty("trailingNames");
    for (size_t i = 0; i < length; i++) {
      const auto& name = (*trailingNames)[i];
      SprintfLiteral(index, "%zu", i);
      json.beginObjectProperty(index);

      json.boolProperty("closedOver", name.closedOver());

      json.boolProperty("isTopLevelFunction", name.isTopLevelFunction());

      json.beginObjectProperty("name");
      DumpTaggedParserAtomIndex(json, name.name(), stencil);
      json.endObject();

      json.endObject();
    }
    json.endObject();
  }

  json.endObject();
}

static void DumpModuleEntryVectorItems(
    js::JSONPrinter& json, const StencilModuleMetadata::EntryVector& entries,
    const BaseCompilationStencil* stencil) {
  for (const auto& entry : entries) {
    json.beginObject();
    if (entry.specifier) {
      json.beginObjectProperty("specifier");
      DumpTaggedParserAtomIndex(json, entry.specifier, stencil);
      json.endObject();
    }
    if (entry.localName) {
      json.beginObjectProperty("localName");
      DumpTaggedParserAtomIndex(json, entry.localName, stencil);
      json.endObject();
    }
    if (entry.importName) {
      json.beginObjectProperty("importName");
      DumpTaggedParserAtomIndex(json, entry.importName, stencil);
      json.endObject();
    }
    if (entry.exportName) {
      json.beginObjectProperty("exportName");
      DumpTaggedParserAtomIndex(json, entry.exportName, stencil);
      json.endObject();
    }
    json.endObject();
  }
}

void StencilModuleMetadata::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr);
}

void StencilModuleMetadata::dump(js::JSONPrinter& json,
                                 const BaseCompilationStencil* stencil) const {
  json.beginObject();
  dumpFields(json, stencil);
  json.endObject();
}

void StencilModuleMetadata::dumpFields(
    js::JSONPrinter& json, const BaseCompilationStencil* stencil) const {
  json.beginListProperty("requestedModules");
  DumpModuleEntryVectorItems(json, requestedModules, stencil);
  json.endList();

  json.beginListProperty("importEntries");
  DumpModuleEntryVectorItems(json, importEntries, stencil);
  json.endList();

  json.beginListProperty("localExportEntries");
  DumpModuleEntryVectorItems(json, localExportEntries, stencil);
  json.endList();

  json.beginListProperty("indirectExportEntries");
  DumpModuleEntryVectorItems(json, indirectExportEntries, stencil);
  json.endList();

  json.beginListProperty("starExportEntries");
  DumpModuleEntryVectorItems(json, starExportEntries, stencil);
  json.endList();

  json.beginListProperty("functionDecls");
  for (const auto& index : functionDecls) {
    json.value("ScriptIndex(%zu)", size_t(index));
  }
  json.endList();

  json.boolProperty("isAsync", isAsync);
}

static void DumpImmutableScriptFlags(js::JSONPrinter& json,
                                     ImmutableScriptFlags immutableFlags) {
  for (uint32_t i = 1; i; i = i << 1) {
    if (uint32_t(immutableFlags) & i) {
      switch (ImmutableScriptFlagsEnum(i)) {
        case ImmutableScriptFlagsEnum::IsForEval:
          json.value("IsForEval");
          break;
        case ImmutableScriptFlagsEnum::IsModule:
          json.value("IsModule");
          break;
        case ImmutableScriptFlagsEnum::IsFunction:
          json.value("IsFunction");
          break;
        case ImmutableScriptFlagsEnum::SelfHosted:
          json.value("SelfHosted");
          break;
        case ImmutableScriptFlagsEnum::ForceStrict:
          json.value("ForceStrict");
          break;
        case ImmutableScriptFlagsEnum::HasNonSyntacticScope:
          json.value("HasNonSyntacticScope");
          break;
        case ImmutableScriptFlagsEnum::NoScriptRval:
          json.value("NoScriptRval");
          break;
        case ImmutableScriptFlagsEnum::TreatAsRunOnce:
          json.value("TreatAsRunOnce");
          break;
        case ImmutableScriptFlagsEnum::Strict:
          json.value("Strict");
          break;
        case ImmutableScriptFlagsEnum::HasModuleGoal:
          json.value("HasModuleGoal");
          break;
        case ImmutableScriptFlagsEnum::HasInnerFunctions:
          json.value("HasInnerFunctions");
          break;
        case ImmutableScriptFlagsEnum::HasDirectEval:
          json.value("HasDirectEval");
          break;
        case ImmutableScriptFlagsEnum::BindingsAccessedDynamically:
          json.value("BindingsAccessedDynamically");
          break;
        case ImmutableScriptFlagsEnum::HasCallSiteObj:
          json.value("HasCallSiteObj");
          break;
        case ImmutableScriptFlagsEnum::IsAsync:
          json.value("IsAsync");
          break;
        case ImmutableScriptFlagsEnum::IsGenerator:
          json.value("IsGenerator");
          break;
        case ImmutableScriptFlagsEnum::FunHasExtensibleScope:
          json.value("FunHasExtensibleScope");
          break;
        case ImmutableScriptFlagsEnum::FunctionHasThisBinding:
          json.value("FunctionHasThisBinding");
          break;
        case ImmutableScriptFlagsEnum::NeedsHomeObject:
          json.value("NeedsHomeObject");
          break;
        case ImmutableScriptFlagsEnum::IsDerivedClassConstructor:
          json.value("IsDerivedClassConstructor");
          break;
        case ImmutableScriptFlagsEnum::IsSyntheticFunction:
          json.value("IsSyntheticFunction");
          break;
        case ImmutableScriptFlagsEnum::UseMemberInitializers:
          json.value("UseMemberInitializers");
          break;
        case ImmutableScriptFlagsEnum::HasRest:
          json.value("HasRest");
          break;
        case ImmutableScriptFlagsEnum::NeedsFunctionEnvironmentObjects:
          json.value("NeedsFunctionEnvironmentObjects");
          break;
        case ImmutableScriptFlagsEnum::FunctionHasExtraBodyVarScope:
          json.value("FunctionHasExtraBodyVarScope");
          break;
        case ImmutableScriptFlagsEnum::ShouldDeclareArguments:
          json.value("ShouldDeclareArguments");
          break;
        case ImmutableScriptFlagsEnum::ArgumentsHasVarBinding:
          json.value("ArgumentsHasVarBinding");
          break;
        case ImmutableScriptFlagsEnum::AlwaysNeedsArgsObj:
          json.value("AlwaysNeedsArgsObj");
          break;
        case ImmutableScriptFlagsEnum::HasMappedArgsObj:
          json.value("HasMappedArgsObj");
          break;
        default:
          json.value("Unknown(%x)", i);
          break;
      }
    }
  }
}

static void DumpFunctionFlagsItems(js::JSONPrinter& json,
                                   FunctionFlags functionFlags) {
  switch (functionFlags.kind()) {
    case FunctionFlags::FunctionKind::NormalFunction:
      json.value("NORMAL_KIND");
      break;
    case FunctionFlags::FunctionKind::AsmJS:
      json.value("ASMJS_KIND");
      break;
    case FunctionFlags::FunctionKind::Wasm:
      json.value("WASM_KIND");
      break;
    case FunctionFlags::FunctionKind::Arrow:
      json.value("ARROW_KIND");
      break;
    case FunctionFlags::FunctionKind::Method:
      json.value("METHOD_KIND");
      break;
    case FunctionFlags::FunctionKind::ClassConstructor:
      json.value("CLASSCONSTRUCTOR_KIND");
      break;
    case FunctionFlags::FunctionKind::Getter:
      json.value("GETTER_KIND");
      break;
    case FunctionFlags::FunctionKind::Setter:
      json.value("SETTER_KIND");
      break;
    default:
      json.value("Unknown(%x)", uint8_t(functionFlags.kind()));
      break;
  }

  static_assert(FunctionFlags::FUNCTION_KIND_MASK == 0x0007,
                "FunctionKind should use the lowest 3 bits");
  for (uint16_t i = 1 << 3; i; i = i << 1) {
    if (functionFlags.toRaw() & i) {
      switch (FunctionFlags::Flags(i)) {
        case FunctionFlags::Flags::EXTENDED:
          json.value("EXTENDED");
          break;
        case FunctionFlags::Flags::SELF_HOSTED:
          json.value("SELF_HOSTED");
          break;
        case FunctionFlags::Flags::BASESCRIPT:
          json.value("BASESCRIPT");
          break;
        case FunctionFlags::Flags::SELFHOSTLAZY:
          json.value("SELFHOSTLAZY");
          break;
        case FunctionFlags::Flags::CONSTRUCTOR:
          json.value("CONSTRUCTOR");
          break;
        case FunctionFlags::Flags::BOUND_FUN:
          json.value("BOUND_FUN");
          break;
        case FunctionFlags::Flags::LAMBDA:
          json.value("LAMBDA");
          break;
        case FunctionFlags::Flags::WASM_JIT_ENTRY:
          json.value("WASM_JIT_ENTRY");
          break;
        case FunctionFlags::Flags::HAS_INFERRED_NAME:
          json.value("HAS_INFERRED_NAME");
          break;
        case FunctionFlags::Flags::ATOM_EXTRA_FLAG:
          json.value("ATOM_EXTRA_FLAG");
          break;
        case FunctionFlags::Flags::RESOLVED_NAME:
          json.value("RESOLVED_NAME");
          break;
        case FunctionFlags::Flags::RESOLVED_LENGTH:
          json.value("RESOLVED_LENGTH");
          break;
        default:
          json.value("Unknown(%x)", i);
          break;
      }
    }
  }
}

static void DumpScriptThing(js::JSONPrinter& json,
                            const BaseCompilationStencil* stencil,
                            TaggedScriptThingIndex thing) {
  switch (thing.tag()) {
    case TaggedScriptThingIndex::Kind::ParserAtomIndex:
    case TaggedScriptThingIndex::Kind::WellKnown:
      json.beginObject();
      json.property("type", "Atom");
      DumpTaggedParserAtomIndex(json, thing.toAtom(), stencil);
      json.endObject();
      break;
    case TaggedScriptThingIndex::Kind::Null:
      json.nullValue();
      break;
    case TaggedScriptThingIndex::Kind::BigInt:
      json.value("BigIntIndex(%zu)", size_t(thing.toBigInt()));
      break;
    case TaggedScriptThingIndex::Kind::ObjLiteral:
      json.value("ObjLiteralIndex(%zu)", size_t(thing.toObjLiteral()));
      break;
    case TaggedScriptThingIndex::Kind::RegExp:
      json.value("RegExpIndex(%zu)", size_t(thing.toRegExp()));
      break;
    case TaggedScriptThingIndex::Kind::Scope:
      json.value("ScopeIndex(%zu)", size_t(thing.toScope()));
      break;
    case TaggedScriptThingIndex::Kind::Function:
      json.value("ScriptIndex(%zu)", size_t(thing.toFunction()));
      break;
    case TaggedScriptThingIndex::Kind::EmptyGlobalScope:
      json.value("EmptyGlobalScope");
      break;
  }
}

void ScriptStencil::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json, nullptr);
}

void ScriptStencil::dump(js::JSONPrinter& json,
                         const BaseCompilationStencil* stencil) const {
  json.beginObject();
  dumpFields(json, stencil);
  json.endObject();
}

void ScriptStencil::dumpFields(js::JSONPrinter& json,
                               const BaseCompilationStencil* stencil) const {
  json.formatProperty("gcThingsOffset", "CompilationGCThingIndex(%u)",
                      gcThingsOffset.index);
  json.property("gcThingsLength", gcThingsLength);

  if (stencil) {
    json.beginListProperty("gcThings");
    for (const auto& thing : gcthings(*stencil)) {
      DumpScriptThing(json, stencil, thing);
    }
    json.endList();
  }

  json.beginListProperty("flags");
  if (flags_ & WasEmittedByEnclosingScriptFlag) {
    json.value("WasEmittedByEnclosingScriptFlag");
  }
  if (flags_ & AllowRelazifyFlag) {
    json.value("AllowRelazifyFlag");
  }
  if (flags_ & HasSharedDataFlag) {
    json.value("HasSharedDataFlag");
  }
  if (flags_ & HasLazyFunctionEnclosingScopeIndexFlag) {
    json.value("HasLazyFunctionEnclosingScopeIndexFlag");
  }
  json.endList();

  if (isFunction()) {
    json.beginObjectProperty("functionAtom");
    DumpTaggedParserAtomIndex(json, functionAtom, stencil);
    json.endObject();

    json.beginListProperty("functionFlags");
    DumpFunctionFlagsItems(json, functionFlags);
    json.endList();

    if (hasLazyFunctionEnclosingScopeIndex()) {
      json.formatProperty("lazyFunctionEnclosingScopeIndex", "ScopeIndex(%zu)",
                          size_t(lazyFunctionEnclosingScopeIndex_));
    }
  }
}

void ScriptStencilExtra::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void ScriptStencilExtra::dump(js::JSONPrinter& json) const {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void ScriptStencilExtra::dumpFields(js::JSONPrinter& json) const {
  json.beginListProperty("immutableFlags");
  DumpImmutableScriptFlags(json, immutableFlags);
  json.endList();

  json.beginObjectProperty("extent");
  json.property("sourceStart", extent.sourceStart);
  json.property("sourceEnd", extent.sourceEnd);
  json.property("toStringStart", extent.toStringStart);
  json.property("toStringEnd", extent.toStringEnd);
  json.property("lineno", extent.lineno);
  json.property("column", extent.column);
  json.endObject();

  json.property("memberInitializers", memberInitializers_);

  json.property("nargs", nargs);
}

void SharedDataContainer::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
}

void SharedDataContainer::dump(js::JSONPrinter& json) const {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void SharedDataContainer::dumpFields(js::JSONPrinter& json) const {
  if (isEmpty()) {
    json.nullProperty("ScriptIndex(0)");
    return;
  }

  if (isSingle()) {
    json.formatProperty("ScriptIndex(0)", "u8[%zu]",
                        asSingle()->immutableDataLength());
    return;
  }

  if (isVector()) {
    auto& vec = *asVector();

    char index[64];
    for (size_t i = 0; i < vec.length(); i++) {
      SprintfLiteral(index, "ScriptIndex(%zu)", i);
      if (vec[i]) {
        json.formatProperty(index, "u8[%zu]", vec[i]->immutableDataLength());
      } else {
        json.nullProperty(index);
      }
    }
    return;
  }

  if (isMap()) {
    auto& map = *asMap();

    char index[64];
    for (auto iter = map.iter(); !iter.done(); iter.next()) {
      SprintfLiteral(index, "ScriptIndex(%u)", iter.get().key().index);
      json.formatProperty(index, "u8[%zu]",
                          iter.get().value()->immutableDataLength());
    }
    return;
  }

  MOZ_ASSERT(isBorrow());
  asBorrow()->dumpFields(json);
}

void BaseCompilationStencil::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
  out.put("\n");
}

void BaseCompilationStencil::dump(js::JSONPrinter& json) const {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void BaseCompilationStencil::dumpFields(js::JSONPrinter& json) const {
  char index[64];

  json.beginObjectProperty("scriptData");
  for (size_t i = 0; i < scriptData.size(); i++) {
    SprintfLiteral(index, "ScriptIndex(%zu)", i);
    json.beginObjectProperty(index);
    scriptData[i].dumpFields(json, this);
    json.endObject();
  }
  json.endObject();

  json.beginObjectProperty("regExpData");
  for (size_t i = 0; i < regExpData.size(); i++) {
    SprintfLiteral(index, "RegExpIndex(%zu)", i);
    json.beginObjectProperty(index);
    regExpData[i].dumpFields(json, this);
    json.endObject();
  }
  json.endObject();

  json.beginObjectProperty("bigIntData");
  for (size_t i = 0; i < bigIntData.size(); i++) {
    SprintfLiteral(index, "BigIntIndex(%zu)", i);
    GenericPrinter& out = json.beginStringProperty(index);
    bigIntData[i].dumpCharsNoQuote(out);
    json.endStringProperty();
  }
  json.endObject();

  json.beginObjectProperty("objLiteralData");
  for (size_t i = 0; i < objLiteralData.size(); i++) {
    SprintfLiteral(index, "ObjLiteralIndex(%zu)", i);
    json.beginObjectProperty(index);
    objLiteralData[i].dumpFields(json, this);
    json.endObject();
  }
  json.endObject();

  json.beginObjectProperty("scopeData");
  MOZ_ASSERT(scopeData.size() == scopeNames.size());
  for (size_t i = 0; i < scopeData.size(); i++) {
    SprintfLiteral(index, "ScopeIndex(%zu)", i);
    json.beginObjectProperty(index);
    scopeData[i].dumpFields(json, scopeNames[i], this);
    json.endObject();
  }
  json.endObject();

  json.beginObjectProperty("sharedData");
  sharedData.dumpFields(json);
  json.endObject();
}

void BaseCompilationStencil::dumpAtom(TaggedParserAtomIndex index) const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  json.beginObject();
  DumpTaggedParserAtomIndex(json, index, this);
  json.endObject();
}

void CompilationStencil::dump() const {
  js::Fprinter out(stderr);
  js::JSONPrinter json(out);
  dump(json);
  out.put("\n");
}

void CompilationStencil::dump(js::JSONPrinter& json) const {
  json.beginObject();
  dumpFields(json);
  json.endObject();
}

void CompilationStencil::dumpFields(js::JSONPrinter& json) const {
  BaseCompilationStencil::dumpFields(json);

  char index[64];
  json.beginObjectProperty("scriptExtra");
  for (size_t i = 0; i < scriptExtra.size(); i++) {
    SprintfLiteral(index, "ScriptIndex(%zu)", i);
    json.beginObjectProperty(index);
    scriptExtra[i].dumpFields(json);
    json.endObject();
  }
  json.endObject();

  if (moduleMetadata) {
    json.beginObjectProperty("moduleMetadata");
    moduleMetadata->dumpFields(json, this);
    json.endObject();
  }

  json.beginObjectProperty("asmJS");
  if (asmJS) {
    for (auto iter = asmJS->moduleMap.iter(); !iter.done(); iter.next()) {
      SprintfLiteral(index, "ScriptIndex(%u)", iter.get().key().index);
      json.formatProperty(index, "asm.js");
    }
  }
  json.endObject();
}

#endif  // defined(DEBUG) || defined(JS_JITSPEW)

JSAtom* CompilationAtomCache::getExistingAtomAt(ParserAtomIndex index) const {
  return atoms_[index];
}

JSAtom* CompilationAtomCache::getExistingAtomAt(
    JSContext* cx, TaggedParserAtomIndex taggedIndex) const {
  if (taggedIndex.isParserAtomIndex()) {
    auto index = taggedIndex.toParserAtomIndex();
    return getExistingAtomAt(index);
  }

  if (taggedIndex.isWellKnownAtomId()) {
    auto index = taggedIndex.toWellKnownAtomId();
    return GetWellKnownAtom(cx, index);
  }

  if (taggedIndex.isLength1StaticParserString()) {
    auto index = taggedIndex.toLength1StaticParserString();
    return cx->staticStrings().getUnit(char16_t(index));
  }

  MOZ_ASSERT(taggedIndex.isLength2StaticParserString());
  auto index = taggedIndex.toLength2StaticParserString();
  return cx->staticStrings().getLength2FromIndex(size_t(index));
}

JSAtom* CompilationAtomCache::getAtomAt(ParserAtomIndex index) const {
  if (size_t(index) >= atoms_.length()) {
    return nullptr;
  }
  return atoms_[index];
}

bool CompilationAtomCache::hasAtomAt(ParserAtomIndex index) const {
  if (size_t(index) >= atoms_.length()) {
    return false;
  }
  return !!atoms_[index];
}

bool CompilationAtomCache::setAtomAt(JSContext* cx, ParserAtomIndex index,
                                     JSAtom* atom) {
  if (size_t(index) < atoms_.length()) {
    atoms_[index] = atom;
    return true;
  }

  if (!atoms_.resize(size_t(index) + 1)) {
    ReportOutOfMemory(cx);
    return false;
  }

  atoms_[index] = atom;
  return true;
}

bool CompilationAtomCache::allocate(JSContext* cx, size_t length) {
  MOZ_ASSERT(length >= atoms_.length());
  if (length == atoms_.length()) {
    return true;
  }

  if (!atoms_.resize(length)) {
    ReportOutOfMemory(cx);
    return false;
  }

  return true;
}

void CompilationAtomCache::stealBuffer(AtomCacheVector& atoms) {
  atoms_ = std::move(atoms);
  // Destroy elements, without unreserving.
  atoms_.clear();
}

void CompilationAtomCache::releaseBuffer(AtomCacheVector& atoms) {
  atoms = std::move(atoms_);
}

bool CompilationState::allocateGCThingsUninitialized(
    JSContext* cx, ScriptIndex scriptIndex, size_t length,
    TaggedScriptThingIndex** cursor) {
  MOZ_ASSERT(gcThingData.length() <= UINT32_MAX);

  auto gcThingsOffset = CompilationGCThingIndex(gcThingData.length());

  if (length > INDEX_LIMIT) {
    ReportAllocationOverflow(cx);
    return false;
  }
  uint32_t gcThingsLength = length;

  if (!gcThingData.growByUninitialized(length)) {
    js::ReportOutOfMemory(cx);
    return false;
  }

  if (gcThingData.length() > UINT32_MAX) {
    ReportAllocationOverflow(cx);
    return false;
  }

  ScriptStencil& script = scriptData[scriptIndex];
  script.gcThingsOffset = gcThingsOffset;
  script.gcThingsLength = gcThingsLength;

  *cursor = gcThingData.begin() + gcThingsOffset;
  return true;
}

bool CompilationState::appendGCThings(
    JSContext* cx, ScriptIndex scriptIndex,
    mozilla::Span<const TaggedScriptThingIndex> things) {
  MOZ_ASSERT(gcThingData.length() <= UINT32_MAX);

  auto gcThingsOffset = CompilationGCThingIndex(gcThingData.length());

  if (things.size() > INDEX_LIMIT) {
    ReportAllocationOverflow(cx);
    return false;
  }
  uint32_t gcThingsLength = uint32_t(things.size());

  if (!gcThingData.append(things.data(), things.size())) {
    js::ReportOutOfMemory(cx);
    return false;
  }

  if (gcThingData.length() > UINT32_MAX) {
    ReportAllocationOverflow(cx);
    return false;
  }

  ScriptStencil& script = scriptData[scriptIndex];
  script.gcThingsOffset = gcThingsOffset;
  script.gcThingsLength = gcThingsLength;
  return true;
}

CompilationState::RewindToken CompilationState::getRewindToken() {
  return RewindToken{scriptData.length(), asmJS ? asmJS->moduleMap.count() : 0};
}

void CompilationState::rewind(const CompilationState::RewindToken& pos) {
  if (asmJS && asmJS->moduleMap.count() != pos.asmJSCount) {
    for (size_t i = pos.scriptDataLength; i < scriptData.length(); i++) {
      asmJS->moduleMap.remove(ScriptIndex(i));
    }
    MOZ_ASSERT(asmJS->moduleMap.count() == pos.asmJSCount);
  }
  scriptData.shrinkTo(pos.scriptDataLength);
}

void JS::StencilAddRef(JS::Stencil* stencil) { stencil->refCount++; }
void JS::StencilRelease(JS::Stencil* stencil) {
  MOZ_RELEASE_ASSERT(stencil->refCount > 0);
  stencil->refCount--;
  if (stencil->refCount == 0) {
    js_delete(stencil);
  }
}

already_AddRefed<JS::Stencil> JS::CompileGlobalScriptToStencil(
    JSContext* cx, const ReadOnlyCompileOptions& options,
    SourceText<mozilla::Utf8Unit>& srcBuf) {
  ScopeKind scopeKind =
      options.nonSyntacticScope ? ScopeKind::NonSyntactic : ScopeKind::Global;

  Rooted<CompilationInput> input(cx, CompilationInput(options));
  auto stencil = js::frontend::CompileGlobalScriptToStencil(cx, input.get(),
                                                            srcBuf, scopeKind);
  if (!stencil) {
    return nullptr;
  }

  // Convert the UniquePtr to a RefPtr and increment the count (to 1).
  return do_AddRef(stencil.release());
}

JSScript* JS::InstantiateGlobalStencil(
    JSContext* cx, const JS::ReadOnlyCompileOptions& options,
    RefPtr<JS::Stencil> stencil) {
  Rooted<CompilationInput> input(cx, CompilationInput(options));
  Rooted<CompilationGCOutput> gcOutput(cx);
  if (!InstantiateStencils(cx, input.get(), *stencil, gcOutput.get())) {
    return nullptr;
  }

  return gcOutput.get().script;
}

JS::TranscodeResult JS::EncodeStencil(JSContext* cx,
                                      const JS::ReadOnlyCompileOptions& options,
                                      RefPtr<JS::Stencil> stencil,
                                      TranscodeBuffer& buffer) {
  Rooted<CompilationInput> input(cx, CompilationInput(options));
  XDRIncrementalStencilEncoder encoder(cx);
  XDRResult res = encoder.codeStencil(input.get(), *stencil);
  if (res.isErr()) {
    return res.unwrapErr();
  }
  res = encoder.linearize(buffer, stencil->source.get());
  if (res.isErr()) {
    return res.unwrapErr();
  }
  return TranscodeResult::Ok;
}

JS::TranscodeResult JS::DecodeStencil(JSContext* cx,
                                      const JS::ReadOnlyCompileOptions& options,
                                      const JS::TranscodeRange& range,
                                      RefPtr<JS::Stencil>& stencilOut) {
  Rooted<CompilationInput> input(cx, CompilationInput(options));
  if (!input.get().initForGlobal(cx)) {
    return TranscodeResult::Throw;
  }
  UniquePtr<JS::Stencil> stencil(
      MakeUnique<CompilationStencil>(input.get().source));
  if (!stencil) {
    return TranscodeResult::Throw;
  }
  XDRStencilDecoder decoder(cx, &options, range);
  XDRResult res = decoder.codeStencils(input.get(), *stencil);
  if (res.isErr()) {
    return res.unwrapErr();
  }
  stencilOut = do_AddRef(stencil.release());
  return TranscodeResult::Ok;
}
