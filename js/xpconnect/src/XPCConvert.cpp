/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Data conversion between native and JavaScript types. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Range.h"

#include "xpcprivate.h"
#include "nsIScriptError.h"
#include "nsWrapperCache.h"
#include "nsJSUtils.h"
#include "nsQueryObject.h"
#include "nsScriptError.h"
#include "WrapperFactory.h"

#include "nsWrapperCacheInlines.h"

#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/CharacterEncoding.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/PrimitiveConversions.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"

using namespace xpc;
using namespace mozilla;
using namespace mozilla::dom;
using namespace JS;

//#define STRICT_CHECK_OF_UNICODE
#ifdef STRICT_CHECK_OF_UNICODE
#define ILLEGAL_RANGE(c) (0!=((c) & 0xFF80))
#else // STRICT_CHECK_OF_UNICODE
#define ILLEGAL_RANGE(c) (0!=((c) & 0xFF00))
#endif // STRICT_CHECK_OF_UNICODE

#define ILLEGAL_CHAR_RANGE(c) (0!=((c) & 0x80))

/***********************************************************/

// static
bool
XPCConvert::IsMethodReflectable(const nsXPTMethodInfo& info)
{
    if (info.IsNotXPCOM() || info.IsHidden())
        return false;

    for (int i = info.GetParamCount() - 1; i >= 0; i--) {
        const nsXPTParamInfo& param = info.GetParam(i);
        const nsXPTType& type = param.GetType();

        // Reflected methods can't use native types. All native types end up
        // getting tagged as void*, so this check is easy.
        if (type.TagPart() == nsXPTType::T_VOID)
            return false;
    }
    return true;
}

static JSObject*
UnwrapNativeCPOW(nsISupports* wrapper)
{
    nsCOMPtr<nsIXPConnectWrappedJS> underware = do_QueryInterface(wrapper);
    if (underware) {
        JSObject* mainObj = underware->GetJSObject();
        if (mainObj && mozilla::jsipc::IsWrappedCPOW(mainObj))
            return mainObj;
    }
    return nullptr;
}

/***************************************************************************/

// static
bool
XPCConvert::GetISupportsFromJSObject(JSObject* obj, nsISupports** iface)
{
    const JSClass* jsclass = js::GetObjectJSClass(obj);
    MOZ_ASSERT(jsclass, "obj has no class");
    if (jsclass &&
        (jsclass->flags & JSCLASS_HAS_PRIVATE) &&
        (jsclass->flags & JSCLASS_PRIVATE_IS_NSISUPPORTS)) {
        *iface = (nsISupports*) xpc_GetJSPrivate(obj);
        return true;
    }
    *iface = UnwrapDOMObjectToISupports(obj);
    return !!*iface;
}

/***************************************************************************/

