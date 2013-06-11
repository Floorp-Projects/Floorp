/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Inline members for javascript type inference. */

#include "mozilla/PodOperations.h"

#include "jsarray.h"
#include "jsanalyze.h"
#include "jscompartment.h"
#include "jsinfer.h"
#include "jsprf.h"
#include "jsproxy.h"
#include "jstypedarray.h"

#include "builtin/ParallelArray.h"
#include "ion/IonFrames.h"
#include "js/RootingAPI.h"
#include "vm/GlobalObject.h"

#include "jsanalyzeinlines.h"
#include "vm/Stack-inl.h"

#ifndef jsinferinlines_h___
#define jsinferinlines_h___

inline bool
js::TaggedProto::isObject() const
{
    /* Skip NULL and Proxy::LazyProto. */
    return uintptr_t(proto) > uintptr_t(Proxy::LazyProto);
}

inline bool
js::TaggedProto::isLazy() const
{
    return proto == Proxy::LazyProto;
}

inline JSObject *
js::TaggedProto::toObject() const
{
    JS_ASSERT(isObject());
    return proto;
}

inline JSObject *
js::TaggedProto::toObjectOrNull() const
{
    JS_ASSERT(!proto || isObject());
    return proto;
}

template<class Outer>
inline bool
js::TaggedProtoOperations<Outer>::isLazy() const
{
    return value()->isLazy();
}

template<class Outer>
inline bool
js::TaggedProtoOperations<Outer>::isObject() const
{
    return value()->isObject();
}

template<class Outer>
inline JSObject *
js::TaggedProtoOperations<Outer>::toObject() const
{
    return value()->toObject();
}

template<class Outer>
inline JSObject *
js::TaggedProtoOperations<Outer>::toObjectOrNull() const
{
    return value()->toObjectOrNull();
}

namespace js {
namespace types {

/////////////////////////////////////////////////////////////////////
// CompilerOutput & RecompileInfo
/////////////////////////////////////////////////////////////////////

inline
CompilerOutput::CompilerOutput()
  : script(NULL),
    kindInt(Ion)
{
}

inline ion::IonScript *
CompilerOutput::ion() const
{
#ifdef JS_ION
    JS_ASSERT(isValid());
    switch (kind()) {
      case Ion: return script->ionScript();
      case ParallelIon: return script->parallelIonScript();
    }
#endif
    JS_NOT_REACHED("Invalid kind of CompilerOutput");
    return NULL;
}

inline bool
CompilerOutput::isValid() const
{
    if (!script)
        return false;

#if defined(DEBUG) && defined(JS_ION)
    TypeCompartment &types = script->compartment()->types;
#endif

    switch (kind()) {
      case Ion:
#ifdef JS_ION
        if (script->hasIonScript()) {
            JS_ASSERT(this == script->ionScript()->recompileInfo().compilerOutput(types));
            return true;
        }
        if (script->isIonCompilingOffThread())
            return true;
#endif
        return false;

      case ParallelIon:
#ifdef JS_ION
        if (script->hasParallelIonScript()) {
            JS_ASSERT(this == script->parallelIonScript()->recompileInfo().compilerOutput(types));
            return true;
        }
        if (script->isParallelIonCompilingOffThread())
            return true;
#endif
        return false;
    }
    return false;
}

inline CompilerOutput*
RecompileInfo::compilerOutput(TypeCompartment &types) const
{
    if (!types.constrainedOutputs || outputIndex >= types.constrainedOutputs->length())
        return NULL;
    return &(*types.constrainedOutputs)[outputIndex];
}

inline CompilerOutput*
RecompileInfo::compilerOutput(JSContext *cx) const
{
    return compilerOutput(cx->compartment()->types);
}

/////////////////////////////////////////////////////////////////////
// Types
/////////////////////////////////////////////////////////////////////

/* static */ inline Type
Type::ObjectType(JSObject *obj)
{
    if (obj->hasSingletonType())
        return Type(uintptr_t(obj) | 1);
    return Type(uintptr_t(obj->type()));
}

/* static */ inline Type
Type::ObjectType(TypeObject *obj)
{
    if (obj->singleton)
        return Type(uintptr_t(obj->singleton.get()) | 1);
    return Type(uintptr_t(obj));
}

/* static */ inline Type
Type::ObjectType(TypeObjectKey *obj)
{
    return Type(uintptr_t(obj));
}

inline Type
GetValueType(JSContext *cx, const Value &val)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (val.isDouble())
        return Type::DoubleType();
    if (val.isObject())
        return Type::ObjectType(&val.toObject());
    return Type::PrimitiveType(val.extractNonDoubleType());
}

inline TypeFlags
PrimitiveTypeFlag(JSValueType type)
{
    switch (type) {
      case JSVAL_TYPE_UNDEFINED:
        return TYPE_FLAG_UNDEFINED;
      case JSVAL_TYPE_NULL:
        return TYPE_FLAG_NULL;
      case JSVAL_TYPE_BOOLEAN:
        return TYPE_FLAG_BOOLEAN;
      case JSVAL_TYPE_INT32:
        return TYPE_FLAG_INT32;
      case JSVAL_TYPE_DOUBLE:
        return TYPE_FLAG_DOUBLE;
      case JSVAL_TYPE_STRING:
        return TYPE_FLAG_STRING;
      case JSVAL_TYPE_MAGIC:
        return TYPE_FLAG_LAZYARGS;
      default:
        JS_NOT_REACHED("Bad type");
        return 0;
    }
}

inline JSValueType
TypeFlagPrimitive(TypeFlags flags)
{
    switch (flags) {
      case TYPE_FLAG_UNDEFINED:
        return JSVAL_TYPE_UNDEFINED;
      case TYPE_FLAG_NULL:
        return JSVAL_TYPE_NULL;
      case TYPE_FLAG_BOOLEAN:
        return JSVAL_TYPE_BOOLEAN;
      case TYPE_FLAG_INT32:
        return JSVAL_TYPE_INT32;
      case TYPE_FLAG_DOUBLE:
        return JSVAL_TYPE_DOUBLE;
      case TYPE_FLAG_STRING:
        return JSVAL_TYPE_STRING;
      case TYPE_FLAG_LAZYARGS:
        return JSVAL_TYPE_MAGIC;
      default:
        JS_NOT_REACHED("Bad type");
        return (JSValueType) 0;
    }
}

/*
 * Get the canonical representation of an id to use when doing inference.  This
 * maintains the constraint that if two different jsids map to the same property
 * in JS (e.g. 3 and "3"), they have the same type representation.
 */
inline jsid
IdToTypeId(jsid id)
{
    JS_ASSERT(!JSID_IS_EMPTY(id));

    /*
     * All integers must map to the aggregate property for index types, including
     * negative integers.
     */
    if (JSID_IS_INT(id))
        return JSID_VOID;

    /*
     * Check for numeric strings, as in js_StringIsIndex, but allow negative
     * and overflowing integers.
     */
    if (JSID_IS_STRING(id)) {
        JSFlatString *str = JSID_TO_FLAT_STRING(id);
        JS::TwoByteChars cp = str->range();
        if (JS7_ISDEC(cp[0]) || cp[0] == '-') {
            for (size_t i = 1; i < cp.length(); ++i) {
                if (!JS7_ISDEC(cp[i]))
                    return id;
            }
            return JSID_VOID;
        }
        return id;
    }

    return JSID_VOID;
}

const char * TypeIdStringImpl(jsid id);

/* Convert an id for printing during debug. */
static inline const char *
TypeIdString(jsid id)
{
#ifdef DEBUG
    return TypeIdStringImpl(id);
#else
    return "(missing)";
#endif
}

/* Assert code to know which PCs are reasonable to be considering inlining on. */
inline bool
IsInlinableCall(jsbytecode *pc)
{
    JSOp op = JSOp(*pc);

    // CALL, FUNCALL, FUNAPPLY, EVAL (Standard callsites)
    // NEW (IonMonkey-only callsite)
    // GETPROP, CALLPROP, and LENGTH. (Inlined Getters)
    // SETPROP, SETNAME, SETGNAME (Inlined Setters)
    return op == JSOP_CALL || op == JSOP_FUNCALL || op == JSOP_FUNAPPLY || op == JSOP_EVAL ||
#ifdef JS_ION
           op == JSOP_NEW ||
#endif
           op == JSOP_GETPROP || op == JSOP_CALLPROP || op == JSOP_LENGTH ||
           op == JSOP_SETPROP || op == JSOP_SETGNAME || op == JSOP_SETNAME;
}

/*
 * Structure for type inference entry point functions. All functions which can
 * change type information must use this, and functions which depend on
 * intermediate types (i.e. JITs) can use this to ensure that intermediate
 * information is not collected and does not change.
 *
 * Pins inference results so that intermediate type information, TypeObjects
 * and JSScripts won't be collected during GC. Does additional sanity checking
 * that inference is not reentrant and that recompilations occur properly.
 */
struct AutoEnterAnalysis
{
    /* Prevent GC activity in the middle of analysis. */
    gc::AutoSuppressGC suppressGC;

