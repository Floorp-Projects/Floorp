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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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

#ifndef jsobj_h___
#define jsobj_h___
/*
 * JS object definitions.
 *
 * A JS object consists of a possibly-shared object descriptor containing
 * ordered property names, called the map; and a dense vector of property
 * values, called slots.  The map/slot pointer pair is GC'ed, while the map
 * is reference counted and the slot vector is malloc'ed.
 */
#include "jshash.h" /* Added by JSIFY */
#include "jspubtd.h"
#include "jsprvtd.h"

namespace js { class AutoDescriptorArray; }

/*
 * A representation of ECMA-262 ed. 5's internal property descriptor data
 * structure.
 */
struct PropertyDescriptor {
  friend class js::AutoDescriptorArray;

  private:
    PropertyDescriptor();

  public:
    /* 8.10.5 ToPropertyDescriptor(Obj) */
    bool initialize(JSContext* cx, jsid id, jsval v);

    /* 8.10.1 IsAccessorDescriptor(desc) */
    bool isAccessorDescriptor() const {
        return hasGet || hasSet;
    }

    /* 8.10.2 IsDataDescriptor(desc) */
    bool isDataDescriptor() const {
        return hasValue || hasWritable;
    }

    /* 8.10.3 IsGenericDescriptor(desc) */
    bool isGenericDescriptor() const {
        return !isAccessorDescriptor() && !isDataDescriptor();
    }

    bool configurable() const {
        return (attrs & JSPROP_PERMANENT) == 0;
    }

    bool enumerable() const {
        return (attrs & JSPROP_ENUMERATE) != 0;
    }

    bool writable() const {
        return (attrs & JSPROP_READONLY) == 0;
    }

    JSObject* getterObject() const {
        return get != JSVAL_VOID ? JSVAL_TO_OBJECT(get) : NULL;
    }
    JSObject* setterObject() const {
        return set != JSVAL_VOID ? JSVAL_TO_OBJECT(set) : NULL;
    }

    jsval getterValue() const {
        return get;
    }
    jsval setterValue() const {
        return set;
    }

    JSPropertyOp getter() const {
        return js_CastAsPropertyOp(getterObject());
    }
    JSPropertyOp setter() const {
        return js_CastAsPropertyOp(setterObject());
    }

    static void traceDescriptorArray(JSTracer* trc, JSObject* obj);
    static void finalizeDescriptorArray(JSContext* cx, JSObject* obj);

    jsid id;
    jsval value, get, set;

    /* Property descriptor boolean fields. */
    uint8 attrs;

    /* Bits indicating which values are set. */
    bool hasGet : 1;
    bool hasSet : 1;
    bool hasValue : 1;
    bool hasWritable : 1;
    bool hasEnumerable : 1;
    bool hasConfigurable : 1;
};

JS_BEGIN_EXTERN_C

/* For detailed comments on these function pointer types, see jsprvtd.h. */
struct JSObjectOps {
    /*
     * Custom shared object map for non-native objects. For native objects
     * this should be null indicating, that JSObject.map is an instance of
     * JSScope.
     */
    const JSObjectMap   *objectMap;

    /* Mandatory non-null function pointer members. */
    JSLookupPropOp      lookupProperty;
    JSDefinePropOp      defineProperty;
    JSPropertyIdOp      getProperty;
    JSPropertyIdOp      setProperty;
    JSAttributesOp      getAttributes;
    JSAttributesOp      setAttributes;
    JSPropertyIdOp      deleteProperty;
    JSConvertOp         defaultValue;
    JSNewEnumerateOp    enumerate;
    JSCheckAccessIdOp   checkAccess;
    JSTypeOfOp          typeOf;
    JSTraceOp           trace;

    /* Optionally non-null members start here. */
    JSObjectOp          thisObject;
    JSPropertyRefOp     dropProperty;
    JSNative            call;
    JSNative            construct;
    JSHasInstanceOp     hasInstance;
    JSFinalizeOp        clear;

    bool inline isNative() const;
};

extern JS_FRIEND_DATA(JSObjectOps) js_ObjectOps;
extern JS_FRIEND_DATA(JSObjectOps) js_WithObjectOps;

/*
 * Test whether the ops is native. FIXME bug 492938: consider how it would
 * affect the performance to do just the !objectMap check.
 */
inline bool
JSObjectOps::isNative() const
{
    return JS_LIKELY(this == &js_ObjectOps) || !objectMap;
}

struct JSObjectMap {
    const JSObjectOps * const   ops;    /* high level object operation vtable */
    uint32                      shape;  /* shape identifier */

    explicit JSObjectMap(const JSObjectOps *ops, uint32 shape) : ops(ops), shape(shape) {}

    enum { SHAPELESS = 0xffffffff };

private:
    /* No copy or assignment semantics. */
    JSObjectMap(JSObjectMap &);
    void operator=(JSObjectMap &);
};

const uint32 JS_INITIAL_NSLOTS = 5;

const uint32 JSSLOT_PROTO   = 0;
const uint32 JSSLOT_PARENT  = 1;

