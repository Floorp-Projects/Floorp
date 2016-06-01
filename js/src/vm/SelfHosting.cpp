/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/SelfHosting.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Maybe.h"

#include "jsarray.h"
#include "jscntxt.h"
#include "jscompartment.h"
#include "jsdate.h"
#include "jsfriendapi.h"
#include "jsfun.h"
#include "jshashutil.h"
#include "jsstr.h"
#include "jsweakmap.h"
#include "jswrapper.h"
#include "selfhosted.out.h"

#include "builtin/Intl.h"
#include "builtin/MapObject.h"
#include "builtin/ModuleObject.h"
#include "builtin/Object.h"
#include "builtin/Promise.h"
#include "builtin/Reflect.h"
#include "builtin/SelfHostingDefines.h"
#include "builtin/SIMD.h"
#include "builtin/TypedObject.h"
#include "builtin/WeakSetObject.h"
#include "gc/Marking.h"
#include "gc/Policy.h"
#include "jit/AtomicOperations.h"
#include "jit/InlinableNatives.h"
#include "js/Date.h"
#include "vm/Compression.h"
#include "vm/GeneratorObject.h"
#include "vm/Interpreter.h"
#include "vm/RegExpObject.h"
#include "vm/String.h"
#include "vm/TypedArrayCommon.h"
#include "vm/WrapperObject.h"

#include "jsfuninlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/BooleanObject-inl.h"
#include "vm/NativeObject-inl.h"
#include "vm/NumberObject-inl.h"
#include "vm/StringObject-inl.h"

using namespace js;
using namespace js::selfhosted;

using JS::AutoCheckCannotGC;
using mozilla::IsInRange;
using mozilla::Maybe;
using mozilla::PodMove;
using mozilla::Maybe;

static void
selfHosting_WarningReporter(JSContext* cx, const char* message, JSErrorReport* report)
{
    MOZ_ASSERT(report);
    MOZ_ASSERT(JSREPORT_IS_WARNING(report->flags));

    PrintError(cx, stderr, message, report, true);
}

static bool
intrinsic_ToObject(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedValue val(cx, args[0]);
    RootedObject obj(cx, ToObject(cx, val));
    if (!obj)
        return false;
    args.rval().setObject(*obj);
    return true;
}

static bool
intrinsic_IsObject(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    Value val = args[0];
    bool isObject = val.isObject();
    args.rval().setBoolean(isObject);
    return true;
}

static bool
intrinsic_IsArray(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedValue val(cx, args[0]);
    if (val.isObject()) {
        RootedObject obj(cx, &val.toObject());
        bool isArray = false;
        if (!IsArray(cx, obj, &isArray))
            return false;
        args.rval().setBoolean(isArray);
    } else {
        args.rval().setBoolean(false);
    }
    return true;
}

static bool
intrinsic_IsWrappedArrayConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    bool result = false;
    if (!IsWrappedArrayConstructor(cx, args[0], &result))
        return false;
    args.rval().setBoolean(result);
    return true;
}

static bool
intrinsic_ToInteger(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    double result;
    if (!ToInteger(cx, args[0], &result))
        return false;
    args.rval().setNumber(result);
    return true;
}

static bool
intrinsic_ToString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedString str(cx);
    str = ToString<CanGC>(cx, args[0]);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
intrinsic_ToPropertyKey(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedId id(cx);
    if (!ToPropertyKey(cx, args[0], &id))
        return false;

    args.rval().set(IdToValue(id));
    return true;
}

static bool
intrinsic_IsCallable(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(IsCallable(args[0]));
    return true;
}

static bool
intrinsic_IsConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setBoolean(IsConstructor(args[0]));
    return true;
}

/**
 * Intrinsic for calling a wrapped self-hosted function without invoking the
 * wrapper's security checks.
 *
 * Takes a wrapped function as the first and the receiver object as the
 * second argument. Any additional arguments are passed on to the unwrapped
 * function.
 *
 * Xray wrappers prevent lower-privileged code from passing objects to wrapped
 * functions from higher-privileged realms. In some cases, this check is too
 * strict, so this intrinsic allows getting around it.
 *
 * Note that it's not possible to replace all usages with dedicated intrinsics
 * as the function in question might be an inner function that closes over
 * state relevant to its execution.
 *
 * Right now, this is used for the Promise implementation to enable creating
 * resolution functions for xrayed Promises in the privileged realm and then
 * creating the Promise instance in the non-privileged one. The callbacks have
 * to be called by non-privileged code in various places, in many cases
 * passing objects as arguments.
 */
static bool
intrinsic_UnsafeCallWrappedFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() >= 2);
    MOZ_ASSERT(IsCallable(args[0]));
    MOZ_ASSERT(IsWrapper(&args[0].toObject()));
    MOZ_ASSERT(args[1].isObject() || args[1].isUndefined());

    MOZ_RELEASE_ASSERT(args[0].isObject());
    RootedObject wrappedFun(cx, &args[0].toObject());
    RootedObject fun(cx, UncheckedUnwrap(wrappedFun));
    MOZ_RELEASE_ASSERT(fun->is<JSFunction>());
    MOZ_RELEASE_ASSERT(fun->as<JSFunction>().isSelfHostedBuiltin());

    InvokeArgs args2(cx);
    if (!args2.init(args.length() - 2))
        return false;

    args2.setThis(args[1]);

    for (size_t i = 0; i < args2.length(); i++)
        args2[i].set(args[i + 2]);

    AutoWaivePolicy waivePolicy(cx, wrappedFun, JSID_VOIDHANDLE, BaseProxyHandler::CALL);
    if (!CrossCompartmentWrapper::singleton.call(cx, wrappedFun, args2))
        return false;
    args.rval().set(args2.rval());
    return true;
}

template<typename T>
static bool
intrinsic_IsInstanceOfBuiltin(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    args.rval().setBoolean(args[0].toObject().is<T>());
    return true;
}

/**
 * Self-hosting intrinsic returning the original constructor for a builtin
 * the name of which is the first and only argument.
 *
 * The return value is guaranteed to be the original constructor even if
 * content code changed the named binding on the global object.
 *
 * This intrinsic shouldn't be called directly. Instead, the
 * `GetBuiltinConstructor` and `GetBuiltinPrototype` helper functions in
 * Utilities.js should be used, as they cache results, improving performance.
 */
static bool
intrinsic_GetBuiltinConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedString str(cx, args[0].toString());
    JSAtom* atom;
    if (str->isAtom()) {
        atom = &str->asAtom();
    } else {
        atom = AtomizeString(cx, str);
        if (!atom)
            return false;
    }
    RootedId id(cx, AtomToId(atom));
    JSProtoKey key = JS_IdToProtoKey(cx, id);
    MOZ_ASSERT(key != JSProto_Null);
    RootedObject ctor(cx);
    if (!GetBuiltinConstructor(cx, key, &ctor))
        return false;
    args.rval().setObject(*ctor);
    return true;
}

static bool
intrinsic_SubstringKernel(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args[0].isString());
    MOZ_ASSERT(args[1].isInt32());
    MOZ_ASSERT(args[2].isInt32());

    RootedString str(cx, args[0].toString());
    int32_t begin = args[1].toInt32();
    int32_t length = args[2].toInt32();

    JSString* substr = SubstringKernel(cx, str, begin, length);
    if (!substr)
        return false;

    args.rval().setString(substr);
    return true;
}

static bool
intrinsic_OwnPropertyKeys(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isInt32());
    return GetOwnPropertyKeys(cx, args, args[1].toInt32());
}

static void
ThrowErrorWithType(JSContext* cx, JSExnType type, const CallArgs& args)
{
    uint32_t errorNumber = args[0].toInt32();

#ifdef DEBUG
    const JSErrorFormatString* efs = GetErrorMessage(nullptr, errorNumber);
    MOZ_ASSERT(efs->argCount == args.length() - 1);
    MOZ_ASSERT(efs->exnType == type, "error-throwing intrinsic and error number are inconsistent");
#endif

    JSAutoByteString errorArgs[3];
    for (unsigned i = 1; i < 4 && i < args.length(); i++) {
        RootedValue val(cx, args[i]);
        if (val.isInt32()) {
            JSString* str = ToString<CanGC>(cx, val);
            if (!str)
                return;
            errorArgs[i - 1].encodeLatin1(cx, str);
        } else if (val.isString()) {
            errorArgs[i - 1].encodeLatin1(cx, val.toString());
        } else {
            UniqueChars bytes = DecompileValueGenerator(cx, JSDVG_SEARCH_STACK, val, nullptr);
            if (!bytes)
                return;
            errorArgs[i - 1].initBytes(bytes.release());
        }
        if (!errorArgs[i - 1])
            return;
    }

    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, errorNumber,
                         errorArgs[0].ptr(), errorArgs[1].ptr(), errorArgs[2].ptr());
}

static bool
intrinsic_ThrowRangeError(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() >= 1);

    ThrowErrorWithType(cx, JSEXN_RANGEERR, args);
    return false;
}

static bool
intrinsic_ThrowTypeError(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() >= 1);

    ThrowErrorWithType(cx, JSEXN_TYPEERR, args);
    return false;
}

static bool
intrinsic_ThrowSyntaxError(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() >= 1);

    ThrowErrorWithType(cx, JSEXN_SYNTAXERR, args);
    return false;
}

/**
 * Handles an assertion failure in self-hosted code just like an assertion
 * failure in C++ code. Information about the failure can be provided in args[0].
 */
static bool
intrinsic_AssertionFailed(JSContext* cx, unsigned argc, Value* vp)
{
#ifdef DEBUG
    CallArgs args = CallArgsFromVp(argc, vp);
    if (args.length() > 0) {
        // try to dump the informative string
        JSString* str = ToString<CanGC>(cx, args[0]);
        if (str) {
            fprintf(stderr, "Self-hosted JavaScript assertion info: ");
            str->dumpCharsNoNewline();
            fputc('\n', stderr);
        }
    }
#endif
    MOZ_ASSERT(false);
    return false;
}

/**
 * Dumps a message to stderr, after stringifying it. Doesn't append a newline.
 */
static bool
intrinsic_DumpMessage(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
#ifdef DEBUG
    if (args.length() > 0) {
        // try to dump the informative string
        JSString* str = ToString<CanGC>(cx, args[0]);
        if (str) {
            str->dumpCharsNoNewline();
            fputc('\n', stderr);
        } else {
            cx->recoverFromOutOfMemory();
        }
    }
#endif
    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_MakeConstructible(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[0].toObject().is<JSFunction>());
    MOZ_ASSERT(args[0].toObject().as<JSFunction>().isSelfHostedBuiltin());
    MOZ_ASSERT(args[1].isObjectOrNull());

    // Normal .prototype properties aren't enumerable.  But for this to clone
    // correctly, it must be enumerable.
    RootedObject ctor(cx, &args[0].toObject());
    if (!DefineProperty(cx, ctor, cx->names().prototype, args[1],
                        nullptr, nullptr,
                        JSPROP_READONLY | JSPROP_ENUMERATE | JSPROP_PERMANENT))
    {
        return false;
    }

    ctor->as<JSFunction>().setIsConstructor();
    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_MakeDefaultConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].toObject().as<JSFunction>().isSelfHostedBuiltin());

    RootedFunction ctor(cx, &args[0].toObject().as<JSFunction>());

    ctor->nonLazyScript()->setIsDefaultClassConstructor();

    args.rval().setUndefined();
    return true;
}

/*
 * Used to mark bound functions as such and make them constructible if the
 * target is.
 * Also sets the name and correct length, both of which are more costly to
 * do in JS.
 */
static bool
intrinsic_FinishBoundFunctionInit(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 4);
    MOZ_ASSERT(IsCallable(args[1]));
    MOZ_ASSERT(args[2].isNumber());
    MOZ_ASSERT(args[3].isString());

    RootedFunction bound(cx, &args[0].toObject().as<JSFunction>());
    bound->setIsBoundFunction();
    RootedObject targetObj(cx, &args[1].toObject());
    MOZ_ASSERT(bound->getBoundFunctionTarget() == targetObj);
    if (targetObj->isConstructor())
        bound->setIsConstructor();

    // 9.4.1.3 BoundFunctionCreate, Steps 2-3., 8.
    RootedObject proto(cx);
    if (!GetPrototype(cx, targetObj, &proto))
        return false;

    if (bound->staticPrototype() != proto) {
        if (!SetPrototype(cx, bound, proto))
            return false;
    }

    bound->setExtendedSlot(BOUND_FUN_LENGTH_SLOT, args[2]);
    MOZ_ASSERT(!bound->hasGuessedAtom());

    // 9.2.11 SetFunctionName, Step 6.
    RootedAtom name(cx, AtomizeString(cx, args[3].toString()));
    if (!name)
        return false;
    bound->setAtom(name);

    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_SetPrototype(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isObjectOrNull());

    RootedObject obj(cx, &args[0].toObject());
    RootedObject proto(cx, args[1].toObjectOrNull());
    if (!SetPrototype(cx, obj, proto))
        return false;

    args.rval().setUndefined();
    return true;
}

/*
 * Used to decompile values in the nearest non-builtin stack frame, falling
 * back to decompiling in the current frame. Helpful for printing higher-order
 * function arguments.
 *
 * The user must supply the argument number of the value in question; it
 * _cannot_ be automatically determined.
 */
static bool
intrinsic_DecompileArg(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);

    RootedValue value(cx, args[1]);
    ScopedJSFreePtr<char> str(DecompileArgument(cx, args[0].toInt32(), value));
    if (!str)
        return false;
    RootedAtom atom(cx, Atomize(cx, str, strlen(str)));
    if (!atom)
        return false;
    args.rval().setString(atom);
    return true;
}

