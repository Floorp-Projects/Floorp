/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Scope.h"

#include "mozilla/ScopeExit.h"

#include <memory>
#include <new>

#include "builtin/ModuleObject.h"
#include "gc/Allocator.h"
#include "util/StringBuffer.h"
#include "vm/EnvironmentObject.h"
#include "vm/JSScript.h"
#include "wasm/WasmInstance.h"

#include "gc/FreeOp-inl.h"
#include "gc/ObjectKind-inl.h"
#include "vm/Shape-inl.h"

using namespace js;

using mozilla::Maybe;

const char* js::BindingKindString(BindingKind kind) {
  switch (kind) {
    case BindingKind::Import:
      return "import";
    case BindingKind::FormalParameter:
      return "formal parameter";
    case BindingKind::Var:
      return "var";
    case BindingKind::Let:
      return "let";
    case BindingKind::Const:
      return "const";
    case BindingKind::NamedLambdaCallee:
      return "named lambda callee";
  }
  MOZ_CRASH("Bad BindingKind");
}

const char* js::ScopeKindString(ScopeKind kind) {
  switch (kind) {
    case ScopeKind::Function:
      return "function";
    case ScopeKind::FunctionBodyVar:
      return "function body var";
    case ScopeKind::ParameterExpressionVar:
      return "parameter expression var";
    case ScopeKind::Lexical:
      return "lexical";
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
      return "catch";
    case ScopeKind::NamedLambda:
      return "named lambda";
    case ScopeKind::StrictNamedLambda:
      return "strict named lambda";
    case ScopeKind::FunctionLexical:
      return "function lexical";
    case ScopeKind::With:
      return "with";
    case ScopeKind::Eval:
      return "eval";
    case ScopeKind::StrictEval:
      return "strict eval";
    case ScopeKind::Global:
      return "global";
    case ScopeKind::NonSyntactic:
      return "non-syntactic";
    case ScopeKind::Module:
      return "module";
    case ScopeKind::WasmInstance:
      return "wasm instance";
    case ScopeKind::WasmFunction:
      return "wasm function";
  }
  MOZ_CRASH("Bad ScopeKind");
}

static Shape* EmptyEnvironmentShape(JSContext* cx, const Class* cls,
                                    uint32_t numSlots,
                                    uint32_t baseShapeFlags) {
  // Put as many slots into the object header as possible.
  uint32_t numFixed = gc::GetGCKindSlots(gc::GetGCObjectKind(numSlots));
  return EmptyShape::getInitialShape(cx, cls, TaggedProto(nullptr), numFixed,
                                     baseShapeFlags);
}

static Shape* NextEnvironmentShape(JSContext* cx, HandleAtom name,
                                   BindingKind bindKind, uint32_t slot,
                                   StackBaseShape& stackBase,
                                   HandleShape shape) {
  UnownedBaseShape* base = BaseShape::getUnowned(cx, stackBase);
  if (!base) {
    return nullptr;
  }

  unsigned attrs = JSPROP_PERMANENT | JSPROP_ENUMERATE;
  switch (bindKind) {
    case BindingKind::Const:
    case BindingKind::NamedLambdaCallee:
      attrs |= JSPROP_READONLY;
      break;
    default:
      break;
  }

  jsid id = NameToId(name->asPropertyName());
  Rooted<StackShape> child(cx, StackShape(base, id, slot, attrs));
  return cx->zone()->propertyTree().getChild(cx, shape, child);
}

static Shape* CreateEnvironmentShape(JSContext* cx, BindingIter& bi,
                                     const Class* cls, uint32_t numSlots,
                                     uint32_t baseShapeFlags) {
  RootedShape shape(cx,
                    EmptyEnvironmentShape(cx, cls, numSlots, baseShapeFlags));
  if (!shape) {
    return nullptr;
  }

  RootedAtom name(cx);
  StackBaseShape stackBase(cls, baseShapeFlags);
  for (; bi; bi++) {
    BindingLocation loc = bi.location();
    if (loc.kind() == BindingLocation::Kind::Environment) {
      name = bi.name();
      cx->markAtom(name);
      shape = NextEnvironmentShape(cx, name, bi.kind(), loc.slot(), stackBase,
                                   shape);
      if (!shape) {
        return nullptr;
      }
    }
  }

  return shape;
}

template <class Data>
inline size_t SizeOfAllocatedData(Data* data) {
  return SizeOfData<Data>(data->length);
}

template <typename ConcreteScope>
static UniquePtr<typename ConcreteScope::Data> CopyScopeData(
    JSContext* cx, typename ConcreteScope::Data* data) {
  // Make sure the binding names are marked in the context's zone, if we are
  // copying data from another zone.
  BindingName* names = data->trailingNames.start();
  uint32_t length = data->length;
  for (size_t i = 0; i < length; i++) {
    if (JSAtom* name = names[i].name()) {
      cx->markAtom(name);
    }
  }

  size_t size = SizeOfAllocatedData(data);
  void* bytes = cx->pod_malloc<char>(size);
  if (!bytes) {
    return nullptr;
  }

  auto* dataCopy = new (bytes) typename ConcreteScope::Data(*data);

  std::uninitialized_copy_n(names, length, dataCopy->trailingNames.start());

  return UniquePtr<typename ConcreteScope::Data>(dataCopy);
}

template <typename ConcreteScope>
static bool PrepareScopeData(
    JSContext* cx, BindingIter& bi,
    Handle<UniquePtr<typename ConcreteScope::Data>> data, const Class* cls,
    uint32_t baseShapeFlags, MutableHandleShape envShape) {
  // Copy a fresh BindingIter for use below.
  BindingIter freshBi(bi);

  // Iterate through all bindings. This counts the number of environment
  // slots needed and computes the maximum frame slot.
  while (bi) {
    bi++;
  }
  data->nextFrameSlot =
      bi.canHaveFrameSlots() ? bi.nextFrameSlot() : LOCALNO_LIMIT;

  // Make a new environment shape if any environment slots were used.
  if (bi.nextEnvironmentSlot() == JSSLOT_FREE(cls)) {
    envShape.set(nullptr);
  } else {
    envShape.set(CreateEnvironmentShape(
        cx, freshBi, cls, bi.nextEnvironmentSlot(), baseShapeFlags));
    if (!envShape) {
      return false;
    }
  }

  return true;
}

template <typename ConcreteScope>
static UniquePtr<typename ConcreteScope::Data> NewEmptyScopeData(
    JSContext* cx, uint32_t length = 0) {
  size_t dataSize = SizeOfData<typename ConcreteScope::Data>(length);
  uint8_t* bytes = cx->pod_malloc<uint8_t>(dataSize);
  auto data = reinterpret_cast<typename ConcreteScope::Data*>(bytes);
  if (data) {
    new (data) typename ConcreteScope::Data(length);
  }
  return UniquePtr<typename ConcreteScope::Data>(data);
}

static constexpr size_t HasAtomMask = 1;
static constexpr size_t HasAtomShift = 1;

static XDRResult XDRTrailingName(XDRState<XDR_ENCODE>* xdr,
                                 BindingName* bindingName,
                                 const uint32_t* length) {
  JSContext* cx = xdr->cx();

  RootedAtom atom(cx, bindingName->name());
  bool hasAtom = !!atom;

  uint8_t flags = bindingName->flagsForXDR();
  MOZ_ASSERT(((flags << HasAtomShift) >> HasAtomShift) == flags);
  uint8_t u8 = (flags << HasAtomShift) | uint8_t(hasAtom);
  MOZ_TRY(xdr->codeUint8(&u8));

  if (hasAtom) {
    MOZ_TRY(XDRAtom(xdr, &atom));
  }

  return Ok();
}