// static
bool
XPCConvert::NativeData2JS(MutableHandleValue d, const void* s,
                          const nsXPTType& type, const nsID* iid,
                          uint32_t arrlen, nsresult* pErr)
{
    MOZ_ASSERT(s, "bad param");

    AutoJSContext cx;
    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    switch (type.TagPart()) {
    case nsXPTType::T_I8    :
        d.setInt32(*static_cast<const int8_t*>(s));
        return true;
    case nsXPTType::T_I16   :
        d.setInt32(*static_cast<const int16_t*>(s));
        return true;
    case nsXPTType::T_I32   :
        d.setInt32(*static_cast<const int32_t*>(s));
        return true;
    case nsXPTType::T_I64   :
        d.setNumber(static_cast<double>(*static_cast<const int64_t*>(s)));
        return true;
    case nsXPTType::T_U8    :
        d.setInt32(*static_cast<const uint8_t*>(s));
        return true;
    case nsXPTType::T_U16   :
        d.setInt32(*static_cast<const uint16_t*>(s));
        return true;
    case nsXPTType::T_U32   :
        d.setNumber(*static_cast<const uint32_t*>(s));
        return true;
    case nsXPTType::T_U64   :
        d.setNumber(static_cast<double>(*static_cast<const uint64_t*>(s)));
        return true;
    case nsXPTType::T_FLOAT :
        d.setNumber(*static_cast<const float*>(s));
        return true;
    case nsXPTType::T_DOUBLE:
        d.setNumber(*static_cast<const double*>(s));
        return true;
    case nsXPTType::T_BOOL  :
        d.setBoolean(*static_cast<const bool*>(s));
        return true;
    case nsXPTType::T_CHAR  :
    {
        char p = *static_cast<const char*>(s);

#ifdef STRICT_CHECK_OF_UNICODE
        MOZ_ASSERT(! ILLEGAL_CHAR_RANGE(p) , "passing non ASCII data");
#endif // STRICT_CHECK_OF_UNICODE

        JSString* str = JS_NewStringCopyN(cx, &p, 1);
        if (!str)
            return false;

        d.setString(str);
        return true;
    }
    case nsXPTType::T_WCHAR :
    {
        char16_t p = *static_cast<const char16_t*>(s);

        JSString* str = JS_NewUCStringCopyN(cx, &p, 1);
        if (!str)
            return false;

        d.setString(str);
        return true;
    }

    case nsXPTType::T_JSVAL :
    {
        d.set(*static_cast<const Value*>(s));
        return JS_WrapValue(cx, d);
    }

    case nsXPTType::T_VOID:
        XPC_LOG_ERROR(("XPCConvert::NativeData2JS : void* params not supported"));
        return false;

    case nsXPTType::T_IID:
    {
        nsID* iid2 = *static_cast<nsID* const*>(s);
        if (!iid2) {
            d.setNull();
            return true;
        }

        RootedObject scope(cx, JS::CurrentGlobalOrNull(cx));
        JSObject* obj = xpc_NewIDObject(cx, scope, *iid2);
        if (!obj)
            return false;

        d.setObject(*obj);
        return true;
    }

    case nsXPTType::T_ASTRING:
        // Fall through to T_DOMSTRING case

    case nsXPTType::T_DOMSTRING:
    {
        const nsAString* p = static_cast<const nsAString*>(s);
        if (!p || p->IsVoid()) {
            d.setNull();
            return true;
        }

        nsStringBuffer* buf;
        if (!XPCStringConvert::ReadableToJSVal(cx, *p, &buf, d))
            return false;
        if (buf)
            buf->AddRef();
        return true;
    }

    case nsXPTType::T_CHAR_STR:
    {
        const char* p = *static_cast<const char* const*>(s);
        arrlen = p ? strlen(p) : 0;
        MOZ_FALLTHROUGH;
    }
    case nsXPTType::T_PSTRING_SIZE_IS:
    {
        const char* p = *static_cast<const char* const*>(s);
        if (!p) {
            d.setNull();
            return true;
        }

#ifdef STRICT_CHECK_OF_UNICODE
        bool isAscii = true;
        for (uint32_t i = 0; i < arrlen; i++) {
            if (ILLEGAL_CHAR_RANGE(p[i]))
                isAscii = false;
        }
        MOZ_ASSERT(isAscii, "passing non ASCII data");
#endif // STRICT_CHECK_OF_UNICODE

        JSString* str = JS_NewStringCopyN(cx, p, arrlen);
        if (!str)
            return false;

        d.setString(str);
        return true;
    }

    case nsXPTType::T_WCHAR_STR:
    {
        const char16_t* p = *static_cast<const char16_t* const*>(s);
        arrlen = p ? nsCharTraits<char16_t>::length(p) : 0;
        MOZ_FALLTHROUGH;
    }
    case nsXPTType::T_PWSTRING_SIZE_IS:
    {
        const char16_t* p = *static_cast<const char16_t* const*>(s);
        if (!p) {
            d.setNull();
            return true;
        }

        JSString* str = JS_NewUCStringCopyN(cx, p, arrlen);
        if (!str)
            return false;

        d.setString(str);
        return true;
    }

    case nsXPTType::T_UTF8STRING:
    {
        const nsACString* utf8String = static_cast<const nsACString*>(s);

        if (!utf8String || utf8String->IsVoid()) {
            d.setNull();
            return true;
        }

        if (utf8String->IsEmpty()) {
            d.set(JS_GetEmptyStringValue(cx));
            return true;
        }

        const uint32_t len = CalcUTF8ToUnicodeLength(*utf8String);
        // The cString is not empty at this point, but the calculated
        // UTF-16 length is zero, meaning no valid conversion exists.
        if (!len)
            return false;

        const size_t buffer_size = (len + 1) * sizeof(char16_t);
        char16_t* buffer =
            static_cast<char16_t*>(JS_malloc(cx, buffer_size));
        if (!buffer)
            return false;

        uint32_t copied;
        if (!UTF8ToUnicodeBuffer(*utf8String, buffer, &copied) ||
            len != copied) {
            // Copy or conversion during copy failed. Did not copy the
            // whole string.
            JS_free(cx, buffer);
            return false;
        }

        // JS_NewUCString takes ownership on success, i.e. a
        // successful call will make it the responsiblity of the JS VM
        // to free the buffer.
        JSString* str = JS_NewUCString(cx, buffer, len);
        if (!str) {
            JS_free(cx, buffer);
            return false;
        }

        d.setString(str);
        return true;
    }
    case nsXPTType::T_CSTRING:
    {
        const nsACString* cString = static_cast<const nsACString*>(s);

        if (!cString || cString->IsVoid()) {
            d.setNull();
            return true;
        }

        // c-strings (binary blobs) are deliberately not converted from
        // UTF-8 to UTF-16. T_UTF8Sting is for UTF-8 encoded strings
        // with automatic conversion.
        JSString* str = JS_NewStringCopyN(cx, cString->Data(),
                                          cString->Length());
        if (!str)
            return false;

        d.setString(str);
        return true;
    }

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
        nsISupports* iface = *static_cast<nsISupports* const*>(s);
        if (!iface) {
            d.setNull();
            return true;
        }

        if (iid->Equals(NS_GET_IID(nsIVariant))) {
            nsCOMPtr<nsIVariant> variant = do_QueryInterface(iface);
            if (!variant)
                return false;

            return XPCVariant::VariantDataToJS(variant,
                                               pErr, d);
        }

        xpcObjectHelper helper(iface);
        return NativeInterface2JSObject(d, helper, iid, true, pErr);
    }

    case nsXPTType::T_DOMOBJECT:
    {
        void* ptr = *static_cast<void* const*>(s);
        if (!ptr) {
            d.setNull();
            return true;
        }

        return type.GetDOMObjectInfo().Wrap(cx, ptr, d);
    }

    case nsXPTType::T_PROMISE:
    {
        Promise* promise = *static_cast<Promise* const*>(s);
        if (!promise) {
            d.setNull();
            return true;
        }

        RootedObject jsobj(cx, promise->PromiseObj());
        if (!JS_WrapObject(cx, &jsobj)) {
            return false;
        }
        d.setObject(*jsobj);
        return true;
    }

    case nsXPTType::T_ARRAY:
        return NativeArray2JS(d, static_cast<const void* const*>(s),
                              type.ArrayElementType(), iid, arrlen, pErr);

    default:
        NS_ERROR("bad type");
        return false;
    }
    return true;
}

/***************************************************************************/

#ifdef DEBUG
static bool
CheckChar16InCharRange(char16_t c)
{
    if (ILLEGAL_RANGE(c)) {
        /* U+0080/U+0100 - U+FFFF data lost. */
        static const size_t MSG_BUF_SIZE = 64;
        char msg[MSG_BUF_SIZE];
        snprintf(msg, MSG_BUF_SIZE, "char16_t out of char range; high bits of data lost: 0x%x", int(c));
        NS_WARNING(msg);
        return false;
    }

    return true;
}

template<typename CharT>
static void
CheckCharsInCharRange(const CharT* chars, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (!CheckChar16InCharRange(chars[i]))
            break;
    }
}
#endif

template<typename T>
bool ConvertToPrimitive(JSContext* cx, HandleValue v, T* retval)
{
    return ValueToPrimitive<T, eDefault>(cx, v, retval);
}

