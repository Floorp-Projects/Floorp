/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Scope.h"

#include "mozilla/ScopeExit.h"

#include "jsscript.h"
#include "builtin/ModuleObject.h"
#include "gc/Allocator.h"
#include "vm/EnvironmentObject.h"
#include "vm/Runtime.h"

#include "vm/Shape-inl.h"

using namespace js;

using mozilla::Maybe;
using mozilla::MakeScopeExit;
using mozilla::Move;
using mozilla::Nothing;
using mozilla::Some;

const char*
js::BindingKindString(BindingKind kind)
{
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

const char*
js::ScopeKindString(ScopeKind kind)
{
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
    }
    MOZ_CRASH("Bad ScopeKind");
}

static Shape*
EmptyEnvironmentShape(ExclusiveContext* cx, const Class* cls, uint32_t numSlots,
                      uint32_t baseShapeFlags)
{
    // Put as many slots into the object header as possible.
    uint32_t numFixed = gc::GetGCKindSlots(gc::GetGCObjectKind(numSlots));
    return EmptyShape::getInitialShape(cx, cls, TaggedProto(nullptr), numFixed,
                                       baseShapeFlags);
}

static Shape*
NextEnvironmentShape(ExclusiveContext* cx, HandleAtom name, BindingKind bindKind, uint32_t slot,
                     StackBaseShape& stackBase, HandleShape shape)
{
    UnownedBaseShape* base = BaseShape::getUnowned(cx, stackBase);
    if (!base)
        return nullptr;

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
    Rooted<StackShape> child(cx, StackShape(base, id, slot, attrs, 0));
    return cx->zone()->propertyTree.getChild(cx, shape, child);
}

static Shape*
CreateEnvironmentShape(ExclusiveContext* cx, BindingIter& bi, const Class* cls,
                       uint32_t numSlots, uint32_t baseShapeFlags)
{
    RootedShape shape(cx, EmptyEnvironmentShape(cx, cls, numSlots, baseShapeFlags));
    if (!shape)
        return nullptr;

    RootedAtom name(cx);
    StackBaseShape stackBase(cx, cls, baseShapeFlags);
    for (; bi; bi++) {
        BindingLocation loc = bi.location();
        if (loc.kind() == BindingLocation::Kind::Environment) {
            name = bi.name();
            shape = NextEnvironmentShape(cx, name, bi.kind(), loc.slot(), stackBase, shape);
            if (!shape)
                return nullptr;
        }
    }

    return shape;
}

template <typename ConcreteScope>
static UniquePtr<typename ConcreteScope::Data>
CopyScopeData(ExclusiveContext* cx, Handle<typename ConcreteScope::Data*> data)
{
    size_t dataSize = ConcreteScope::sizeOfData(data->length);
    uint8_t* copyBytes = cx->zone()->pod_malloc<uint8_t>(dataSize);
    if (!copyBytes) {
        ReportOutOfMemory(cx);
        return nullptr;
    }

    mozilla::PodCopy<uint8_t>(copyBytes, reinterpret_cast<uint8_t*>(data.get()), dataSize);
    auto dataCopy = reinterpret_cast<typename ConcreteScope::Data*>(copyBytes);
    return UniquePtr<typename ConcreteScope::Data>(dataCopy);
}

template <typename ConcreteScope>
static UniquePtr<typename ConcreteScope::Data>
CopyScopeData(ExclusiveContext* cx, BindingIter& bi, Handle<typename ConcreteScope::Data*> data,
              const Class* cls, uint32_t baseShapeFlags, MutableHandleShape envShape)
{
    // Copy a fresh BindingIter for use below.
    BindingIter freshBi(bi);

    // Iterate through all bindings. This counts the number of environment
    // slots needed and computes the maximum frame slot.
    while (bi)
        bi++;
    data->nextFrameSlot = bi.canHaveFrameSlots() ? bi.nextFrameSlot() : LOCALNO_LIMIT;

    // Make a new environment shape if any environment slots were used.
    if (bi.nextEnvironmentSlot() == JSSLOT_FREE(cls)) {
        envShape.set(nullptr);
    } else {
        envShape.set(CreateEnvironmentShape(cx, freshBi, cls, bi.nextEnvironmentSlot(),
                                            baseShapeFlags));
        if (!envShape)
            return nullptr;
    }

    return CopyScopeData<ConcreteScope>(cx, data);
}

template <typename ConcreteScope>
static UniquePtr<typename ConcreteScope::Data>
NewEmptyScopeData(ExclusiveContext* cx, uint32_t length = 0)
{
    uint8_t* bytes = cx->zone()->pod_calloc<uint8_t>(ConcreteScope::sizeOfData(length));
    if (!bytes)
        ReportOutOfMemory(cx);
    auto data = reinterpret_cast<typename ConcreteScope::Data*>(bytes);
    return UniquePtr<typename ConcreteScope::Data>(data);
}

static bool
XDRBindingName(XDRState<XDR_ENCODE>* xdr, BindingName* bindingName)
{
    JSContext* cx = xdr->cx();

    RootedAtom atom(cx, bindingName->name());
    bool hasAtom = !!atom;

    uint8_t u8 = uint8_t(hasAtom << 1) | uint8_t(bindingName->closedOver());
    if (!xdr->codeUint8(&u8))
        return false;

    if (atom && !XDRAtom(xdr, &atom))
        return false;

    return true;
}