    FreeOp *freeOp;
    JSCompartment *compartment;
    bool oldActiveAnalysis;

    AutoEnterAnalysis(JSContext *cx)
      : suppressGC(cx)
    {
        init(cx->runtime()->defaultFreeOp(), cx->compartment());
    }

    AutoEnterAnalysis(FreeOp *fop, JSCompartment *comp)
      : suppressGC(comp)
    {
        init(fop, comp);
    }

    ~AutoEnterAnalysis()
    {
        compartment->activeAnalysis = oldActiveAnalysis;

        /*
         * If there are no more type inference activations on the stack,
         * process any triggered recompilations. Note that we should not be
         * invoking any scripted code while type inference is running.
         * :TODO: assert this.
         */
        if (!compartment->activeAnalysis) {
            TypeCompartment *types = &compartment->types;
            if (compartment->zone()->types.pendingNukeTypes)
                compartment->zone()->types.nukeTypes(freeOp);
            else if (types->pendingRecompiles)
                types->processPendingRecompiles(freeOp);
        }
    }

  private:
    void init(FreeOp *fop, JSCompartment *comp) {
        freeOp = fop;
        compartment = comp;
        oldActiveAnalysis = compartment->activeAnalysis;
        compartment->activeAnalysis = true;
    }
};

/*
 * Structure marking the currently compiled script, for constraints which can
 * trigger recompilation.
 */
struct AutoEnterCompilation
{
    JSContext *cx;
    RecompileInfo &info;
    CompilerOutput::Kind kind;

    AutoEnterCompilation(JSContext *cx, CompilerOutput::Kind kind)
      : cx(cx),
        info(cx->compartment()->types.compiledInfo),
        kind(kind)
    {
        JS_ASSERT(cx->compartment()->activeAnalysis);
        JS_ASSERT(info.outputIndex == RecompileInfo::NoCompilerRunning);
    }

    bool init(JSScript *script)
    {
        CompilerOutput co;
        co.script = script;
        co.setKind(kind);

        // This flag is used to prevent adding the current compiled script in
        // the list of compiler output which should be invalided.  This is
        // necessary because we can run some analysis might discard the script
        // it-self, which can happen when the monitored value does not reflect
        // the types propagated by the type inference.
        co.pendingRecompilation = true;

        JS_ASSERT(!co.isValid());
        TypeCompartment &types = cx->compartment()->types;
        if (!types.constrainedOutputs) {
            types.constrainedOutputs = cx->new_< Vector<CompilerOutput> >(cx);
            if (!types.constrainedOutputs) {
                types.setPendingNukeTypes(cx);
                return false;
            }
        }

        info.outputIndex = types.constrainedOutputs->length();
        // I hope we GC before we reach 64k of compilation attempts.
        if (info.outputIndex >= RecompileInfo::NoCompilerRunning)
            return false;

        if (!types.constrainedOutputs->append(co)) {
            info.outputIndex = RecompileInfo::NoCompilerRunning;
            return false;
        }
        return true;
    }

    void initExisting(RecompileInfo oldInfo)
    {
        // Initialize the active compilation index from that produced during a
        // previous compilation, for finishing an off thread compilation.
        info = oldInfo;
    }