// static
bool
XPCConvert::JSData2Native(void* d, HandleValue s,
                          const nsXPTType& type,
                          const nsID* iid,
                          uint32_t arrlen,
                          nsresult* pErr)
{
    MOZ_ASSERT(d, "bad param");

    AutoJSContext cx;
    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

    bool sizeis = type.Tag() == TD_PSTRING_SIZE_IS ||
        type.Tag() == TD_PWSTRING_SIZE_IS;

    switch (type.TagPart()) {
    case nsXPTType::T_I8     :
        return ConvertToPrimitive(cx, s, static_cast<int8_t*>(d));
    case nsXPTType::T_I16    :
        return ConvertToPrimitive(cx, s, static_cast<int16_t*>(d));
    case nsXPTType::T_I32    :
        return ConvertToPrimitive(cx, s, static_cast<int32_t*>(d));
    case nsXPTType::T_I64    :
        return ConvertToPrimitive(cx, s, static_cast<int64_t*>(d));
    case nsXPTType::T_U8     :
        return ConvertToPrimitive(cx, s, static_cast<uint8_t*>(d));
    case nsXPTType::T_U16    :
        return ConvertToPrimitive(cx, s, static_cast<uint16_t*>(d));
    case nsXPTType::T_U32    :
        return ConvertToPrimitive(cx, s, static_cast<uint32_t*>(d));
    case nsXPTType::T_U64    :
        return ConvertToPrimitive(cx, s, static_cast<uint64_t*>(d));
    case nsXPTType::T_FLOAT  :
        return ConvertToPrimitive(cx, s, static_cast<float*>(d));
    case nsXPTType::T_DOUBLE :
        return ConvertToPrimitive(cx, s, static_cast<double*>(d));
    case nsXPTType::T_BOOL   :
        return ConvertToPrimitive(cx, s, static_cast<bool*>(d));
    case nsXPTType::T_CHAR   :
    {
        JSString* str = ToString(cx, s);
        if (!str) {
            return false;
        }

        char16_t ch;
        if (JS_GetStringLength(str) == 0) {
            ch = 0;
        } else {
            if (!JS_GetStringCharAt(cx, str, 0, &ch))
                return false;
        }
#ifdef DEBUG
        CheckChar16InCharRange(ch);
#endif
        *((char*)d) = char(ch);
        break;
    }
    case nsXPTType::T_WCHAR  :
    {
        JSString* str;
        if (!(str = ToString(cx, s))) {
            return false;
        }
        size_t length = JS_GetStringLength(str);
        if (length == 0) {
            *((uint16_t*)d) = 0;
            break;
        }

        char16_t ch;
        if (!JS_GetStringCharAt(cx, str, 0, &ch))
            return false;

        *((uint16_t*)d) = uint16_t(ch);
        break;
    }
    case nsXPTType::T_JSVAL :
        *((Value*)d) = s;
        break;
    case nsXPTType::T_VOID:
        XPC_LOG_ERROR(("XPCConvert::JSData2Native : void* params not supported"));
        NS_ERROR("void* params not supported");
        return false;
    case nsXPTType::T_IID:
    {
        const nsID* pid = nullptr;

        // There's no good reason to pass a null IID.
        if (s.isNullOrUndefined()) {
            if (pErr)
                *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;
            return false;
        }

        if (!s.isObject() ||
            !(pid = xpc_JSObjectToID(cx, &s.toObject()))) {
            return false;
        }
        *((const nsID**)d) = pid->Clone();
        return true;
    }

    case nsXPTType::T_ASTRING:
    {
        if (s.isUndefined()) {
            ((nsAString*)d)->SetIsVoid(true);
            return true;
        }
        MOZ_FALLTHROUGH;
    }
    case nsXPTType::T_DOMSTRING:
    {
        nsAString* ws = (nsAString*)d;
        if (s.isNull()) {
            ws->SetIsVoid(true);
            return true;
        }
        size_t length = 0;
        JSString* str = nullptr;
        if (!s.isUndefined()) {
            str = ToString(cx, s);
            if (!str)
                return false;

            length = JS_GetStringLength(str);
            if (!length) {
                ws->Truncate();
                return true;
            }
        }

        if (!str) {
            ws->AssignLiteral(u"undefined");
        } else if (XPCStringConvert::IsDOMString(str)) {
            // The characters represent an existing nsStringBuffer that
            // was shared by XPCStringConvert::ReadableToJSVal.
            const char16_t* chars = JS_GetTwoByteExternalStringChars(str);
            if (chars[length] == '\0') {
                // Safe to share the buffer.
                nsStringBuffer::FromData((void*)chars)->ToString(length, *ws);
            } else {
                // We have to copy to ensure null-termination.
                ws->Assign(chars, length);
            }
        } else if (XPCStringConvert::IsLiteral(str)) {
            // The characters represent a literal char16_t string constant
            // compiled into libxul, such as the string "undefined" above.
            const char16_t* chars = JS_GetTwoByteExternalStringChars(str);
            ws->AssignLiteral(chars, length);
        } else {
            // We don't bother checking for a dynamic-atom external string,
            // because we'd just need to copy out of it anyway.
            if (!AssignJSString(cx, *ws, str))
                return false;
        }
        return true;
    }

    case nsXPTType::T_CHAR_STR:
    case nsXPTType::T_PSTRING_SIZE_IS:
    {
        if (s.isUndefined() || s.isNull()) {
            if (sizeis && 0 != arrlen) {
                if (pErr)
                    *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                return false;
            }
            *((char**)d) = nullptr;
            return true;
        }

        JSString* str = ToString(cx, s);
        if (!str) {
            return false;
        }

#ifdef DEBUG
        if (JS_StringHasLatin1Chars(str)) {
            size_t len;
            AutoCheckCannotGC nogc;
            const Latin1Char* chars = JS_GetLatin1StringCharsAndLength(cx, nogc, str, &len);
            if (chars)
                CheckCharsInCharRange(chars, len);
        } else {
            size_t len;
            AutoCheckCannotGC nogc;
            const char16_t* chars = JS_GetTwoByteStringCharsAndLength(cx, nogc, str, &len);
            if (chars)
                CheckCharsInCharRange(chars, len);
        }
#endif // DEBUG

        size_t length = JS_GetStringEncodingLength(cx, str);
        if (length == size_t(-1)) {
            return false;
        }
        if (sizeis) {
            if (length > arrlen) {
                if (pErr)
                    *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                return false;
            }
            if (length < arrlen) {
                length = arrlen;
            }
        }
        char* buffer = static_cast<char*>(moz_xmalloc(length + 1));
        if (!buffer) {
            return false;
        }
        if (!JS_EncodeStringToBuffer(cx, str, buffer, length)) {
            free(buffer);
            return false;
        }
        buffer[length] = '\0';
        *((void**)d) = buffer;
        return true;
    }

    case nsXPTType::T_WCHAR_STR:
    case nsXPTType::T_PWSTRING_SIZE_IS:
    {
        JSString* str;

        if (s.isUndefined() || s.isNull()) {
            if (sizeis && 0 != arrlen) {
                if (pErr)
                    *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                return false;
            }
            *((char16_t**)d) = nullptr;
            return true;
        }

        if (!(str = ToString(cx, s))) {
            return false;
        }
        size_t len = JS_GetStringLength(str);
        if (sizeis) {
            if (len > arrlen) {
                if (pErr)
                    *pErr = NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING;
                return false;
            }
            if (len < arrlen) {
                len = arrlen;
            }
        }

        size_t byte_len = (len+1)*sizeof(char16_t);
        if (!(*((void**)d) = moz_xmalloc(byte_len))) {
            // XXX should report error
            return false;
        }
        mozilla::Range<char16_t> destChars(*((char16_t**)d), len + 1);
        if (!JS_CopyStringChars(cx, destChars, str))
            return false;
        destChars[len] = 0;

        return true;
    }

    case nsXPTType::T_UTF8STRING:
    {
        nsACString* rs = (nsACString*)d;
        if (s.isNull() || s.isUndefined()) {
            rs->SetIsVoid(true);
            return true;
        }

        // The JS val is neither null nor void...
        JSString* str = ToString(cx, s);
        if (!str)
            return false;

        size_t length = JS_GetStringLength(str);
        if (!length) {
            rs->Truncate();
            return true;
        }

        JSFlatString* flat = JS_FlattenString(cx, str);
        if (!flat)
            return false;

        size_t utf8Length = JS::GetDeflatedUTF8StringLength(flat);
        rs->SetLength(utf8Length);

        JS::DeflateStringToUTF8Buffer(flat, mozilla::RangedPtr<char>(rs->BeginWriting(), utf8Length));

        return true;
    }

    case nsXPTType::T_CSTRING:
    {
        nsACString* rs = (nsACString*)d;
        if (s.isNull() || s.isUndefined()) {
            rs->SetIsVoid(true);
            return true;
        }

        // The JS val is neither null nor void...
        JSString* str = ToString(cx, s);
        if (!str) {
            return false;
        }

        size_t length = JS_GetStringEncodingLength(cx, str);
        if (length == size_t(-1)) {
            return false;
        }

        if (!length) {
            rs->Truncate();
            return true;
        }

        rs->SetLength(uint32_t(length));
        if (rs->Length() != uint32_t(length)) {
            return false;
        }
        if (!JS_EncodeStringToBuffer(cx, str, rs->BeginWriting(), length)) {
            return false;
        }

        return true;
    }

    case nsXPTType::T_INTERFACE:
    case nsXPTType::T_INTERFACE_IS:
    {
        MOZ_ASSERT(iid,"can't do interface conversions without iid");

        if (iid->Equals(NS_GET_IID(nsIVariant))) {
            nsCOMPtr<nsIVariant> variant = XPCVariant::newVariant(cx, s);
            if (!variant)
                return false;

            variant.forget(static_cast<nsISupports**>(d));
            return true;
        }

        if (s.isNullOrUndefined()) {
            *((nsISupports**)d) = nullptr;
            return true;
        }

        // only wrap JSObjects
        if (!s.isObject()) {
            if (pErr && s.isInt32() && 0 == s.toInt32())
                *pErr = NS_ERROR_XPC_BAD_CONVERT_JS_ZERO_ISNOT_NULL;
            return false;
        }

        RootedObject src(cx, &s.toObject());
        return JSObject2NativeInterface((void**)d, src, iid, nullptr, pErr);
    }

    case nsXPTType::T_DOMOBJECT:
    {
        if (s.isNullOrUndefined()) {
            *((void**)d) = nullptr;
            return true;
        }

        // Can't handle non-JSObjects
        if (!s.isObject()) {
            return false;
        }

        nsresult err = type.GetDOMObjectInfo().Unwrap(s, (void**)d);
        if (pErr) {
            *pErr = err;
        }
        return NS_SUCCEEDED(err);
    }

    case nsXPTType::T_PROMISE:
    {
        nsIGlobalObject* glob = CurrentNativeGlobal(cx);
        if (!glob) {
            if (pErr) {
                *pErr = NS_ERROR_UNEXPECTED;
            }
            return false;
        }

        // Call Promise::Resolve to create a Promise object. This allows us to
        // support returning non-promise values from Promise-returning functions
        // in JS.
        IgnoredErrorResult err;
        *(Promise**)d = Promise::Resolve(glob, cx, s, err).take();
        bool ok = !err.Failed();
        if (pErr) {
            *pErr = err.StealNSResult();
        }

        return ok;
    }

    case nsXPTType::T_ARRAY:
        return JSArray2Native((void**)d, s, arrlen,
                              type.ArrayElementType(), iid, pErr);

    default:
        NS_ERROR("bad type");
        return false;
    }
    return true;
}