static bool
XDRBindingName(XDRState<XDR_DECODE>* xdr, BindingName* bindingName)
{
    JSContext* cx = xdr->cx();

    uint8_t u8;
    if (!xdr->codeUint8(&u8))
        return false;

    bool closedOver = u8 & 1;
    bool hasAtom = u8 >> 1;

    RootedAtom atom(cx);
    if (hasAtom && !XDRAtom(xdr, &atom))
        return false;

    *bindingName = BindingName(atom, closedOver);

    return true;
}

template <typename ConcreteScope, XDRMode mode>
/* static */ bool
Scope::XDRSizedBindingNames(XDRState<mode>* xdr, Handle<ConcreteScope*> scope,
                            MutableHandle<typename ConcreteScope::Data*> data)
{
    MOZ_ASSERT(!data);

    JSContext* cx = xdr->cx();

    uint32_t length;
    if (mode == XDR_ENCODE)
        length = scope->data().length;
    if (!xdr->codeUint32(&length))
        return false;

    if (mode == XDR_ENCODE) {
        data.set(&scope->data());
    } else {
        data.set(NewEmptyScopeData<ConcreteScope>(cx, length).release());
        if (!data)
            return false;
        data->length = length;
    }

    for (uint32_t i = 0; i < length; i++) {
        if (!XDRBindingName(xdr, &data->names[i])) {
            if (mode == XDR_DECODE) {
                js_free(data);
                data.set(nullptr);
            }

            return false;
        }
    }

    return true;
}

/* static */ Scope*
Scope::create(ExclusiveContext* cx, ScopeKind kind, HandleScope enclosing, HandleShape envShape)
{
    Scope* scope = Allocate<Scope>(cx);
    if (scope)
        new (scope) Scope(kind, enclosing, envShape);
    return scope;
}

template <typename T, typename D>
/* static */ Scope*
Scope::create(ExclusiveContext* cx, ScopeKind kind, HandleScope enclosing,
              HandleShape envShape, mozilla::UniquePtr<T, D> data)
{
    Scope* scope = create(cx, kind, enclosing, envShape);
    if (!scope)
        return nullptr;

    // It is an invariant that all Scopes that have data (currently, all
    // ScopeKinds except With) must have non-null data.
    MOZ_ASSERT(data);
    scope->initData(Move(data));

    return scope;
}

uint32_t
Scope::chainLength() const
{
    uint32_t length = 0;
    for (ScopeIter si(const_cast<Scope*>(this)); si; si++)
        length++;
    return length;
}

uint32_t
Scope::environmentChainLength() const
{
    uint32_t length = 0;
    for (ScopeIter si(const_cast<Scope*>(this)); si; si++) {
        if (si.hasSyntacticEnvironment())
            length++;
    }
    return length;
}

Shape*
Scope::maybeCloneEnvironmentShape(JSContext* cx)
{
    // Clone the environment shape if cloning into a different zone.
    if (environmentShape_ && environmentShape_->zoneFromAnyThread() != cx->zone()) {
        BindingIter bi(this);
        return CreateEnvironmentShape(cx, bi,
                                      environmentShape_->getObjectClass(),
                                      environmentShape_->slotSpan(),
                                      environmentShape_->getObjectFlags());
    }
    return environmentShape_;
}

/* static */ Scope*
Scope::clone(JSContext* cx, HandleScope scope, HandleScope enclosing)
{
    RootedShape envShape(cx);
    if (scope->environmentShape()) {
        envShape = scope->maybeCloneEnvironmentShape(cx);
        if (!envShape)
            return nullptr;
    }

    switch (scope->kind_) {
      case ScopeKind::Function:
        MOZ_CRASH("Use FunctionScope::clone.");
        break;

      case ScopeKind::FunctionBodyVar:
      case ScopeKind::ParameterExpressionVar: {
        Rooted<VarScope::Data*> original(cx, &scope->as<VarScope>().data());
        UniquePtr<VarScope::Data> dataClone = CopyScopeData<VarScope>(cx, original);
        if (!dataClone)
            return nullptr;
        return create(cx, scope->kind_, enclosing, envShape, Move(dataClone));
      }

      case ScopeKind::Lexical:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
      case ScopeKind::NamedLambda:
      case ScopeKind::StrictNamedLambda: {
        Rooted<LexicalScope::Data*> original(cx, &scope->as<LexicalScope>().data());
        UniquePtr<LexicalScope::Data> dataClone = CopyScopeData<LexicalScope>(cx, original);
        if (!dataClone)
            return nullptr;
        return create(cx, scope->kind_, enclosing, envShape, Move(dataClone));
      }

      case ScopeKind::With:
        return create(cx, scope->kind_, enclosing, envShape);

      case ScopeKind::Eval:
      case ScopeKind::StrictEval: {
        Rooted<EvalScope::Data*> original(cx, &scope->as<EvalScope>().data());
        UniquePtr<EvalScope::Data> dataClone = CopyScopeData<EvalScope>(cx, original);
        if (!dataClone)
            return nullptr;
        return create(cx, scope->kind_, enclosing, envShape, Move(dataClone));
      }

      case ScopeKind::Global:
      case ScopeKind::NonSyntactic:
        MOZ_CRASH("Use GlobalScope::clone.");
        break;

      case ScopeKind::Module:
        MOZ_CRASH("NYI");
        break;
    }

    return nullptr;
}