static bool
intrinsic_DefineDataProperty(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    MOZ_ASSERT(args.length() >= 3);
    MOZ_ASSERT(args[0].isObject());

    RootedObject obj(cx, &args[0].toObject());
    RootedId id(cx);
    if (!ValueToId<CanGC>(cx, args[1], &id))
        return false;
    RootedValue value(cx, args[2]);

    unsigned attrs = 0;
    if (args.length() >= 4) {
        unsigned attributes = args[3].toInt32();

        MOZ_ASSERT(bool(attributes & ATTR_ENUMERABLE) != bool(attributes & ATTR_NONENUMERABLE),
                   "_DefineDataProperty must receive either ATTR_ENUMERABLE xor ATTR_NONENUMERABLE");
        if (attributes & ATTR_ENUMERABLE)
            attrs |= JSPROP_ENUMERATE;

        MOZ_ASSERT(bool(attributes & ATTR_CONFIGURABLE) != bool(attributes & ATTR_NONCONFIGURABLE),
                   "_DefineDataProperty must receive either ATTR_CONFIGURABLE xor "
                   "ATTR_NONCONFIGURABLE");
        if (attributes & ATTR_NONCONFIGURABLE)
            attrs |= JSPROP_PERMANENT;

        MOZ_ASSERT(bool(attributes & ATTR_WRITABLE) != bool(attributes & ATTR_NONWRITABLE),
                   "_DefineDataProperty must receive either ATTR_WRITABLE xor ATTR_NONWRITABLE");
        if (attributes & ATTR_NONWRITABLE)
            attrs |= JSPROP_READONLY;
    } else {
        // If the fourth argument is unspecified, the attributes are for a
        // plain data property.
        attrs = JSPROP_ENUMERATE;
    }

    Rooted<PropertyDescriptor> desc(cx);
    desc.setDataDescriptor(value, attrs);
    if (!DefineProperty(cx, obj, id, desc))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_ObjectHasPrototype(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    RootedObject obj(cx, &args[0].toObject());
    RootedObject proto(cx, &args[1].toObject());

    RootedObject actualProto(cx);
    if (!GetPrototype(cx, obj, &actualProto))
        return false;

    args.rval().setBoolean(actualProto == proto);
    return true;
}

static bool
intrinsic_UnsafeSetReservedSlot(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isInt32());

    args[0].toObject().as<NativeObject>().setReservedSlot(args[1].toPrivateUint32(), args[2]);
    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_UnsafeGetReservedSlot(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[1].isInt32());

    args.rval().set(args[0].toObject().as<NativeObject>().getReservedSlot(args[1].toPrivateUint32()));
    return true;
}

static bool
intrinsic_UnsafeGetObjectFromReservedSlot(JSContext* cx, unsigned argc, Value* vp)
{
    if (!intrinsic_UnsafeGetReservedSlot(cx, argc, vp))
        return false;
    MOZ_ASSERT(vp->isObject());
    return true;
}

static bool
intrinsic_UnsafeGetInt32FromReservedSlot(JSContext* cx, unsigned argc, Value* vp)
{
    if (!intrinsic_UnsafeGetReservedSlot(cx, argc, vp))
        return false;
    MOZ_ASSERT(vp->isInt32());
    return true;
}

static bool
intrinsic_UnsafeGetStringFromReservedSlot(JSContext* cx, unsigned argc, Value* vp)
{
    if (!intrinsic_UnsafeGetReservedSlot(cx, argc, vp))
        return false;
    MOZ_ASSERT(vp->isString());
    return true;
}

static bool
intrinsic_UnsafeGetBooleanFromReservedSlot(JSContext* cx, unsigned argc, Value* vp)
{
    if (!intrinsic_UnsafeGetReservedSlot(cx, argc, vp))
        return false;
    MOZ_ASSERT(vp->isBoolean());
    return true;
}

/**
 * Intrinsic for creating an empty array in the compartment of the object
 * passed as the first argument.
 *
 * Returns the array, wrapped in the default wrapper to use between the two
 * compartments.
 */
static bool
intrinsic_NewArrayInCompartment(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedObject wrapped(cx, &args[0].toObject());
    MOZ_ASSERT(IsWrapper(wrapped));
    RootedObject obj(cx, UncheckedUnwrap(wrapped));

    RootedArrayObject arr(cx);
    {
        AutoCompartment ac(cx, obj);
        arr = NewDenseEmptyArray(cx);
        if (!arr)
            return false;
    }

    args.rval().setObject(*arr);
    return wrapped->compartment()->wrap(cx, args.rval());
}

static bool
intrinsic_IsPackedArray(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());
    args.rval().setBoolean(IsPackedArray(&args[0].toObject()));
    return true;
}

static bool
intrinsic_GetIteratorPrototype(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    JSObject* obj = GlobalObject::getOrCreateIteratorPrototype(cx, cx->global());
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
intrinsic_NewArrayIterator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedObject proto(cx, GlobalObject::getOrCreateArrayIteratorPrototype(cx, cx->global()));
    if (!proto)
        return false;

    JSObject* obj = NewObjectWithGivenProto(cx, &ArrayIteratorObject::class_, proto);
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
intrinsic_GetNextMapEntryForIterator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].toObject().is<MapIteratorObject>());
    MOZ_ASSERT(args[1].isObject());

    Rooted<MapIteratorObject*> mapIterator(cx, &args[0].toObject().as<MapIteratorObject>());
    RootedArrayObject result(cx, &args[1].toObject().as<ArrayObject>());

    args.rval().setBoolean(MapIteratorObject::next(cx, mapIterator, result));
    return true;
}

static bool
intrinsic_CreateMapIterationResultPair(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedObject result(cx, MapIteratorObject::createResultPair(cx));
    if (!result)
        return false;

    args.rval().setObject(*result);
    return true;
}

static bool
intrinsic_NewStringIterator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedObject proto(cx, GlobalObject::getOrCreateStringIteratorPrototype(cx, cx->global()));
    if (!proto)
        return false;

    JSObject* obj = NewObjectWithGivenProto(cx, &StringIteratorObject::class_, proto);
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
intrinsic_NewListIterator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedObject proto(cx, GlobalObject::getOrCreateIteratorPrototype(cx, cx->global()));
    if (!proto)
        return false;

    RootedObject iterator(cx);
    iterator = NewObjectWithGivenProto(cx, &ListIteratorObject::class_, proto);
    if (!iterator)
        return false;

    args.rval().setObject(*iterator);
    return true;
}

static bool
intrinsic_ActiveFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    ScriptFrameIter iter(cx);
    MOZ_ASSERT(iter.isFunctionFrame());
    args.rval().setObject(*iter.callee(cx));
    return true;
}

static bool
intrinsic_SetCanonicalName(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);

    RootedFunction fun(cx, &args[0].toObject().as<JSFunction>());
    MOZ_ASSERT(fun->isSelfHostedBuiltin());
    RootedAtom atom(cx, AtomizeString(cx, args[1].toString()));
    if (!atom)
        return false;

    fun->setAtom(atom);
#ifdef DEBUG
    fun->setExtendedSlot(HAS_SELFHOSTED_CANONICAL_NAME_SLOT, BooleanValue(true));
#endif
    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_StarGeneratorObjectIsClosed(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    StarGeneratorObject* genObj = &args[0].toObject().as<StarGeneratorObject>();
    args.rval().setBoolean(genObj->isClosed());
    return true;
}

bool
js::intrinsic_IsSuspendedStarGenerator(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    if (!args[0].isObject() || !args[0].toObject().is<StarGeneratorObject>()) {
        args.rval().setBoolean(false);
        return true;
    }

    StarGeneratorObject& genObj = args[0].toObject().as<StarGeneratorObject>();
    args.rval().setBoolean(!genObj.isClosed() && genObj.isSuspended());
    return true;
}

static bool
intrinsic_LegacyGeneratorObjectIsClosed(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    LegacyGeneratorObject* genObj = &args[0].toObject().as<LegacyGeneratorObject>();
    args.rval().setBoolean(genObj->isClosed());
    return true;
}

static bool
intrinsic_CloseClosingLegacyGeneratorObject(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    LegacyGeneratorObject* genObj = &args[0].toObject().as<LegacyGeneratorObject>();
    MOZ_ASSERT(genObj->isClosing());
    genObj->setClosed();
    return true;
}

static bool
intrinsic_ThrowStopIteration(JSContext* cx, unsigned argc, Value* vp)
{
    MOZ_ASSERT(CallArgsFromVp(argc, vp).length() == 0);

    return ThrowStopIteration(cx);
}

static bool
intrinsic_GeneratorIsRunning(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    GeneratorObject* genObj = &args[0].toObject().as<GeneratorObject>();
    args.rval().setBoolean(genObj->isRunning() || genObj->isClosing());
    return true;
}

static bool
intrinsic_GeneratorSetClosed(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    GeneratorObject* genObj = &args[0].toObject().as<GeneratorObject>();
    genObj->setClosed();
    return true;
}

static bool
intrinsic_IsWrappedArrayBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    if (!args[0].isObject()) {
        args.rval().setBoolean(false);
        return true;
    }

    JSObject* obj = &args[0].toObject();
    if (!obj->is<WrapperObject>()) {
        args.rval().setBoolean(false);
        return true;
    }

    JSObject* unwrapped = CheckedUnwrap(obj);
    if (!unwrapped) {
        JS_ReportError(cx, "Permission denied to access object");
        return false;
    }

    args.rval().setBoolean(unwrapped->is<ArrayBufferObject>());
    return true;
}

static bool
intrinsic_ArrayBufferByteLength(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());
    MOZ_ASSERT(args[0].toObject().is<ArrayBufferObject>());

    size_t byteLength = args[0].toObject().as<ArrayBufferObject>().byteLength();
    args.rval().setInt32(mozilla::AssertedCast<int32_t>(byteLength));
    return true;
}

static bool
intrinsic_PossiblyWrappedArrayBufferByteLength(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    JSObject* obj = CheckedUnwrap(&args[0].toObject());
    if (!obj) {
        JS_ReportError(cx, "Permission denied to access object");
        return false;
    }

    uint32_t length = obj->as<ArrayBufferObject>().byteLength();
    args.rval().setInt32(mozilla::AssertedCast<int32_t>(length));
    return true;
}

static bool
intrinsic_ArrayBufferCopyData(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 5);

    bool isWrapped = args[4].toBoolean();
    Rooted<ArrayBufferObject*> toBuffer(cx);
    if (!isWrapped) {
        toBuffer = &args[0].toObject().as<ArrayBufferObject>();
    } else {
        JSObject* wrapped = &args[0].toObject();
        MOZ_ASSERT(wrapped->is<WrapperObject>());
        RootedObject toBufferObj(cx, CheckedUnwrap(wrapped));
        if (!toBufferObj) {
            JS_ReportError(cx, "Permission denied to access object");
            return false;
        }
        toBuffer = toBufferObj.as<ArrayBufferObject>();
    }
    Rooted<ArrayBufferObject*> fromBuffer(cx, &args[1].toObject().as<ArrayBufferObject>());
    uint32_t fromIndex = uint32_t(args[2].toInt32());
    uint32_t count = uint32_t(args[3].toInt32());

    ArrayBufferObject::copyData(toBuffer, fromBuffer, fromIndex, count);

    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_IsSpecificTypedArray(JSContext* cx, unsigned argc, Value* vp, Scalar::Type type)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    JSObject* obj = &args[0].toObject();

    bool isArray = JS_GetArrayBufferViewType(obj) == type;

    args.rval().setBoolean(isArray);
    return true;
}

static bool
intrinsic_IsUint8TypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    return intrinsic_IsSpecificTypedArray(cx, argc, vp, Scalar::Uint8) ||
           intrinsic_IsSpecificTypedArray(cx, argc, vp, Scalar::Uint8Clamped);
}

static bool
intrinsic_IsInt8TypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    return intrinsic_IsSpecificTypedArray(cx, argc, vp, Scalar::Int8);
}

static bool
intrinsic_IsUint16TypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    return intrinsic_IsSpecificTypedArray(cx, argc, vp, Scalar::Uint16);
}

static bool
intrinsic_IsInt16TypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    return intrinsic_IsSpecificTypedArray(cx, argc, vp, Scalar::Int16);
}

static bool
intrinsic_IsUint32TypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    return intrinsic_IsSpecificTypedArray(cx, argc, vp, Scalar::Uint32);
}

static bool
intrinsic_IsInt32TypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    return intrinsic_IsSpecificTypedArray(cx, argc, vp, Scalar::Int32);
}

static bool
intrinsic_IsFloat32TypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    return intrinsic_IsSpecificTypedArray(cx, argc, vp, Scalar::Float32);
}

static bool
intrinsic_IsPossiblyWrappedTypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    bool isTypedArray = false;
    if (args[0].isObject()) {
        JSObject* obj = CheckedUnwrap(&args[0].toObject());
        if (!obj) {
            JS_ReportError(cx, "Permission denied to access object");
            return false;
        }

        isTypedArray = obj->is<TypedArrayObject>();
    }

    args.rval().setBoolean(isTypedArray);
    return true;
}

static bool
intrinsic_TypedArrayBuffer(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(TypedArrayObject::is(args[0]));

    Rooted<TypedArrayObject*> tarray(cx, &args[0].toObject().as<TypedArrayObject>());
    if (!TypedArrayObject::ensureHasBuffer(cx, tarray))
        return false;

    args.rval().set(TypedArrayObject::bufferValue(tarray));
    return true;
}

static bool
intrinsic_TypedArrayByteOffset(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(TypedArrayObject::is(args[0]));

    args.rval().set(TypedArrayObject::byteOffsetValue(&args[0].toObject().as<TypedArrayObject>()));
    return true;
}

static bool
intrinsic_TypedArrayElementShift(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(TypedArrayObject::is(args[0]));

    unsigned shift = TypedArrayShift(args[0].toObject().as<TypedArrayObject>().type());
    MOZ_ASSERT(shift == 0 || shift == 1 || shift == 2 || shift == 3);

    args.rval().setInt32(mozilla::AssertedCast<int32_t>(shift));
    return true;
}

// Return the value of [[ArrayLength]] internal slot of the TypedArray
static bool
intrinsic_TypedArrayLength(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    RootedObject obj(cx, &args[0].toObject());
    MOZ_ASSERT(obj->is<TypedArrayObject>());
    args.rval().setInt32(obj->as<TypedArrayObject>().length());
    return true;
}

static bool
intrinsic_PossiblyWrappedTypedArrayLength(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    JSObject* obj = CheckedUnwrap(&args[0].toObject());

    if (!obj) {
        JS_ReportError(cx, "Permission denied to access object");
        return false;
    }

    MOZ_ASSERT(obj->is<TypedArrayObject>());
    uint32_t typedArrayLength = obj->as<TypedArrayObject>().length();
    args.rval().setInt32(mozilla::AssertedCast<int32_t>(typedArrayLength));
    return true;
}