static XDRResult XDRTrailingName(XDRState<XDR_DECODE>* xdr, void* bindingName,
                                 uint32_t* length) {
  JSContext* cx = xdr->cx();

  uint8_t u8;
  MOZ_TRY(xdr->codeUint8(&u8));

  bool hasAtom = u8 & HasAtomMask;
  RootedAtom atom(cx);
  if (hasAtom) {
    MOZ_TRY(XDRAtom(xdr, &atom));
  }

  uint8_t flags = u8 >> HasAtomShift;
  new (bindingName) BindingName(BindingName::fromXDR(atom, flags));
  ++*length;

  return Ok();
}

template <typename ConcreteScopeData>
static void DeleteScopeData(ConcreteScopeData* data) {
  // Some scope Data classes have GCManagedDeletePolicy because then contain
  // GCPtrs. Dispose of them in the appropriate way.
  JS::DeletePolicy<ConcreteScopeData>()(data);
}

template <typename ConcreteScope, XDRMode mode>
/* static */
XDRResult Scope::XDRSizedBindingNames(
    XDRState<mode>* xdr, Handle<ConcreteScope*> scope,
    MutableHandle<typename ConcreteScope::Data*> data) {
  MOZ_ASSERT(!data);

  JSContext* cx = xdr->cx();

  uint32_t length;
  if (mode == XDR_ENCODE) {
    length = scope->data().length;
  }
  MOZ_TRY(xdr->codeUint32(&length));

  if (mode == XDR_ENCODE) {
    data.set(&scope->data());
  } else {
    data.set(NewEmptyScopeData<ConcreteScope>(cx, length).release());
    if (!data) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  auto dataGuard = mozilla::MakeScopeExit([&]() {
    if (mode == XDR_DECODE) {
      DeleteScopeData(data.get());
      data.set(nullptr);
    }
  });

  for (uint32_t i = 0; i < length; i++) {
    if (mode == XDR_DECODE) {
      MOZ_ASSERT(i == data->length, "must be decoding at the end");
    }
    MOZ_TRY(XDRTrailingName(xdr, &data->trailingNames[i], &data->length));
  }
  MOZ_ASSERT(data->length == length);

  dataGuard.release();
  return Ok();
}

/* static */
Scope* Scope::create(JSContext* cx, ScopeKind kind, HandleScope enclosing,
                     HandleShape envShape) {
  Scope* scope = Allocate<Scope>(cx);
  if (scope) {
    new (scope) Scope(kind, enclosing, envShape);
  }
  return scope;
}

template <typename ConcreteScope>
/* static */
ConcreteScope* Scope::create(
    JSContext* cx, ScopeKind kind, HandleScope enclosing, HandleShape envShape,
    MutableHandle<UniquePtr<typename ConcreteScope::Data>> data) {
  Scope* scope = create(cx, kind, enclosing, envShape);
  if (!scope) {
    return nullptr;
  }

  // It is an invariant that all Scopes that have data (currently, all
  // ScopeKinds except With) must have non-null data.
  MOZ_ASSERT(data);
  scope->initData<ConcreteScope>(data);

  return &scope->as<ConcreteScope>();
}

template <typename ConcreteScope>
inline void Scope::initData(
    MutableHandle<UniquePtr<typename ConcreteScope::Data>> data) {
  MOZ_ASSERT(!data_);

  AddCellMemory(this, SizeOfAllocatedData(data.get().get()),
                MemoryUse::ScopeData);

  data_ = data.get().release();
}

uint32_t Scope::chainLength() const {
  uint32_t length = 0;
  for (ScopeIter si(const_cast<Scope*>(this)); si; si++) {
    length++;
  }
  return length;
}

uint32_t Scope::environmentChainLength() const {
  uint32_t length = 0;
  for (ScopeIter si(const_cast<Scope*>(this)); si; si++) {
    if (si.hasSyntacticEnvironment()) {
      length++;
    }
  }
  return length;
}

Shape* Scope::maybeCloneEnvironmentShape(JSContext* cx) {
  // Clone the environment shape if cloning into a different zone.
  if (environmentShape_ &&
      environmentShape_->zoneFromAnyThread() != cx->zone()) {
    BindingIter bi(this);
    return CreateEnvironmentShape(cx, bi, environmentShape_->getObjectClass(),
                                  environmentShape_->slotSpan(),
                                  environmentShape_->getObjectFlags());
  }
  return environmentShape_;
}

/* static */
Scope* Scope::clone(JSContext* cx, HandleScope scope, HandleScope enclosing) {
  RootedShape envShape(cx);
  if (scope->environmentShape()) {
    envShape = scope->maybeCloneEnvironmentShape(cx);
    if (!envShape) {
      return nullptr;
    }
  }

  switch (scope->kind_) {
    case ScopeKind::Function: {
      RootedScript script(cx, scope->as<FunctionScope>().script());
      const char* filename = script->filename();
      // If the script has an internal URL, include it in the crash reason. If
      // not, it may be a web URL, and therefore privacy-sensitive.
      if (!strncmp(filename, "chrome:", 7) ||
          !strncmp(filename, "resource:", 9)) {
        MOZ_CRASH_UNSAFE_PRINTF("Use FunctionScope::clone (script URL: %s)",
                                filename);
      }

      MOZ_CRASH("Use FunctionScope::clone.");
      break;
    }

    case ScopeKind::FunctionBodyVar:
    case ScopeKind::ParameterExpressionVar: {
      Rooted<UniquePtr<VarScope::Data>> dataClone(cx);
      dataClone = CopyScopeData<VarScope>(cx, &scope->as<VarScope>().data());
      if (!dataClone) {
        return nullptr;
      }
      return create<VarScope>(cx, scope->kind_, enclosing, envShape,
                              &dataClone);
    }

    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
    case ScopeKind::FunctionLexical: {
      Rooted<UniquePtr<LexicalScope::Data>> dataClone(cx);
      dataClone =
          CopyScopeData<LexicalScope>(cx, &scope->as<LexicalScope>().data());
      if (!dataClone) {
        return nullptr;
      }
      return create<LexicalScope>(cx, scope->kind_, enclosing, envShape,
                                  &dataClone);
    }

    case ScopeKind::With:
      return create(cx, scope->kind_, enclosing, envShape);

    case ScopeKind::Eval:
    case ScopeKind::StrictEval: {
      Rooted<UniquePtr<EvalScope::Data>> dataClone(cx);
      dataClone = CopyScopeData<EvalScope>(cx, &scope->as<EvalScope>().data());
      if (!dataClone) {
        return nullptr;
      }
      return create<EvalScope>(cx, scope->kind_, enclosing, envShape,
                               &dataClone);
    }

    case ScopeKind::Global:
    case ScopeKind::NonSyntactic:
      MOZ_CRASH("Use GlobalScope::clone.");
      break;

    case ScopeKind::WasmFunction:
      MOZ_CRASH("wasm functions are not nested in JSScript");
      break;

    case ScopeKind::Module:
    case ScopeKind::WasmInstance:
      MOZ_CRASH("NYI");
      break;
  }

  return nullptr;
}

void Scope::finalize(JSFreeOp* fop) {
  MOZ_ASSERT(CurrentThreadIsGCSweeping());
  applyScopeDataTyped([this, fop](auto data) {
    fop->delete_(this, data, SizeOfAllocatedData(data), MemoryUse::ScopeData);
  });
  data_ = nullptr;
}

size_t Scope::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
  if (data_) {
    return mallocSizeOf(data_);
  }
  return 0;
}