/*
 * The first available slot to store generic value. For JSCLASS_HAS_PRIVATE
 * classes the slot stores a pointer to private data reinterpreted as jsval.
 * Such pointer is stored as is without an overhead of PRIVATE_TO_JSVAL
 * tagging and should be accessed using the (get|set)Private methods of
 * JSObject.
 */
const uint32 JSSLOT_PRIVATE = 2;

const uint32 JSSLOT_PRIMITIVE_THIS = JSSLOT_PRIVATE;

const uintptr_t JSSLOT_CLASS_MASK_BITS = 3;

/*
 * JSObject struct, with members sized to fit in 32 bytes on 32-bit targets,
 * 64 bytes on 64-bit systems. The JSFunction struct is an extension of this
 * struct allocated from a larger GC size-class.
 *
 * The classword member stores the JSClass pointer for this object, with the
 * least two bits encoding whether this object is a "delegate" or a "system"
 * object. We do *not* synchronize updates of classword -- API clients must
 * take care.
 *
 * An object is a delegate if it is on another object's prototype (linked by
 * JSSLOT_PROTO) or scope (JSSLOT_PARENT) chain, and therefore the delegate
 * might be asked implicitly to get or set a property on behalf of another
 * object. Delegates may be accessed directly too, as may any object, but only
 * those objects linked after the head of any prototype or scope chain are
 * flagged as delegates. This definition helps to optimize shape-based property
 * cache invalidation (see Purge{Scope,Proto}Chain in jsobj.cpp).
 *
 * The meaning of the system object bit is defined by the API client. It is
 * set in JS_NewSystemObject and is queried by JS_IsSystemObject (jsdbgapi.h),
 * but it has no intrinsic meaning to SpiderMonkey. Further, JSFILENAME_SYSTEM
 * and JS_FlagScriptFilenamePrefix (also exported via jsdbgapi.h) are intended
 * to be complementary to this bit, but it is up to the API client to implement
 * any such association.
 *
 * Both these classword tag bits are initially zero; they may be set or queried
 * using the (is|set)(Delegate|System) inline methods.
 *
 * The dslots member is null or a pointer into a dynamically allocated vector
 * of jsvals for reserved and dynamic slots. If dslots is not null, dslots[-1]
 * records the number of available slots.
 */
struct JSObject {
    JSObjectMap *map;                       /* property map, see jsscope.h */
    jsuword     classword;                  /* JSClass ptr | bits, see above */
    jsval       fslots[JS_INITIAL_NSLOTS];  /* small number of fixed slots */
    jsval       *dslots;                    /* dynamically allocated slots */

    bool isNative() const { return map->ops->isNative(); }

    JSClass *getClass() const {
        return (JSClass *) (classword & ~JSSLOT_CLASS_MASK_BITS);
    }

    bool hasClass(const JSClass *clasp) const {
        return clasp == getClass();
    }

    bool isDelegate() const {
        return (classword & jsuword(1)) != jsuword(0);
    }

    void setDelegate() {
        classword |= jsuword(1);
    }

    static void setDelegateNullSafe(JSObject *obj) {
        if (obj)
            obj->setDelegate();
    }

    bool isSystem() const {
        return (classword & jsuword(2)) != jsuword(0);
    }

    void setSystem() {
        classword |= jsuword(2);
    }

    uint32 numSlots(void) {
        return dslots ? (uint32)dslots[-1] : (uint32)JS_INITIAL_NSLOTS;
    }

    jsval& getSlotRef(uintN slot) {
        return (slot < JS_INITIAL_NSLOTS)
               ? fslots[slot]
               : (JS_ASSERT(slot < (uint32)dslots[-1]),
                  dslots[slot - JS_INITIAL_NSLOTS]);
    }

    jsval getSlot(uintN slot) const {
        return (slot < JS_INITIAL_NSLOTS)
               ? fslots[slot]
               : (JS_ASSERT(slot < (uint32)dslots[-1]),
                  dslots[slot - JS_INITIAL_NSLOTS]);
    }

    void setSlot(uintN slot, jsval value) {
        if (slot < JS_INITIAL_NSLOTS) {
            fslots[slot] = value;
        } else {
            JS_ASSERT(slot < (uint32)dslots[-1]);
            dslots[slot - JS_INITIAL_NSLOTS] = value;
        }
    }

    /*
     * These ones are for multi-threaded ("MT") objects.  Use getSlot(),
     * getSlotRef(), setSlot() to directly manipulate slots in obj when only
     * one thread can access obj, or when accessing read-only slots within
     * JS_INITIAL_NSLOTS.
     */
    inline jsval getSlotMT(JSContext *cx, uintN slot);
    inline void setSlotMT(JSContext *cx, uintN slot, jsval value);

    JSObject *getProto() const {
        return JSVAL_TO_OBJECT(fslots[JSSLOT_PROTO]);
    }

    void clearProto() {
        fslots[JSSLOT_PROTO] = JSVAL_NULL;
    }

    void setProto(JSObject *newProto) {
        setDelegateNullSafe(newProto);
        fslots[JSSLOT_PROTO] = OBJECT_TO_JSVAL(newProto);
    }

