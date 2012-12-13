/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscompartment.h"
#include "jsiter.h"
#include "jsscope.h"

#include "GlobalObject.h"
#include "ScopeObject.h"
#include "Xdr.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "ScopeObject-inl.h"

using namespace js;
using namespace js::types;

/*****************************************************************************/

StaticScopeIter::StaticScopeIter(JSObject *obj)
  : obj(obj), onNamedLambda(false)
{
    JS_ASSERT_IF(obj, obj->isStaticBlock() || obj->isFunction());
}

bool
StaticScopeIter::done() const
{
    return obj == NULL;
}

void
StaticScopeIter::operator++(int)
{
    if (obj->isStaticBlock()) {
        obj = obj->asStaticBlock().enclosingStaticScope();
    } else if (onNamedLambda || !obj->toFunction()->isNamedLambda()) {
        onNamedLambda = false;
        obj = obj->toFunction()->nonLazyScript()->enclosingStaticScope();
    } else {
        onNamedLambda = true;
    }
    JS_ASSERT_IF(obj, obj->isStaticBlock() || obj->isFunction());
    JS_ASSERT_IF(onNamedLambda, obj->isFunction());
}

bool
StaticScopeIter::hasDynamicScopeObject() const
{
    return obj->isStaticBlock()
           ? obj->asStaticBlock().needsClone()
           : obj->toFunction()->isHeavyweight();
}

Shape *
StaticScopeIter::scopeShape() const
{
    JS_ASSERT(hasDynamicScopeObject());
    JS_ASSERT(type() != NAMED_LAMBDA);
    return type() == BLOCK ? block().lastProperty() : funScript()->bindings.callObjShape();
}

StaticScopeIter::Type
StaticScopeIter::type() const
{
    if (onNamedLambda)
        return NAMED_LAMBDA;
    return obj->isStaticBlock() ? BLOCK : FUNCTION;
}

StaticBlockObject &
StaticScopeIter::block() const
{
    JS_ASSERT(type() == BLOCK);
    return obj->asStaticBlock();
}

UnrootedScript
StaticScopeIter::funScript() const
{
    JS_ASSERT(type() == FUNCTION);
    return obj->toFunction()->nonLazyScript();
}

/*****************************************************************************/

StaticScopeIter
js::ScopeCoordinateToStaticScope(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(pc >= script->code && pc < script->code + script->length);
    JS_ASSERT(JOF_OPTYPE(*pc) == JOF_SCOPECOORD);

    uint32_t blockIndex = GET_UINT32_INDEX(pc + 2 * sizeof(uint16_t));
    JSObject *innermostStaticScope;
    if (blockIndex == UINT32_MAX)
        innermostStaticScope = script->function();
    else
        innermostStaticScope = &script->getObject(blockIndex)->asStaticBlock();

    StaticScopeIter ssi(innermostStaticScope);
    ScopeCoordinate sc(pc);
    while (true) {
        if (ssi.hasDynamicScopeObject()) {
            if (!sc.hops)
                break;
            sc.hops--;
        }
        ssi++;
    }
    return ssi;
}

PropertyName *
js::ScopeCoordinateName(JSRuntime *rt, JSScript *script, jsbytecode *pc)
{
    Shape::Range r = ScopeCoordinateToStaticScope(script, pc).scopeShape()->all();
    ScopeCoordinate sc(pc);
    while (r.front().slot() != sc.slot)
        r.popFront();
    jsid id = r.front().propid();

    /* Beware nameless destructuring formal. */
    if (!JSID_IS_ATOM(id))
        return rt->atomState.empty;
    return JSID_TO_ATOM(id)->asPropertyName();
}

/*****************************************************************************/

/*
 * Construct a bare-bones call object given a shape, type, and slots pointer.
 * The call object must be further initialized to be usable.
 */
CallObject *
CallObject::create(JSContext *cx, HandleShape shape, HandleTypeObject type, HeapSlot *slots)
{
    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
    JS_ASSERT(CanBeFinalizedInBackground(kind, &CallClass));
    kind = gc::GetBackgroundAllocKind(kind);

    JSObject *obj = JSObject::create(cx, kind, shape, type, slots);
    if (!obj)
        return NULL;
    return &obj->asCall();
}

/*
 * Create a CallObject for a JSScript that is not initialized to any particular
 * callsite. This object can either be initialized (with an enclosing scope and
 * callee) or used as a template for jit compilation.
 */
CallObject *
CallObject::createTemplateObject(JSContext *cx, JSScript *script)
{
    RootedShape shape(cx, script->bindings.callObjShape());

    RootedTypeObject type(cx, cx->compartment->getNewType(cx, NULL));
    if (!type)
        return NULL;

    HeapSlot *slots;
    if (!PreallocateObjectDynamicSlots(cx, shape, &slots))
        return NULL;

    CallObject *callobj = CallObject::create(cx, shape, type, slots);
    if (!callobj) {
        js_free(slots);
        return NULL;
    }

    return callobj;
}

/*
 * Construct a call object for the given bindings.  If this is a call object
 * for a function invocation, callee should be the function being called.
 * Otherwise it must be a call object for eval of strict mode code, and callee
 * must be null.
 */
CallObject *
CallObject::create(JSContext *cx, HandleScript script, HandleObject enclosing, HandleFunction callee)
{
    CallObject *callobj = CallObject::createTemplateObject(cx, script);
    if (!callobj)
        return NULL;

    callobj->asScope().setEnclosingScope(enclosing);
    callobj->initFixedSlot(CALLEE_SLOT, ObjectOrNullValue(callee));
    return callobj;
}

CallObject *
CallObject::createForFunction(JSContext *cx, HandleObject enclosing, HandleFunction callee)
{
    AssertCanGC();

    RootedObject scopeChain(cx, enclosing);
    JS_ASSERT(scopeChain);

    /*
     * For a named function expression Call's parent points to an environment
     * object holding function's name.
     */
    if (callee->isNamedLambda()) {
        scopeChain = DeclEnvObject::create(cx, scopeChain, callee);
        if (!scopeChain)
            return NULL;
    }

    RootedScript script(cx, callee->nonLazyScript());
    return create(cx, script, scopeChain, callee);
}

CallObject *
CallObject::createForFunction(JSContext *cx, StackFrame *fp)
{
    AssertCanGC();
    JS_ASSERT(fp->isNonEvalFunctionFrame());
    assertSameCompartment(cx, fp);

    RootedObject scopeChain(cx, fp->scopeChain());
    RootedFunction callee(cx, &fp->callee());

    CallObject *callobj = createForFunction(cx, scopeChain, callee);
    if (!callobj)
        return NULL;

    /* Copy in the closed-over formal arguments. */
    for (AliasedFormalIter i(fp->script()); i; i++)
        callobj->setAliasedVar(i, fp->unaliasedFormal(i.frameIndex(), DONT_CHECK_ALIASING));

    return callobj;
}

CallObject *
CallObject::createForStrictEval(JSContext *cx, StackFrame *fp)
{
    AssertCanGC();
    JS_ASSERT(fp->isStrictEvalFrame());
    JS_ASSERT(cx->fp() == fp);
    JS_ASSERT(cx->regs().pc == fp->script()->code);

    RootedFunction callee(cx);
    RootedScript script(cx, fp->script());
    return create(cx, script, fp->scopeChain(), callee);
}

JS_PUBLIC_DATA(Class) js::CallClass = {
    "Call",
    JSCLASS_IS_ANONYMOUS | JSCLASS_HAS_RESERVED_SLOTS(CallObject::RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    NULL                     /* convert: Leave it NULL so we notice if calls ever escape */
};

Class js::DeclEnvClass = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(DeclEnvObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Object),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

/*
 * Create a DeclEnvObject for a JSScript that is not initialized to any
 * particular callsite. This object can either be initialized (with an enclosing
 * scope and callee) or used as a template for jit compilation.
 */
DeclEnvObject *
DeclEnvObject::createTemplateObject(JSContext *cx, HandleFunction fun)
{
    RootedTypeObject type(cx, cx->compartment->getNewType(cx, NULL));
    if (!type)
        return NULL;

    RootedShape emptyDeclEnvShape(cx);
    emptyDeclEnvShape = EmptyShape::getInitialShape(cx, &DeclEnvClass, NULL,
                                                    cx->global(), FINALIZE_KIND,
                                                    BaseShape::DELEGATE);
    if (!emptyDeclEnvShape)
        return NULL;

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, emptyDeclEnvShape, type, NULL));
    if (!obj)
        return NULL;

    // Assign a fixed slot to a property with the same name as the lambda.
    Rooted<jsid> id(cx, AtomToId(fun->atom()));
    Class *clasp = obj->getClass();
    unsigned attrs = JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY;
    if (!obj->putProperty(cx, id, clasp->getProperty, clasp->setProperty,
                          lambdaSlot(), attrs, 0, 0))
    {
        return NULL;
    }

    JS_ASSERT(!obj->hasDynamicSlots());
    return &obj->asDeclEnv();
}