void
Scope::finalize(FreeOp* fop)
{
    if (data_) {
        fop->free_(reinterpret_cast<void*>(data_));
        data_ = 0;
    }
}

size_t
Scope::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    if (data_)
        return mallocSizeOf(reinterpret_cast<void*>(data_));
    return 0;
}

void
Scope::dump()
{
    for (ScopeIter si(this); si; si++) {
        fprintf(stderr, "%s [%p]", ScopeKindString(si.kind()), si.scope());
        if (si.scope()->enclosing())
            fprintf(stderr, " -> ");
    }
    fprintf(stderr, "\n");
}

uint32_t
LexicalScope::firstFrameSlot() const
{
    switch (kind()) {
      case ScopeKind::Lexical:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
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

/* static */ uint32_t
LexicalScope::nextFrameSlot(Scope* scope)
{
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
        }
    }
    MOZ_CRASH("Not an enclosing intra-frame Scope");
}

/* static */ LexicalScope*
LexicalScope::create(ExclusiveContext* cx, ScopeKind kind, Handle<Data*> data,
                     uint32_t firstFrameSlot, HandleScope enclosing)
{
    bool isNamedLambda = kind == ScopeKind::NamedLambda || kind == ScopeKind::StrictNamedLambda;

    MOZ_ASSERT(data, "LexicalScopes should not be created if there are no bindings.");
    MOZ_ASSERT_IF(!isNamedLambda && firstFrameSlot != 0,
                  firstFrameSlot == nextFrameSlot(enclosing));
    MOZ_ASSERT_IF(isNamedLambda, firstFrameSlot == LOCALNO_LIMIT);

    // The data that's passed in may be from the frontend and LifoAlloc'd.
    // Copy it now that we're creating a permanent VM scope.
    RootedShape envShape(cx);
    BindingIter bi(*data, firstFrameSlot, isNamedLambda);
    Rooted<UniquePtr<Data>> copy(cx,
        CopyScopeData<LexicalScope>(cx, bi, data,
                                    &LexicalEnvironmentObject::class_,
                                    BaseShape::NOT_EXTENSIBLE | BaseShape::DELEGATE,
                                    &envShape));
    if (!copy)
        return nullptr;

    Scope* scope = Scope::create(cx, kind, enclosing, envShape, Move(copy.get()));
    if (!scope)
        return nullptr;
    MOZ_ASSERT(scope->as<LexicalScope>().firstFrameSlot() == firstFrameSlot);
    return &scope->as<LexicalScope>();
}

/* static */ Shape*
LexicalScope::getEmptyExtensibleEnvironmentShape(ExclusiveContext* cx)
{
    const Class* cls = &LexicalEnvironmentObject::class_;
    return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls), BaseShape::DELEGATE);
}

template <XDRMode mode>
/* static */ bool
LexicalScope::XDR(XDRState<mode>* xdr, ScopeKind kind, HandleScope enclosing,
                  MutableHandleScope scope)
{
    JSContext* cx = xdr->cx();

    Rooted<Data*> data(cx);
    if (!XDRSizedBindingNames<LexicalScope>(xdr, scope.as<LexicalScope>(), &data))
        return false;

    {
        auto freeOnLeave = MakeScopeExit([&data]() {
            if (mode == XDR_DECODE)
                js_free(data);
        });

        uint32_t firstFrameSlot;
        uint32_t nextFrameSlot;
        if (mode == XDR_ENCODE) {
            firstFrameSlot = scope->as<LexicalScope>().firstFrameSlot();
            nextFrameSlot = data->nextFrameSlot;
        }

        if (!xdr->codeUint32(&data->constStart))
            return false;
        if (!xdr->codeUint32(&firstFrameSlot))
            return false;
        if (!xdr->codeUint32(&nextFrameSlot))
            return false;

        if (mode == XDR_DECODE) {
            scope.set(create(cx, kind, data, firstFrameSlot, enclosing));
            if (!scope)
                return false;

            // nextFrameSlot is used only for this correctness check.
            MOZ_ASSERT(nextFrameSlot == scope->as<LexicalScope>().data().nextFrameSlot);
        }
    }

    return true;
}

template
/* static */ bool
LexicalScope::XDR(XDRState<XDR_ENCODE>* xdr, ScopeKind kind, HandleScope enclosing,
                  MutableHandleScope scope);

template
/* static */ bool
LexicalScope::XDR(XDRState<XDR_DECODE>* xdr, ScopeKind kind, HandleScope enclosing,
                  MutableHandleScope scope);

static inline uint32_t
FunctionScopeEnvShapeFlags(bool hasParameterExprs)
{
    if (hasParameterExprs)
        return BaseShape::DELEGATE;
    return BaseShape::QUALIFIED_VAROBJ | BaseShape::DELEGATE;
}

/* static */ UniquePtr<FunctionScope::Data>
FunctionScope::copyData(ExclusiveContext* cx, Handle<Data*> data,
                        bool hasParameterExprs, MutableHandleShape envShape)
{
    if (data) {
        BindingIter bi(*data, hasParameterExprs);
        uint32_t shapeFlags = FunctionScopeEnvShapeFlags(hasParameterExprs);
        return CopyScopeData<FunctionScope>(cx, bi, data,
                                            &CallObject::class_,
                                            shapeFlags, envShape);
    }
    return NewEmptyScopeData<FunctionScope>(cx);
}

