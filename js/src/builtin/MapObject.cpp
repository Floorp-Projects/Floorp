/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
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
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011-2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Jason Orendorff <jorendorff@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "mozilla/FloatingPoint.h"

#include "builtin/MapObject.h"

#include "jscntxt.h"
#include "jsgcmark.h"
#include "jsiter.h"
#include "jsobj.h"

#include "vm/GlobalObject.h"
#include "vm/MethodGuard.h"
#include "vm/Stack.h"

#include "jsobjinlines.h"

using namespace js;

static JSObject *
InitClass(JSContext *cx, Handle<GlobalObject*> global, Class *clasp, JSProtoKey key, Native construct,
          JSFunctionSpec *methods)
{
    RootedVarObject proto(cx, global->createBlankPrototype(cx, clasp));
    if (!proto)
        return NULL;
    proto->setPrivate(NULL);

    JSAtom *atom = cx->runtime->atomState.classAtoms[key];
    RootedVarFunction ctor(cx, global->createConstructor(cx, construct, atom, 1));
    if (!ctor ||
        !LinkConstructorAndPrototype(cx, ctor, proto) ||
        !DefinePropertiesAndBrand(cx, proto, NULL, methods) ||
        !DefineConstructorAndPrototype(cx, global, key, ctor, proto))
    {
        return NULL;
    }
    return proto;
}


/*** HashableValue *******************************************************************************/

bool
HashableValue::setValue(JSContext *cx, const Value &v)
{
    if (v.isString() && v.toString()->isRope()) {
        /* Flatten this rope so that equals() is infallible. */
        JSString *str = v.toString()->ensureLinear(cx);
        if (!str)
            return false;
        value = StringValue(str);
    } else if (v.isDouble()) {
        double d = v.toDouble();
        int32_t i;
        if (MOZ_DOUBLE_IS_INT32(d, &i)) {
            /* Normalize int32-valued doubles to int32 for faster hashing and testing. */
            value = Int32Value(i);
        } else if (MOZ_DOUBLE_IS_NaN(d)) {
            /* NaNs with different bits must hash and test identically. */
            value = DoubleValue(js_NaN);
        } else {
            value = v;
        }
    } else {
        value = v;
    }

    JS_ASSERT(value.isUndefined() || value.isNull() || value.isBoolean() ||
              value.isNumber() || value.isString() || value.isObject());
    return true;
}

HashNumber
HashableValue::hash() const
{
    /*
     * HashableValue::setValue normalizes values so that the SameValue relation
     * on HashableValues is the same as the == relationship on
     * value.data.asBits, except for strings.
     */
    if (value.isString()) {
        JSLinearString &s = value.toString()->asLinear();
        return HashChars(s.chars(), s.length());
    }

    /* Having dispensed with strings, we can just hash asBits. */
    uint64_t u = value.asRawBits();
    return HashNumber((u >> 3) ^ (u >> (32 + 3)) ^ (u << (32 - 3)));
}

bool
HashableValue::equals(const HashableValue &other) const
{
    /* Two HashableValues are equal if they have equal bits or they're equal strings. */
    bool b = (value.asRawBits() == other.value.asRawBits()) ||
              (value.isString() &&
               other.value.isString() &&
               EqualStrings(&value.toString()->asLinear(),
                            &other.value.toString()->asLinear()));

#ifdef DEBUG
    bool same;
    JS_ASSERT(SameValue(NULL, value, other.value, &same));
    JS_ASSERT(same == b);
#endif
    return b;
}


/*** Map *****************************************************************************************/

Class MapObject::class_ = {
    "Map",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Map),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    mark
};

JSFunctionSpec MapObject::methods[] = {
    JS_FN("size", size, 0, 0),
    JS_FN("get", get, 1, 0),
    JS_FN("has", has, 1, 0),
    JS_FN("set", set, 2, 0),
    JS_FN("delete", delete_, 1, 0),
    JS_FS_END
};

JSObject *
MapObject::initClass(JSContext *cx, JSObject *obj)
{
    return InitClass(cx, RootedVar<GlobalObject*>(cx, &obj->asGlobal()),
                     &class_, JSProto_Map, construct, methods);
}

