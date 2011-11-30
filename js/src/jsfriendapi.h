/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *
 * The Original Code is SpiderMonkey code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
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

#ifndef jsfriendapi_h___
#define jsfriendapi_h___

#include "jsclass.h"
#include "jspubtd.h"
#include "jsprvtd.h"

JS_BEGIN_EXTERN_C

extern JS_FRIEND_API(void)
JS_SetGrayGCRootsTracer(JSRuntime *rt, JSTraceDataOp traceOp, void *data);

extern JS_FRIEND_API(JSString *)
JS_GetAnonymousString(JSRuntime *rt);

extern JS_FRIEND_API(JSObject *)
JS_FindCompilationScope(JSContext *cx, JSObject *obj);

extern JS_FRIEND_API(JSFunction *)
JS_GetObjectFunction(JSObject *obj);

extern JS_FRIEND_API(JSObject *)
JS_GetGlobalForFrame(JSStackFrame *fp);

extern JS_FRIEND_API(JSBool)
JS_SplicePrototype(JSContext *cx, JSObject *obj, JSObject *proto);

extern JS_FRIEND_API(JSObject *)
JS_NewObjectWithUniqueType(JSContext *cx, JSClass *clasp, JSObject *proto, JSObject *parent);

extern JS_FRIEND_API(uint32)
JS_ObjectCountDynamicSlots(JSObject *obj);

extern JS_FRIEND_API(void)
JS_ShrinkingGC(JSContext *cx);

extern JS_FRIEND_API(size_t)
JS_GetE4XObjectsCreated(JSContext *cx);

extern JS_FRIEND_API(size_t)
JS_SetProtoCalled(JSContext *cx);

extern JS_FRIEND_API(size_t)
JS_GetCustomIteratorCount(JSContext *cx);

extern JS_FRIEND_API(JSBool)
JS_NondeterministicGetWeakMapKeys(JSContext *cx, JSObject *obj, JSObject **ret);

enum {
    JS_TELEMETRY_GC_REASON,
    JS_TELEMETRY_GC_IS_COMPARTMENTAL,
    JS_TELEMETRY_GC_IS_SHAPE_REGEN,
    JS_TELEMETRY_GC_MS,
    JS_TELEMETRY_GC_MARK_MS,
    JS_TELEMETRY_GC_SWEEP_MS
};

typedef void
(* JSAccumulateTelemetryDataCallback)(int id, JSUint32 sample);

extern JS_FRIEND_API(void)
JS_SetAccumulateTelemetryCallback(JSRuntime *rt, JSAccumulateTelemetryDataCallback callback);

typedef void
(* JSGCFinishedCallback)(JSRuntime *rt, JSCompartment *comp, const char *description);

extern JS_FRIEND_API(void)
JS_SetGCFinishedCallback(JSRuntime *rt, JSGCFinishedCallback callback);

/* Data for tracking analysis/inference memory usage. */
typedef struct TypeInferenceMemoryStats
{
    int64 scripts;
    int64 objects;
    int64 tables;
    int64 temporary;
} TypeInferenceMemoryStats;

extern JS_FRIEND_API(void)
JS_GetTypeInferenceMemoryStats(JSContext *cx, JSCompartment *compartment,
                               TypeInferenceMemoryStats *stats,
                               JSMallocSizeOfFun mallocSizeOf);

extern JS_FRIEND_API(void)
JS_GetTypeInferenceObjectStats(/*TypeObject*/ void *object,
                               TypeInferenceMemoryStats *stats,
                               JSMallocSizeOfFun mallocSizeOf);

extern JS_FRIEND_API(JSPrincipals *)
JS_GetCompartmentPrincipals(JSCompartment *compartment);

/* Safe to call with input obj == NULL. Returns non-NULL iff obj != NULL. */
extern JS_FRIEND_API(JSObject *)
JS_ObjectToInnerObject(JSContext *cx, JSObject *obj);

/* Requires obj != NULL. */
extern JS_FRIEND_API(JSObject *)
JS_ObjectToOuterObject(JSContext *cx, JSObject *obj);

extern JS_FRIEND_API(JSObject *)
JS_CloneObject(JSContext *cx, JSObject *obj, JSObject *proto, JSObject *parent);

extern JS_FRIEND_API(JSBool)
js_GetterOnlyPropertyStub(JSContext *cx, JSObject *obj, jsid id, JSBool strict, jsval *vp);

#ifdef __cplusplus

extern JS_FRIEND_API(bool)
JS_CopyPropertiesFrom(JSContext *cx, JSObject *target, JSObject *obj);

extern JS_FRIEND_API(JSBool)
JS_WrapPropertyDescriptor(JSContext *cx, js::PropertyDescriptor *desc);

extern JS_FRIEND_API(JSBool)
JS_EnumerateState(JSContext *cx, JSObject *obj, JSIterateOp enum_op, js::Value *statep, jsid *idp);

#endif

JS_END_EXTERN_C

#ifdef __cplusplus