    JSObject *getParent() const {
        return JSVAL_TO_OBJECT(fslots[JSSLOT_PARENT]);
    }

    void clearParent() {
        fslots[JSSLOT_PARENT] = JSVAL_NULL;
    }

    void setParent(JSObject *newParent) {
        setDelegateNullSafe(newParent);
        fslots[JSSLOT_PARENT] = OBJECT_TO_JSVAL(newParent);
    }

    void traceProtoAndParent(JSTracer *trc) const {
        JSObject *proto = getProto();
        if (proto)
            JS_CALL_OBJECT_TRACER(trc, proto, "__proto__");

        JSObject *parent = getParent();
        if (parent)
            JS_CALL_OBJECT_TRACER(trc, parent, "__parent__");
    }

    JSObject *getGlobal();

    void *getPrivate() const {
        JS_ASSERT(getClass()->flags & JSCLASS_HAS_PRIVATE);
        jsval v = fslots[JSSLOT_PRIVATE];
        JS_ASSERT((v & jsval(1)) == jsval(0));
        return reinterpret_cast<void *>(v);
    }

    void setPrivate(void *data) {
        JS_ASSERT(getClass()->flags & JSCLASS_HAS_PRIVATE);
        jsval v = reinterpret_cast<jsval>(data);
        JS_ASSERT((v & jsval(1)) == jsval(0));
        fslots[JSSLOT_PRIVATE] = v;
    }

    static jsval defaultPrivate(JSClass *clasp) {
        return (clasp->flags & JSCLASS_HAS_PRIVATE)
               ? JSVAL_NULL
               : JSVAL_VOID;
    }

    bool isCallable();

    /* The map field is not initialized here and should be set separately. */
    void init(JSClass *clasp, JSObject *proto, JSObject *parent,
              jsval privateSlotValue) {
        JS_ASSERT(((jsuword) clasp & 3) == 0);
        JS_STATIC_ASSERT(JSSLOT_PRIVATE + 3 == JS_INITIAL_NSLOTS);
        JS_ASSERT_IF(clasp->flags & JSCLASS_HAS_PRIVATE,
                     (privateSlotValue & jsval(1)) == jsval(0));

        classword = jsuword(clasp);
        JS_ASSERT(!isDelegate());
        JS_ASSERT(!isSystem());

        setProto(proto);
        setParent(parent);
        fslots[JSSLOT_PRIVATE] = privateSlotValue;
        fslots[JSSLOT_PRIVATE + 1] = JSVAL_VOID;
        fslots[JSSLOT_PRIVATE + 2] = JSVAL_VOID;
        dslots = NULL;
    }

    /*
     * Like init, but also initializes map. The catch: proto must be the result
     * of a call to js_InitClass(...clasp, ...).
     */
    inline void initSharingEmptyScope(JSClass *clasp, JSObject *proto, JSObject *parent,
                                      jsval privateSlotValue);

    inline bool hasSlotsArray() const { return !!dslots; }

    /* This method can only be called when hasSlotsArray() returns true. */
    inline void freeSlotsArray(JSContext *cx);

    JSBool lookupProperty(JSContext *cx, jsid id,
                          JSObject **objp, JSProperty **propp) {
        return map->ops->lookupProperty(cx, this, id, objp, propp);
    }

    JSBool defineProperty(JSContext *cx, jsid id, jsval value,
                          JSPropertyOp getter = JS_PropertyStub,
                          JSPropertyOp setter = JS_PropertyStub,
                          uintN attrs = JSPROP_ENUMERATE) {
        return map->ops->defineProperty(cx, this, id, value, getter, setter, attrs);
    }

    JSBool getProperty(JSContext *cx, jsid id, jsval *vp) {
        return map->ops->getProperty(cx, this, id, vp);
    }

    JSBool setProperty(JSContext *cx, jsid id, jsval *vp) {
        return map->ops->setProperty(cx, this, id, vp);
    }

    JSBool getAttributes(JSContext *cx, jsid id, JSProperty *prop,
                         uintN *attrsp) {
        return map->ops->getAttributes(cx, this, id, prop, attrsp);
    }

    JSBool setAttributes(JSContext *cx, jsid id, JSProperty *prop,
                         uintN *attrsp) {
        return map->ops->setAttributes(cx, this, id, prop, attrsp);
    }

    JSBool deleteProperty(JSContext *cx, jsid id, jsval *rval) {
        return map->ops->deleteProperty(cx, this, id, rval);
    }

    JSBool defaultValue(JSContext *cx, JSType hint, jsval *vp) {
        return map->ops->defaultValue(cx, this, hint, vp);
    }

    JSBool enumerate(JSContext *cx, JSIterateOp op, jsval *statep,
                     jsid *idp) {
        return map->ops->enumerate(cx, this, op, statep, idp);
    }

    JSBool checkAccess(JSContext *cx, jsid id, JSAccessMode mode, jsval *vp,
                       uintN *attrsp) {
        return map->ops->checkAccess(cx, this, id, mode, vp, attrsp);
    }

    JSType typeOf(JSContext *cx) {
        return map->ops->typeOf(cx, this);
    }