/***************************************************************************/
// static
bool
XPCConvert::NativeInterface2JSObject(MutableHandleValue d,
                                     xpcObjectHelper& aHelper,
                                     const nsID* iid,
                                     bool allowNativeWrapper,
                                     nsresult* pErr)
{
    if (!iid)
        iid = &NS_GET_IID(nsISupports);

    d.setNull();
    if (!aHelper.Object())
        return true;
    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    // We used to have code here that unwrapped and simply exposed the
    // underlying JSObject. That caused anomolies when JSComponents were
    // accessed from other JS code - they didn't act like other xpconnect
    // wrapped components. So, instead, we create "double wrapped" objects
    // (that means an XPCWrappedNative around an nsXPCWrappedJS). This isn't
    // optimal -- we could detect this and roll the functionality into a
    // single wrapper, but the current solution is good enough for now.
    AutoJSContext cx;
    XPCWrappedNativeScope* xpcscope = ObjectScope(JS::CurrentGlobalOrNull(cx));
    if (!xpcscope)
        return false;

    // First, see if this object supports the wrapper cache. In that case, the object
    // to use is found as cache->GetWrapper(). If that is null, then the object will
    // create (and fill the cache) from its WrapObject call.
    nsWrapperCache* cache = aHelper.GetWrapperCache();

    RootedObject flat(cx, cache ? cache->GetWrapper() : nullptr);
    if (!flat && cache) {
        RootedObject global(cx, xpcscope->GetGlobalJSObject());
        js::AssertSameCompartment(cx, global);
        flat = cache->WrapObject(cx, nullptr);
        if (!flat)
            return false;
    }
    if (flat) {
        if (allowNativeWrapper && !JS_WrapObject(cx, &flat))
            return false;
        d.setObjectOrNull(flat);
        return true;
    }

    // NOTE(nika): Remove if Promise becomes non-nsISupports
    if (iid->Equals(NS_GET_IID(nsISupports))) {
        // Check for a Promise being returned via nsISupports.  In that
        // situation, we want to dig out its underlying JS object and return
        // that.
        RefPtr<Promise> promise = do_QueryObject(aHelper.Object());
        if (promise) {
            flat = promise->PromiseObj();
            if (!JS_WrapObject(cx, &flat))
                return false;
            d.setObjectOrNull(flat);
            return true;
        }
    }

    // Don't double wrap CPOWs. This is a temporary measure for compatibility
    // with objects that don't provide necessary QIs (such as objects under
    // the new DOM bindings). We expect the other side of the CPOW to have
    // the appropriate wrappers in place.
    RootedObject cpow(cx, UnwrapNativeCPOW(aHelper.Object()));
    if (cpow) {
        if (!JS_WrapObject(cx, &cpow))
            return false;
        d.setObject(*cpow);
        return true;
    }

    // Go ahead and create an XPCWrappedNative for this object.
    RefPtr<XPCNativeInterface> iface =
        XPCNativeInterface::GetNewOrUsed(iid);
    if (!iface)
        return false;

    RefPtr<XPCWrappedNative> wrapper;
    nsresult rv = XPCWrappedNative::GetNewOrUsed(aHelper, xpcscope, iface,
                                                 getter_AddRefs(wrapper));
    if (NS_FAILED(rv) && pErr)
        *pErr = rv;

    // If creating the wrapped native failed, then return early.
    if (NS_FAILED(rv) || !wrapper)
        return false;

    // If we're not creating security wrappers, we can return the
    // XPCWrappedNative as-is here.
    flat = wrapper->GetFlatJSObject();
    if (!allowNativeWrapper) {
        d.setObjectOrNull(flat);
        if (pErr)
            *pErr = NS_OK;
        return true;
    }

    // The call to wrap here handles both cross-compartment and same-compartment
    // security wrappers.
    RootedObject original(cx, flat);
    if (!JS_WrapObject(cx, &flat))
        return false;

    d.setObjectOrNull(flat);

    if (pErr)
        *pErr = NS_OK;

    return true;
}

