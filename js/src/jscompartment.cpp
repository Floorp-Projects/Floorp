/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "jsgc.h"
#include "jscntxt.h"
#include "jsproxy.h"
#include "jsscope.h"
#include "methodjit/PolyIC.h"
#include "methodjit/MonoIC.h"

#include "jsgcinlines.h"

using namespace js;
using namespace js::gc;

JSCompartment::JSCompartment(JSRuntime *rt)
  : rt(rt), principals(NULL), data(NULL), marked(false), debugMode(false),
    anynameObject(NULL), functionNamespaceObject(NULL)
{
    JS_INIT_CLIST(&scripts);
}

JSCompartment::~JSCompartment()
{
}

bool
JSCompartment::init()
{
    chunk = NULL;
    shortStringArena.init();
    stringArena.init();
    funArena.init();
#if JS_HAS_XML_SUPPORT
    xmlArena.init();
#endif
    objArena.init();
    for (unsigned i = 0; i < JS_EXTERNAL_STRING_LIMIT; i++)
        externalStringArenas[i].init();
    for (unsigned i = 0; i < FINALIZE_LIMIT; i++)
        freeLists.finalizables[i] = NULL;
#ifdef JS_GCMETER
    memset(&compartmentStats, 0, sizeof(JSGCArenaStats) * FINALIZE_LIMIT);
#endif
    return crossCompartmentWrappers.init();
}

bool
JSCompartment::arenaListsAreEmpty()
{
    bool empty = objArena.isEmpty() &&
                 funArena.isEmpty() &&
#if JS_HAS_XML_SUPPORT
                 xmlArena.isEmpty() &&
#endif
                 shortStringArena.isEmpty() &&
                 stringArena.isEmpty();
  if (!empty)
      return false;

  for (unsigned i = 0; i < JS_EXTERNAL_STRING_LIMIT; i++) {
       if (!externalStringArenas[i].isEmpty())
           return false;
  }

  return true;
}

bool
JSCompartment::wrap(JSContext *cx, Value *vp)
{
    JS_ASSERT(cx->compartment == this);

    uintN flags = 0;

    JS_CHECK_RECURSION(cx, return false);

    /* Only GC things have to be wrapped or copied. */
    if (!vp->isMarkable())
        return true;

    if (vp->isString()) {
        JSString *str = vp->toString();

        /* Static strings do not have to be wrapped. */
        if (JSString::isStatic(str))
            return true;

        /* If the string is already in this compartment, we are done. */
        if (str->asCell()->compartment() == this)
            return true;

        /* If the string is an atom, we don't have to copy. */
        if (str->isAtomized()) {
            JS_ASSERT(str->asCell()->compartment() == cx->runtime->defaultCompartment);
            return true;
        }
    }

    /*
     * Wrappers should really be parented to the wrapped parent of the wrapped
     * object, but in that case a wrapped global object would have a NULL
     * parent without being a proper global object (JSCLASS_IS_GLOBAL). Instead
,
     * we parent all wrappers to the global object in their home compartment.
     * This loses us some transparency, and is generally very cheesy.
     */
    JSObject *global;
    if (cx->hasfp()) {
        global = cx->fp()->scopeChain().getGlobal();
    } else {
        global = cx->globalObject;
        OBJ_TO_INNER_OBJECT(cx, global);
        if (!global)
            return false;
    }

    /* Unwrap incoming objects. */
    if (vp->isObject()) {
        JSObject *obj = &vp->toObject();

        /* If the object is already in this compartment, we are done. */
        if (obj->compartment() == this)
            return true;

        /* Don't unwrap an outer window proxy. */
        if (!obj->getClass()->ext.innerObject) {
            obj = vp->toObject().unwrap(&flags);
            vp->setObject(*obj);
            if (obj->getCompartment() == this)
                return true;

            if (cx->runtime->preWrapObjectCallback)
                obj = cx->runtime->preWrapObjectCallback(cx, global, obj, flags);
            if (!obj)
                return false;

            vp->setObject(*obj);
            if (obj->getCompartment() == this)
                return true;
        } else {
            if (cx->runtime->preWrapObjectCallback)
                obj = cx->runtime->preWrapObjectCallback(cx, global, obj, flags);

            JS_ASSERT(!obj->isWrapper() || obj->getClass()->ext.innerObject);
            vp->setObject(*obj);
        }

#ifdef DEBUG
        {
            JSObject *outer = obj;
            OBJ_TO_OUTER_OBJECT(cx, outer);
            JS_ASSERT(outer && outer == obj);
        }
#endif
    }

    /* If we already have a wrapper for this value, use it. */
    if (WrapperMap::Ptr p = crossCompartmentWrappers.lookup(*vp)) {
        *vp = p->value;
        if (vp->isObject())
            vp->toObject().setParent(global);
        return true;
    }

    if (vp->isString()) {
        Value orig = *vp;
        JSString *str = vp->toString();
        JSString *wrapped = js_NewStringCopyN(cx, str->chars(), str->length());
        if (!wrapped)
            return false;
        vp->setString(wrapped);
        return crossCompartmentWrappers.put(orig, *vp);
    }

    JSObject *obj = &vp->toObject();

    /*
     * Recurse to wrap the prototype. Long prototype chains will run out of
     * stack, causing an error in CHECK_RECURSE.
     *
     * Wrapping the proto before creating the new wrapper and adding it to the
     * cache helps avoid leaving a bad entry in the cache on OOM. But note that
     * if we wrapped both proto and parent, we would get infinite recursion
     * here (since Object.prototype->parent->proto leads to Object.prototype
     * itself).
     */
    JSObject *proto = obj->getProto();
    if (!wrap(cx, &proto))
        return false;

    /*
     * We hand in the original wrapped object into the wrap hook to allow
     * the wrap hook to reason over what wrappers are currently applied
     * to the object.
     */
    JSObject *wrapper = cx->runtime->wrapObjectCallback(cx, obj, proto, global, flags);
    if (!wrapper)
        return false;

    vp->setObject(*wrapper);

    wrapper->setProto(proto);
    if (!crossCompartmentWrappers.put(wrapper->getProxyPrivate(), *vp))
        return false;

    wrapper->setParent(global);
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSString **strp)
{
    AutoValueRooter tvr(cx, StringValue(*strp));
    if (!wrap(cx, tvr.addr()))
        return false;
    *strp = tvr.value().toString();
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, JSObject **objp)
{
    if (!*objp)
        return true;
    AutoValueRooter tvr(cx, ObjectValue(**objp));
    if (!wrap(cx, tvr.addr()))
        return false;
    *objp = &tvr.value().toObject();
    return true;
}

