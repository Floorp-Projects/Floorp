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

StaticBlockObject *
js::ScopeCoordinateBlockChain(JSScript *script, jsbytecode *pc)
{
    ScopeCoordinate sc(pc);

    uint32_t blockIndex = GET_UINT32_INDEX(pc + 2 * sizeof(uint16_t));
    if (blockIndex == UINT32_MAX)
        return NULL;

    StaticBlockObject *block = &script->getObject(blockIndex)->asStaticBlock();
    unsigned i = 0;
    while (true) {
        while (block && !block->needsClone())
            block = block->enclosingBlock();
        if (i++ == sc.hops)
            break;
        block = block->enclosingBlock();
    }
    return block;
}

PropertyName *
js::ScopeCoordinateName(JSRuntime *rt, JSScript *script, jsbytecode *pc)
{
    StaticBlockObject *maybeBlock = ScopeCoordinateBlockChain(script, pc);
    ScopeCoordinate sc(pc);
    uint32_t targetSlot = ScopeObject::CALL_BLOCK_RESERVED_SLOTS + sc.slot;
    Shape *shape = maybeBlock ? maybeBlock->lastProperty() : script->bindings.lastShape();
    Shape::Range r = shape->all();
    while (r.front().slot() != targetSlot)
        r.popFront();
    jsid id = r.front().propid();
    /* Beware nameless destructuring formal. */
    if (!JSID_IS_ATOM(id))
        return rt->atomState.emptyAtom;
    return JSID_TO_ATOM(id)->asPropertyName();
}

FrameVarType
js::ScopeCoordinateToFrameVar(JSScript *script, jsbytecode *pc, unsigned *index)
{
    ScopeCoordinate sc(pc);
    if (StaticBlockObject *block = ScopeCoordinateBlockChain(script, pc)) {
        *index = block->slotToFrameLocal(script, sc.slot);
        return FrameVar_Local;
    }

    if (script->bindings.slotIsLocal(sc.slot)) {
        *index = script->bindings.slotToLocal(sc.slot);
        return FrameVar_Local;
    }

    *index = script->bindings.slotToArg(sc.slot);
    return FrameVar_Arg;
}

/*****************************************************************************/

/*
 * Construct a call object for the given bindings.  If this is a call object
 * for a function invocation, callee should be the function being called.
 * Otherwise it must be a call object for eval of strict mode code, and callee
 * must be null.
 */
CallObject *
CallObject::create(JSContext *cx, JSScript *script, HandleObject enclosing, HandleFunction callee)
{
    RootedShape shape(cx);
    shape = script->bindings.callObjectShape(cx);
    if (shape == NULL)
        return NULL;

    gc::AllocKind kind = gc::GetGCObjectKind(shape->numFixedSlots());
    JS_ASSERT(CanBeFinalizedInBackground(kind, &CallClass));
    kind = gc::GetBackgroundAllocKind(kind);

    RootedTypeObject type(cx);
    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    HeapSlot *slots;
    if (!PreallocateObjectDynamicSlots(cx, shape, &slots))
        return NULL;

    RootedObject obj(cx, JSObject::create(cx, kind, shape, type, slots));
    if (!obj)
        return NULL;

    /*
     * Update the parent for bindings associated with non-compileAndGo scripts,
     * whose call objects do not have a consistent global variable and need
     * to be updated dynamically.
     */
    if (&enclosing->global() != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        if (!JSObject::setParent(cx, obj, RootedObject(cx, &enclosing->global())))
            return NULL;
    }

    if (!obj->asScope().setEnclosingScope(cx, enclosing))
        return NULL;

    obj->initFixedSlot(CALLEE_SLOT, ObjectOrNullValue(callee));

    /*
     * If |bindings| is for a function that has extensible parents, that means
     * its Call should have its own shape; see BaseShape::extensibleParents.
     */
    if (obj->lastProperty()->extensibleParents()) {
        if (!obj->generateOwnShape(cx))
            return NULL;
    }

    return &obj->asCall();
}

CallObject *
CallObject::createForFunction(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(fp->isNonEvalFunctionFrame());

    RootedObject scopeChain(cx, fp->scopeChain());

    /*
     * For a named function expression Call's parent points to an environment
     * object holding function's name.
     */
    if (js_IsNamedLambda(fp->fun())) {
        scopeChain = DeclEnvObject::create(cx, fp);
        if (!scopeChain)
            return NULL;
    }

    RootedScript script(cx, fp->script());
    CallObject *callobj = create(cx, script, scopeChain, RootedFunction(cx, &fp->callee()));
    if (!callobj)
        return NULL;

    /* Copy in the closed-over formal arguments. */
    if (script->bindingsAccessedDynamically) {
        Value *formals = fp->formals();
        for (unsigned slot = 0, n = fp->fun()->nargs; slot < n; ++slot)
            callobj->setArg(slot, formals[slot]);
    } else if (unsigned n = script->numClosedArgs()) {
        Value *formals = fp->formals();
        for (unsigned i = 0; i < n; ++i) {
            uint32_t slot = script->getClosedArg(i);
            callobj->setArg(slot, formals[slot]);
        }
    }

    return callobj;
}