DeclEnvObject *
DeclEnvObject::create(JSContext *cx, HandleObject enclosing, HandleFunction callee)
{
    RootedObject obj(cx, createTemplateObject(cx, callee));
    if (!obj)
        return NULL;

    obj->asScope().setEnclosingScope(enclosing);
    obj->setFixedSlot(lambdaSlot(), ObjectValue(*callee));
    return &obj->asDeclEnv();
}

WithObject *
WithObject::create(JSContext *cx, HandleObject proto, HandleObject enclosing, uint32_t depth)
{
    RootedTypeObject type(cx, proto->getNewType(cx));
    if (!type)
        return NULL;

    RootedShape shape(cx, EmptyShape::getInitialShape(cx, &WithClass, TaggedProto(proto),
                                                      &enclosing->global(), FINALIZE_KIND));
    if (!shape)
        return NULL;

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, shape, type, NULL));
    if (!obj)
        return NULL;

    obj->asScope().setEnclosingScope(enclosing);
    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(depth));

    RawObject thisp = JSObject::thisObject(cx, proto);
    if (!thisp)
        return NULL;

    obj->setFixedSlot(THIS_SLOT, ObjectValue(*thisp));

    return &obj->asWith();
}

static JSBool
with_LookupGeneric(JSContext *cx, HandleObject obj, HandleId id,
                   MutableHandleObject objp, MutableHandleShape propp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::lookupGeneric(cx, actual, id, objp, propp);
}

static JSBool
with_LookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                    MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
with_LookupElement(JSContext *cx, HandleObject obj, uint32_t index,
                   MutableHandleObject objp, MutableHandleShape propp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
with_LookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                   MutableHandleObject objp, MutableHandleShape propp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
with_GetGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id,
                MutableHandleValue vp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::getGeneric(cx, actual, actual, id, vp);
}

static JSBool
with_GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name,
                 MutableHandleValue vp)
{
    Rooted<jsid> id(cx, NameToId(name));
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
with_GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index,
                MutableHandleValue vp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
with_GetSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid,
                MutableHandleValue vp)
{
    Rooted<jsid> id(cx, SPECIALID_TO_JSID(sid));
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
with_SetGeneric(JSContext *cx, HandleObject obj, HandleId id,
                MutableHandleValue vp, JSBool strict)
{
    Rooted<JSObject*> actual(cx, &obj->asWith().object());
    return JSObject::setGeneric(cx, actual, actual, id, vp, strict);
}

static JSBool
with_SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                 MutableHandleValue vp, JSBool strict)
{
    Rooted<JSObject*> actual(cx, &obj->asWith().object());
    return JSObject::setProperty(cx, actual, actual, name, vp, strict);
}

static JSBool
with_SetElement(JSContext *cx, HandleObject obj, uint32_t index,
                MutableHandleValue vp, JSBool strict)
{
    Rooted<JSObject*> actual(cx, &obj->asWith().object());
    return JSObject::setElement(cx, actual, actual, index, vp, strict);
}

static JSBool
with_SetSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                MutableHandleValue vp, JSBool strict)
{
    Rooted<JSObject*> actual(cx, &obj->asWith().object());
    return JSObject::setSpecial(cx, actual, actual, sid, vp, strict);
}

static JSBool
with_GetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::getGenericAttributes(cx, actual, id, attrsp);
}

static JSBool
with_GetPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::getPropertyAttributes(cx, actual, name, attrsp);
}

static JSBool
with_GetElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::getElementAttributes(cx, actual, index, attrsp);
}

static JSBool
with_GetSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::getSpecialAttributes(cx, actual, sid, attrsp);
}

static JSBool
with_SetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::setGenericAttributes(cx, actual, id, attrsp);
}

static JSBool
with_SetPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::setPropertyAttributes(cx, actual, name, attrsp);
}

static JSBool
with_SetElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::setElementAttributes(cx, actual, index, attrsp);
}

static JSBool
with_SetSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::setSpecialAttributes(cx, actual, sid, attrsp);
}

static JSBool
with_DeleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name,
                    MutableHandleValue rval, JSBool strict)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::deleteProperty(cx, actual, name, rval, strict);
}

static JSBool
with_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index,
                   MutableHandleValue rval, JSBool strict)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::deleteElement(cx, actual, index, rval, strict);
}

static JSBool
with_DeleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid,
                   MutableHandleValue rval, JSBool strict)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::deleteSpecial(cx, actual, sid, rval, strict);
}

static JSBool
with_Enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
               MutableHandleValue statep, MutableHandleId idp)
{
    RootedObject actual(cx, &obj->asWith().object());
    return JSObject::enumerate(cx, actual, enum_op, statep, idp);
}

static JSType
with_TypeOf(JSContext *cx, HandleObject obj)
{
    return JSTYPE_OBJECT;
}

static JSObject *
with_ThisObject(JSContext *cx, HandleObject obj)
{
    return &obj->asWith().withThis();
}

Class js::WithClass = {
    "With",
    JSCLASS_HAS_RESERVED_SLOTS(WithObject::RESERVED_SLOTS) |
    JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    NULL,                    /* finalize */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    NULL,                    /* trace       */
    JS_NULL_CLASS_EXT,
    {
        with_LookupGeneric,
        with_LookupProperty,
        with_LookupElement,
        with_LookupSpecial,
        NULL,             /* defineGeneric */
        NULL,             /* defineProperty */
        NULL,             /* defineElement */
        NULL,             /* defineSpecial */
        with_GetGeneric,
        with_GetProperty,
        with_GetElement,
        NULL,             /* getElementIfPresent */
        with_GetSpecial,
        with_SetGeneric,
        with_SetProperty,
        with_SetElement,
        with_SetSpecial,
        with_GetGenericAttributes,
        with_GetPropertyAttributes,
        with_GetElementAttributes,
        with_GetSpecialAttributes,
        with_SetGenericAttributes,
        with_SetPropertyAttributes,
        with_SetElementAttributes,
        with_SetSpecialAttributes,
        with_DeleteProperty,
        with_DeleteElement,
        with_DeleteSpecial,
        with_Enumerate,
        with_TypeOf,
        with_ThisObject,
    }
};

/*****************************************************************************/

ClonedBlockObject *
ClonedBlockObject::create(JSContext *cx, Handle<StaticBlockObject *> block, StackFrame *fp)
{
    assertSameCompartment(cx, fp);

    RootedTypeObject type(cx, block->getNewType(cx));
    if (!type)
        return NULL;

    HeapSlot *slots;
    if (!PreallocateObjectDynamicSlots(cx, block->lastProperty(), &slots))
        return NULL;

    RootedShape shape(cx, block->lastProperty());

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, shape, type, slots));
    if (!obj)
        return NULL;

    /* Set the parent if necessary, as for call objects. */
    if (&fp->global() != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        Rooted<GlobalObject*> global(cx, &fp->global());
        if (!JSObject::setParent(cx, obj, global))
            return NULL;
    }

    JS_ASSERT(!obj->inDictionaryMode());
    JS_ASSERT(obj->slotSpan() >= block->slotCount() + RESERVED_SLOTS);

    obj->setReservedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*fp->scopeChain()));
    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(block->stackDepth()));

    /*
     * Copy in the closed-over locals. Closed-over locals don't need
     * any fixup since the initial value is 'undefined'.
     */
    Value *src = fp->base() + block->stackDepth();
    unsigned nslots = block->slotCount();
    for (unsigned i = 0; i < nslots; ++i, ++src) {
        if (block->isAliased(i))
            obj->asClonedBlock().setVar(i, *src);
    }

    JS_ASSERT(obj->isDelegate());

    return &obj->asClonedBlock();
}

