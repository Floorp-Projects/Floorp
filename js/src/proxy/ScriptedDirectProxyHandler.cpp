/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "proxy/ScriptedDirectProxyHandler.h"

#include "jsapi.h"

#include "jsobjinlines.h"
#include "vm/NativeObject-inl.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using JS::IsArrayAnswer;
using mozilla::ArrayLength;

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93
// 9.1.6.2 IsCompatiblePropertyDescriptor.  BUT that method just calls
// 9.1.6.3 ValidateAndApplyPropertyDescriptor with two additional constant
// arguments.  Therefore step numbering is from the latter method, and
// resulting dead code has been removed.
static bool
IsCompatiblePropertyDescriptor(JSContext* cx, bool extensible, Handle<PropertyDescriptor> desc,
                               Handle<PropertyDescriptor> current, bool* bp)
{
    // Step 2.
    if (!current.object()) {
        // Step 2a-b,e.  As |O| is always undefined, steps 2c-d fall away.
        *bp = extensible;
        return true;
    }

    // Step 3.
    if (!desc.hasValue() && !desc.hasWritable() &&
        !desc.hasGetterObject() && !desc.hasSetterObject() &&
        !desc.hasEnumerable() && !desc.hasConfigurable())
    {
        *bp = true;
        return true;
    }

    // Step 4.
    if ((!desc.hasWritable() ||
         (current.hasWritable() && desc.writable() == current.writable())) &&
        (!desc.hasGetterObject() || desc.getter() == current.getter()) &&
        (!desc.hasSetterObject() || desc.setter() == current.setter()) &&
        (!desc.hasEnumerable() || desc.enumerable() == current.enumerable()) &&
        (!desc.hasConfigurable() || desc.configurable() == current.configurable()))
    {
        if (!desc.hasValue()) {
            *bp = true;
            return true;
        }
        bool same = false;
        if (!SameValue(cx, desc.value(), current.value(), &same))
            return false;
        if (same) {
            *bp = true;
            return true;
        }
    }

    // Step 5.
    if (!current.configurable()) {
        // Step 5a.
        if (desc.hasConfigurable() && desc.configurable()) {
            *bp = false;
            return true;
        }

        // Step 5b.
        if (desc.hasEnumerable() && desc.enumerable() != current.enumerable()) {
            *bp = false;
            return true;
        }
    }

    // Step 6.
    if (desc.isGenericDescriptor()) {
        *bp = true;
        return true;
    }

    // Step 7.
    if (current.isDataDescriptor() != desc.isDataDescriptor()) {
        // Steps 7a, 11.  As |O| is always undefined, steps 2b-c fall away.
        *bp = current.configurable();
        return true;
    }

    // Step 8.
    if (current.isDataDescriptor()) {
        MOZ_ASSERT(desc.isDataDescriptor()); // by step 7
        if (!current.configurable() && !current.writable()) {
            if (desc.hasWritable() && desc.writable()) {
                *bp = false;
                return true;
            }

            if (desc.hasValue()) {
                bool same;
                if (!SameValue(cx, desc.value(), current.value(), &same))
                    return false;
                if (!same) {
                    *bp = false;
                    return true;
                }
            }
        }

        *bp = true;
        return true;
    }

    // Step 9.
    MOZ_ASSERT(current.isAccessorDescriptor()); // by step 8
    MOZ_ASSERT(desc.isAccessorDescriptor()); // by step 7
    *bp = (current.configurable() ||
           ((!desc.hasSetterObject() || desc.setter() == current.setter()) &&
            (!desc.hasGetterObject() || desc.getter() == current.getter())));
    return true;
}