    /* These four are time-optimized to avoid stub calls. */
    JSObject *thisObject(JSContext *cx) {
        return map->ops->thisObject ? map->ops->thisObject(cx, this) : this;
    }

    void dropProperty(JSContext *cx, JSProperty *prop) {
        if (map->ops->dropProperty)
            map->ops->dropProperty(cx, this, prop);
    }

    inline bool isArguments() const;
    inline bool isArray() const;
    inline bool isDenseArray() const;
    inline bool isFunction() const;
    inline bool isRegExp() const;
    inline bool isXML() const;

    inline bool unbrand(JSContext *cx);
};

#define JSSLOT_START(clasp) (((clasp)->flags & JSCLASS_HAS_PRIVATE)           \
                             ? JSSLOT_PRIVATE + 1                             \
                             : JSSLOT_PRIVATE)

#define JSSLOT_FREE(clasp)  (JSSLOT_START(clasp)                              \
                             + JSCLASS_RESERVED_SLOTS(clasp))

/*
 * Maximum capacity of the obj->dslots vector, net of the hidden slot at
 * obj->dslots[-1] that is used to store the length of the vector biased by
 * JS_INITIAL_NSLOTS (and again net of the slot at index -1).
 */
#define MAX_DSLOTS_LENGTH   (JS_MAX(~uint32(0), ~size_t(0)) / sizeof(jsval) - 1)
#define MAX_DSLOTS_LENGTH32 (~uint32(0) / sizeof(jsval) - 1)

#define OBJ_CHECK_SLOT(obj,slot)                                              \
    (JS_ASSERT(obj->isNative()), JS_ASSERT(slot < OBJ_SCOPE(obj)->freeslot))

#define LOCKED_OBJ_GET_SLOT(obj,slot)                                         \
    (OBJ_CHECK_SLOT(obj, slot), obj->getSlot(slot))
#define LOCKED_OBJ_SET_SLOT(obj,slot,value)                                   \
    (OBJ_CHECK_SLOT(obj, slot), obj->setSlot(slot, value))

#ifdef JS_THREADSAFE

/*
 * The GC runs only when all threads except the one on which the GC is active
 * are suspended at GC-safe points, so calling obj->getSlot() from the GC's
 * thread is safe when rt->gcRunning is set. See jsgc.c for details.
 */
#define THREAD_IS_RUNNING_GC(rt, thread)                                      \
    ((rt)->gcRunning && (rt)->gcThread == (thread))

#define CX_THREAD_IS_RUNNING_GC(cx)                                           \
    THREAD_IS_RUNNING_GC((cx)->runtime, (cx)->thread)

#endif /* JS_THREADSAFE */

/*
 * Class is invariant and comes from the fixed clasp member. Thus no locking
 * is necessary to read it. Same for the private slot.
 */
#define OBJ_GET_CLASS(cx,obj)           (obj)->getClass()

#ifdef __cplusplus
inline void
OBJ_TO_INNER_OBJECT(JSContext *cx, JSObject *&obj)
{
    JSClass *clasp = OBJ_GET_CLASS(cx, obj);
    if (clasp->flags & JSCLASS_IS_EXTENDED) {
        JSExtendedClass *xclasp = (JSExtendedClass *) clasp;
        if (xclasp->innerObject)
            obj = xclasp->innerObject(cx, obj);
    }
}

/*
 * The following function has been copied to jsd/jsd_val.c. If making changes to
 * OBJ_TO_OUTER_OBJECT, please update jsd/jsd_val.c as well.
 */
inline void
OBJ_TO_OUTER_OBJECT(JSContext *cx, JSObject *&obj)
{
    JSClass *clasp = OBJ_GET_CLASS(cx, obj);
    if (clasp->flags & JSCLASS_IS_EXTENDED) {
        JSExtendedClass *xclasp = (JSExtendedClass *) clasp;
        if (xclasp->outerObject)
            obj = xclasp->outerObject(cx, obj);
    }
}
#endif

extern JSClass  js_ObjectClass;
extern JSClass  js_WithClass;
extern JSClass  js_BlockClass;

/*
 * Block scope object macros.  The slots reserved by js_BlockClass are:
 *
 *   JSSLOT_PRIVATE       JSStackFrame *    active frame pointer or null
 *   JSSLOT_BLOCK_DEPTH   int               depth of block slots in frame
 *
 * After JSSLOT_BLOCK_DEPTH come one or more slots for the block locals.
 *
 * A With object is like a Block object, in that both have one reserved slot
 * telling the stack depth of the relevant slots (the slot whose value is the
 * object named in the with statement, the slots containing the block's local
 * variables); and both have a private slot referring to the JSStackFrame in
 * whose activation they were created (or null if the with or block object
 * outlives the frame).
 */
#define JSSLOT_BLOCK_DEPTH      (JSSLOT_PRIVATE + 1)

static inline bool
OBJ_IS_CLONED_BLOCK(JSObject *obj)
{
    return obj->getProto() != NULL;
}

extern JSBool
js_DefineBlockVariable(JSContext *cx, JSObject *obj, jsid id, intN index);