/* static */ FunctionScope*
FunctionScope::create(ExclusiveContext* cx, Handle<Data*> data,
                      bool hasParameterExprs, bool needsEnvironment,
                      HandleFunction fun, HandleScope enclosing)
{
    MOZ_ASSERT(fun->isTenured());

    // FunctionScope::Data has GCManagedDeletePolicy because it contains a
    // GCPtr. Destruction of |copy| below may trigger calls into the GC.
    Rooted<FunctionScope*> funScope(cx);

    {
        // The data that's passed in may be from the frontend and LifoAlloc'd.
        // Copy it now that we're creating a permanent VM scope.
        RootedShape envShape(cx);
        Rooted<UniquePtr<Data>> copy(cx, copyData(cx, data, hasParameterExprs, &envShape));
        if (!copy)
            return nullptr;

        copy->hasParameterExprs = hasParameterExprs;

        // An environment may be needed regardless of existence of any closed over
        // bindings:
        //   - Extensible scopes (i.e., due to direct eval)
        //   - Needing a home object
        //   - Being a derived class constructor
        //   - Being a generator
        if (!envShape && needsEnvironment) {
            envShape = getEmptyEnvironmentShape(cx, hasParameterExprs);
            if (!envShape)
                return nullptr;
        }

        Scope* scope = Scope::create(cx, ScopeKind::Function, enclosing, envShape);
        if (!scope)
            return nullptr;

        copy->canonicalFunction.init(fun);

        funScope = &scope->as<FunctionScope>();
        funScope->initData(Move(copy.get()));
    }

    return funScope;
}

JSScript*
FunctionScope::script() const
{
    return canonicalFunction()->nonLazyScript();
}

/* static */ Shape*
FunctionScope::getEmptyEnvironmentShape(ExclusiveContext* cx, bool hasParameterExprs)
{
    const Class* cls = &CallObject::class_;
    uint32_t shapeFlags = FunctionScopeEnvShapeFlags(hasParameterExprs);
    return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls), shapeFlags);
}

/* static */ FunctionScope*
FunctionScope::clone(JSContext* cx, Handle<FunctionScope*> scope, HandleFunction fun,
                     HandleScope enclosing)
{
    MOZ_ASSERT(fun != scope->canonicalFunction());

    // FunctionScope::Data has GCManagedDeletePolicy because it contains a
    // GCPtr. Destruction of |dataClone| below may trigger calls into the GC.
    Rooted<FunctionScope*> funScopeClone(cx);

    {
        RootedShape envShape(cx);
        if (scope->environmentShape()) {
            envShape = scope->maybeCloneEnvironmentShape(cx);
            if (!envShape)
                return nullptr;
        }

        Rooted<Data*> dataOriginal(cx, &scope->as<FunctionScope>().data());
        Rooted<UniquePtr<Data>> dataClone(cx, CopyScopeData<FunctionScope>(cx, dataOriginal));
        if (!dataClone)
            return nullptr;

        Scope* scopeClone= Scope::create(cx, scope->kind(), enclosing, envShape);
        if (!scopeClone)
            return nullptr;

        dataClone->canonicalFunction.init(fun);

        funScopeClone = &scopeClone->as<FunctionScope>();
        funScopeClone->initData(Move(dataClone.get()));
    }

    return funScopeClone;
}

template <XDRMode mode>
/* static */ bool
FunctionScope::XDR(XDRState<mode>* xdr, HandleFunction fun, HandleScope enclosing,
                   MutableHandleScope scope)
{
    JSContext* cx = xdr->cx();
    Rooted<Data*> data(cx);
    if (!XDRSizedBindingNames<FunctionScope>(xdr, scope.as<FunctionScope>(), &data))
        return false;

    {
        auto freeOnLeave = MakeScopeExit([&data]() {
            if (mode == XDR_DECODE)
                js_free(data);
        });

        uint8_t needsEnvironment;
        uint8_t hasParameterExprs;
        uint32_t nextFrameSlot;
        if (mode == XDR_ENCODE) {
            needsEnvironment = scope->hasEnvironment();
            hasParameterExprs = data->hasParameterExprs;
            nextFrameSlot = data->nextFrameSlot;
        }
        if (!xdr->codeUint8(&needsEnvironment))
            return false;
        if (!xdr->codeUint8(&hasParameterExprs))
            return false;
        if (!xdr->codeUint16(&data->nonPositionalFormalStart))
            return false;
        if (!xdr->codeUint16(&data->varStart))
            return false;
        if (!xdr->codeUint32(&nextFrameSlot))
            return false;

        if (mode == XDR_DECODE) {
            if (!data->length) {
                MOZ_ASSERT(!data->nonPositionalFormalStart);
                MOZ_ASSERT(!data->varStart);
                MOZ_ASSERT(!data->nextFrameSlot);
                js_free(data);
                data = nullptr;
            }

            scope.set(create(cx, data, hasParameterExprs, needsEnvironment, fun, enclosing));
            if (!scope)
                return false;

            // nextFrameSlot is used only for this correctness check.
            MOZ_ASSERT(nextFrameSlot == scope->as<FunctionScope>().data().nextFrameSlot);
        }
    }

    return true;
}

