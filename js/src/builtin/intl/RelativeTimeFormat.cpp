/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Implementation of the Intl.RelativeTimeFormat proposal. */

#include "builtin/intl/RelativeTimeFormat.h"

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"

#include "jscntxt.h"

#include "builtin/intl/CommonFunctions.h"
#include "builtin/intl/ICUStubs.h"
#include "builtin/intl/ScopedICUObject.h"
#include "gc/FreeOp.h"
#include "vm/GlobalObject.h"

#include "vm/NativeObject-inl.h"

using namespace js;

using mozilla::IsNegativeZero;
using mozilla::Range;
using mozilla::RangedPtr;

using js::intl::CallICU;
using js::intl::DateTimeFormatOptions;
using js::intl::GetAvailableLocales;
using js::intl::IcuLocale;
using js::intl::StringsAreEqual;

/**************** RelativeTimeFormat *****************/

const ClassOps RelativeTimeFormatObject::classOps_ = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* enumerate */
    nullptr, /* newEnumerate */
    nullptr, /* resolve */
    nullptr, /* mayResolve */
    RelativeTimeFormatObject::finalize
};

const Class RelativeTimeFormatObject::class_ = {
    js_Object_str,
    JSCLASS_HAS_RESERVED_SLOTS(RelativeTimeFormatObject::SLOT_COUNT) |
    JSCLASS_FOREGROUND_FINALIZE,
    &RelativeTimeFormatObject::classOps_
};

#if JS_HAS_TOSOURCE
static bool
relativeTimeFormat_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().setString(cx->names().RelativeTimeFormat);
    return true;
}
#endif

static const JSFunctionSpec relativeTimeFormat_static_methods[] = {
    JS_SELF_HOSTED_FN("supportedLocalesOf", "Intl_RelativeTimeFormat_supportedLocalesOf", 1, 0),
    JS_FS_END
};

static const JSFunctionSpec relativeTimeFormat_methods[] = {
    JS_SELF_HOSTED_FN("resolvedOptions", "Intl_RelativeTimeFormat_resolvedOptions", 0, 0),
    JS_SELF_HOSTED_FN("format", "Intl_RelativeTimeFormat_format", 2, 0),
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str, relativeTimeFormat_toSource, 0, 0),
#endif
    JS_FS_END
};

/**
 * RelativeTimeFormat constructor.
 * Spec: ECMAScript 402 API, RelativeTimeFormat, 1.1
 */
static bool
RelativeTimeFormat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Step 1.
    if (!ThrowIfNotConstructing(cx, args, "Intl.RelativeTimeFormat"))
        return false;

    // Step 2 (Inlined 9.1.14, OrdinaryCreateFromConstructor).
    RootedObject proto(cx);
    if (!GetPrototypeFromBuiltinConstructor(cx, args, &proto))
        return false;

    if (!proto) {
        proto = GlobalObject::getOrCreateRelativeTimeFormatPrototype(cx, cx->global());
        if (!proto)
            return false;
    }

    Rooted<RelativeTimeFormatObject*> relativeTimeFormat(cx);
    relativeTimeFormat = NewObjectWithGivenProto<RelativeTimeFormatObject>(cx, proto);
    if (!relativeTimeFormat)
        return false;

    relativeTimeFormat->setReservedSlot(RelativeTimeFormatObject::INTERNALS_SLOT, NullValue());
    relativeTimeFormat->setReservedSlot(RelativeTimeFormatObject::URELATIVE_TIME_FORMAT_SLOT, PrivateValue(nullptr));

    HandleValue locales = args.get(0);
    HandleValue options = args.get(1);

    // Step 3.
    if (!intl::InitializeObject(cx, relativeTimeFormat, cx->names().InitializeRelativeTimeFormat,
                                locales, options))
    {
        return false;
    }

    args.rval().setObject(*relativeTimeFormat);
    return true;
}

void
js::RelativeTimeFormatObject::finalize(FreeOp* fop, JSObject* obj)
{
    MOZ_ASSERT(fop->onActiveCooperatingThread());

    const Value& slot =
        obj->as<RelativeTimeFormatObject>().getReservedSlot(RelativeTimeFormatObject::URELATIVE_TIME_FORMAT_SLOT);
    if (URelativeDateTimeFormatter* rtf = static_cast<URelativeDateTimeFormatter*>(slot.toPrivate()))
        ureldatefmt_close(rtf);
}

JSObject*
js::CreateRelativeTimeFormatPrototype(JSContext* cx, HandleObject Intl,
                                      Handle<GlobalObject*> global)
{
    RootedFunction ctor(cx);
    ctor = global->createConstructor(cx, &RelativeTimeFormat, cx->names().RelativeTimeFormat, 0);
    if (!ctor)
        return nullptr;

    RootedObject proto(cx, GlobalObject::createBlankPrototype<PlainObject>(cx, global));
    if (!proto)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (!JS_DefineFunctions(cx, ctor, relativeTimeFormat_static_methods))
        return nullptr;

    if (!JS_DefineFunctions(cx, proto, relativeTimeFormat_methods))
        return nullptr;

    RootedValue ctorValue(cx, ObjectValue(*ctor));
    if (!DefineDataProperty(cx, Intl, cx->names().RelativeTimeFormat, ctorValue, 0))
        return nullptr;

    return proto;
}