#define OBJ_BLOCK_COUNT(cx,obj)                                               \
    (OBJ_SCOPE(OBJ_IS_CLONED_BLOCK(obj) ? obj->getProto() : obj)->entryCount)
#define OBJ_BLOCK_DEPTH(cx,obj)                                               \
    JSVAL_TO_INT(obj->getSlot(JSSLOT_BLOCK_DEPTH))
#define OBJ_SET_BLOCK_DEPTH(cx,obj,depth)                                     \
    obj->setSlot(JSSLOT_BLOCK_DEPTH, INT_TO_JSVAL(depth))

/*
 * To make sure this slot is well-defined, always call js_NewWithObject to
 * create a With object, don't call js_NewObject directly.  When creating a
 * With object that does not correspond to a stack slot, pass -1 for depth.
 *
 * When popping the stack across this object's "with" statement, client code
 * must call withobj->setPrivate(NULL).
 */
extern JS_REQUIRES_STACK JSObject *
js_NewWithObject(JSContext *cx, JSObject *proto, JSObject *parent, jsint depth);

/*
 * Create a new block scope object not linked to any proto or parent object.
 * Blocks are created by the compiler to reify let blocks and comprehensions.
 * Only when dynamic scope is captured do they need to be cloned and spliced
 * into an active scope chain.
 */
extern JSObject *
js_NewBlockObject(JSContext *cx);

extern JSObject *
js_CloneBlockObject(JSContext *cx, JSObject *proto, JSStackFrame *fp);

extern JS_REQUIRES_STACK JSBool
js_PutBlockObject(JSContext *cx, JSBool normalUnwind);

JSBool
js_XDRBlockObject(JSXDRState *xdr, JSObject **objp);

struct JSSharpObjectMap {
    jsrefcount  depth;
    jsatomid    sharpgen;
    JSHashTable *table;
};

#define SHARP_BIT       ((jsatomid) 1)
#define BUSY_BIT        ((jsatomid) 2)
#define SHARP_ID_SHIFT  2
#define IS_SHARP(he)    (uintptr_t((he)->value) & SHARP_BIT)
#define MAKE_SHARP(he)  ((he)->value = (void *) (uintptr_t((he)->value)|SHARP_BIT))
#define IS_BUSY(he)     (uintptr_t((he)->value) & BUSY_BIT)
#define MAKE_BUSY(he)   ((he)->value = (void *) (uintptr_t((he)->value)|BUSY_BIT))
#define CLEAR_BUSY(he)  ((he)->value = (void *) (uintptr_t((he)->value)&~BUSY_BIT))

extern JSHashEntry *
js_EnterSharpObject(JSContext *cx, JSObject *obj, JSIdArray **idap,
                    jschar **sp);

extern void
js_LeaveSharpObject(JSContext *cx, JSIdArray **idap);

/*
 * Mark objects stored in map if GC happens between js_EnterSharpObject
 * and js_LeaveSharpObject. GC calls this when map->depth > 0.
 */
extern void
js_TraceSharpMap(JSTracer *trc, JSSharpObjectMap *map);

extern JSBool
js_HasOwnPropertyHelper(JSContext *cx, JSLookupPropOp lookup, uintN argc,
                        jsval *vp);

extern JSBool
js_HasOwnProperty(JSContext *cx, JSLookupPropOp lookup, JSObject *obj, jsid id,
                  JSObject **objp, JSProperty **propp);

