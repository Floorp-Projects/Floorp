/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey call object code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Paul Biggar <pbiggar@mozilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jscompartment.h"
#include "jsiter.h"
#include "jsscope.h"
#if JS_HAS_XDR
#include "jsxdrapi.h"
#endif

#include "GlobalObject.h"
#include "ScopeObject.h"

#include "jsatominlines.h"
#include "jsobjinlines.h"

#include "ScopeObject-inl.h"

using namespace js;
using namespace js::types;

/*
 * Construct a call object for the given bindings.  If this is a call object
 * for a function invocation, callee should be the function being called.
 * Otherwise it must be a call object for eval of strict mode code, and callee
 * must be null.
 */
CallObject *
CallObject::create(JSContext *cx, JSScript *script, JSObject &enclosing, JSObject *callee)
{
    Bindings &bindings = script->bindings;
    gc::AllocKind kind = gc::GetGCObjectKind(bindings.lastShape()->numFixedSlots() + 1);

    RootedVarTypeObject type(cx);

    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    HeapValue *slots;
    if (!PreallocateObjectDynamicSlots(cx, bindings.lastShape(), &slots))
        return NULL;

    RootedVarShape shape(cx);
    shape = bindings.lastShape();

    JSObject *obj = JSObject::create(cx, kind, shape, type, slots);
    if (!obj)
        return NULL;

    /*
     * Update the parent for bindings associated with non-compileAndGo scripts,
     * whose call objects do not have a consistent global variable and need
     * to be updated dynamically.
     */
    JSObject &global = enclosing.global();
    if (&global != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        if (!obj->setParent(cx, &global))
            return NULL;
    }

#ifdef DEBUG
    JS_ASSERT(!obj->inDictionaryMode());
    for (Shape::Range r = obj->lastProperty(); !r.empty(); r.popFront()) {
        const Shape &s = r.front();
        if (s.hasSlot()) {
            JS_ASSERT(s.slot() + 1 == obj->slotSpan());
            break;
        }
    }
#endif

    if (!obj->asScope().setEnclosingScope(cx, enclosing))
        return NULL;

    JS_ASSERT_IF(callee, callee->isFunction());
    obj->initFixedSlot(CALLEE_SLOT, ObjectOrNullValue(callee));

    /*
     * If |bindings| is for a function that has extensible parents, that means
     * its Call should have its own shape; see BaseShape::extensibleParents.
     */
    if (obj->lastProperty()->extensibleParents() && !obj->generateOwnShape(cx))
        return NULL;

    return &obj->asCall();
}

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
    RootedVarTypeObject type(cx);
    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    RootedVarShape emptyDeclEnvShape(cx);
    emptyDeclEnvShape = EmptyShape::getInitialShape(cx, &DeclEnvClass, NULL,
                                                    &fp->scopeChain().global(),
                                                    FINALIZE_KIND);
    if (!emptyDeclEnvShape)
        return NULL;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyDeclEnvShape, type, NULL);
    if (!obj)
        return NULL;

    obj->setPrivate(fp);
    if (!obj->asScope().setEnclosingScope(cx, fp->scopeChain()))
        return NULL;

    return &obj->asDeclEnv();
}

WithObject *
WithObject::create(JSContext *cx, StackFrame *fp, JSObject &proto, JSObject &enclosing,
                   uint32_t depth)
{
    RootedVarTypeObject type(cx);
    type = proto.getNewType(cx);
    if (!type)
        return NULL;

    RootedVarShape emptyWithShape(cx);
    emptyWithShape = EmptyShape::getInitialShape(cx, &WithClass, &proto,
                                                 &enclosing.global(), FINALIZE_KIND);
    if (!emptyWithShape)
        return NULL;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyWithShape, type, NULL);
    if (!obj)
        return NULL;

    if (!obj->asScope().setEnclosingScope(cx, enclosing))
        return NULL;

    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(depth));
    obj->setPrivate(js_FloatingFrameIfGenerator(cx, fp));

    JSObject *thisp = proto.thisObject(cx);
    if (!thisp)
        return NULL;

    obj->setFixedSlot(THIS_SLOT, ObjectValue(*thisp));

    return &obj->asWith();
}

static JSBool
with_LookupGeneric(JSContext *cx, JSObject *obj, jsid id, JSObject **objp, JSProperty **propp)
{
    /* Fixes bug 463997 */
    uintN flags = cx->resolveFlags;
    if (flags == RESOLVE_INFER)
        flags = js_InferFlags(cx, flags);
    flags |= JSRESOLVE_WITH;
    JSAutoResolveFlags rf(cx, flags);
    return obj->asWith().object().lookupGeneric(cx, id, objp, propp);
}