void
ClonedBlockObject::copyUnaliasedValues(StackFrame *fp)
{
    AutoAssertNoGC nogc;
    StaticBlockObject &block = staticBlock();
    unsigned base = fp->script()->nfixed + block.stackDepth();
    for (unsigned i = 0; i < slotCount(); ++i) {
        if (!block.isAliased(i))
            setVar(i, fp->unaliasedLocal(base + i), DONT_CHECK_ALIASING);
    }
}

StaticBlockObject *
StaticBlockObject::create(JSContext *cx)
{
    RootedTypeObject type(cx, cx->compartment->getNewType(cx, NULL));
    if (!type)
        return NULL;

    RootedShape emptyBlockShape(cx);
    emptyBlockShape = EmptyShape::getInitialShape(cx, &BlockClass, NULL, NULL, FINALIZE_KIND,
                                                  BaseShape::DELEGATE);
    if (!emptyBlockShape)
        return NULL;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyBlockShape, type, NULL);
    if (!obj)
        return NULL;

    return &obj->asStaticBlock();
}

/* static */ Shape *
StaticBlockObject::addVar(JSContext *cx, Handle<StaticBlockObject*> block, HandleId id,
                          int index, bool *redeclared)
{
    JS_ASSERT(JSID_IS_ATOM(id) || (JSID_IS_INT(id) && JSID_TO_INT(id) == index));

    *redeclared = false;

    /* Inline JSObject::addProperty in order to trap the redefinition case. */
    Shape **spp;
    if (Shape::search(cx, block->lastProperty(), id, &spp, true)) {
        *redeclared = true;
        return NULL;
    }

    /*
     * Don't convert this object to dictionary mode so that we can clone the
     * block's shape later.
     */
    uint32_t slot = JSSLOT_FREE(&BlockClass) + index;
    return block->addPropertyInternal(cx, id, /* getter = */ NULL, /* setter = */ NULL,
                                      slot, JSPROP_ENUMERATE | JSPROP_PERMANENT,
                                      Shape::HAS_SHORTID, index, spp,
                                      /* allowDictionary = */ false);
}

Class js::BlockClass = {
    "Block",
    JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_RESERVED_SLOTS(BlockObject::RESERVED_SLOTS) |
    JSCLASS_IS_ANONYMOUS,
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub
};

template<XDRMode mode>
bool
js::XDRStaticBlockObject(XDRState<mode> *xdr, HandleObject enclosingScope, HandleScript script,
                         StaticBlockObject **objp)
{
    /* NB: Keep this in sync with CloneStaticBlockObject. */

    JSContext *cx = xdr->cx();

    Rooted<StaticBlockObject*> obj(cx);
    uint32_t count = 0;
    uint32_t depthAndCount = 0;

    if (mode == XDR_ENCODE) {
        obj = *objp;
        uint32_t depth = obj->stackDepth();
        JS_ASSERT(depth <= UINT16_MAX);
        count = obj->slotCount();
        JS_ASSERT(count <= UINT16_MAX);
        depthAndCount = (depth << 16) | uint16_t(count);
    }

    if (mode == XDR_DECODE) {
        obj = StaticBlockObject::create(cx);
        if (!obj)
            return false;
        obj->initEnclosingStaticScope(enclosingScope);
        *objp = obj;
    }

    if (!xdr->codeUint32(&depthAndCount))
        return false;

    if (mode == XDR_DECODE) {
        uint32_t depth = uint16_t(depthAndCount >> 16);
        count = uint16_t(depthAndCount);
        obj->setStackDepth(depth);

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (unsigned i = 0; i < count; i++) {
            RootedAtom atom(cx);
            if (!XDRAtom(xdr, &atom))
                return false;

            /* The empty string indicates an int id. */
            RootedId id(cx, atom != cx->runtime->emptyString
                            ? AtomToId(atom)
                            : INT_TO_JSID(i));

            bool redeclared;
            if (!StaticBlockObject::addVar(cx, obj, id, i, &redeclared)) {
                JS_ASSERT(!redeclared);
                return false;
            }

            uint32_t aliased;
            if (!xdr->codeUint32(&aliased))
                return false;

            JS_ASSERT(aliased == 0 || aliased == 1);
            obj->setAliased(i, !!aliased);
        }
    } else {
        AutoShapeVector shapes(cx);
        if (!shapes.growBy(count))
            return false;

        for (Shape::Range r(obj->lastProperty()); !r.empty(); r.popFront()) {
            Shape *shape = &r.front();
            shapes[shape->shortid()] = shape;
        }

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (unsigned i = 0; i < count; i++) {
            Shape *shape = shapes[i];
            JS_ASSERT(shape->hasDefaultGetter());
            JS_ASSERT(unsigned(shape->shortid()) == i);

            jsid propid = shape->propid();
            JS_ASSERT(JSID_IS_ATOM(propid) || JSID_IS_INT(propid));

            /* The empty string indicates an int id. */
            RootedAtom atom(cx, JSID_IS_ATOM(propid)
                                ? JSID_TO_ATOM(propid)
                                : cx->runtime->emptyString);
            if (!XDRAtom(xdr, &atom))
                return false;

            uint32_t aliased = obj->isAliased(i);
            if (!xdr->codeUint32(&aliased))
                return false;
        }
    }
    return true;
}

template bool
js::XDRStaticBlockObject(XDRState<XDR_ENCODE> *, HandleObject, HandleScript, StaticBlockObject **);

template bool
js::XDRStaticBlockObject(XDRState<XDR_DECODE> *, HandleObject, HandleScript, StaticBlockObject **);

JSObject *
js::CloneStaticBlockObject(JSContext *cx, HandleObject enclosingScope, Handle<StaticBlockObject*> srcBlock)
{
    /* NB: Keep this in sync with XDRStaticBlockObject. */

    Rooted<StaticBlockObject*> clone(cx, StaticBlockObject::create(cx));
    if (!clone)
        return NULL;

    clone->initEnclosingStaticScope(enclosingScope);
    clone->setStackDepth(srcBlock->stackDepth());

    /* Shape::Range is reverse order, so build a list in forward order. */
    AutoShapeVector shapes(cx);
    if (!shapes.growBy(srcBlock->slotCount()))
        return NULL;
    for (Shape::Range r = srcBlock->lastProperty()->all(); !r.empty(); r.popFront())
        shapes[r.front().shortid()] = &r.front();

    for (Shape **p = shapes.begin(); p != shapes.end(); ++p) {
        RootedId id(cx, (*p)->propid());
        unsigned i = (*p)->shortid();

        bool redeclared;
        if (!StaticBlockObject::addVar(cx, clone, id, i, &redeclared)) {
            JS_ASSERT(!redeclared);
            return NULL;
        }

        clone->setAliased(i, srcBlock->isAliased(i));
    }

    return clone;
}

/*****************************************************************************/