// Get the [[ProxyHandler]] of a scripted direct proxy.
static JSObject*
GetDirectProxyHandlerObject(JSObject* proxy)
{
    MOZ_ASSERT(proxy->as<ProxyObject>().handler() == &ScriptedDirectProxyHandler::singleton);
    return proxy->as<ProxyObject>().extra(ScriptedDirectProxyHandler::HANDLER_EXTRA).toObjectOrNull();
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 7.3.9 GetMethod,
// reimplemented for proxy handler trap-getting to produce better error
// messages.
static bool
GetProxyTrap(JSContext* cx, HandleObject handler, HandlePropertyName name, MutableHandleValue func)
{
    // Steps 2, 5.
    if (!GetProperty(cx, handler, handler, name, func))
        return false;

    // Step 3.
    if (func.isUndefined())
        return true;

    if (func.isNull()) {
        func.setUndefined();
        return true;
    }

    // Step 4.
    if (!IsCallable(func)) {
        JSAutoByteString bytes(cx, name);
        if (!bytes)
            return false;

        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_BAD_TRAP, bytes.ptr());
        return false;
    }

    return true;
}

// ES6 implements both getPrototype and setPrototype traps. We don't have them yet (see bug
// 888969). For now, use these, to account for proxy revocation.
bool
ScriptedDirectProxyHandler::getPrototype(JSContext* cx, HandleObject proxy,
                                         MutableHandleObject protop) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    // Though handler is used elsewhere, spec mandates that both get set to null.
    if (!target) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    return GetPrototype(cx, target, protop);
}

bool
ScriptedDirectProxyHandler::setPrototype(JSContext* cx, HandleObject proxy, HandleObject proto,
                                         ObjectOpResult& result) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (!target) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    return SetPrototype(cx, target, proto, result);
}