static bool
intrinsic_MoveTypedArrayElements(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 4);

    Rooted<TypedArrayObject*> tarray(cx, &args[0].toObject().as<TypedArrayObject>());
    uint32_t to = uint32_t(args[1].toInt32());
    uint32_t from = uint32_t(args[2].toInt32());
    uint32_t count = uint32_t(args[3].toInt32());

    MOZ_ASSERT(count > 0,
               "don't call this method if copying no elements, because then "
               "the not-detached requirement is wrong");

    if (tarray->hasDetachedBuffer()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
        return false;
    }

    // Don't multiply by |tarray->bytesPerElement()| in case the compiler can't
    // strength-reduce multiplication by 1/2/4/8 into the equivalent shift.
    const size_t ElementShift = TypedArrayShift(tarray->type());

    MOZ_ASSERT((UINT32_MAX >> ElementShift) > to);
    uint32_t byteDest = to << ElementShift;

    MOZ_ASSERT((UINT32_MAX >> ElementShift) > from);
    uint32_t byteSrc = from << ElementShift;

    MOZ_ASSERT((UINT32_MAX >> ElementShift) >= count);
    uint32_t byteSize = count << ElementShift;

#ifdef DEBUG
    {
        uint32_t viewByteLength = tarray->byteLength();
        MOZ_ASSERT(byteSize <= viewByteLength);
        MOZ_ASSERT(byteDest < viewByteLength);
        MOZ_ASSERT(byteSrc < viewByteLength);
        MOZ_ASSERT(byteDest <= viewByteLength - byteSize);
        MOZ_ASSERT(byteSrc <= viewByteLength - byteSize);
    }
#endif

    SharedMem<uint8_t*> data = tarray->viewDataEither().cast<uint8_t*>();
    jit::AtomicOperations::memmoveSafeWhenRacy(data + byteDest, data + byteSrc, byteSize);

    args.rval().setUndefined();
    return true;
}

// Extract the TypedArrayObject* underlying |obj| and return it.  This method,
// in a TOTALLY UNSAFE manner, completely violates the normal compartment
// boundaries, returning an object not necessarily in the current compartment
// or in |obj|'s compartment.
//
// All callers of this method are expected to sigil this TypedArrayObject*, and
// all values and information derived from it, with an "unsafe" prefix, to
// indicate the extreme caution required when dealing with such values.
//
// If calling code discipline ever fails to be maintained, it's gonna have a
// bad time.
static TypedArrayObject*
DangerouslyUnwrapTypedArray(JSContext* cx, JSObject* obj)
{
    // An unwrapped pointer to an object potentially on the other side of a
    // compartment boundary!  Isn't this such fun?
    JSObject* unwrapped = CheckedUnwrap(obj);
    if (!unwrapped->is<TypedArrayObject>()) {
        // By *appearances* this can't happen, as self-hosted TypedArraySet
        // checked this.  But.  Who's to say a GC couldn't happen between
        // the check that this value was a typed array, and this extraction
        // occurring?  A GC might turn a cross-compartment wrapper |obj| into
        // |unwrapped == obj|, a dead object no longer connected its typed
        // array.
        //
        // Yeah, yeah, it's pretty unlikely.  Are you willing to stake a
        // sec-critical bug on that assessment, now and forever, against
        // all changes those pesky GC and JIT people might make?
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_DEAD_OBJECT);
        return nullptr;
    }

    // Be super-duper careful using this, as we've just punched through
    // the compartment boundary, and things like buffer() on this aren't
    // same-compartment with anything else in the calling method.
    return &unwrapped->as<TypedArrayObject>();
}

// ES6 draft 20150403 22.2.3.22.2, steps 12-24, 29.
static bool
intrinsic_SetFromTypedArrayApproach(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 4);

    Rooted<TypedArrayObject*> target(cx, &args[0].toObject().as<TypedArrayObject>());
    MOZ_ASSERT(!target->hasDetachedBuffer(),
               "something should have defended against a target viewing a "
               "detached buffer");

    // As directed by |DangerouslyUnwrapTypedArray|, sigil this pointer and all
    // variables derived from it to counsel extreme caution here.
    Rooted<TypedArrayObject*> unsafeTypedArrayCrossCompartment(cx);
    unsafeTypedArrayCrossCompartment = DangerouslyUnwrapTypedArray(cx, &args[1].toObject());
    if (!unsafeTypedArrayCrossCompartment)
        return false;

    double doubleTargetOffset = args[2].toNumber();
    MOZ_ASSERT(doubleTargetOffset >= 0, "caller failed to ensure |targetOffset >= 0|");

    uint32_t targetLength = uint32_t(args[3].toInt32());

    // Handle all checks preceding the actual element-setting.  A visual skim
    // of 22.2.3.22.2 should confirm these are the only steps after steps 1-11
    // that might abort processing (other than for reason of internal error.)

    // Steps 12-13.
    if (unsafeTypedArrayCrossCompartment->hasDetachedBuffer()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_TYPED_ARRAY_DETACHED);
        return false;
    }

    // Steps 21, 23.
    uint32_t unsafeSrcLengthCrossCompartment = unsafeTypedArrayCrossCompartment->length();
    if (unsafeSrcLengthCrossCompartment + doubleTargetOffset > targetLength) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_BAD_INDEX);
        return false;
    }

    // Now that that's confirmed, we can use |targetOffset| of a sane type.
    uint32_t targetOffset = uint32_t(doubleTargetOffset);

    // The remaining steps are unobservable *except* through their effect on
    // which elements are copied and how.

    Scalar::Type targetType = target->type();
    Scalar::Type unsafeSrcTypeCrossCompartment = unsafeTypedArrayCrossCompartment->type();

    size_t targetElementSize = TypedArrayElemSize(targetType);
    SharedMem<uint8_t*> targetData =
        target->viewDataEither().cast<uint8_t*>() + targetOffset * targetElementSize;

    SharedMem<uint8_t*> unsafeSrcDataCrossCompartment =
        unsafeTypedArrayCrossCompartment->viewDataEither().cast<uint8_t*>();

    uint32_t unsafeSrcElementSizeCrossCompartment =
        TypedArrayElemSize(unsafeSrcTypeCrossCompartment);
    uint32_t unsafeSrcByteLengthCrossCompartment =
        unsafeSrcLengthCrossCompartment * unsafeSrcElementSizeCrossCompartment;

    // Step 29.
    //
    // The same-type case requires exact copying preserving the bit-level
    // encoding of the source data, so move the values.  (We could PodCopy if
    // we knew the buffers differed, but it's doubtful the work to check
    // wouldn't swap any minor wins PodCopy would afford.  Because of the
    // TOTALLY UNSAFE CROSS-COMPARTMENT NONSENSE here, comparing buffer
    // pointers directly could give an incorrect answer.)  If this occurs,
    // the %TypedArray%.prototype.set operation is completely finished.
    if (targetType == unsafeSrcTypeCrossCompartment) {
        jit::AtomicOperations::memmoveSafeWhenRacy(targetData,
                                                   unsafeSrcDataCrossCompartment,
                                                   unsafeSrcByteLengthCrossCompartment);
        args.rval().setInt32(JS_SETTYPEDARRAY_SAME_TYPE);
        return true;
    }

    // Every other bit of element-copying is handled by step 28.  Indicate
    // whether such copying must take care not to overlap, so that self-hosted
    // code may correctly perform the copying.

    SharedMem<uint8_t*> unsafeSrcDataLimitCrossCompartment =
        unsafeSrcDataCrossCompartment + unsafeSrcByteLengthCrossCompartment;
    SharedMem<uint8_t*> targetDataLimit =
        target->viewDataEither().cast<uint8_t*>() + targetLength * targetElementSize;

    // Step 24 test (but not steps 24a-d -- the caller handles those).
    bool overlap =
        IsInRange(targetData.unwrap(/*safe - used for ptr value*/),
                  unsafeSrcDataCrossCompartment.unwrap(/*safe - ditto*/),
                  unsafeSrcDataLimitCrossCompartment.unwrap(/*safe - ditto*/)) ||
        IsInRange(unsafeSrcDataCrossCompartment.unwrap(/*safe - ditto*/),
                  targetData.unwrap(/*safe - ditto*/),
                  targetDataLimit.unwrap(/*safe - ditto*/));

    args.rval().setInt32(overlap ? JS_SETTYPEDARRAY_OVERLAPPING : JS_SETTYPEDARRAY_DISJOINT);
    return true;
}

template <typename From, typename To>
static void
CopyValues(SharedMem<To*> dest, SharedMem<From*> src, uint32_t count)
{
#ifdef DEBUG
    void* destVoid = dest.template cast<void*>().unwrap(/*safe - used for ptr value*/);
    void* destVoidEnd = (dest + count).template cast<void*>().unwrap(/*safe - ditto*/);
    const void* srcVoid = src.template cast<void*>().unwrap(/*safe - ditto*/);
    const void* srcVoidEnd = (src + count).template cast<void*>().unwrap(/*safe - ditto*/);
    MOZ_ASSERT(!IsInRange(destVoid, srcVoid, srcVoidEnd));
    MOZ_ASSERT(!IsInRange(srcVoid, destVoid, destVoidEnd));
#endif

    using namespace jit;

    for (; count > 0; count--) {
        AtomicOperations::storeSafeWhenRacy(dest++,
                                            To(AtomicOperations::loadSafeWhenRacy(src++)));
    }
}

struct DisjointElements
{
    template <typename To>
    static void
    copy(SharedMem<To*> dest, SharedMem<void*> src, Scalar::Type fromType, uint32_t count) {
        switch (fromType) {
          case Scalar::Int8:
            CopyValues(dest, src.cast<int8_t*>(), count);
            return;

          case Scalar::Uint8:
            CopyValues(dest, src.cast<uint8_t*>(), count);
            return;

          case Scalar::Int16:
            CopyValues(dest, src.cast<int16_t*>(), count);
            return;

          case Scalar::Uint16:
            CopyValues(dest, src.cast<uint16_t*>(), count);
            return;

          case Scalar::Int32:
            CopyValues(dest, src.cast<int32_t*>(), count);
            return;

          case Scalar::Uint32:
            CopyValues(dest, src.cast<uint32_t*>(), count);
            return;

          case Scalar::Float32:
            CopyValues(dest, src.cast<float*>(), count);
            return;

          case Scalar::Float64:
            CopyValues(dest, src.cast<double*>(), count);
            return;

          case Scalar::Uint8Clamped:
            CopyValues(dest, src.cast<uint8_clamped*>(), count);
            return;

          default:
            MOZ_CRASH("NonoverlappingSet with bogus from-type");
        }
    }
};

static void
CopyToDisjointArray(TypedArrayObject* target, uint32_t targetOffset, SharedMem<void*> src,
                    Scalar::Type srcType, uint32_t count)
{
    Scalar::Type destType = target->type();
    SharedMem<uint8_t*> dest = target->viewDataEither().cast<uint8_t*>() + targetOffset * TypedArrayElemSize(destType);

    switch (destType) {
      case Scalar::Int8: {
        DisjointElements::copy(dest.cast<int8_t*>(), src, srcType, count);
        break;
      }

      case Scalar::Uint8: {
        DisjointElements::copy(dest.cast<uint8_t*>(), src, srcType, count);
        break;
      }

      case Scalar::Int16: {
        DisjointElements::copy(dest.cast<int16_t*>(), src, srcType, count);
        break;
      }

      case Scalar::Uint16: {
        DisjointElements::copy(dest.cast<uint16_t*>(), src, srcType, count);
        break;
      }

      case Scalar::Int32: {
        DisjointElements::copy(dest.cast<int32_t*>(), src, srcType, count);
        break;
      }

      case Scalar::Uint32: {
        DisjointElements::copy(dest.cast<uint32_t*>(), src, srcType, count);
        break;
      }

      case Scalar::Float32: {
        DisjointElements::copy(dest.cast<float*>(), src, srcType, count);
        break;
      }

      case Scalar::Float64: {
        DisjointElements::copy(dest.cast<double*>(), src, srcType, count);
        break;
      }

      case Scalar::Uint8Clamped: {
        DisjointElements::copy(dest.cast<uint8_clamped*>(), src, srcType, count);
        break;
      }

      default:
        MOZ_CRASH("setFromTypedArray with a typed array with bogus type");
    }
}

// |unsafeSrcCrossCompartment| is produced by |DangerouslyUnwrapTypedArray|,
// counseling extreme caution when using it.  As directed by
// |DangerouslyUnwrapTypedArray|, sigil this pointer and all variables derived
// from it to counsel extreme caution here.
void
js::SetDisjointTypedElements(TypedArrayObject* target, uint32_t targetOffset,
                             TypedArrayObject* unsafeSrcCrossCompartment)
{
    Scalar::Type unsafeSrcTypeCrossCompartment = unsafeSrcCrossCompartment->type();

    SharedMem<void*> unsafeSrcDataCrossCompartment = unsafeSrcCrossCompartment->viewDataEither();
    uint32_t count = unsafeSrcCrossCompartment->length();

    CopyToDisjointArray(target, targetOffset,
                        unsafeSrcDataCrossCompartment,
                        unsafeSrcTypeCrossCompartment, count);
}