static JSBool
with_LookupProperty(JSContext *cx, JSObject *obj, PropertyName *name, JSObject **objp, JSProperty **propp)
{
    return with_LookupGeneric(cx, obj, ATOM_TO_JSID(name), objp, propp);
}

static JSBool
with_LookupElement(JSContext *cx, JSObject *obj, uint32_t index, JSObject **objp,
                   JSProperty **propp)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return with_LookupGeneric(cx, obj, id, objp, propp);
}

static JSBool
with_LookupSpecial(JSContext *cx, JSObject *obj, SpecialId sid, JSObject **objp, JSProperty **propp)
{
    return with_LookupGeneric(cx, obj, SPECIALID_TO_JSID(sid), objp, propp);
}

static JSBool
with_GetGeneric(JSContext *cx, JSObject *obj, JSObject *receiver, jsid id, Value *vp)
{
    return obj->asWith().object().getGeneric(cx, id, vp);
}

static JSBool
with_GetProperty(JSContext *cx, JSObject *obj, JSObject *receiver, PropertyName *name, Value *vp)
{
    return with_GetGeneric(cx, obj, receiver, ATOM_TO_JSID(name), vp);
}

static JSBool
with_GetElement(JSContext *cx, JSObject *obj, JSObject *receiver, uint32_t index, Value *vp)
{
    jsid id;
    if (!IndexToId(cx, index, &id))
        return false;
    return with_GetGeneric(cx, obj, receiver, id, vp);
}

static JSBool
with_GetSpecial(JSContext *cx, JSObject *obj, JSObject *receiver, SpecialId sid, Value *vp)
{
    return with_GetGeneric(cx, obj, receiver, SPECIALID_TO_JSID(sid), vp);
}

static JSBool
with_SetGeneric(JSContext *cx, JSObject *obj, jsid id, Value *vp, JSBool strict)
{
    return obj->asWith().object().setGeneric(cx, id, vp, strict);
}

static JSBool
with_SetProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *vp, JSBool strict)
{
    return obj->asWith().object().setProperty(cx, name, vp, strict);
}

static JSBool
with_SetElement(JSContext *cx, JSObject *obj, uint32_t index, Value *vp, JSBool strict)
{
    return obj->asWith().object().setElement(cx, index, vp, strict);
}

static JSBool
with_SetSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *vp, JSBool strict)
{
    return obj->asWith().object().setSpecial(cx, sid, vp, strict);
}

static JSBool
with_GetGenericAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    return obj->asWith().object().getGenericAttributes(cx, id, attrsp);
}

static JSBool
with_GetPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, uintN *attrsp)
{
    return obj->asWith().object().getPropertyAttributes(cx, name, attrsp);
}

static JSBool
with_GetElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, uintN *attrsp)
{
    return obj->asWith().object().getElementAttributes(cx, index, attrsp);
}

static JSBool
with_GetSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, uintN *attrsp)
{
    return obj->asWith().object().getSpecialAttributes(cx, sid, attrsp);
}

static JSBool
with_SetGenericAttributes(JSContext *cx, JSObject *obj, jsid id, uintN *attrsp)
{
    return obj->asWith().object().setGenericAttributes(cx, id, attrsp);
}

static JSBool
with_SetPropertyAttributes(JSContext *cx, JSObject *obj, PropertyName *name, uintN *attrsp)
{
    return obj->asWith().object().setPropertyAttributes(cx, name, attrsp);
}

static JSBool
with_SetElementAttributes(JSContext *cx, JSObject *obj, uint32_t index, uintN *attrsp)
{
    return obj->asWith().object().setElementAttributes(cx, index, attrsp);
}

static JSBool
with_SetSpecialAttributes(JSContext *cx, JSObject *obj, SpecialId sid, uintN *attrsp)
{
    return obj->asWith().object().setSpecialAttributes(cx, sid, attrsp);
}

static JSBool
with_DeleteGeneric(JSContext *cx, JSObject *obj, jsid id, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteGeneric(cx, id, rval, strict);
}

static JSBool
with_DeleteProperty(JSContext *cx, JSObject *obj, PropertyName *name, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteProperty(cx, name, rval, strict);
}

static JSBool
with_DeleteElement(JSContext *cx, JSObject *obj, uint32_t index, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteElement(cx, index, rval, strict);
}

static JSBool
with_DeleteSpecial(JSContext *cx, JSObject *obj, SpecialId sid, Value *rval, JSBool strict)
{
    return obj->asWith().object().deleteSpecial(cx, sid, rval, strict);
}

static JSBool
with_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
               Value *statep, jsid *idp)
{
    return obj->asWith().object().enumerate(cx, enum_op, statep, idp);
}