void Scope::dump() {
  for (ScopeIter si(this); si; si++) {
    fprintf(stderr, "%s [%p]", ScopeKindString(si.kind()), si.scope());
    if (si.scope()->enclosing()) {
      fprintf(stderr, " -> ");
    }
  }
  fprintf(stderr, "\n");
}

uint32_t LexicalScope::firstFrameSlot() const {
  switch (kind()) {
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::FunctionLexical:
      // For intra-frame scopes, find the enclosing scope's next frame slot.
      return nextFrameSlot(enclosing());
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
      // Named lambda scopes cannot have frame slots.
      return LOCALNO_LIMIT;
    default:
      // Otherwise start at 0.
      break;
  }
  return 0;
}

/* static */
uint32_t LexicalScope::nextFrameSlot(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    switch (si.kind()) {
      case ScopeKind::Function:
        return si.scope()->as<FunctionScope>().nextFrameSlot();
      case ScopeKind::FunctionBodyVar:
      case ScopeKind::ParameterExpressionVar:
        return si.scope()->as<VarScope>().nextFrameSlot();
      case ScopeKind::Lexical:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
      case ScopeKind::FunctionLexical:
        return si.scope()->as<LexicalScope>().nextFrameSlot();
      case ScopeKind::NamedLambda:
      case ScopeKind::StrictNamedLambda:
        // Named lambda scopes cannot have frame slots.
        return 0;
      case ScopeKind::With:
        continue;
      case ScopeKind::Eval:
      case ScopeKind::StrictEval:
        return si.scope()->as<EvalScope>().nextFrameSlot();
      case ScopeKind::Global:
      case ScopeKind::NonSyntactic:
        return 0;
      case ScopeKind::Module:
        return si.scope()->as<ModuleScope>().nextFrameSlot();
      case ScopeKind::WasmInstance:
        // TODO return si.scope()->as<WasmInstanceScope>().nextFrameSlot();
        return 0;
      case ScopeKind::WasmFunction:
        // TODO return si.scope()->as<WasmFunctionScope>().nextFrameSlot();
        return 0;
    }
  }
  MOZ_CRASH("Not an enclosing intra-frame Scope");
}

/* static */
LexicalScope* LexicalScope::create(JSContext* cx, ScopeKind kind,
                                   Handle<Data*> data, uint32_t firstFrameSlot,
                                   HandleScope enclosing) {
  MOZ_ASSERT(data,
             "LexicalScopes should not be created if there are no bindings.");

  // The data that's passed in is from the frontend and is LifoAlloc'd.
  // Copy it now that we're creating a permanent VM scope.
  Rooted<UniquePtr<Data>> copy(cx, CopyScopeData<LexicalScope>(cx, data));
  if (!copy) {
    return nullptr;
  }

  return createWithData(cx, kind, &copy, firstFrameSlot, enclosing);
}

/* static */
LexicalScope* LexicalScope::createWithData(JSContext* cx, ScopeKind kind,
                                           MutableHandle<UniquePtr<Data>> data,
                                           uint32_t firstFrameSlot,
                                           HandleScope enclosing) {
  bool isNamedLambda =
      kind == ScopeKind::NamedLambda || kind == ScopeKind::StrictNamedLambda;

  MOZ_ASSERT_IF(!isNamedLambda && firstFrameSlot != 0,
                firstFrameSlot == nextFrameSlot(enclosing));
  MOZ_ASSERT_IF(isNamedLambda, firstFrameSlot == LOCALNO_LIMIT);

  RootedShape envShape(cx);
  BindingIter bi(*data, firstFrameSlot, isNamedLambda);
  if (!PrepareScopeData<LexicalScope>(
          cx, bi, data, &LexicalEnvironmentObject::class_,
          BaseShape::NOT_EXTENSIBLE | BaseShape::DELEGATE, &envShape)) {
    return nullptr;
  }

  auto scope = Scope::create<LexicalScope>(cx, kind, enclosing, envShape, data);
  if (!scope) {
    return nullptr;
  }

  MOZ_ASSERT(scope->firstFrameSlot() == firstFrameSlot);
  return scope;
}

/* static */
Shape* LexicalScope::getEmptyExtensibleEnvironmentShape(JSContext* cx) {
  const Class* cls = &LexicalEnvironmentObject::class_;
  return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls), BaseShape::DELEGATE);
}

template <XDRMode mode>
/* static */
XDRResult LexicalScope::XDR(XDRState<mode>* xdr, ScopeKind kind,
                            HandleScope enclosing, MutableHandleScope scope) {
  JSContext* cx = xdr->cx();

  Rooted<Data*> data(cx);
  MOZ_TRY(
      XDRSizedBindingNames<LexicalScope>(xdr, scope.as<LexicalScope>(), &data));

  {
    Maybe<Rooted<UniquePtr<Data>>> uniqueData;
    if (mode == XDR_DECODE) {
      uniqueData.emplace(cx, data);
    }

    uint32_t firstFrameSlot;
    uint32_t nextFrameSlot;
    if (mode == XDR_ENCODE) {
      firstFrameSlot = scope->as<LexicalScope>().firstFrameSlot();
      nextFrameSlot = data->nextFrameSlot;
    }

    MOZ_TRY(xdr->codeUint32(&data->constStart));
    MOZ_TRY(xdr->codeUint32(&firstFrameSlot));
    MOZ_TRY(xdr->codeUint32(&nextFrameSlot));

    if (mode == XDR_DECODE) {
      scope.set(createWithData(cx, kind, &uniqueData.ref(), firstFrameSlot,
                               enclosing));
      if (!scope) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }

      // nextFrameSlot is used only for this correctness check.
      MOZ_ASSERT(nextFrameSlot ==
                 scope->as<LexicalScope>().data().nextFrameSlot);
    }
  }

  return Ok();
}

template
    /* static */
    XDRResult
    LexicalScope::XDR(XDRState<XDR_ENCODE>* xdr, ScopeKind kind,
                      HandleScope enclosing, MutableHandleScope scope);

template
    /* static */
    XDRResult
    LexicalScope::XDR(XDRState<XDR_DECODE>* xdr, ScopeKind kind,
                      HandleScope enclosing, MutableHandleScope scope);

static inline uint32_t FunctionScopeEnvShapeFlags(bool hasParameterExprs) {
  if (hasParameterExprs) {
    return BaseShape::DELEGATE;
  }
  return BaseShape::QUALIFIED_VAROBJ | BaseShape::DELEGATE;
}

/* static */
FunctionScope* FunctionScope::create(JSContext* cx, Handle<Data*> dataArg,
                                     bool hasParameterExprs,
                                     bool needsEnvironment, HandleFunction fun,
                                     HandleScope enclosing) {
  // The data that's passed in is from the frontend and is LifoAlloc'd.
  // Copy it now that we're creating a permanent VM scope.
  Rooted<UniquePtr<Data>> data(
      cx, dataArg ? CopyScopeData<FunctionScope>(cx, dataArg)
                  : NewEmptyScopeData<FunctionScope>(cx));
  if (!data) {
    return nullptr;
  }

  return createWithData(
      cx, &data, hasParameterExprs,
      dataArg ? dataArg->isFieldInitializer : IsFieldInitializer::No,
      needsEnvironment, fun, enclosing);
}