// Not yet part of ES6, but hopefully to be standards-tracked -- and needed to
// handle revoked proxies in any event.
bool
ScriptedDirectProxyHandler::setImmutablePrototype(JSContext* cx, HandleObject proxy,
                                                  bool* succeeded) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (!target) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    return SetImmutablePrototype(cx, target, succeeded);
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.4 Proxy.[[PreventExtensions]]()
bool
ScriptedDirectProxyHandler::preventExtensions(JSContext* cx, HandleObject proxy,
                                              ObjectOpResult& result) const
{
    // Steps 1-3.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 4.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Step 5.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().preventExtensions, &trap))
        return false;

    // Step 6.
    if (trap.isUndefined())
        return PreventExtensions(cx, target, result);

    // Step 7.
    bool booleanTrapResult;
    {
        Value argv[] = {
            ObjectValue(*target)
        };
        RootedValue trapResult(cx);
        if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
            return false;

        booleanTrapResult = ToBoolean(trapResult);
    }

    // Step 8.
    if (booleanTrapResult) {
        // Step 8a.
        bool targetIsExtensible;
        if (!IsExtensible(cx, target, &targetIsExtensible))
            return false;

        if (targetIsExtensible) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr,
                                 JSMSG_CANT_REPORT_AS_NON_EXTENSIBLE);
            return false;
        }

        // Step 9.
        return result.succeed();
    }

    // Also step 9.
    return result.fail(JSMSG_PROXY_PREVENTEXTENSIONS_RETURNED_FALSE);
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.3 Proxy.[[IsExtensible]]()
bool
ScriptedDirectProxyHandler::isExtensible(JSContext* cx, HandleObject proxy, bool* extensible) const
{
    // Steps 1-3.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 4.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Step 5.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().isExtensible, &trap))
        return false;

    // Step 6.
    if (trap.isUndefined())
        return IsExtensible(cx, target, extensible);

    // Step 7.
    Value argv[] = {
        ObjectValue(*target)
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    bool booleanTrapResult = ToBoolean(trapResult);

    // Steps 8.
    bool targetResult;
    if (!IsExtensible(cx, target, &targetResult))
        return false;

    // Step 9.
    if (targetResult != booleanTrapResult) {
       JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_EXTENSIBILITY);
       return false;
    }

    // Step 10.
    *extensible = booleanTrapResult;
    return true;
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.5 Proxy.[[GetOwnProperty]](P)
bool
ScriptedDirectProxyHandler::getOwnPropertyDescriptor(JSContext* cx, HandleObject proxy, HandleId id,
                                                     MutableHandle<PropertyDescriptor> desc) const
{
    // Steps 2-4.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 5.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Step 6.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().getOwnPropertyDescriptor, &trap))
        return false;

    // Step 7.
    if (trap.isUndefined())
        return GetOwnPropertyDescriptor(cx, target, id, desc);

    // Step 8.
    RootedValue propKey(cx);
    if (!IdToStringOrSymbol(cx, id, &propKey))
        return false;

    Value argv[] = {
        ObjectValue(*target),
        propKey
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // Step 9.
    if (!trapResult.isUndefined() && !trapResult.isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_GETOWN_OBJORUNDEF);
        return false;
    }

    // Step 10.
    Rooted<PropertyDescriptor> targetDesc(cx);
    if (!GetOwnPropertyDescriptor(cx, target, id, &targetDesc))
        return false;

    // Step 11.
    if (trapResult.isUndefined()) {
        // Step 11a.
        if (!targetDesc.object()) {
            desc.object().set(nullptr);
            return true;
        }

        // Step 11b.
        if (!targetDesc.configurable()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NC_AS_NE);
            return false;
        }

        // Steps 11c-d.
        bool extensibleTarget;
        if (!IsExtensible(cx, target, &extensibleTarget))
            return false;

        // Step 11e.
        if (!extensibleTarget) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_E_AS_NE);
            return false;
        }

        // Step 11f.
        desc.object().set(nullptr);
        return true;
    }

    // Step 12.
    bool extensibleTarget;
    if (!IsExtensible(cx, target, &extensibleTarget))
        return false;

    // Step 13.
    Rooted<PropertyDescriptor> resultDesc(cx);
    if (!ToPropertyDescriptor(cx, trapResult, true, &resultDesc))
        return false;

    // Step 14.
    CompletePropertyDescriptor(&resultDesc);

    // Step 15.
    bool valid;
    if (!IsCompatiblePropertyDescriptor(cx, extensibleTarget, resultDesc, targetDesc, &valid))
        return false;

    // Step 16.
    if (!valid) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_INVALID);
        return false;
    }

    // Step 17.
    if (!resultDesc.configurable()) {
        if (!targetDesc.object()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NE_AS_NC);
            return false;
        }

        if (targetDesc.configurable()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_C_AS_NC);
            return false;
        }
    }

    // Step 18.
    desc.set(resultDesc);
    desc.object().set(proxy);
    return true;
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.6 Proxy.[[DefineOwnProperty]](P, Desc)
bool
ScriptedDirectProxyHandler::defineProperty(JSContext* cx, HandleObject proxy, HandleId id,
                                           Handle<PropertyDescriptor> desc,
                                           ObjectOpResult& result) const
{
    // Steps 2-4.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 5.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Step 6.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().defineProperty, &trap))
        return false;

    // Step 7.
    if (trap.isUndefined())
        return DefineProperty(cx, target, id, desc, result);

    // Step 8.
    RootedValue descObj(cx);
    if (!FromPropertyDescriptorToObject(cx, desc, &descObj))
        return false;

    // Step 9.
    RootedValue propKey(cx);
    if (!IdToStringOrSymbol(cx, id, &propKey))
        return false;

    Value argv[] = {
        ObjectValue(*target),
        propKey,
        descObj
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // Step 10.
    if (!ToBoolean(trapResult))
        return result.fail(JSMSG_PROXY_DEFINE_RETURNED_FALSE);

    // Step 11.
    Rooted<PropertyDescriptor> targetDesc(cx);
    if (!GetOwnPropertyDescriptor(cx, target, id, &targetDesc))
        return false;

    // Step 12.
    bool extensibleTarget;
    if (!IsExtensible(cx, target, &extensibleTarget))
        return false;

    // Steps 13-14.
    bool settingConfigFalse = desc.hasConfigurable() && !desc.configurable();

    // Steps 15-16.
    if (!targetDesc.object()) {
        // Step 15a.
        if (!extensibleTarget) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_DEFINE_NEW);
            return false;
        }

        // Step 15b.
        if (settingConfigFalse) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_DEFINE_NE_AS_NC);
            return false;
        }
    } else {
        // Steps 16a-b.
        bool valid;
        if (!IsCompatiblePropertyDescriptor(cx, extensibleTarget, desc, targetDesc, &valid))
            return false;

        if (!valid || (settingConfigFalse && targetDesc.configurable())) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_DEFINE_INVALID);
            return false;
        }
    }

    // Step 17.
    return result.succeed();
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93
// 7.3.17 CreateListFromArrayLike with elementTypes fixed to symbol/string.
static bool
CreateFilteredListFromArrayLike(JSContext* cx, HandleValue v, AutoIdVector& props)
{
    // Step 2.
    RootedObject obj(cx, NonNullObject(cx, v));
    if (!obj)
        return false;

    // Step 3.
    uint32_t len;
    if (!GetLengthProperty(cx, obj, &len))
        return false;

    // Steps 4-6.
    RootedValue next(cx);
    RootedId id(cx);
    uint32_t index = 0;
    while (index < len) {
        // Steps 6a-b.
        if (!GetElement(cx, obj, obj, index, &next))
            return false;

        // Step 6c.
        if (!next.isString() && !next.isSymbol()) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_ONWKEYS_STR_SYM);
            return false;
        }

        if (!ValueToId<CanGC>(cx, next, &id))
            return false;

        // Step 6d.
        if (!props.append(id))
            return false;

        // Step 6e.
        index++;
    }

    // Step 7.
    return true;
}


// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.11 Proxy.[[OwnPropertyKeys]]()
bool
ScriptedDirectProxyHandler::ownPropertyKeys(JSContext* cx, HandleObject proxy,
                                            AutoIdVector& props) const
{
    // Steps 1-3.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 4.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Step 5.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().ownKeys, &trap))
        return false;

    // Step 6.
    if (trap.isUndefined())
        return GetPropertyKeys(cx, target, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &props);

    // Step 7.
    Value argv[] = {
        ObjectValue(*target)
    };
    RootedValue trapResultArray(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResultArray))
        return false;

    // Step 8.
    AutoIdVector trapResult(cx);
    if (!CreateFilteredListFromArrayLike(cx, trapResultArray, trapResult))
        return false;

    // Step 9.
    bool extensibleTarget;
    if (!IsExtensible(cx, target, &extensibleTarget))
        return false;

    // Steps 10-11.
    AutoIdVector targetKeys(cx);
    if (!GetPropertyKeys(cx, target, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, &targetKeys))
        return false;

    // Steps 12-13.
    AutoIdVector targetConfigurableKeys(cx);
    AutoIdVector targetNonconfigurableKeys(cx);

    // Step 14.
    Rooted<PropertyDescriptor> desc(cx);
    for (size_t i = 0; i < targetKeys.length(); ++i) {
        // Step 14a.
        if (!GetOwnPropertyDescriptor(cx, target, targetKeys[i], &desc))
            return false;

        // Steps 14b-c.
        if (desc.object() && !desc.configurable()) {
            if (!targetNonconfigurableKeys.append(targetKeys[i]))
                return false;
        } else {
            if (!targetConfigurableKeys.append(targetKeys[i]))
                return false;
        }
    }

    // Step 15.
    if (extensibleTarget && targetNonconfigurableKeys.empty())
        return props.appendAll(trapResult);

    // Step 16.
    AutoIdVector uncheckedResultKeys(cx);
    if (!uncheckedResultKeys.appendAll(trapResult))
        return false;

    // Step 17.
    RootedId key(cx);
    for (size_t i = 0; i < targetNonconfigurableKeys.length(); ++i) {
        key = targetNonconfigurableKeys[i];
        MOZ_ASSERT(key != JSID_VOID);

        bool found = false;
        for (size_t j = 0; j < uncheckedResultKeys.length(); ++j) {
            if (key == uncheckedResultKeys[j]) {
                uncheckedResultKeys[j].set(JSID_VOID);
                found = true;
                break;
            }
        }

        if (!found) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_SKIP_NC);
            return false;
        }
    }

    // Step 18.
    if (extensibleTarget)
        return props.appendAll(trapResult);

    // Step 19.
    for (size_t i = 0; i < targetConfigurableKeys.length(); ++i) {
        key = targetConfigurableKeys[i];
        MOZ_ASSERT(key != JSID_VOID);

        bool found = false;
        for (size_t j = 0; j < uncheckedResultKeys.length(); ++j) {
            if (key == uncheckedResultKeys[j]) {
                uncheckedResultKeys[j].set(JSID_VOID);
                found = true;
                break;
            }
        }

        if (!found) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_E_AS_NE);
            return false;
        }
    }

    // Step 20.
    for (size_t i = 0; i < uncheckedResultKeys.length(); ++i) {
        if (uncheckedResultKeys[i].get() != JSID_VOID) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NEW);
            return false;
        }
    }

    // Step 21.
    return props.appendAll(trapResult);
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.10 Proxy.[[Delete]](P)
bool
ScriptedDirectProxyHandler::delete_(JSContext* cx, HandleObject proxy, HandleId id,
                                    ObjectOpResult& result) const
{
    // Steps 2-4.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 5.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Step 6.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().deleteProperty, &trap))
        return false;

    // Step 7.
    if (trap.isUndefined())
        return DeleteProperty(cx, target, id, result);

    // Step 8.
    bool booleanTrapResult;
    {
        RootedValue value(cx);
        if (!IdToStringOrSymbol(cx, id, &value))
            return false;

        Value argv[] = {
            ObjectValue(*target),
            value
        };
        RootedValue trapResult(cx);
        if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
            return false;

        booleanTrapResult = ToBoolean(trapResult);
    }

    // Step 9.
    if (!booleanTrapResult)
        return result.fail(JSMSG_PROXY_DELETE_RETURNED_FALSE);

    // Step 10.
    Rooted<PropertyDescriptor> desc(cx);
    if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
        return false;

    // Step 12.
    if (desc.object() && !desc.configurable()) {
        RootedValue v(cx, IdToValue(id));
        ReportValueError(cx, JSMSG_CANT_DELETE, JSDVG_IGNORE_STACK, v, nullptr);
        return false;
    }

    // Steps 11,13.
    return result.succeed();
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.7 Proxy.[[HasProperty]](P)
bool
ScriptedDirectProxyHandler::has(JSContext* cx, HandleObject proxy, HandleId id, bool* bp) const
{
    // Steps 2-4.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 5.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Step 6.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().has, &trap))
        return false;

    // Step 7.
    if (trap.isUndefined())
        return HasProperty(cx, target, id, bp);

    // Step 8.
    RootedValue value(cx);
    if (!IdToStringOrSymbol(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    bool booleanTrapResult = ToBoolean(trapResult);

    // Step 9.
    if (!booleanTrapResult) {
        // Step 9a.
        Rooted<PropertyDescriptor> desc(cx);
        if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
            return false;

        // Step 9b.
        if (desc.object()) {
            // Step 9b(i).
            if (!desc.configurable()) {
                JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_NC_AS_NE);
                return false;
            }

            // Step 9b(ii).
            bool extensible;
            if (!IsExtensible(cx, target, &extensible))
                return false;

            // Step 9b(iii).
            if (!extensible) {
                JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_REPORT_E_AS_NE);
                return false;
            }
        }
    }

    // Step 10.
    *bp = booleanTrapResult;
    return true;
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.8 Proxy.[[GetP]](P, Receiver)
bool
ScriptedDirectProxyHandler::get(JSContext* cx, HandleObject proxy, HandleValue receiver,
                                HandleId id, MutableHandleValue vp) const
{
    // Steps 2-4.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 5.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Steps 6.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().get, &trap))
        return false;

    // Step 7.
    if (trap.isUndefined())
        return GetProperty(cx, target, receiver, id, vp);

    // Step 8.
    RootedValue value(cx);
    if (!IdToStringOrSymbol(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value,
        receiver
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // Step 9.
    Rooted<PropertyDescriptor> desc(cx);
    if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
        return false;

    // Step 10.
    if (desc.object()) {
        // Step 10a.
        if (desc.isDataDescriptor() && !desc.configurable() && !desc.writable()) {
            bool same;
            if (!SameValue(cx, trapResult, desc.value(), &same))
                return false;
            if (!same) {
                JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_MUST_REPORT_SAME_VALUE);
                return false;
            }
        }

        // Step 10b.
        if (desc.isAccessorDescriptor() && !desc.configurable() && desc.getterObject() == nullptr) {
            if (!trapResult.isUndefined()) {
                JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_MUST_REPORT_UNDEFINED);
                return false;
            }
        }
    }

    // Step 11.
    vp.set(trapResult);
    return true;
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.9 Proxy.[[Set]](P, V, Receiver)
bool
ScriptedDirectProxyHandler::set(JSContext* cx, HandleObject proxy, HandleId id, HandleValue v,
                                HandleValue receiver, ObjectOpResult& result) const
{
    // Steps 2-4.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 5.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);

    // Step 6.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().set, &trap))
        return false;

    // Step 7.
    if (trap.isUndefined())
        return SetProperty(cx, target, id, v, receiver, result);

    // Step 8.
    RootedValue value(cx);
    if (!IdToStringOrSymbol(cx, id, &value))
        return false;
    Value argv[] = {
        ObjectOrNullValue(target),
        value,
        v.get(),
        receiver.get()
    };
    RootedValue trapResult(cx);
    if (!Invoke(cx, ObjectValue(*handler), trap, ArrayLength(argv), argv, &trapResult))
        return false;

    // Step 9.
    if (!ToBoolean(trapResult))
        return result.fail(JSMSG_PROXY_SET_RETURNED_FALSE);

    // Step 10.
    Rooted<PropertyDescriptor> desc(cx);
    if (!GetOwnPropertyDescriptor(cx, target, id, &desc))
        return false;

    // Step 11.
    if (desc.object()) {
        // Step 11a.
        if (desc.isDataDescriptor() && !desc.configurable() && !desc.writable()) {
            bool same;
            if (!SameValue(cx, v, desc.value(), &same))
                return false;
            if (!same) {
                JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_SET_NW_NC);
                return false;
            }
        }

        // Step 11b.
        if (desc.isAccessorDescriptor() && !desc.configurable() && desc.setterObject() == nullptr) {
            JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_CANT_SET_WO_SETTER);
            return false;
        }
    }

    // Step 12.
    return result.succeed();
}

// ES7 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.13 Proxy.[[Call]]
bool
ScriptedDirectProxyHandler::call(JSContext* cx, HandleObject proxy, const CallArgs& args) const
{
    // Steps 1-3.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 4.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);
    MOZ_ASSERT(target->isCallable());

    // Step 5.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().apply, &trap))
        return false;

    // Step 6.
    if (trap.isUndefined()) {
        RootedValue targetv(cx, ObjectValue(*target));
        return Invoke(cx, args.thisv(), targetv, args.length(), args.array(), args.rval());
    }

    // Step 7.
    RootedObject argArray(cx, NewDenseCopiedArray(cx, args.length(), args.array()));
    if (!argArray)
        return false;

    // Step 8.
    Value argv[] = {
        ObjectValue(*target),
        args.thisv(),
        ObjectValue(*argArray)
    };
    RootedValue thisValue(cx, ObjectValue(*handler));
    return Invoke(cx, thisValue, trap, ArrayLength(argv), argv, args.rval());
}

// ES7 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.14 Proxy.[[Construct]]
bool
ScriptedDirectProxyHandler::construct(JSContext* cx, HandleObject proxy, const CallArgs& args) const
{
    // Steps 1-3.
    RootedObject handler(cx, GetDirectProxyHandlerObject(proxy));
    if (!handler) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    // Step 4.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    MOZ_ASSERT(target);
    MOZ_ASSERT(target->isConstructor());

    // Step 5.
    RootedValue trap(cx);
    if (!GetProxyTrap(cx, handler, cx->names().construct, &trap))
        return false;

    // Step 6.
    if (trap.isUndefined()) {
        ConstructArgs cargs(cx);
        if (!FillArgumentsFromArraylike(cx, cargs, args))
            return false;

        RootedValue targetv(cx, ObjectValue(*target));
        RootedObject obj(cx);
        if (!Construct(cx, targetv, cargs, args.newTarget(), &obj))
            return false;

        args.rval().setObject(*obj);
        return true;
    }

    // Step 7.
    RootedObject argArray(cx, NewDenseCopiedArray(cx, args.length(), args.array()));
    if (!argArray)
        return false;

    // Steps 8, 10.
    Value constructArgv[] = {
        ObjectValue(*target),
        ObjectValue(*argArray),
        args.newTarget()
    };
    RootedValue thisValue(cx, ObjectValue(*handler));
    if (!Invoke(cx, thisValue, trap, ArrayLength(constructArgv), constructArgv, args.rval()))
        return false;

    // Step 9.
    if (!args.rval().isObject()) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_CONSTRUCT_OBJECT);
        return false;
    }

    return true;
}