ScopeIter::ScopeIter(JSContext *cx
                     JS_GUARD_OBJECT_NOTIFIER_PARAM_NO_INIT)
  : fp_(NULL),
    cur_(cx, reinterpret_cast<JSObject *>(-1)),
    block_(cx, reinterpret_cast<StaticBlockObject *>(-1)),
    type_(Type(-1))
{
    JS_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeIter::ScopeIter(const ScopeIter &si, JSContext *cx
                     JS_GUARD_OBJECT_NOTIFIER_PARAM_NO_INIT)
  : fp_(si.fp_),
    cur_(cx, si.cur_),
    block_(cx, si.block_),
    type_(si.type_),
    hasScopeObject_(si.hasScopeObject_)
{
    JS_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeIter::ScopeIter(JSObject &enclosingScope, JSContext *cx
                     JS_GUARD_OBJECT_NOTIFIER_PARAM_NO_INIT)
  : fp_(NULL),
    cur_(cx, &enclosingScope),
    block_(cx, reinterpret_cast<StaticBlockObject *>(-1)),
    type_(Type(-1))
{
    JS_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeIter::ScopeIter(StackFrame *fp, JSContext *cx
                     JS_GUARD_OBJECT_NOTIFIER_PARAM_NO_INIT)
  : fp_(fp),
    cur_(cx, fp->scopeChain()),
    block_(cx, fp->maybeBlockChain())
{
    assertSameCompartment(cx, fp);
    settle();
    JS_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeIter::ScopeIter(const ScopeIter &si, StackFrame *fp, JSContext *cx
                     JS_GUARD_OBJECT_NOTIFIER_PARAM_NO_INIT)
  : fp_(fp),
    cur_(cx, si.cur_),
    block_(cx, si.block_),
    type_(si.type_),
    hasScopeObject_(si.hasScopeObject_)
{
    JS_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeIter::ScopeIter(StackFrame *fp, ScopeObject &scope, JSContext *cx
                     JS_GUARD_OBJECT_NOTIFIER_PARAM_NO_INIT)
  : fp_(fp),
    cur_(cx, &scope),
    block_(cx)
{
    /*
     * Find the appropriate static block for this iterator, given 'scope'. We
     * know that 'scope' is a (non-optimized) scope on fp's scope chain. We do
     * not, however, know whether fp->maybeScopeChain() encloses 'scope'. E.g.:
     *
     *   let (x = 1) {
     *     g = function() { eval('debugger') };
     *     let (y = 1) g();
     *   }
     *
     * g will have x's block in its enclosing scope but not y's. However, at
     * the debugger statement, both the x's and y's blocks will be on
     * fp->blockChain. Fortunately, we can compare scope object stack depths to
     * determine the block (if any) that encloses 'scope'.
     */
    if (cur_->isNestedScope()) {
        block_ = fp->maybeBlockChain();
        while (block_) {
            if (block_->stackDepth() <= cur_->asNestedScope().stackDepth())
                break;
            block_ = block_->enclosingBlock();
        }
        JS_ASSERT_IF(cur_->isClonedBlock(), cur_->asClonedBlock().staticBlock() == *block_);
    } else {
        block_ = NULL;
    }
    settle();
    JS_GUARD_OBJECT_NOTIFIER_INIT;
}

ScopeObject &
ScopeIter::scope() const
{
    JS_ASSERT(hasScopeObject());
    return cur_->asScope();
}

ScopeIter &
ScopeIter::operator++()
{
    JS_ASSERT(!done());
    switch (type_) {
      case Call:
        if (hasScopeObject_) {
            cur_ = &cur_->asCall().enclosingScope();
            if (CallObjectLambdaName(*fp_->fun()))
                cur_ = &cur_->asDeclEnv().enclosingScope();
        }
        fp_ = NULL;
        break;
      case Block:
        block_ = block_->enclosingBlock();
        if (hasScopeObject_)
            cur_ = &cur_->asClonedBlock().enclosingScope();
        settle();
        break;
      case With:
        JS_ASSERT(hasScopeObject_);
        cur_ = &cur_->asWith().enclosingScope();
        settle();
        break;
      case StrictEvalScope:
        if (hasScopeObject_)
            cur_ = &cur_->asCall().enclosingScope();
        fp_ = NULL;
        break;
    }
    return *this;
}

void
ScopeIter::settle()
{
    AutoAssertNoGC nogc;
    /*
     * Given an iterator state (cur_, block_), figure out which (potentially
     * optimized) scope the iterator should report. Thus, the result is a pair
     * (type_, hasScopeObject_) where hasScopeObject_ indicates whether the
     * scope object has been optimized away and does not exist on the scope
     * chain. Beware: while ScopeIter iterates over the scopes of a single
     * frame, the scope chain (pointed to by cur_) continues into the scopes of
     * enclosing frames. Thus, it is important not to look at cur_ until it is
     * certain that cur_ points to a scope object in the current frame. In
     * particular, there are three tricky corner cases:
     *  - non-heavyweight functions;
     *  - non-strict direct eval.
     *  - heavyweight functions observed before the prologue has finished;
     * In all cases, cur_ can already be pointing into an enclosing frame's
     * scope chain. Furthermore, in the first two cases: even if cur_ points
     * into an enclosing frame's scope chain, the current frame may still have
     * uncloned blocks. In the last case, since we haven't entered the
     * function, we simply return a ScopeIter where done() == true.
     *
     * Note: DebugScopeObject falls nicely into this plan: since they are only
     * ever introduced as the *enclosing* scope of a frame, they should never
     * show up in scope iteration and fall into the final non-scope case.
     */
    if (fp_->isNonEvalFunctionFrame() && !fp_->fun()->isHeavyweight()) {
        if (block_) {
            type_ = Block;
            hasScopeObject_ = block_->needsClone();
        } else {
            type_ = Call;
            hasScopeObject_ = false;
        }
    } else if (fp_->isNonStrictDirectEvalFrame() && cur_ == fp_->prev()->scopeChain()) {
        if (block_) {
            JS_ASSERT(!block_->needsClone());
            type_ = Block;
            hasScopeObject_ = false;
        } else {
            fp_ = NULL;
        }
    } else if (fp_->isNonEvalFunctionFrame() && !fp_->hasCallObj()) {
        JS_ASSERT(cur_ == fp_->fun()->environment());
        fp_ = NULL;
    } else if (fp_->isStrictEvalFrame() && !fp_->hasCallObj()) {
        JS_ASSERT(cur_ == fp_->prev()->scopeChain());
        fp_ = NULL;
    } else if (cur_->isWith()) {
        JS_ASSERT_IF(fp_->isFunctionFrame(), fp_->fun()->isHeavyweight());
        JS_ASSERT_IF(block_, block_->needsClone());
        JS_ASSERT_IF(block_, block_->stackDepth() < cur_->asWith().stackDepth());
        type_ = With;
        hasScopeObject_ = true;
    } else if (block_) {
        type_ = Block;
        hasScopeObject_ = block_->needsClone();
        JS_ASSERT_IF(hasScopeObject_, cur_->asClonedBlock().staticBlock() == *block_);
    } else if (cur_->isCall()) {
        CallObject &callobj = cur_->asCall();
        type_ = callobj.isForEval() ? StrictEvalScope : Call;
        hasScopeObject_ = true;
        JS_ASSERT_IF(type_ == Call, callobj.callee().nonLazyScript() == fp_->script());
    } else {
        JS_ASSERT(!cur_->isScope());
        JS_ASSERT(fp_->isGlobalFrame() || fp_->isDebuggerFrame());
        fp_ = NULL;
    }
}

/* static */ HashNumber
ScopeIterKey::hash(ScopeIterKey si)
{
    /* hasScopeObject_ is determined by the other fields. */
    return size_t(si.fp_) ^ size_t(si.cur_) ^ size_t(si.block_) ^ si.type_;
}

/* static */ bool
ScopeIterKey::match(ScopeIterKey si1, ScopeIterKey si2)
{
    /* hasScopeObject_ is determined by the other fields. */
    return si1.fp_ == si2.fp_ &&
           (!si1.fp_ ||
            (si1.cur_   == si2.cur_   &&
             si1.block_ == si2.block_ &&
             si1.type_  == si2.type_));
}

/*****************************************************************************/

/*
 * DebugScopeProxy is the handler for DebugScopeObject proxy objects. Having a
 * custom handler (rather than trying to reuse js::Wrapper) gives us several
 * important abilities:
 *  - We want to pass the ScopeObject as the receiver to forwarded scope
 *    property ops on aliased variables so that Call/Block/With ops do not all
 *    require a 'normalization' step.
 *  - The debug scope proxy can directly manipulate the stack frame to allow
 *    the debugger to read/write args/locals that were otherwise unaliased.
 *  - The debug scope proxy can store unaliased variables after the stack frame
 *    is popped so that they may still be read/written by the debugger.
 *  - The engine has made certain assumptions about the possible reads/writes
 *    in a scope. DebugScopeProxy allows us to prevent the debugger from
 *    breaking those assumptions.
 *  - The engine makes optimizations that are observable to the debugger. The
 *    proxy can either hide these optimizations or make the situation more
 *    clear to the debugger. An example is 'arguments'.
 */
class DebugScopeProxy : public BaseProxyHandler
{
    enum Action { SET, GET };

    /*
     * This function handles access to unaliased locals/formals. Since they are
     * unaliased, the values of these variables are not stored in the slots of
     * the normal Call/BlockObject scope objects and thus must be recovered
     * from somewhere else:
     *  + if the invocation for which the scope was created is still executing,
     *    there is a StackFrame (either live on the stack or floating in a
     *    generator object) holding the values;
     *  + if the invocation for which the scope was created finished executing:
     *     - and there was a DebugScopeObject associated with scope, then the
     *       DebugScopes::onPop(Call|Block) handler copied out the unaliased
     *       variables:
     *        . for block scopes, the unaliased values were copied directly
     *          into the block object, since there is a slot allocated for every
     *          block binding, regardless of whether it is aliased;
     *        . for function scopes, a dense array is created in onPopCall to hold
     *          the unaliased values and attached to the DebugScopeObject;
     *     - and there was not a DebugScopeObject yet associated with the
     *       scope, then the unaliased values are lost and not recoverable.
     *
     * handleUnaliasedAccess returns 'true' if the access was unaliased and
     * completed by handleUnaliasedAccess.
     */
    bool handleUnaliasedAccess(JSContext *cx, Handle<DebugScopeObject*> debugScope, Handle<ScopeObject*> scope,
                               jsid id, Action action, Value *vp)
    {
        JS_ASSERT(&debugScope->scope() == scope);
        StackFrame *maybefp = DebugScopes::hasLiveFrame(*scope);

        /* Handle unaliased formals, vars, and consts at function scope. */
        if (scope->isCall() && !scope->asCall().isForEval()) {
            CallObject &callobj = scope->asCall();
            RootedScript script(cx, callobj.callee().nonLazyScript());
            if (!script->ensureHasTypes(cx))
                return false;

            Bindings &bindings = script->bindings;
            BindingIter bi(script);
            while (bi && NameToId(bi->name()) != id)
                bi++;
            if (!bi)
                return false;

            if (bi->kind() == VARIABLE || bi->kind() == CONSTANT) {
                unsigned i = bi.frameIndex();
                if (script->varIsAliased(i))
                    return false;

                if (maybefp) {
                    if (action == GET)
                        *vp = maybefp->unaliasedVar(i);
                    else
                        maybefp->unaliasedVar(i) = *vp;
                } else if (JSObject *snapshot = debugScope->maybeSnapshot()) {
                    if (action == GET)
                        *vp = snapshot->getDenseArrayElement(bindings.numArgs() + i);
                    else
                        snapshot->setDenseArrayElement(bindings.numArgs() + i, *vp);
                } else {
                    /* The unaliased value has been lost to the debugger. */
                    if (action == GET)
                        *vp = UndefinedValue();
                }

                if (action == SET)
                    TypeScript::SetLocal(cx, script, i, *vp);

            } else {
                JS_ASSERT(bi->kind() == ARGUMENT);
                unsigned i = bi.frameIndex();
                if (script->formalIsAliased(i))
                    return false;

                if (maybefp) {
                    if (script->argsObjAliasesFormals() && maybefp->hasArgsObj()) {
                        if (action == GET)
                            *vp = maybefp->argsObj().arg(i);
                        else
                            maybefp->argsObj().setArg(i, *vp);
                    } else {
                        if (action == GET)
                            *vp = maybefp->unaliasedFormal(i, DONT_CHECK_ALIASING);
                        else
                            maybefp->unaliasedFormal(i, DONT_CHECK_ALIASING) = *vp;
                    }
                } else if (JSObject *snapshot = debugScope->maybeSnapshot()) {
                    if (action == GET)
                        *vp = snapshot->getDenseArrayElement(i);
                    else
                        snapshot->setDenseArrayElement(i, *vp);
                } else {
                    /* The unaliased value has been lost to the debugger. */
                    if (action == GET)
                        *vp = UndefinedValue();
                }

                if (action == SET)
                    TypeScript::SetArgument(cx, script, i, *vp);
            }

            return true;
        }

        /* Handle unaliased let and catch bindings at block scope. */
        if (scope->isClonedBlock()) {
            ClonedBlockObject &block = scope->asClonedBlock();
            Shape *shape = block.lastProperty()->search(cx, id);
            if (!shape)
                return false;

            AutoAssertNoGC nogc;
            unsigned i = shape->shortid();
            if (block.staticBlock().isAliased(i))
                return false;

            if (maybefp) {
                UnrootedScript script = maybefp->script();
                unsigned local = block.slotToLocalIndex(script->bindings, shape->slot());
                if (action == GET)
                    *vp = maybefp->unaliasedLocal(local);
                else
                    maybefp->unaliasedLocal(local) = *vp;
                JS_ASSERT(analyze::LocalSlot(script, local) >= analyze::TotalSlots(script));
            } else {
                if (action == GET)
                    *vp = block.var(i, DONT_CHECK_ALIASING);
                else
                    block.setVar(i, *vp, DONT_CHECK_ALIASING);
            }

            return true;
        }

        /* The rest of the internal scopes do not have unaliased vars. */
        JS_ASSERT(scope->isDeclEnv() || scope->isWith() || scope->asCall().isForEval());
        return false;
    }

    static bool isArguments(JSContext *cx, jsid id)
    {
        return id == NameToId(cx->names().arguments);
    }

    static bool isFunctionScope(ScopeObject &scope)
    {
        return scope.isCall() && !scope.asCall().isForEval();
    }

    /*
     * In theory, every function scope contains an 'arguments' bindings.
     * However, the engine only adds a binding if 'arguments' is used in the
     * function body. Thus, from the debugger's perspective, 'arguments' may be
     * missing from the list of bindings.
     */
    static bool isMissingArgumentsBinding(ScopeObject &scope)
    {
        return isFunctionScope(scope) &&
               !scope.asCall().callee().nonLazyScript()->argumentsHasVarBinding();
    }

    /*
     * This function creates an arguments object when the debugger requests
     * 'arguments' for a function scope where the arguments object has been
     * optimized away (either because the binding is missing altogether or
     * because !ScriptAnalysis::needsArgsObj).
     */
    static bool checkForMissingArguments(JSContext *cx, jsid id, ScopeObject &scope,
                                         ArgumentsObject **maybeArgsObj)
    {
        AssertCanGC();
        *maybeArgsObj = NULL;

        if (!isArguments(cx, id) || !isFunctionScope(scope))
            return true;

        if (scope.asCall().callee().nonLazyScript()->needsArgsObj())
            return true;

        StackFrame *maybefp = DebugScopes::hasLiveFrame(scope);
        if (!maybefp) {
            JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_DEBUG_NOT_LIVE,
                                 "Debugger scope");
            return false;
        }

        *maybeArgsObj = ArgumentsObject::createUnexpected(cx, maybefp);
        return true;
    }

  public:
    static int family;
    static DebugScopeProxy singleton;

    DebugScopeProxy() : BaseProxyHandler(&family) {}

    bool getPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                               PropertyDescriptor *desc) MOZ_OVERRIDE
    {
        return getOwnPropertyDescriptor(cx, proxy, id, set, desc);
    }

    bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid idArg, bool set,
                                  PropertyDescriptor *desc) MOZ_OVERRIDE
    {
        Rooted<DebugScopeObject*> debugScope(cx, &proxy->asDebugScope());
        Rooted<ScopeObject*> scope(cx, &debugScope->scope());
        RootedId id(cx, idArg);

        ArgumentsObject *maybeArgsObj;
        if (!checkForMissingArguments(cx, id, *scope, &maybeArgsObj))
            return false;

        if (maybeArgsObj) {
            PodZero(desc);
            desc->obj = debugScope;
            desc->attrs = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
            desc->value = ObjectValue(*maybeArgsObj);
            return true;
        }

        Value v;
        if (handleUnaliasedAccess(cx, debugScope, scope, id, GET, &v)) {
            PodZero(desc);
            desc->obj = debugScope;
            desc->attrs = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
            desc->value = v;
            return true;
        }

        return JS_GetPropertyDescriptorById(cx, scope, id, JSRESOLVE_QUALIFIED, desc);
    }

    bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid idArg, Value *vp) MOZ_OVERRIDE
    {
        Rooted<DebugScopeObject*> debugScope(cx, &proxy->asDebugScope());
        Rooted<ScopeObject*> scope(cx, &proxy->asDebugScope().scope());
        RootedId id(cx, idArg);

        ArgumentsObject *maybeArgsObj;
        if (!checkForMissingArguments(cx, id, *scope, &maybeArgsObj))
            return false;

        if (maybeArgsObj) {
            *vp = ObjectValue(*maybeArgsObj);
            return true;
        }

        if (handleUnaliasedAccess(cx, debugScope, scope, id, GET, vp))
            return true;

        RootedValue value(cx);
        if (!JSObject::getGeneric(cx, scope, scope, id, &value))
            return false;

        *vp = value;
        return true;
    }

    bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid idArg, bool strict,
                     Value *vp) MOZ_OVERRIDE
    {
        Rooted<DebugScopeObject*> debugScope(cx, &proxy->asDebugScope());
        Rooted<ScopeObject*> scope(cx, &proxy->asDebugScope().scope());
        RootedId id(cx, idArg);

        if (handleUnaliasedAccess(cx, debugScope, scope, id, SET, vp))
            return true;

        RootedValue value(cx, *vp);
        if (!JSObject::setGeneric(cx, scope, scope, id, &value, strict))
            return false;

        *vp = value;
        return true;
    }

    bool defineProperty(JSContext *cx, JSObject *proxy, jsid idArg, PropertyDescriptor *desc) MOZ_OVERRIDE
    {
        Rooted<ScopeObject*> scope(cx, &proxy->asDebugScope().scope());
        RootedId id(cx, idArg);

        bool found;
        if (!has(cx, proxy, id, &found))
            return false;
        if (found)
            return Throw(cx, id, JSMSG_CANT_REDEFINE_PROP);

        return JS_DefinePropertyById(cx, scope, id, desc->value, desc->getter, desc->setter,
                                     desc->attrs);
    }

    bool getScopePropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props, unsigned flags)
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        if (isMissingArgumentsBinding(scope)) {
            if (!props.append(NameToId(cx->names().arguments)))
                return false;
        }

        RootedObject rootedScope(cx, &scope);
        if (!GetPropertyNames(cx, rootedScope, flags, &props))
            return false;

        /*
         * Function scopes are optimized to not contain unaliased variables so
         * they must be manually appended here.
         */
        if (scope.isCall() && !scope.asCall().isForEval()) {
            RootedScript script(cx, scope.asCall().callee().nonLazyScript());
            for (BindingIter bi(script); bi; bi++) {
                if (!bi->aliased() && !props.append(NameToId(bi->name())))
                    return false;
            }
        }

        return true;
    }

    bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE
    {
        return getScopePropertyNames(cx, proxy, props, JSITER_OWNONLY);
    }

    bool enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE
    {
        return getScopePropertyNames(cx, proxy, props, 0);
    }

    bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE
    {
        ScopeObject &scopeObj = proxy->asDebugScope().scope();

        if (isArguments(cx, id) && isFunctionScope(scopeObj)) {
            *bp = true;
            return true;
        }

        JSBool found;
        RootedObject scope(cx, &scopeObj);
        if (!JS_HasPropertyById(cx, scope, id, &found))
            return false;

        /*
         * Function scopes are optimized to not contain unaliased variables so
         * a manual search is necessary.
         */
        if (!found && scope->isCall() && !scope->asCall().isForEval()) {
            RootedScript script(cx, scope->asCall().callee().nonLazyScript());
            for (BindingIter bi(script); bi; bi++) {
                if (!bi->aliased() && NameToId(bi->name()) == id) {
                    found = true;
                    break;
                }
            }
        }

        *bp = found;
        return true;
    }

    bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE
    {
        RootedValue idval(cx, IdToValue(id));
        return js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_CANT_DELETE,
                                        JSDVG_IGNORE_STACK, idval, NullPtr(), NULL, NULL);
    }
};