namespace js {

#ifdef DEBUG
 /*
  * DEBUG-only method to dump the complete object graph of heap-allocated things.
  * fp is the file for the dump output.
  */
extern JS_FRIEND_API(void)
DumpHeapComplete(JSContext *cx, FILE *fp);

#endif

class JS_FRIEND_API(AutoPreserveCompartment) {
  private:
    JSContext *cx;
    JSCompartment *oldCompartment;
  public:
    AutoPreserveCompartment(JSContext *cx JS_GUARD_OBJECT_NOTIFIER_PARAM);
    ~AutoPreserveCompartment();
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

class JS_FRIEND_API(AutoSwitchCompartment) {
  private:
    JSContext *cx;
    JSCompartment *oldCompartment;
  public:
    AutoSwitchCompartment(JSContext *cx, JSCompartment *newCompartment
                          JS_GUARD_OBJECT_NOTIFIER_PARAM);
    AutoSwitchCompartment(JSContext *cx, JSObject *target JS_GUARD_OBJECT_NOTIFIER_PARAM);
    ~AutoSwitchCompartment();
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

#ifdef OLD_GETTER_SETTER_METHODS
JS_FRIEND_API(JSBool) obj_defineGetter(JSContext *cx, uintN argc, js::Value *vp);
JS_FRIEND_API(JSBool) obj_defineSetter(JSContext *cx, uintN argc, js::Value *vp);
#endif

extern JS_FRIEND_API(size_t)
GetObjectDynamicSlotSize(JSObject *obj, JSMallocSizeOfFun mallocSizeOf);

extern JS_FRIEND_API(size_t)
GetCompartmentShapeTableSize(JSCompartment *c, JSMallocSizeOfFun mallocSizeOf);

/*
 * Check whether it is OK to assign an undeclared property with name
 * propname of the global object in the current script on cx.  Reports
 * an error if one needs to be reported (in particular in all cases
 * when it returns false).
 */
extern JS_FRIEND_API(bool)
CheckUndeclaredVarAssignment(JSContext *cx, JSString *propname);

struct WeakMapTracer;

/*
 * Weak map tracer callback, called once for every binding of every
 * weak map that was live at the time of the last garbage collection.
 *
 * m will be NULL if the weak map is not contained in a JS Object.
 */
typedef void
(* WeakMapTraceCallback)(WeakMapTracer *trc, JSObject *m,
                         void *k, JSGCTraceKind kkind,
                         void *v, JSGCTraceKind vkind);

struct WeakMapTracer {
    JSContext            *context;
    WeakMapTraceCallback callback;

    WeakMapTracer(JSContext *cx, WeakMapTraceCallback cb) 
        : context(cx), callback(cb) {}
};

extern JS_FRIEND_API(void)
TraceWeakMaps(WeakMapTracer *trc);

/*
 * Shadow declarations of JS internal structures, for access by inline access
 * functions below. Do not use these structures in any other way. When adding
 * new fields for access by inline methods, make sure to add static asserts to
 * the original header file to ensure that offsets are consistent.
 */
namespace shadow {

struct TypeObject {
    JSObject    *proto;
};

struct BaseShape {
    js::Class   *clasp;
    JSObject    *parent;
};

struct Shape {
    BaseShape   *base;
    jsid        _1;
    uint32      slotInfo;

    static const uint32 FIXED_SLOTS_SHIFT = 27;
};

struct Object {
    Shape       *shape;
    TypeObject  *type;
    js::Value   *slots;
    js::Value   *_1;

    size_t numFixedSlots() const { return shape->slotInfo >> Shape::FIXED_SLOTS_SHIFT; }
    Value *fixedSlots() const {
        return (Value *)((jsuword) this + sizeof(shadow::Object));
    }