/* static */
FunctionScope* FunctionScope::createWithData(
    JSContext* cx, MutableHandle<UniquePtr<Data>> data, bool hasParameterExprs,
    IsFieldInitializer isFieldInitializer, bool needsEnvironment,
    HandleFunction fun, HandleScope enclosing) {
  MOZ_ASSERT(data);
  MOZ_ASSERT(fun->isTenured());

  // FunctionScope::Data has GCManagedDeletePolicy because it contains a
  // GCPtr. Destruction of |data| below may trigger calls into the GC.

  RootedShape envShape(cx);

  BindingIter bi(*data, hasParameterExprs);
  uint32_t shapeFlags = FunctionScopeEnvShapeFlags(hasParameterExprs);
  if (!PrepareScopeData<FunctionScope>(cx, bi, data, &CallObject::class_,
                                       shapeFlags, &envShape)) {
    return nullptr;
  }

  data->isFieldInitializer = isFieldInitializer;
  data->hasParameterExprs = hasParameterExprs;
  data->canonicalFunction.init(fun);

  // An environment may be needed regardless of existence of any closed over
  // bindings:
  //   - Extensible scopes (i.e., due to direct eval)
  //   - Needing a home object
  //   - Being a derived class constructor
  //   - Being a generator
  if (!envShape && needsEnvironment) {
    envShape = getEmptyEnvironmentShape(cx, hasParameterExprs);
    if (!envShape) {
      return nullptr;
    }
  }

  return Scope::create<FunctionScope>(cx, ScopeKind::Function, enclosing,
                                      envShape, data);
}

JSScript* FunctionScope::script() const {
  return canonicalFunction()->nonLazyScript();
}

/* static */
bool FunctionScope::isSpecialName(JSContext* cx, JSAtom* name) {
  return name == cx->names().arguments || name == cx->names().dotThis ||
         name == cx->names().dotGenerator;
}

/* static */
Shape* FunctionScope::getEmptyEnvironmentShape(JSContext* cx,
                                               bool hasParameterExprs) {
  const Class* cls = &CallObject::class_;
  uint32_t shapeFlags = FunctionScopeEnvShapeFlags(hasParameterExprs);
  return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls), shapeFlags);
}

/* static */
FunctionScope* FunctionScope::clone(JSContext* cx, Handle<FunctionScope*> scope,
                                    HandleFunction fun, HandleScope enclosing) {
  MOZ_ASSERT(fun != scope->canonicalFunction());

  // FunctionScope::Data has GCManagedDeletePolicy because it contains a
  // GCPtr. Destruction of |dataClone| below may trigger calls into the GC.

  RootedShape envShape(cx);
  if (scope->environmentShape()) {
    envShape = scope->maybeCloneEnvironmentShape(cx);
    if (!envShape) {
      return nullptr;
    }
  }

  Rooted<Data*> dataOriginal(cx, &scope->as<FunctionScope>().data());
  Rooted<UniquePtr<Data>> dataClone(
      cx, CopyScopeData<FunctionScope>(cx, dataOriginal));
  if (!dataClone) {
    return nullptr;
  }

  dataClone->canonicalFunction.init(fun);

  return Scope::create<FunctionScope>(cx, scope->kind(), enclosing, envShape,
                                      &dataClone);
}

template <XDRMode mode>
/* static */
XDRResult FunctionScope::XDR(XDRState<mode>* xdr, HandleFunction fun,
                             HandleScope enclosing, MutableHandleScope scope) {
  JSContext* cx = xdr->cx();
  Rooted<Data*> data(cx);
  MOZ_TRY(XDRSizedBindingNames<FunctionScope>(xdr, scope.as<FunctionScope>(),
                                              &data));

  {
    Maybe<Rooted<UniquePtr<Data>>> uniqueData;
    if (mode == XDR_DECODE) {
      uniqueData.emplace(cx, data);
    }

    uint8_t needsEnvironment;
    uint8_t hasParameterExprs;
    uint8_t isFieldInitializer;
    uint32_t nextFrameSlot;
    if (mode == XDR_ENCODE) {
      needsEnvironment = scope->hasEnvironment();
      hasParameterExprs = data->hasParameterExprs;
      isFieldInitializer =
          (data->isFieldInitializer == IsFieldInitializer::Yes ? 1 : 0);
      nextFrameSlot = data->nextFrameSlot;
    }
    MOZ_TRY(xdr->codeUint8(&needsEnvironment));
    MOZ_TRY(xdr->codeUint8(&hasParameterExprs));
    MOZ_TRY(xdr->codeUint8(&isFieldInitializer));
    MOZ_TRY(xdr->codeUint16(&data->nonPositionalFormalStart));
    MOZ_TRY(xdr->codeUint16(&data->varStart));
    MOZ_TRY(xdr->codeUint32(&nextFrameSlot));

    if (mode == XDR_DECODE) {
      if (!data->length) {
        MOZ_ASSERT(!data->nonPositionalFormalStart);
        MOZ_ASSERT(!data->varStart);
        MOZ_ASSERT(!data->nextFrameSlot);
      }

      scope.set(createWithData(
          cx, &uniqueData.ref(), hasParameterExprs,
          isFieldInitializer ? IsFieldInitializer::Yes : IsFieldInitializer::No,
          needsEnvironment, fun, enclosing));
      if (!scope) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }

      // nextFrameSlot is used only for this correctness check.
      MOZ_ASSERT(nextFrameSlot ==
                 scope->as<FunctionScope>().data().nextFrameSlot);
    }
  }

  return Ok();
}

template
    /* static */
    XDRResult
    FunctionScope::XDR(XDRState<XDR_ENCODE>* xdr, HandleFunction fun,
                       HandleScope enclosing, MutableHandleScope scope);

template
    /* static */
    XDRResult
    FunctionScope::XDR(XDRState<XDR_DECODE>* xdr, HandleFunction fun,
                       HandleScope enclosing, MutableHandleScope scope);

static const uint32_t VarScopeEnvShapeFlags =
    BaseShape::QUALIFIED_VAROBJ | BaseShape::DELEGATE;

static UniquePtr<VarScope::Data> NewEmptyVarScopeData(JSContext* cx,
                                                      uint32_t firstFrameSlot) {
  UniquePtr<VarScope::Data> data(NewEmptyScopeData<VarScope>(cx));
  if (data) {
    data->nextFrameSlot = firstFrameSlot;
  }

  return data;
}

/* static */
VarScope* VarScope::create(JSContext* cx, ScopeKind kind, Handle<Data*> dataArg,
                           uint32_t firstFrameSlot, bool needsEnvironment,
                           HandleScope enclosing) {
  // The data that's passed in is from the frontend and is LifoAlloc'd.
  // Copy it now that we're creating a permanent VM scope.
  Rooted<UniquePtr<Data>> data(
      cx, dataArg ? CopyScopeData<VarScope>(cx, dataArg)
                  : NewEmptyVarScopeData(cx, firstFrameSlot));
  if (!data) {
    return nullptr;
  }

  return createWithData(cx, kind, &data, firstFrameSlot, needsEnvironment,
                        enclosing);
}