int DebugScopeProxy::family = 0;
DebugScopeProxy DebugScopeProxy::singleton;

/* static */ DebugScopeObject *
DebugScopeObject::create(JSContext *cx, ScopeObject &scope, HandleObject enclosing)
{
    JS_ASSERT(scope.compartment() == cx->compartment);
    JSObject *obj = NewProxyObject(cx, &DebugScopeProxy::singleton, ObjectValue(scope),
                                   NULL /* proto */, &scope.global(),
                                   NULL /* call */, NULL /* construct */);
    if (!obj)
        return NULL;

    JS_ASSERT(!enclosing->isScope());
    SetProxyExtra(obj, ENCLOSING_EXTRA, ObjectValue(*enclosing));
    SetProxyExtra(obj, SNAPSHOT_EXTRA, NullValue());

    return &obj->asDebugScope();
}

ScopeObject &
DebugScopeObject::scope() const
{
    return GetProxyTargetObject(const_cast<DebugScopeObject*>(this))->asScope();
}

JSObject &
DebugScopeObject::enclosingScope() const
{
    return GetProxyExtra(const_cast<DebugScopeObject*>(this), ENCLOSING_EXTRA).toObject();
}

JSObject *
DebugScopeObject::maybeSnapshot() const
{
    JS_ASSERT(!scope().asCall().isForEval());
    return GetProxyExtra(const_cast<DebugScopeObject*>(this), SNAPSHOT_EXTRA).toObjectOrNull();
}