    ~AutoEnterCompilation()
    {
        // Handle failure cases of init.
        if (info.outputIndex >= RecompileInfo::NoCompilerRunning)
            return;

        JS_ASSERT(info.outputIndex < cx->compartment()->types.constrainedOutputs->length());
        CompilerOutput *co = info.compilerOutput(cx);
        co->pendingRecompilation = false;
        if (!co->isValid())
            co->invalidate();

        info.outputIndex = RecompileInfo::NoCompilerRunning;
    }
};

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

/*
 * These functions check whether inference is enabled before performing some
 * action on the type state. To avoid checking cx->typeInferenceEnabled()
 * everywhere, it is generally preferred to use one of these functions or
 * a type function on JSScript to perform inference operations.
 */

inline Class *
GetClassForProtoKey(JSProtoKey key)
{
    switch (key) {
      case JSProto_Object:
        return &ObjectClass;
      case JSProto_Array:
        return &ArrayClass;

      case JSProto_Number:
        return &NumberClass;
      case JSProto_Boolean:
        return &BooleanClass;
      case JSProto_String:
        return &StringClass;
      case JSProto_RegExp:
        return &RegExpClass;

      case JSProto_Int8Array:
      case JSProto_Uint8Array:
      case JSProto_Int16Array:
      case JSProto_Uint16Array:
      case JSProto_Int32Array:
      case JSProto_Uint32Array:
      case JSProto_Float32Array:
      case JSProto_Float64Array:
      case JSProto_Uint8ClampedArray:
        return &TypedArray::classes[key - JSProto_Int8Array];

      case JSProto_ArrayBuffer:
        return &ArrayBufferClass;

      case JSProto_DataView:
        return &DataViewClass;

      case JSProto_ParallelArray:
        return &ParallelArrayObject::class_;

      default:
        JS_NOT_REACHED("Bad proto key");
        return NULL;
    }
}

/*
 * Get the default 'new' object for a given standard class, per the currently
 * active global.
 */
inline TypeObject *
GetTypeNewObject(JSContext *cx, JSProtoKey key)
{
    RootedObject proto(cx);
    if (!js_GetClassPrototype(cx, key, &proto))
        return NULL;
    return proto->getNewType(cx, GetClassForProtoKey(key));
}

/* Get a type object for the immediate allocation site within a native. */
inline TypeObject *
GetTypeCallerInitObject(JSContext *cx, JSProtoKey key)
{
    if (cx->typeInferenceEnabled()) {
        jsbytecode *pc;
        RootedScript script(cx, cx->stack.currentScript(&pc));
        if (script)
            return TypeScript::InitObject(cx, script, pc, key);
    }
    return GetTypeNewObject(cx, key);
}

void MarkIteratorUnknownSlow(JSContext *cx);

/*
 * When using a custom iterator within the initialization of a 'for in' loop,
 * mark the iterator values as unknown.
 */
inline void
MarkIteratorUnknown(JSContext *cx)
{
    if (cx->typeInferenceEnabled())
        MarkIteratorUnknownSlow(cx);
}

void TypeMonitorCallSlow(JSContext *cx, JSObject *callee, const CallArgs &args,
                         bool constructing);

/*
 * Monitor a javascript call, either on entry to the interpreter or made
 * from within the interpreter.
 */
inline void
TypeMonitorCall(JSContext *cx, const js::CallArgs &args, bool constructing)
{
    if (args.callee().isFunction()) {
        JSFunction *fun = args.callee().toFunction();
        if (fun->isInterpreted() && fun->nonLazyScript()->types && cx->typeInferenceEnabled())
            TypeMonitorCallSlow(cx, &args.callee(), args, constructing);
    }
}

inline bool
TrackPropertyTypes(JSContext *cx, JSObject *obj, jsid id)
{
    if (!cx->typeInferenceEnabled() || obj->hasLazyType() || obj->type()->unknownProperties())
        return false;

    if (obj->hasSingletonType() && !obj->type()->maybeGetProperty(id, cx))
        return false;

    return true;
}

inline void
EnsureTrackPropertyTypes(JSContext *cx, JSObject *obj, jsid id)
{
    JS_ASSERT(!obj->hasLazyType());

    if (!cx->typeInferenceEnabled() || obj->type()->unknownProperties())
        return;

    id = IdToTypeId(id);

    if (obj->hasSingletonType()) {
        AutoEnterAnalysis enter(cx);
        obj->type()->getProperty(cx, id, true);
    }

    JS_ASSERT(obj->type()->unknownProperties() || TrackPropertyTypes(cx, obj, id));
}

/* Add a possible type for a property of obj. */
inline void
AddTypePropertyId(JSContext *cx, JSObject *obj, jsid id, Type type)
{
    if (cx->typeInferenceEnabled())
        id = IdToTypeId(id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->addPropertyType(cx, id, type);
}

inline void
AddTypePropertyId(JSContext *cx, JSObject *obj, jsid id, const Value &value)
{
    if (cx->typeInferenceEnabled())
        id = IdToTypeId(id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->addPropertyType(cx, id, value);
}

inline void
AddTypeProperty(JSContext *cx, TypeObject *obj, const char *name, Type type)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyType(cx, name, type);
}

inline void
AddTypeProperty(JSContext *cx, TypeObject *obj, const char *name, const Value &value)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyType(cx, name, value);
}

/* Set one or more dynamic flags on a type object. */
inline void
MarkTypeObjectFlags(JSContext *cx, JSObject *obj, TypeObjectFlags flags)
{
    if (cx->typeInferenceEnabled() && !obj->hasLazyType() && !obj->type()->hasAllFlags(flags))
        obj->type()->setFlags(cx, flags);
}

/*
 * Mark all properties of a type object as unknown. If markSetsUnknown is set,
 * scan the entire compartment and mark all type sets containing it as having
 * an unknown object. This is needed for correctness in dealing with mutable
 * __proto__, which can change the type of an object dynamically.
 */
inline void
MarkTypeObjectUnknownProperties(JSContext *cx, TypeObject *obj,
                                bool markSetsUnknown = false)
{
    if (cx->typeInferenceEnabled()) {
        if (!obj->unknownProperties())
            obj->markUnknown(cx);
        if (markSetsUnknown && !(obj->flags & OBJECT_FLAG_SETS_MARKED_UNKNOWN))
            cx->compartment()->types.markSetsUnknown(cx, obj);
    }
}

/*
 * Mark any property which has been deleted or configured to be non-writable or
 * have a getter/setter.
 */
inline void
MarkTypePropertyConfigured(JSContext *cx, HandleObject obj, jsid id)
{
    if (cx->typeInferenceEnabled())
        id = IdToTypeId(id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->markPropertyConfigured(cx, id);
}

/* Mark a state change on a particular object. */
inline void
MarkObjectStateChange(JSContext *cx, JSObject *obj)
{
    if (cx->typeInferenceEnabled() && !obj->hasLazyType() && !obj->type()->unknownProperties())
        obj->type()->markStateChange(cx);
}

/*
 * For an array or object which has not yet escaped and been referenced elsewhere,
 * pick a new type based on the object's current contents.
 */

inline void
FixArrayType(JSContext *cx, HandleObject obj)
{
    if (cx->typeInferenceEnabled())
        cx->compartment()->types.fixArrayType(cx, obj);
}

inline void
FixObjectType(JSContext *cx, HandleObject obj)
{
    if (cx->typeInferenceEnabled())
        cx->compartment()->types.fixObjectType(cx, obj);
}

/* Interface helpers for JSScript*. */
extern void TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc,
                              const js::Value &rval);
extern void TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc,
                              js::types::Type type);

inline bool
UseNewTypeAtEntry(JSContext *cx, StackFrame *fp)
{
    if (!fp->isConstructing() || !cx->typeInferenceEnabled() || !fp->prev())
        return false;

    JSScript *prevScript = fp->prev()->script();
    return UseNewType(cx, prevScript, fp->prevpc());
}