void
CallObject::copyUnaliasedValues(StackFrame *fp)
{
    JS_ASSERT(fp->script() == getCalleeFunction()->script());
    JSScript *script = fp->script();

    /* If bindings are accessed dynamically, everything is aliased. */
    if (script->bindingsAccessedDynamically)
        return;

    /* Copy the unaliased formals. */
    for (unsigned i = 0; i < script->bindings.numArgs(); ++i) {
        if (!script->formalLivesInCallObject(i)) {
            if (script->argsObjAliasesFormals() && fp->hasArgsObj())
                setArg(i, fp->argsObj().arg(i), DONT_CHECK_ALIASING);
            else
                setArg(i, fp->unaliasedFormal(i, DONT_CHECK_ALIASING), DONT_CHECK_ALIASING);
        }
    }

    /* Copy the unaliased var/let bindings. */
    for (unsigned i = 0; i < script->bindings.numVars(); ++i) {
        if (!script->varIsAliased(i))
            setVar(i, fp->unaliasedLocal(i), DONT_CHECK_ALIASING);
    }
}

CallObject *
CallObject::createForStrictEval(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(fp->isStrictEvalFrame());
    JS_ASSERT(cx->fp() == fp);
    JS_ASSERT(cx->regs().pc == fp->script()->code);

    return create(cx, fp->script(), fp->scopeChain(), RootedFunction(cx));
}

JSBool
CallObject::setArgOp(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp)
{
    CallObject &callobj = obj->asCall();

    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    JSScript *script = callobj.getCalleeFunction()->script();
    JS_ASSERT(script->formalLivesInCallObject(i));

    callobj.setArg(i, *vp);

    if (!script->ensureHasTypes(cx))
        return false;

    TypeScript::SetArgument(cx, script, i, *vp);
    return true;
}

JSBool
CallObject::setVarOp(JSContext *cx, HandleObject obj, HandleId id, JSBool strict, Value *vp)
{
    CallObject &callobj = obj->asCall();

    JS_ASSERT((int16_t) JSID_TO_INT(id) == JSID_TO_INT(id));
    unsigned i = (uint16_t) JSID_TO_INT(id);

    JSScript *script = callobj.getCalleeFunction()->script();
    JS_ASSERT(script->varIsAliased(i));

    callobj.setVar(i, *vp);

    if (!script->ensureHasTypes(cx))
        return false;

    TypeScript::SetLocal(cx, script, i, *vp);
    return true;
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
    JSCLASS_HAS_PRIVATE |
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

DeclEnvObject *
DeclEnvObject::create(JSContext *cx, StackFrame *fp)
{
    RootedTypeObject type(cx);
    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    RootedShape emptyDeclEnvShape(cx);
    emptyDeclEnvShape = EmptyShape::getInitialShape(cx, &DeclEnvClass, NULL,
                                                    &fp->global(), FINALIZE_KIND);
    if (!emptyDeclEnvShape)
        return NULL;

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, emptyDeclEnvShape, type, NULL));
    if (!obj)
        return NULL;

    if (!obj->asScope().setEnclosingScope(cx, fp->scopeChain()))
        return NULL;


    if (!DefineNativeProperty(cx, obj, RootedId(cx, AtomToId(fp->fun()->atom)),
                              ObjectValue(fp->callee()), NULL, NULL,
                              JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY,
                              0, 0)) {
        return NULL;
    }

    return &obj->asDeclEnv();
}

WithObject *
WithObject::create(JSContext *cx, HandleObject proto, HandleObject enclosing, uint32_t depth)
{
    RootedTypeObject type(cx);
    type = proto->getNewType(cx);
    if (!type)
        return NULL;

    RootedShape emptyWithShape(cx);
    emptyWithShape = EmptyShape::getInitialShape(cx, &WithClass, proto,
                                                 &enclosing->global(), FINALIZE_KIND);
    if (!emptyWithShape)
        return NULL;

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, emptyWithShape, type, NULL));
    if (!obj)
        return NULL;

    if (!obj->asScope().setEnclosingScope(cx, enclosing))
        return NULL;

    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(depth));

    JSObject *thisp = proto->thisObject(cx);
    if (!thisp)
        return NULL;

    obj->setFixedSlot(THIS_SLOT, ObjectValue(*thisp));

    return &obj->asWith();
}

static JSBool
with_LookupGeneric(JSContext *cx, HandleObject obj, HandleId id, JSObject **objp, JSProperty **propp)
{
    return obj->asWith().object().lookupGeneric(cx, id, objp, propp);
}

static JSBool
with_LookupProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, JSObject **objp, JSProperty **propp)
{
    return with_LookupGeneric(cx, obj, RootedId(cx, NameToId(name)), objp, propp);
}

static JSBool
with_LookupElement(JSContext *cx, HandleObject obj, uint32_t index, JSObject **objp,
                   JSProperty **propp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
with_LookupSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, JSObject **objp, JSProperty **propp)
{
    return with_LookupGeneric(cx, obj, RootedId(cx, SPECIALID_TO_JSID(sid)), objp, propp);
}

static JSBool
with_GetGeneric(JSContext *cx, HandleObject obj, HandleObject receiver, HandleId id, Value *vp)
{
    return obj->asWith().object().getGeneric(cx, id, vp);
}