bool
ScriptedDirectProxyHandler::nativeCall(JSContext* cx, IsAcceptableThis test, NativeImpl impl,
                                       const CallArgs& args) const
{
    ReportIncompatible(cx, args);
    return false;
}

bool
ScriptedDirectProxyHandler::hasInstance(JSContext* cx, HandleObject proxy, MutableHandleValue v,
                                        bool* bp) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (!target) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_REVOKED);
        return false;
    }

    return HasInstance(cx, target, v, bp);
}

bool
ScriptedDirectProxyHandler::getBuiltinClass(JSContext* cx, HandleObject proxy,
                                            ESClassValue* classValue) const
{
    *classValue = ESClass_Other;
    return true;
}

bool
ScriptedDirectProxyHandler::isArray(JSContext* cx, HandleObject proxy,
                                    IsArrayAnswer* answer) const
{
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (target)
        return JS::IsArray(cx, target, answer);

    *answer = IsArrayAnswer::RevokedProxy;
    return true;
}

const char*
ScriptedDirectProxyHandler::className(JSContext* cx, HandleObject proxy) const
{
    // Right now the caller is not prepared to handle failures.
    RootedObject target(cx, proxy->as<ProxyObject>().target());
    if (!target)
        return BaseProxyHandler::className(cx, proxy);

    return GetObjectClassName(cx, target);
}
JSString*
ScriptedDirectProxyHandler::fun_toString(JSContext* cx, HandleObject proxy,
                                         unsigned indent) const
{
    JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_INCOMPATIBLE_PROTO,
                         js_Function_str, js_toString_str, "object");
    return nullptr;
}

