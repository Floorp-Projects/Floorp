/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "frontend/EmitterScope.h"

#include "frontend/AbstractScopePtr.h"
#include "frontend/BytecodeEmitter.h"
#include "frontend/ModuleSharedContext.h"
#include "frontend/TDZCheckCache.h"
#include "vm/GlobalObject.h"

using namespace js;
using namespace js::frontend;

using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::Nothing;
using mozilla::Some;

EmitterScope::EmitterScope(BytecodeEmitter* bce)
    : Nestable<EmitterScope>(&bce->innermostEmitterScope_),
      nameCache_(bce->cx->frontendCollectionPool()),
      hasEnvironment_(false),
      environmentChainLength_(0),
      nextFrameSlot_(0),
      scopeIndex_(ScopeNote::NoScopeIndex),
      noteIndex_(ScopeNote::NoScopeNoteIndex) {}

static inline void MarkAllBindingsClosedOver(LexicalScope::Data& data) {
  TrailingNamesArray& names = data.trailingNames;
  for (uint32_t i = 0; i < data.length; i++) {
    names[i] = BindingName(names[i].name(), true);
  }
}

bool EmitterScope::ensureCache(BytecodeEmitter* bce) {
  return nameCache_.acquire(bce->cx);
}

bool EmitterScope::checkSlotLimits(BytecodeEmitter* bce,
                                   const BindingIter& bi) {
  if (bi.nextFrameSlot() >= LOCALNO_LIMIT ||
      bi.nextEnvironmentSlot() >= ENVCOORD_SLOT_LIMIT) {
    bce->reportError(nullptr, JSMSG_TOO_MANY_LOCALS);
    return false;
  }
  return true;
}

bool EmitterScope::checkEnvironmentChainLength(BytecodeEmitter* bce) {
  uint32_t hops;
  if (EmitterScope* emitterScope = enclosing(&bce)) {
    hops = emitterScope->environmentChainLength_;
  } else {
    hops = bce->sc->compilationEnclosingScope()->environmentChainLength();
  }

  if (hops >= ENVCOORD_HOPS_LIMIT - 1) {
    bce->reportError(nullptr, JSMSG_TOO_DEEP, js_function_str);
    return false;
  }

  environmentChainLength_ = mozilla::AssertedCast<uint8_t>(hops + 1);
  return true;
}

void EmitterScope::updateFrameFixedSlots(BytecodeEmitter* bce,
                                         const BindingIter& bi) {
  nextFrameSlot_ = bi.nextFrameSlot();
  if (nextFrameSlot_ > bce->maxFixedSlots) {
    bce->maxFixedSlots = nextFrameSlot_;
  }
  MOZ_ASSERT_IF(
      bce->sc->isFunctionBox() && (bce->sc->asFunctionBox()->isGenerator() ||
                                   bce->sc->asFunctionBox()->isAsync()),
      bce->maxFixedSlots == 0);
}

bool EmitterScope::putNameInCache(BytecodeEmitter* bce, JSAtom* name,
                                  NameLocation loc) {
  NameLocationMap& cache = *nameCache_;
  NameLocationMap::AddPtr p = cache.lookupForAdd(name);
  MOZ_ASSERT(!p);
  if (!cache.add(p, name, loc)) {
    ReportOutOfMemory(bce->cx);
    return false;
  }
  return true;
}

Maybe<NameLocation> EmitterScope::lookupInCache(BytecodeEmitter* bce,
                                                JSAtom* name) {
  if (NameLocationMap::Ptr p = nameCache_->lookup(name)) {
    return Some(p->value().wrapped);
  }
  if (fallbackFreeNameLocation_ && nameCanBeFree(bce, name)) {
    return fallbackFreeNameLocation_;
  }
  return Nothing();
}

EmitterScope* EmitterScope::enclosing(BytecodeEmitter** bce) const {
  // There is an enclosing scope with access to the same frame.
  if (EmitterScope* inFrame = enclosingInFrame()) {
    return inFrame;
  }

  // We are currently compiling the enclosing script, look in the
  // enclosing BCE.
  if ((*bce)->parent) {
    *bce = (*bce)->parent;
    return (*bce)->innermostEmitterScopeNoCheck();
  }

  return nullptr;
}

AbstractScopePtr EmitterScope::enclosingScope(BytecodeEmitter* bce) const {
  if (EmitterScope* es = enclosing(&bce)) {
    return es->scope(bce);
  }

  // The enclosing script is already compiled or the current script is the
  // global script.
  return AbstractScopePtr(bce->sc->compilationEnclosingScope());
}