void
MapObject::mark(JSTracer *trc, JSObject *obj)
{
    MapObject *mapobj = static_cast<MapObject *>(obj);
    if (ValueMap *map = mapobj->getData()) {
        for (ValueMap::Range r = map->all(); !r.empty(); r.popFront()) {
            const HeapValue &key = r.front().key;
            HeapValue tmp(key);
            gc::MarkValue(trc, &tmp, "key");
            JS_ASSERT(tmp.get() == key.get());
            gc::MarkValue(trc, &r.front().value, "value");
        }
    }
}

void
MapObject::finalize(FreeOp *fop, JSObject *obj)
{
    MapObject *mapobj = static_cast<MapObject *>(obj);
    if (ValueMap *map = mapobj->getData())
        fop->delete_(map);
}

class AddToMap {
  private:
    ValueMap *map;

  public:
    AddToMap(ValueMap *map) : map(map) {}

    bool operator()(JSContext *cx, const Value &v) {
        JSObject *pairobj = js_ValueToNonNullObject(cx, v);
        if (!pairobj)
            return false;

        Value key;
        if (!pairobj->getElement(cx, 0, &key))
            return false;
        HashableValue hkey;
        if (!hkey.setValue(cx, key))
            return false;

        HashableValue::StackRoot hkeyRoot(cx, &hkey);

        Value val;
        if (!pairobj->getElement(cx, 1, &val))
            return false;

        if (!map->put(hkey, val)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        return true;
    }
};

JSBool
MapObject::construct(JSContext *cx, unsigned argc, Value *vp)
{
    RootedVarObject obj(cx, NewBuiltinClassInstance(cx, &class_));
    if (!obj)
        return false;

    ValueMap *map = cx->new_<ValueMap>(cx->runtime);
    if (!map)
        return false;
    if (!map->init()) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    obj->setPrivate(map);

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.hasDefined(0)) {
        if (!ForOf(cx, args[0], AddToMap(map)))
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

#define UNPACK_THIS(T, native, cx, argc, vp, args, data)                      \
    CallArgs args = CallArgsFromVp(argc, vp);                                 \
    if (!args.thisv().isObject() ||                                           \
        !args.thisv().toObject().hasClass(&T::class_))                        \
    {                                                                         \
        return js::HandleNonGenericMethodClassMismatch(cx, args, native,      \
                                                       &T::class_);           \
    }                                                                         \
    if (!args.thisv().toObject().getPrivate()) {                              \
        ReportIncompatibleMethod(cx, args, &T::class_);                       \
        return false;                                                         \
    }                                                                         \
    T::Data &data = *static_cast<T &>(args.thisv().toObject()).getData();     \
    (void) data

#define THIS_MAP(native, cx, argc, vp, args, map)                             \
    UNPACK_THIS(MapObject, native, cx, argc, vp, args, map)

#define ARG0_KEY(cx, args, key)                                               \
    HashableValue key;                                                        \
    if (args.length() > 0 && !key.setValue(cx, args[0]))                      \
        return false

JSBool
MapObject::size(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_MAP(size, cx, argc, vp, args, map);
    JS_STATIC_ASSERT(sizeof map.count() <= sizeof(uint32_t));
    args.rval().setNumber(map.count());
    return true;
}

JSBool
MapObject::get(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_MAP(get, cx, argc, vp, args, map);
    ARG0_KEY(cx, args, key);

    if (ValueMap::Ptr p = map.lookup(key))
        args.rval() = p->value;
    else
        args.rval().setUndefined();
    return true;
}

JSBool
MapObject::has(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_MAP(has, cx, argc, vp, args, map);
    ARG0_KEY(cx, args, key);
    args.rval().setBoolean(map.lookup(key));
    return true;
}

JSBool
MapObject::set(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_MAP(set, cx, argc, vp, args, map);
    ARG0_KEY(cx, args, key);
    if (!map.put(key, args.length() > 1 ? args[1] : UndefinedValue())) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    args.rval().setUndefined();
    return true;
}

JSBool
MapObject::delete_(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_MAP(delete_, cx, argc, vp, args, map);
    ARG0_KEY(cx, args, key);
    ValueMap::Ptr p = map.lookup(key);
    bool found = p.found();
    if (found)
        map.remove(p);
    args.rval().setBoolean(found);
    return true;
}

JSObject *
js_InitMapClass(JSContext *cx, JSObject *obj)
{
    return MapObject::initClass(cx, obj);
}


/*** Set *****************************************************************************************/

Class SetObject::class_ = {
    "Set",
    JSCLASS_HAS_PRIVATE | JSCLASS_IMPLEMENTS_BARRIERS |
    JSCLASS_HAS_CACHED_PROTO(JSProto_Set),
    JS_PropertyStub,         /* addProperty */
    JS_PropertyStub,         /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    finalize,
    NULL,                    /* checkAccess */
    NULL,                    /* call        */
    NULL,                    /* construct   */
    NULL,                    /* hasInstance */
    mark
};

JSFunctionSpec SetObject::methods[] = {
    JS_FN("size", size, 0, 0),
    JS_FN("has", has, 1, 0),
    JS_FN("add", add, 1, 0),
    JS_FN("delete", delete_, 1, 0),
    JS_FS_END
};

JSObject *
SetObject::initClass(JSContext *cx, JSObject *obj)
{
    return InitClass(cx, RootedVar<GlobalObject*>(cx, &obj->asGlobal()),
                     &class_, JSProto_Set, construct, methods);
}

void
SetObject::mark(JSTracer *trc, JSObject *obj)
{
    SetObject *setobj = static_cast<SetObject *>(obj);
    if (ValueSet *set = setobj->getData()) {
        for (ValueSet::Range r = set->all(); !r.empty(); r.popFront()) {
            const HeapValue &key = r.front();
            HeapValue tmp(key);
            gc::MarkValue(trc, &tmp, "key");
            JS_ASSERT(tmp.get() == key.get());
        }
    }
}

void
SetObject::finalize(FreeOp *fop, JSObject *obj)
{
    SetObject *setobj = static_cast<SetObject *>(obj);
    if (ValueSet *set = setobj->getData())
        fop->delete_(set);
}

class AddToSet {
  private:
    ValueSet *set;

  public:
    AddToSet(ValueSet *set) : set(set) {}

    bool operator()(JSContext *cx, const Value &v) {
        HashableValue key;
        if (!key.setValue(cx, v))
            return false;
        if (!set->put(key)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        return true;
    }
};

JSBool
SetObject::construct(JSContext *cx, unsigned argc, Value *vp)
{
    RootedVarObject obj(cx, NewBuiltinClassInstance(cx, &class_));
    if (!obj)
        return false;

    ValueSet *set = cx->new_<ValueSet>(cx->runtime);
    if (!set)
        return false;
    if (!set->init()) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    obj->setPrivate(set);

    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.hasDefined(0)) {
        if (!ForOf(cx, args[0], AddToSet(set)))
            return false;
    }

    args.rval().setObject(*obj);
    return true;
}

#define THIS_SET(native, cx, argc, vp, args, set)                             \
    UNPACK_THIS(SetObject, native, cx, argc, vp, args, set)

JSBool
SetObject::size(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SET(size, cx, argc, vp, args, set);
    JS_STATIC_ASSERT(sizeof set.count() <= sizeof(uint32_t));
    args.rval().setNumber(set.count());
    return true;
}

JSBool
SetObject::has(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SET(has, cx, argc, vp, args, set);
    ARG0_KEY(cx, args, key);
    args.rval().setBoolean(set.has(key));
    return true;
}

JSBool
SetObject::add(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SET(add, cx, argc, vp, args, set);
    ARG0_KEY(cx, args, key);
    if (!set.put(key)) {
        js_ReportOutOfMemory(cx);
        return false;
    }
    args.rval().setUndefined();
    return true;
}

JSBool
SetObject::delete_(JSContext *cx, unsigned argc, Value *vp)
{
    THIS_SET(delete_, cx, argc, vp, args, set);
    ARG0_KEY(cx, args, key);
    ValueSet::Ptr p = set.lookup(key);
    bool found = p.found();
    if (found)
        set.remove(p);
    args.rval().setBoolean(found);
    return true;
}

JSObject *
js_InitSetClass(JSContext *cx, JSObject *obj)
{
    return SetObject::initClass(cx, obj);
} 