static bool
intrinsic_SetDisjointTypedElements(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);

    Rooted<TypedArrayObject*> target(cx, &args[0].toObject().as<TypedArrayObject>());
    MOZ_ASSERT(!target->hasDetachedBuffer(),
               "a typed array viewing a detached buffer has no elements to "
               "set, so it's nonsensical to be setting them");

    uint32_t targetOffset = uint32_t(args[1].toInt32());

    // As directed by |DangerouslyUnwrapTypedArray|, sigil this pointer and all
    // variables derived from it to counsel extreme caution here.
    Rooted<TypedArrayObject*> unsafeSrcCrossCompartment(cx);
    unsafeSrcCrossCompartment = DangerouslyUnwrapTypedArray(cx, &args[2].toObject());
    if (!unsafeSrcCrossCompartment)
        return false;

    SetDisjointTypedElements(target, targetOffset, unsafeSrcCrossCompartment);

    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_SetOverlappingTypedElements(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);

    Rooted<TypedArrayObject*> target(cx, &args[0].toObject().as<TypedArrayObject>());
    MOZ_ASSERT(!target->hasDetachedBuffer(),
               "shouldn't set elements if underlying buffer is detached");

    uint32_t targetOffset = uint32_t(args[1].toInt32());

    // As directed by |DangerouslyUnwrapTypedArray|, sigil this pointer and all
    // variables derived from it to counsel extreme caution here.
    Rooted<TypedArrayObject*> unsafeSrcCrossCompartment(cx);
    unsafeSrcCrossCompartment = DangerouslyUnwrapTypedArray(cx, &args[2].toObject());
    if (!unsafeSrcCrossCompartment)
        return false;

    // Smarter algorithms exist to perform overlapping transfers of the sort
    // this method performs (for example, v8's self-hosted implementation).
    // But it seems likely deliberate overlapping transfers are rare enough
    // that it's not worth the trouble to implement one (and worry about its
    // safety/correctness!).  Make a copy and do a disjoint set from that.
    uint32_t count = unsafeSrcCrossCompartment->length();
    Scalar::Type unsafeSrcTypeCrossCompartment = unsafeSrcCrossCompartment->type();
    size_t sourceByteLen = count * TypedArrayElemSize(unsafeSrcTypeCrossCompartment);

    auto copyOfSrcData = target->zone()->make_pod_array<uint8_t>(sourceByteLen);
    if (!copyOfSrcData)
        return false;

    jit::AtomicOperations::memcpySafeWhenRacy(SharedMem<uint8_t*>::unshared(copyOfSrcData.get()),
                                              unsafeSrcCrossCompartment->viewDataEither().cast<uint8_t*>(),
                                              sourceByteLen);

    CopyToDisjointArray(target, targetOffset, SharedMem<void*>::unshared(copyOfSrcData.get()),
                        unsafeSrcTypeCrossCompartment, count);

    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_RegExpCreate(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    MOZ_ASSERT(args.length() == 1 || args.length() == 2);
    MOZ_ASSERT_IF(args.length() == 2, args[1].isString() || args[1].isUndefined());
    MOZ_ASSERT(!args.isConstructing());

    return RegExpCreate(cx, args[0], args.get(1), args.rval());
}

static bool
intrinsic_RegExpGetSubstitution(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    MOZ_ASSERT(args.length() == 6);

    RootedString matched(cx, args[0].toString());
    RootedString string(cx, args[1].toString());

    int32_t position = int32_t(args[2].toNumber());
    MOZ_ASSERT(position >= 0);

    RootedObject captures(cx, &args[3].toObject());
#ifdef DEBUG
    bool isArray = false;
    MOZ_ALWAYS_TRUE(IsArray(cx, captures, &isArray));
    MOZ_ASSERT(isArray);
#endif

    RootedString replacement(cx, args[4].toString());

    int32_t firstDollarIndex = int32_t(args[5].toNumber());
    MOZ_ASSERT(firstDollarIndex >= 0);

    RootedLinearString matchedLinear(cx, matched->ensureLinear(cx));
    if (!matchedLinear)
        return false;
    RootedLinearString stringLinear(cx, string->ensureLinear(cx));
    if (!stringLinear)
        return false;
    RootedLinearString replacementLinear(cx, replacement->ensureLinear(cx));
    if (!replacementLinear)
        return false;

    return RegExpGetSubstitution(cx, matchedLinear, stringLinear, size_t(position), captures,
                                 replacementLinear, size_t(firstDollarIndex), args.rval());
}

static bool
intrinsic_StringReplaceString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);

    RootedString string(cx, args[0].toString());
    RootedString pattern(cx, args[1].toString());
    RootedString replacement(cx, args[2].toString());
    JSString* result = str_replace_string_raw(cx, string, pattern, replacement);
    if (!result)
        return false;

    args.rval().setString(result);
    return true;
}

bool
js::intrinsic_StringSplitString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);

    RootedString string(cx, args[0].toString());
    RootedString sep(cx, args[1].toString());

    RootedObjectGroup group(cx, ObjectGroup::callingAllocationSiteGroup(cx, JSProto_Array));
    if (!group)
        return false;

    RootedObject aobj(cx);
    aobj = str_split_string(cx, group, string, sep, INT32_MAX);
    if (!aobj)
        return false;

    args.rval().setObject(*aobj);
    return true;
}

static bool
intrinsic_StringSplitStringLimit(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);

    RootedString string(cx, args[0].toString());
    RootedString sep(cx, args[1].toString());

    // args[2] should be already in UInt32 range, but it could be double typed,
    // because of Ion optimization.
    uint32_t limit = uint32_t(args[2].toNumber());

    RootedObjectGroup group(cx, ObjectGroup::callingAllocationSiteGroup(cx, JSProto_Array));
    if (!group)
        return false;

    RootedObject aobj(cx);
    aobj = str_split_string(cx, group, string, sep, limit);
    if (!aobj)
        return false;

    args.rval().setObject(*aobj);
    return true;
}

bool
CallSelfHostedNonGenericMethod(JSContext* cx, const CallArgs& args)
{
    // This function is called when a self-hosted method is invoked on a
    // wrapper object, like a CrossCompartmentWrapper. The last argument is
    // the name of the self-hosted function. The other arguments are the
    // arguments to pass to this function.

    MOZ_ASSERT(args.length() > 0);
    RootedPropertyName name(cx, args[args.length() - 1].toString()->asAtom().asPropertyName());

    RootedValue selfHostedFun(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), name, &selfHostedFun))
        return false;

    MOZ_ASSERT(selfHostedFun.toObject().is<JSFunction>());

    InvokeArgs args2(cx);
    if (!args2.init(args.length() - 1))
        return false;

    for (size_t i = 0; i < args.length() - 1; i++)
        args2[i].set(args[i]);

    return js::Call(cx, selfHostedFun, args.thisv(), args2, args.rval());
}

bool
js::CallSelfHostedFunction(JSContext* cx, const char* name, HandleValue thisv,
                           const AnyInvokeArgs& args, MutableHandleValue rval)
{
    RootedAtom funAtom(cx, Atomize(cx, name, strlen(name)));
    if (!funAtom)
        return false;
    RootedPropertyName funName(cx, funAtom->asPropertyName());
    return CallSelfHostedFunction(cx, funName, thisv, args, rval);
}

bool
js::CallSelfHostedFunction(JSContext* cx, HandlePropertyName name, HandleValue thisv,
                           const AnyInvokeArgs& args, MutableHandleValue rval)
{
    RootedValue fun(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), name, &fun))
        return false;
    MOZ_ASSERT(fun.toObject().is<JSFunction>());

    return Call(cx, fun, thisv, args, rval);
}

template<typename T>
bool
Is(HandleValue v)
{
    return v.isObject() && v.toObject().is<T>();
}

template<IsAcceptableThis Test>
static bool
CallNonGenericSelfhostedMethod(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<Test, CallSelfHostedNonGenericMethod>(cx, args);
}

bool
js::IsCallSelfHostedNonGenericMethod(NativeImpl impl)
{
    return impl == CallSelfHostedNonGenericMethod;
}

bool
js::ReportIncompatibleSelfHostedMethod(JSContext* cx, const CallArgs& args)
{
    // The contract for this function is the same as CallSelfHostedNonGenericMethod.
    // The normal ReportIncompatible function doesn't work for selfhosted functions,
    // because they always call the different CallXXXMethodIfWrapped methods,
    // which would be reported as the called function instead.

    // Lookup the selfhosted method that was invoked.  But skip over
    // IsTypedArrayEnsuringArrayBuffer frames, because those are never the
    // actual self-hosted callee from external code.  We can't just skip
    // self-hosted things until we find a non-self-hosted one because of cases
    // like array.sort(somethingSelfHosted), where we want to report the error
    // in the somethingSelfHosted, not in the sort() call.
    ScriptFrameIter iter(cx);
    MOZ_ASSERT(iter.isFunctionFrame());

    while (!iter.done()) {
        MOZ_ASSERT(iter.callee(cx)->isSelfHostedOrIntrinsic() &&
                   !iter.callee(cx)->isBoundFunction());
        JSAutoByteString funNameBytes;
        const char* funName = GetFunctionNameBytes(cx, iter.callee(cx), &funNameBytes);
        if (!funName)
            return false;
        if (strcmp(funName, "IsTypedArrayEnsuringArrayBuffer") != 0) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_METHOD,
                                 funName, "method", InformalValueTypeName(args.thisv()));
            return false;
        }
        ++iter;
    }

    MOZ_ASSERT_UNREACHABLE("How did we not find a useful self-hosted frame?");
    return false;
}

// ES6, 25.4.1.6.
static bool
intrinsic_EnqueuePromiseReactionJob(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].toObject().as<NativeObject>().getDenseInitializedLength() == 4);

    // When using JS::AddPromiseReactions, no actual promise is created, so we
    // might not have one here.
    RootedObject promise(cx);
    if (args[1].isObject())
        promise = UncheckedUnwrap(&args[1].toObject());

#ifdef DEBUG
    MOZ_ASSERT_IF(promise, promise->is<PromiseObject>());
    RootedNativeObject jobArgs(cx, &args[0].toObject().as<NativeObject>());
    MOZ_ASSERT((jobArgs->getDenseElement(0).isNumber() &&
                (jobArgs->getDenseElement(0).toNumber() == PROMISE_HANDLER_IDENTITY ||
                 jobArgs->getDenseElement(0).toNumber() == PROMISE_HANDLER_THROWER)) ||
               jobArgs->getDenseElement(0).toObject().isCallable());
    MOZ_ASSERT(jobArgs->getDenseElement(2).toObject().isCallable());
    MOZ_ASSERT(jobArgs->getDenseElement(3).toObject().isCallable());
#endif

    RootedAtom funName(cx, cx->names().empty);
    RootedFunction job(cx, NewNativeFunction(cx, PromiseReactionJob, 0, funName,
                                             gc::AllocKind::FUNCTION_EXTENDED));
    if (!job)
        return false;

    job->setExtendedSlot(0, args[0]);
    if (!cx->runtime()->enqueuePromiseJob(cx, job, promise))
        return false;
    args.rval().setUndefined();
    return true;
}

// ES6, 25.4.1.6.
static bool
intrinsic_EnqueuePromiseResolveThenableJob(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

#ifdef DEBUG
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(UncheckedUnwrap(&args[1].toObject())->is<PromiseObject>());
    RootedNativeObject jobArgs(cx, &args[0].toObject().as<NativeObject>());
    MOZ_ASSERT(jobArgs->getDenseInitializedLength() == 3);
    MOZ_ASSERT(jobArgs->getDenseElement(0).toObject().isCallable());
    MOZ_ASSERT(jobArgs->getDenseElement(1).isObject());
    MOZ_ASSERT(UncheckedUnwrap(&jobArgs->getDenseElement(2).toObject())->is<PromiseObject>());
#endif

    RootedAtom funName(cx, cx->names().empty);
    RootedFunction job(cx, NewNativeFunction(cx, PromiseResolveThenableJob, 0, funName,
                                             gc::AllocKind::FUNCTION_EXTENDED));
    if (!job)
        return false;

    job->setExtendedSlot(0, args[0]);
    RootedObject promise(cx, CheckedUnwrap(&args[1].toObject()));
    if (!cx->runtime()->enqueuePromiseJob(cx, job, promise))
        return false;
    args.rval().setUndefined();
    return true;
}

// ES2016, February 12 draft, 25.4.1.9.
static bool
intrinsic_HostPromiseRejectionTracker(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].toObject().is<PromiseObject>());

    Rooted<PromiseObject*> promise(cx, &args[0].toObject().as<PromiseObject>());
    mozilla::DebugOnly<bool> isHandled = args[1].toBoolean();
    MOZ_ASSERT(isHandled, "HostPromiseRejectionTracker intrinsic currently only marks as handled");
    cx->runtime()->removeUnhandledRejectedPromise(cx, promise);
    args.rval().setUndefined();
    return true;
}

/**
 * Returns the default locale as a well-formed, but not necessarily canonicalized,
 * BCP-47 language tag.
 */
static bool
intrinsic_RuntimeDefaultLocale(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    const char* locale = cx->runtime()->getDefaultLocale();
    if (!locale) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_DEFAULT_LOCALE_ERROR);
        return false;
    }

    RootedString jslocale(cx, JS_NewStringCopyZ(cx, locale));
    if (!jslocale)
        return false;

    args.rval().setString(jslocale);
    return true;
}

static bool
intrinsic_LocalTZA(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0, "the LocalTZA intrinsic takes no arguments");

    args.rval().setDouble(DateTimeInfo::localTZA());
    return true;
}

static bool
intrinsic_AddContentTelemetry(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);

    int id = args[0].toInt32();
    MOZ_ASSERT(id < JS_TELEMETRY_END);
    MOZ_ASSERT(id >= 0);

    if (!cx->compartment()->isProbablySystemOrAddonCode())
        cx->runtime()->addTelemetry(id, args[1].toInt32());

    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_ConstructFunction(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].toObject().is<JSFunction>());
    MOZ_ASSERT(args[1].toObject().is<ArrayObject>());

    RootedArrayObject argsList(cx, &args[1].toObject().as<ArrayObject>());
    uint32_t len = argsList->length();
    ConstructArgs constructArgs(cx);
    if (!constructArgs.init(len))
        return false;
    for (uint32_t index = 0; index < len; index++)
        constructArgs[index].set(argsList->getDenseElement(index));

    RootedObject res(cx);
    if (!Construct(cx, args[0], constructArgs, args[0], &res))
        return false;

    args.rval().setObject(*res);
    return true;
}


static bool
intrinsic_IsConstructing(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    ScriptFrameIter iter(cx);
    bool isConstructing = iter.isConstructing();
    args.rval().setBoolean(isConstructing);
    return true;
}

static bool
intrinsic_ConstructorForTypedArray(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    MOZ_ASSERT(args[0].isObject());

    RootedObject object(cx, &args[0].toObject());
    object = CheckedUnwrap(object);
    MOZ_ASSERT(object->is<TypedArrayObject>());

    JSProtoKey protoKey = StandardProtoKeyOrNull(object);
    MOZ_ASSERT(protoKey);

    // While it may seem like an invariant that in any compartment,
    // seeing a typed array object implies that the TypedArray constructor
    // for that type is initialized on the compartment's global, this is not
    // the case. When we construct a typed array given a cross-compartment
    // ArrayBuffer, we put the constructed TypedArray in the same compartment
    // as the ArrayBuffer. Since we use the prototype from the initial
    // compartment, and never call the constructor in the ArrayBuffer's
    // compartment from script, we are not guaranteed to have initialized
    // the constructor.
    RootedObject ctor(cx);
    if (!GetBuiltinConstructor(cx, protoKey, &ctor))
        return false;

    args.rval().setObject(*ctor);
    return true;
}