/* static */
VarScope* VarScope::createWithData(JSContext* cx, ScopeKind kind,
                                   MutableHandle<UniquePtr<Data>> data,
                                   uint32_t firstFrameSlot,
                                   bool needsEnvironment,
                                   HandleScope enclosing) {
  MOZ_ASSERT(data);

  RootedShape envShape(cx);
  BindingIter bi(*data, firstFrameSlot);
  if (!PrepareScopeData<VarScope>(cx, bi, data, &VarEnvironmentObject::class_,
                                  VarScopeEnvShapeFlags, &envShape)) {
    return nullptr;
  }

  // An environment may be needed regardless of existence of any closed over
  // bindings:
  //   - Extensible scopes (i.e., due to direct eval)
  //   - Being a generator
  if (!envShape && needsEnvironment) {
    envShape = getEmptyEnvironmentShape(cx);
    if (!envShape) {
      return nullptr;
    }
  }

  return Scope::create<VarScope>(cx, kind, enclosing, envShape, data);
}

/* static */
Shape* VarScope::getEmptyEnvironmentShape(JSContext* cx) {
  const Class* cls = &VarEnvironmentObject::class_;
  return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls),
                               VarScopeEnvShapeFlags);
}

uint32_t VarScope::firstFrameSlot() const {
  if (enclosing()->is<FunctionScope>()) {
    return enclosing()->as<FunctionScope>().nextFrameSlot();
  }
  return 0;
}

template <XDRMode mode>
/* static */
XDRResult VarScope::XDR(XDRState<mode>* xdr, ScopeKind kind,
                        HandleScope enclosing, MutableHandleScope scope) {
  JSContext* cx = xdr->cx();
  Rooted<Data*> data(cx);
  MOZ_TRY(XDRSizedBindingNames<VarScope>(xdr, scope.as<VarScope>(), &data));

  {
    Maybe<Rooted<UniquePtr<Data>>> uniqueData;
    if (mode == XDR_DECODE) {
      uniqueData.emplace(cx, data);
    }

    uint8_t needsEnvironment;
    uint32_t firstFrameSlot;
    uint32_t nextFrameSlot;
    if (mode == XDR_ENCODE) {
      needsEnvironment = scope->hasEnvironment();
      firstFrameSlot = scope->as<VarScope>().firstFrameSlot();
      nextFrameSlot = data->nextFrameSlot;
    }
    MOZ_TRY(xdr->codeUint8(&needsEnvironment));
    MOZ_TRY(xdr->codeUint32(&firstFrameSlot));
    MOZ_TRY(xdr->codeUint32(&nextFrameSlot));

    if (mode == XDR_DECODE) {
      if (!data->length) {
        MOZ_ASSERT(!data->nextFrameSlot);
      }

      scope.set(createWithData(cx, kind, &uniqueData.ref(), firstFrameSlot,
                               needsEnvironment, enclosing));
      if (!scope) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }

      // nextFrameSlot is used only for this correctness check.
      MOZ_ASSERT(nextFrameSlot == scope->as<VarScope>().data().nextFrameSlot);
    }
  }

  return Ok();
}

template
    /* static */
    XDRResult
    VarScope::XDR(XDRState<XDR_ENCODE>* xdr, ScopeKind kind,
                  HandleScope enclosing, MutableHandleScope scope);

template
    /* static */
    XDRResult
    VarScope::XDR(XDRState<XDR_DECODE>* xdr, ScopeKind kind,
                  HandleScope enclosing, MutableHandleScope scope);

/* static */
GlobalScope* GlobalScope::create(JSContext* cx, ScopeKind kind,
                                 Handle<Data*> dataArg) {
  // The data that's passed in is from the frontend and is LifoAlloc'd.
  // Copy it now that we're creating a permanent VM scope.
  Rooted<UniquePtr<Data>> data(cx, dataArg
                                       ? CopyScopeData<GlobalScope>(cx, dataArg)
                                       : NewEmptyScopeData<GlobalScope>(cx));
  if (!data) {
    return nullptr;
  }

  return createWithData(cx, kind, &data);
}

/* static */
GlobalScope* GlobalScope::createWithData(JSContext* cx, ScopeKind kind,
                                         MutableHandle<UniquePtr<Data>> data) {
  MOZ_ASSERT(data);

  // The global scope has no environment shape. Its environment is the
  // global lexical scope and the global object or non-syntactic objects
  // created by embedding, all of which are not only extensible but may
  // have names on them deleted.
  return Scope::create<GlobalScope>(cx, kind, nullptr, nullptr, data);
}

/* static */
GlobalScope* GlobalScope::clone(JSContext* cx, Handle<GlobalScope*> scope,
                                ScopeKind kind) {
  Rooted<Data*> dataOriginal(cx, &scope->as<GlobalScope>().data());
  Rooted<UniquePtr<Data>> dataClone(
      cx, CopyScopeData<GlobalScope>(cx, dataOriginal));
  if (!dataClone) {
    return nullptr;
  }

  return Scope::create<GlobalScope>(cx, kind, nullptr, nullptr, &dataClone);
}

template <XDRMode mode>
/* static */
XDRResult GlobalScope::XDR(XDRState<mode>* xdr, ScopeKind kind,
                           MutableHandleScope scope) {
  MOZ_ASSERT((mode == XDR_DECODE) == !scope);

  JSContext* cx = xdr->cx();
  Rooted<Data*> data(cx);
  MOZ_TRY(
      XDRSizedBindingNames<GlobalScope>(xdr, scope.as<GlobalScope>(), &data));

  {
    Maybe<Rooted<UniquePtr<Data>>> uniqueData;
    if (mode == XDR_DECODE) {
      uniqueData.emplace(cx, data);
    }

    MOZ_TRY(xdr->codeUint32(&data->letStart));
    MOZ_TRY(xdr->codeUint32(&data->constStart));

    if (mode == XDR_DECODE) {
      if (!data->length) {
        MOZ_ASSERT(!data->letStart);
        MOZ_ASSERT(!data->constStart);
      }

      scope.set(createWithData(cx, kind, &uniqueData.ref()));
      if (!scope) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
  }

  return Ok();
}

template
    /* static */
    XDRResult
    GlobalScope::XDR(XDRState<XDR_ENCODE>* xdr, ScopeKind kind,
                     MutableHandleScope scope);

template
    /* static */
    XDRResult
    GlobalScope::XDR(XDRState<XDR_DECODE>* xdr, ScopeKind kind,
                     MutableHandleScope scope);

/* static */
WithScope* WithScope::create(JSContext* cx, HandleScope enclosing) {
  Scope* scope = Scope::create(cx, ScopeKind::With, enclosing, nullptr);
  return static_cast<WithScope*>(scope);
}

template <XDRMode mode>
/* static */
XDRResult WithScope::XDR(XDRState<mode>* xdr, HandleScope enclosing,
                         MutableHandleScope scope) {
  JSContext* cx = xdr->cx();

  if (mode == XDR_DECODE) {
    scope.set(create(cx, enclosing));
    if (!scope) {
      return xdr->fail(JS::TranscodeResult_Throw);
    }
  }

  return Ok();
}

template
    /* static */
    XDRResult
    WithScope::XDR(XDRState<XDR_ENCODE>* xdr, HandleScope enclosing,
                   MutableHandleScope scope);

template
    /* static */
    XDRResult
    WithScope::XDR(XDRState<XDR_DECODE>* xdr, HandleScope enclosing,
                   MutableHandleScope scope);

static const uint32_t EvalScopeEnvShapeFlags =
    BaseShape::QUALIFIED_VAROBJ | BaseShape::DELEGATE;

/* static */
EvalScope* EvalScope::create(JSContext* cx, ScopeKind scopeKind,
                             Handle<Data*> dataArg, HandleScope enclosing) {
  // The data that's passed in is from the frontend and is LifoAlloc'd.
  // Copy it now that we're creating a permanent VM scope.
  Rooted<UniquePtr<Data>> data(cx, dataArg
                                       ? CopyScopeData<EvalScope>(cx, dataArg)
                                       : NewEmptyScopeData<EvalScope>(cx));
  if (!data) {
    return nullptr;
  }

  return createWithData(cx, scopeKind, &data, enclosing);
}

/* static */
EvalScope* EvalScope::createWithData(JSContext* cx, ScopeKind scopeKind,
                                     MutableHandle<UniquePtr<Data>> data,
                                     HandleScope enclosing) {
  MOZ_ASSERT(data);

  RootedShape envShape(cx);
  if (scopeKind == ScopeKind::StrictEval) {
    BindingIter bi(*data, true);
    if (!PrepareScopeData<EvalScope>(cx, bi, data,
                                     &VarEnvironmentObject::class_,
                                     EvalScopeEnvShapeFlags, &envShape)) {
      return nullptr;
    }
  }

  // Strict eval and direct eval in parameter expressions always get their own
  // var environment even if there are no bindings.
  if (!envShape && scopeKind == ScopeKind::StrictEval) {
    envShape = getEmptyEnvironmentShape(cx);
    if (!envShape) {
      return nullptr;
    }
  }

  return Scope::create<EvalScope>(cx, scopeKind, enclosing, envShape, data);
}

/* static */
Scope* EvalScope::nearestVarScopeForDirectEval(Scope* scope) {
  for (ScopeIter si(scope); si; si++) {
    switch (si.kind()) {
      case ScopeKind::Function:
      case ScopeKind::FunctionBodyVar:
      case ScopeKind::ParameterExpressionVar:
      case ScopeKind::Global:
      case ScopeKind::NonSyntactic:
        return scope;
      default:
        break;
    }
  }
  return nullptr;
}

/* static */
Shape* EvalScope::getEmptyEnvironmentShape(JSContext* cx) {
  const Class* cls = &VarEnvironmentObject::class_;
  return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls),
                               EvalScopeEnvShapeFlags);
}