extern JSBool
js_PropertyIsEnumerable(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JSObject *
js_InitEval(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitObjectClass(JSContext *cx, JSObject *obj);

extern JSObject *
js_InitClass(JSContext *cx, JSObject *obj, JSObject *parent_proto,
             JSClass *clasp, JSNative constructor, uintN nargs,
             JSPropertySpec *ps, JSFunctionSpec *fs,
             JSPropertySpec *static_ps, JSFunctionSpec *static_fs);

/*
 * Select Object.prototype method names shared between jsapi.cpp and jsobj.cpp.
 */
extern const char js_watch_str[];
extern const char js_unwatch_str[];
extern const char js_hasOwnProperty_str[];
extern const char js_isPrototypeOf_str[];
extern const char js_propertyIsEnumerable_str[];
extern const char js_defineGetter_str[];
extern const char js_defineSetter_str[];
extern const char js_lookupGetter_str[];
extern const char js_lookupSetter_str[];

extern JSObject *
js_NewObject(JSContext *cx, JSClass *clasp, JSObject *proto,
             JSObject *parent, size_t objectSize = 0);

/*
 * See jsapi.h, JS_NewObjectWithGivenProto.
 */
extern JSObject *
js_NewObjectWithGivenProto(JSContext *cx, JSClass *clasp, JSObject *proto,
                           JSObject *parent, size_t objectSize = 0);

/*
 * Allocate a new native object with the given value of the proto and private
 * slots. The parent slot is set to the value of proto's parent slot.
 *
 * clasp must be a native class. proto must be the result of a call to
 * js_InitClass(...clasp, ...).
 *
 * Note that this is the correct global object for native class instances, but
 * not for user-defined functions called as constructors.  Functions used as
 * constructors must create instances parented by the parent of the function
 * object, not by the parent of its .prototype object value.
 */
extern JSObject*
js_NewObjectWithClassProto(JSContext *cx, JSClass *clasp, JSObject *proto,
                           jsval privateSlotValue);

/*
 * Fast access to immutable standard objects (constructors and prototypes).
 */
extern JSBool
js_GetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key,
                  JSObject **objp);

extern JSBool
js_SetClassObject(JSContext *cx, JSObject *obj, JSProtoKey key, JSObject *cobj);

/*
 * If protoKey is not JSProto_Null, then clasp is ignored. If protoKey is
 * JSProto_Null, clasp must non-null.
 */
extern JSBool
js_FindClassObject(JSContext *cx, JSObject *start, JSProtoKey key, jsval *vp,
                   JSClass *clasp = NULL);

extern JSObject *
js_ConstructObject(JSContext *cx, JSClass *clasp, JSObject *proto,
                   JSObject *parent, uintN argc, jsval *argv);

extern JSBool
js_AllocSlot(JSContext *cx, JSObject *obj, uint32 *slotp);

extern void
js_FreeSlot(JSContext *cx, JSObject *obj, uint32 slot);

extern bool
js_GrowSlots(JSContext *cx, JSObject *obj, size_t nslots);

extern void
js_ShrinkSlots(JSContext *cx, JSObject *obj, size_t nslots);

/*
 * Ensure that the object has at least JSCLASS_RESERVED_SLOTS(clasp)+nreserved
 * slots. The function can be called only for native objects just created with
 * js_NewObject or its forms. In particular, the object should not be shared
 * between threads and its dslots array must be null. nreserved must match the
 * value that JSClass.reserveSlots (if any) would return after the object is
 * fully initialized.
 */
bool
js_EnsureReservedSlots(JSContext *cx, JSObject *obj, size_t nreserved);

extern jsid
js_CheckForStringIndex(jsid id);

/*
 * js_PurgeScopeChain does nothing if obj is not itself a prototype or parent
 * scope, else it reshapes the scope and prototype chains it links. It calls
 * js_PurgeScopeChainHelper, which asserts that obj is flagged as a delegate
 * (i.e., obj has ever been on a prototype or parent chain).
 */
extern void
js_PurgeScopeChainHelper(JSContext *cx, JSObject *obj, jsid id);

#ifdef __cplusplus /* Aargh, libgjs, bug 492720. */
static JS_INLINE void
js_PurgeScopeChain(JSContext *cx, JSObject *obj, jsid id)
{
    if (obj->isDelegate())
        js_PurgeScopeChainHelper(cx, obj, id);
}
#endif

/*
 * Find or create a property named by id in obj's scope, with the given getter
 * and setter, slot, attributes, and other members.
 */
extern JSScopeProperty *
js_AddNativeProperty(JSContext *cx, JSObject *obj, jsid id,
                     JSPropertyOp getter, JSPropertyOp setter, uint32 slot,
                     uintN attrs, uintN flags, intN shortid);

/*
 * Change sprop to have the given attrs, getter, and setter in scope, morphing
 * it into a potentially new JSScopeProperty.  Return a pointer to the changed
 * or identical property.
 */
extern JSScopeProperty *
js_ChangeNativePropertyAttrs(JSContext *cx, JSObject *obj,
                             JSScopeProperty *sprop, uintN attrs, uintN mask,
                             JSPropertyOp getter, JSPropertyOp setter);

extern JSBool
js_DefineProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                  JSPropertyOp getter, JSPropertyOp setter, uintN attrs);

extern JSBool
js_DefineOwnProperty(JSContext *cx, JSObject *obj, jsid id, jsval descriptor, JSBool *bp);

/*
 * Flags for the defineHow parameter of js_DefineNativeProperty.
 */
const uintN JSDNP_CACHE_RESULT = 1; /* an interpreter call from JSOP_INITPROP */
const uintN JSDNP_DONT_PURGE   = 2; /* suppress js_PurgeScopeChain */
const uintN JSDNP_SET_METHOD   = 4; /* js_{DefineNativeProperty,SetPropertyHelper}
                                       must pass the JSScopeProperty::METHOD
                                       flag on to js_AddScopeProperty */

/*
 * On error, return false.  On success, if propp is non-null, return true with
 * obj locked and with a held property in *propp; if propp is null, return true
 * but release obj's lock first.  Therefore all callers who pass non-null propp
 * result parameters must later call obj->dropProperty(cx, *propp) both to drop
 * the held property, and to release the lock on obj.
 */
extern JSBool
js_DefineNativeProperty(JSContext *cx, JSObject *obj, jsid id, jsval value,
                        JSPropertyOp getter, JSPropertyOp setter, uintN attrs,
                        uintN flags, intN shortid, JSProperty **propp,
                        uintN defineHow = 0);

/*
 * Unlike js_DefineNativeProperty, propp must be non-null. On success, and if
 * id was found, return true with *objp non-null and locked, and with a held
 * property stored in *propp. If successful but id was not found, return true
 * with both *objp and *propp null. Therefore all callers who receive a
 * non-null *propp must later call (*objp)->dropProperty(cx, *propp).
 */