/***************************************************************************/

// static
bool
XPCConvert::JSObject2NativeInterface(void** dest, HandleObject src,
                                     const nsID* iid,
                                     nsISupports* aOuter,
                                     nsresult* pErr)
{
    MOZ_ASSERT(dest, "bad param");
    MOZ_ASSERT(src, "bad param");
    MOZ_ASSERT(iid, "bad param");

    AutoJSContext cx;
    JSAutoRealm ar(cx, src);

    *dest = nullptr;
     if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

    nsISupports* iface;

    if (!aOuter) {
        // Note that if we have a non-null aOuter then it means that we are
        // forcing the creation of a wrapper even if the object *is* a
        // wrappedNative or other wise has 'nsISupportness'.
        // This allows wrapJSAggregatedToNative to work.

        // If we're looking at a security wrapper, see now if we're allowed to
        // pass it to C++. If we are, then fall through to the code below. If
        // we aren't, throw an exception eagerly.
        //
        // NB: It's very important that we _don't_ unwrap in the aOuter case,
        // because the caller may explicitly want to create the XPCWrappedJS
        // around a security wrapper. XBL does this with Xrays from the XBL
        // scope - see nsBindingManager::GetBindingImplementation.
        //
        // It's also very important that "inner" be rooted here.
        RootedObject inner(cx,
                           js::CheckedUnwrap(src,
                                             /* stopAtWindowProxy = */ false));
        if (!inner) {
            if (pErr)
                *pErr = NS_ERROR_XPC_SECURITY_MANAGER_VETO;
            return false;
        }

        // Is this really a native xpcom object with a wrapper?
        XPCWrappedNative* wrappedNative = nullptr;
        if (IS_WN_REFLECTOR(inner))
            wrappedNative = XPCWrappedNative::Get(inner);
        if (wrappedNative) {
            iface = wrappedNative->GetIdentityObject();
            return NS_SUCCEEDED(iface->QueryInterface(*iid, dest));
        }
        // else...

        // Deal with slim wrappers here.
        if (GetISupportsFromJSObject(inner ? inner : src, &iface)) {
            if (iface && NS_SUCCEEDED(iface->QueryInterface(*iid, dest))) {
                return true;
            }

            // If that failed, and iid is for mozIDOMWindowProxy, we actually
            // want the outer!
            if (iid->Equals(NS_GET_IID(mozIDOMWindowProxy))) {
                if (nsCOMPtr<mozIDOMWindow> inner = do_QueryInterface(iface)) {
                    iface = nsPIDOMWindowInner::From(inner)->GetOuterWindow();
                    return NS_SUCCEEDED(iface->QueryInterface(*iid, dest));
                }
            }

            return false;
        }

        // NOTE(nika): Remove if Promise becomes non-nsISupports
        // Deal with Promises being passed as nsISupports.  In that situation we
        // want to create a dom::Promise and use that.
        if (iid->Equals(NS_GET_IID(nsISupports))) {
            RootedObject innerObj(cx, inner);
            if (IsPromiseObject(innerObj)) {
                nsIGlobalObject* glob = NativeGlobal(innerObj);
                RefPtr<Promise> p = Promise::CreateFromExisting(glob, innerObj);
                return p && NS_SUCCEEDED(p->QueryInterface(*iid, dest));
            }
        }
    }

    RefPtr<nsXPCWrappedJS> wrapper;
    nsresult rv = nsXPCWrappedJS::GetNewOrUsed(src, *iid, getter_AddRefs(wrapper));
    if (pErr)
        *pErr = rv;

    if (NS_FAILED(rv) || !wrapper)
        return false;

    // If the caller wanted to aggregate this JS object to a native,
    // attach it to the wrapper. Note that we allow a maximum of one
    // aggregated native for a given XPCWrappedJS.
    if (aOuter)
        wrapper->SetAggregatedNativeObject(aOuter);

    // We need to go through the QueryInterface logic to make this return
    // the right thing for the various 'special' interfaces; e.g.
    // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
    // there is an outer to avoid nasty recursion.
    rv = aOuter ? wrapper->AggregatedQueryInterface(*iid, dest) :
        wrapper->QueryInterface(*iid, dest);
    if (pErr)
        *pErr = rv;
    return NS_SUCCEEDED(rv);
}