static bool
intrinsic_OriginalPromiseConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    JSObject* obj = GlobalObject::getOrCreatePromiseConstructor(cx, cx->global());
    if (!obj)
        return false;

    args.rval().setObject(*obj);
    return true;
}

static bool
intrinsic_RejectUnwrappedPromise(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);

    RootedObject obj(cx, &args[0].toObject());
    MOZ_ASSERT(IsWrapper(obj));
    Rooted<PromiseObject*> promise(cx, &UncheckedUnwrap(obj)->as<PromiseObject>());
    AutoCompartment ac(cx, promise);
    RootedValue reasonVal(cx, args[1]);

    // The rejection reason might've been created in a compartment with higher
    // privileges than the Promise's. In that case, object-type rejection
    // values might be wrapped into a wrapper that throws whenever the
    // Promise's reaction handler wants to do anything useful with it. To
    // avoid that situation, we synthesize a generic error that doesn't
    // expose any privileged information but can safely be used in the
    // rejection handler.
    if (!promise->compartment()->wrap(cx, &reasonVal))
        return false;
    if (reasonVal.isObject() && !CheckedUnwrap(&reasonVal.toObject())) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr,
                             JSMSG_PROMISE_ERROR_IN_WRAPPED_REJECTION_REASON);
        if (!GetAndClearException(cx, &reasonVal))
            return false;
    }

    RootedAtom atom(cx, Atomize(cx, "RejectPromise", strlen("RejectPromise")));
    if (!atom)
        return false;
    RootedPropertyName name(cx, atom->asPropertyName());

    FixedInvokeArgs<2> args2(cx);

    args2[0].setObject(*promise);
    args2[1].set(reasonVal);

    return CallSelfHostedFunction(cx, name, UndefinedHandleValue, args2, args.rval());
}

static bool
intrinsic_IsWrappedPromiseObject(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);

    RootedObject obj(cx, &args[0].toObject());
    MOZ_ASSERT(!obj->is<PromiseObject>(),
               "Unwrapped promises should be filtered out in inlineable code");
    args.rval().setBoolean(JS::IsPromiseObject(obj));
    return true;
}

static bool
intrinsic_HostResolveImportedModule(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].toObject().is<ModuleObject>());
    MOZ_ASSERT(args[1].isString());

    RootedFunction moduleResolveHook(cx, cx->global()->moduleResolveHook());
    if (!moduleResolveHook) {
        JS_ReportError(cx, "Module resolve hook not set");
        return false;
    }

    RootedValue result(cx);
    if (!JS_CallFunction(cx, nullptr, moduleResolveHook, args, &result))
        return false;

    if (!result.isObject() || !result.toObject().is<ModuleObject>()) {
        JS_ReportError(cx, "Module resolve hook did not return Module object");
        return false;
    }

    args.rval().set(result);
    return true;
}

static bool
intrinsic_GetModuleEnvironment(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedModuleObject module(cx, &args[0].toObject().as<ModuleObject>());
    RootedModuleEnvironmentObject env(cx, module->environment());
    args.rval().setUndefined();
    if (!env) {
        args.rval().setUndefined();
        return true;
    }

    args.rval().setObject(*env);
    return true;
}

static bool
intrinsic_CreateModuleEnvironment(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedModuleObject module(cx, &args[0].toObject().as<ModuleObject>());
    module->createEnvironment();
    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_CreateImportBinding(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 4);
    RootedModuleEnvironmentObject environment(cx, &args[0].toObject().as<ModuleEnvironmentObject>());
    RootedAtom importedName(cx, &args[1].toString()->asAtom());
    RootedModuleObject module(cx, &args[2].toObject().as<ModuleObject>());
    RootedAtom localName(cx, &args[3].toString()->asAtom());
    if (!environment->createImportBinding(cx, importedName, module, localName))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_CreateNamespaceBinding(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);
    RootedModuleEnvironmentObject environment(cx, &args[0].toObject().as<ModuleEnvironmentObject>());
    RootedId name(cx, AtomToId(&args[1].toString()->asAtom()));
    MOZ_ASSERT(args[2].toObject().is<ModuleNamespaceObject>());
    // The property already exists in the evironment but is not writable, so set
    // the slot directly.
    RootedShape shape(cx, environment->lookup(cx, name));
    MOZ_ASSERT(shape);
    MOZ_ASSERT(environment->getSlot(shape->slot()).isMagic(JS_UNINITIALIZED_LEXICAL));
    environment->setSlot(shape->slot(), args[2]);
    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_InstantiateModuleFunctionDeclarations(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedModuleObject module(cx, &args[0].toObject().as<ModuleObject>());
    args.rval().setUndefined();
    return ModuleObject::instantiateFunctionDeclarations(cx, module);
}

static bool
intrinsic_SetModuleEvaluated(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedModuleObject module(cx, &args[0].toObject().as<ModuleObject>());
    module->setEvaluated();
    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_EvaluateModule(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedModuleObject module(cx, &args[0].toObject().as<ModuleObject>());
    return ModuleObject::evaluate(cx, module, args.rval());
}

static bool
intrinsic_NewModuleNamespace(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    RootedModuleObject module(cx, &args[0].toObject().as<ModuleObject>());
    RootedObject exports(cx, &args[1].toObject());
    RootedObject namespace_(cx, ModuleObject::createNamespace(cx, module, exports));
    if (!namespace_)
        return false;

    args.rval().setObject(*namespace_);
    return true;
}

static bool
intrinsic_AddModuleNamespaceBinding(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 4);
    RootedModuleNamespaceObject namespace_(cx, &args[0].toObject().as<ModuleNamespaceObject>());
    RootedAtom exportedName(cx, &args[1].toString()->asAtom());
    RootedModuleObject targetModule(cx, &args[2].toObject().as<ModuleObject>());
    RootedAtom localName(cx, &args[3].toString()->asAtom());
    if (!namespace_->addBinding(cx, exportedName, targetModule, localName))
        return false;

    args.rval().setUndefined();
    return true;
}

static bool
intrinsic_ModuleNamespaceExports(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    RootedModuleNamespaceObject namespace_(cx, &args[0].toObject().as<ModuleNamespaceObject>());
    args.rval().setObject(namespace_->exports());
    return true;
}

/**
 * Intrinsic used to tell the debugger about settled promises.
 *
 * This is invoked both when resolving and rejecting promises, after the
 * resulting state has been set on the promise, and it's up to the debugger
 * to act on this signal in whichever way it wants.
 */
static bool
intrinsic_onPromiseSettled(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 1);
    Rooted<PromiseObject*> promise(cx, &args[0].toObject().as<PromiseObject>());
    promise->onSettled(cx);
    args.rval().setUndefined();
    return true;
}

