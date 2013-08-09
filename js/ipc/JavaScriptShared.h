/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=80:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_jsipc_JavaScriptShared_h__
#define mozilla_jsipc_JavaScriptShared_h__

#include "jsapi.h"
#include "jspubtd.h"
#include "js/HashTable.h"
#include "mozilla/dom/DOMTypes.h"
#include "mozilla/jsipc/PJavaScript.h"
#include "nsJSUtils.h"
#include "nsFrameMessageManager.h"

namespace mozilla {
namespace jsipc {

typedef uint64_t ObjectId;

class JavaScriptShared;

class CpowIdHolder : public CpowHolder
{
  public:
    CpowIdHolder(JavaScriptShared *js, const InfallibleTArray<CpowEntry> &cpows)
      : js_(js),
        cpows_(cpows)
    {
    }

    bool ToObject(JSContext *cx, JSObject **objp);

  private:
    JavaScriptShared *js_;
    const InfallibleTArray<CpowEntry> &cpows_;
};

// Map ids -> JSObjects
class ObjectStore
{
    typedef js::DefaultHasher<ObjectId> TableKeyHasher;

    typedef js::HashMap<ObjectId, JS::Heap<JSObject *>, TableKeyHasher, js::SystemAllocPolicy> ObjectTable;

  public:
    ObjectStore();

    bool init();
    void trace(JSTracer *trc);

    bool add(ObjectId id, JSObject *obj);
    JSObject *find(ObjectId id);
    void remove(ObjectId id);

  private:
    ObjectTable table_;
};

// Map JSObjects -> ids
class ObjectIdCache
{
    typedef js::PointerHasher<JSObject *, 3> Hasher;
    typedef js::HashMap<JSObject *, ObjectId, Hasher, js::SystemAllocPolicy> ObjectIdTable;

  public:
    ObjectIdCache();

    bool init();
    void trace(JSTracer *trc);

    bool add(JSContext *cx, JSObject *obj, ObjectId id);
    ObjectId find(JSObject *obj);
    void remove(JSObject *obj);

  private:
    static void keyMarkCallback(JSTracer *trc, void *key, void *data);

    ObjectIdTable table_;
};

class JavaScriptShared
{
  public:
    bool init();

    static const uint32_t OBJECT_EXTRA_BITS  = 1;
    static const uint32_t OBJECT_IS_CALLABLE = (1 << 0);

    bool Unwrap(JSContext *cx, const InfallibleTArray<CpowEntry> &aCpows, JSObject **objp);
    bool Wrap(JSContext *cx, JS::HandleObject aObj, InfallibleTArray<CpowEntry> *outCpows);

  protected:
    bool toVariant(JSContext *cx, jsval from, JSVariant *to);
    bool toValue(JSContext *cx, const JSVariant &from, JS::MutableHandleValue to);
    bool fromDescriptor(JSContext *cx, const JSPropertyDescriptor &desc, PPropertyDescriptor *out);
    bool toDescriptor(JSContext *cx, const PPropertyDescriptor &in, JSPropertyDescriptor *out);
    bool convertIdToGeckoString(JSContext *cx, JS::HandleId id, nsString *to);
    bool convertGeckoStringToId(JSContext *cx, const nsString &from, JS::MutableHandleId id);

    bool toValue(JSContext *cx, const JSVariant &from, jsval *to) {
        JS::RootedValue v(cx);
        if (!toValue(cx, from, &v))
            return false;
        *to = v;
        return true;
    }

    virtual bool makeId(JSContext *cx, JSObject *obj, ObjectId *idp) = 0;
    virtual JSObject *unwrap(JSContext *cx, ObjectId id) = 0;

    bool unwrap(JSContext *cx, ObjectId id, JS::MutableHandle<JSObject*> objp) {
        if (!id) {
            objp.set(NULL);
            return true;
        }

        objp.set(unwrap(cx, id));
        return bool(objp.get());
    }

    static void ConvertID(const nsID &from, JSIID *to);
    static void ConvertID(const JSIID &from, nsID *to);

    JSObject *findObject(uint32_t objId) {
        return objects_.find(objId);
    }

  protected:
    ObjectStore objects_;
};

// Use 47 at most, to be safe, since jsval privates are encoded as doubles.
static const uint64_t MAX_CPOW_IDS = (uint64_t(1) << 47) - 1;

} // namespace jsipc
} // namespace mozilla

#endif