template
/* static */ bool
FunctionScope::XDR(XDRState<XDR_ENCODE>* xdr, HandleFunction fun, HandleScope enclosing,
                   MutableHandleScope scope);

template
/* static */ bool
FunctionScope::XDR(XDRState<XDR_DECODE>* xdr, HandleFunction fun, HandleScope enclosing,
                   MutableHandleScope scope);

static const uint32_t VarScopeEnvShapeFlags =
    BaseShape::QUALIFIED_VAROBJ | BaseShape::DELEGATE;

/* static */ UniquePtr<VarScope::Data>
VarScope::copyData(ExclusiveContext* cx, Handle<Data*> data, uint32_t firstFrameSlot,
                   MutableHandleShape envShape)
{
    if (data) {
        BindingIter bi(*data, firstFrameSlot);
        return CopyScopeData<VarScope>(cx, bi, data,
                                       &VarEnvironmentObject::class_,
                                       VarScopeEnvShapeFlags, envShape);
    }

    UniquePtr<Data> empty = NewEmptyScopeData<VarScope>(cx);
    if (empty)
        empty->nextFrameSlot = firstFrameSlot;
    return empty;
}

/* static */ VarScope*
VarScope::create(ExclusiveContext* cx, ScopeKind kind, Handle<Data*> data,
                 uint32_t firstFrameSlot, bool needsEnvironment, HandleScope enclosing)
{
    // The data that's passed in may be from the frontend and LifoAlloc'd.
    // Copy it now that we're creating a permanent VM scope.
    RootedShape envShape(cx);
    Rooted<UniquePtr<Data>> copy(cx, copyData(cx, data, firstFrameSlot, &envShape));
    if (!copy)
        return nullptr;

    // An environment may be needed regardless of existence of any closed over
    // bindings:
    //   - Extensible scopes (i.e., due to direct eval)
    //   - Being a generator
    if (!envShape && needsEnvironment) {
        envShape = getEmptyEnvironmentShape(cx);
        if (!envShape)
            return nullptr;
    }

    Scope* scope = Scope::create(cx, kind, enclosing, envShape, Move(copy.get()));
    if (!scope)
        return nullptr;
    return &scope->as<VarScope>();
}

/* static */ Shape*
VarScope::getEmptyEnvironmentShape(ExclusiveContext* cx)
{
    const Class* cls = &VarEnvironmentObject::class_;
    return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls), VarScopeEnvShapeFlags);
}

uint32_t
VarScope::firstFrameSlot() const
{
    if (enclosing()->is<FunctionScope>())
        return enclosing()->as<FunctionScope>().nextFrameSlot();
    return 0;
}

template <XDRMode mode>
/* static */ bool
VarScope::XDR(XDRState<mode>* xdr, ScopeKind kind, HandleScope enclosing,
              MutableHandleScope scope)
{
    JSContext* cx = xdr->cx();
    Rooted<Data*> data(cx);
    if (!XDRSizedBindingNames<VarScope>(xdr, scope.as<VarScope>(), &data))
        return false;

    {
        auto freeOnLeave = MakeScopeExit([&data]() {
            if (mode == XDR_DECODE)
                js_free(data);
        });

        uint8_t needsEnvironment;
        uint32_t firstFrameSlot;
        uint32_t nextFrameSlot;
        if (mode == XDR_ENCODE) {
            needsEnvironment = scope->hasEnvironment();
            firstFrameSlot = scope->as<VarScope>().firstFrameSlot();
            nextFrameSlot = data->nextFrameSlot;
        }
        if (!xdr->codeUint8(&needsEnvironment))
            return false;
        if (!xdr->codeUint32(&firstFrameSlot))
            return false;
        if (!xdr->codeUint32(&nextFrameSlot))
            return false;

        if (mode == XDR_DECODE) {
            if (!data->length) {
                MOZ_ASSERT(!data->nextFrameSlot);
                js_free(data);
                data = nullptr;
            }

            scope.set(create(cx, kind, data, firstFrameSlot, needsEnvironment, enclosing));
            if (!scope)
                return false;

            // nextFrameSlot is used only for this correctness check.
            MOZ_ASSERT(nextFrameSlot == scope->as<VarScope>().data().nextFrameSlot);
        }
    }

    return true;
}

template
/* static */ bool
VarScope::XDR(XDRState<XDR_ENCODE>* xdr, ScopeKind kind, HandleScope enclosing,
              MutableHandleScope scope);

template
/* static */ bool
VarScope::XDR(XDRState<XDR_DECODE>* xdr, ScopeKind kind, HandleScope enclosing,
              MutableHandleScope scope);

/* static */ UniquePtr<GlobalScope::Data>
GlobalScope::copyData(ExclusiveContext *cx, Handle<Data*> data)
{
    if (data) {
        // The global scope has no environment shape. Its environment is the
        // global lexical scope and the global object or non-syntactic objects
        // created by embedding, all of which are not only extensible but may
        // have names on them deleted.
        return CopyScopeData<GlobalScope>(cx, data);
    }
    return NewEmptyScopeData<GlobalScope>(cx);
}