// The self-hosting global isn't initialized with the normal set of builtins.
// Instead, individual C++-implemented functions that're required by
// self-hosted code are defined as global functions. Accessing these
// functions via a content compartment's builtins would be unsafe, because
// content script might have changed the builtins' prototypes' members.
// Installing the whole set of builtins in the self-hosting compartment, OTOH,
// would be wasteful: it increases memory usage and initialization time for
// self-hosting compartment.
//
// Additionally, a set of C++-implemented helper functions is defined on the
// self-hosting global.
static const JSFunctionSpec intrinsic_functions[] = {
    JS_INLINABLE_FN("std_Array",                 array_construct,              1,0, Array),
    JS_FN("std_Array_join",                      array_join,                   1,0),
    JS_INLINABLE_FN("std_Array_push",            array_push,                   1,0, ArrayPush),
    JS_INLINABLE_FN("std_Array_pop",             array_pop,                    0,0, ArrayPop),
    JS_INLINABLE_FN("std_Array_shift",           array_shift,                  0,0, ArrayShift),
    JS_FN("std_Array_unshift",                   array_unshift,                1,0),
    JS_INLINABLE_FN("std_Array_slice",           array_slice,                  2,0, ArraySlice),
    JS_FN("std_Array_sort",                      array_sort,                   1,0),
    JS_FN("std_Array_reverse",                   array_reverse,                0,0),
    JS_INLINABLE_FN("std_Array_splice",          array_splice,                 2,0, ArraySplice),

    JS_FN("std_Date_now",                        date_now,                     0,0),
    JS_FN("std_Date_valueOf",                    date_valueOf,                 0,0),

    JS_FN("std_Function_apply",                  fun_apply,                    2,0),

    JS_INLINABLE_FN("std_Math_floor",            math_floor,                   1,0, MathFloor),
    JS_INLINABLE_FN("std_Math_max",              math_max,                     2,0, MathMax),
    JS_INLINABLE_FN("std_Math_min",              math_min,                     2,0, MathMin),
    JS_INLINABLE_FN("std_Math_abs",              math_abs,                     1,0, MathAbs),
    JS_INLINABLE_FN("std_Math_imul",             math_imul,                    2,0, MathImul),
    JS_INLINABLE_FN("std_Math_log2",             math_log2,                    1,0, MathLog2),

    JS_FN("std_Map_has",                         MapObject::has,               1,0),
    JS_FN("std_Map_iterator",                    MapObject::entries,           0,0),

    JS_FN("std_Number_valueOf",                  num_valueOf,                  0,0),

    JS_INLINABLE_FN("std_Object_create",         obj_create,                   2, 0, ObjectCreate),
    JS_FN("std_Object_propertyIsEnumerable",     obj_propertyIsEnumerable,     1,0),
    JS_FN("std_Object_defineProperty",           obj_defineProperty,           3,0),
    JS_FN("std_Object_getOwnPropertyNames",      obj_getOwnPropertyNames,      1,0),
    JS_FN("std_Object_getOwnPropertyDescriptor", obj_getOwnPropertyDescriptor, 2,0),
    JS_FN("std_Object_hasOwnProperty",           obj_hasOwnProperty,           1,0),
    JS_FN("std_Object_setPrototypeOf",           intrinsic_SetPrototype,       2,0),
    JS_FN("std_Object_toString",                 obj_toString,                 0,0),

    JS_FN("std_Reflect_getPrototypeOf",          Reflect_getPrototypeOf,       1,0),
    JS_FN("std_Reflect_isExtensible",            Reflect_isExtensible,         1,0),

    JS_FN("std_Set_has",                         SetObject::has,               1,0),
    JS_FN("std_Set_iterator",                    SetObject::values,            0,0),

    JS_INLINABLE_FN("std_String_fromCharCode",   str_fromCharCode,             1,0, StringFromCharCode),
    JS_INLINABLE_FN("std_String_charCodeAt",     str_charCodeAt,               1,0, StringCharCodeAt),
    JS_FN("std_String_includes",                 str_includes,                 1,0),
    JS_FN("std_String_indexOf",                  str_indexOf,                  1,0),
    JS_FN("std_String_lastIndexOf",              str_lastIndexOf,              1,0),
    JS_FN("std_String_startsWith",               str_startsWith,               1,0),
    JS_FN("std_String_toLowerCase",              str_toLowerCase,              0,0),
    JS_FN("std_String_toUpperCase",              str_toUpperCase,              0,0),

    JS_INLINABLE_FN("std_String_charAt",         str_charAt,                   1,0, StringCharAt),
    JS_FN("std_String_endsWith",                 str_endsWith,                 1,0),
    JS_FN("std_String_trim",                     str_trim,                     0,0),
    JS_FN("std_String_trimLeft",                 str_trimLeft,                 0,0),
    JS_FN("std_String_trimRight",                str_trimRight,                0,0),
    JS_FN("std_String_toLocaleLowerCase",        str_toLocaleLowerCase,        0,0),
    JS_FN("std_String_toLocaleUpperCase",        str_toLocaleUpperCase,        0,0),
#if !EXPOSE_INTL_API
    JS_FN("std_String_localeCompare",            str_localeCompare,            1,0),
#else
    JS_FN("std_String_normalize",                str_normalize,                0,0),
#endif
    JS_FN("std_String_concat",                   str_concat,                   1,0),


    JS_FN("std_WeakMap_has",                     WeakMap_has,                  1,0),
    JS_FN("std_WeakMap_get",                     WeakMap_get,                  2,0),
    JS_FN("std_WeakMap_set",                     WeakMap_set,                  2,0),
    JS_FN("std_WeakMap_delete",                  WeakMap_delete,               1,0),

    JS_FN("std_SIMD_Int8x16_extractLane",        simd_int8x16_extractLane,     2,0),
    JS_FN("std_SIMD_Int16x8_extractLane",        simd_int16x8_extractLane,     2,0),
    JS_INLINABLE_FN("std_SIMD_Int32x4_extractLane",   simd_int32x4_extractLane,  2,0, SimdInt32x4_extractLane),
    JS_FN("std_SIMD_Uint8x16_extractLane",       simd_uint8x16_extractLane,    2,0),
    JS_FN("std_SIMD_Uint16x8_extractLane",       simd_uint16x8_extractLane,    2,0),
    JS_FN("std_SIMD_Uint32x4_extractLane",       simd_uint32x4_extractLane,    2,0),
    JS_INLINABLE_FN("std_SIMD_Float32x4_extractLane", simd_float32x4_extractLane,2,0, SimdFloat32x4_extractLane),
    JS_FN("std_SIMD_Float64x2_extractLane",      simd_float64x2_extractLane,   2,0),
    JS_FN("std_SIMD_Bool8x16_extractLane",       simd_bool8x16_extractLane,    2,0),
    JS_FN("std_SIMD_Bool16x8_extractLane",       simd_bool16x8_extractLane,    2,0),
    JS_FN("std_SIMD_Bool32x4_extractLane",       simd_bool32x4_extractLane,    2,0),
    JS_FN("std_SIMD_Bool64x2_extractLane",       simd_bool64x2_extractLane,    2,0),

    // Helper funtions after this point.
    JS_INLINABLE_FN("ToObject",      intrinsic_ToObject,                1,0, IntrinsicToObject),
    JS_INLINABLE_FN("IsObject",      intrinsic_IsObject,                1,0, IntrinsicIsObject),
    JS_INLINABLE_FN("IsArray",       intrinsic_IsArray,                 1,0, ArrayIsArray),
    JS_INLINABLE_FN("IsWrappedArrayConstructor", intrinsic_IsWrappedArrayConstructor, 1,0,
                    IntrinsicIsWrappedArrayConstructor),
    JS_INLINABLE_FN("ToInteger",     intrinsic_ToInteger,               1,0, IntrinsicToInteger),
    JS_INLINABLE_FN("ToString",      intrinsic_ToString,                1,0, IntrinsicToString),
    JS_FN("ToPropertyKey",           intrinsic_ToPropertyKey,           1,0),
    JS_INLINABLE_FN("IsCallable",    intrinsic_IsCallable,              1,0, IntrinsicIsCallable),
    JS_INLINABLE_FN("IsConstructor", intrinsic_IsConstructor,           1,0,
                    IntrinsicIsConstructor),
    JS_FN("GetBuiltinConstructorImpl", intrinsic_GetBuiltinConstructor, 1,0),
    JS_FN("MakeConstructible",       intrinsic_MakeConstructible,       2,0),
    JS_FN("_ConstructFunction",      intrinsic_ConstructFunction,       2,0),
    JS_FN("ThrowRangeError",         intrinsic_ThrowRangeError,         4,0),
    JS_FN("ThrowTypeError",          intrinsic_ThrowTypeError,          4,0),
    JS_FN("ThrowSyntaxError",        intrinsic_ThrowSyntaxError,        4,0),
    JS_FN("AssertionFailed",         intrinsic_AssertionFailed,         1,0),
    JS_FN("DumpMessage",             intrinsic_DumpMessage,             1,0),
    JS_FN("OwnPropertyKeys",         intrinsic_OwnPropertyKeys,         1,0),
    JS_FN("MakeDefaultConstructor",  intrinsic_MakeDefaultConstructor,  2,0),
    JS_FN("_ConstructorForTypedArray", intrinsic_ConstructorForTypedArray, 1,0),
    JS_FN("DecompileArg",            intrinsic_DecompileArg,            2,0),
    JS_FN("_FinishBoundFunctionInit", intrinsic_FinishBoundFunctionInit, 4,0),
    JS_FN("RuntimeDefaultLocale",    intrinsic_RuntimeDefaultLocale,    0,0),
    JS_FN("LocalTZA",                intrinsic_LocalTZA,                0,0),
    JS_FN("AddContentTelemetry",     intrinsic_AddContentTelemetry,     2,0),

    JS_INLINABLE_FN("_IsConstructing", intrinsic_IsConstructing,        0,0,
                    IntrinsicIsConstructing),
    JS_INLINABLE_FN("SubstringKernel", intrinsic_SubstringKernel,       3,0,
                    IntrinsicSubstringKernel),
    JS_INLINABLE_FN("_DefineDataProperty",              intrinsic_DefineDataProperty,      4,0,
                    IntrinsicDefineDataProperty),
    JS_INLINABLE_FN("ObjectHasPrototype",               intrinsic_ObjectHasPrototype,      2,0,
                    IntrinsicObjectHasPrototype),
    JS_INLINABLE_FN("UnsafeSetReservedSlot",            intrinsic_UnsafeSetReservedSlot,   3,0,
                    IntrinsicUnsafeSetReservedSlot),
    JS_INLINABLE_FN("UnsafeGetReservedSlot",            intrinsic_UnsafeGetReservedSlot,   2,0,
                    IntrinsicUnsafeGetReservedSlot),
    JS_INLINABLE_FN("UnsafeGetObjectFromReservedSlot",  intrinsic_UnsafeGetObjectFromReservedSlot, 2,0,
                    IntrinsicUnsafeGetObjectFromReservedSlot),
    JS_INLINABLE_FN("UnsafeGetInt32FromReservedSlot",   intrinsic_UnsafeGetInt32FromReservedSlot,  2,0,
                    IntrinsicUnsafeGetInt32FromReservedSlot),
    JS_INLINABLE_FN("UnsafeGetStringFromReservedSlot",  intrinsic_UnsafeGetStringFromReservedSlot, 2,0,
                    IntrinsicUnsafeGetStringFromReservedSlot),
    JS_INLINABLE_FN("UnsafeGetBooleanFromReservedSlot", intrinsic_UnsafeGetBooleanFromReservedSlot,2,0,
                    IntrinsicUnsafeGetBooleanFromReservedSlot),

    JS_FN("UnsafeCallWrappedFunction", intrinsic_UnsafeCallWrappedFunction,2,0),
    JS_FN("NewArrayInCompartment",   intrinsic_NewArrayInCompartment,   1,0),

    JS_FN("IsPackedArray",           intrinsic_IsPackedArray,           1,0),

    JS_FN("GetIteratorPrototype",    intrinsic_GetIteratorPrototype,    0,0),

    JS_FN("NewArrayIterator",        intrinsic_NewArrayIterator,        0,0),
    JS_FN("CallArrayIteratorMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<ArrayIteratorObject>>,      2,0),

    JS_FN("NewListIterator",         intrinsic_NewListIterator,         0,0),
    JS_FN("CallListIteratorMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<ListIteratorObject>>,       2,0),
    JS_FN("ActiveFunction",          intrinsic_ActiveFunction,          0,0),

    JS_FN("_SetCanonicalName",       intrinsic_SetCanonicalName,        2,0),

    JS_INLINABLE_FN("IsArrayIterator",
                    intrinsic_IsInstanceOfBuiltin<ArrayIteratorObject>, 1,0,
                    IntrinsicIsArrayIterator),
    JS_INLINABLE_FN("IsMapIterator",
                    intrinsic_IsInstanceOfBuiltin<MapIteratorObject>,   1,0,
                    IntrinsicIsMapIterator),
    JS_INLINABLE_FN("IsStringIterator",
                    intrinsic_IsInstanceOfBuiltin<StringIteratorObject>, 1,0,
                    IntrinsicIsStringIterator),
    JS_INLINABLE_FN("IsListIterator",
                    intrinsic_IsInstanceOfBuiltin<ListIteratorObject>,  1,0,
                    IntrinsicIsListIterator),

    JS_FN("_CreateMapIterationResultPair", intrinsic_CreateMapIterationResultPair, 0, 0),
    JS_INLINABLE_FN("_GetNextMapEntryForIterator", intrinsic_GetNextMapEntryForIterator, 2,0,
                    IntrinsicGetNextMapEntryForIterator),
    JS_FN("CallMapIteratorMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<MapIteratorObject>>,        2,0),


    JS_FN("NewStringIterator",       intrinsic_NewStringIterator,       0,0),
    JS_FN("CallStringIteratorMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<StringIteratorObject>>,     2,0),

    JS_FN("IsStarGeneratorObject",
          intrinsic_IsInstanceOfBuiltin<StarGeneratorObject>,           1,0),
    JS_FN("StarGeneratorObjectIsClosed", intrinsic_StarGeneratorObjectIsClosed, 1,0),
    JS_FN("IsSuspendedStarGenerator",intrinsic_IsSuspendedStarGenerator,1,0),

    JS_FN("IsLegacyGeneratorObject",
          intrinsic_IsInstanceOfBuiltin<LegacyGeneratorObject>,         1,0),
    JS_FN("LegacyGeneratorObjectIsClosed", intrinsic_LegacyGeneratorObjectIsClosed, 1,0),
    JS_FN("CloseClosingLegacyGeneratorObject", intrinsic_CloseClosingLegacyGeneratorObject, 1,0),
    JS_FN("ThrowStopIteration",      intrinsic_ThrowStopIteration,      0,0),

    JS_FN("GeneratorIsRunning",      intrinsic_GeneratorIsRunning,      1,0),
    JS_FN("GeneratorSetClosed",      intrinsic_GeneratorSetClosed,      1,0),

    JS_FN("IsArrayBuffer",
          intrinsic_IsInstanceOfBuiltin<ArrayBufferObject>,             1,0),
    JS_FN("IsSharedArrayBuffer",
          intrinsic_IsInstanceOfBuiltin<SharedArrayBufferObject>,       1,0),
    JS_FN("IsWrappedArrayBuffer",    intrinsic_IsWrappedArrayBuffer,    1,0),

    JS_INLINABLE_FN("ArrayBufferByteLength",   intrinsic_ArrayBufferByteLength, 1,0,
                    IntrinsicArrayBufferByteLength),
    JS_INLINABLE_FN("PossiblyWrappedArrayBufferByteLength", intrinsic_PossiblyWrappedArrayBufferByteLength, 1,0,
                    IntrinsicPossiblyWrappedArrayBufferByteLength),
    JS_FN("ArrayBufferCopyData",     intrinsic_ArrayBufferCopyData,     5,0),

    JS_FN("IsUint8TypedArray",        intrinsic_IsUint8TypedArray,      1,0),
    JS_FN("IsInt8TypedArray",         intrinsic_IsInt8TypedArray,       1,0),
    JS_FN("IsUint16TypedArray",       intrinsic_IsUint16TypedArray,     1,0),
    JS_FN("IsInt16TypedArray",        intrinsic_IsInt16TypedArray,      1,0),
    JS_FN("IsUint32TypedArray",       intrinsic_IsUint32TypedArray,     1,0),
    JS_FN("IsInt32TypedArray",        intrinsic_IsInt32TypedArray,      1,0),
    JS_FN("IsFloat32TypedArray",      intrinsic_IsFloat32TypedArray,    1,0),
    JS_INLINABLE_FN("IsTypedArray",
                    intrinsic_IsInstanceOfBuiltin<TypedArrayObject>,    1,0,
                    IntrinsicIsTypedArray),
    JS_INLINABLE_FN("IsPossiblyWrappedTypedArray",intrinsic_IsPossiblyWrappedTypedArray,1,0,
                    IntrinsicIsPossiblyWrappedTypedArray),

    JS_FN("TypedArrayBuffer",        intrinsic_TypedArrayBuffer,        1,0),
    JS_FN("TypedArrayByteOffset",    intrinsic_TypedArrayByteOffset,    1,0),
    JS_FN("TypedArrayElementShift",  intrinsic_TypedArrayElementShift,  1,0),

    JS_INLINABLE_FN("TypedArrayLength", intrinsic_TypedArrayLength,     1,0,
                    IntrinsicTypedArrayLength),
    JS_INLINABLE_FN("PossiblyWrappedTypedArrayLength", intrinsic_PossiblyWrappedTypedArrayLength,
                    1, 0, IntrinsicPossiblyWrappedTypedArrayLength),

    JS_FN("MoveTypedArrayElements",  intrinsic_MoveTypedArrayElements,  4,0),
    JS_FN("SetFromTypedArrayApproach",intrinsic_SetFromTypedArrayApproach, 4, 0),
    JS_FN("SetOverlappingTypedElements",intrinsic_SetOverlappingTypedElements,3,0),

    JS_INLINABLE_FN("SetDisjointTypedElements",intrinsic_SetDisjointTypedElements,3,0,
                    IntrinsicSetDisjointTypedElements),

    JS_FN("CallArrayBufferMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<ArrayBufferObject>>, 2, 0),
    JS_FN("CallTypedArrayMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<TypedArrayObject>>, 2, 0),

    JS_FN("CallLegacyGeneratorMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<LegacyGeneratorObject>>, 2, 0),
    JS_FN("CallStarGeneratorMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<StarGeneratorObject>>, 2, 0),

    JS_FN("IsWeakSet", intrinsic_IsInstanceOfBuiltin<WeakSetObject>, 1,0),
    JS_FN("CallWeakSetMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<WeakSetObject>>, 2, 0),

    JS_FN("IsPromise",                      intrinsic_IsInstanceOfBuiltin<PromiseObject>, 1,0),
    JS_FN("IsWrappedPromise",               intrinsic_IsWrappedPromiseObject,     1, 0),
    JS_FN("_EnqueuePromiseReactionJob",     intrinsic_EnqueuePromiseReactionJob,  2, 0),
    JS_FN("_EnqueuePromiseResolveThenableJob", intrinsic_EnqueuePromiseResolveThenableJob, 2, 0),
    JS_FN("HostPromiseRejectionTracker",    intrinsic_HostPromiseRejectionTracker,2, 0),
    JS_FN("_GetOriginalPromiseConstructor", intrinsic_OriginalPromiseConstructor, 0, 0),
    JS_FN("RejectUnwrappedPromise",         intrinsic_RejectUnwrappedPromise,     2, 0),
    JS_FN("CallPromiseMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<PromiseObject>>,      2,0),

    // See builtin/TypedObject.h for descriptors of the typedobj functions.
    JS_FN("NewOpaqueTypedObject",           js::NewOpaqueTypedObject, 1, 0),
    JS_FN("NewDerivedTypedObject",          js::NewDerivedTypedObject, 3, 0),
    JS_FN("TypedObjectBuffer",              TypedObject::GetBuffer, 1, 0),
    JS_FN("TypedObjectByteOffset",          TypedObject::GetByteOffset, 1, 0),
    JS_FN("AttachTypedObject",              js::AttachTypedObject, 3, 0),
    JS_FN("TypedObjectIsAttached",          js::TypedObjectIsAttached, 1, 0),
    JS_FN("TypedObjectTypeDescr",           js::TypedObjectTypeDescr, 1, 0),
    JS_FN("ClampToUint8",                   js::ClampToUint8, 1, 0),
    JS_FN("GetTypedObjectModule",           js::GetTypedObjectModule, 0, 0),
    JS_FN("GetSimdTypeDescr",               js::GetSimdTypeDescr, 1, 0),

    JS_INLINABLE_FN("ObjectIsTypeDescr"    ,          js::ObjectIsTypeDescr, 1, 0,
                    IntrinsicObjectIsTypeDescr),
    JS_INLINABLE_FN("ObjectIsTypedObject",            js::ObjectIsTypedObject, 1, 0,
                    IntrinsicObjectIsTypedObject),
    JS_INLINABLE_FN("ObjectIsOpaqueTypedObject",      js::ObjectIsOpaqueTypedObject, 1, 0,
                    IntrinsicObjectIsOpaqueTypedObject),
    JS_INLINABLE_FN("ObjectIsTransparentTypedObject", js::ObjectIsTransparentTypedObject, 1, 0,
                    IntrinsicObjectIsTransparentTypedObject),
    JS_INLINABLE_FN("TypeDescrIsArrayType",           js::TypeDescrIsArrayType, 1, 0,
                    IntrinsicTypeDescrIsArrayType),
    JS_INLINABLE_FN("TypeDescrIsSimpleType",          js::TypeDescrIsSimpleType, 1, 0,
                    IntrinsicTypeDescrIsSimpleType),
    JS_INLINABLE_FN("SetTypedObjectOffset",           js::SetTypedObjectOffset, 2, 0,
                    IntrinsicSetTypedObjectOffset),

#define LOAD_AND_STORE_SCALAR_FN_DECLS(_constant, _type, _name)         \
    JS_FN("Store_" #_name, js::StoreScalar##_type::Func, 3, 0),         \
    JS_FN("Load_" #_name,  js::LoadScalar##_type::Func, 3, 0),
    JS_FOR_EACH_UNIQUE_SCALAR_TYPE_REPR_CTYPE(LOAD_AND_STORE_SCALAR_FN_DECLS)
#undef LOAD_AND_STORE_SCALAR_FN_DECLS

#define LOAD_AND_STORE_REFERENCE_FN_DECLS(_constant, _type, _name)      \
    JS_FN("Store_" #_name, js::StoreReference##_name::Func, 3, 0),      \
    JS_FN("Load_" #_name,  js::LoadReference##_name::Func, 3, 0),
    JS_FOR_EACH_REFERENCE_TYPE_REPR(LOAD_AND_STORE_REFERENCE_FN_DECLS)
#undef LOAD_AND_STORE_REFERENCE_FN_DECLS

    // See builtin/Intl.h for descriptions of the intl_* functions.
    JS_FN("intl_availableCalendars", intl_availableCalendars, 1,0),
    JS_FN("intl_availableCollations", intl_availableCollations, 1,0),
    JS_FN("intl_Collator", intl_Collator, 2,0),
    JS_FN("intl_Collator_availableLocales", intl_Collator_availableLocales, 0,0),
    JS_FN("intl_CompareStrings", intl_CompareStrings, 3,0),
    JS_FN("intl_DateTimeFormat", intl_DateTimeFormat, 2,0),
    JS_FN("intl_DateTimeFormat_availableLocales", intl_DateTimeFormat_availableLocales, 0,0),
    JS_FN("intl_FormatDateTime", intl_FormatDateTime, 2,0),
    JS_FN("intl_FormatNumber", intl_FormatNumber, 2,0),
    JS_FN("intl_NumberFormat", intl_NumberFormat, 2,0),
    JS_FN("intl_NumberFormat_availableLocales", intl_NumberFormat_availableLocales, 0,0),
    JS_FN("intl_numberingSystem", intl_numberingSystem, 1,0),
    JS_FN("intl_patternForSkeleton", intl_patternForSkeleton, 2,0),

    JS_INLINABLE_FN("IsRegExpObject",
                    intrinsic_IsInstanceOfBuiltin<RegExpObject>, 1,0,
                    IsRegExpObject),
    JS_FN("CallRegExpMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<RegExpObject>>, 2,0),
    JS_INLINABLE_FN("RegExpMatcher", RegExpMatcher, 4,0,
                    RegExpMatcher),
    JS_INLINABLE_FN("RegExpSearcher", RegExpSearcher, 4,0,
                    RegExpSearcher),
    JS_INLINABLE_FN("RegExpTester", RegExpTester, 4,0,
                    RegExpTester),
    JS_FN("RegExpCreate", intrinsic_RegExpCreate, 2,0),
    JS_INLINABLE_FN("RegExpPrototypeOptimizable", RegExpPrototypeOptimizable, 1,0,
                    RegExpPrototypeOptimizable),
    JS_INLINABLE_FN("RegExpInstanceOptimizable", RegExpInstanceOptimizable, 1,0,
                    RegExpInstanceOptimizable),
    JS_FN("RegExpGetSubstitution", intrinsic_RegExpGetSubstitution, 6,0),
    JS_FN("GetElemBaseForLambda", intrinsic_GetElemBaseForLambda, 1,0),
    JS_FN("GetStringDataProperty", intrinsic_GetStringDataProperty, 2,0),
    JS_INLINABLE_FN("GetFirstDollarIndex", GetFirstDollarIndex, 1,0,
                    GetFirstDollarIndex),

    JS_FN("FlatStringMatch", FlatStringMatch, 2,0),
    JS_FN("FlatStringSearch", FlatStringSearch, 2,0),
    JS_INLINABLE_FN("StringReplaceString", intrinsic_StringReplaceString, 3, 0,
                    IntrinsicStringReplaceString),
    JS_INLINABLE_FN("StringSplitString", intrinsic_StringSplitString, 2, 0,
                    IntrinsicStringSplitString),
    JS_FN("StringSplitStringLimit", intrinsic_StringSplitStringLimit, 3, 0),

    // See builtin/RegExp.h for descriptions of the regexp_* functions.
    JS_FN("regexp_exec_no_statics", regexp_exec_no_statics, 2,0),
    JS_FN("regexp_test_no_statics", regexp_test_no_statics, 2,0),
    JS_FN("regexp_construct_no_sticky", regexp_construct_no_sticky, 2,0),

    JS_FN("IsModule", intrinsic_IsInstanceOfBuiltin<ModuleObject>, 1, 0),
    JS_FN("CallModuleMethodIfWrapped",
          CallNonGenericSelfhostedMethod<Is<ModuleObject>>, 2, 0),
    JS_FN("HostResolveImportedModule", intrinsic_HostResolveImportedModule, 2, 0),
    JS_FN("GetModuleEnvironment", intrinsic_GetModuleEnvironment, 1, 0),
    JS_FN("CreateModuleEnvironment", intrinsic_CreateModuleEnvironment, 1, 0),
    JS_FN("CreateImportBinding", intrinsic_CreateImportBinding, 4, 0),
    JS_FN("CreateNamespaceBinding", intrinsic_CreateNamespaceBinding, 3, 0),
    JS_FN("InstantiateModuleFunctionDeclarations",
          intrinsic_InstantiateModuleFunctionDeclarations, 1, 0),
    JS_FN("SetModuleEvaluated", intrinsic_SetModuleEvaluated, 1, 0),
    JS_FN("EvaluateModule", intrinsic_EvaluateModule, 1, 0),
    JS_FN("IsModuleNamespace", intrinsic_IsInstanceOfBuiltin<ModuleNamespaceObject>, 1, 0),
    JS_FN("NewModuleNamespace", intrinsic_NewModuleNamespace, 2, 0),
    JS_FN("AddModuleNamespaceBinding", intrinsic_AddModuleNamespaceBinding, 4, 0),
    JS_FN("ModuleNamespaceExports", intrinsic_ModuleNamespaceExports, 1, 0),

    JS_FN("_dbg_onPromiseSettled", intrinsic_onPromiseSettled, 1, 0),

    JS_FS_END
};

void
js::FillSelfHostingCompileOptions(CompileOptions& options)
{
    /*
     * In self-hosting mode, scripts use JSOP_GETINTRINSIC instead of
     * JSOP_GETNAME or JSOP_GETGNAME to access unbound variables.
     * JSOP_GETINTRINSIC does a name lookup on a special object, whose
     * properties are filled in lazily upon first access for a given global.
     *
     * As that object is inaccessible to client code, the lookups are
     * guaranteed to return the original objects, ensuring safe implementation
     * of self-hosted builtins.
     *
     * Additionally, the special syntax callFunction(fun, receiver, ...args)
     * is supported, for which bytecode is emitted that invokes |fun| with
     * |receiver| as the this-object and ...args as the arguments.
     */
    options.setIntroductionType("self-hosted");
    options.setFileAndLine("self-hosted", 1);
    options.setSelfHostingMode(true);
    options.setCanLazilyParse(false);
    options.setVersion(JSVERSION_LATEST);
    options.werrorOption = true;
    options.strictOption = true;

#ifdef DEBUG
    options.extraWarningsOption = true;
#endif
}

GlobalObject*
JSRuntime::createSelfHostingGlobal(JSContext* cx)
{
    MOZ_ASSERT(!cx->isExceptionPending());
    MOZ_ASSERT(!cx->runtime()->isAtomsCompartment(cx->compartment()));

    JS::CompartmentOptions options;
    options.creationOptions().setZone(JS::FreshZone);
    options.behaviors().setDiscardSource(true);

    JSCompartment* compartment = NewCompartment(cx, nullptr, nullptr, options);
    if (!compartment)
        return nullptr;

    static const ClassOps shgClassOps = {
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr,
        JS_GlobalObjectTraceHook
    };

    static const Class shgClass = {
        "self-hosting-global", JSCLASS_GLOBAL_FLAGS,
        &shgClassOps
    };

    AutoCompartment ac(cx, compartment);
    Rooted<GlobalObject*> shg(cx, GlobalObject::createInternal(cx, &shgClass));
    if (!shg)
        return nullptr;

    cx->runtime()->selfHostingGlobal_ = shg;
    compartment->isSelfHosting = true;
    compartment->setIsSystem(true);

    if (!GlobalObject::initSelfHostingBuiltins(cx, shg, intrinsic_functions))
        return nullptr;

    JS_FireOnNewGlobalObject(cx, shg);

    return shg;
}

static void
MaybePrintAndClearPendingException(JSContext* cx, FILE* file)
{
    if (!cx->isExceptionPending())
        return;

    AutoClearPendingException acpe(cx);

    RootedValue exn(cx);
    if (!cx->getPendingException(&exn)) {
        fprintf(file, "error getting pending exception\n");
        return;
    }
    cx->clearPendingException();

    ErrorReport report(cx);
    if (!report.init(cx, exn, js::ErrorReport::WithSideEffects)) {
        fprintf(file, "out of memory initializing ErrorReport\n");
        return;
    }

    MOZ_ASSERT(!JSREPORT_IS_WARNING(report.report()->flags));
    PrintError(cx, file, report.message(), report.report(), true);
}

class MOZ_STACK_CLASS AutoSelfHostingErrorReporter
{
    JSContext* cx_;
    bool prevDontReportUncaught_;
    bool prevAutoJSAPIOwnsErrorReporting_;
    JSErrorReporter oldReporter_;

  public:
    explicit AutoSelfHostingErrorReporter(JSContext* cx)
      : cx_(cx),
        prevDontReportUncaught_(cx_->options().dontReportUncaught()),
        prevAutoJSAPIOwnsErrorReporting_(cx_->options().autoJSAPIOwnsErrorReporting())
    {
        cx_->options().setDontReportUncaught(true);
        cx_->options().setAutoJSAPIOwnsErrorReporting(true);

        oldReporter_ = JS_SetErrorReporter(cx_->runtime(), selfHosting_WarningReporter);
    }
    ~AutoSelfHostingErrorReporter() {
        cx_->options().setDontReportUncaught(prevDontReportUncaught_);
        cx_->options().setAutoJSAPIOwnsErrorReporting(prevAutoJSAPIOwnsErrorReporting_);

        JS_SetErrorReporter(cx_->runtime(), oldReporter_);

        // Exceptions in self-hosted code will usually be printed to stderr in
        // ErrorToException, but not all exceptions are handled there. For
        // instance, ReportOutOfMemory will throw the "out of memory" string
        // without going through ErrorToException. We handle these other
        // exceptions here.
        MaybePrintAndClearPendingException(cx_, stderr);
    }
};

bool
JSRuntime::initSelfHosting(JSContext* cx)
{
    MOZ_ASSERT(!selfHostingGlobal_);

    if (cx->runtime()->parentRuntime) {
        selfHostingGlobal_ = cx->runtime()->parentRuntime->selfHostingGlobal_;
        return true;
    }

    /*
     * Self hosted state can be accessed from threads for other runtimes
     * parented to this one, so cannot include state in the nursery.
     */
    JS::AutoDisableGenerationalGC disable(cx->runtime());

    Rooted<GlobalObject*> shg(cx, JSRuntime::createSelfHostingGlobal(cx));
    if (!shg)
        return false;

    JSAutoCompartment ac(cx, shg);

    /*
     * Set a temporary error reporter printing to stderr because it is too
     * early in the startup process for any other reporter to be registered
     * and we don't want errors in self-hosted code to be silently swallowed.
     *
     * This class also overrides the warning reporter to print warnings to
     * stderr. See selfHosting_WarningReporter.
     */
    AutoSelfHostingErrorReporter errorReporter(cx);

    CompileOptions options(cx);
    FillSelfHostingCompileOptions(options);

    RootedValue rv(cx);

    char* filename = getenv("MOZ_SELFHOSTEDJS");
    if (filename) {
        if (!Evaluate(cx, options, filename, &rv))
            return false;
    } else {
        uint32_t srcLen = GetRawScriptsSize();

        const unsigned char* compressed = compressedSources;
        uint32_t compressedLen = GetCompressedSize();
        ScopedJSFreePtr<char> src(selfHostingGlobal_->zone()->pod_malloc<char>(srcLen));
        if (!src || !DecompressString(compressed, compressedLen,
                                      reinterpret_cast<unsigned char*>(src.get()), srcLen))
        {
            return false;
        }

        if (!Evaluate(cx, options, src, srcLen, &rv))
            return false;
    }
    return true;
}

void
JSRuntime::finishSelfHosting()
{
    selfHostingGlobal_ = nullptr;
}

void
JSRuntime::markSelfHostingGlobal(JSTracer* trc)
{
    if (selfHostingGlobal_ && !parentRuntime)
        TraceRoot(trc, &selfHostingGlobal_, "self-hosting global");
}

bool
JSRuntime::isSelfHostingCompartment(JSCompartment* comp) const
{
    return selfHostingGlobal_->compartment() == comp;
}

bool
JSRuntime::isSelfHostingZone(const JS::Zone* zone) const
{
    return selfHostingGlobal_ && selfHostingGlobal_->zoneFromAnyThread() == zone;
}

static bool
CloneValue(JSContext* cx, HandleValue selfHostedValue, MutableHandleValue vp);

static bool
GetUnclonedValue(JSContext* cx, HandleNativeObject selfHostedObject,
                 HandleId id, MutableHandleValue vp)
{
    vp.setUndefined();

    if (JSID_IS_INT(id)) {
        size_t index = JSID_TO_INT(id);
        if (index < selfHostedObject->getDenseInitializedLength() &&
            !selfHostedObject->getDenseElement(index).isMagic(JS_ELEMENTS_HOLE))
        {
            vp.set(selfHostedObject->getDenseElement(JSID_TO_INT(id)));
            return true;
        }
    }

    // Since all atoms used by self hosting are marked as permanent, any
    // attempt to look up a non-permanent atom will fail. We should only
    // see such atoms when code is looking for properties on the self
    // hosted global which aren't present.
    if (JSID_IS_STRING(id) && !JSID_TO_STRING(id)->isPermanentAtom()) {
        MOZ_ASSERT(selfHostedObject->is<GlobalObject>());
        RootedValue value(cx, IdToValue(id));
        return ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_NO_SUCH_SELF_HOSTED_PROP,
                                     JSDVG_IGNORE_STACK, value, nullptr, nullptr, nullptr);
    }

    RootedShape shape(cx, selfHostedObject->lookupPure(id));
    if (!shape) {
        RootedValue value(cx, IdToValue(id));
        return ReportValueErrorFlags(cx, JSREPORT_ERROR, JSMSG_NO_SUCH_SELF_HOSTED_PROP,
                                     JSDVG_IGNORE_STACK, value, nullptr, nullptr, nullptr);
    }

    MOZ_ASSERT(shape->hasSlot() && shape->hasDefaultGetter());
    vp.set(selfHostedObject->getSlot(shape->slot()));
    return true;
}

static bool
CloneProperties(JSContext* cx, HandleNativeObject selfHostedObject, HandleObject clone)
{
    AutoIdVector ids(cx);
    Vector<uint8_t, 16> attrs(cx);

    for (size_t i = 0; i < selfHostedObject->getDenseInitializedLength(); i++) {
        if (!selfHostedObject->getDenseElement(i).isMagic(JS_ELEMENTS_HOLE)) {
            if (!ids.append(INT_TO_JSID(i)))
                return false;
            if (!attrs.append(JSPROP_ENUMERATE))
                return false;
        }
    }

    Rooted<ShapeVector> shapes(cx, ShapeVector(cx));
    for (Shape::Range<NoGC> range(selfHostedObject->lastProperty()); !range.empty(); range.popFront()) {
        Shape& shape = range.front();
        if (shape.enumerable() && !shapes.append(&shape))
            return false;
    }

    // Now our shapes are in last-to-first order, so....
    Reverse(shapes.begin(), shapes.end());
    for (size_t i = 0; i < shapes.length(); ++i) {
        MOZ_ASSERT(!shapes[i]->isAccessorShape(),
                   "Can't handle cloning accessors here yet.");
        if (!ids.append(shapes[i]->propid()))
            return false;
        uint8_t shapeAttrs =
            shapes[i]->attributes() & (JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY);
        if (!attrs.append(shapeAttrs))
            return false;
    }

    RootedId id(cx);
    RootedValue val(cx);
    RootedValue selfHostedValue(cx);
    for (uint32_t i = 0; i < ids.length(); i++) {
        id = ids[i];
        if (!GetUnclonedValue(cx, selfHostedObject, id, &selfHostedValue))
            return false;
        if (!CloneValue(cx, selfHostedValue, &val) ||
            !JS_DefinePropertyById(cx, clone, id, val, attrs[i]))
        {
            return false;
        }
    }

    return true;
}

static JSString*
CloneString(JSContext* cx, JSFlatString* selfHostedString)
{
    size_t len = selfHostedString->length();
    {
        JS::AutoCheckCannotGC nogc;
        JSString* clone;
        if (selfHostedString->hasLatin1Chars())
            clone = NewStringCopyN<NoGC>(cx, selfHostedString->latin1Chars(nogc), len);
        else
            clone = NewStringCopyNDontDeflate<NoGC>(cx, selfHostedString->twoByteChars(nogc), len);
        if (clone)
            return clone;
    }

    AutoStableStringChars chars(cx);
    if (!chars.init(cx, selfHostedString))
        return nullptr;

    return chars.isLatin1()
           ? NewStringCopyN<CanGC>(cx, chars.latin1Range().start().get(), len)
           : NewStringCopyNDontDeflate<CanGC>(cx, chars.twoByteRange().start().get(), len);
}

static JSObject*
CloneObject(JSContext* cx, HandleNativeObject selfHostedObject)
{
#ifdef DEBUG
    // Object hash identities are owned by the hashed object, which may be on a
    // different thread than the clone target. In theory, these objects are all
    // tenured and will not be compacted; however, we simply avoid the issue
    // altogether by skipping the cycle-detection when off the main thread.
    mozilla::Maybe<AutoCycleDetector> detect;
    if (js::CurrentThreadCanAccessZone(selfHostedObject->zoneFromAnyThread())) {
        detect.emplace(cx, selfHostedObject);
        if (!detect->init())
            return nullptr;
        if (detect->foundCycle())
            MOZ_CRASH("SelfHosted cloning cannot handle cyclic object graphs.");
    }
#endif

    RootedObject clone(cx);
    if (selfHostedObject->is<JSFunction>()) {
        RootedFunction selfHostedFunction(cx, &selfHostedObject->as<JSFunction>());
        bool hasName = selfHostedFunction->name() != nullptr;

        // Arrow functions use the first extended slot for their lexical |this| value.
        MOZ_ASSERT(!selfHostedFunction->isArrow());
        js::gc::AllocKind kind = hasName
                                 ? gc::AllocKind::FUNCTION_EXTENDED
                                 : selfHostedFunction->getAllocKind();
        MOZ_ASSERT(!CanReuseScriptForClone(cx->compartment(), selfHostedFunction, cx->global()));
        Rooted<ClonedBlockObject*> globalLexical(cx, &cx->global()->lexicalScope());
        RootedObject staticGlobalLexical(cx, &globalLexical->staticBlock());
        clone = CloneFunctionAndScript(cx, selfHostedFunction, globalLexical,
                                       staticGlobalLexical, kind);
        // To be able to re-lazify the cloned function, its name in the
        // self-hosting compartment has to be stored on the clone.
        if (clone && hasName) {
            clone->as<JSFunction>().setExtendedSlot(LAZY_FUNCTION_NAME_SLOT,
                                                    StringValue(selfHostedFunction->name()));
        }
    } else if (selfHostedObject->is<RegExpObject>()) {
        RegExpObject& reobj = selfHostedObject->as<RegExpObject>();
        RootedAtom source(cx, reobj.getSource());
        MOZ_ASSERT(source->isPermanentAtom());
        clone = RegExpObject::create(cx, source, reobj.getFlags(), nullptr, cx->tempLifoAlloc());
    } else if (selfHostedObject->is<DateObject>()) {
        clone = JS::NewDateObject(cx, selfHostedObject->as<DateObject>().clippedTime());
    } else if (selfHostedObject->is<BooleanObject>()) {
        clone = BooleanObject::create(cx, selfHostedObject->as<BooleanObject>().unbox());
    } else if (selfHostedObject->is<NumberObject>()) {
        clone = NumberObject::create(cx, selfHostedObject->as<NumberObject>().unbox());
    } else if (selfHostedObject->is<StringObject>()) {
        JSString* selfHostedString = selfHostedObject->as<StringObject>().unbox();
        if (!selfHostedString->isFlat())
            MOZ_CRASH();
        RootedString str(cx, CloneString(cx, &selfHostedString->asFlat()));
        if (!str)
            return nullptr;
        clone = StringObject::create(cx, str);
    } else if (selfHostedObject->is<ArrayObject>()) {
        clone = NewDenseEmptyArray(cx, nullptr, TenuredObject);
    } else {
        MOZ_ASSERT(selfHostedObject->isNative());
        clone = NewObjectWithGivenProto(cx, selfHostedObject->getClass(), nullptr,
                                        selfHostedObject->asTenured().getAllocKind(),
                                        SingletonObject);
    }
    if (!clone)
        return nullptr;

    if (!CloneProperties(cx, selfHostedObject, clone))
        return nullptr;
    return clone;
}

static bool
CloneValue(JSContext* cx, HandleValue selfHostedValue, MutableHandleValue vp)
{
    if (selfHostedValue.isObject()) {
        RootedNativeObject selfHostedObject(cx, &selfHostedValue.toObject().as<NativeObject>());
        JSObject* clone = CloneObject(cx, selfHostedObject);
        if (!clone)
            return false;
        vp.setObject(*clone);
    } else if (selfHostedValue.isBoolean() || selfHostedValue.isNumber() || selfHostedValue.isNullOrUndefined()) {
        // Nothing to do here: these are represented inline in the value.
        vp.set(selfHostedValue);
    } else if (selfHostedValue.isString()) {
        if (!selfHostedValue.toString()->isFlat())
            MOZ_CRASH();
        JSFlatString* selfHostedString = &selfHostedValue.toString()->asFlat();
        JSString* clone = CloneString(cx, selfHostedString);
        if (!clone)
            return false;
        vp.setString(clone);
    } else if (selfHostedValue.isSymbol()) {
        // Well-known symbols are shared.
        mozilla::DebugOnly<JS::Symbol*> sym = selfHostedValue.toSymbol();
        MOZ_ASSERT(sym->isWellKnownSymbol());
        MOZ_ASSERT(cx->wellKnownSymbols().get(size_t(sym->code())) == sym);
        vp.set(selfHostedValue);
    } else {
        MOZ_CRASH("Self-hosting CloneValue can't clone given value.");
    }
    return true;
}

bool
JSRuntime::createLazySelfHostedFunctionClone(JSContext* cx, HandlePropertyName selfHostedName,
                                             HandleAtom name, unsigned nargs,
                                             HandleObject proto, NewObjectKind newKind,
                                             MutableHandleFunction fun)
{
    MOZ_ASSERT(newKind != GenericObject);

    RootedAtom funName(cx, name);
    JSFunction* selfHostedFun = getUnclonedSelfHostedFunction(cx, selfHostedName);
    if (!selfHostedFun)
        return false;

    if (!selfHostedFun->hasGuessedAtom() && selfHostedFun->name() != selfHostedName) {
        MOZ_ASSERT(selfHostedFun->getExtendedSlot(HAS_SELFHOSTED_CANONICAL_NAME_SLOT).toBoolean());
        funName = selfHostedFun->name();
    }

    fun.set(NewScriptedFunction(cx, nargs, JSFunction::INTERPRETED_LAZY,
                                funName, proto, gc::AllocKind::FUNCTION_EXTENDED, newKind));
    if (!fun)
        return false;
    fun->setIsSelfHostedBuiltin();
    fun->setExtendedSlot(LAZY_FUNCTION_NAME_SLOT, StringValue(selfHostedName));
    return true;
}

bool
JSRuntime::cloneSelfHostedFunctionScript(JSContext* cx, HandlePropertyName name,
                                         HandleFunction targetFun)
{
    RootedFunction sourceFun(cx, getUnclonedSelfHostedFunction(cx, name));
    if (!sourceFun)
        return false;
    // JSFunction::generatorKind can't handle lazy self-hosted functions, so we make sure there
    // aren't any.
    MOZ_ASSERT(!sourceFun->isGenerator());
    MOZ_ASSERT(targetFun->isExtended());
    MOZ_ASSERT(targetFun->isInterpretedLazy());
    MOZ_ASSERT(targetFun->isSelfHostedBuiltin());

    RootedScript sourceScript(cx, sourceFun->getOrCreateScript(cx));
    if (!sourceScript)
        return false;

    // Assert that there are no intervening scopes between the global scope
    // and the self-hosted script. Toplevel lexicals are explicitly forbidden
    // by the parser when parsing self-hosted code. The fact they have the
    // global lexical scope on the scope chain is for uniformity and engine
    // invariants.
    MOZ_ASSERT(IsStaticGlobalLexicalScope(sourceScript->enclosingStaticScope()));
    Rooted<StaticScope*> enclosingScope(cx, &cx->global()->lexicalScope().staticBlock());
    if (!CloneScriptIntoFunction(cx, enclosingScope, targetFun, sourceScript))
        return false;
    MOZ_ASSERT(!targetFun->isInterpretedLazy());

    MOZ_ASSERT(sourceFun->nargs() == targetFun->nargs());
    MOZ_ASSERT(sourceFun->hasRest() == targetFun->hasRest());

    // The target function might have been relazified after its flags changed.
    targetFun->setFlags(targetFun->flags() | sourceFun->flags());
    return true;
}

bool
JSRuntime::getUnclonedSelfHostedValue(JSContext* cx, HandlePropertyName name,
                                      MutableHandleValue vp)
{
    RootedId id(cx, NameToId(name));
    return GetUnclonedValue(cx, HandleNativeObject::fromMarkedLocation(&selfHostingGlobal_), id, vp);
}

JSFunction*
JSRuntime::getUnclonedSelfHostedFunction(JSContext* cx, HandlePropertyName name)
{
    RootedValue selfHostedValue(cx);
    if (!getUnclonedSelfHostedValue(cx, name, &selfHostedValue))
        return nullptr;

    return &selfHostedValue.toObject().as<JSFunction>();
}

bool
JSRuntime::cloneSelfHostedValue(JSContext* cx, HandlePropertyName name, MutableHandleValue vp)
{
    RootedValue selfHostedValue(cx);
    if (!getUnclonedSelfHostedValue(cx, name, &selfHostedValue))
        return false;

    /*
     * We don't clone if we're operating in the self-hosting global, as that
     * means we're currently executing the self-hosting script while
     * initializing the runtime (see JSRuntime::initSelfHosting).
     */
    if (cx->global() == selfHostingGlobal_) {
        vp.set(selfHostedValue);
        return true;
    }

    return CloneValue(cx, selfHostedValue, vp);
}

void
JSRuntime::assertSelfHostedFunctionHasCanonicalName(JSContext* cx, HandlePropertyName name)
{
#ifdef DEBUG
    JSFunction* selfHostedFun = getUnclonedSelfHostedFunction(cx, name);
    MOZ_ASSERT(selfHostedFun);
    MOZ_ASSERT(selfHostedFun->getExtendedSlot(HAS_SELFHOSTED_CANONICAL_NAME_SLOT).toBoolean());
#endif
}

JSFunction*
js::SelfHostedFunction(JSContext* cx, HandlePropertyName propName)
{
    RootedValue func(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), propName, &func))
        return nullptr;

    MOZ_ASSERT(func.isObject());
    MOZ_ASSERT(func.toObject().is<JSFunction>());
    return &func.toObject().as<JSFunction>();
}

bool
js::IsSelfHostedFunctionWithName(JSFunction* fun, JSAtom* name)
{
    return fun->isSelfHostedBuiltin() && GetSelfHostedFunctionName(fun) == name;
}

JSAtom*
js::GetSelfHostedFunctionName(JSFunction* fun)
{
    return &fun->getExtendedSlot(LAZY_FUNCTION_NAME_SLOT).toString()->asAtom();
}

static_assert(JSString::MAX_LENGTH <= INT32_MAX,
              "StringIteratorNext in builtin/String.js assumes the stored index "
              "into the string is an Int32Value");