/* static */
bool EmitterScope::nameCanBeFree(BytecodeEmitter* bce, JSAtom* name) {
  // '.generator' cannot be accessed by name.
  return name != bce->cx->names().dotGenerator;
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
NameLocation EmitterScope::searchInEnclosingScope(JSAtom* name, Scope* scope,
                                                  uint8_t hops) {
  for (ScopeIter si(scope); si; si++) {
    MOZ_ASSERT(NameIsOnEnvironment(si.scope(), name));

    bool hasEnv = si.hasSyntacticEnvironment();

    switch (si.kind()) {
      case ScopeKind::Function:
        if (hasEnv) {
          JSScript* script = si.scope()->as<FunctionScope>().script();
          if (script->funHasExtensibleScope()) {
            return NameLocation::Dynamic();
          }

          for (BindingIter bi(si.scope()); bi; bi++) {
            if (bi.name() != name) {
              continue;
            }

            BindingLocation bindLoc = bi.location();
            if (bi.hasArgumentSlot() &&
                script->functionAllowsParameterRedeclaration()) {
              // Check for duplicate positional formal parameters.
              for (BindingIter bi2(bi); bi2 && bi2.hasArgumentSlot(); bi2++) {
                if (bi2.name() == name) {
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
        if (hasEnv) {
          for (BindingIter bi(si.scope()); bi; bi++) {
            if (bi.name() != name) {
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
        if (hasEnv) {
          for (BindingIter bi(si.scope()); bi; bi++) {
            if (bi.name() != name) {
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

NameLocation EmitterScope::searchAndCache(BytecodeEmitter* bce, JSAtom* name) {
  Maybe<NameLocation> loc;
  uint8_t hops = hasEnvironment() ? 1 : 0;
  DebugOnly<bool> inCurrentScript = enclosingInFrame();

  // Start searching in the current compilation.
  for (EmitterScope* es = enclosing(&bce); es; es = es->enclosing(&bce)) {
    loc = es->lookupInCache(bce, name);
    if (loc) {
      if (loc->kind() == NameLocation::Kind::EnvironmentCoordinate) {
        *loc = loc->addHops(hops);
      }
      break;
    }

    if (es->hasEnvironment()) {
      hops++;
    }

#ifdef DEBUG
    if (!es->enclosingInFrame()) {
      inCurrentScript = false;
    }
#endif
  }

  // If the name is not found in the current compilation, walk the Scope
  // chain encompassing the compilation.
  if (!loc) {
    inCurrentScript = false;
    loc = Some(searchInEnclosingScope(
        name, bce->sc->compilationEnclosingScope(), hops));
  }

  // Each script has its own frame. A free name that is accessed
  // from an inner script must not be a frame slot access. If this
  // assertion is hit, it is a bug in the free name analysis in the
  // parser.
  MOZ_ASSERT_IF(!inCurrentScript, loc->kind() != NameLocation::Kind::FrameSlot);

  // It is always correct to not cache the location. Ignore OOMs to make
  // lookups infallible.
  if (!putNameInCache(bce, name, *loc)) {
    bce->cx->recoverFromOutOfMemory();
  }

  return *loc;
}

bool EmitterScope::internEmptyGlobalScopeAsBody(BytecodeEmitter* bce) {
  Scope* scope = &bce->cx->global()->emptyGlobalScope();
  hasEnvironment_ = scope->hasEnvironment();

  bce->bodyScopeIndex = bce->perScriptData().gcThingList().length();
  return bce->perScriptData().gcThingList().appendEmptyGlobalScope(
      &scopeIndex_);
}

template <typename ScopeCreator>
bool EmitterScope::internScopeCreationData(BytecodeEmitter* bce,
                                           ScopeCreator createScope) {
  Rooted<AbstractScopePtr> enclosing(bce->cx, enclosingScope(bce));
  ScopeIndex index;
  if (!createScope(bce->cx, enclosing, &index)) {
    return false;
  }
  auto scope = bce->compilationInfo.scopeCreationData[index.index];
  hasEnvironment_ = scope.get().hasEnvironment();
  return bce->perScriptData().gcThingList().append(index, &scopeIndex_);
}

template <typename ScopeCreator>
bool EmitterScope::internBodyScopeCreationData(BytecodeEmitter* bce,
                                               ScopeCreator createScope) {
  MOZ_ASSERT(bce->bodyScopeIndex == UINT32_MAX,
             "There can be only one body scope");
  bce->bodyScopeIndex = bce->perScriptData().gcThingList().length();
  return internScopeCreationData(bce, createScope);
}

bool EmitterScope::appendScopeNote(BytecodeEmitter* bce) {
  MOZ_ASSERT(ScopeKindIsInBody(scope(bce).kind()) && enclosingInFrame(),
             "Scope notes are not needed for body-level scopes.");
  noteIndex_ = bce->bytecodeSection().scopeNoteList().length();
  return bce->bytecodeSection().scopeNoteList().append(
      index(), bce->bytecodeSection().offset(),
      enclosingInFrame() ? enclosingInFrame()->noteIndex()
                         : ScopeNote::NoScopeNoteIndex);
}

bool EmitterScope::deadZoneFrameSlotRange(BytecodeEmitter* bce,
                                          uint32_t slotStart,
                                          uint32_t slotEnd) const {
  // Lexical bindings throw ReferenceErrors if they are used before
  // initialization. See ES6 8.1.1.1.6.
  //
  // For completeness, lexical bindings are initialized in ES6 by calling
  // InitializeBinding, after which touching the binding will no longer
  // throw reference errors. See 13.1.11, 9.2.13, 13.6.3.4, 13.6.4.6,
  // 13.6.4.8, 13.14.5, 15.1.8, and 15.2.0.15.
  if (slotStart != slotEnd) {
    if (!bce->emit1(JSOp::Uninitialized)) {
      return false;
    }
    for (uint32_t slot = slotStart; slot < slotEnd; slot++) {
      if (!bce->emitLocalOp(JSOp::InitLexical, slot)) {
        return false;
      }
    }
    if (!bce->emit1(JSOp::Pop)) {
      return false;
    }
  }

  return true;
}

void EmitterScope::dump(BytecodeEmitter* bce) {
  fprintf(stdout, "EmitterScope [%s] %p\n", ScopeKindString(scope(bce).kind()),
          this);

  for (NameLocationMap::Range r = nameCache_->all(); !r.empty(); r.popFront()) {
    const NameLocation& l = r.front().value();

    UniqueChars bytes = AtomToPrintableString(bce->cx, r.front().key());
    if (!bytes) {
      return;
    }
    if (l.kind() != NameLocation::Kind::Dynamic) {
      fprintf(stdout, "  %s %s ", BindingKindString(l.bindingKind()),
              bytes.get());
    } else {
      fprintf(stdout, "  %s ", bytes.get());
    }

    switch (l.kind()) {
      case NameLocation::Kind::Dynamic:
        fprintf(stdout, "dynamic\n");
        break;
      case NameLocation::Kind::Global:
        fprintf(stdout, "global\n");
        break;
      case NameLocation::Kind::Intrinsic:
        fprintf(stdout, "intrinsic\n");
        break;
      case NameLocation::Kind::NamedLambdaCallee:
        fprintf(stdout, "named lambda callee\n");
        break;
      case NameLocation::Kind::Import:
        fprintf(stdout, "import\n");
        break;
      case NameLocation::Kind::ArgumentSlot:
        fprintf(stdout, "arg slot=%u\n", l.argumentSlot());
        break;
      case NameLocation::Kind::FrameSlot:
        fprintf(stdout, "frame slot=%u\n", l.frameSlot());
        break;
      case NameLocation::Kind::EnvironmentCoordinate:
        fprintf(stdout, "environment hops=%u slot=%u\n",
                l.environmentCoordinate().hops(),
                l.environmentCoordinate().slot());
        break;
      case NameLocation::Kind::DynamicAnnexBVar:
        fprintf(stdout, "dynamic annex b var\n");
        break;
    }
  }

  fprintf(stdout, "\n");
}

bool EmitterScope::enterLexical(BytecodeEmitter* bce, ScopeKind kind,
                                Handle<LexicalScope::Data*> bindings) {
  MOZ_ASSERT(kind != ScopeKind::NamedLambda &&
             kind != ScopeKind::StrictNamedLambda);
  MOZ_ASSERT(this == bce->innermostEmitterScopeNoCheck());

  if (!ensureCache(bce)) {
    return false;
  }

  // Marks all names as closed over if the context requires it. This
  // cannot be done in the Parser as we may not know if the context requires
  // all bindings to be closed over until after parsing is finished. For
  // example, legacy generators require all bindings to be closed over but
  // it is unknown if a function is a legacy generator until the first
  // 'yield' expression is parsed.
  //
  // This is not a problem with other scopes, as all other scopes with
  // bindings are body-level. At the time of their creation, whether or not
  // the context requires all bindings to be closed over is already known.
  if (bce->sc->allBindingsClosedOver()) {
    MarkAllBindingsClosedOver(*bindings);
  }

  // Resolve bindings.
  TDZCheckCache* tdzCache = bce->innermostTDZCheckCache;
  uint32_t firstFrameSlot = frameSlotStart();
  BindingIter bi(*bindings, firstFrameSlot, /* isNamedLambda = */ false);
  for (; bi; bi++) {
    if (!checkSlotLimits(bce, bi)) {
      return false;
    }

    NameLocation loc = NameLocation::fromBinding(bi.kind(), bi.location());
    if (!putNameInCache(bce, bi.name(), loc)) {
      return false;
    }

    if (!tdzCache->noteTDZCheck(bce, bi.name(), CheckTDZ)) {
      return false;
    }
  }

  updateFrameFixedSlots(bce, bi);

  auto createScope = [kind, bindings, firstFrameSlot, bce](
                         JSContext* cx, Handle<AbstractScopePtr> enclosing,
                         ScopeIndex* index) {
    return ScopeCreationData::create(cx, bce->compilationInfo, kind, bindings,
                                     firstFrameSlot, enclosing, index);
  };
  if (!internScopeCreationData(bce, createScope)) {
    return false;
  }

  if (ScopeKindIsInBody(kind) && hasEnvironment()) {
    // After interning the VM scope we can get the scope index.
    if (!bce->emitInternedScopeOp(index(), JSOp::PushLexicalEnv)) {
      return false;
    }
  }

  // Lexical scopes need notes to be mapped from a pc.
  if (!appendScopeNote(bce)) {
    return false;
  }

  // Put frame slots in TDZ. Environment slots are poisoned during
  // environment creation.
  //
  // This must be done after appendScopeNote to be considered in the extent
  // of the scope.
  if (!deadZoneFrameSlotRange(bce, firstFrameSlot, frameSlotEnd())) {
    return false;
  }

  return checkEnvironmentChainLength(bce);
}

bool EmitterScope::enterNamedLambda(BytecodeEmitter* bce, FunctionBox* funbox) {
  MOZ_ASSERT(this == bce->innermostEmitterScopeNoCheck());
  MOZ_ASSERT(funbox->namedLambdaBindings());

  if (!ensureCache(bce)) {
    return false;
  }

  // See comment in enterLexical about allBindingsClosedOver.
  if (funbox->allBindingsClosedOver()) {
    MarkAllBindingsClosedOver(*funbox->namedLambdaBindings());
  }

  BindingIter bi(*funbox->namedLambdaBindings(), LOCALNO_LIMIT,
                 /* isNamedLambda = */ true);
  MOZ_ASSERT(bi.kind() == BindingKind::NamedLambdaCallee);

  // The lambda name, if not closed over, is accessed via JSOp::Callee and
  // not a frame slot. Do not update frame slot information.
  NameLocation loc = NameLocation::fromBinding(bi.kind(), bi.location());
  if (!putNameInCache(bce, bi.name(), loc)) {
    return false;
  }

  bi++;
  MOZ_ASSERT(!bi, "There should be exactly one binding in a NamedLambda scope");

  ScopeKind scopeKind =
      funbox->strict() ? ScopeKind::StrictNamedLambda : ScopeKind::NamedLambda;

  auto createScope = [funbox, scopeKind, bce](
                         JSContext* cx, Handle<AbstractScopePtr> enclosing,
                         ScopeIndex* index) {
    return ScopeCreationData::create(cx, bce->compilationInfo, scopeKind,
                                     funbox->namedLambdaBindings(),
                                     LOCALNO_LIMIT, enclosing, index);
  };
  if (!internScopeCreationData(bce, createScope)) {
    return false;
  }

  return checkEnvironmentChainLength(bce);
}

bool EmitterScope::enterFunction(BytecodeEmitter* bce, FunctionBox* funbox) {
  MOZ_ASSERT(this == bce->innermostEmitterScopeNoCheck());

  // If there are parameter expressions, there is an extra var scope.
  if (!funbox->hasExtraBodyVarScope()) {
    bce->setVarEmitterScope(this);
  }

  if (!ensureCache(bce)) {
    return false;
  }

  // Resolve body-level bindings, if there are any.
  auto bindings = funbox->functionScopeBindings();
  if (bindings) {
    NameLocationMap& cache = *nameCache_;

    BindingIter bi(*bindings, funbox->hasParameterExprs);
    for (; bi; bi++) {
      if (!checkSlotLimits(bce, bi)) {
        return false;
      }

      NameLocation loc = NameLocation::fromBinding(bi.kind(), bi.location());
      NameLocationMap::AddPtr p = cache.lookupForAdd(bi.name());

      // The only duplicate bindings that occur are simple formal
      // parameters, in which case the last position counts, so update the
      // location.
      if (p) {
        MOZ_ASSERT(bi.kind() == BindingKind::FormalParameter);
        MOZ_ASSERT(!funbox->hasDestructuringArgs);
        MOZ_ASSERT(!funbox->hasRest());
        p->value() = loc;
        continue;
      }

      if (!cache.add(p, bi.name(), loc)) {
        ReportOutOfMemory(bce->cx);
        return false;
      }
    }

    updateFrameFixedSlots(bce, bi);
  } else {
    nextFrameSlot_ = 0;
  }

  // If the function's scope may be extended at runtime due to sloppy direct
  // eval, any names beyond the function scope must be accessed dynamically as
  // we don't know if the name will become a 'var' binding due to direct eval.
  if (funbox->hasExtensibleScope()) {
    fallbackFreeNameLocation_ = Some(NameLocation::Dynamic());
  }

  // In case of parameter expressions, the parameters are lexical
  // bindings and have TDZ.
  if (funbox->hasParameterExprs && nextFrameSlot_) {
    uint32_t paramFrameSlotEnd = 0;
    for (BindingIter bi(*bindings, true); bi; bi++) {
      if (!BindingKindIsLexical(bi.kind())) {
        break;
      }

      NameLocation loc = NameLocation::fromBinding(bi.kind(), bi.location());
      if (loc.kind() == NameLocation::Kind::FrameSlot) {
        MOZ_ASSERT(paramFrameSlotEnd <= loc.frameSlot());
        paramFrameSlotEnd = loc.frameSlot() + 1;
      }
    }

    if (!deadZoneFrameSlotRange(bce, 0, paramFrameSlotEnd)) {
      return false;
    }
  }

  auto createScope = [funbox, bce](JSContext* cx,
                                   Handle<AbstractScopePtr> enclosing,
                                   ScopeIndex* index) {
    return ScopeCreationData::create(
        cx, bce->compilationInfo, funbox->functionScopeBindings(),
        funbox->hasParameterExprs,
        funbox->needsCallObjectRegardlessOfBindings(), funbox, enclosing,
        index);
  };
  if (!internBodyScopeCreationData(bce, createScope)) {
    return false;
  }

  return checkEnvironmentChainLength(bce);
}

bool EmitterScope::enterFunctionExtraBodyVar(BytecodeEmitter* bce,
                                             FunctionBox* funbox) {
  MOZ_ASSERT(funbox->hasParameterExprs);
  MOZ_ASSERT(funbox->extraVarScopeBindings() ||
             funbox->needsExtraBodyVarEnvironmentRegardlessOfBindings());
  MOZ_ASSERT(this == bce->innermostEmitterScopeNoCheck());

  // The extra var scope is never popped once it's entered. It replaces the
  // function scope as the var emitter scope.
  bce->setVarEmitterScope(this);

  if (!ensureCache(bce)) {
    return false;
  }

  // Resolve body-level bindings, if there are any.
  uint32_t firstFrameSlot = frameSlotStart();
  if (auto bindings = funbox->extraVarScopeBindings()) {
    BindingIter bi(*bindings, firstFrameSlot);
    for (; bi; bi++) {
      if (!checkSlotLimits(bce, bi)) {
        return false;
      }

      NameLocation loc = NameLocation::fromBinding(bi.kind(), bi.location());
      if (!putNameInCache(bce, bi.name(), loc)) {
        return false;
      }
    }

    updateFrameFixedSlots(bce, bi);
  } else {
    nextFrameSlot_ = firstFrameSlot;
  }

  // If the extra var scope may be extended at runtime due to sloppy
  // direct eval, any names beyond the var scope must be accessed
  // dynamically as we don't know if the name will become a 'var' binding
  // due to direct eval.
  if (funbox->hasExtensibleScope()) {
    fallbackFreeNameLocation_ = Some(NameLocation::Dynamic());
  }

  // Create and intern the VM scope.
  auto createScope = [funbox, firstFrameSlot, bce](
                         JSContext* cx, Handle<AbstractScopePtr> enclosing,
                         ScopeIndex* index) {
    return ScopeCreationData::create(
        cx, bce->compilationInfo, ScopeKind::FunctionBodyVar,
        funbox->extraVarScopeBindings(), firstFrameSlot,
        funbox->needsExtraBodyVarEnvironmentRegardlessOfBindings(), enclosing,
        index);
  };
  if (!internScopeCreationData(bce, createScope)) {
    return false;
  }

  if (hasEnvironment()) {
    if (!bce->emitInternedScopeOp(index(), JSOp::PushVarEnv)) {
      return false;
    }
  }

  // The extra var scope needs a note to be mapped from a pc.
  if (!appendScopeNote(bce)) {
    return false;
  }

  return checkEnvironmentChainLength(bce);
}

class DynamicBindingIter : public BindingIter {
 public:
  explicit DynamicBindingIter(GlobalSharedContext* sc)
      : BindingIter(*sc->bindings) {}

  explicit DynamicBindingIter(EvalSharedContext* sc)
      : BindingIter(*sc->bindings, /* strict = */ false) {
    MOZ_ASSERT(!sc->strict());
  }

  JSOp bindingOp() const {
    switch (kind()) {
      case BindingKind::Var:
        return JSOp::DefVar;
      case BindingKind::Let:
        return JSOp::DefLet;
      case BindingKind::Const:
        return JSOp::DefConst;
      default:
        MOZ_CRASH("Bad BindingKind");
    }
  }
};

bool EmitterScope::enterGlobal(BytecodeEmitter* bce,
                               GlobalSharedContext* globalsc) {
  MOZ_ASSERT(this == bce->innermostEmitterScopeNoCheck());

  bce->setVarEmitterScope(this);

  if (!ensureCache(bce)) {
    return false;
  }

  if (bce->emitterMode == BytecodeEmitter::SelfHosting) {
    // In self-hosting, it is incorrect to consult the global scope because
    // self-hosted scripts are cloned into their target compartments before
    // they are run. Instead of Global, Intrinsic is used for all names.
    //
    // Intrinsic lookups are redirected to the special intrinsics holder
    // in the global object, into which any missing values are cloned
    // lazily upon first access.
    fallbackFreeNameLocation_ = Some(NameLocation::Intrinsic());

    return internEmptyGlobalScopeAsBody(bce);
  }

  // Resolve binding names and emit DEF{VAR,LET,CONST} prologue ops.
  if (globalsc->bindings) {
    for (DynamicBindingIter bi(globalsc); bi; bi++) {
      NameLocation loc = NameLocation::fromBinding(bi.kind(), bi.location());
      JSAtom* name = bi.name();
      if (!putNameInCache(bce, name, loc)) {
        return false;
      }

      // Define the name in the prologue. Do not emit DefVar for
      // functions that we'll emit DefFun for.
      if (bi.isTopLevelFunction()) {
        continue;
      }

      if (!bce->emitAtomOp(bi.bindingOp(), name)) {
        return false;
      }
    }
  }

  // Note that to save space, we don't add free names to the cache for
  // global scopes. They are assumed to be global vars in the syntactic
  // global scope, dynamic accesses under non-syntactic global scope.
  if (globalsc->scopeKind() == ScopeKind::Global) {
    fallbackFreeNameLocation_ = Some(NameLocation::Global(BindingKind::Var));
  } else {
    fallbackFreeNameLocation_ = Some(NameLocation::Dynamic());
  }

  auto createScope = [globalsc, bce](JSContext* cx,
                                     Handle<AbstractScopePtr> enclosing,
                                     ScopeIndex* index) {
    MOZ_ASSERT(!enclosing.get());
    return ScopeCreationData::create(cx, bce->compilationInfo,
                                     globalsc->scopeKind(), globalsc->bindings,
                                     index);
  };
  return internBodyScopeCreationData(bce, createScope);
}

bool EmitterScope::enterEval(BytecodeEmitter* bce, EvalSharedContext* evalsc) {
  MOZ_ASSERT(this == bce->innermostEmitterScopeNoCheck());

  bce->setVarEmitterScope(this);

  if (!ensureCache(bce)) {
    return false;
  }

  // For simplicity, treat all free name lookups in eval scripts as dynamic.
  fallbackFreeNameLocation_ = Some(NameLocation::Dynamic());

  // Create the `var` scope. Note that there is also a lexical scope, created
  // separately in emitScript().
  auto createScope = [evalsc, bce](JSContext* cx,
                                   Handle<AbstractScopePtr> enclosing,
                                   ScopeIndex* index) {
    ScopeKind scopeKind =
        evalsc->strict() ? ScopeKind::StrictEval : ScopeKind::Eval;
    return ScopeCreationData::create(cx, bce->compilationInfo, scopeKind,
                                     evalsc->bindings, enclosing, index);
  };
  if (!internBodyScopeCreationData(bce, createScope)) {
    return false;
  }

  if (hasEnvironment()) {
    if (!bce->emitInternedScopeOp(index(), JSOp::PushVarEnv)) {
      return false;
    }
  } else {
    // Resolve binding names and emit DefVar prologue ops if we don't have
    // an environment (i.e., a sloppy eval).
    // Eval scripts always have their own lexical scope, but non-strict
    // scopes may introduce 'var' bindings to the nearest var scope.
    //
    // TODO: We may optimize strict eval bindings in the future to be on
    // the frame. For now, handle everything dynamically.
    if (!hasEnvironment() && evalsc->bindings) {
      for (DynamicBindingIter bi(evalsc); bi; bi++) {
        MOZ_ASSERT(bi.bindingOp() == JSOp::DefVar);

        if (bi.isTopLevelFunction()) {
          continue;
        }

        if (!bce->emitAtomOp(JSOp::DefVar, bi.name())) {
          return false;
        }
      }
    }

    // As an optimization, if the eval does not have its own var
    // environment and is directly enclosed in a global scope, then all
    // free name lookups are global.
    if (scope(bce).enclosing().is<GlobalScope>()) {
      fallbackFreeNameLocation_ = Some(NameLocation::Global(BindingKind::Var));
    }
  }

  return true;
}

bool EmitterScope::enterModule(BytecodeEmitter* bce,
                               ModuleSharedContext* modulesc) {
  MOZ_ASSERT(this == bce->innermostEmitterScopeNoCheck());

  bce->setVarEmitterScope(this);

  if (!ensureCache(bce)) {
    return false;
  }

  // Resolve body-level bindings, if there are any.
  TDZCheckCache* tdzCache = bce->innermostTDZCheckCache;
  Maybe<uint32_t> firstLexicalFrameSlot;
  if (ModuleScope::Data* bindings = modulesc->bindings) {
    BindingIter bi(*bindings);
    for (; bi; bi++) {
      if (!checkSlotLimits(bce, bi)) {
        return false;
      }

      NameLocation loc = NameLocation::fromBinding(bi.kind(), bi.location());
      if (!putNameInCache(bce, bi.name(), loc)) {
        return false;
      }

      if (BindingKindIsLexical(bi.kind())) {
        if (loc.kind() == NameLocation::Kind::FrameSlot &&
            !firstLexicalFrameSlot) {
          firstLexicalFrameSlot = Some(loc.frameSlot());
        }

        if (!tdzCache->noteTDZCheck(bce, bi.name(), CheckTDZ)) {
          return false;
        }
      }
    }

    updateFrameFixedSlots(bce, bi);
  } else {
    nextFrameSlot_ = 0;
  }

  // Modules are toplevel, so any free names are global.
  fallbackFreeNameLocation_ = Some(NameLocation::Global(BindingKind::Var));

  // Put lexical frame slots in TDZ. Environment slots are poisoned during
  // environment creation.
  if (firstLexicalFrameSlot) {
    if (!deadZoneFrameSlotRange(bce, *firstLexicalFrameSlot, frameSlotEnd())) {
      return false;
    }
  }

  // Create and intern the VM scope creation data.
  auto createScope = [modulesc, bce](JSContext* cx,
                                     Handle<AbstractScopePtr> enclosing,
                                     ScopeIndex* index) {
    return ScopeCreationData::create(cx, bce->compilationInfo,
                                     modulesc->bindings, modulesc->module(),
                                     enclosing, index);
  };
  if (!internBodyScopeCreationData(bce, createScope)) {
    return false;
  }

  return checkEnvironmentChainLength(bce);
}

bool EmitterScope::enterWith(BytecodeEmitter* bce) {
  MOZ_ASSERT(this == bce->innermostEmitterScopeNoCheck());

  if (!ensureCache(bce)) {
    return false;
  }

  // 'with' make all accesses dynamic and unanalyzable.
  fallbackFreeNameLocation_ = Some(NameLocation::Dynamic());

  auto createScope = [bce](JSContext* cx, Handle<AbstractScopePtr> enclosing,
                           ScopeIndex* index) {
    return ScopeCreationData::create(cx, bce->compilationInfo, enclosing,
                                     index);
  };
  if (!internScopeCreationData(bce, createScope)) {
    return false;
  }

  if (!bce->emitInternedScopeOp(index(), JSOp::EnterWith)) {
    return false;
  }

  if (!appendScopeNote(bce)) {
    return false;
  }

  return checkEnvironmentChainLength(bce);
}

bool EmitterScope::deadZoneFrameSlots(BytecodeEmitter* bce) const {
  return deadZoneFrameSlotRange(bce, frameSlotStart(), frameSlotEnd());
}

bool EmitterScope::leave(BytecodeEmitter* bce, bool nonLocal) {
  // If we aren't leaving the scope due to a non-local jump (e.g., break),
  // we must be the innermost scope.
  MOZ_ASSERT_IF(!nonLocal, this == bce->innermostEmitterScopeNoCheck());

  ScopeKind kind = scope(bce).kind();
  switch (kind) {
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::FunctionLexical:
      if (!bce->emit1(hasEnvironment() ? JSOp::PopLexicalEnv
                                       : JSOp::DebugLeaveLexicalEnv)) {
        return false;
      }
      break;

    case ScopeKind::With:
      if (!bce->emit1(JSOp::LeaveWith)) {
        return false;
      }
      break;

    case ScopeKind::Function:
    case ScopeKind::FunctionBodyVar:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::Eval:
    case ScopeKind::StrictEval:
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic:
    case ScopeKind::Module:
      break;

    case ScopeKind::WasmInstance:
    case ScopeKind::WasmFunction:
      MOZ_CRASH("No wasm function scopes in JS");
  }

  // Finish up the scope if we are leaving it in LIFO fashion.
  if (!nonLocal) {
    // Popping scopes due to non-local jumps generate additional scope
    // notes. See NonLocalExitControl::prepareForNonLocalJump.
    if (ScopeKindIsInBody(kind)) {
      if (kind == ScopeKind::FunctionBodyVar) {
        // The extra function var scope is never popped once it's pushed,
        // so its scope note extends until the end of any possible code.
        bce->bytecodeSection().scopeNoteList().recordEndFunctionBodyVar(
            noteIndex_);
      } else {
        bce->bytecodeSection().scopeNoteList().recordEnd(
            noteIndex_, bce->bytecodeSection().offset());
      }
    }
  }

  return true;
}

AbstractScopePtr EmitterScope::scope(const BytecodeEmitter* bce) const {
  return bce->perScriptData().gcThingList().getScope(index());
}

NameLocation EmitterScope::lookup(BytecodeEmitter* bce, JSAtom* name) {
  if (Maybe<NameLocation> loc = lookupInCache(bce, name)) {
    return *loc;
  }
  return searchAndCache(bce, name);
}

Maybe<NameLocation> EmitterScope::locationBoundInScope(JSAtom* name,
                                                       EmitterScope* target) {
  // The target scope must be an intra-frame enclosing scope of this
  // one. Count the number of extra hops to reach it.
  uint8_t extraHops = 0;
  for (EmitterScope* es = this; es != target; es = es->enclosingInFrame()) {
    if (es->hasEnvironment()) {
      extraHops++;
    }
  }

  // Caches are prepopulated with bound names. So if the name is bound in a
  // particular scope, it must already be in the cache. Furthermore, don't
  // consult the fallback location as we only care about binding names.
  Maybe<NameLocation> loc;
  if (NameLocationMap::Ptr p = target->nameCache_->lookup(name)) {
    NameLocation l = p->value().wrapped;
    if (l.kind() == NameLocation::Kind::EnvironmentCoordinate) {
      loc = Some(l.addHops(extraHops));
    } else {
      loc = Some(l);
    }
  }
  return loc;
}