bool
ScriptedDirectProxyHandler::regexp_toShared(JSContext* cx, HandleObject proxy,
                                            RegExpGuard* g) const
{
    MOZ_CRASH("Should not end up in ScriptedDirectProxyHandler::regexp_toShared");
    return false;
}

bool
ScriptedDirectProxyHandler::boxedValue_unbox(JSContext* cx, HandleObject proxy,
                                             MutableHandleValue vp) const
{
    MOZ_CRASH("Should not end up in ScriptedDirectProxyHandler::boxedValue_unbox");
    return false;
}

bool
ScriptedDirectProxyHandler::isCallable(JSObject* obj) const
{
    MOZ_ASSERT(obj->as<ProxyObject>().handler() == &ScriptedDirectProxyHandler::singleton);
    uint32_t callConstruct = obj->as<ProxyObject>().extra(IS_CALLCONSTRUCT_EXTRA).toPrivateUint32();
    return !!(callConstruct & IS_CALLABLE);
}

bool
ScriptedDirectProxyHandler::isConstructor(JSObject* obj) const
{
    MOZ_ASSERT(obj->as<ProxyObject>().handler() == &ScriptedDirectProxyHandler::singleton);
    uint32_t callConstruct = obj->as<ProxyObject>().extra(IS_CALLCONSTRUCT_EXTRA).toPrivateUint32();
    return !!(callConstruct & IS_CONSTRUCTOR);
}