inline bool
UseNewTypeForClone(JSFunction *fun)
{
    if (!fun->isInterpreted())
        return false;

    if (fun->hasScript() && fun->nonLazyScript()->shouldCloneAtCallsite)
        return true;

    if (fun->isArrow())
        return true;

    if (fun->hasSingletonType())
        return false;

    /*
     * When a function is being used as a wrapper for another function, it
     * improves precision greatly to distinguish between different instances of
     * the wrapper; otherwise we will conflate much of the information about
     * the wrapped functions.
     *
     * An important example is the Class.create function at the core of the
     * Prototype.js library, which looks like:
     *
     * var Class = {
     *   create: function() {
     *     return function() {
     *       this.initialize.apply(this, arguments);
     *     }
     *   }
     * };
     *
     * Each instance of the innermost function will have a different wrapped
     * initialize method. We capture this, along with similar cases, by looking
     * for short scripts which use both .apply and arguments. For such scripts,
     * whenever creating a new instance of the function we both give that
     * instance a singleton type and clone the underlying script.
     */

    JSScript *script = fun->nonLazyScript();

    if (script->length >= 50)
        return false;

    if (script->hasConsts() || script->hasObjects() || script->hasRegexps() || fun->isHeavyweight())
        return false;

    bool hasArguments = false;
    bool hasApply = false;

    for (jsbytecode *pc = script->code;
         pc != script->code + script->length;
         pc += GetBytecodeLength(pc))
    {
        if (*pc == JSOP_ARGUMENTS)
            hasArguments = true;
        if (*pc == JSOP_FUNAPPLY)
            hasApply = true;
    }

    return hasArguments && hasApply;
}

/////////////////////////////////////////////////////////////////////
// Script interface functions
/////////////////////////////////////////////////////////////////////

/* static */ inline unsigned
TypeScript::NumTypeSets(JSScript *script)
{
    return script->nTypeSets + analyze::LocalSlot(script, 0);
}

/* static */ inline HeapTypeSet *
TypeScript::ReturnTypes(JSScript *script)
{
    TypeSet *types = script->types->typeArray() + script->nTypeSets + js::analyze::CalleeSlot();
    return types->toHeapTypeSet();
}

/* static */ inline StackTypeSet *
TypeScript::ThisTypes(JSScript *script)
{
    TypeSet *types = script->types->typeArray() + script->nTypeSets + js::analyze::ThisSlot();
    return types->toStackTypeSet();
}

/*
 * Note: for non-escaping arguments and locals, argTypes/localTypes reflect
 * only the initial type of the variable (e.g. passed values for argTypes,
 * or undefined for localTypes) and not types from subsequent assignments.
 */

/* static */ inline StackTypeSet *
TypeScript::ArgTypes(JSScript *script, unsigned i)
{
    JS_ASSERT(i < script->function()->nargs);
    TypeSet *types = script->types->typeArray() + script->nTypeSets + js::analyze::ArgSlot(i);
    return types->toStackTypeSet();
}

/* static */ inline StackTypeSet *
TypeScript::SlotTypes(JSScript *script, unsigned slot)
{
    JS_ASSERT(slot < js::analyze::LocalSlot(script, 0));
    TypeSet *types = script->types->typeArray() + script->nTypeSets + slot;
    return types->toStackTypeSet();
}

/* static */ inline StackTypeSet *
TypeScript::BytecodeTypes(JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(js_CodeSpec[*pc].format & JOF_TYPESET);
    JS_ASSERT(script->types && script->types->bytecodeMap);
    uint32_t *bytecodeMap = script->types->bytecodeMap;
    uint32_t *hint = bytecodeMap + script->nTypeSets;
    uint32_t offset = pc - script->code;
    JS_ASSERT(offset < script->length);

    // See if this pc is the next typeset opcode after the last one looked up.
    if (bytecodeMap[*hint + 1] == offset && (*hint + 1) < script->nTypeSets) {
        (*hint)++;
        return script->types->typeArray()->toStackTypeSet() + *hint;
    }

    // See if this pc is the same as the last one looked up.
    if (bytecodeMap[*hint] == offset)
        return script->types->typeArray()->toStackTypeSet() + *hint;

    // Fall back to a binary search.
    size_t bottom = 0;
    size_t top = script->nTypeSets - 1;
    size_t mid = (bottom + top) / 2;
    while (mid < top) {
        if (bytecodeMap[mid] < offset)
            bottom = mid + 1;
        else if (bytecodeMap[mid] > offset)
            top = mid;
        else
            break;
        mid = (bottom + top) / 2;
    }

    // We should have have zeroed in on either the exact offset, unless there
    // are more JOF_TYPESET opcodes than nTypeSets in the script (as can happen
    // if the script is very long).
    JS_ASSERT(bytecodeMap[mid] == offset || mid == top);

    *hint = mid;
    return script->types->typeArray()->toStackTypeSet() + *hint;
}

/* static */ inline TypeObject *
TypeScript::StandardType(JSContext *cx, JSProtoKey key)
{
    RootedObject proto(cx);
    if (!js_GetClassPrototype(cx, key, &proto, NULL))
        return NULL;
    return proto->getNewType(cx, GetClassForProtoKey(key));
}

struct AllocationSiteKey {
    JSScript *script;

    uint32_t offset : 24;
    JSProtoKey kind : 8;

    static const uint32_t OFFSET_LIMIT = (1 << 23);

    AllocationSiteKey() { mozilla::PodZero(this); }

    typedef AllocationSiteKey Lookup;

    static inline uint32_t hash(AllocationSiteKey key) {
        return uint32_t(size_t(key.script->code + key.offset)) ^ key.kind;
    }

    static inline bool match(const AllocationSiteKey &a, const AllocationSiteKey &b) {
        return a.script == b.script && a.offset == b.offset && a.kind == b.kind;
    }
};

/* Whether to use a new type object for an initializer opcode at script/pc. */
js::NewObjectKind
UseNewTypeForInitializer(JSContext *cx, JSScript *script, jsbytecode *pc, JSProtoKey key);

js::NewObjectKind
UseNewTypeForInitializer(JSContext *cx, JSScript *script, jsbytecode *pc, Class *clasp);