/* static */ GlobalScope*
GlobalScope::create(ExclusiveContext* cx, ScopeKind kind, Handle<Data*> data)
{
    // The data that's passed in may be from the frontend and LifoAlloc'd.
    // Copy it now that we're creating a permanent VM scope.
    Rooted<UniquePtr<Data>> copy(cx, copyData(cx, data));
    if (!copy)
        return nullptr;

    Scope* scope = Scope::create(cx, kind, nullptr, nullptr, Move(copy.get()));
    if (!scope)
        return nullptr;
    return &scope->as<GlobalScope>();
}

/* static */ GlobalScope*
GlobalScope::clone(JSContext* cx, Handle<GlobalScope*> scope, ScopeKind kind)
{
    Rooted<Data*> dataOriginal(cx, &scope->as<GlobalScope>().data());
    Rooted<UniquePtr<Data>> dataClone(cx, CopyScopeData<GlobalScope>(cx, dataOriginal));
    if (!dataClone)
        return nullptr;

    Scope* scopeClone = Scope::create(cx, kind, nullptr, nullptr, Move(dataClone.get()));
    if (!scopeClone)
        return nullptr;
    return &scopeClone->as<GlobalScope>();
}

template <XDRMode mode>
/* static */ bool
GlobalScope::XDR(XDRState<mode>* xdr, ScopeKind kind, MutableHandleScope scope)
{
    MOZ_ASSERT((mode == XDR_DECODE) == !scope);

    JSContext* cx = xdr->cx();
    Rooted<Data*> data(cx);
    if (!XDRSizedBindingNames<GlobalScope>(xdr, scope.as<GlobalScope>(), &data))
        return false;

    {
        auto freeOnLeave = MakeScopeExit([&data]() {
            if (mode == XDR_DECODE)
                js_free(data);
        });

        if (!xdr->codeUint32(&data->letStart))
            return false;
        if (!xdr->codeUint32(&data->constStart))
            return false;

        if (mode == XDR_DECODE) {
            if (!data->length) {
                MOZ_ASSERT(!data->letStart);
                MOZ_ASSERT(!data->constStart);
                js_free(data);
                data = nullptr;
            }

            scope.set(create(cx, kind, data));
            if (!scope)
                return false;
        }
    }

    return true;
}

template
/* static */ bool
GlobalScope::XDR(XDRState<XDR_ENCODE>* xdr, ScopeKind kind, MutableHandleScope scope);

template
/* static */ bool
GlobalScope::XDR(XDRState<XDR_DECODE>* xdr, ScopeKind kind, MutableHandleScope scope);

/* static */ WithScope*
WithScope::create(ExclusiveContext* cx, HandleScope enclosing)
{
    Scope* scope = Scope::create(cx, ScopeKind::With, enclosing, nullptr);
    return static_cast<WithScope*>(scope);
}

static const uint32_t EvalScopeEnvShapeFlags =
    BaseShape::QUALIFIED_VAROBJ | BaseShape::DELEGATE;

/* static */ UniquePtr<EvalScope::Data>
EvalScope::copyData(ExclusiveContext* cx, ScopeKind scopeKind, Handle<Data*> data,
                    MutableHandleShape envShape)
{
    if (data) {
        if (scopeKind == ScopeKind::StrictEval) {
            BindingIter bi(*data, true);
            return CopyScopeData<EvalScope>(cx, bi, data,
                                            &VarEnvironmentObject::class_,
                                            EvalScopeEnvShapeFlags, envShape);
        }
        return CopyScopeData<EvalScope>(cx, data);
    }
    return NewEmptyScopeData<EvalScope>(cx);
}

/* static */ EvalScope*
EvalScope::create(ExclusiveContext* cx, ScopeKind scopeKind, Handle<Data*> data,
                  HandleScope enclosing)
{
    // The data that's passed in may be from the frontend and LifoAlloc'd.
    // Copy it now that we're creating a permanent VM scope.
    RootedShape envShape(cx);
    Rooted<UniquePtr<Data>> copy(cx, copyData(cx, scopeKind, data, &envShape));
    if (!copy)
        return nullptr;

    // Strict eval and direct eval in parameter expressions always get their own
    // var environment even if there are no bindings.
    if (!envShape && scopeKind == ScopeKind::StrictEval) {
        envShape = getEmptyEnvironmentShape(cx);
        if (!envShape)
            return nullptr;
    }

    Scope* scope = Scope::create(cx, scopeKind, enclosing, envShape, Move(copy.get()));
    if (!scope)
        return nullptr;
    return &scope->as<EvalScope>();
}

/* static */ Scope*
EvalScope::nearestVarScopeForDirectEval(Scope* scope)
{
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

/* static */ Shape*
EvalScope::getEmptyEnvironmentShape(ExclusiveContext* cx)
{
    const Class* cls = &VarEnvironmentObject::class_;
    return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls), EvalScopeEnvShapeFlags);
}

template <XDRMode mode>
/* static */ bool
EvalScope::XDR(XDRState<mode>* xdr, ScopeKind kind, HandleScope enclosing,
               MutableHandleScope scope)
{
    JSContext* cx = xdr->cx();
    Rooted<Data*> data(cx);

    {
        auto freeOnLeave = MakeScopeExit([&data]() {
            if (mode == XDR_DECODE)
                js_free(data);
        });

        if (!XDRSizedBindingNames<EvalScope>(xdr, scope.as<EvalScope>(), &data))
            return false;

        if (mode == XDR_DECODE) {
            if (!data->length) {
                MOZ_ASSERT(!data->nextFrameSlot);
                js_free(data);
                data = nullptr;
            }

            scope.set(create(cx, kind, data, enclosing));
            if (!scope)
                return false;
        }
    }

    return true;
}