static JSType
with_TypeOf(JSContext *cx, JSObject *obj)
{
    return JSTYPE_OBJECT;
}

static JSObject *
with_ThisObject(JSContext *cx, JSObject *obj)
{
    return &obj->asWith().withThis();
}

Class js::WithClass = {
    "With",
    JSCLASS_HAS_PRIVATE |
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
    NULL,                    /* reserved    */
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* xdrObject   */
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
        with_DeleteGeneric,
        with_DeleteProperty,
        with_DeleteElement,
        with_DeleteSpecial,
        with_Enumerate,
        with_TypeOf,
        NULL,             /* fix   */
        with_ThisObject,
        NULL,             /* clear */
    }
};

ClonedBlockObject *
ClonedBlockObject::create(JSContext *cx, StaticBlockObject &block, StackFrame *fp)
{
    RootedVarTypeObject type(cx);
    type = block.getNewType(cx);
    if (!type)
        return NULL;

    HeapValue *slots;
    if (!PreallocateObjectDynamicSlots(cx, block.lastProperty(), &slots))
        return NULL;

    RootedVarShape shape(cx);
    shape = block.lastProperty();

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, shape, type, slots);
    if (!obj)
        return NULL;

    /* Set the parent if necessary, as for call objects. */
    JSObject &global = fp->scopeChain().global();
    if (&global != obj->getParent()) {
        JS_ASSERT(obj->getParent() == NULL);
        if (!obj->setParent(cx, &global))
            return NULL;
    }

    JS_ASSERT(!obj->inDictionaryMode());
    JS_ASSERT(obj->slotSpan() >= block.slotCount() + RESERVED_SLOTS);

    obj->setReservedSlot(DEPTH_SLOT, PrivateUint32Value(block.stackDepth()));
    obj->setPrivate(js_FloatingFrameIfGenerator(cx, fp));

    if (obj->lastProperty()->extensibleParents() && !obj->generateOwnShape(cx))
        return NULL;

    return &obj->asClonedBlock();
}

void
ClonedBlockObject::put(JSContext *cx)
{
    StackFrame *fp = cx->fp();
    JS_ASSERT(maybeStackFrame() == js_FloatingFrameIfGenerator(cx, fp));

    uint32_t count = slotCount();
    uint32_t depth = stackDepth();

    /* The block and its locals must be on the current stack for GC safety. */
    JS_ASSERT(depth <= uint32_t(cx->regs().sp - fp->base()));
    JS_ASSERT(count <= uint32_t(cx->regs().sp - fp->base() - depth));

    /* See comments in CheckDestructuring in frontend/Parser.cpp. */
    JS_ASSERT(count >= 1);

    copySlotRange(RESERVED_SLOTS, fp->base() + depth, count);

    /* We must clear the private slot even with errors. */
    setPrivate(NULL);
    fp->setScopeChainNoCallObj(enclosingScope());
}

static JSBool
block_getProperty(JSContext *cx, JSObject *obj, jsid id, Value *vp)
{
    /*
     * Block objects are never exposed to script, and the engine handles them
     * with care. So unlike other getters, this one can assert (rather than
     * check) certain invariants about obj.
     */
    ClonedBlockObject &block = obj->asClonedBlock();
    uintN index = (uintN) JSID_TO_INT(id);
    JS_ASSERT(index < block.slotCount());

    if (StackFrame *fp = block.maybeStackFrame()) {
        fp = js_LiveFrameIfGenerator(fp);
        index += fp->numFixed() + block.stackDepth();
        JS_ASSERT(index < fp->numSlots());
        *vp = fp->slots()[index];
        return true;
    }

    /* Values are in slots immediately following the class-reserved ones. */
    JS_ASSERT(block.closedSlot(index) == *vp);
    return true;
}

static JSBool
block_setProperty(JSContext *cx, JSObject *obj, jsid id, JSBool strict, Value *vp)
{
    ClonedBlockObject &block = obj->asClonedBlock();
    uintN index = (uintN) JSID_TO_INT(id);
    JS_ASSERT(index < block.slotCount());

    if (StackFrame *fp = block.maybeStackFrame()) {
        fp = js_LiveFrameIfGenerator(fp);
        index += fp->numFixed() + block.stackDepth();
        JS_ASSERT(index < fp->numSlots());
        fp->slots()[index] = *vp;
        return true;
    }

    /*
     * The value in *vp will be written back to the slot in obj that was
     * allocated when this let binding was defined.
     */
    return true;
}