/* static */ inline TypeObject *
TypeScript::InitObject(JSContext *cx, JSScript *script, jsbytecode *pc, JSProtoKey kind)
{
    JS_ASSERT(!UseNewTypeForInitializer(cx, script, pc, kind));

    /* :XXX: Limit script->length so we don't need to check the offset up front? */
    uint32_t offset = pc - script->code;

    if (!cx->typeInferenceEnabled() || !script->compileAndGo || offset >= AllocationSiteKey::OFFSET_LIMIT)
        return GetTypeNewObject(cx, kind);

    AllocationSiteKey key;
    key.script = script;
    key.offset = offset;
    key.kind = kind;

    if (!cx->compartment()->types.allocationSiteTable)
        return cx->compartment()->types.addAllocationSiteTypeObject(cx, key);

    AllocationSiteTable::Ptr p = cx->compartment()->types.allocationSiteTable->lookup(key);

    if (p)
        return p->value;
    return cx->compartment()->types.addAllocationSiteTypeObject(cx, key);
}

/* Set the type to use for obj according to the site it was allocated at. */
static inline bool
SetInitializerObjectType(JSContext *cx, HandleScript script, jsbytecode *pc, HandleObject obj, NewObjectKind kind)
{
    if (!cx->typeInferenceEnabled())
        return true;

    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(obj->getClass());
    JS_ASSERT(key != JSProto_Null);
    JS_ASSERT(kind == UseNewTypeForInitializer(cx, script, pc, key));

    if (kind == SingletonObject) {
        JS_ASSERT(obj->hasSingletonType());

        /*
         * Inference does not account for types of run-once initializer
         * objects, as these may not be created until after the script
         * has been analyzed.
         */
        TypeScript::Monitor(cx, script, pc, ObjectValue(*obj));
    } else {
        types::TypeObject *type = TypeScript::InitObject(cx, script, pc, key);
        if (!type)
            return false;
        obj->setType(type);
    }

    return true;
}

/* static */ inline void
TypeScript::Monitor(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    if (cx->typeInferenceEnabled())
        TypeMonitorResult(cx, script, pc, rval);
}

/* static */ inline void
TypeScript::MonitorOverflow(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script, pc, Type::DoubleType());
}

/* static */ inline void
TypeScript::MonitorString(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script, pc, Type::StringType());
}

/* static */ inline void
TypeScript::MonitorUnknown(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script, pc, Type::UnknownType());
}

/* static */ inline void
TypeScript::GetPcScript(JSContext *cx, JSScript **script, jsbytecode **pc)
{
#ifdef JS_ION
    if (cx->fp()->beginsIonActivation()) {
        ion::GetPcScript(cx, script, pc);
        return;
    }
#endif
    *script = cx->fp()->script();
    *pc = cx->regs().pc;
}

/* static */ inline void
TypeScript::MonitorOverflow(JSContext *cx)
{
    RootedScript script(cx);
    jsbytecode *pc;
    GetPcScript(cx, script.address(), &pc);
    MonitorOverflow(cx, script, pc);
}

/* static */ inline void
TypeScript::MonitorString(JSContext *cx)
{
    RootedScript script(cx);
    jsbytecode *pc;
    GetPcScript(cx, script.address(), &pc);
    MonitorString(cx, script, pc);
}

/* static */ inline void
TypeScript::MonitorUnknown(JSContext *cx)
{
    RootedScript script(cx);
    jsbytecode *pc;
    GetPcScript(cx, script.address(), &pc);
    MonitorUnknown(cx, script, pc);
}

/* static */ inline void
TypeScript::Monitor(JSContext *cx, const js::Value &rval)
{
    RootedScript script(cx);
    jsbytecode *pc;
    GetPcScript(cx, script.address(), &pc);
    Monitor(cx, script, pc, rval);
}

/* static */ inline void
TypeScript::MonitorAssign(JSContext *cx, HandleObject obj, jsid id)
{
    if (cx->typeInferenceEnabled() && !obj->hasSingletonType()) {
        /*
         * Mark as unknown any object which has had dynamic assignments to
         * non-integer properties at SETELEM opcodes. This avoids making large
         * numbers of type properties for hashmap-style objects. We don't need
         * to do this for objects with singleton type, because type properties
         * are only constructed for them when analyzed scripts depend on those
         * specific properties.
         */
        uint32_t i;
        if (js_IdIsIndex(id, &i))
            return;
        MarkTypeObjectUnknownProperties(cx, obj->type());
    }
}

/* static */ inline void
TypeScript::SetThis(JSContext *cx, JSScript *script, Type type)
{
    if (!cx->typeInferenceEnabled() || !script->types)
        return;

    if (!ThisTypes(script)->hasType(type)) {
        AutoEnterAnalysis enter(cx);

        InferSpew(ISpewOps, "externalType: setThis #%u: %s",
                  script->id(), TypeString(type));
        ThisTypes(script)->addType(cx, type);
    }
}

/* static */ inline void
TypeScript::SetThis(JSContext *cx, JSScript *script, const js::Value &value)
{
    if (cx->typeInferenceEnabled())
        SetThis(cx, script, GetValueType(cx, value));
}

/* static */ inline void
TypeScript::SetArgument(JSContext *cx, JSScript *script, unsigned arg, Type type)
{
    if (!cx->typeInferenceEnabled() || !script->types)
        return;

    if (!ArgTypes(script, arg)->hasType(type)) {
        AutoEnterAnalysis enter(cx);

        InferSpew(ISpewOps, "externalType: setArg #%u %u: %s",
                  script->id(), arg, TypeString(type));
        ArgTypes(script, arg)->addType(cx, type);
    }
}

/* static */ inline void
TypeScript::SetArgument(JSContext *cx, JSScript *script, unsigned arg, const js::Value &value)
{
    if (cx->typeInferenceEnabled()) {
        Type type = GetValueType(cx, value);
        SetArgument(cx, script, arg, type);
    }
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

inline JSCompartment *
TypeCompartment::compartment()
{
    return (JSCompartment *)((char *)this - offsetof(JSCompartment, types));
}

inline void
TypeCompartment::addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, Type type)
{
    JS_ASSERT(this == &cx->compartment()->types);
    JS_ASSERT(!cx->runtime()->isHeapBusy());

    InferSpew(ISpewOps, "pending: %sC%p%s %s",
              InferSpewColor(constraint), constraint, InferSpewColorReset(),
              TypeString(type));

    if ((pendingCount == pendingCapacity) && !growPendingArray(cx))
        return;

    PendingWork &pending = pendingArray[pendingCount++];
    pending.constraint = constraint;
    pending.source = source;
    pending.type = type;
}