bool
JSCompartment::wrapId(JSContext *cx, jsid *idp)
{
    if (JSID_IS_INT(*idp))
        return true;
    AutoValueRooter tvr(cx, IdToValue(*idp));
    if (!wrap(cx, tvr.addr()))
        return false;
    return ValueToId(cx, tvr.value(), idp);
}

bool
JSCompartment::wrap(JSContext *cx, PropertyOp *propp)
{
    Value v = CastAsObjectJsval(*propp);
    if (!wrap(cx, &v))
        return false;
    *propp = CastAsPropertyOp(v.toObjectOrNull());
    return true;
}

bool
JSCompartment::wrap(JSContext *cx, PropertyDescriptor *desc)
{
    return wrap(cx, &desc->obj) &&
           (!(desc->attrs & JSPROP_GETTER) || wrap(cx, &desc->getter)) &&
           (!(desc->attrs & JSPROP_SETTER) || wrap(cx, &desc->setter)) &&
           wrap(cx, &desc->value);
}

bool
JSCompartment::wrap(JSContext *cx, AutoIdVector &props)
{
    jsid *vector = props.begin();
    jsint length = props.length();
    for (size_t n = 0; n < size_t(length); ++n) {
        if (!wrapId(cx, &vector[n]))
            return false;
    }
    return true;
}

bool
JSCompartment::wrapException(JSContext *cx)
{
    JS_ASSERT(cx->compartment == this);

    if (cx->throwing) {
        AutoValueRooter tvr(cx, cx->exception);
        cx->throwing = false;
        cx->exception.setNull();
        if (wrap(cx, tvr.addr())) {
            cx->throwing = true;
            cx->exception = tvr.value();
        }
        return false;
    }
    return true;
}

void
JSCompartment::sweep(JSContext *cx)
{
    chunk = NULL;
    /* Remove dead wrappers from the table. */
    for (WrapperMap::Enum e(crossCompartmentWrappers); !e.empty(); e.popFront()) {
        JS_ASSERT_IF(IsAboutToBeFinalized(e.front().key.toGCThing()) &&
                     !IsAboutToBeFinalized(e.front().value.toGCThing()),
                     e.front().key.isString());
        if (IsAboutToBeFinalized(e.front().key.toGCThing()) ||
            IsAboutToBeFinalized(e.front().value.toGCThing())) {
            e.removeFront();
        }
    }

#if defined JS_METHODJIT && defined JS_MONOIC
    for (JSCList *cursor = scripts.next; cursor != &scripts; cursor = cursor->next) {
        JSScript *script = reinterpret_cast<JSScript *>(cursor);
        if (script->hasJITCode())
            mjit::ic::SweepCallICs(script);
    }
#endif
}

void
JSCompartment::purge(JSContext *cx)
{
    freeLists.purge();

#ifdef JS_METHODJIT
    for (JSScript *script = (JSScript *)scripts.next;
         &script->links != &scripts;
         script = (JSScript *)script->links.next) {
        if (script->hasJITCode()) {
# if defined JS_POLYIC
            mjit::ic::PurgePICs(cx, script);
# endif
# if defined JS_MONOIC
            /*
             * MICs do not refer to data which can be GC'ed, but are sensitive
             * to shape regeneration.
             */
            if (cx->runtime->gcRegenShapes)
                mjit::ic::PurgeMICs(cx, script);
# endif
        }
    }
#endif
}