/***************************************************************************/
/***************************************************************************/

// static
nsresult
XPCConvert::ConstructException(nsresult rv, const char* message,
                               const char* ifaceName, const char* methodName,
                               nsISupports* data,
                               Exception** exceptn,
                               JSContext* cx,
                               Value* jsExceptionPtr)
{
    MOZ_ASSERT(!cx == !jsExceptionPtr, "Expected cx and jsExceptionPtr to cooccur.");

    static const char format[] = "\'%s\' when calling method: [%s::%s]";
    const char * msg = message;
    nsAutoCString sxmsg;    // must have the same lifetime as msg

    nsCOMPtr<nsIScriptError> errorObject = do_QueryInterface(data);
    if (errorObject) {
        nsString xmsg;
        if (NS_SUCCEEDED(errorObject->GetMessageMoz(getter_Copies(xmsg)))) {
            CopyUTF16toUTF8(xmsg, sxmsg);
            msg = sxmsg.get();
        }
    }
    if (!msg)
        if (!nsXPCException::NameAndFormatForNSResult(rv, nullptr, &msg) || ! msg)
            msg = "<error>";

    nsCString msgStr(msg);
    if (ifaceName && methodName)
        msgStr.AppendPrintf(format, msg, ifaceName, methodName);

    RefPtr<Exception> e = new Exception(msgStr, rv, EmptyCString(), nullptr, data);

    if (cx && jsExceptionPtr) {
        e->StowJSVal(*jsExceptionPtr);
    }

    e.forget(exceptn);
    return NS_OK;
}

/********************************/

class MOZ_STACK_CLASS AutoExceptionRestorer
{
public:
    AutoExceptionRestorer(JSContext* cx, const Value& v)
        : mContext(cx), tvr(cx, v)
    {
        JS_ClearPendingException(mContext);
    }

    ~AutoExceptionRestorer()
    {
        JS_SetPendingException(mContext, tvr);
    }

private:
    JSContext * const mContext;
    RootedValue tvr;
};

static nsresult
JSErrorToXPCException(const char* toStringResult,
                      const char* ifaceName,
                      const char* methodName,
                      const JSErrorReport* report,
                      Exception** exceptn)
{
    AutoJSContext cx;
    nsresult rv = NS_ERROR_FAILURE;
    RefPtr<nsScriptError> data;
    if (report) {
        nsAutoString bestMessage;
        if (report && report->message()) {
            CopyUTF8toUTF16(report->message().c_str(), bestMessage);
        } else if (toStringResult) {
            CopyUTF8toUTF16(toStringResult, bestMessage);
        } else {
            bestMessage.AssignLiteral("JavaScript Error");
        }

        const char16_t* linebuf = report->linebuf();

        data = new nsScriptError();
        data->InitWithWindowID(
            bestMessage,
            NS_ConvertASCIItoUTF16(report->filename),
            linebuf ? nsDependentString(linebuf, report->linebufLength()) : EmptyString(),
            report->lineno,
            report->tokenOffset(), report->flags,
            NS_LITERAL_CSTRING("XPConnect JavaScript"),
            nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(cx));
    }

    if (data) {
        // Pass nullptr for the message: ConstructException will get a message
        // from the nsIScriptError.
        rv = XPCConvert::ConstructException(NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS,
                                            nullptr, ifaceName,
                                            methodName,
                                            static_cast<nsIScriptError*>(data.get()),
                                            exceptn, nullptr, nullptr);
    } else {
        rv = XPCConvert::ConstructException(NS_ERROR_XPC_JAVASCRIPT_ERROR,
                                            nullptr, ifaceName, methodName,
                                            nullptr, exceptn, nullptr, nullptr);
    }
    return rv;
}