extern JS_FRIEND_API(JSBool)
js_LookupProperty(JSContext *cx, JSObject *obj, jsid id, JSObject **objp,
                  JSProperty **propp);

/*
 * Specialized subroutine that allows caller to preset JSRESOLVE_* flags and
 * returns the index along the prototype chain in which *propp was found, or
 * the last index if not found, or -1 on error.
 */
extern int
js_LookupPropertyWithFlags(JSContext *cx, JSObject *obj, jsid id, uintN flags,
                           JSObject **objp, JSProperty **propp);


/*
 * We cache name lookup results only for the global object or for native
 * non-global objects without prototype or with prototype that never mutates,
 * see bug 462734 and bug 487039.
 */
static inline bool
js_IsCacheableNonGlobalScope(JSObject *obj)
{
    extern JS_FRIEND_DATA(JSClass) js_CallClass;
    extern JS_FRIEND_DATA(JSClass) js_DeclEnvClass;
    JS_ASSERT(obj->getParent());

    JSClass *clasp = obj->getClass();
    bool cacheable = (clasp == &js_CallClass ||
                      clasp == &js_BlockClass ||
                      clasp == &js_DeclEnvClass);

    JS_ASSERT_IF(cacheable, obj->map->ops->lookupProperty == js_LookupProperty);
    return cacheable;
}

/*
 * If cacheResult is false, return JS_NO_PROP_CACHE_FILL on success.
 */
extern js::PropertyCacheEntry *
js_FindPropertyHelper(JSContext *cx, jsid id, JSBool cacheResult,
                      JSObject **objp, JSObject **pobjp, JSProperty **propp);

/*
 * Return the index along the scope chain in which id was found, or the last
 * index if not found, or -1 on error.
 */
extern JS_FRIEND_API(JSBool)
js_FindProperty(JSContext *cx, jsid id, JSObject **objp, JSObject **pobjp,
                JSProperty **propp);

extern JS_REQUIRES_STACK JSObject *
js_FindIdentifierBase(JSContext *cx, JSObject *scopeChain, jsid id);

extern JSObject *
js_FindVariableScope(JSContext *cx, JSFunction **funp);

/*
 * JSGET_CACHE_RESULT is the analogue of JSDNP_CACHE_RESULT for js_GetMethod.
 *
 * JSGET_METHOD_BARRIER (the default, hence 0 but provided for documentation)
 * enables a read barrier that preserves standard function object semantics (by
 * default we assume our caller won't leak a joined callee to script, where it
 * would create hazardous mutable object sharing as well as observable identity
 * according to == and ===.
 *
 * JSGET_NO_METHOD_BARRIER avoids the performance overhead of the method read
 * barrier, which is not needed when invoking a lambda that otherwise does not
 * leak its callee reference (via arguments.callee or its name).
 */
const uintN JSGET_CACHE_RESULT      = 1; // from a caching interpreter opcode
const uintN JSGET_METHOD_BARRIER    = 0; // get can leak joined function object
const uintN JSGET_NO_METHOD_BARRIER = 2; // call to joined function can't leak

/*
 * NB: js_NativeGet and js_NativeSet are called with the scope containing sprop
 * (pobj's scope for Get, obj's for Set) locked, and on successful return, that
 * scope is again locked.  But on failure, both functions return false with the
 * scope containing sprop unlocked.
 */
extern JSBool
js_NativeGet(JSContext *cx, JSObject *obj, JSObject *pobj,
             JSScopeProperty *sprop, uintN getHow, jsval *vp);

extern JSBool
js_NativeSet(JSContext *cx, JSObject *obj, JSScopeProperty *sprop, bool added,
             jsval *vp);

extern JSBool
js_GetPropertyHelper(JSContext *cx, JSObject *obj, jsid id, uintN getHow,
                     jsval *vp);

extern JSBool
js_GetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JSBool
js_GetOwnPropertyDescriptor(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JSBool
js_GetMethod(JSContext *cx, JSObject *obj, jsid id, uintN getHow, jsval *vp);

/*
 * Check whether it is OK to assign an undeclared property of the global
 * object at the current script PC.
 */
extern JS_FRIEND_API(bool)
js_CheckUndeclaredVarAssignment(JSContext *cx);

extern JSBool
js_SetPropertyHelper(JSContext *cx, JSObject *obj, jsid id, uintN defineHow,
                     jsval *vp);

extern JSBool
js_SetProperty(JSContext *cx, JSObject *obj, jsid id, jsval *vp);

extern JSBool
js_GetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                 uintN *attrsp);

extern JSBool
js_SetAttributes(JSContext *cx, JSObject *obj, jsid id, JSProperty *prop,
                 uintN *attrsp);

extern JSBool
js_DeleteProperty(JSContext *cx, JSObject *obj, jsid id, jsval *rval);

extern JSBool
js_DefaultValue(JSContext *cx, JSObject *obj, JSType hint, jsval *vp);