void
DebugScopeObject::initSnapshot(JSObject &o)
{
    JS_ASSERT(maybeSnapshot() == NULL);
    SetProxyExtra(this, SNAPSHOT_EXTRA, ObjectValue(o));
}

bool
DebugScopeObject::isForDeclarative() const
{
    ScopeObject &s = scope();
    return s.isCall() || s.isBlock() || s.isDeclEnv();
}

bool
js_IsDebugScopeSlow(RawObject obj)
{
    return obj->getClass() == &ObjectProxyClass &&
           GetProxyHandler(obj) == &DebugScopeProxy::singleton;
}

/*****************************************************************************/

DebugScopes::DebugScopes(JSContext *cx)
 : proxiedScopes(cx),
   missingScopes(cx),
   liveScopes(cx)
{}

DebugScopes::~DebugScopes()
{
    JS_ASSERT(missingScopes.empty());
    WeakMapBase::removeWeakMapFromList(&proxiedScopes);
}

bool
DebugScopes::init()
{
    if (!liveScopes.init() ||
        !proxiedScopes.init() ||
        !missingScopes.init())
    {
        return false;
    }
    return true;
}

void
DebugScopes::mark(JSTracer *trc)
{
    proxiedScopes.trace(trc);
}

void
DebugScopes::sweep(JSRuntime *rt)
{
    /*
     * Note: missingScopes points to debug scopes weakly not just so that debug
     * scopes can be released more eagerly, but, more importantly, to avoid
     * creating an uncollectable cycle with suspended generator frames.
     */
    for (MissingScopeMap::Enum e(missingScopes); !e.empty(); e.popFront()) {
        if (IsObjectAboutToBeFinalized(e.front().value.unsafeGet()))
            e.removeFront();
    }

    for (LiveScopeMap::Enum e(liveScopes); !e.empty(); e.popFront()) {
        ScopeObject *scope = e.front().key;
        StackFrame *fp = e.front().value;

        /*
         * Scopes can be finalized when a debugger-synthesized ScopeObject is
         * no longer reachable via its DebugScopeObject.
         */
        if (IsObjectAboutToBeFinalized(&scope)) {
            e.removeFront();
            continue;
        }

        /*
         * As explained in onGeneratorFrameChange, liveScopes includes
         * suspended generator frames. Since a generator can be finalized while
         * its scope is live, we must explicitly detect finalized generators.
         */
        if (JSGenerator *gen = fp->maybeSuspendedGenerator(rt)) {
            JS_ASSERT(gen->state == JSGEN_NEWBORN || gen->state == JSGEN_OPEN);
            if (IsObjectAboutToBeFinalized(&gen->obj)) {
                e.removeFront();
                continue;
            }
        }
    }
}

/*
 * Unfortunately, GetDebugScopeForFrame needs to work even outside debug mode
 * (in particular, JS_GetFrameScopeChain does not require debug mode). Since
 * DebugScopes::onPop* are only called in debug mode, this means we cannot
 * use any of the maps in DebugScopes. This will produce debug scope chains
 * that do not obey the debugger invariants but that is just fine.
 */
static bool
CanUseDebugScopeMaps(JSContext *cx)
{
    return cx->compartment->debugMode();
}

DebugScopes *
DebugScopes::ensureCompartmentData(JSContext *cx)
{
    JSCompartment *c = cx->compartment;
    if (c->debugScopes)
        return c->debugScopes;

    c->debugScopes = c->rt->new_<DebugScopes>(cx);
    if (c->debugScopes && c->debugScopes->init())
        return c->debugScopes;

    js_ReportOutOfMemory(cx);
    return NULL;
}