template <XDRMode mode>
/* static */
XDRResult EvalScope::XDR(XDRState<mode>* xdr, ScopeKind kind,
                         HandleScope enclosing, MutableHandleScope scope) {
  JSContext* cx = xdr->cx();
  Rooted<Data*> data(cx);

  {
    Maybe<Rooted<UniquePtr<Data>>> uniqueData;
    if (mode == XDR_DECODE) {
      uniqueData.emplace(cx, data);
    }

    MOZ_TRY(XDRSizedBindingNames<EvalScope>(xdr, scope.as<EvalScope>(), &data));

    if (mode == XDR_DECODE) {
      if (!data->length) {
        MOZ_ASSERT(!data->nextFrameSlot);
      }

      scope.set(createWithData(cx, kind, &uniqueData.ref(), enclosing));
      if (!scope) {
        return xdr->fail(JS::TranscodeResult_Throw);
      }
    }
  }

  return Ok();
}

template
    /* static */
    XDRResult
    EvalScope::XDR(XDRState<XDR_ENCODE>* xdr, ScopeKind kind,
                   HandleScope enclosing, MutableHandleScope scope);

template
    /* static */
    XDRResult
    EvalScope::XDR(XDRState<XDR_DECODE>* xdr, ScopeKind kind,
                   HandleScope enclosing, MutableHandleScope scope);

static const uint32_t ModuleScopeEnvShapeFlags = BaseShape::NOT_EXTENSIBLE |
                                                 BaseShape::QUALIFIED_VAROBJ |
                                                 BaseShape::DELEGATE;

Zone* ModuleScope::Data::zone() const {
  return module ? module->zone() : nullptr;
}

/* static */
ModuleScope* ModuleScope::create(JSContext* cx, Handle<Data*> dataArg,
                                 HandleModuleObject module,
                                 HandleScope enclosing) {
  Rooted<UniquePtr<Data>> data(cx, dataArg
                                       ? CopyScopeData<ModuleScope>(cx, dataArg)
                                       : NewEmptyScopeData<ModuleScope>(cx));
  if (!data) {
    return nullptr;
  }

  return createWithData(cx, &data, module, enclosing);
}

/* static */
ModuleScope* ModuleScope::createWithData(JSContext* cx,
                                         MutableHandle<UniquePtr<Data>> data,
                                         HandleModuleObject module,
                                         HandleScope enclosing) {
  MOZ_ASSERT(data);
  MOZ_ASSERT(enclosing->is<GlobalScope>());

  // ModuleScope::Data has GCManagedDeletePolicy because it contains a
  // GCPtr. Destruction of |copy| below may trigger calls into the GC.

  // The data that's passed in is from the frontend and is LifoAlloc'd.
  // Copy it now that we're creating a permanent VM scope.
  RootedShape envShape(cx);
  BindingIter bi(*data);
  if (!PrepareScopeData<ModuleScope>(cx, bi, data,
                                     &ModuleEnvironmentObject::class_,
                                     ModuleScopeEnvShapeFlags, &envShape)) {
    return nullptr;
  }

  // Modules always need an environment object for now.
  if (!envShape) {
    envShape = getEmptyEnvironmentShape(cx);
    if (!envShape) {
      return nullptr;
    }
  }

  data->module.init(module);

  return Scope::create<ModuleScope>(cx, ScopeKind::Module, enclosing, envShape,
                                    data);
}

/* static */
Shape* ModuleScope::getEmptyEnvironmentShape(JSContext* cx) {
  const Class* cls = &ModuleEnvironmentObject::class_;
  return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls),
                               ModuleScopeEnvShapeFlags);
}

static const uint32_t WasmInstanceEnvShapeFlags =
    BaseShape::NOT_EXTENSIBLE | BaseShape::DELEGATE;

template <size_t ArrayLength>
static JSAtom* GenerateWasmName(JSContext* cx,
                                const char (&prefix)[ArrayLength],
                                uint32_t index) {
  StringBuffer sb(cx);
  if (!sb.append(prefix)) {
    return nullptr;
  }
  if (!NumberValueToStringBuffer(cx, Int32Value(index), sb)) {
    return nullptr;
  }

  return sb.finishAtom();
}

static void InitializeTrailingName(TrailingNamesArray& trailingNames, size_t i,
                                   JSAtom* name) {
  void* trailingName = &trailingNames[i];
  new (trailingName) BindingName(name, false);
}

template <class Data>
static void InitializeNextTrailingName(const Rooted<UniquePtr<Data>>& data,
                                       JSAtom* name) {
  InitializeTrailingName(data->trailingNames, data->length, name);
  data->length++;
}

