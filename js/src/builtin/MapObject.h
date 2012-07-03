/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MapObject_h__
#define MapObject_h__

#include "jsapi.h"
#include "jscntxt.h"
#include "jsobj.h"

#include "mozilla/FloatingPoint.h"

namespace js {

/*
 * Comparing two ropes for equality can fail. The js::HashTable template
 * requires infallible hash() and match() operations. Therefore we require
 * all values to be converted to hashable form before being used as a key
 * in a Map or Set object.
 *
 * All values except ropes are hashable as-is.
 */
class HashableValue {
    RelocatableValue value;

  public:
    struct Hasher {
        typedef HashableValue Lookup;
        static HashNumber hash(const Lookup &v) { return v.hash(); }
        static bool match(const HashableValue &k, const Lookup &l) { return k.equals(l); }
        static bool isEmpty(const HashableValue &v) { return v.value.isMagic(JS_HASH_KEY_EMPTY); }
        static void makeEmpty(HashableValue *vp) { vp->value = MagicValue(JS_HASH_KEY_EMPTY); }
    };

    HashableValue() : value(UndefinedValue()) {}

    bool setValue(JSContext *cx, const Value &v);
    HashNumber hash() const;
    bool equals(const HashableValue &other) const;
    HashableValue mark(JSTracer *trc) const;
    Value get() const { return value.get(); }

    class AutoRooter : private AutoGCRooter
    {
      public:
        explicit AutoRooter(JSContext *cx, HashableValue *v_
                            JS_GUARD_OBJECT_NOTIFIER_PARAM)
          : AutoGCRooter(cx, HASHABLEVALUE), v(v_), skip(cx, v_)
        {
            JS_GUARD_OBJECT_NOTIFIER_INIT;
        }

        friend void AutoGCRooter::trace(JSTracer *trc);
        void trace(JSTracer *trc);

      private:
        HashableValue *v;
        SkipRoot skip;
        JS_DECL_USE_GUARD_OBJECT_NOTIFIER
    };
};

template <class Key, class Value, class OrderedHashPolicy, class AllocPolicy>
class OrderedHashMap;

template <class T, class OrderedHashPolicy, class AllocPolicy>
class OrderedHashSet;

typedef OrderedHashMap<HashableValue,
                       RelocatableValue,
                       HashableValue::Hasher,
                       RuntimeAllocPolicy> ValueMap;

typedef OrderedHashSet<HashableValue,
                       HashableValue::Hasher,
                       RuntimeAllocPolicy> ValueSet;

class MapObject : public JSObject {
  public:
    static JSObject *initClass(JSContext *cx, JSObject *obj);
    static Class class_;
  private:
    static JSFunctionSpec methods[];
    ValueMap *getData() { return static_cast<ValueMap *>(getPrivate()); }
    static ValueMap & extract(CallReceiver call);
    static void mark(JSTracer *trc, JSObject *obj);
    static void finalize(FreeOp *fop, JSObject *obj);
    static JSBool construct(JSContext *cx, unsigned argc, Value *vp);

    static bool is(const Value &v);

    static bool size_impl(JSContext *cx, CallArgs args);
    static JSBool size(JSContext *cx, unsigned argc, Value *vp);
    static bool get_impl(JSContext *cx, CallArgs args);
    static JSBool get(JSContext *cx, unsigned argc, Value *vp);
    static bool has_impl(JSContext *cx, CallArgs args);
    static JSBool has(JSContext *cx, unsigned argc, Value *vp);
    static bool set_impl(JSContext *cx, CallArgs args);
    static JSBool set(JSContext *cx, unsigned argc, Value *vp);
    static bool delete_impl(JSContext *cx, CallArgs args);
    static JSBool delete_(JSContext *cx, unsigned argc, Value *vp);
};

class SetObject : public JSObject {
  public:
    static JSObject *initClass(JSContext *cx, JSObject *obj);
    static Class class_;
  private:
    static JSFunctionSpec methods[];
    ValueSet *getData() { return static_cast<ValueSet *>(getPrivate()); }
    static ValueSet & extract(CallReceiver call);
    static void mark(JSTracer *trc, JSObject *obj);
    static void finalize(FreeOp *fop, JSObject *obj);
    static JSBool construct(JSContext *cx, unsigned argc, Value *vp);

    static bool is(const Value &v);

    static bool size_impl(JSContext *cx, CallArgs args);
    static JSBool size(JSContext *cx, unsigned argc, Value *vp);
    static bool has_impl(JSContext *cx, CallArgs args);
    static JSBool has(JSContext *cx, unsigned argc, Value *vp);
    static bool add_impl(JSContext *cx, CallArgs args);
    static JSBool add(JSContext *cx, unsigned argc, Value *vp);
    static bool delete_impl(JSContext *cx, CallArgs args);
    static JSBool delete_(JSContext *cx, unsigned argc, Value *vp);
};

} /* namespace js */

extern JSObject *
js_InitMapClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitSetClass(JSContext *cx, JSObject *obj);

#endif  /* MapObject_h__ */