static JSBool
with_GetProperty(JSContext *cx, HandleObject obj, HandleObject receiver, HandlePropertyName name, Value *vp)
{
    return with_GetGeneric(cx, obj, receiver, RootedId(cx, NameToId(name)), vp);
}

static JSBool
with_GetElement(JSContext *cx, HandleObject obj, HandleObject receiver, uint32_t index, Value *vp)
{
    RootedId id(cx);
    if (!IndexToId(cx, index, id.address()))
        return false;
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
with_GetSpecial(JSContext *cx, HandleObject obj, HandleObject receiver, HandleSpecialId sid, Value *vp)
{
    return with_GetGeneric(cx, obj, receiver, RootedId(cx, SPECIALID_TO_JSID(sid)), vp);
}

static JSBool
with_SetGeneric(JSContext *cx, HandleObject obj, HandleId id, Value *vp, JSBool strict)
{
    return obj->asWith().object().setGeneric(cx, id, vp, strict);
}

static JSBool
with_SetProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *vp, JSBool strict)
{
    return obj->asWith().object().setProperty(cx, name, vp, strict);
}

static JSBool
with_SetElement(JSContext *cx, HandleObject obj, uint32_t index, Value *vp, JSBool strict)
{
    return obj->asWith().object().setElement(cx, index, vp, strict);
}

static JSBool
with_SetSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *vp, JSBool strict)
{
    return obj->asWith().object().setSpecial(cx, sid, vp, strict);
}

static JSBool
with_GetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    return obj->asWith().object().getGenericAttributes(cx, id, attrsp);
}

static JSBool
with_GetPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    return obj->asWith().object().getPropertyAttributes(cx, name, attrsp);
}

static JSBool
with_GetElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    return obj->asWith().object().getElementAttributes(cx, index, attrsp);
}

static JSBool
with_GetSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    return obj->asWith().object().getSpecialAttributes(cx, sid, attrsp);
}

static JSBool
with_SetGenericAttributes(JSContext *cx, HandleObject obj, HandleId id, unsigned *attrsp)
{
    return obj->asWith().object().setGenericAttributes(cx, id, attrsp);
}

static JSBool
with_SetPropertyAttributes(JSContext *cx, HandleObject obj, HandlePropertyName name, unsigned *attrsp)
{
    return obj->asWith().object().setPropertyAttributes(cx, name, attrsp);
}

static JSBool
with_SetElementAttributes(JSContext *cx, HandleObject obj, uint32_t index, unsigned *attrsp)
{
    return obj->asWith().object().setElementAttributes(cx, index, attrsp);
}

static JSBool
with_SetSpecialAttributes(JSContext *cx, HandleObject obj, HandleSpecialId sid, unsigned *attrsp)
{
    return obj->asWith().object().setSpecialAttributes(cx, sid, attrsp);
}

static JSBool
with_DeleteProperty(JSContext *cx, HandleObject obj, HandlePropertyName name, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteProperty(cx, name, rval, strict);
}

static JSBool
with_DeleteElement(JSContext *cx, HandleObject obj, uint32_t index, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteElement(cx, index, rval, strict);
}

static JSBool
with_DeleteSpecial(JSContext *cx, HandleObject obj, HandleSpecialId sid, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteSpecial(cx, sid, rval, strict);
}

static JSBool
with_Enumerate(JSContext *cx, HandleObject obj, JSIterateOp enum_op,
               Value *statep, jsid *idp)
{
    return obj->asWith().object().enumerate(cx, enum_op, statep, idp);
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
        NULL,             /* clear */
    }
};

/*****************************************************************************/

ClonedBlockObject *
ClonedBlockObject::create(JSContext *cx, Handle<StaticBlockObject *> block, StackFrame *fp)
{
    RootedTypeObject type(cx);
    type = block->getNewType(cx);
    if (!type)
        return NULL;

    HeapSlot *slots;
    if (!PreallocateObjectDynamicSlots(cx, block->lastProperty(), &slots))
        return NULL;

    RootedShape shape(cx);
    shape = block->lastProperty();

    RootedObject obj(cx, JSObject::create(cx, FINALIZE_KIND, shape, type, slots));
    if (!obj)
        return NULL;

    /* Set the parent if necessary, as for call objects. */
    if (&fp->global() != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        if (!JSObject::setParent(cx, obj, RootedObject(cx, &fp->global())))
            return NULL;
    }

    JS_ASSERT(!obj->inDictionaryMode());
    JS_ASSERT(obj->slotSpan() >= block->slotCount() + RESERVED_SLOTS);

    obj->setReservedSlot(SCOPE_CHAIN_SLOT, ObjectValue(*fp->scopeChain()));
    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(block->stackDepth()));

    if (obj->lastProperty()->extensibleParents() && !obj->generateOwnShape(cx))
        return NULL;

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

    return &obj->asClonedBlock();
}

void
ClonedBlockObject::copyUnaliasedValues(StackFrame *fp)
{
    StaticBlockObject &block = staticBlock();
    unsigned base = block.slotToFrameLocal(fp->script(), 0);
    for (unsigned i = 0; i < slotCount(); ++i) {
        if (!block.isAliased(i))
            setVar(i, fp->unaliasedLocal(base + i), DONT_CHECK_ALIASING);
    }
}