inline void
TypeCompartment::resolvePending(JSContext *cx)
{
    JS_ASSERT(this == &cx->compartment()->types);

    if (resolving) {
        /* There is an active call further up resolving the worklist. */
        return;
    }

    resolving = true;

    /* Handle all pending type registrations. */
    while (pendingCount) {
        const PendingWork &pending = pendingArray[--pendingCount];
        InferSpew(ISpewOps, "resolve: %sC%p%s %s",
                  InferSpewColor(pending.constraint), pending.constraint,
                  InferSpewColorReset(), TypeString(pending.type));
        pending.constraint->newType(cx, pending.source, pending.type);
    }

    resolving = false;
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

/*
 * The sets of objects and scripts in a type set grow monotonically, are usually
 * empty, almost always small, and sometimes big.  For empty or singleton sets,
 * the pointer refers directly to the value.  For sets fitting into SET_ARRAY_SIZE,
 * an array of this length is used to store the elements.  For larger sets, a hash
 * table filled to 25%-50% of capacity is used, with collisions resolved by linear
 * probing.  TODO: replace these with jshashtables.
 */
const unsigned SET_ARRAY_SIZE = 8;

/* Get the capacity of a set with the given element count. */
static inline unsigned
HashSetCapacity(unsigned count)
{
    JS_ASSERT(count >= 2);

    if (count <= SET_ARRAY_SIZE)
        return SET_ARRAY_SIZE;

    unsigned log2;
    JS_FLOOR_LOG2(log2, count);
    return 1 << (log2 + 2);
}

/* Compute the FNV hash for the low 32 bits of v. */
template <class T, class KEY>
static inline uint32_t
HashKey(T v)
{
    uint32_t nv = KEY::keyBits(v);

    uint32_t hash = 84696351 ^ (nv & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 8) & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 16) & 0xff);
    return (hash * 16777619) ^ ((nv >> 24) & 0xff);
}

/*
 * Insert space for an element into the specified set and grow its capacity if needed.
 * returned value is an existing or new entry (NULL if new).
 */
template <class T, class U, class KEY>
static U **
HashSetInsertTry(LifoAlloc &alloc, U **&values, unsigned &count, T key)
{
    unsigned capacity = HashSetCapacity(count);
    unsigned insertpos = HashKey<T,KEY>(key) & (capacity - 1);

    /* Whether we are converting from a fixed array to hashtable. */
    bool converting = (count == SET_ARRAY_SIZE);

    if (!converting) {
        while (values[insertpos] != NULL) {
            if (KEY::getKey(values[insertpos]) == key)
                return &values[insertpos];
            insertpos = (insertpos + 1) & (capacity - 1);
        }
    }

    count++;
    unsigned newCapacity = HashSetCapacity(count);

    if (newCapacity == capacity) {
        JS_ASSERT(!converting);
        return &values[insertpos];
    }

    U **newValues = alloc.newArray<U*>(newCapacity);
    if (!newValues)
        return NULL;
    mozilla::PodZero(newValues, newCapacity);

    for (unsigned i = 0; i < capacity; i++) {
        if (values[i]) {
            unsigned pos = HashKey<T,KEY>(KEY::getKey(values[i])) & (newCapacity - 1);
            while (newValues[pos] != NULL)
                pos = (pos + 1) & (newCapacity - 1);
            newValues[pos] = values[i];
        }
    }

    values = newValues;

    insertpos = HashKey<T,KEY>(key) & (newCapacity - 1);
    while (values[insertpos] != NULL)
        insertpos = (insertpos + 1) & (newCapacity - 1);
    return &values[insertpos];
}

/*
 * Insert an element into the specified set if it is not already there, returning
 * an entry which is NULL if the element was not there.
 */
template <class T, class U, class KEY>
static inline U **
HashSetInsert(LifoAlloc &alloc, U **&values, unsigned &count, T key)
{
    if (count == 0) {
        JS_ASSERT(values == NULL);
        count++;
        return (U **) &values;
    }

    if (count == 1) {
        U *oldData = (U*) values;
        if (KEY::getKey(oldData) == key)
            return (U **) &values;

        values = alloc.newArray<U*>(SET_ARRAY_SIZE);
        if (!values) {
            values = (U **) oldData;
            return NULL;
        }
        mozilla::PodZero(values, SET_ARRAY_SIZE);
        count++;

        values[0] = oldData;
        return &values[1];
    }

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return &values[i];
        }

        if (count < SET_ARRAY_SIZE) {
            count++;
            return &values[count - 1];
        }
    }

    return HashSetInsertTry<T,U,KEY>(alloc, values, count, key);
}

/* Lookup an entry in a hash set, return NULL if it does not exist. */
template <class T, class U, class KEY>
static inline U *
HashSetLookup(U **values, unsigned count, T key)
{
    if (count == 0)
        return NULL;

    if (count == 1)
        return (KEY::getKey((U *) values) == key) ? (U *) values : NULL;

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return values[i];
        }
        return NULL;
    }

    unsigned capacity = HashSetCapacity(count);
    unsigned pos = HashKey<T,KEY>(key) & (capacity - 1);

    while (values[pos] != NULL) {
        if (KEY::getKey(values[pos]) == key)
            return values[pos];
        pos = (pos + 1) & (capacity - 1);
    }

    return NULL;
}

inline TypeObjectKey *
Type::objectKey() const
{
    JS_ASSERT(isObject());
    if (isTypeObject())
        TypeObject::readBarrier((TypeObject *) data);
    else
        JSObject::readBarrier((JSObject *) (data ^ 1));
    return (TypeObjectKey *) data;
}

inline JSObject *
Type::singleObject() const
{
    JS_ASSERT(isSingleObject());
    JSObject::readBarrier((JSObject *) (data ^ 1));
    return (JSObject *) (data ^ 1);
}

inline TypeObject *
Type::typeObject() const
{
    JS_ASSERT(isTypeObject());
    TypeObject::readBarrier((TypeObject *) data);
    return (TypeObject *) data;
}

inline bool
TypeSet::hasType(Type type) const
{
    if (unknown())
        return true;

    if (type.isUnknown()) {
        return false;
    } else if (type.isPrimitive()) {
        return !!(flags & PrimitiveTypeFlag(type.primitive()));
    } else if (type.isAnyObject()) {
        return !!(flags & TYPE_FLAG_ANYOBJECT);
    } else {
        return !!(flags & TYPE_FLAG_ANYOBJECT) ||
            HashSetLookup<TypeObjectKey*,TypeObjectKey,TypeObjectKey>
            (objectSet, baseObjectCount(), type.objectKey()) != NULL;
    }
}

inline void
TypeSet::setBaseObjectCount(uint32_t count)
{
    JS_ASSERT(count <= TYPE_FLAG_OBJECT_COUNT_LIMIT);
    flags = (flags & ~TYPE_FLAG_OBJECT_COUNT_MASK)
          | (count << TYPE_FLAG_OBJECT_COUNT_SHIFT);
}

inline void
TypeSet::clearObjects()
{
    setBaseObjectCount(0);
    objectSet = NULL;
}