    js::Value &slotRef(size_t slot) const {
        size_t nfixed = numFixedSlots();
        if (slot < nfixed)
            return fixedSlots()[slot];
        return slots[slot - nfixed];
    }
};

} /* namespace shadow */

extern JS_FRIEND_DATA(js::Class) AnyNameClass;
extern JS_FRIEND_DATA(js::Class) AttributeNameClass;
extern JS_FRIEND_DATA(js::Class) CallClass;
extern JS_FRIEND_DATA(js::Class) DeclEnvClass;
extern JS_FRIEND_DATA(js::Class) FunctionClass;
extern JS_FRIEND_DATA(js::Class) FunctionProxyClass;
extern JS_FRIEND_DATA(js::Class) NamespaceClass;
extern JS_FRIEND_DATA(js::Class) OuterWindowProxyClass;
extern JS_FRIEND_DATA(js::Class) ObjectProxyClass;
extern JS_FRIEND_DATA(js::Class) QNameClass;
extern JS_FRIEND_DATA(js::Class) ScriptClass;
extern JS_FRIEND_DATA(js::Class) XMLClass;

inline js::Class *
GetObjectClass(const JSObject *obj)
{
    return reinterpret_cast<const shadow::Object*>(obj)->shape->base->clasp;
}

inline JSClass *
GetObjectJSClass(const JSObject *obj)
{
    return js::Jsvalify(GetObjectClass(obj));
}

JS_FRIEND_API(bool)
IsScopeObject(const JSObject *obj);

inline JSObject *
GetObjectParent(const JSObject *obj)
{
    JS_ASSERT(!IsScopeObject(obj));
    return reinterpret_cast<const shadow::Object*>(obj)->shape->base->parent;
}

JS_FRIEND_API(JSObject *)
GetObjectParentMaybeScope(const JSObject *obj);

JS_FRIEND_API(JSObject *)
GetGlobalForObjectCrossCompartment(JSObject *obj);

JS_FRIEND_API(bool)
IsOriginalScriptFunction(JSFunction *fun);

JS_FRIEND_API(JSFunction *)
DefineFunctionWithReserved(JSContext *cx, JSObject *obj, const char *name, JSNative call,
                           uintN nargs, uintN attrs);

JS_FRIEND_API(JSFunction *)
NewFunctionWithReserved(JSContext *cx, JSNative call, uintN nargs, uintN flags,
                        JSObject *parent, const char *name);

JS_FRIEND_API(JSFunction *)
NewFunctionByIdWithReserved(JSContext *cx, JSNative native, uintN nargs, uintN flags,
                            JSObject *parent, jsid id);

JS_FRIEND_API(JSObject *)
InitClassWithReserved(JSContext *cx, JSObject *obj, JSObject *parent_proto,
                      JSClass *clasp, JSNative constructor, uintN nargs,
                      JSPropertySpec *ps, JSFunctionSpec *fs,
                      JSPropertySpec *static_ps, JSFunctionSpec *static_fs);

JS_FRIEND_API(const Value &)
GetFunctionNativeReserved(JSObject *fun, size_t which);

JS_FRIEND_API(void)
SetFunctionNativeReserved(JSObject *fun, size_t which, const Value &val);

inline JSObject *
GetObjectProto(const JSObject *obj)
{
    return reinterpret_cast<const shadow::Object*>(obj)->type->proto;
}

inline void *
GetObjectPrivate(const JSObject *obj)
{
    const shadow::Object *nobj = reinterpret_cast<const shadow::Object*>(obj);
    void **addr = reinterpret_cast<void**>(&nobj->fixedSlots()[nobj->numFixedSlots()]);
    return *addr;
}

/*
 * Get a slot that is both reserved for object's clasp *and* is fixed (fits
 * within the maximum capacity for the object's fixed slots).
 */
inline const Value &
GetReservedSlot(const JSObject *obj, size_t slot)
{
    JS_ASSERT(slot < JSCLASS_RESERVED_SLOTS(GetObjectClass(obj)));
    return reinterpret_cast<const shadow::Object *>(obj)->slotRef(slot);
}

inline void
SetReservedSlot(JSObject *obj, size_t slot, const Value &value)
{
    JS_ASSERT(slot < JSCLASS_RESERVED_SLOTS(GetObjectClass(obj)));
    reinterpret_cast<shadow::Object *>(obj)->slotRef(slot) = value;
}

JS_FRIEND_API(uint32)
GetObjectSlotSpan(const JSObject *obj);

inline const Value &
GetObjectSlot(const JSObject *obj, size_t slot)
{
    JS_ASSERT(slot < GetObjectSlotSpan(obj));
    return reinterpret_cast<const shadow::Object *>(obj)->slotRef(slot);
}

inline Shape *
GetObjectShape(const JSObject *obj)
{
    shadow::Shape *shape = reinterpret_cast<const shadow::Object*>(obj)->shape;
    return reinterpret_cast<Shape *>(shape);
}

static inline js::PropertyOp
CastAsJSPropertyOp(JSObject *object)
{
    return JS_DATA_TO_FUNC_PTR(js::PropertyOp, object);
}

static inline js::StrictPropertyOp
CastAsJSStrictPropertyOp(JSObject *object)
{
    return JS_DATA_TO_FUNC_PTR(js::StrictPropertyOp, object);
}

JS_FRIEND_API(bool)
GetPropertyNames(JSContext *cx, JSObject *obj, uintN flags, js::AutoIdVector *props);

JS_FRIEND_API(bool)
StringIsArrayIndex(JSLinearString *str, jsuint *indexp);

/*
 * NB: these flag bits are encoded into the bytecode stream in the immediate
 * operand of JSOP_ITER, so don't change them without advancing jsxdrapi.h's
 * JSXDR_BYTECODE_VERSION.
 */
#define JSITER_ENUMERATE  0x1   /* for-in compatible hidden default iterator */
#define JSITER_FOREACH    0x2   /* return [key, value] pair rather than key */
#define JSITER_KEYVALUE   0x4   /* destructuring for-in wants [key, value] */
#define JSITER_OWNONLY    0x8   /* iterate over obj's own properties only */
#define JSITER_HIDDEN     0x10  /* also enumerate non-enumerable properties */

} /* namespace js */
#endif

#endif /* jsfriendapi_h___ */