StaticBlockObject *
StaticBlockObject::create(JSContext *cx)
{
    RootedTypeObject type(cx);
    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    RootedShape emptyBlockShape(cx);
    emptyBlockShape = EmptyShape::getInitialShape(cx, &BlockClass, NULL, NULL, FINALIZE_KIND);
    if (!emptyBlockShape)
        return NULL;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyBlockShape, type, NULL);
    if (!obj)
        return NULL;

    return &obj->asStaticBlock();
}

const Shape *
StaticBlockObject::addVar(JSContext *cx, jsid id, int index, bool *redeclared)
{
    JS_ASSERT(JSID_IS_ATOM(id) || (JSID_IS_INT(id) && JSID_TO_INT(id) == index));

    *redeclared = false;

    /* Inline JSObject::addProperty in order to trap the redefinition case. */
    Shape **spp;
    if (Shape::search(cx, lastProperty(), id, &spp, true)) {
        *redeclared = true;
        return NULL;
    }

    /*
     * Don't convert this object to dictionary mode so that we can clone the
     * block's shape later.
     */
    uint32_t slot = JSSLOT_FREE(&BlockClass) + index;
    return addPropertyInternal(cx, id, /* getter = */ NULL, /* setter = */ NULL,
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

#define NO_PARENT_INDEX UINT32_MAX

/*
 * If there's a parent id, then get the parent out of our script's object
 * array. We know that we clone block objects in outer-to-inner order, which
 * means that getting the parent now will work.
 */
static uint32_t
FindObjectIndex(JSScript *script, StaticBlockObject *maybeBlock)
{
    if (!maybeBlock || !script->hasObjects())
        return NO_PARENT_INDEX;

    ObjectArray *objects = script->objects();
    HeapPtrObject *vector = objects->vector;
    unsigned length = objects->length;
    for (unsigned i = 0; i < length; ++i) {
        if (vector[i] == maybeBlock)
            return i;
    }

    return NO_PARENT_INDEX;
}

template<XDRMode mode>
bool
js::XDRStaticBlockObject(XDRState<mode> *xdr, JSScript *script, StaticBlockObject **objp)
{
    /* NB: Keep this in sync with CloneStaticBlockObject. */

    JSContext *cx = xdr->cx();

    StaticBlockObject *obj = NULL;
    uint32_t parentId = 0;
    uint32_t count = 0;
    uint32_t depthAndCount = 0;
    if (mode == XDR_ENCODE) {
        obj = *objp;
        parentId = FindObjectIndex(script, obj->enclosingBlock());
        uint32_t depth = obj->stackDepth();
        JS_ASSERT(depth <= UINT16_MAX);
        count = obj->slotCount();
        JS_ASSERT(count <= UINT16_MAX);
        depthAndCount = (depth << 16) | uint16_t(count);
    }

    /* First, XDR the parent atomid. */
    if (!xdr->codeUint32(&parentId))
        return false;

    if (mode == XDR_DECODE) {
        obj = StaticBlockObject::create(cx);
        if (!obj)
            return false;
        *objp = obj;

        obj->setEnclosingBlock(parentId == NO_PARENT_INDEX
                               ? NULL
                               : &script->getObject(parentId)->asStaticBlock());
    }

    AutoObjectRooter tvr(cx, obj);

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
            JSAtom *atom;
            if (!XDRAtom(xdr, &atom))
                return false;

            /* The empty string indicates an int id. */
            jsid id = atom != cx->runtime->emptyString
                      ? AtomToId(atom)
                      : INT_TO_JSID(i);

            bool redeclared;
            if (!obj->addVar(cx, id, i, &redeclared)) {
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
            const Shape *shape = &r.front();
            shapes[shape->shortid()] = shape;
        }

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (unsigned i = 0; i < count; i++) {
            const Shape *shape = shapes[i];
            JS_ASSERT(shape->hasDefaultGetter());
            JS_ASSERT(unsigned(shape->shortid()) == i);

            jsid propid = shape->propid();
            JS_ASSERT(JSID_IS_ATOM(propid) || JSID_IS_INT(propid));

            /* The empty string indicates an int id. */
            JSAtom *atom = JSID_IS_ATOM(propid)
                           ? JSID_TO_ATOM(propid)
                           : cx->runtime->emptyString;

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
js::XDRStaticBlockObject(XDRState<XDR_ENCODE> *xdr, JSScript *script, StaticBlockObject **objp);

template bool
js::XDRStaticBlockObject(XDRState<XDR_DECODE> *xdr, JSScript *script, StaticBlockObject **objp);

JSObject *
js::CloneStaticBlockObject(JSContext *cx, StaticBlockObject &srcBlock,
                           const AutoObjectVector &objects, JSScript *src)
{
    /* NB: Keep this in sync with XDRStaticBlockObject. */

    StaticBlockObject *clone = StaticBlockObject::create(cx);
    if (!clone)
        return NULL;

    uint32_t parentId = FindObjectIndex(src, srcBlock.enclosingBlock());
    clone->setEnclosingBlock(parentId == NO_PARENT_INDEX
                             ? NULL
                             : &objects[parentId]->asStaticBlock());

    clone->setStackDepth(srcBlock.stackDepth());

    /* Shape::Range is reverse order, so build a list in forward order. */
    AutoShapeVector shapes(cx);
    if (!shapes.growBy(srcBlock.slotCount()))
        return NULL;
    for (Shape::Range r = srcBlock.lastProperty()->all(); !r.empty(); r.popFront())
        shapes[r.front().shortid()] = &r.front();

    for (const Shape **p = shapes.begin(); p != shapes.end(); ++p) {
        jsid id = (*p)->propid();
        unsigned i = (*p)->shortid();

        bool redeclared;
        if (!clone->addVar(cx, id, i, &redeclared)) {
            JS_ASSERT(!redeclared);
            return NULL;
        }

        clone->setAliased(i, srcBlock.isAliased(i));
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
            if (CallObjectLambdaName(fp_->fun()))
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
        JS_ASSERT_IF(type_ == Call, callobj.getCalleeFunction()->script() == fp_->script());
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

namespace js {

/*
 * DebugScopeProxy is the handler for DebugScopeObject proxy objects and mostly
 * just wraps ScopeObjects. Having a custom handler (rather than trying to
 * reuse js::Wrapper) gives us several important abilities:
 *  - We want to pass the ScopeObject as the receiver to forwarded scope
 *    property ops so that Call/Block/With ops do not all require a
 *    'normalization' step.
 *  - The debug scope proxy can directly manipulate the stack frame to allow
 *    the debugger to read/write args/locals that were otherwise unaliased.
 *  - The engine has made certain assumptions about the possible reads/writes
 *    in a scope. DebugScopeProxy allows us to prevent the debugger from
 *    breaking those assumptions. Examples include adding shadowing variables
 *    or changing the property attributes of bindings.
 *  - The engine makes optimizations that are observable to the debugger. The
 *    proxy can either hide these optimizations or make the situation more
 *    clear to the debugger. An example is 'arguments'.
 */
class DebugScopeProxy : public BaseProxyHandler
{
    enum Action { SET, GET };

    /*
     * This function handles access to unaliased locals/formals. If such
     * accesses were passed on directly to the DebugScopeObject::scope, they
     * would not be reading/writing the canonical location for the variable,
     * which is on the stack. Thus, handleUnaliasedAccess must translate
     * accesses to scope objects into analogous accesses of the stack frame.
     *
     * handleUnaliasedAccess returns 'true' if the access was unaliased and
     * completed by handleUnaliasedAccess.
     */
    bool handleUnaliasedAccess(JSContext *cx, ScopeObject &scope, jsid id, Action action, Value *vp)
    {
        Shape *shape = scope.lastProperty()->search(cx, id);
        if (!shape)
            return false;

        StackFrame *maybefp = cx->runtime->debugScopes->hasLiveFrame(scope);

        if (scope.isCall() && !scope.asCall().isForEval()) {
            CallObject &callobj = scope.asCall();
            JSScript *script = callobj.getCalleeFunction()->script();
            if (!script->ensureHasTypes(cx))
                return false;

            if (shape->setterOp() == CallObject::setVarOp) {
                unsigned i = shape->shortid();
                if (script->varIsAliased(i))
                    return false;

                if (maybefp) {
                    if (action == GET)
                        *vp = maybefp->unaliasedVar(i);
                    else
                        maybefp->unaliasedVar(i) = *vp;
                } else {
                    if (action == GET)
                        *vp = callobj.var(i, DONT_CHECK_ALIASING);
                    else
                        callobj.setVar(i, *vp, DONT_CHECK_ALIASING);
                }

                if (action == SET)
                    TypeScript::SetLocal(cx, script, i, *vp);

                return true;
            }

            if (shape->setterOp() == CallObject::setArgOp) {
                unsigned i = shape->shortid();
                if (script->formalLivesInCallObject(i))
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
                } else {
                    if (action == GET)
                        *vp = callobj.arg(i, DONT_CHECK_ALIASING);
                    else
                        callobj.setArg(i, *vp, DONT_CHECK_ALIASING);
                }

                if (action == SET)
                    TypeScript::SetArgument(cx, script, i, *vp);

                return true;
            }

            return false;
        }

        if (scope.isClonedBlock()) {
            ClonedBlockObject &block = scope.asClonedBlock();
            unsigned i = shape->shortid();
            if (block.staticBlock().isAliased(i))
                return false;

            if (maybefp) {
                JSScript *script = maybefp->script();
                unsigned local = block.slotToFrameLocal(script, i);
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

        JS_ASSERT(scope.isDeclEnv() || scope.isWith() || scope.asCall().isForEval());
        return false;
    }

    static bool isArguments(JSContext *cx, jsid id)
    {
        return id == NameToId(cx->runtime->atomState.argumentsAtom);
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
               !scope.asCall().getCalleeFunction()->script()->argumentsHasLocalBinding();
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
        *maybeArgsObj = NULL;

        if (!isArguments(cx, id) || !isFunctionScope(scope))
            return true;

        JSScript *script = scope.asCall().getCalleeFunction()->script();
        if (script->needsArgsObj())
            return true;

        StackFrame *maybefp = cx->runtime->debugScopes->hasLiveFrame(scope);
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

    bool getOwnPropertyDescriptor(JSContext *cx, JSObject *proxy, jsid id, bool set,
                                  PropertyDescriptor *desc) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        ArgumentsObject *maybeArgsObj;
        if (!checkForMissingArguments(cx, id, scope, &maybeArgsObj))
            return false;

        if (maybeArgsObj) {
            PodZero(desc);
            desc->obj = proxy;
            desc->attrs = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
            desc->value = ObjectValue(*maybeArgsObj);
            return true;
        }

        Value v;
        if (handleUnaliasedAccess(cx, scope, id, GET, &v)) {
            PodZero(desc);
            desc->obj = proxy;
            desc->attrs = JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT;
            desc->value = v;
            return true;
        }

        return JS_GetPropertyDescriptorById(cx, &scope, id, JSRESOLVE_QUALIFIED, desc);
    }

    bool get(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, Value *vp) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        ArgumentsObject *maybeArgsObj;
        if (!checkForMissingArguments(cx, id, scope, &maybeArgsObj))
            return false;

        if (maybeArgsObj) {
            *vp = ObjectValue(*maybeArgsObj);
            return true;
        }

        if (handleUnaliasedAccess(cx, scope, id, GET, vp))
            return true;

        return scope.getGeneric(cx, RootedObject(cx, &scope), RootedId(cx, id), vp);
    }

    bool set(JSContext *cx, JSObject *proxy, JSObject *receiver, jsid id, bool strict,
                     Value *vp) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        if (handleUnaliasedAccess(cx, scope, id, SET, vp))
            return true;

        return scope.setGeneric(cx, RootedId(cx, id), vp, strict);
    }

    bool defineProperty(JSContext *cx, JSObject *proxy, jsid id, PropertyDescriptor *desc) MOZ_OVERRIDE
    {
        bool found;
        if (!has(cx, proxy, id, &found))
            return false;
        if (found)
            return Throw(cx, id, JSMSG_CANT_REDEFINE_PROP);

        ScopeObject &scope = proxy->asDebugScope().scope();
        return JS_DefinePropertyById(cx, &scope, id, desc->value, desc->getter, desc->setter,
                                     desc->attrs);
    }

    bool getOwnPropertyNames(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        if (isMissingArgumentsBinding(scope) &&
            !props.append(NameToId(cx->runtime->atomState.argumentsAtom)))
        {
            return false;
        }

        return GetPropertyNames(cx, &scope, JSITER_OWNONLY, &props);
    }

    bool delete_(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE
    {
        return js_ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_CANT_DELETE,
                                        JSDVG_IGNORE_STACK, IdToValue(id), NULL,
                                        NULL, NULL);
    }

    bool enumerate(JSContext *cx, JSObject *proxy, AutoIdVector &props) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        if (isMissingArgumentsBinding(scope) &&
            !props.append(NameToId(cx->runtime->atomState.argumentsAtom)))
        {
            return false;
        }

        return GetPropertyNames(cx, &scope, 0, &props);
    }

    bool has(JSContext *cx, JSObject *proxy, jsid id, bool *bp) MOZ_OVERRIDE
    {
        ScopeObject &scope = proxy->asDebugScope().scope();

        if (isArguments(cx, id) && isFunctionScope(scope)) {
            *bp = true;
            return true;
        }

        JSBool found;
        if (!JS_HasPropertyById(cx, &scope, id, &found))
            return false;

        *bp = found;
        return true;
    }
};

}  /* namespace js */

int DebugScopeProxy::family = 0;
DebugScopeProxy DebugScopeProxy::singleton;

/* static */ DebugScopeObject *
DebugScopeObject::create(JSContext *cx, ScopeObject &scope, HandleObject enclosing)
{
    JSObject *obj = NewProxyObject(cx, &DebugScopeProxy::singleton, ObjectValue(scope),
                                   NULL /* proto */, &scope.global(),
                                   NULL /* call */, NULL /* construct */);
    if (!obj)
        return NULL;

    JS_ASSERT(!enclosing->isScope());
    SetProxyExtra(obj, ENCLOSING_EXTRA, ObjectValue(*enclosing.value()));

    return &obj->asDebugScope();
}

ScopeObject &
DebugScopeObject::scope() const
{
    return Wrapper::wrappedObject(this)->asScope();
}

JSObject &
DebugScopeObject::enclosingScope() const
{
    return GetProxyExtra(this, ENCLOSING_EXTRA).toObject();
}

bool
DebugScopeObject::isForDeclarative() const
{
    ScopeObject &s = scope();
    return s.isCall() || s.isBlock() || s.isDeclEnv();
}

bool
js_IsDebugScopeSlow(const JSObject *obj)
{
    return obj->getClass() == &ObjectProxyClass &&
           GetProxyHandler(obj) == &DebugScopeProxy::singleton;
}

/*****************************************************************************/

DebugScopes::DebugScopes(JSRuntime *rt)
 : rt(rt),
   proxiedScopes(rt),
   missingScopes(rt),
   liveScopes(rt)
{}

DebugScopes::~DebugScopes()
{
    JS_ASSERT(missingScopes.empty());
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
DebugScopes::sweep()
{
    /*
     * Note: missingScopes points to debug scopes weakly not just so that debug
     * scopes can be released more eagerly, but, more importantly, to avoid
     * creating an uncollectable cycle with suspended generator frames.
     */
    for (MissingScopeMap::Enum e(missingScopes); !e.empty(); e.popFront()) {
        if (!IsObjectMarked(e.front().value.unsafeGet()))
            e.removeFront();
    }

    for (LiveScopeMap::Enum e(liveScopes); !e.empty(); e.popFront()) {
        ScopeObject *scope = e.front().key;
        StackFrame *fp = e.front().value;

        /*
         * Scopes can be finalized when a debugger-synthesized ScopeObject is
         * no longer reachable via its DebugScopeObject.
         */
        if (!IsObjectMarked(&scope)) {
            e.removeFront();
            continue;
        }

        /*
         * As explained in onGeneratorFrameChange, liveScopes includes
         * suspended generator frames. Since a generator can be finalized while
         * its scope is live, we must explicitly detect finalized generators.
         * Since the scope is still live, we simulate the onPop* call by
         * copying unaliased variables into the scope object.
         */
        if (JSGenerator *gen = fp->maybeSuspendedGenerator(rt)) {
            JS_ASSERT(gen->state == JSGEN_NEWBORN || gen->state == JSGEN_OPEN);
            if (!IsObjectMarked(&gen->obj)) {
                if (scope->isCall())
                    scope->asCall().copyUnaliasedValues(fp);
                else if (scope->isBlock())
                    scope->asClonedBlock().copyUnaliasedValues(fp);
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

DebugScopeObject *
DebugScopes::hasDebugScope(JSContext *cx, ScopeObject &scope) const
{
    if (ObjectWeakMap::Ptr p = proxiedScopes.lookup(&scope)) {
        JS_ASSERT(CanUseDebugScopeMaps(cx));
        return &p->value->asDebugScope();
    }
    return NULL;
}

bool
DebugScopes::addDebugScope(JSContext *cx, ScopeObject &scope, DebugScopeObject &debugScope)
{
    if (!CanUseDebugScopeMaps(cx))
        return true;

    JS_ASSERT(!proxiedScopes.has(&scope));
    if (!proxiedScopes.put(&scope, &debugScope)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

DebugScopeObject *
DebugScopes::hasDebugScope(JSContext *cx, const ScopeIter &si) const
{
    JS_ASSERT(!si.hasScopeObject());
    if (MissingScopeMap::Ptr p = missingScopes.lookup(si)) {
        JS_ASSERT(CanUseDebugScopeMaps(cx));
        return p->value;
    }
    return NULL;
}

bool
DebugScopes::addDebugScope(JSContext *cx, const ScopeIter &si, DebugScopeObject &debugScope)
{
    JS_ASSERT(!si.hasScopeObject());
    if (!CanUseDebugScopeMaps(cx))
        return true;

    JS_ASSERT(!missingScopes.has(si));
    if (!missingScopes.put(si, &debugScope)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    JS_ASSERT(!liveScopes.has(&debugScope.scope()));
    if (!liveScopes.put(&debugScope.scope(), si.fp())) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

void
DebugScopes::onPopCall(StackFrame *fp, JSContext *cx)
{
    JS_ASSERT(!fp->isYielding());
    if (fp->fun()->isHeavyweight()) {
        /*
         * The StackFrame may be observed before the prologue has created the
         * CallObject. See ScopeIter::settle.
         */
        if (fp->hasCallObj()) {
            CallObject &callobj = fp->scopeChain()->asCall();
            callobj.copyUnaliasedValues(fp);
            liveScopes.remove(&callobj);
        }
    } else {
        ScopeIter si(fp, cx);
        if (MissingScopeMap::Ptr p = missingScopes.lookup(si)) {
            CallObject &callobj = p->value->scope().asCall();
            callobj.copyUnaliasedValues(fp);
            liveScopes.remove(&callobj);
            missingScopes.remove(p);
        }
    }
}

void
DebugScopes::onPopBlock(JSContext *cx, StackFrame *fp)
{
    StaticBlockObject &staticBlock = *fp->maybeBlockChain();
    if (staticBlock.needsClone()) {
        ClonedBlockObject &clone = fp->scopeChain()->asClonedBlock();
        clone.copyUnaliasedValues(fp);
        liveScopes.remove(&clone);
    } else {
        ScopeIter si(fp, cx);
        if (MissingScopeMap::Ptr p = missingScopes.lookup(si)) {
            ClonedBlockObject &clone = p->value->scope().asClonedBlock();
            clone.copyUnaliasedValues(fp);
            liveScopes.remove(&clone);
            missingScopes.remove(p);
        }
    }
}

void
DebugScopes::onPopWith(StackFrame *fp)
{
    liveScopes.remove(&fp->scopeChain()->asWith());
}

void
DebugScopes::onPopStrictEvalScope(StackFrame *fp)
{
    /*
     * The StackFrame may be observed before the prologue has created the
     * CallObject. See ScopeIter::settle.
     */
    if (fp->hasCallObj())
        liveScopes.remove(&fp->scopeChain()->asCall());
}

void
DebugScopes::onGeneratorFrameChange(StackFrame *from, StackFrame *to, JSContext *cx)
{
    for (ScopeIter toIter(to, cx); !toIter.done(); ++toIter) {
        if (toIter.hasScopeObject()) {
            /*
             * Not only must we correctly replace mappings [scope -> from] with
             * mappings [scope -> to], but we must add [scope -> to] if it
             * doesn't already exist so that if we need to proxy a generator's
             * scope while it is suspended, we can find its frame (which would
             * otherwise not be found by AllFramesIter).
             */
            LiveScopeMap::AddPtr livePtr = liveScopes.lookupForAdd(&toIter.scope());
            if (livePtr)
                livePtr->value = to;
            else
                liveScopes.add(livePtr, &toIter.scope(), to);
        } else {
            ScopeIter si(toIter, from, cx);
            if (MissingScopeMap::Ptr p = missingScopes.lookup(si)) {
                DebugScopeObject &debugScope = *p->value;
                liveScopes.lookup(&debugScope.scope())->value = to;
                missingScopes.remove(p);
                missingScopes.put(toIter, &debugScope);
            }
        }
    }
}

void
DebugScopes::onCompartmentLeaveDebugMode(JSCompartment *c)
{
    for (MissingScopeMap::Enum e(missingScopes); !e.empty(); e.popFront()) {
        if (e.front().key.fp()->compartment() == c)
            e.removeFront();
    }
    for (LiveScopeMap::Enum e(liveScopes); !e.empty(); e.popFront()) {
        if (e.front().key->compartment() == c)
            e.removeFront();
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
        StackFrame *fp = i.fp();
        if (fp->isDummyFrame() || fp->scopeChain()->compartment() != cx->compartment)
            continue;

        for (ScopeIter si(fp, cx); !si.done(); ++si) {
            if (si.hasScopeObject() && !liveScopes.put(&si.scope(), fp))
                return false;
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
    if (LiveScopeMap::Ptr p = liveScopes.lookup(&scope)) {
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
        if (JSGenerator *gen = fp->maybeSuspendedGenerator(rt))
            JSObject::readBarrier(gen->obj);

        return fp;
    }
    return NULL;
}

/*****************************************************************************/

static JSObject *
GetDebugScope(JSContext *cx, const ScopeIter &si);

static DebugScopeObject *
GetDebugScopeForScope(JSContext *cx, ScopeObject &scope, const ScopeIter &enclosing)
{
    DebugScopes &debugScopes = *cx->runtime->debugScopes;
    if (DebugScopeObject *debugScope = debugScopes.hasDebugScope(cx, scope))
        return debugScope;

    RootedObject enclosingDebug(cx, GetDebugScope(cx, enclosing));
    if (!enclosingDebug)
        return NULL;

    JSObject &maybeDecl = scope.enclosingScope();
    if (maybeDecl.isDeclEnv()) {
        JS_ASSERT(CallObjectLambdaName(scope.asCall().getCalleeFunction()));
        enclosingDebug = DebugScopeObject::create(cx, maybeDecl.asDeclEnv(), enclosingDebug);
        if (!enclosingDebug)
            return NULL;
    }

    DebugScopeObject *debugScope = DebugScopeObject::create(cx, scope, enclosingDebug);
    if (!debugScope)
        return NULL;

    if (!debugScopes.addDebugScope(cx, scope, *debugScope))
        return NULL;

    return debugScope;
}

static DebugScopeObject *
GetDebugScopeForMissing(JSContext *cx, const ScopeIter &si)
{
    DebugScopes &debugScopes = *cx->runtime->debugScopes;
    if (DebugScopeObject *debugScope = debugScopes.hasDebugScope(cx, si))
        return debugScope;

    ScopeIter copy(si, cx);
    RootedObject enclosingDebug(cx, GetDebugScope(cx, ++copy));
    if (!enclosingDebug)
        return NULL;

    /*
     * Create the missing scope object. This takes care of storing variable
     * values after the StackFrame has been popped. To preserve scopeChain
     * depth invariants, these lazily-reified scopes must not be put on the
     * frame's scope chain; instead, they are maintained via DebugScopes hooks.
     */
    DebugScopeObject *debugScope = NULL;
    switch (si.type()) {
      case ScopeIter::Call: {
        CallObject *callobj = CallObject::createForFunction(cx, si.fp());
        if (!callobj)
            return NULL;

        if (callobj->enclosingScope().isDeclEnv()) {
            JS_ASSERT(CallObjectLambdaName(callobj->getCalleeFunction()));
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

    if (!debugScopes.addDebugScope(cx, si, *debugScope))
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

    ScopeObject &scope = obj.asScope();
    if (StackFrame *fp = cx->runtime->debugScopes->hasLiveFrame(scope)) {
        ScopeIter si(fp, scope, cx);
        return GetDebugScope(cx, si);
    }
    ScopeIter si(scope.enclosingScope(), cx);
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

    ScopeIter copy(si, cx);
    return GetDebugScopeForScope(cx, si.scope(), ++copy);
}

JSObject *
js::GetDebugScopeForFunction(JSContext *cx, JSFunction *fun)
{
    assertSameCompartment(cx, fun);
    JS_ASSERT(cx->compartment->debugMode());
    if (!cx->runtime->debugScopes->updateLiveScopes(cx))
        return NULL;
    return GetDebugScope(cx, *fun->environment());
}

JSObject *
js::GetDebugScopeForFrame(JSContext *cx, StackFrame *fp)
{
    assertSameCompartment(cx, fp);
    if (CanUseDebugScopeMaps(cx) && !cx->runtime->debugScopes->updateLiveScopes(cx))
        return NULL;
    ScopeIter si(fp, cx);
    return GetDebugScope(cx, si);
}