StaticBlockObject *
StaticBlockObject::create(JSContext *cx)
{
    RootedVarTypeObject type(cx);
    type = cx->compartment->getEmptyType(cx);
    if (!type)
        return NULL;

    RootedVarShape emptyBlockShape(cx);
    emptyBlockShape = EmptyShape::getInitialShape(cx, &BlockClass, NULL, NULL, FINALIZE_KIND);
    if (!emptyBlockShape)
        return NULL;

    JSObject *obj = JSObject::create(cx, FINALIZE_KIND, emptyBlockShape, type, NULL);
    if (!obj)
        return NULL;

    return &obj->asStaticBlock();
}

const Shape *
StaticBlockObject::addVar(JSContext *cx, jsid id, intN index, bool *redeclared)
{
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
    return addPropertyInternal(cx, id, block_getProperty, block_setProperty,
                               slot, JSPROP_ENUMERATE | JSPROP_PERMANENT,
                               Shape::HAS_SHORTID, index, spp,
                               /* allowDictionary = */ false);
}

Class js::BlockClass = {
    "Block",
    JSCLASS_HAS_PRIVATE |
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

#if JS_HAS_XDR

#define NO_PARENT_INDEX UINT32_MAX

static uint32_t
FindObjectIndex(JSObjectArray *array, JSObject *obj)
{
    size_t i;

    if (array) {
        i = array->length;
        do {

            if (array->vector[--i] == obj)
                return i;
        } while (i != 0);
    }

    return NO_PARENT_INDEX;
}

bool
js_XDRStaticBlockObject(JSXDRState *xdr, StaticBlockObject **objp)
{
    JSContext *cx;
    uint32_t parentId;
    StaticBlockObject *obj, *parent;
    uintN depth, count;
    uint32_t depthAndCount;
    const Shape *shape;

    cx = xdr->cx;
#ifdef __GNUC__
    obj = NULL;         /* quell GCC overwarning */
#endif

    if (xdr->mode == JSXDR_ENCODE) {
        obj = *objp;
        parent = obj->enclosingBlock();
        parentId = JSScript::isValidOffset(xdr->script->objectsOffset)
                   ? FindObjectIndex(xdr->script->objects(), parent)
                   : NO_PARENT_INDEX;
        depth = uint16_t(obj->stackDepth());
        count = uint16_t(obj->slotCount());
        depthAndCount = uint32_t(depth << 16) | count;
    }
#ifdef __GNUC__ /* suppress bogus gcc warnings */
    else count = 0;
#endif

    /* First, XDR the parent atomid. */
    if (!JS_XDRUint32(xdr, &parentId))
        return JS_FALSE;

    if (xdr->mode == JSXDR_DECODE) {
        obj = StaticBlockObject::create(cx);
        if (!obj)
            return JS_FALSE;
        *objp = obj;

        /*
         * If there's a parent id, then get the parent out of our script's
         * object array. We know that we XDR block object in outer-to-inner
         * order, which means that getting the parent now will work.
         */
        if (parentId == NO_PARENT_INDEX)
            parent = NULL;
        else
            parent = &xdr->script->getObject(parentId)->asStaticBlock();
        obj->setEnclosingBlock(parent);
    }

    AutoObjectRooter tvr(cx, obj);

    if (!JS_XDRUint32(xdr, &depthAndCount))
        return false;

    if (xdr->mode == JSXDR_DECODE) {
        depth = uint16_t(depthAndCount >> 16);
        count = uint16_t(depthAndCount);
        obj->setStackDepth(depth);

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (uintN i = 0; i < count; i++) {
            JSAtom *atom;

            /* XDR the real id. */
            if (!js_XDRAtom(xdr, &atom))
                return false;

            bool redeclared;
            if (!obj->addVar(cx, ATOM_TO_JSID(atom), i, &redeclared)) {
                JS_ASSERT(!redeclared);
                return false;
            }
        }
    } else {
        AutoShapeVector shapes(cx);
        shapes.growBy(count);

        for (Shape::Range r(obj->lastProperty()); !r.empty(); r.popFront()) {
            shape = &r.front();
            shapes[shape->shortid()] = shape;
        }

        /*
         * XDR the block object's properties. We know that there are 'count'
         * properties to XDR, stored as id/shortid pairs.
         */
        for (uintN i = 0; i < count; i++) {
            shape = shapes[i];
            JS_ASSERT(shape->getter() == block_getProperty);

            jsid propid = shape->propid();
            JS_ASSERT(JSID_IS_ATOM(propid));
            JSAtom *atom = JSID_TO_ATOM(propid);

#ifdef DEBUG
            uint16_t shortid = uint16_t(shape->shortid());
            JS_ASSERT(shortid == i);
#endif

            /* XDR the real id. */
            if (!js_XDRAtom(xdr, &atom))
                return false;
        }
    }
    return true;
}

#endif  /* JS_HAS_XDR */