template
/* static */ bool
EvalScope::XDR(XDRState<XDR_ENCODE>* xdr, ScopeKind kind, HandleScope enclosing,
               MutableHandleScope scope);

template
/* static */ bool
EvalScope::XDR(XDRState<XDR_DECODE>* xdr, ScopeKind kind, HandleScope enclosing,
               MutableHandleScope scope);

static const uint32_t ModuleScopeEnvShapeFlags =
    BaseShape::NOT_EXTENSIBLE | BaseShape::QUALIFIED_VAROBJ | BaseShape::DELEGATE;

/* static */ UniquePtr<ModuleScope::Data>
ModuleScope::copyData(ExclusiveContext* cx, Handle<Data*> data, MutableHandleShape envShape)
{
    if (data) {
        BindingIter bi(*data);
        return CopyScopeData<ModuleScope>(cx, bi, data,
                                          &ModuleEnvironmentObject::class_,
                                          ModuleScopeEnvShapeFlags, envShape);
    }
    return NewEmptyScopeData<ModuleScope>(cx);
}

/* static */ ModuleScope*
ModuleScope::create(ExclusiveContext* cx, Handle<Data*> data,
                    HandleModuleObject module, HandleScope enclosing)
{
    MOZ_ASSERT(enclosing->is<GlobalScope>());

    // ModuleScope::Data has GCManagedDeletePolicy because it contains a
    // GCPtr. Destruction of |copy| below may trigger calls into the GC.
    Rooted<ModuleScope*> moduleScope(cx);

    {
        // The data that's passed in may be from the frontend and LifoAlloc'd.
        // Copy it now that we're creating a permanent VM scope.
        RootedShape envShape(cx);
        Rooted<UniquePtr<Data>> copy(cx, copyData(cx, data, &envShape));
        if (!copy)
            return nullptr;

        // Modules always need an environment object for now.
        if (!envShape) {
            envShape = getEmptyEnvironmentShape(cx);
            if (!envShape)
                return nullptr;
        }

        Scope* scope = Scope::create(cx, ScopeKind::Module, enclosing, envShape);
        if (!scope)
            return nullptr;

        copy->module.init(module);

        moduleScope = &scope->as<ModuleScope>();
        moduleScope->initData(Move(copy.get()));
    }

    return moduleScope;
}

/* static */ Shape*
ModuleScope::getEmptyEnvironmentShape(ExclusiveContext* cx)
{
    const Class* cls = &ModuleEnvironmentObject::class_;
    return EmptyEnvironmentShape(cx, cls, JSSLOT_FREE(cls), ModuleScopeEnvShapeFlags);
}

JSScript*
ModuleScope::script() const
{
    return module()->script();
}

ScopeIter::ScopeIter(JSScript* script)
  : scope_(script->bodyScope())
{ }

bool
ScopeIter::hasSyntacticEnvironment() const
{
    return scope()->hasEnvironment() && scope()->kind() != ScopeKind::NonSyntactic;
}

BindingIter::BindingIter(Scope* scope)
{
    switch (scope->kind()) {
      case ScopeKind::Lexical:
      case ScopeKind::SimpleCatch:
      case ScopeKind::Catch:
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
        if (scope->as<FunctionScope>().hasParameterExprs())
            flags |= HasFormalParameterExprs;
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
        init(scope->as<EvalScope>().data(), scope->kind() == ScopeKind::StrictEval);
        break;
      case ScopeKind::Global:
      case ScopeKind::NonSyntactic:
        init(scope->as<GlobalScope>().data());
        break;
      case ScopeKind::Module:
        init(scope->as<ModuleScope>().data());
        break;
    }
}

BindingIter::BindingIter(JSScript* script)
  : BindingIter(script->bodyScope())
{ }

void
BindingIter::init(LexicalScope::Data& data, uint32_t firstFrameSlot, uint8_t flags)
{
    // Named lambda scopes can only have environment slots. If the callee
    // isn't closed over, it is accessed via JSOP_CALLEE.
    if (flags & IsNamedLambda) {
        // Named lambda binding is weird. Normal BindingKind ordering rules
        // don't apply.
        init(0, 0, 0, 0, 0,
             CanHaveEnvironmentSlots | flags,
             firstFrameSlot, JSSLOT_FREE(&LexicalEnvironmentObject::class_),
             data.names, data.length);
    } else {
        //            imports - [0, 0)
        // positional formals - [0, 0)
        //      other formals - [0, 0)
        //               vars - [0, 0)
        //               lets - [0, data.constStart)
        //             consts - [data.constStart, data.length)
        init(0, 0, 0, 0, data.constStart,
             CanHaveFrameSlots | CanHaveEnvironmentSlots | flags,
             firstFrameSlot, JSSLOT_FREE(&LexicalEnvironmentObject::class_),
             data.names, data.length);
    }
}