extern JSBool
js_Enumerate(JSContext *cx, JSObject *obj, JSIterateOp enum_op,
             jsval *statep, jsid *idp);

extern void
js_MarkEnumeratorState(JSTracer *trc, JSObject *obj, jsval state);

extern void
js_PurgeCachedNativeEnumerators(JSContext *cx, JSThreadData *data);

extern JSBool
js_CheckAccess(JSContext *cx, JSObject *obj, jsid id, JSAccessMode mode,
               jsval *vp, uintN *attrsp);

extern JSType
js_TypeOf(JSContext *cx, JSObject *obj);

extern JSBool
js_Call(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

extern JSBool
js_Construct(JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
             jsval *rval);

extern JSBool
js_HasInstance(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

extern JSBool
js_SetProtoOrParent(JSContext *cx, JSObject *obj, uint32 slot, JSObject *pobj,
                    JSBool checkForCycles);

extern JSBool
js_IsDelegate(JSContext *cx, JSObject *obj, jsval v, JSBool *bp);

/*
 * If protoKey is not JSProto_Null, then clasp is ignored. If protoKey is
 * JSProto_Null, clasp must non-null.
 */
extern JS_FRIEND_API(JSBool)
js_GetClassPrototype(JSContext *cx, JSObject *scope, JSProtoKey protoKey,
                     JSObject **protop, JSClass *clasp = NULL);

extern JSBool
js_SetClassPrototype(JSContext *cx, JSObject *ctor, JSObject *proto,
                     uintN attrs);

/*
 * Wrap boolean, number or string as Boolean, Number or String object.
 * *vp must not be an object, null or undefined.
 */
extern JSBool
js_PrimitiveToObject(JSContext *cx, jsval *vp);

extern JSBool
js_ValueToObject(JSContext *cx, jsval v, JSObject **objp);

extern JSObject *
js_ValueToNonNullObject(JSContext *cx, jsval v);

extern JSBool
js_TryValueOf(JSContext *cx, JSObject *obj, JSType type, jsval *rval);

extern JSBool
js_TryMethod(JSContext *cx, JSObject *obj, JSAtom *atom,
             uintN argc, jsval *argv, jsval *rval);

extern JSBool
js_XDRObject(JSXDRState *xdr, JSObject **objp);

extern void
js_TraceObject(JSTracer *trc, JSObject *obj);

extern void
js_PrintObjectSlotName(JSTracer *trc, char *buf, size_t bufsize);

extern void
js_Clear(JSContext *cx, JSObject *obj);

#ifdef JS_THREADSAFE
#define NATIVE_DROP_PROPERTY js_DropProperty

extern void
js_DropProperty(JSContext *cx, JSObject *obj, JSProperty *prop);
#else
#define NATIVE_DROP_PROPERTY NULL
#endif

extern bool
js_GetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval *vp);

bool
js_SetReservedSlot(JSContext *cx, JSObject *obj, uint32 index, jsval v);

/*
 * Precondition: obj must be locked.
 */
extern JSBool
js_ReallocSlots(JSContext *cx, JSObject *obj, uint32 nslots,
                JSBool exactAllocation);

extern JSObject *
js_CheckScopeChainValidity(JSContext *cx, JSObject *scopeobj, const char *caller);

extern JSBool
js_CheckPrincipalsAccess(JSContext *cx, JSObject *scopeobj,
                         JSPrincipals *principals, JSAtom *caller);

/* For CSP -- checks if eval() and friends are allowed to run. */
extern JSBool
js_CheckContentSecurityPolicy(JSContext *cx);

/* Infallible -- returns its argument if there is no wrapped object. */
extern JSObject *
js_GetWrappedObject(JSContext *cx, JSObject *obj);

/* NB: Infallible. */
extern const char *
js_ComputeFilename(JSContext *cx, JSStackFrame *caller,
                   JSPrincipals *principals, uintN *linenop);

static inline bool
js_IsCallable(jsval v) {
    return !JSVAL_IS_PRIMITIVE(v) && JSVAL_TO_OBJECT(v)->isCallable();
}

extern JSBool
js_ReportGetterOnlyAssignment(JSContext *cx);

extern JS_FRIEND_API(JSBool)
js_GetterOnlyPropertyStub(JSContext *cx, JSObject *obj, jsval id, jsval *vp);

#ifdef DEBUG
JS_FRIEND_API(void) js_DumpChars(const jschar *s, size_t n);
JS_FRIEND_API(void) js_DumpString(JSString *str);
JS_FRIEND_API(void) js_DumpAtom(JSAtom *atom);
JS_FRIEND_API(void) js_DumpValue(jsval val);
JS_FRIEND_API(void) js_DumpId(jsid id);
JS_FRIEND_API(void) js_DumpObject(JSObject *obj);
JS_FRIEND_API(void) js_DumpStackFrame(JSStackFrame *fp);
#endif

extern uintN
js_InferFlags(JSContext *cx, uintN defaultFlags);

/* Object constructor native. Exposed only so the JIT can know its address. */
JSBool
js_Object(JSContext *cx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);

JS_END_EXTERN_C

#endif /* jsobj_h___ */