/* static */
WasmInstanceScope* WasmInstanceScope::create(JSContext* cx,
                                             WasmInstanceObject* instance) {
  // WasmInstanceScope::Data has GCManagedDeletePolicy because it contains a
  // GCPtr. Destruction of |data| below may trigger calls into the GC.

  size_t namesCount = 0;
  if (instance->instance().memory()) {
    namesCount++;
  }
  size_t globalsStart = namesCount;
  size_t globalsCount = instance->instance().metadata().globals.length();
  namesCount += globalsCount;

  Rooted<UniquePtr<Data>> data(
      cx, NewEmptyScopeData<WasmInstanceScope>(cx, namesCount));
  if (!data) {
    return nullptr;
  }

  if (instance->instance().memory()) {
    JSAtom* wasmName = GenerateWasmName(cx, "memory", /* index = */ 0);
    if (!wasmName) {
      return nullptr;
    }

    InitializeNextTrailingName(data, wasmName);
  }

  for (size_t i = 0; i < globalsCount; i++) {
    JSAtom* wasmName = GenerateWasmName(cx, "global", i);
    if (!wasmName) {
      return nullptr;
    }

    InitializeNextTrailingName(data, wasmName);
  }

  MOZ_ASSERT(data->length == namesCount);

  data->instance.init(instance);
  data->memoriesStart = 0;
  data->globalsStart = globalsStart;

  Rooted<Scope*> enclosingScope(cx, &cx->global()->emptyGlobalScope());

  return Scope::create<WasmInstanceScope>(cx, ScopeKind::WasmInstance,
                                          enclosingScope,
                                          /* envShape = */ nullptr, &data);
}

/* static */
Shape* WasmInstanceScope::getEmptyEnvironmentShape(JSContext* cx) {
  const Class* cls = &WasmInstanceEnvironmentObject::class_;
  return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls),
                               WasmInstanceEnvShapeFlags);
}

// TODO Check what Debugger behavior should be when it evaluates a
// var declaration.
static const uint32_t WasmFunctionEnvShapeFlags =
    BaseShape::NOT_EXTENSIBLE | BaseShape::DELEGATE;

/* static */
WasmFunctionScope* WasmFunctionScope::create(JSContext* cx,
                                             HandleScope enclosing,
                                             uint32_t funcIndex) {
  MOZ_ASSERT(enclosing->is<WasmInstanceScope>());

  Rooted<WasmFunctionScope*> wasmFunctionScope(cx);

  Rooted<WasmInstanceObject*> instance(
      cx, enclosing->as<WasmInstanceScope>().instance());

  // TODO pull the local variable names from the wasm function definition.
  wasm::ValTypeVector locals;
  size_t argsLength;
  if (!instance->instance().debug().debugGetLocalTypes(funcIndex, &locals,
                                                       &argsLength)) {
    return nullptr;
  }
  uint32_t namesCount = locals.length();

  Rooted<UniquePtr<Data>> data(
      cx, NewEmptyScopeData<WasmFunctionScope>(cx, namesCount));
  if (!data) {
    return nullptr;
  }

  for (size_t i = 0; i < namesCount; i++) {
    JSAtom* wasmName = GenerateWasmName(cx, "var", i);
    if (!wasmName) {
      return nullptr;
    }

    InitializeNextTrailingName(data, wasmName);
  }
  MOZ_ASSERT(data->length == namesCount);

  data->funcIndex = funcIndex;

  return Scope::create<WasmFunctionScope>(cx, ScopeKind::WasmFunction,
                                          enclosing,
                                          /* envShape = */ nullptr, &data);
}

/* static */
Shape* WasmFunctionScope::getEmptyEnvironmentShape(JSContext* cx) {
  const Class* cls = &WasmFunctionCallObject::class_;
  return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls),
                               WasmFunctionEnvShapeFlags);
}

ScopeIter::ScopeIter(JSScript* script) : scope_(script->bodyScope()) {}

bool ScopeIter::hasSyntacticEnvironment() const {
  return scope()->hasEnvironment() &&
         scope()->kind() != ScopeKind::NonSyntactic;
}

BindingIter::BindingIter(Scope* scope) {
  switch (scope->kind()) {
    case ScopeKind::Lexical:
    case ScopeKind::SimpleCatch:
    case ScopeKind::Catch:
    case ScopeKind::FunctionLexical:
      init(scope->as<LexicalScope>().data(),
           scope->as<LexicalScope>().firstFrameSlot(), 0);
      break;
    case ScopeKind::NamedLambda:
    case ScopeKind::StrictNamedLambda:
      init(scope->as<LexicalScope>().data(), LOCALNO_LIMIT, IsNamedLambda);
      break;
    case ScopeKind::With:
      // With scopes do not have bindings.
      index_ = length_ = 0;
      MOZ_ASSERT(done());
      break;
    case ScopeKind::Function: {
      uint8_t flags = IgnoreDestructuredFormalParameters;
      if (scope->as<FunctionScope>().hasParameterExprs()) {
        flags |= HasFormalParameterExprs;
      }
      init(scope->as<FunctionScope>().data(), flags);
      break;
    }
    case ScopeKind::FunctionBodyVar:
    case ScopeKind::ParameterExpressionVar:
      init(scope->as<VarScope>().data(),
           scope->as<VarScope>().firstFrameSlot());
      break;
    case ScopeKind::Eval:
    case ScopeKind::StrictEval:
      init(scope->as<EvalScope>().data(),
           scope->kind() == ScopeKind::StrictEval);
      break;
    case ScopeKind::Global:
    case ScopeKind::NonSyntactic:
      init(scope->as<GlobalScope>().data());
      break;
    case ScopeKind::Module:
      init(scope->as<ModuleScope>().data());
      break;
    case ScopeKind::WasmInstance:
      init(scope->as<WasmInstanceScope>().data());
      break;
    case ScopeKind::WasmFunction:
      init(scope->as<WasmFunctionScope>().data());
      break;
  }
}

BindingIter::BindingIter(JSScript* script) : BindingIter(script->bodyScope()) {}

void BindingIter::init(LexicalScope::Data& data, uint32_t firstFrameSlot,
                       uint8_t flags) {
  // Named lambda scopes can only have environment slots. If the callee
  // isn't closed over, it is accessed via JSOP_CALLEE.
  if (flags & IsNamedLambda) {
    // Named lambda binding is weird. Normal BindingKind ordering rules
    // don't apply.
    init(0, 0, 0, 0, 0, CanHaveEnvironmentSlots | flags, firstFrameSlot,
         JSSLOT_FREE(&LexicalEnvironmentObject::class_),
         data.trailingNames.start(), data.length);
  } else {
    //            imports - [0, 0)
    // positional formals - [0, 0)
    //      other formals - [0, 0)
    //               vars - [0, 0)
    //               lets - [0, data.constStart)
    //             consts - [data.constStart, data.length)
    init(0, 0, 0, 0, data.constStart,
         CanHaveFrameSlots | CanHaveEnvironmentSlots | flags, firstFrameSlot,
         JSSLOT_FREE(&LexicalEnvironmentObject::class_),
         data.trailingNames.start(), data.length);
  }
}

void BindingIter::init(FunctionScope::Data& data, uint8_t flags) {
  flags = CanHaveFrameSlots | CanHaveEnvironmentSlots | flags;
  if (!(flags & HasFormalParameterExprs)) {
    flags |= CanHaveArgumentSlots;
  }

  //            imports - [0, 0)
  // positional formals - [0, data.nonPositionalFormalStart)
  //      other formals - [data.nonPositionalParamStart, data.varStart)
  //               vars - [data.varStart, data.length)
  //               lets - [data.length, data.length)
  //             consts - [data.length, data.length)
  init(0, data.nonPositionalFormalStart, data.varStart, data.length,
       data.length, flags, 0, JSSLOT_FREE(&CallObject::class_),
       data.trailingNames.start(), data.length);
}