const char ScriptedDirectProxyHandler::family = 0;
const ScriptedDirectProxyHandler ScriptedDirectProxyHandler::singleton;

bool
IsRevokedScriptedProxy(JSObject* obj)
{
    obj = CheckedUnwrap(obj);
    return obj && IsScriptedProxy(obj) && !obj->as<ProxyObject>().target();
}

// ES8 rev 0c1bd3004329336774cbc90de727cd0cf5f11e93 9.5.14 ProxyCreate.
static bool
ProxyCreate(JSContext* cx, CallArgs& args, const char* callerName)
{
    if (args.length() < 2) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_MORE_ARGS_NEEDED,
                             callerName, "1", "s");
        return false;
    }

    // Step 1.
    RootedObject target(cx, NonNullObject(cx, args[0]));
    if (!target)
        return false;

    // Step 2.
    if (IsRevokedScriptedProxy(target)) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_ARG_REVOKED, "1");
        return false;
    }

    // Step 3.
    RootedObject handler(cx, NonNullObject(cx, args[1]));
    if (!handler)
        return false;

    // Step 4.
    if (IsRevokedScriptedProxy(handler)) {
        JS_ReportErrorNumber(cx, GetErrorMessage, nullptr, JSMSG_PROXY_ARG_REVOKED, "2");
        return false;
    }

    // Steps 5-6, 8.
    RootedValue priv(cx, ObjectValue(*target));
    JSObject* proxy_ =
        NewProxyObject(cx, &ScriptedDirectProxyHandler::singleton,
                       priv, TaggedProto::LazyProto);
    if (!proxy_)
        return false;

    // Step 9 (reordered).
    Rooted<ProxyObject*> proxy(cx, &proxy_->as<ProxyObject>());
    proxy->setExtra(ScriptedDirectProxyHandler::HANDLER_EXTRA, ObjectValue(*handler));

    // Step 7.
    uint32_t callable = target->isCallable() ? ScriptedDirectProxyHandler::IS_CALLABLE : 0;
    uint32_t constructor = target->isConstructor() ? ScriptedDirectProxyHandler::IS_CONSTRUCTOR : 0;
    proxy->setExtra(ScriptedDirectProxyHandler::IS_CALLCONSTRUCT_EXTRA,
                    PrivateUint32Value(callable | constructor));

    // Step 10.
    args.rval().setObject(*proxy);
    return true;
}