/* static */ bool
js::GlobalObject::addRelativeTimeFormatConstructor(JSContext* cx, HandleObject intl)
{
    Handle<GlobalObject*> global = cx->global();

    {
        const HeapSlot& slot = global->getReservedSlotRef(RELATIVE_TIME_FORMAT_PROTO);
        if (!slot.isUndefined()) {
            MOZ_ASSERT(slot.isObject());
            JS_ReportErrorASCII(cx,
                                "the RelativeTimeFormat constructor can't be added "
                                "multiple times in the same global");
            return false;
        }
    }

    JSObject* relativeTimeFormatProto = CreateRelativeTimeFormatPrototype(cx, intl, global);
    if (!relativeTimeFormatProto)
        return false;

    global->setReservedSlot(RELATIVE_TIME_FORMAT_PROTO, ObjectValue(*relativeTimeFormatProto));
    return true;
}

bool
js::AddRelativeTimeFormatConstructor(JSContext* cx, JS::Handle<JSObject*> intl)
{
    return GlobalObject::addRelativeTimeFormatConstructor(cx, intl);
}


bool
js::intl_RelativeTimeFormat_availableLocales(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 0);

    RootedValue result(cx);
    // We're going to use ULocale availableLocales as per ICU recommendation:
    // https://ssl.icu-project.org/trac/ticket/12756
    if (!GetAvailableLocales(cx, uloc_countAvailable, uloc_getAvailable, &result))
        return false;
    args.rval().set(result);
    return true;
}

enum class RelativeTimeType
{
    /**
     * Only strings with numeric components like `1 day ago`.
     */
    Numeric,
    /**
     * Natural-language strings like `yesterday` when possible,
     * otherwise strings with numeric components as in `7 months ago`.
     */
    Text,
};

bool
js::intl_FormatRelativeTime(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 3);

    RootedObject relativeTimeFormat(cx, &args[0].toObject());

    double t = args[1].toNumber();

    RootedObject internals(cx, intl::GetInternalsObject(cx, relativeTimeFormat));
    if (!internals)
        return false;

    RootedValue value(cx);

    if (!GetProperty(cx, internals, internals, cx->names().locale, &value))
        return false;
    JSAutoByteString locale(cx, value.toString());
    if (!locale)
        return false;

    if (!GetProperty(cx, internals, internals, cx->names().style, &value))
        return false;

    UDateRelativeDateTimeFormatterStyle relDateTimeStyle;
    {
        JSLinearString* style = value.toString()->ensureLinear(cx);
        if (!style)
            return false;

        if (StringEqualsAscii(style, "short")) {
            relDateTimeStyle = UDAT_STYLE_SHORT;
        } else if (StringEqualsAscii(style, "narrow")) {
            relDateTimeStyle = UDAT_STYLE_NARROW;
        } else {
            MOZ_ASSERT(StringEqualsAscii(style, "long"));
            relDateTimeStyle = UDAT_STYLE_LONG;
        }
    }

    if (!GetProperty(cx, internals, internals, cx->names().type, &value))
        return false;

    RelativeTimeType relDateTimeType;
    {
        JSLinearString* type = value.toString()->ensureLinear(cx);
        if (!type)
            return false;

        if (StringEqualsAscii(type, "text")) {
            relDateTimeType = RelativeTimeType::Text;
        } else {
            MOZ_ASSERT(StringEqualsAscii(type, "numeric"));
            relDateTimeType = RelativeTimeType::Numeric;
        }
    }

    URelativeDateTimeUnit relDateTimeUnit;
    {
        JSLinearString* unit = args[2].toString()->ensureLinear(cx);
        if (!unit)
            return false;

        if (StringEqualsAscii(unit, "second")) {
            relDateTimeUnit = UDAT_REL_UNIT_SECOND;
        } else if (StringEqualsAscii(unit, "minute")) {
            relDateTimeUnit = UDAT_REL_UNIT_MINUTE;
        } else if (StringEqualsAscii(unit, "hour")) {
            relDateTimeUnit = UDAT_REL_UNIT_HOUR;
        } else if (StringEqualsAscii(unit, "day")) {
            relDateTimeUnit = UDAT_REL_UNIT_DAY;
        } else if (StringEqualsAscii(unit, "week")) {
            relDateTimeUnit = UDAT_REL_UNIT_WEEK;
        } else if (StringEqualsAscii(unit, "month")) {
            relDateTimeUnit = UDAT_REL_UNIT_MONTH;
        } else if (StringEqualsAscii(unit, "quarter")) {
            relDateTimeUnit = UDAT_REL_UNIT_QUARTER;
        } else {
            MOZ_ASSERT(StringEqualsAscii(unit, "year"));
            relDateTimeUnit = UDAT_REL_UNIT_YEAR;
        }
    }

    // ICU doesn't handle -0 well: work around this by converting it to +0.
    // See: http://bugs.icu-project.org/trac/ticket/12936
    if (IsNegativeZero(t))
        t = +0.0;

    UErrorCode status = U_ZERO_ERROR;
    URelativeDateTimeFormatter* rtf =
        ureldatefmt_open(IcuLocale(locale.ptr()), nullptr, relDateTimeStyle,
                         UDISPCTX_CAPITALIZATION_FOR_STANDALONE, &status);
    if (U_FAILURE(status)) {
        intl::ReportInternalError(cx);
        return false;
    }

    ScopedICUObject<URelativeDateTimeFormatter, ureldatefmt_close> closeRelativeTimeFormat(rtf);

    JSString* str =
        CallICU(cx, [rtf, t, relDateTimeUnit, relDateTimeType](UChar* chars, int32_t size,
                                                               UErrorCode* status)
        {
            auto fmt = relDateTimeType == RelativeTimeType::Text
                       ? ureldatefmt_format
                       : ureldatefmt_formatNumeric;
            return fmt(rtf, t, relDateTimeUnit, chars, size, status);
        });
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}