inline void
TypeSet::addType(JSContext *cx, Type type)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);

    if (unknown())
        return;

    if (type.isUnknown()) {
        flags |= TYPE_FLAG_BASE_MASK;
        clearObjects();
        JS_ASSERT(unknown());
    } else if (type.isPrimitive()) {
        TypeFlags flag = PrimitiveTypeFlag(type.primitive());
        if (flags & flag)
            return;

        /* If we add float to a type set it is also considered to contain int. */
        if (flag == TYPE_FLAG_DOUBLE)
            flag |= TYPE_FLAG_INT32;

        flags |= flag;
    } else {
        if (flags & TYPE_FLAG_ANYOBJECT)
            return;
        if (type.isAnyObject())
            goto unknownObject;

        LifoAlloc &alloc =
            purged() ? cx->compartment()->analysisLifoAlloc : cx->typeLifoAlloc();

        uint32_t objectCount = baseObjectCount();
        TypeObjectKey *object = type.objectKey();
        TypeObjectKey **pentry = HashSetInsert<TypeObjectKey *,TypeObjectKey,TypeObjectKey>
                                     (alloc, objectSet, objectCount, object);
        if (!pentry) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
        if (*pentry)
            return;
        *pentry = object;

        setBaseObjectCount(objectCount);

        if (objectCount == TYPE_FLAG_OBJECT_COUNT_LIMIT)
            goto unknownObject;

        if (type.isTypeObject()) {
            TypeObject *nobject = type.typeObject();
            JS_ASSERT(!nobject->singleton);
            if (nobject->unknownProperties())
                goto unknownObject;
            if (objectCount > 1) {
                nobject->contribution += (objectCount - 1) * (objectCount - 1);
                if (nobject->contribution >= TypeObject::CONTRIBUTION_LIMIT) {
                    InferSpew(ISpewOps, "limitUnknown: %sT%p%s",
                              InferSpewColor(this), this, InferSpewColorReset());
                    goto unknownObject;
                }
            }
        }
    }

    if (false) {
    unknownObject:
        type = Type::AnyObjectType();
        flags |= TYPE_FLAG_ANYOBJECT;
        clearObjects();
    }

    InferSpew(ISpewOps, "addType: %sT%p%s %s",
              InferSpewColor(this), this, InferSpewColorReset(),
              TypeString(type));

    /* Propagate the type to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        cx->compartment()->types.addPending(cx, constraint, this, type);
        constraint = constraint->next;
    }

    cx->compartment()->types.resolvePending(cx);
}

inline void
TypeSet::setOwnProperty(JSContext *cx, bool configured)
{
    TypeFlags nflags = TYPE_FLAG_OWN_PROPERTY | (configured ? TYPE_FLAG_CONFIGURED_PROPERTY : 0);

    if ((flags & nflags) == nflags)
        return;

    flags |= nflags;

    /* Propagate the change to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        constraint->newPropertyState(cx, this);
        constraint = constraint->next;
    }
}

inline unsigned
TypeSet::getObjectCount() const
{
    JS_ASSERT(!unknownObject());
    uint32_t count = baseObjectCount();
    if (count > SET_ARRAY_SIZE)
        return HashSetCapacity(count);
    return count;
}

inline TypeObjectKey *
TypeSet::getObject(unsigned i) const
{
    JS_ASSERT(i < getObjectCount());
    if (baseObjectCount() == 1) {
        JS_ASSERT(i == 0);
        return (TypeObjectKey *) objectSet;
    }
    return objectSet[i];
}

inline JSObject *
TypeSet::getSingleObject(unsigned i) const
{
    TypeObjectKey *key = getObject(i);
    return (uintptr_t(key) & 1) ? (JSObject *)(uintptr_t(key) ^ 1) : NULL;
}

inline TypeObject *
TypeSet::getTypeObject(unsigned i) const
{
    TypeObjectKey *key = getObject(i);
    return (key && !(uintptr_t(key) & 1)) ? (TypeObject *) key : NULL;
}

inline TypeObject *
TypeSet::getTypeOrSingleObject(JSContext *cx, unsigned i) const
{
    JS_ASSERT(cx->compartment()->activeAnalysis);
    TypeObject *type = getTypeObject(i);
    if (!type) {
        JSObject *singleton = getSingleObject(i);
        if (!singleton)
            return NULL;
        type = singleton->getType(cx);
        if (!type)
            cx->compartment()->types.setPendingNukeTypes(cx);
    }
    return type;
}

/////////////////////////////////////////////////////////////////////
// TypeCallsite
/////////////////////////////////////////////////////////////////////

inline
TypeCallsite::TypeCallsite(JSContext *cx, JSScript *script, jsbytecode *pc,
                           bool isNew, unsigned argumentCount)
    : script(script), pc(pc), isNew(isNew), argumentCount(argumentCount),
      thisTypes(NULL), returnTypes(NULL)
{
    /* Caller must check for failure. */
    argumentTypes = cx->analysisLifoAlloc().newArray<StackTypeSet*>(argumentCount);
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

inline TypeObject::TypeObject(Class *clasp, TaggedProto proto, bool function, bool unknown)
{
    mozilla::PodZero(this);

    /* Inner objects may not appear on prototype chains. */
    JS_ASSERT_IF(proto.isObject(), !proto.toObject()->getClass()->ext.outerObject);

    this->clasp = clasp;
    this->proto = proto.raw();

    if (function)
        flags |= OBJECT_FLAG_FUNCTION;
    if (unknown)
        flags |= OBJECT_FLAG_UNKNOWN_MASK;

    InferSpew(ISpewOps, "newObject: %s", TypeObjectString(this));
}

inline uint32_t
TypeObject::basePropertyCount() const
{
    return (flags & OBJECT_FLAG_PROPERTY_COUNT_MASK) >> OBJECT_FLAG_PROPERTY_COUNT_SHIFT;
}

inline void
TypeObject::setBasePropertyCount(uint32_t count)
{
    JS_ASSERT(count <= OBJECT_FLAG_PROPERTY_COUNT_LIMIT);
    flags = (flags & ~OBJECT_FLAG_PROPERTY_COUNT_MASK)
          | (count << OBJECT_FLAG_PROPERTY_COUNT_SHIFT);
}