DebugScopeObject *
DebugScopes::hasDebugScope(JSContext *cx, ScopeObject &scope)
{
    DebugScopes *scopes = scope.compartment()->debugScopes;
    if (!scopes)
        return NULL;

    if (ObjectWeakMap::Ptr p = scopes->proxiedScopes.lookup(&scope)) {
        JS_ASSERT(CanUseDebugScopeMaps(cx));
        return &p->value->asDebugScope();
    }

    return NULL;
}

bool
DebugScopes::addDebugScope(JSContext *cx, ScopeObject &scope, DebugScopeObject &debugScope)
{
    JS_ASSERT(cx->compartment == scope.compartment());
    JS_ASSERT(cx->compartment == debugScope.compartment());

    if (!CanUseDebugScopeMaps(cx))
        return true;

    DebugScopes *scopes = ensureCompartmentData(cx);
    if (!scopes)
        return false;

    JS_ASSERT(!scopes->proxiedScopes.has(&scope));
    if (!scopes->proxiedScopes.put(&scope, &debugScope)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    HashTableWriteBarrierPost(cx->compartment, &scopes->proxiedScopes, &scope);
    return true;
}

DebugScopeObject *
DebugScopes::hasDebugScope(JSContext *cx, const ScopeIter &si)
{
    JS_ASSERT(!si.hasScopeObject());

    DebugScopes *scopes = cx->compartment->debugScopes;
    if (!scopes)
        return NULL;

    if (MissingScopeMap::Ptr p = scopes->missingScopes.lookup(si)) {
        JS_ASSERT(CanUseDebugScopeMaps(cx));
        return p->value;
    }
    return NULL;
}

bool
DebugScopes::addDebugScope(JSContext *cx, const ScopeIter &si, DebugScopeObject &debugScope)
{
    JS_ASSERT(!si.hasScopeObject());
    JS_ASSERT(cx->compartment == debugScope.compartment());

    if (!CanUseDebugScopeMaps(cx))
        return true;

    DebugScopes *scopes = ensureCompartmentData(cx);
    if (!scopes)
        return false;

    JS_ASSERT(!scopes->missingScopes.has(si));
    if (!scopes->missingScopes.put(si, &debugScope)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    JS_ASSERT(!scopes->liveScopes.has(&debugScope.scope()));
    if (!scopes->liveScopes.put(&debugScope.scope(), si.fp())) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    return true;
}

void
DebugScopes::onPopCall(StackFrame *fp, JSContext *cx)
{
    JS_ASSERT(!fp->isYielding());
    assertSameCompartment(cx, fp);

    DebugScopes *scopes = cx->compartment->debugScopes;
    if (!scopes)
        return;

    DebugScopeObject *debugScope = NULL;

    if (fp->fun()->isHeavyweight()) {
        /*
         * The StackFrame may be observed before the prologue has created the
         * CallObject. See ScopeIter::settle.
         */
        if (!fp->hasCallObj())
            return;

        CallObject &callobj = fp->scopeChain()->asCall();
        scopes->liveScopes.remove(&callobj);
        if (ObjectWeakMap::Ptr p = scopes->proxiedScopes.lookup(&callobj))
            debugScope = &p->value->asDebugScope();
    } else {
        ScopeIter si(fp, cx);
        if (MissingScopeMap::Ptr p = scopes->missingScopes.lookup(si)) {
            debugScope = p->value;
            scopes->liveScopes.remove(&debugScope->scope().asCall());
            scopes->missingScopes.remove(p);
        }
    }

    /*
     * When the StackFrame is popped, the values of unaliased variables
     * are lost. If there is any debug scope referring to this scope, save a
     * copy of the unaliased variables' values in an array for later debugger
     * access via DebugScopeProxy::handleUnaliasedAccess.
     *
     * Note: since it is simplest for this function to be infallible, failure
     * in this code will be silently ignored. This does not break any
     * invariants since DebugScopeObject::maybeSnapshot can already be NULL.
     */
    if (debugScope) {
        /*
         * Copy all frame values into the snapshot, regardless of
         * aliasing. This unnecessarily includes aliased variables
         * but it simplifies later indexing logic.
         */
        AutoValueVector vec(cx);
        if (!fp->copyRawFrameSlots(&vec) || vec.length() == 0)
            return;

        /*
         * Copy in formals that are not aliased via the scope chain
         * but are aliased via the arguments object.
         */
        RootedScript script(cx, fp->script());
        if (script->needsArgsObj() && fp->hasArgsObj()) {
            for (unsigned i = 0; i < fp->numFormalArgs(); ++i) {
                if (script->formalLivesInArgumentsObject(i))
                    vec[i] = fp->argsObj().arg(i);
            }
        }

        /*
         * Use a dense array as storage (since proxies do not have trace
         * hooks). This array must not escape into the wild.
         */
        RootedObject snapshot(cx, NewDenseCopiedArray(cx, vec.length(), vec.begin()));
        if (!snapshot) {
            cx->clearPendingException();
            return;
        }

        debugScope->initSnapshot(*snapshot);
    }
}

void
DebugScopes::onPopBlock(JSContext *cx, StackFrame *fp)
{
    assertSameCompartment(cx, fp);

    DebugScopes *scopes = cx->compartment->debugScopes;
    if (!scopes)
        return;

    StaticBlockObject &staticBlock = *fp->maybeBlockChain();
    if (staticBlock.needsClone()) {
        ClonedBlockObject &clone = fp->scopeChain()->asClonedBlock();
        clone.copyUnaliasedValues(fp);
        scopes->liveScopes.remove(&clone);
    } else {
        ScopeIter si(fp, cx);
        if (MissingScopeMap::Ptr p = scopes->missingScopes.lookup(si)) {
            ClonedBlockObject &clone = p->value->scope().asClonedBlock();
            clone.copyUnaliasedValues(fp);
            scopes->liveScopes.remove(&clone);
            scopes->missingScopes.remove(p);
        }
    }
}

void
DebugScopes::onPopWith(StackFrame *fp)
{
    DebugScopes *scopes = fp->compartment()->debugScopes;
    if (scopes)
        scopes->liveScopes.remove(&fp->scopeChain()->asWith());
}

void
DebugScopes::onPopStrictEvalScope(StackFrame *fp)
{
    DebugScopes *scopes = fp->compartment()->debugScopes;
    if (!scopes)
        return;

    /*
     * The StackFrame may be observed before the prologue has created the
     * CallObject. See ScopeIter::settle.
     */
    if (fp->hasCallObj())
        scopes->liveScopes.remove(&fp->scopeChain()->asCall());
}

void
DebugScopes::onGeneratorFrameChange(StackFrame *from, StackFrame *to, JSContext *cx)
{
    for (ScopeIter toIter(to, cx); !toIter.done(); ++toIter) {
        DebugScopes *scopes = ensureCompartmentData(cx);
        if (!scopes)
            return;

        if (toIter.hasScopeObject()) {
            /*
             * Not only must we correctly replace mappings [scope -> from] with
             * mappings [scope -> to], but we must add [scope -> to] if it
             * doesn't already exist so that if we need to proxy a generator's
             * scope while it is suspended, we can find its frame (which would
             * otherwise not be found by AllFramesIter).
             */
            JS_ASSERT(toIter.scope().compartment() == cx->compartment);
            LiveScopeMap::AddPtr livePtr = scopes->liveScopes.lookupForAdd(&toIter.scope());
            if (livePtr)
                livePtr->value = to;
            else
                scopes->liveScopes.add(livePtr, &toIter.scope(), to);  // OOM here?
        } else {
            ScopeIter si(toIter, from, cx);
            JS_ASSERT(si.fp()->compartment() == cx->compartment);
            if (MissingScopeMap::Ptr p = scopes->missingScopes.lookup(si)) {
                DebugScopeObject &debugScope = *p->value;
                scopes->liveScopes.lookup(&debugScope.scope())->value = to;
                scopes->missingScopes.remove(p);
                scopes->missingScopes.put(toIter, &debugScope);  // OOM here?
            }
        }
    }
}

void
DebugScopes::onCompartmentLeaveDebugMode(JSCompartment *c)
{
    DebugScopes *scopes = c->debugScopes;
    if (scopes) {
        scopes->proxiedScopes.clear();
        scopes->missingScopes.clear();
        scopes->liveScopes.clear();
    }
}

bool
DebugScopes::updateLiveScopes(JSContext *cx)
{
    JS_CHECK_RECURSION(cx, return false);

    /*
     * Note that we must always update the top frame's scope objects' entries
     * in liveScopes because we can't be sure code hasn't run in that frame to
     * change the scope chain since we were last called. The fp->prevUpToDate()
     * flag indicates whether the scopes of frames older than fp are already
     * included in liveScopes. It might seem simpler to have fp instead carry a
     * flag indicating whether fp itself is accurately described, but then we
     * would need to clear that flag whenever fp ran code. By storing the 'up
     * to date' bit for fp->prev() in fp, simply popping fp effectively clears
     * the flag for us, at exactly the time when execution resumes fp->prev().
     */
    for (AllFramesIter i(cx->runtime->stackSpace); !i.done(); ++i) {
        /*
         * Debug-mode currently disables Ion compilation in the compartment of
         * the debuggee.
         */
        if (i.isIon())
            continue;

        StackFrame *fp = i.interpFrame();
        if (fp->scopeChain()->compartment() != cx->compartment)
            continue;

        for (ScopeIter si(fp, cx); !si.done(); ++si) {
            if (si.hasScopeObject()) {
                JS_ASSERT(si.scope().compartment() == cx->compartment);
                DebugScopes *scopes = ensureCompartmentData(cx);
                if (!scopes)
                    return false;
                if (!scopes->liveScopes.put(&si.scope(), fp))
                    return false;
            }
        }

        if (fp->prevUpToDate())
            return true;
        JS_ASSERT(fp->compartment()->debugMode());
        fp->setPrevUpToDate();
    }

    return true;
}

StackFrame *
DebugScopes::hasLiveFrame(ScopeObject &scope)
{
    DebugScopes *scopes = scope.compartment()->debugScopes;
    if (!scopes)
        return NULL;

    if (LiveScopeMap::Ptr p = scopes->liveScopes.lookup(&scope)) {
        StackFrame *fp = p->value;

        /*
         * Since liveScopes is effectively a weak pointer, we need a read
         * barrier. The scenario where this is necessary is:
         *  1. GC starts, a suspended generator is not live
         *  2. hasLiveFrame returns a StackFrame* to the (soon to be dead)
         *     suspended generator
         *  3. stack frame values (which will neve be marked) are read from the
         *     StackFrame
         *  4. GC completes, live objects may now point to values that weren't
         *     marked and thus may point to swept GC things
         */
        if (JSGenerator *gen = fp->maybeSuspendedGenerator(scope.compartment()->rt))
            JSObject::readBarrier(gen->obj);

        return fp;
    }
    return NULL;
}

/*****************************************************************************/

static JSObject *
GetDebugScope(JSContext *cx, const ScopeIter &si);

static DebugScopeObject *
GetDebugScopeForScope(JSContext *cx, Handle<ScopeObject*> scope, const ScopeIter &enclosing)
{
    if (DebugScopeObject *debugScope = DebugScopes::hasDebugScope(cx, *scope))
        return debugScope;

    RootedObject enclosingDebug(cx, GetDebugScope(cx, enclosing));
    if (!enclosingDebug)
        return NULL;

    JSObject &maybeDecl = scope->enclosingScope();
    if (maybeDecl.isDeclEnv()) {
        JS_ASSERT(CallObjectLambdaName(scope->asCall().callee()));
        enclosingDebug = DebugScopeObject::create(cx, maybeDecl.asDeclEnv(), enclosingDebug);
        if (!enclosingDebug)
            return NULL;
    }

    DebugScopeObject *debugScope = DebugScopeObject::create(cx, *scope, enclosingDebug);
    if (!debugScope)
        return NULL;

    if (!DebugScopes::addDebugScope(cx, *scope, *debugScope))
        return NULL;

    return debugScope;
}

static DebugScopeObject *
GetDebugScopeForMissing(JSContext *cx, const ScopeIter &si)
{
    if (DebugScopeObject *debugScope = DebugScopes::hasDebugScope(cx, si))
        return debugScope;

    ScopeIter copy(si, cx);
    RootedObject enclosingDebug(cx, GetDebugScope(cx, ++copy));
    if (!enclosingDebug)
        return NULL;

    /*
     * Create the missing scope object. For block objects, this takes care of
     * storing variable values after the StackFrame has been popped. For call
     * objects, we only use the pretend call object to access callee, bindings
     * and to receive dynamically added properties. Together, this provides the
     * nice invariant that every DebugScopeObject has a ScopeObject.
     *
     * Note: to preserve scopeChain depth invariants, these lazily-reified
     * scopes must not be put on the frame's scope chain; instead, they are
     * maintained via DebugScopes hooks.
     */
    DebugScopeObject *debugScope = NULL;
    switch (si.type()) {
      case ScopeIter::Call: {
        Rooted<CallObject*> callobj(cx, CallObject::createForFunction(cx, si.fp()));
        if (!callobj)
            return NULL;

        if (callobj->enclosingScope().isDeclEnv()) {
            JS_ASSERT(CallObjectLambdaName(callobj->callee()));
            DeclEnvObject &declenv = callobj->enclosingScope().asDeclEnv();
            enclosingDebug = DebugScopeObject::create(cx, declenv, enclosingDebug);
            if (!enclosingDebug)
                return NULL;
        }

        debugScope = DebugScopeObject::create(cx, *callobj, enclosingDebug);
        break;
      }
      case ScopeIter::Block: {
        Rooted<StaticBlockObject *> staticBlock(cx, &si.staticBlock());
        ClonedBlockObject *block = ClonedBlockObject::create(cx, staticBlock, si.fp());
        if (!block)
            return NULL;

        debugScope = DebugScopeObject::create(cx, *block, enclosingDebug);
        break;
      }
      case ScopeIter::With:
      case ScopeIter::StrictEvalScope:
        JS_NOT_REACHED("should already have a scope");
    }
    if (!debugScope)
        return NULL;

    if (!DebugScopes::addDebugScope(cx, si, *debugScope))
        return NULL;

    return debugScope;
}

static JSObject *
GetDebugScope(JSContext *cx, JSObject &obj)
{
    /*
     * As an engine invariant (maintained internally and asserted by Execute),
     * ScopeObjects and non-ScopeObjects cannot be interleaved on the scope
     * chain; every scope chain must start with zero or more ScopeObjects and
     * terminate with one or more non-ScopeObjects (viz., GlobalObject).
     */
    if (!obj.isScope()) {
#ifdef DEBUG
        JSObject *o = &obj;
        while ((o = o->enclosingScope()))
            JS_ASSERT(!o->isScope());
#endif
        return &obj;
    }

    Rooted<ScopeObject*> scope(cx, &obj.asScope());
    if (StackFrame *fp = DebugScopes::hasLiveFrame(*scope)) {
        ScopeIter si(fp, *scope, cx);
        return GetDebugScope(cx, si);
    }
    ScopeIter si(scope->enclosingScope(), cx);
    return GetDebugScopeForScope(cx, scope, si);
}

static JSObject *
GetDebugScope(JSContext *cx, const ScopeIter &si)
{
    JS_CHECK_RECURSION(cx, return NULL);

    if (si.done())
        return GetDebugScope(cx, si.enclosingScope());

    if (!si.hasScopeObject())
        return GetDebugScopeForMissing(cx, si);

    Rooted<ScopeObject*> scope(cx, &si.scope());

    ScopeIter copy(si, cx);
    return GetDebugScopeForScope(cx, scope, ++copy);
}

JSObject *
js::GetDebugScopeForFunction(JSContext *cx, JSFunction *fun)
{
    assertSameCompartment(cx, fun);
    JS_ASSERT(cx->compartment->debugMode());
    if (!DebugScopes::updateLiveScopes(cx))
        return NULL;
    return GetDebugScope(cx, *fun->environment());
}

JSObject *
js::GetDebugScopeForFrame(JSContext *cx, StackFrame *fp)
{
    assertSameCompartment(cx, fp);
    if (CanUseDebugScopeMaps(cx) && !DebugScopes::updateLiveScopes(cx))
        return NULL;
    ScopeIter si(fp, cx);
    return GetDebugScope(cx, si);
}