void
BindingIter::init(FunctionScope::Data& data, uint8_t flags)
{
    flags = CanHaveFrameSlots | CanHaveEnvironmentSlots | flags;
    if (!(flags & HasFormalParameterExprs))
        flags |= CanHaveArgumentSlots;

    //            imports - [0, 0)
    // positional formals - [0, data.nonPositionalFormalStart)
    //      other formals - [data.nonPositionalParamStart, data.varStart)
    //               vars - [data.varStart, data.length)
    //               lets - [data.length, data.length)
    //             consts - [data.length, data.length)
    init(0, data.nonPositionalFormalStart, data.varStart, data.length, data.length,
         flags,
         0, JSSLOT_FREE(&CallObject::class_),
         data.names, data.length);
}

void
BindingIter::init(VarScope::Data& data, uint32_t firstFrameSlot)
{
    //            imports - [0, 0)
    // positional formals - [0, 0)
    //      other formals - [0, 0)
    //               vars - [0, data.length)
    //               lets - [data.length, data.length)
    //             consts - [data.length, data.length)
    init(0, 0, 0, data.length, data.length,
         CanHaveFrameSlots | CanHaveEnvironmentSlots,
         firstFrameSlot, JSSLOT_FREE(&VarEnvironmentObject::class_),
         data.names, data.length);
}

void
BindingIter::init(GlobalScope::Data& data)
{
    //            imports - [0, 0)
    // positional formals - [0, 0)
    //      other formals - [0, 0)
    //               vars - [0, data.letStart)
    //               lets - [data.letStart, data.constStart)
    //             consts - [data.constStart, data.length)
    init(0, 0, 0, data.letStart, data.constStart,
         CannotHaveSlots,
         UINT32_MAX, UINT32_MAX,
         data.names, data.length);
}

void
BindingIter::init(EvalScope::Data& data, bool strict)
{
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
    init(0, 0, 0, data.length, data.length,
         flags, firstFrameSlot, firstEnvironmentSlot,
         data.names, data.length);
}

void
BindingIter::init(ModuleScope::Data& data)
{
    //            imports - [0, data.varStart)
    // positional formals - [data.varStart, data.varStart)
    //      other formals - [data.varStart, data.varStart)
    //               vars - [data.varStart, data.letStart)
    //               lets - [data.letStart, data.constStart)
    //             consts - [data.constStart, data.length)
    init(data.varStart, data.varStart, data.varStart, data.letStart, data.constStart,
         CanHaveFrameSlots | CanHaveEnvironmentSlots,
         0, JSSLOT_FREE(&ModuleEnvironmentObject::class_),
         data.names, data.length);
}

PositionalFormalParameterIter::PositionalFormalParameterIter(JSScript* script)
  : BindingIter(script)
{
    // Reinit with flags = 0, i.e., iterate over all positional parameters.
    if (script->bodyScope()->is<FunctionScope>())
        init(script->bodyScope()->as<FunctionScope>().data(), /* flags = */ 0);
    settle();
}

void
js::DumpBindings(JSContext* cx, Scope* scopeArg)
{
    RootedScope scope(cx, scopeArg);
    for (Rooted<BindingIter> bi(cx, BindingIter(scope)); bi; bi++) {
        JSAutoByteString bytes;
        if (!AtomToPrintableString(cx, bi.name(), &bytes))
            return;
        fprintf(stderr, "%s %s ", BindingKindString(bi.kind()), bytes.ptr());
        switch (bi.location().kind()) {
          case BindingLocation::Kind::Global:
            fprintf(stderr, "global\n");
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

static JSAtom*
GetFrameSlotNameInScope(Scope* scope, uint32_t slot)
{
    for (BindingIter bi(scope); bi; bi++) {
        BindingLocation loc = bi.location();
        if (loc.kind() == BindingLocation::Kind::Frame && loc.slot() == slot)
            return bi.name();
    }
    return nullptr;
}

JSAtom*
js::FrameSlotName(JSScript* script, jsbytecode* pc)
{
    MOZ_ASSERT(IsLocalOp(JSOp(*pc)));
    uint32_t slot = GET_LOCALNO(pc);
    MOZ_ASSERT(slot < script->nfixed());

    // Look for it in the body scope first.
    if (JSAtom* name = GetFrameSlotNameInScope(script->bodyScope(), slot))
        return name;

    // If this is a function script and there is an extra var scope, look for
    // it there.
    if (script->functionHasExtraBodyVarScope()) {
        if (JSAtom* name = GetFrameSlotNameInScope(script->functionExtraBodyVarScope(), slot))
            return name;
    }

    // If not found, look for it in a lexical scope.
    for (ScopeIter si(script->innermostScope(pc)); si; si++) {
        if (!si.scope()->is<LexicalScope>())
            continue;
        LexicalScope& lexicalScope = si.scope()->as<LexicalScope>();

        // Is the slot within bounds of the current lexical scope?
        if (slot < lexicalScope.firstFrameSlot())
            continue;
        if (slot >= lexicalScope.nextFrameSlot())
            break;

        // If so, get the name.
        if (JSAtom* name = GetFrameSlotNameInScope(&lexicalScope, slot))
            return name;
    }

    MOZ_CRASH("Frame slot not found");
}

JS::ubi::Node::Size
JS::ubi::Concrete<Scope>::size(mozilla::MallocSizeOf mallocSizeOf) const
{
    return js::gc::Arena::thingSize(get().asTenured().getAllocKind()) +
           get().sizeOfExcludingThis(mallocSizeOf);
}