// static
nsresult
XPCConvert::JSValToXPCException(MutableHandleValue s,
                                const char* ifaceName,
                                const char* methodName,
                                Exception** exceptn)
{
    AutoJSContext cx;
    AutoExceptionRestorer aer(cx, s);

    if (!s.isPrimitive()) {
        // we have a JSObject
        RootedObject obj(cx, s.toObjectOrNull());

        if (!obj) {
            NS_ERROR("when is an object not an object?");
            return NS_ERROR_FAILURE;
        }

        // is this really a native xpcom object with a wrapper?
        JSObject* unwrapped = js::CheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
        if (!unwrapped)
            return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
        if (nsCOMPtr<nsISupports> supports = UnwrapReflectorToISupports(unwrapped)) {
            nsCOMPtr<Exception> iface = do_QueryInterface(supports);
            if (iface) {
                // just pass through the exception (with extra ref and all)
                iface.forget(exceptn);
                return NS_OK;
            }

            // it is a wrapped native, but not an exception!
            return ConstructException(NS_ERROR_XPC_JS_THREW_NATIVE_OBJECT,
                                      nullptr, ifaceName, methodName, supports,
                                      exceptn, nullptr, nullptr);
        } else {
            // It is a JSObject, but not a wrapped native...

            // If it is an engine Error with an error report then let's
            // extract the report and build an xpcexception from that
            const JSErrorReport* report;
            if (nullptr != (report = JS_ErrorFromException(cx, obj))) {
                JSAutoByteString toStringResult;
                RootedString str(cx, ToString(cx, s));
                if (str)
                    toStringResult.encodeUtf8(cx, str);
                return JSErrorToXPCException(toStringResult.ptr(), ifaceName,
                                             methodName, report, exceptn);
            }

            // XXX we should do a check against 'js_ErrorClass' here and
            // do the right thing - even though it has no JSErrorReport,
            // The fact that it is a JSError exceptions means we can extract
            // particular info and our 'result' should reflect that.

            // otherwise we'll just try to convert it to a string

            JSString* str = ToString(cx, s);
            if (!str)
                return NS_ERROR_FAILURE;

            JSAutoByteString strBytes(cx, str);
            if (!strBytes)
                return NS_ERROR_FAILURE;

            return ConstructException(NS_ERROR_XPC_JS_THREW_JS_OBJECT,
                                      strBytes.ptr(), ifaceName, methodName,
                                      nullptr, exceptn, cx, s.address());
        }
    }

    if (s.isUndefined() || s.isNull()) {
        return ConstructException(NS_ERROR_XPC_JS_THREW_NULL,
                                  nullptr, ifaceName, methodName, nullptr,
                                  exceptn, cx, s.address());
    }

    if (s.isNumber()) {
        // lets see if it looks like an nsresult
        nsresult rv;
        double number;
        bool isResult = false;

        if (s.isInt32()) {
            rv = (nsresult) s.toInt32();
            if (NS_FAILED(rv))
                isResult = true;
            else
                number = (double) s.toInt32();
        } else {
            number = s.toDouble();
            if (number > 0.0 &&
                number < (double)0xffffffff &&
                0.0 == fmod(number,1)) {
                // Visual Studio 9 doesn't allow casting directly from a
                // double to an enumeration type, contrary to 5.2.9(10) of
                // C++11, so add an intermediate cast.
                rv = (nsresult)(uint32_t) number;
                if (NS_FAILED(rv))
                    isResult = true;
            }
        }

        if (isResult)
            return ConstructException(rv, nullptr, ifaceName, methodName,
                                      nullptr, exceptn, cx, s.address());
        else {
            // XXX all this nsISupportsDouble code seems a little redundant
            // now that we're storing the Value in the exception...
            nsCOMPtr<nsISupportsDouble> data;
            nsCOMPtr<nsIComponentManager> cm;
            if (NS_FAILED(NS_GetComponentManager(getter_AddRefs(cm))) || !cm ||
                NS_FAILED(cm->CreateInstanceByContractID(NS_SUPPORTS_DOUBLE_CONTRACTID,
                                                         nullptr,
                                                         NS_GET_IID(nsISupportsDouble),
                                                         getter_AddRefs(data))))
                return NS_ERROR_FAILURE;
            data->SetData(number);
            rv = ConstructException(NS_ERROR_XPC_JS_THREW_NUMBER, nullptr,
                                    ifaceName, methodName, data, exceptn, cx, s.address());
            return rv;
        }
    }

    // otherwise we'll just try to convert it to a string
    // Note: e.g., bools get converted to JSStrings by this code.

    JSString* str = ToString(cx, s);
    if (str) {
        JSAutoByteString strBytes(cx, str);
        if (!!strBytes) {
            return ConstructException(NS_ERROR_XPC_JS_THREW_STRING,
                                      strBytes.ptr(), ifaceName, methodName,
                                      nullptr, exceptn, cx, s.address());
        }
    }
    return NS_ERROR_FAILURE;
}

/***************************************************************************/

// array fun...

static bool
ValidArrayType(const nsXPTType& type)
{
    switch (type.Tag()) {
        // These types aren't allowed to be in arrays.
        case nsXPTType::T_VOID:
        case nsXPTType::T_DOMSTRING:
        case nsXPTType::T_UTF8STRING:
        case nsXPTType::T_CSTRING:
        case nsXPTType::T_ASTRING:
        case nsXPTType::T_PSTRING_SIZE_IS:
        case nsXPTType::T_PWSTRING_SIZE_IS:
        case nsXPTType::T_ARRAY:
            return false;
        default:
            return true;
    }
}

// static
bool
XPCConvert::NativeArray2JS(MutableHandleValue d, const void* const* s,
                           const nsXPTType& type, const nsID* iid,
                           uint32_t count, nsresult* pErr)
{
    MOZ_ASSERT(s, "bad param");

    AutoJSContext cx;

    // XXX add support for putting chars in a string rather than an array

    // XXX add support to indicate *which* array element was not convertable

    RootedObject array(cx, JS_NewArrayObject(cx, count));
    if (!array)
        return false;

    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_NATIVE;

    if (!ValidArrayType(type))
        return false;

    RootedValue current(cx, JS::NullValue());
    for (uint32_t i = 0; i < count; ++i) {
        if (!NativeData2JS(&current, type.ElementPtr(*s, i), type, iid, 0, pErr) ||
            !JS_DefineElement(cx, array, i, current, JSPROP_ENUMERATE))
            return false;
    }

    if (pErr)
        *pErr = NS_OK;
    d.setObject(*array);
    return true;
}


// Fast conversion of typed arrays to native using memcpy.
// No float or double canonicalization is done. Called by
// JSarray2Native whenever a TypedArray is met. ArrayBuffers
// are not accepted; create a properly typed array view on them
// first. The element type of array must match the XPCOM
// type in size, type and signedness exactly. As an exception,
// Uint8ClampedArray is allowed for arrays of uint8_t. DataViews
// are not supported.

// static
bool
XPCConvert::JSTypedArray2Native(void** d,
                                JSObject* jsArray,
                                uint32_t count,
                                const nsXPTType& type,
                                nsresult* pErr)
{
    MOZ_ASSERT(jsArray, "bad param");
    MOZ_ASSERT(d, "bad param");
    MOZ_ASSERT(JS_IsTypedArrayObject(jsArray), "not a typed array");

    // Check the actual length of the input array against the
    // given size_is.
    uint32_t len = JS_GetTypedArrayLength(jsArray);
    if (len < count) {
        if (pErr)
            *pErr = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;

        return false;
    }

    uint8_t expected;
    switch (JS_GetArrayBufferViewType(jsArray)) {
    case js::Scalar::Int8:
        expected = nsXPTType::T_I8;
        break;

    case js::Scalar::Uint8:
    case js::Scalar::Uint8Clamped:
        expected = nsXPTType::T_U8;
        break;

    case js::Scalar::Int16:
        expected = nsXPTType::T_I16;
        break;

    case js::Scalar::Uint16:
        expected = nsXPTType::T_U16;
        break;

    case js::Scalar::Int32:
        expected = nsXPTType::T_I32;
        break;

    case js::Scalar::Uint32:
        expected = nsXPTType::T_U32;
        break;

    case js::Scalar::Float32:
        expected = nsXPTType::T_FLOAT;
        break;

    case js::Scalar::Float64:
        expected = nsXPTType::T_DOUBLE;
        break;

    // Yet another array type was defined? It is not supported yet...
    default:
        if (pErr)
            *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;
        return false;
    }

    // Check that the type we got is the type we expected.
    if (expected != type.Tag()) {
        if (pErr)
            *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;
        return false;
    }

    // Check if the size would overflow uint32_t.
    if (count > (UINT32_MAX / type.Stride())) {
        if (pErr)
            *pErr = NS_ERROR_OUT_OF_MEMORY;
        return false;
    }

    // Get the backing memory buffer to copy out of.
    JS::AutoCheckCannotGC nogc;
    bool isShared;
    void* buf = JS_GetArrayBufferViewData(jsArray, &isShared, nogc);

    // Require opting in to shared memory - a future project.
    if (isShared) {
        if (pErr)
            *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;
        return false;
    }

    // Allocate the buffer, and copy.
    *d = moz_xmalloc(count * type.Stride());
    if (!*d) {
        if (pErr)
            *pErr = NS_ERROR_OUT_OF_MEMORY;
        return false;
    }

    memcpy(*d, buf, count * type.Stride());

    if (pErr)
        *pErr = NS_OK;

    return true;
}