void BindingIter::init(VarScope::Data& data, uint32_t firstFrameSlot) {
  //            imports - [0, 0)
  // positional formals - [0, 0)
  //      other formals - [0, 0)
  //               vars - [0, data.length)
  //               lets - [data.length, data.length)
  //             consts - [data.length, data.length)
  init(0, 0, 0, data.length, data.length,
       CanHaveFrameSlots | CanHaveEnvironmentSlots, firstFrameSlot,
       JSSLOT_FREE(&VarEnvironmentObject::class_), data.trailingNames.start(),
       data.length);
}

void BindingIter::init(GlobalScope::Data& data) {
  //            imports - [0, 0)
  // positional formals - [0, 0)
  //      other formals - [0, 0)
  //               vars - [0, data.letStart)
  //               lets - [data.letStart, data.constStart)
  //             consts - [data.constStart, data.length)
  init(0, 0, 0, data.letStart, data.constStart, CannotHaveSlots, UINT32_MAX,
       UINT32_MAX, data.trailingNames.start(), data.length);
}

void BindingIter::init(EvalScope::Data& data, bool strict) {
  uint32_t flags;
  uint32_t firstFrameSlot;
  uint32_t firstEnvironmentSlot;
  if (strict) {
    flags = CanHaveFrameSlots | CanHaveEnvironmentSlots;
    firstFrameSlot = 0;
    firstEnvironmentSlot = JSSLOT_FREE(&VarEnvironmentObject::class_);
  } else {
    flags = CannotHaveSlots;
    firstFrameSlot = UINT32_MAX;
    firstEnvironmentSlot = UINT32_MAX;
  }

  //            imports - [0, 0)
  // positional formals - [0, 0)
  //      other formals - [0, 0)
  //               vars - [0, data.length)
  //               lets - [data.length, data.length)
  //             consts - [data.length, data.length)
  init(0, 0, 0, data.length, data.length, flags, firstFrameSlot,
       firstEnvironmentSlot, data.trailingNames.start(), data.length);
}

void BindingIter::init(ModuleScope::Data& data) {
  //            imports - [0, data.varStart)
  // positional formals - [data.varStart, data.varStart)
  //      other formals - [data.varStart, data.varStart)
  //               vars - [data.varStart, data.letStart)
  //               lets - [data.letStart, data.constStart)
  //             consts - [data.constStart, data.length)
  init(data.varStart, data.varStart, data.varStart, data.letStart,
       data.constStart, CanHaveFrameSlots | CanHaveEnvironmentSlots, 0,
       JSSLOT_FREE(&ModuleEnvironmentObject::class_),
       data.trailingNames.start(), data.length);
}

void BindingIter::init(WasmInstanceScope::Data& data) {
  //            imports - [0, 0)
  // positional formals - [0, 0)
  //      other formals - [0, 0)
  //               vars - [0, data.length)
  //               lets - [data.length, data.length)
  //             consts - [data.length, data.length)
  init(0, 0, 0, data.length, data.length,
       CanHaveFrameSlots | CanHaveEnvironmentSlots, UINT32_MAX, UINT32_MAX,
       data.trailingNames.start(), data.length);
}

void BindingIter::init(WasmFunctionScope::Data& data) {
  //            imports - [0, 0)
  // positional formals - [0, 0)
  //      other formals - [0, 0)
  //               vars - [0, data.length)
  //               lets - [data.length, data.length)
  //             consts - [data.length, data.length)
  init(0, 0, 0, data.length, data.length,
       CanHaveFrameSlots | CanHaveEnvironmentSlots, UINT32_MAX, UINT32_MAX,
       data.trailingNames.start(), data.length);
}

PositionalFormalParameterIter::PositionalFormalParameterIter(Scope* scope)
    : BindingIter(scope) {
  // Reinit with flags = 0, i.e., iterate over all positional parameters.
  if (scope->is<FunctionScope>()) {
    init(scope->as<FunctionScope>().data(), /* flags = */ 0);
  }
  settle();
}

PositionalFormalParameterIter::PositionalFormalParameterIter(JSScript* script)
    : PositionalFormalParameterIter(script->bodyScope()) {}

void js::DumpBindings(JSContext* cx, Scope* scopeArg) {
  RootedScope scope(cx, scopeArg);
  for (Rooted<BindingIter> bi(cx, BindingIter(scope)); bi; bi++) {
    UniqueChars bytes = AtomToPrintableString(cx, bi.name());
    if (!bytes) {
      return;
    }
    fprintf(stderr, "%s %s ", BindingKindString(bi.kind()), bytes.get());
    switch (bi.location().kind()) {
      case BindingLocation::Kind::Global:
        if (bi.isTopLevelFunction()) {
          fprintf(stderr, "global function\n");
        } else {
          fprintf(stderr, "global\n");
        }
        break;
      case BindingLocation::Kind::Argument:
        fprintf(stderr, "arg slot %u\n", bi.location().argumentSlot());
        break;
      case BindingLocation::Kind::Frame:
        fprintf(stderr, "frame slot %u\n", bi.location().slot());
        break;
      case BindingLocation::Kind::Environment:
        fprintf(stderr, "env slot %u\n", bi.location().slot());
        break;
      case BindingLocation::Kind::NamedLambdaCallee:
        fprintf(stderr, "named lambda callee\n");
        break;
      case BindingLocation::Kind::Import:
        fprintf(stderr, "import\n");
        break;
    }
  }
}

static JSAtom* GetFrameSlotNameInScope(Scope* scope, uint32_t slot) {
  for (BindingIter bi(scope); bi; bi++) {
    BindingLocation loc = bi.location();
    if (loc.kind() == BindingLocation::Kind::Frame && loc.slot() == slot) {
      return bi.name();
    }
  }
  return nullptr;
}

JSAtom* js::FrameSlotName(JSScript* script, jsbytecode* pc) {
  MOZ_ASSERT(IsLocalOp(JSOp(*pc)));
  uint32_t slot = GET_LOCALNO(pc);
  MOZ_ASSERT(slot < script->nfixed());

  // Look for it in the body scope first.
  if (JSAtom* name = GetFrameSlotNameInScope(script->bodyScope(), slot)) {
    return name;
  }

  // If this is a function script and there is an extra var scope, look for
  // it there.
  if (script->functionHasExtraBodyVarScope()) {
    if (JSAtom* name = GetFrameSlotNameInScope(
            script->functionExtraBodyVarScope(), slot)) {
      return name;
    }
  }

  // If not found, look for it in a lexical scope.
  for (ScopeIter si(script->innermostScope(pc)); si; si++) {
    if (!si.scope()->is<LexicalScope>()) {
      continue;
    }
    LexicalScope& lexicalScope = si.scope()->as<LexicalScope>();

    // Is the slot within bounds of the current lexical scope?
    if (slot < lexicalScope.firstFrameSlot()) {
      continue;
    }
    if (slot >= lexicalScope.nextFrameSlot()) {
      break;
    }

    // If so, get the name.
    if (JSAtom* name = GetFrameSlotNameInScope(&lexicalScope, slot)) {
      return name;
    }
  }

  MOZ_CRASH("Frame slot not found");
}

JS::ubi::Node::Size JS::ubi::Concrete<Scope>::size(
    mozilla::MallocSizeOf mallocSizeOf) const {
  return js::gc::Arena::thingSize(get().asTenured().getAllocKind()) +
         get().sizeOfExcludingThis(mallocSizeOf);
}