bool
js::proxy(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ThrowIfNotConstructing(cx, args, "Proxy"))
        return false;

    return ProxyCreate(cx, args, "Proxy");
}

static bool
RevokeProxy(JSContext* cx, unsigned argc, Value* vp)
{
    CallReceiver rec = CallReceiverFromVp(vp);

    RootedFunction func(cx, &rec.callee().as<JSFunction>());
    RootedObject p(cx, func->getExtendedSlot(ScriptedDirectProxyHandler::REVOKE_SLOT).toObjectOrNull());

    if (p) {
        func->setExtendedSlot(ScriptedDirectProxyHandler::REVOKE_SLOT, NullValue());

        MOZ_ASSERT(p->is<ProxyObject>());

        p->as<ProxyObject>().setSameCompartmentPrivate(NullValue());
        p->as<ProxyObject>().setExtra(ScriptedDirectProxyHandler::HANDLER_EXTRA, NullValue());
    }

    rec.rval().setUndefined();
    return true;
}

bool
js::proxy_revocable(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!ProxyCreate(cx, args, "Proxy.revocable"))
        return false;

    RootedValue proxyVal(cx, args.rval());
    MOZ_ASSERT(proxyVal.toObject().is<ProxyObject>());

    RootedObject revoker(cx, NewFunctionByIdWithReserved(cx, RevokeProxy, 0, 0,
                         AtomToId(cx->names().revoke)));
    if (!revoker)
        return false;

    revoker->as<JSFunction>().initExtendedSlot(ScriptedDirectProxyHandler::REVOKE_SLOT, proxyVal);

    RootedPlainObject result(cx, NewBuiltinClassInstance<PlainObject>(cx));
    if (!result)
        return false;

    RootedValue revokeVal(cx, ObjectValue(*revoker));
    if (!DefineProperty(cx, result, cx->names().proxy, proxyVal) ||
        !DefineProperty(cx, result, cx->names().revoke, revokeVal))
    {
        return false;
    }

    args.rval().setObject(*result);
    return true;
}