// static
bool
XPCConvert::JSArray2Native(void** d, HandleValue s,
                           uint32_t count, const nsXPTType& type,
                           const nsID* iid, nsresult* pErr)
{
    MOZ_ASSERT(d, "bad param");

    AutoJSContext cx;

    // FIXME: XPConnect historically has shortcut the JSArray2Native codepath in
    // its caller if count is 0, allowing arbitrary values to be passed as
    // arrays and interpreted as the empty array (bug 1458987).
    //
    // NOTE: Once this is fixed, null/undefined should be allowed for arrays if
    // count is 0.
    if (count == 0) {
        *d = nullptr;
        return true;
    }

    // XXX add support for getting chars from strings

    // XXX add support to indicate *which* array element was not convertable

    if (pErr)
        *pErr = NS_ERROR_XPC_BAD_CONVERT_JS;

    if (!ValidArrayType(type))
        return false;

    if (s.isNullOrUndefined()) {
        if (pErr)
            *pErr = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;
        return false;
    }

    if (!s.isObject()) {
        if (pErr)
            *pErr = NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY;
        return false;
    }

    RootedObject jsarray(cx, &s.toObject());

    // If this is a typed array, then try a fast conversion with memcpy.
    if (JS_IsTypedArrayObject(jsarray)) {
        return JSTypedArray2Native(d, jsarray, count, type, pErr);
    }

    bool isArray;
    if (!JS_IsArrayObject(cx, jsarray, &isArray) || !isArray) {
        if (pErr)
            *pErr = NS_ERROR_XPC_CANT_CONVERT_OBJECT_TO_ARRAY;
        return false;
    }

    uint32_t len;
    if (!JS_GetArrayLength(cx, jsarray, &len) || len < count) {
        if (pErr)
            *pErr = NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY;
        return false;
    }

    if (count > (UINT32_MAX / type.Stride())) {
        if (pErr)
            *pErr = NS_ERROR_OUT_OF_MEMORY;
        return false;
    }

    // Allocate the destination array, and begin converting elements.
    *d = moz_xmalloc(count * type.Stride());
    if (!*d) {
        if (pErr)
            *pErr = NS_ERROR_OUT_OF_MEMORY;
        return false;
    }

    RootedValue current(cx);
    uint32_t initedCount;
    for (initedCount = 0; initedCount < count; ++initedCount) {
        if (!JS_GetElement(cx, jsarray, initedCount, &current) ||
            !JSData2Native(type.ElementPtr(*d, initedCount),
                           current, type, iid, 0, pErr))
            break;
    }

    // Check if we handled every array element.
    if (initedCount == count) {
        if (pErr)
            *pErr = NS_OK;
        return true;
    }

    // Something failed! Clean up after ourselves.
    for (uint32_t i = 0; i < initedCount; ++i) {
        CleanupValue(type, type.ElementPtr(*d, i));
    }
    free(*d);
    *d = nullptr;
    return false;
}

/***************************************************************************/

// Internal implementation details for xpc::CleanupValue.

void
xpc::InnerCleanupValue(const nsXPTType& aType, void* aValue, uint32_t aArrayLen)
{
    MOZ_ASSERT(!aType.IsArithmetic(),
               "Arithmetic types should not get to InnerCleanupValue!");
    MOZ_ASSERT(aArrayLen == 0 ||
               aType.Tag() == nsXPTType::T_PSTRING_SIZE_IS ||
               aType.Tag() == nsXPTType::T_PWSTRING_SIZE_IS ||
               aType.Tag() == nsXPTType::T_ARRAY,
               "Array lengths may only appear for certain types!");

    switch (aType.Tag()) {
        // Pointer types
        case nsXPTType::T_DOMOBJECT:
            aType.GetDOMObjectInfo().Cleanup(*(void**)aValue);
            break;

        case nsXPTType::T_PROMISE:
            (*(mozilla::dom::Promise**)aValue)->Release();
            break;

        case nsXPTType::T_INTERFACE:
        case nsXPTType::T_INTERFACE_IS:
            (*(nsISupports**)aValue)->Release();
            break;

        // String types
        case nsXPTType::T_DOMSTRING:
        case nsXPTType::T_ASTRING:
            ((nsAString*)aValue)->Truncate();
            break;
        case nsXPTType::T_UTF8STRING:
        case nsXPTType::T_CSTRING:
            ((nsACString*)aValue)->Truncate();
            break;

        // Pointer Types
        case nsXPTType::T_IID:
        case nsXPTType::T_CHAR_STR:
        case nsXPTType::T_WCHAR_STR:
        case nsXPTType::T_PSTRING_SIZE_IS:
        case nsXPTType::T_PWSTRING_SIZE_IS:
            free(*(void**)aValue);
            break;

        // Array Types
        case nsXPTType::T_ARRAY:
        {
            const nsXPTType& elty = aType.ArrayElementType();
            void* elements = *(void**)aValue;

            for (uint32_t i = 0; i < aArrayLen; ++i) {
                CleanupValue(elty, elty.ElementPtr(elements, i));
            }
            free(elements);
            break;
        }
    }

    // Null out the pointer if we have it.
    if (aType.HasPointerRepr()) {
        *(void**)aValue = nullptr;
    }
}