inline HeapTypeSet *
TypeObject::getProperty(JSContext *cx, jsid id, bool own)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);

    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(!JSID_IS_EMPTY(id), id == IdToTypeId(id));
    JS_ASSERT(!unknownProperties());

    uint32_t propertyCount = basePropertyCount();
    Property **pprop = HashSetInsert<jsid,Property,Property>
        (cx->typeLifoAlloc(), propertySet, propertyCount, id);
    if (!pprop) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return NULL;
    }

    if (!*pprop) {
        setBasePropertyCount(propertyCount);
        if (!addProperty(cx, id, pprop)) {
            setBasePropertyCount(0);
            propertySet = NULL;
            return NULL;
        }
        if (propertyCount == OBJECT_FLAG_PROPERTY_COUNT_LIMIT) {
            markUnknown(cx);

            /*
             * Return an arbitrary property in the object, as all have unknown
             * type and are treated as configured.
             */
            unsigned count = getPropertyCount();
            for (unsigned i = 0; i < count; i++) {
                if (Property *prop = getProperty(i))
                    return &prop->types;
            }

            JS_NOT_REACHED("Missing property");
            return NULL;
        }
    }

    HeapTypeSet *types = &(*pprop)->types;
    if (own)
        types->setOwnProperty(cx, false);

    return types;
}

inline HeapTypeSet *
TypeObject::maybeGetProperty(jsid id, JSContext *cx)
{
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(!JSID_IS_EMPTY(id), id == IdToTypeId(id));
    JS_ASSERT(!unknownProperties());

    Property *prop = HashSetLookup<jsid,Property,Property>
        (propertySet, basePropertyCount(), id);

    return prop ? &prop->types : NULL;
}

inline unsigned
TypeObject::getPropertyCount()
{
    uint32_t count = basePropertyCount();
    if (count > SET_ARRAY_SIZE)
        return HashSetCapacity(count);
    return count;
}

inline Property *
TypeObject::getProperty(unsigned i)
{
    JS_ASSERT(i < getPropertyCount());
    if (basePropertyCount() == 1) {
        JS_ASSERT(i == 0);
        return (Property *) propertySet;
    }
    return propertySet[i];
}

inline void
TypeObject::writeBarrierPre(TypeObject *type)
{
#ifdef JSGC_INCREMENTAL
    if (!type || !type->runtime()->needsBarrier())
        return;

    JS::Zone *zone = type->zone();
    if (zone->needsBarrier()) {
        TypeObject *tmp = type;
        MarkTypeObjectUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == type);
    }
#endif
}

inline void
TypeObject::writeBarrierPost(TypeObject *type, void *addr)
{
}

inline void
TypeObject::readBarrier(TypeObject *type)
{
#ifdef JSGC_INCREMENTAL
    JS::Zone *zone = type->zone();
    if (zone->needsBarrier()) {
        TypeObject *tmp = type;
        MarkTypeObjectUnbarriered(zone->barrierTracer(), &tmp, "read barrier");
        JS_ASSERT(tmp == type);
    }
#endif
}

inline void
TypeNewScript::writeBarrierPre(TypeNewScript *newScript)
{
#ifdef JSGC_INCREMENTAL
    if (!newScript || !newScript->fun->runtime()->needsBarrier())
        return;

    JS::Zone *zone = newScript->fun->zone();
    if (zone->needsBarrier()) {
        MarkObject(zone->barrierTracer(), &newScript->fun, "write barrier");
        MarkShape(zone->barrierTracer(), &newScript->shape, "write barrier");
    }
#endif
}

inline void
TypeNewScript::writeBarrierPost(TypeNewScript *newScript, void *addr)
{
}

inline
Property::Property(jsid id)
  : id(id)
{
}

inline
Property::Property(const Property &o)
  : id(o.id.get()), types(o.types)
{
}

inline bool
HasOperationOverflowed(JSScript *script, jsbytecode *pc)
{
    types::TypeResult *result = script->types->dynamicList;
    while (result) {
        if (result->offset == uint32_t(pc - script->code)) {
            if (result->type == types::Type::DoubleType())
                return true;
        }
        result = result->next;
    }
    return false;
}

inline bool
IterationValuesMustBeStrings(JSScript *script)
{
    // Return true if no custom non-string-producing iterators have been used
    // in a 'for in' loop within the script.
    types::TypeResult *result = script->types->dynamicList;
    while (result) {
        if (result->offset == UINT32_MAX)
            return false;
        result = result->next;
    }
    return true;
}

} } /* namespace js::types */

inline bool
JSScript::ensureHasTypes(JSContext *cx)
{
    return types || makeTypes(cx);
}

inline bool
JSScript::ensureHasBytecodeTypeMap(JSContext *cx)
{
    return ensureHasTypes(cx) && (types->bytecodeMap || makeBytecodeTypeMap(cx));
}

inline bool
JSScript::ensureRanAnalysis(JSContext *cx)
{
    js::types::AutoEnterAnalysis aea(cx);

    if (!ensureHasTypes(cx))
        return false;
    if (!hasAnalysis() && !makeAnalysis(cx))
        return false;
    JS_ASSERT(analysis()->ranBytecode());
    return true;
}

inline bool
JSScript::ensureRanInference(JSContext *cx)
{
    if (!ensureRanAnalysis(cx))
        return false;
    if (!analysis()->ranInference()) {
        js::types::AutoEnterAnalysis enter(cx);
        analysis()->analyzeTypes(cx);
    }
    return !analysis()->OOM() && !cx->zone()->types.pendingNukeTypes;
}

inline bool
JSScript::hasAnalysis()
{
    return types && types->analysis;
}

inline js::analyze::ScriptAnalysis *
JSScript::analysis()
{
    JS_ASSERT(hasAnalysis());
    return types->analysis;
}

inline void
JSScript::clearAnalysis()
{
    if (types) {
        types->analysis = NULL;
        types->bytecodeMap = NULL;
    }
}

inline void
JSScript::clearPropertyReadTypes()
{
    if (types && types->propertyReadTypes)
        types->propertyReadTypes = NULL;
}

inline void
js::analyze::ScriptAnalysis::addPushedType(JSContext *cx, uint32_t offset, uint32_t which,
                                           js::types::Type type)
{
    js::types::TypeSet *pushed = pushedTypes(offset, which);
    pushed->addType(cx, type);
}

namespace js {

template <>
struct RootMethods<const types::Type>
{
    static types::Type initial() { return types::Type::UnknownType(); }
    static ThingRootKind kind() { return THING_ROOT_TYPE; }
    static bool poisoned(const types::Type &v) {
        return (v.isTypeObject() && IsPoisonedPtr(v.typeObject()))
            || (v.isSingleObject() && IsPoisonedPtr(v.singleObject()));
    }
};

template <>
struct RootMethods<types::Type>
{
    static types::Type initial() { return types::Type::UnknownType(); }
    static ThingRootKind kind() { return THING_ROOT_TYPE; }
    static bool poisoned(const types::Type &v) {
        return (v.isTypeObject() && IsPoisonedPtr(v.typeObject()))
            || (v.isSingleObject() && IsPoisonedPtr(v.singleObject()));
    }
};

} // namespace js

namespace JS {
template<> class AnchorPermitted<js::types::TypeObject *> { };
}  // namespace JS

#endif // jsinferinlines_h___
