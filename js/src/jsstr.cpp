/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsstr.h"

#include "mozilla/Attributes.h"
#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Range.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Unused.h"

#include <ctype.h>
#include <limits>
#include <string.h>

#include "jsapi.h"
#include "jsarray.h"
#include "jsatom.h"
#include "jsbool.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsopcode.h"
#include "jstypes.h"
#include "jsutil.h"

#include "builtin/RegExp.h"
#include "jit/InlinableNatives.h"
#include "js/Conversions.h"
#include "js/UniquePtr.h"
#if ENABLE_INTL_API
#include "unicode/uchar.h"
#include "unicode/unorm2.h"
#endif
#include "vm/GlobalObject.h"
#include "vm/Interpreter.h"
#include "vm/Opcodes.h"
#include "vm/Printer.h"
#include "vm/RegExpObject.h"
#include "vm/RegExpStatics.h"
#include "vm/StringBuffer.h"
#include "vm/Unicode.h"

#include "vm/Interpreter-inl.h"
#include "vm/String-inl.h"
#include "vm/StringObject-inl.h"
#include "vm/TypeInference-inl.h"

using namespace js;
using namespace js::gc;

using JS::Symbol;
using JS::SymbolCode;
using JS::ToInt32;
using JS::ToUint32;

using mozilla::AssertedCast;
using mozilla::CheckedInt;
using mozilla::IsNaN;
using mozilla::IsNegativeZero;
using mozilla::IsSame;
using mozilla::Move;
using mozilla::PodCopy;
using mozilla::PodEqual;
using mozilla::RangedPtr;

using JS::AutoCheckCannotGC;

static JSLinearString*
ArgToRootedString(JSContext* cx, const CallArgs& args, unsigned argno)
{
    if (argno >= args.length())
        return cx->names().undefined;

    JSString* str = ToString<CanGC>(cx, args[argno]);
    if (!str)
        return nullptr;

    args[argno].setString(str);
    return str->ensureLinear(cx);
}

/*
 * Forward declarations for URI encode/decode and helper routines
 */
static bool
str_decodeURI(JSContext* cx, unsigned argc, Value* vp);

static bool
str_decodeURI_Component(JSContext* cx, unsigned argc, Value* vp);

static bool
str_encodeURI(JSContext* cx, unsigned argc, Value* vp);

static bool
str_encodeURI_Component(JSContext* cx, unsigned argc, Value* vp);

/*
 * Global string methods
 */


/* ES5 B.2.1 */
template <typename CharT>
static Latin1Char*
Escape(JSContext* cx, const CharT* chars, uint32_t length, uint32_t* newLengthOut)
{
    static const uint8_t shouldPassThrough[128] = {
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,1,1,0,1,1,1,       /*    !"#$%&'()*+,-./  */
         1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,       /*   0123456789:;<=>?  */
         1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,       /*   @ABCDEFGHIJKLMNO  */
         1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,1,       /*   PQRSTUVWXYZ[\]^_  */
         0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,       /*   `abcdefghijklmno  */
         1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,       /*   pqrstuvwxyz{\}~  DEL */
    };

    /* Take a first pass and see how big the result string will need to be. */
    uint32_t newLength = length;
    for (size_t i = 0; i < length; i++) {
        char16_t ch = chars[i];
        if (ch < 128 && shouldPassThrough[ch])
            continue;

        /* The character will be encoded as %XX or %uXXXX. */
        newLength += (ch < 256) ? 2 : 5;

        /*
         * newlength is incremented by at most 5 on each iteration, so worst
         * case newlength == length * 6. This can't overflow.
         */
        static_assert(JSString::MAX_LENGTH < UINT32_MAX / 6,
                      "newlength must not overflow");
    }

    Latin1Char* newChars = cx->pod_malloc<Latin1Char>(newLength + 1);
    if (!newChars)
        return nullptr;

    static const char digits[] = "0123456789ABCDEF";

    size_t i, ni;
    for (i = 0, ni = 0; i < length; i++) {
        char16_t ch = chars[i];
        if (ch < 128 && shouldPassThrough[ch]) {
            newChars[ni++] = ch;
        } else if (ch < 256) {
            newChars[ni++] = '%';
            newChars[ni++] = digits[ch >> 4];
            newChars[ni++] = digits[ch & 0xF];
        } else {
            newChars[ni++] = '%';
            newChars[ni++] = 'u';
            newChars[ni++] = digits[ch >> 12];
            newChars[ni++] = digits[(ch & 0xF00) >> 8];
            newChars[ni++] = digits[(ch & 0xF0) >> 4];
            newChars[ni++] = digits[ch & 0xF];
        }
    }
    MOZ_ASSERT(ni == newLength);
    newChars[newLength] = 0;

    *newLengthOut = newLength;
    return newChars;
}

static bool
str_escape(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    JSLinearString* str = ArgToRootedString(cx, args, 0);
    if (!str)
        return false;

    ScopedJSFreePtr<Latin1Char> newChars;
    uint32_t newLength = 0;  // initialize to silence GCC warning
    if (str->hasLatin1Chars()) {
        AutoCheckCannotGC nogc;
        newChars = Escape(cx, str->latin1Chars(nogc), str->length(), &newLength);
    } else {
        AutoCheckCannotGC nogc;
        newChars = Escape(cx, str->twoByteChars(nogc), str->length(), &newLength);
    }

    if (!newChars)
        return false;

    JSString* res = NewString<CanGC>(cx, newChars.get(), newLength);
    if (!res)
        return false;

    newChars.forget();
    args.rval().setString(res);
    return true;
}

template <typename CharT>
static inline bool
Unhex4(const RangedPtr<const CharT> chars, char16_t* result)
{
    char16_t a = chars[0],
             b = chars[1],
             c = chars[2],
             d = chars[3];

    if (!(JS7_ISHEX(a) && JS7_ISHEX(b) && JS7_ISHEX(c) && JS7_ISHEX(d)))
        return false;

    *result = (((((JS7_UNHEX(a) << 4) + JS7_UNHEX(b)) << 4) + JS7_UNHEX(c)) << 4) + JS7_UNHEX(d);
    return true;
}

template <typename CharT>
static inline bool
Unhex2(const RangedPtr<const CharT> chars, char16_t* result)
{
    char16_t a = chars[0],
             b = chars[1];

    if (!(JS7_ISHEX(a) && JS7_ISHEX(b)))
        return false;

    *result = (JS7_UNHEX(a) << 4) + JS7_UNHEX(b);
    return true;
}

template <typename CharT>
static bool
Unescape(StringBuffer& sb, const mozilla::Range<const CharT> chars)
{
    /*
     * NB: use signed integers for length/index to allow simple length
     * comparisons without unsigned-underflow hazards.
     */
    static_assert(JSString::MAX_LENGTH <= INT_MAX, "String length must fit in a signed integer");
    int length = AssertedCast<int>(chars.length());

    /*
     * Note that the spec algorithm has been optimized to avoid building
     * a string in the case where no escapes are present.
     */

    /* Step 4. */
    int k = 0;
    bool building = false;

    /* Step 5. */
    while (k < length) {
        /* Step 6. */
        char16_t c = chars[k];

        /* Step 7. */
        if (c != '%')
            goto step_18;

        /* Step 8. */
        if (k > length - 6)
            goto step_14;

        /* Step 9. */
        if (chars[k + 1] != 'u')
            goto step_14;

#define ENSURE_BUILDING                                      \
        do {                                                 \
            if (!building) {                                 \
                building = true;                             \
                if (!sb.reserve(length))                     \
                    return false;                            \
                sb.infallibleAppend(chars.begin().get(), k); \
            }                                                \
        } while(false);

        /* Step 10-13. */
        if (Unhex4(chars.begin() + k + 2, &c)) {
            ENSURE_BUILDING;
            k += 5;
            goto step_18;
        }

      step_14:
        /* Step 14. */
        if (k > length - 3)
            goto step_18;

        /* Step 15-17. */
        if (Unhex2(chars.begin() + k + 1, &c)) {
            ENSURE_BUILDING;
            k += 2;
        }

      step_18:
        if (building && !sb.append(c))
            return false;

        /* Step 19. */
        k += 1;
    }

    return true;
#undef ENSURE_BUILDING
}

/* ES5 B.2.2 */
static bool
str_unescape(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    /* Step 1. */
    RootedLinearString str(cx, ArgToRootedString(cx, args, 0));
    if (!str)
        return false;

    /* Step 3. */
    StringBuffer sb(cx);
    if (str->hasTwoByteChars() && !sb.ensureTwoByteChars())
        return false;

    if (str->hasLatin1Chars()) {
        AutoCheckCannotGC nogc;
        if (!Unescape(sb, str->latin1Range(nogc)))
            return false;
    } else {
        AutoCheckCannotGC nogc;
        if (!Unescape(sb, str->twoByteRange(nogc)))
            return false;
    }

    JSLinearString* result;
    if (!sb.empty()) {
        result = sb.finishString();
        if (!result)
            return false;
    } else {
        result = str;
    }

    args.rval().setString(result);
    return true;
}

#if JS_HAS_UNEVAL
static bool
str_uneval(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSString* str = ValueToSource(cx, args.get(0));
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}
#endif

static const JSFunctionSpec string_functions[] = {
    JS_FN(js_escape_str,             str_escape,                1, JSPROP_RESOLVING),
    JS_FN(js_unescape_str,           str_unescape,              1, JSPROP_RESOLVING),
#if JS_HAS_UNEVAL
    JS_FN(js_uneval_str,             str_uneval,                1, JSPROP_RESOLVING),
#endif
    JS_FN(js_decodeURI_str,          str_decodeURI,             1, JSPROP_RESOLVING),
    JS_FN(js_encodeURI_str,          str_encodeURI,             1, JSPROP_RESOLVING),
    JS_FN(js_decodeURIComponent_str, str_decodeURI_Component,   1, JSPROP_RESOLVING),
    JS_FN(js_encodeURIComponent_str, str_encodeURI_Component,   1, JSPROP_RESOLVING),

    JS_FS_END
};

static const unsigned STRING_ELEMENT_ATTRS = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;

static bool
str_enumerate(JSContext* cx, HandleObject obj)
{
    RootedString str(cx, obj->as<StringObject>().unbox());
    RootedValue value(cx);
    for (size_t i = 0, length = str->length(); i < length; i++) {
        JSString* str1 = NewDependentString(cx, str, i, 1);
        if (!str1)
            return false;
        value.setString(str1);
        if (!DefineElement(cx, obj, i, value, nullptr, nullptr,
                           STRING_ELEMENT_ATTRS | JSPROP_RESOLVING))
        {
            return false;
        }
    }

    return true;
}

static bool
str_mayResolve(const JSAtomState&, jsid id, JSObject*)
{
    // str_resolve ignores non-integer ids.
    return JSID_IS_INT(id);
}

static bool
str_resolve(JSContext* cx, HandleObject obj, HandleId id, bool* resolvedp)
{
    if (!JSID_IS_INT(id))
        return true;

    RootedString str(cx, obj->as<StringObject>().unbox());

    int32_t slot = JSID_TO_INT(id);
    if ((size_t)slot < str->length()) {
        JSString* str1 = cx->staticStrings().getUnitStringForElement(cx, str, size_t(slot));
        if (!str1)
            return false;
        RootedValue value(cx, StringValue(str1));
        if (!DefineElement(cx, obj, uint32_t(slot), value, nullptr, nullptr,
                           STRING_ELEMENT_ATTRS | JSPROP_RESOLVING))
        {
            return false;
        }
        *resolvedp = true;
    }
    return true;
}

static const ClassOps StringObjectClassOps = {
    nullptr, /* addProperty */
    nullptr, /* delProperty */
    nullptr, /* getProperty */
    nullptr, /* setProperty */
    str_enumerate,
    nullptr, /* newEnumerate */
    str_resolve,
    str_mayResolve
};

const Class StringObject::class_ = {
    js_String_str,
    JSCLASS_HAS_RESERVED_SLOTS(StringObject::RESERVED_SLOTS) |
    JSCLASS_HAS_CACHED_PROTO(JSProto_String),
    &StringObjectClassOps
};

/*
 * Perform the initial |RequireObjectCoercible(thisv)| and |ToString(thisv)|
 * from nearly all String.prototype.* functions.
 */
static MOZ_ALWAYS_INLINE JSString*
ToStringForStringFunction(JSContext* cx, HandleValue thisv)
{
    if (!CheckRecursionLimit(cx))
        return nullptr;

    if (thisv.isString())
        return thisv.toString();

    if (thisv.isObject()) {
        RootedObject obj(cx, &thisv.toObject());
        if (obj->is<StringObject>()) {
            StringObject* nobj = &obj->as<StringObject>();
            // We have to make sure that the ToPrimitive call from ToString
            // would be unobservable.
            if (HasNoToPrimitiveMethodPure(nobj, cx) &&
                HasNativeMethodPure(nobj, cx->names().toString, str_toString, cx))
            {
                return nobj->unbox();
            }
        }
    } else if (thisv.isNullOrUndefined()) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_CANT_CONVERT_TO,
                                  thisv.isNull() ? "null" : "undefined", "object");
        return nullptr;
    }

    return ToStringSlow<CanGC>(cx, thisv);
}

MOZ_ALWAYS_INLINE bool
IsString(HandleValue v)
{
    return v.isString() || (v.isObject() && v.toObject().is<StringObject>());
}

#if JS_HAS_TOSOURCE

MOZ_ALWAYS_INLINE bool
str_toSource_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsString(args.thisv()));

    Rooted<JSString*> str(cx, ToString<CanGC>(cx, args.thisv()));
    if (!str)
        return false;

    str = QuoteString(cx, str, '"');
    if (!str)
        return false;

    StringBuffer sb(cx);
    if (!sb.append("(new String(") || !sb.append(str) || !sb.append("))"))
        return false;

    str = sb.finishString();
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

static bool
str_toSource(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsString, str_toSource_impl>(cx, args);
}

#endif /* JS_HAS_TOSOURCE */

MOZ_ALWAYS_INLINE bool
str_toString_impl(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(IsString(args.thisv()));

    args.rval().setString(args.thisv().isString()
                              ? args.thisv().toString()
                              : args.thisv().toObject().as<StringObject>().unbox());
    return true;
}

bool
js::str_toString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return CallNonGenericMethod<IsString, str_toString_impl>(cx, args);
}

/*
 * Java-like string native methods.
 */

JSString*
js::SubstringKernel(JSContext* cx, HandleString str, int32_t beginInt, int32_t lengthInt)
{
    MOZ_ASSERT(0 <= beginInt);
    MOZ_ASSERT(0 <= lengthInt);
    MOZ_ASSERT(uint32_t(beginInt) <= str->length());
    MOZ_ASSERT(uint32_t(lengthInt) <= str->length() - beginInt);

    uint32_t begin = beginInt;
    uint32_t len = lengthInt;

    /*
     * Optimization for one level deep ropes.
     * This is common for the following pattern:
     *
     * while() {
     *   text = text.substr(0, x) + "bla" + text.substr(x)
     *   test.charCodeAt(x + 1)
     * }
     */
    if (str->isRope()) {
        JSRope* rope = &str->asRope();

        /* Substring is totally in leftChild of rope. */
        if (begin + len <= rope->leftChild()->length())
            return NewDependentString(cx, rope->leftChild(), begin, len);

        /* Substring is totally in rightChild of rope. */
        if (begin >= rope->leftChild()->length()) {
            begin -= rope->leftChild()->length();
            return NewDependentString(cx, rope->rightChild(), begin, len);
        }

        /*
         * Requested substring is partly in the left and partly in right child.
         * Create a rope of substrings for both childs.
         */
        MOZ_ASSERT(begin < rope->leftChild()->length() &&
                   begin + len > rope->leftChild()->length());

        size_t lhsLength = rope->leftChild()->length() - begin;
        size_t rhsLength = begin + len - rope->leftChild()->length();

        Rooted<JSRope*> ropeRoot(cx, rope);
        RootedString lhs(cx, NewDependentString(cx, ropeRoot->leftChild(), begin, lhsLength));
        if (!lhs)
            return nullptr;

        RootedString rhs(cx, NewDependentString(cx, ropeRoot->rightChild(), 0, rhsLength));
        if (!rhs)
            return nullptr;

        return JSRope::new_<CanGC>(cx, lhs, rhs, len);
    }

    return NewDependentString(cx, str, begin, len);
}

template <typename CharT> struct MaximumInlineLength;

template<> struct MaximumInlineLength<Latin1Char> {
    static constexpr size_t value = JSFatInlineString::MAX_LENGTH_LATIN1;
};

template<> struct MaximumInlineLength<char16_t> {
    static constexpr size_t value = JSFatInlineString::MAX_LENGTH_TWO_BYTE;
};

// Character buffer class used for ToLowerCase and ToUpperCase operations.
//
// Case conversion operations normally return a string with the same length as
// the input string. To avoid over-allocation, we optimistically allocate an
// array with same size as the input string and only when we detect special
// casing characters, which can change the output string length, we reallocate
// the output buffer to the final string length.
//
// As a further mean to improve runtime performance, the character buffer
// contains an inline storage, so we don't need to heap-allocate an array when
// a JSInlineString will be used for the output string.
//
// Why not use mozilla::Vector instead? mozilla::Vector doesn't provide enough
// fine-grained control to avoid over-allocation when (re)allocating for exact
// buffer sizes. This led to visible performance regressions in µ-benchmarks.
template <typename CharT>
class MOZ_NON_PARAM InlineCharBuffer
{
    using CharPtr = UniquePtr<CharT[], JS::FreePolicy>;
    static constexpr size_t InlineCapacity = MaximumInlineLength<CharT>::value;

    CharT inlineStorage[InlineCapacity];
    CharPtr heapStorage;

#ifdef DEBUG
    // In debug mode, we keep track of the requested string lengths to ensure
    // all character buffer methods are called in the correct order and with
    // the expected argument values.
    size_t lastRequestedLength = 0;

    void assertValidRequest(size_t expectedLastLength, size_t length) {
        MOZ_ASSERT(length > expectedLastLength, "cannot shrink requested length");
        MOZ_ASSERT(lastRequestedLength == expectedLastLength);
        lastRequestedLength = length;
    }
#else
    void assertValidRequest(size_t expectedLastLength, size_t length) {}
#endif

  public:
    CharT* get()
    {
        return heapStorage ? heapStorage.get() : inlineStorage;
    }

    bool maybeAlloc(JSContext* cx, size_t length)
    {
        assertValidRequest(0, length);

        if (length <= InlineCapacity)
            return true;

        MOZ_ASSERT(!heapStorage, "heap storage already allocated");
        heapStorage = cx->make_pod_array<CharT>(length + 1);
        return !!heapStorage;
    }

    bool maybeRealloc(JSContext* cx, size_t oldLength, size_t newLength)
    {
        assertValidRequest(oldLength, newLength);

        if (newLength <= InlineCapacity)
            return true;

        if (!heapStorage) {
            heapStorage = cx->make_pod_array<CharT>(newLength + 1);
            if (!heapStorage)
                return false;

            MOZ_ASSERT(oldLength <= InlineCapacity);
            PodCopy(heapStorage.get(), inlineStorage, oldLength);
            return true;
        }

        CharT* oldChars = heapStorage.release();
        CharT* newChars = cx->pod_realloc(oldChars, oldLength + 1, newLength + 1);
        if (!newChars) {
            js_free(oldChars);
            return false;
        }

        heapStorage.reset(newChars);
        return true;
    }

    JSString* toString(JSContext* cx, size_t length)
    {
        MOZ_ASSERT(length == lastRequestedLength);

        if (JSInlineString::lengthFits<CharT>(length)) {
            MOZ_ASSERT(!heapStorage,
                       "expected only inline storage when length fits in inline string");

            mozilla::Range<const CharT> range(inlineStorage, length);
            return NewInlineString<CanGC>(cx, range);
        }

        MOZ_ASSERT(heapStorage, "heap storage was not allocated for non-inline string");

        heapStorage.get()[length] = '\0'; // Null-terminate
        JSString* res = NewStringDontDeflate<CanGC>(cx, heapStorage.get(), length);
        if (!res)
            return nullptr;

        mozilla::Unused << heapStorage.release();
        return res;
    }
};

/**
 * U+03A3 GREEK CAPITAL LETTER SIGMA has two different lower case mappings
 * depending on its context:
 * When it's preceded by a cased character and not followed by another cased
 * character, its lower case form is U+03C2 GREEK SMALL LETTER FINAL SIGMA.
 * Otherwise its lower case mapping is U+03C3 GREEK SMALL LETTER SIGMA.
 *
 * Unicode 9.0, §3.13 Default Case Algorithms
 */
static char16_t
Final_Sigma(const char16_t* chars, size_t length, size_t index)
{
    MOZ_ASSERT(index < length);
    MOZ_ASSERT(chars[index] == unicode::GREEK_CAPITAL_LETTER_SIGMA);
    MOZ_ASSERT(unicode::ToLowerCase(unicode::GREEK_CAPITAL_LETTER_SIGMA) ==
               unicode::GREEK_SMALL_LETTER_SIGMA);

#if ENABLE_INTL_API
    // Tell the analysis the BinaryProperty.contains function pointer called by
    // u_hasBinaryProperty cannot GC.
    JS::AutoSuppressGCAnalysis nogc;

    bool precededByCased = false;
    for (size_t i = index; i > 0; ) {
        char16_t c = chars[--i];
        uint32_t codePoint = c;
        if (unicode::IsTrailSurrogate(c) && i > 0) {
            char16_t lead = chars[i - 1];
            if (unicode::IsLeadSurrogate(lead)) {
                codePoint = unicode::UTF16Decode(lead, c);
                i--;
            }
        }

        // Ignore any characters with the property Case_Ignorable.
        // NB: We need to skip over all Case_Ignorable characters, even when
        // they also have the Cased binary property.
        if (u_hasBinaryProperty(codePoint, UCHAR_CASE_IGNORABLE))
            continue;

        precededByCased = u_hasBinaryProperty(codePoint, UCHAR_CASED);
        break;
    }
    if (!precededByCased)
        return unicode::GREEK_SMALL_LETTER_SIGMA;

    bool followedByCased = false;
    for (size_t i = index + 1; i < length; ) {
        char16_t c = chars[i++];
        uint32_t codePoint = c;
        if (unicode::IsLeadSurrogate(c) && i < length) {
            char16_t trail = chars[i];
            if (unicode::IsTrailSurrogate(trail)) {
                codePoint = unicode::UTF16Decode(c, trail);
                i++;
            }
        }

        // Ignore any characters with the property Case_Ignorable.
        // NB: We need to skip over all Case_Ignorable characters, even when
        // they also have the Cased binary property.
        if (u_hasBinaryProperty(codePoint, UCHAR_CASE_IGNORABLE))
            continue;

        followedByCased = u_hasBinaryProperty(codePoint, UCHAR_CASED);
        break;
    }
    if (!followedByCased)
        return unicode::GREEK_SMALL_LETTER_FINAL_SIGMA;
#endif

    return unicode::GREEK_SMALL_LETTER_SIGMA;
}

static Latin1Char
Final_Sigma(const Latin1Char* chars, size_t length, size_t index)
{
    MOZ_ASSERT_UNREACHABLE("U+03A3 is not a Latin-1 character");
    return 0;
}

// If |srcLength == destLength| is true, the destination buffer was allocated
// with the same size as the source buffer. When we append characters which
// have special casing mappings, we test |srcLength == destLength| to decide
// if we need to back out and reallocate a sufficiently large destination
// buffer. Otherwise the destination buffer was allocated with the correct
// size to hold all lower case mapped characters, i.e.
// |destLength == ToLowerCaseLength(srcChars, 0, srcLength)| is true.
template <typename CharT>
static size_t
ToLowerCaseImpl(CharT* destChars, const CharT* srcChars, size_t startIndex, size_t srcLength,
                size_t destLength)
{
    MOZ_ASSERT(startIndex < srcLength);
    MOZ_ASSERT(srcLength <= destLength);
    MOZ_ASSERT_IF((IsSame<CharT, Latin1Char>::value), srcLength == destLength);

    size_t j = startIndex;
    for (size_t i = startIndex; i < srcLength; i++) {
        char16_t c = srcChars[i];
        if (!IsSame<CharT, Latin1Char>::value) {
            if (unicode::IsLeadSurrogate(c) && i + 1 < srcLength) {
                char16_t trail = srcChars[i + 1];
                if (unicode::IsTrailSurrogate(trail)) {
                    trail = unicode::ToLowerCaseNonBMPTrail(c, trail);
                    destChars[j++] = c;
                    destChars[j++] = trail;
                    i++;
                    continue;
                }
            }

            // Special case: U+0130 LATIN CAPITAL LETTER I WITH DOT ABOVE
            // lowercases to <U+0069 U+0307>.
            if (c == unicode::LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE) {
                // Return if the output buffer is too small.
                if (srcLength == destLength)
                    return i;

                destChars[j++] = CharT('i');
                destChars[j++] = CharT(unicode::COMBINING_DOT_ABOVE);
                continue;
            }

            // Special case: U+03A3 GREEK CAPITAL LETTER SIGMA lowercases to
            // one of two codepoints depending on context.
            if (c == unicode::GREEK_CAPITAL_LETTER_SIGMA) {
                destChars[j++] = Final_Sigma(srcChars, srcLength, i);
                continue;
            }
        }

        c = unicode::ToLowerCase(c);
        MOZ_ASSERT_IF((IsSame<CharT, Latin1Char>::value), c <= JSString::MAX_LATIN1_CHAR);
        destChars[j++] = c;
    }

    MOZ_ASSERT(j == destLength);
    return srcLength;
}

static size_t
ToLowerCaseLength(const char16_t* chars, size_t startIndex, size_t length)
{
    size_t lowerLength = length;
    for (size_t i = startIndex; i < length; i++) {
        char16_t c = chars[i];

        // U+0130 is lowercased to the two-element sequence <U+0069 U+0307>.
        if (c == unicode::LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE)
            lowerLength += 1;
    }
    return lowerLength;
}

static size_t
ToLowerCaseLength(const Latin1Char* chars, size_t startIndex, size_t length)
{
    MOZ_ASSERT_UNREACHABLE("never called for Latin-1 strings");
    return 0;
}

template <typename CharT>
static JSString*
ToLowerCase(JSContext* cx, JSLinearString* str)
{
    // Unlike toUpperCase, toLowerCase has the nice invariant that if the
    // input is a Latin-1 string, the output is also a Latin-1 string.

    InlineCharBuffer<CharT> newChars;

    const size_t length = str->length();
    size_t resultLength;
    {
        AutoCheckCannotGC nogc;
        const CharT* chars = str->chars<CharT>(nogc);

        // We don't need extra special casing checks in the loop below,
        // because U+0130 LATIN CAPITAL LETTER I WITH DOT ABOVE and U+03A3
        // GREEK CAPITAL LETTER SIGMA already have simple lower case mappings.
        MOZ_ASSERT(unicode::CanLowerCase(unicode::LATIN_CAPITAL_LETTER_I_WITH_DOT_ABOVE),
                   "U+0130 has a simple lower case mapping");
        MOZ_ASSERT(unicode::CanLowerCase(unicode::GREEK_CAPITAL_LETTER_SIGMA),
                   "U+03A3 has a simple lower case mapping");

        // One element Latin-1 strings can be directly retrieved from the
        // static strings cache.
        if (IsSame<CharT, Latin1Char>::value) {
            if (length == 1) {
                char16_t lower = unicode::ToLowerCase(chars[0]);
                MOZ_ASSERT(lower <= JSString::MAX_LATIN1_CHAR);
                MOZ_ASSERT(StaticStrings::hasUnit(lower));

                return cx->staticStrings().getUnit(lower);
            }
        }

        // Look for the first character that changes when lowercased.
        size_t i = 0;
        for (; i < length; i++) {
            char16_t c = chars[i];
            if (!IsSame<CharT, Latin1Char>::value) {
                if (unicode::IsLeadSurrogate(c) && i + 1 < length) {
                    char16_t trail = chars[i + 1];
                    if (unicode::IsTrailSurrogate(trail)) {
                        if (unicode::CanLowerCaseNonBMP(c, trail))
                            break;

                        i++;
                        continue;
                    }
                }
            }
            if (unicode::CanLowerCase(c))
                break;
        }

        // If no character needs to change, return the input string.
        if (i == length)
            return str;

        resultLength = length;
        if (!newChars.maybeAlloc(cx, resultLength))
            return nullptr;

        PodCopy(newChars.get(), chars, i);

        size_t readChars = ToLowerCaseImpl(newChars.get(), chars, i, length, resultLength);
        if (readChars < length) {
            MOZ_ASSERT((!IsSame<CharT, Latin1Char>::value),
                       "Latin-1 strings don't have special lower case mappings");
            resultLength = ToLowerCaseLength(chars, readChars, length);

            if (!newChars.maybeRealloc(cx, length, resultLength))
                return nullptr;

            MOZ_ALWAYS_TRUE(length ==
                ToLowerCaseImpl(newChars.get(), chars, readChars, length, resultLength));
        }
    }

    return newChars.toString(cx, resultLength);
}

JSString*
js::StringToLowerCase(JSContext* cx, HandleLinearString string)
{
    if (string->hasLatin1Chars())
        return ToLowerCase<Latin1Char>(cx, string);
    return ToLowerCase<char16_t>(cx, string);
}

bool
js::str_toLowerCase(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    RootedLinearString linear(cx, str->ensureLinear(cx));
    if (!linear)
        return false;

    JSString* result = StringToLowerCase(cx, linear);
    if (!result)
        return false;

    args.rval().setString(result);
    return true;
}

#if !EXPOSE_INTL_API
bool
js::str_toLocaleLowerCase(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    /*
     * Forcefully ignore the first (or any) argument and return toLowerCase(),
     * ECMA has reserved that argument, presumably for defining the locale.
     */
    if (cx->runtime()->localeCallbacks && cx->runtime()->localeCallbacks->localeToLowerCase) {
        RootedValue result(cx);
        if (!cx->runtime()->localeCallbacks->localeToLowerCase(cx, str, &result))
            return false;

        args.rval().set(result);
        return true;
    }

    RootedLinearString linear(cx, str->ensureLinear(cx));
    if (!linear)
        return false;

    JSString* result = StringToLowerCase(cx, linear);
    if (!result)
        return false;

    args.rval().setString(result);
    return true;
}
#endif /* !EXPOSE_INTL_API */

static inline bool
CanUpperCaseSpecialCasing(Latin1Char charCode)
{
    // Handle U+00DF LATIN SMALL LETTER SHARP S inline, all other Latin-1
    // characters don't have special casing rules.
    MOZ_ASSERT_IF(charCode != unicode::LATIN_SMALL_LETTER_SHARP_S,
                  !unicode::CanUpperCaseSpecialCasing(charCode));

    return charCode == unicode::LATIN_SMALL_LETTER_SHARP_S;
}

static inline bool
CanUpperCaseSpecialCasing(char16_t charCode)
{
    return unicode::CanUpperCaseSpecialCasing(charCode);
}

static inline size_t
LengthUpperCaseSpecialCasing(Latin1Char charCode)
{
    // U+00DF LATIN SMALL LETTER SHARP S is uppercased to two 'S'.
    MOZ_ASSERT(charCode == unicode::LATIN_SMALL_LETTER_SHARP_S);

    return 2;
}

static inline size_t
LengthUpperCaseSpecialCasing(char16_t charCode)
{
    MOZ_ASSERT(CanUpperCaseSpecialCasing(charCode));

    return unicode::LengthUpperCaseSpecialCasing(charCode);
}

static inline void
AppendUpperCaseSpecialCasing(char16_t charCode, Latin1Char* elements, size_t* index)
{
    // U+00DF LATIN SMALL LETTER SHARP S is uppercased to two 'S'.
    MOZ_ASSERT(charCode == unicode::LATIN_SMALL_LETTER_SHARP_S);
    static_assert('S' <= JSString::MAX_LATIN1_CHAR, "'S' is a Latin-1 character");

    elements[(*index)++] = 'S';
    elements[(*index)++] = 'S';
}

static inline void
AppendUpperCaseSpecialCasing(char16_t charCode, char16_t* elements, size_t* index)
{
    unicode::AppendUpperCaseSpecialCasing(charCode, elements, index);
}

// See ToLowerCaseImpl for an explanation of the parameters.
template <typename DestChar, typename SrcChar>
static size_t
ToUpperCaseImpl(DestChar* destChars, const SrcChar* srcChars, size_t startIndex, size_t srcLength,
                size_t destLength)
{
    static_assert(IsSame<SrcChar, Latin1Char>::value || !IsSame<DestChar, Latin1Char>::value,
                  "cannot write non-Latin-1 characters into Latin-1 string");
    MOZ_ASSERT(startIndex < srcLength);
    MOZ_ASSERT(srcLength <= destLength);

    size_t j = startIndex;
    for (size_t i = startIndex; i < srcLength; i++) {
        char16_t c = srcChars[i];
        if (!IsSame<DestChar, Latin1Char>::value) {
            if (unicode::IsLeadSurrogate(c) && i + 1 < srcLength) {
                char16_t trail = srcChars[i + 1];
                if (unicode::IsTrailSurrogate(trail)) {
                    trail = unicode::ToUpperCaseNonBMPTrail(c, trail);
                    destChars[j++] = c;
                    destChars[j++] = trail;
                    i++;
                    continue;
                }
            }
        }

        if (MOZ_UNLIKELY(c > 0x7f && CanUpperCaseSpecialCasing(static_cast<SrcChar>(c)))) {
            // Return if the output buffer is too small.
            if (srcLength == destLength)
                return i;

            AppendUpperCaseSpecialCasing(c, destChars, &j);
            continue;
        }

        c = unicode::ToUpperCase(c);
        MOZ_ASSERT_IF((IsSame<DestChar, Latin1Char>::value), c <= JSString::MAX_LATIN1_CHAR);
        destChars[j++] = c;
    }

    MOZ_ASSERT(j == destLength);
    return srcLength;
}

// Explicit instantiation so we don't hit the static_assert from above.
static bool
ToUpperCaseImpl(Latin1Char* destChars, const char16_t* srcChars, size_t startIndex,
                size_t srcLength, size_t destLength)
{
    MOZ_ASSERT_UNREACHABLE("cannot write non-Latin-1 characters into Latin-1 string");
    return false;
}

template <typename CharT>
static size_t
ToUpperCaseLength(const CharT* chars, size_t startIndex, size_t length)
{
    size_t upperLength = length;
    for (size_t i = startIndex; i < length; i++) {
        char16_t c = chars[i];

        if (c > 0x7f && CanUpperCaseSpecialCasing(static_cast<CharT>(c)))
            upperLength += LengthUpperCaseSpecialCasing(static_cast<CharT>(c)) - 1;
    }
    return upperLength;
}

template <typename DestChar, typename SrcChar>
static inline void
CopyChars(DestChar* destChars, const SrcChar* srcChars, size_t length)
{
    static_assert(!IsSame<DestChar, SrcChar>::value, "PodCopy is used for the same type case");
    for (size_t i = 0; i < length; i++)
        destChars[i] = srcChars[i];
}

template <typename CharT>
static inline void
CopyChars(CharT* destChars, const CharT* srcChars, size_t length)
{
    PodCopy(destChars, srcChars, length);
}

template <typename DestChar, typename SrcChar>
static inline bool
ToUpperCase(JSContext* cx, InlineCharBuffer<DestChar>& newChars, const SrcChar* chars,
            size_t startIndex, size_t length, size_t* resultLength)
{
    MOZ_ASSERT(startIndex < length);

    *resultLength = length;
    if (!newChars.maybeAlloc(cx, length))
        return false;

    CopyChars(newChars.get(), chars, startIndex);

    size_t readChars = ToUpperCaseImpl(newChars.get(), chars, startIndex, length, length);
    if (readChars < length) {
        size_t actualLength = ToUpperCaseLength(chars, readChars, length);

        *resultLength = actualLength;
        if (!newChars.maybeRealloc(cx, length, actualLength))
            return false;

        MOZ_ALWAYS_TRUE(length ==
            ToUpperCaseImpl(newChars.get(), chars, readChars, length, actualLength));
    }

    return true;
}

template <typename CharT>
static JSString*
ToUpperCase(JSContext* cx, JSLinearString* str)
{
    using Latin1Buffer = InlineCharBuffer<Latin1Char>;
    using TwoByteBuffer = InlineCharBuffer<char16_t>;

    mozilla::MaybeOneOf<Latin1Buffer, TwoByteBuffer> newChars;
    const size_t length = str->length();
    size_t resultLength;
    {
        AutoCheckCannotGC nogc;
        const CharT* chars = str->chars<CharT>(nogc);

        // Most one element Latin-1 strings can be directly retrieved from the
        // static strings cache.
        if (IsSame<CharT, Latin1Char>::value) {
            if (length == 1) {
                Latin1Char c = chars[0];
                if (c != unicode::MICRO_SIGN &&
                    c != unicode::LATIN_SMALL_LETTER_Y_WITH_DIAERESIS &&
                    c != unicode::LATIN_SMALL_LETTER_SHARP_S)
                {
                    char16_t upper = unicode::ToUpperCase(c);
                    MOZ_ASSERT(upper <= JSString::MAX_LATIN1_CHAR);
                    MOZ_ASSERT(StaticStrings::hasUnit(upper));

                    return cx->staticStrings().getUnit(upper);
                }

                MOZ_ASSERT(unicode::ToUpperCase(c) > JSString::MAX_LATIN1_CHAR ||
                           CanUpperCaseSpecialCasing(c));
            }
        }

        // Look for the first character that changes when uppercased.
        size_t i = 0;
        for (; i < length; i++) {
            char16_t c = chars[i];
            if (!IsSame<CharT, Latin1Char>::value) {
                if (unicode::IsLeadSurrogate(c) && i + 1 < length) {
                    char16_t trail = chars[i + 1];
                    if (unicode::IsTrailSurrogate(trail)) {
                        if (unicode::CanUpperCaseNonBMP(c, trail))
                            break;

                        i++;
                        continue;
                    }
                }
            }
            if (unicode::CanUpperCase(c))
                break;
            if (MOZ_UNLIKELY(c > 0x7f && CanUpperCaseSpecialCasing(static_cast<CharT>(c))))
                break;
        }

        // If no character needs to change, return the input string.
        if (i == length)
            return str;

        // The string changes when uppercased, so we must create a new string.
        // Can it be Latin-1?
        //
        // If the original string is Latin-1, it can -- unless the string
        // contains U+00B5 MICRO SIGN or U+00FF SMALL LETTER Y WITH DIAERESIS,
        // the only Latin-1 codepoints that don't uppercase within Latin-1.
        // Search for those codepoints to decide whether the new string can be
        // Latin-1.
        // If the original string is a two-byte string, its uppercase form is
        // so rarely Latin-1 that we don't even consider creating a new
        // Latin-1 string.
        bool resultIsLatin1;
        if (IsSame<CharT, Latin1Char>::value) {
            resultIsLatin1 = true;
            for (size_t j = i; j < length; j++) {
                Latin1Char c = chars[j];
                if (c == unicode::MICRO_SIGN ||
                    c == unicode::LATIN_SMALL_LETTER_Y_WITH_DIAERESIS)
                {
                    MOZ_ASSERT(unicode::ToUpperCase(c) > JSString::MAX_LATIN1_CHAR);
                    resultIsLatin1 = false;
                    break;
                } else {
                    MOZ_ASSERT(unicode::ToUpperCase(c) <= JSString::MAX_LATIN1_CHAR);
                }
            }
        } else {
            resultIsLatin1 = false;
        }

        if (resultIsLatin1) {
            newChars.construct<Latin1Buffer>();

            if (!ToUpperCase(cx, newChars.ref<Latin1Buffer>(), chars, i, length, &resultLength))
                return nullptr;
        } else {
            newChars.construct<TwoByteBuffer>();

            if (!ToUpperCase(cx, newChars.ref<TwoByteBuffer>(), chars, i, length, &resultLength))
                return nullptr;
        }
    }

    return newChars.constructed<Latin1Buffer>()
           ? newChars.ref<Latin1Buffer>().toString(cx, resultLength)
           : newChars.ref<TwoByteBuffer>().toString(cx, resultLength);
}

JSString*
js::StringToUpperCase(JSContext* cx, HandleLinearString string)
{
    if (string->hasLatin1Chars())
        return ToUpperCase<Latin1Char>(cx, string);
    return ToUpperCase<char16_t>(cx, string);
}

bool
js::str_toUpperCase(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    RootedLinearString linear(cx, str->ensureLinear(cx));
    if (!linear)
        return false;

    JSString* result = StringToUpperCase(cx, linear);
    if (!result)
        return false;

    args.rval().setString(result);
    return true;
}

#if !EXPOSE_INTL_API
bool
js::str_toLocaleUpperCase(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    /*
     * Forcefully ignore the first (or any) argument and return toUpperCase(),
     * ECMA has reserved that argument, presumably for defining the locale.
     */
    if (cx->runtime()->localeCallbacks && cx->runtime()->localeCallbacks->localeToUpperCase) {
        RootedValue result(cx);
        if (!cx->runtime()->localeCallbacks->localeToUpperCase(cx, str, &result))
            return false;

        args.rval().set(result);
        return true;
    }

    RootedLinearString linear(cx, str->ensureLinear(cx));
    if (!linear)
        return false;

    JSString* result = StringToUpperCase(cx, linear);
    if (!result)
        return false;

    args.rval().setString(result);
    return true;
}
#endif /* !EXPOSE_INTL_API */

#if !EXPOSE_INTL_API
bool
js::str_localeCompare(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    RootedString thatStr(cx, ToString<CanGC>(cx, args.get(0)));
    if (!thatStr)
        return false;

    if (cx->runtime()->localeCallbacks && cx->runtime()->localeCallbacks->localeCompare) {
        RootedValue result(cx);
        if (!cx->runtime()->localeCallbacks->localeCompare(cx, str, thatStr, &result))
            return false;

        args.rval().set(result);
        return true;
    }

    int32_t result;
    if (!CompareStrings(cx, str, thatStr, &result))
        return false;

    args.rval().setInt32(result);
    return true;
}
#endif

#if EXPOSE_INTL_API
// ES2017 draft rev 45e890512fd77add72cc0ee742785f9f6f6482de
// 21.1.3.12 String.prototype.normalize ( [ form ] )
bool
js::str_normalize(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Steps 1-2.
    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    enum NormalizationForm {
        NFC, NFD, NFKC, NFKD
    };

    NormalizationForm form;
    if (!args.hasDefined(0)) {
        // Step 3.
        form = NFC;
    } else {
        // Step 4.
        RootedLinearString formStr(cx, ArgToRootedString(cx, args, 0));
        if (!formStr)
            return false;

        // Step 5.
        if (EqualStrings(formStr, cx->names().NFC)) {
            form = NFC;
        } else if (EqualStrings(formStr, cx->names().NFD)) {
            form = NFD;
        } else if (EqualStrings(formStr, cx->names().NFKC)) {
            form = NFKC;
        } else if (EqualStrings(formStr, cx->names().NFKD)) {
            form = NFKD;
        } else {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INVALID_NORMALIZE_FORM);
            return false;
        }
    }

    // Latin-1 strings are already in Normalization Form C.
    if (form == NFC && str->hasLatin1Chars()) {
        // Step 7.
        args.rval().setString(str);
        return true;
    }

    // Step 6.
    AutoStableStringChars stableChars(cx);
    if (!stableChars.initTwoByte(cx, str))
        return false;

    mozilla::Range<const char16_t> srcChars = stableChars.twoByteRange();

    // The unorm2_getXXXInstance() methods return a shared instance which must
    // not be deleted.
    UErrorCode status = U_ZERO_ERROR;
    const UNormalizer2* normalizer;
    if (form == NFC) {
        normalizer = unorm2_getNFCInstance(&status);
    } else if (form == NFD) {
        normalizer = unorm2_getNFDInstance(&status);
    } else if (form == NFKC) {
        normalizer = unorm2_getNFKCInstance(&status);
    } else {
        MOZ_ASSERT(form == NFKD);
        normalizer = unorm2_getNFKDInstance(&status);
    }
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    int32_t spanLength = unorm2_spanQuickCheckYes(normalizer,
                                                  srcChars.begin().get(), srcChars.length(),
                                                  &status);
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }
    MOZ_ASSERT(0 <= spanLength && size_t(spanLength) <= srcChars.length());

    // Return if the input string is already normalized.
    if (size_t(spanLength) == srcChars.length()) {
        // Step 7.
        args.rval().setString(str);
        return true;
    }

    static const size_t INLINE_CAPACITY = 32;

    Vector<char16_t, INLINE_CAPACITY> chars(cx);
    if (!chars.resize(Max(INLINE_CAPACITY, srcChars.length())))
        return false;

    // Copy the already normalized prefix.
    if (spanLength > 0)
        PodCopy(chars.begin(), srcChars.begin().get(), size_t(spanLength));

    mozilla::RangedPtr<const char16_t> remainingStart = srcChars.begin() + spanLength;
    size_t remainingLength = srcChars.length() - size_t(spanLength);

    int32_t size = unorm2_normalizeSecondAndAppend(normalizer,
                                                   chars.begin(), spanLength, chars.length(),
                                                   remainingStart.get(), remainingLength, &status);
    if (status == U_BUFFER_OVERFLOW_ERROR) {
        MOZ_ASSERT(size >= 0);
        if (!chars.resize(size))
            return false;
        status = U_ZERO_ERROR;
#ifdef DEBUG
        int32_t finalSize =
#endif
        unorm2_normalizeSecondAndAppend(normalizer,
                                        chars.begin(), spanLength, chars.length(),
                                        remainingStart.get(), remainingLength, &status);
        MOZ_ASSERT_IF(!U_FAILURE(status), size == finalSize);
    }
    if (U_FAILURE(status)) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INTERNAL_INTL_ERROR);
        return false;
    }

    MOZ_ASSERT(size >= 0);
    JSString* ns = NewStringCopyN<CanGC>(cx, chars.begin(), size);
    if (!ns)
        return false;

    // Step 7.
    args.rval().setString(ns);
    return true;
}
#endif

bool
js::str_charAt(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedString str(cx);
    size_t i;
    if (args.thisv().isString() && args.length() != 0 && args[0].isInt32()) {
        str = args.thisv().toString();
        i = size_t(args[0].toInt32());
        if (i >= str->length())
            goto out_of_range;
    } else {
        str = ToStringForStringFunction(cx, args.thisv());
        if (!str)
            return false;

        double d = 0.0;
        if (args.length() > 0 && !ToInteger(cx, args[0], &d))
            return false;

        if (d < 0 || str->length() <= d)
            goto out_of_range;
        i = size_t(d);
    }

    str = cx->staticStrings().getUnitStringForElement(cx, str, i);
    if (!str)
        return false;
    args.rval().setString(str);
    return true;

  out_of_range:
    args.rval().setString(cx->runtime()->emptyString);
    return true;
}

bool
js::str_charCodeAt_impl(JSContext* cx, HandleString string, HandleValue index, MutableHandleValue res)
{
    RootedString str(cx);
    size_t i;
    if (index.isInt32()) {
        i = index.toInt32();
        if (i >= string->length())
            goto out_of_range;
    } else {
        double d = 0.0;
        if (!ToInteger(cx, index, &d))
            return false;
        // check whether d is negative as size_t is unsigned
        if (d < 0 || string->length() <= d )
            goto out_of_range;
        i = size_t(d);
    }
    char16_t c;
    if (!string->getChar(cx, i , &c))
        return false;
    res.setInt32(c);
    return true;

out_of_range:
    res.setNaN();
    return true;
}

bool
js::str_charCodeAt(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedString str(cx);
    RootedValue index(cx);
    if (args.thisv().isString()) {
        str = args.thisv().toString();
    } else {
        str = ToStringForStringFunction(cx, args.thisv());
        if (!str)
            return false;
    }
    if (args.length() != 0)
        index = args[0];
    else
        index.setInt32(0);

    return js::str_charCodeAt_impl(cx, str, index, args.rval());
}

/*
 * Boyer-Moore-Horspool superlinear search for pat:patlen in text:textlen.
 * The patlen argument must be positive and no greater than sBMHPatLenMax.
 *
 * Return the index of pat in text, or -1 if not found.
 */
static const uint32_t sBMHCharSetSize = 256; /* ISO-Latin-1 */
static const uint32_t sBMHPatLenMax   = 255; /* skip table element is uint8_t */
static const int      sBMHBadPattern  = -2;  /* return value if pat is not ISO-Latin-1 */

template <typename TextChar, typename PatChar>
static int
BoyerMooreHorspool(const TextChar* text, uint32_t textLen, const PatChar* pat, uint32_t patLen)
{
    MOZ_ASSERT(0 < patLen && patLen <= sBMHPatLenMax);

    uint8_t skip[sBMHCharSetSize];
    for (uint32_t i = 0; i < sBMHCharSetSize; i++)
        skip[i] = uint8_t(patLen);

    uint32_t patLast = patLen - 1;
    for (uint32_t i = 0; i < patLast; i++) {
        char16_t c = pat[i];
        if (c >= sBMHCharSetSize)
            return sBMHBadPattern;
        skip[c] = uint8_t(patLast - i);
    }

    for (uint32_t k = patLast; k < textLen; ) {
        for (uint32_t i = k, j = patLast; ; i--, j--) {
            if (text[i] != pat[j])
                break;
            if (j == 0)
                return static_cast<int>(i);  /* safe: max string size */
        }

        char16_t c = text[k];
        k += (c >= sBMHCharSetSize) ? patLen : skip[c];
    }
    return -1;
}

template <typename TextChar, typename PatChar>
struct MemCmp {
    typedef uint32_t Extent;
    static MOZ_ALWAYS_INLINE Extent computeExtent(const PatChar*, uint32_t patLen) {
        return (patLen - 1) * sizeof(PatChar);
    }
    static MOZ_ALWAYS_INLINE bool match(const PatChar* p, const TextChar* t, Extent extent) {
        MOZ_ASSERT(sizeof(TextChar) == sizeof(PatChar));
        return memcmp(p, t, extent) == 0;
    }
};

template <typename TextChar, typename PatChar>
struct ManualCmp {
    typedef const PatChar* Extent;
    static MOZ_ALWAYS_INLINE Extent computeExtent(const PatChar* pat, uint32_t patLen) {
        return pat + patLen;
    }
    static MOZ_ALWAYS_INLINE bool match(const PatChar* p, const TextChar* t, Extent extent) {
        for (; p != extent; ++p, ++t) {
            if (*p != *t)
                return false;
        }
        return true;
    }
};

template <typename TextChar, typename PatChar>
static const TextChar*
FirstCharMatcherUnrolled(const TextChar* text, uint32_t n, const PatChar pat)
{
    const TextChar* textend = text + n;
    const TextChar* t = text;

    switch ((textend - t) & 7) {
        case 0: if (*t++ == pat) return t - 1; MOZ_FALLTHROUGH;
        case 7: if (*t++ == pat) return t - 1; MOZ_FALLTHROUGH;
        case 6: if (*t++ == pat) return t - 1; MOZ_FALLTHROUGH;
        case 5: if (*t++ == pat) return t - 1; MOZ_FALLTHROUGH;
        case 4: if (*t++ == pat) return t - 1; MOZ_FALLTHROUGH;
        case 3: if (*t++ == pat) return t - 1; MOZ_FALLTHROUGH;
        case 2: if (*t++ == pat) return t - 1; MOZ_FALLTHROUGH;
        case 1: if (*t++ == pat) return t - 1;
    }
    while (textend != t) {
        if (t[0] == pat) return t;
        if (t[1] == pat) return t + 1;
        if (t[2] == pat) return t + 2;
        if (t[3] == pat) return t + 3;
        if (t[4] == pat) return t + 4;
        if (t[5] == pat) return t + 5;
        if (t[6] == pat) return t + 6;
        if (t[7] == pat) return t + 7;
        t += 8;
    }
    return nullptr;
}

static const char*
FirstCharMatcher8bit(const char* text, uint32_t n, const char pat)
{
    return reinterpret_cast<const char*>(memchr(text, pat, n));
}

template <class InnerMatch, typename TextChar, typename PatChar>
static int
Matcher(const TextChar* text, uint32_t textlen, const PatChar* pat, uint32_t patlen)
{
    MOZ_ASSERT(patlen > 0);

    if (sizeof(TextChar) == 1 && sizeof(PatChar) > 1 && pat[0] > 0xff)
        return -1;

    const typename InnerMatch::Extent extent = InnerMatch::computeExtent(pat, patlen);

    uint32_t i = 0;
    uint32_t n = textlen - patlen + 1;
    while (i < n) {
        const TextChar* pos;

        if (sizeof(TextChar) == 1) {
            MOZ_ASSERT(pat[0] <= 0xff);
            pos = (TextChar*) FirstCharMatcher8bit((char*) text + i, n - i, pat[0]);
        } else {
            pos = FirstCharMatcherUnrolled(text + i, n - i, char16_t(pat[0]));
        }

        if (pos == nullptr)
            return -1;

        i = static_cast<uint32_t>(pos - text);
        if (InnerMatch::match(pat + 1, text + i + 1, extent))
            return i;

        i += 1;
     }
     return -1;
 }


template <typename TextChar, typename PatChar>
static MOZ_ALWAYS_INLINE int
StringMatch(const TextChar* text, uint32_t textLen, const PatChar* pat, uint32_t patLen)
{
    if (patLen == 0)
        return 0;
    if (textLen < patLen)
        return -1;

#if defined(__i386__) || defined(_M_IX86) || defined(__i386)
    /*
     * Given enough registers, the unrolled loop below is faster than the
     * following loop. 32-bit x86 does not have enough registers.
     */
    if (patLen == 1) {
        const PatChar p0 = *pat;
        const TextChar* end = text + textLen;
        for (const TextChar* c = text; c != end; ++c) {
            if (*c == p0)
                return c - text;
        }
        return -1;
    }
#endif

    /*
     * If the text or pattern string is short, BMH will be more expensive than
     * the basic linear scan due to initialization cost and a more complex loop
     * body. While the correct threshold is input-dependent, we can make a few
     * conservative observations:
     *  - When |textLen| is "big enough", the initialization time will be
     *    proportionally small, so the worst-case slowdown is minimized.
     *  - When |patLen| is "too small", even the best case for BMH will be
     *    slower than a simple scan for large |textLen| due to the more complex
     *    loop body of BMH.
     * From this, the values for "big enough" and "too small" are determined
     * empirically. See bug 526348.
     */
    if (textLen >= 512 && patLen >= 11 && patLen <= sBMHPatLenMax) {
        int index = BoyerMooreHorspool(text, textLen, pat, patLen);
        if (index != sBMHBadPattern)
            return index;
    }

    /*
     * For big patterns with large potential overlap we want the SIMD-optimized
     * speed of memcmp. For small patterns, a simple loop is faster. We also can't
     * use memcmp if one of the strings is TwoByte and the other is Latin-1.
     *
     * FIXME: Linux memcmp performance is sad and the manual loop is faster.
     */
    return
#if !defined(__linux__)
        (patLen > 128 && IsSame<TextChar, PatChar>::value)
            ? Matcher<MemCmp<TextChar, PatChar>, TextChar, PatChar>(text, textLen, pat, patLen)
            :
#endif
              Matcher<ManualCmp<TextChar, PatChar>, TextChar, PatChar>(text, textLen, pat, patLen);
}

static int32_t
StringMatch(JSLinearString* text, JSLinearString* pat, uint32_t start = 0)
{
    MOZ_ASSERT(start <= text->length());
    uint32_t textLen = text->length() - start;
    uint32_t patLen = pat->length();

    int match;
    AutoCheckCannotGC nogc;
    if (text->hasLatin1Chars()) {
        const Latin1Char* textChars = text->latin1Chars(nogc) + start;
        if (pat->hasLatin1Chars())
            match = StringMatch(textChars, textLen, pat->latin1Chars(nogc), patLen);
        else
            match = StringMatch(textChars, textLen, pat->twoByteChars(nogc), patLen);
    } else {
        const char16_t* textChars = text->twoByteChars(nogc) + start;
        if (pat->hasLatin1Chars())
            match = StringMatch(textChars, textLen, pat->latin1Chars(nogc), patLen);
        else
            match = StringMatch(textChars, textLen, pat->twoByteChars(nogc), patLen);
    }

    return (match == -1) ? -1 : start + match;
}

static const size_t sRopeMatchThresholdRatioLog2 = 4;

bool
js::StringHasPattern(JSLinearString* text, const char16_t* pat, uint32_t patLen)
{
    AutoCheckCannotGC nogc;
    return text->hasLatin1Chars()
           ? StringMatch(text->latin1Chars(nogc), text->length(), pat, patLen) != -1
           : StringMatch(text->twoByteChars(nogc), text->length(), pat, patLen) != -1;
}

int
js::StringFindPattern(JSLinearString* text, JSLinearString* pat, size_t start)
{
    return StringMatch(text, pat, start);
}

// When an algorithm does not need a string represented as a single linear
// array of characters, this range utility may be used to traverse the string a
// sequence of linear arrays of characters. This avoids flattening ropes.
class StringSegmentRange
{
    // If malloc() shows up in any profiles from this vector, we can add a new
    // StackAllocPolicy which stashes a reusable freed-at-gc buffer in the cx.
    using StackVector = JS::GCVector<JSString*, 16>;
    Rooted<StackVector> stack;
    RootedLinearString cur;

    bool settle(JSString* str) {
        while (str->isRope()) {
            JSRope& rope = str->asRope();
            if (!stack.append(rope.rightChild()))
                return false;
            str = rope.leftChild();
        }
        cur = &str->asLinear();
        return true;
    }

  public:
    explicit StringSegmentRange(JSContext* cx)
      : stack(cx, StackVector(cx)), cur(cx)
    {}

    MOZ_MUST_USE bool init(JSString* str) {
        MOZ_ASSERT(stack.empty());
        return settle(str);
    }

    bool empty() const {
        return cur == nullptr;
    }

    JSLinearString* front() const {
        MOZ_ASSERT(!cur->isRope());
        return cur;
    }

    MOZ_MUST_USE bool popFront() {
        MOZ_ASSERT(!empty());
        if (stack.empty()) {
            cur = nullptr;
            return true;
        }
        return settle(stack.popCopy());
    }
};

typedef Vector<JSLinearString*, 16, SystemAllocPolicy> LinearStringVector;

template <typename TextChar, typename PatChar>
static int
RopeMatchImpl(const AutoCheckCannotGC& nogc, LinearStringVector& strings,
              const PatChar* pat, size_t patLen)
{
    /* Absolute offset from the beginning of the logical text string. */
    int pos = 0;

    for (JSLinearString** outerp = strings.begin(); outerp != strings.end(); ++outerp) {
        /* Try to find a match within 'outer'. */
        JSLinearString* outer = *outerp;
        const TextChar* chars = outer->chars<TextChar>(nogc);
        size_t len = outer->length();
        int matchResult = StringMatch(chars, len, pat, patLen);
        if (matchResult != -1) {
            /* Matched! */
            return pos + matchResult;
        }

        /* Try to find a match starting in 'outer' and running into other nodes. */
        const TextChar* const text = chars + (patLen > len ? 0 : len - patLen + 1);
        const TextChar* const textend = chars + len;
        const PatChar p0 = *pat;
        const PatChar* const p1 = pat + 1;
        const PatChar* const patend = pat + patLen;
        for (const TextChar* t = text; t != textend; ) {
            if (*t++ != p0)
                continue;

            JSLinearString** innerp = outerp;
            const TextChar* ttend = textend;
            const TextChar* tt = t;
            for (const PatChar* pp = p1; pp != patend; ++pp, ++tt) {
                while (tt == ttend) {
                    if (++innerp == strings.end())
                        return -1;

                    JSLinearString* inner = *innerp;
                    tt = inner->chars<TextChar>(nogc);
                    ttend = tt + inner->length();
                }
                if (*pp != *tt)
                    goto break_continue;
            }

            /* Matched! */
            return pos + (t - chars) - 1;  /* -1 because of *t++ above */

          break_continue:;
        }

        pos += len;
    }

    return -1;
}

/*
 * RopeMatch takes the text to search and the pattern to search for in the text.
 * RopeMatch returns false on OOM and otherwise returns the match index through
 * the 'match' outparam (-1 for not found).
 */
static bool
RopeMatch(JSContext* cx, JSRope* text, JSLinearString* pat, int* match)
{
    uint32_t patLen = pat->length();
    if (patLen == 0) {
        *match = 0;
        return true;
    }
    if (text->length() < patLen) {
        *match = -1;
        return true;
    }

    /*
     * List of leaf nodes in the rope. If we run out of memory when trying to
     * append to this list, we can still fall back to StringMatch, so use the
     * system allocator so we don't report OOM in that case.
     */
    LinearStringVector strings;

    /*
     * We don't want to do rope matching if there is a poor node-to-char ratio,
     * since this means spending a lot of time in the match loop below. We also
     * need to build the list of leaf nodes. Do both here: iterate over the
     * nodes so long as there are not too many.
     *
     * We also don't use rope matching if the rope contains both Latin-1 and
     * TwoByte nodes, to simplify the match algorithm.
     */
    {
        size_t threshold = text->length() >> sRopeMatchThresholdRatioLog2;
        StringSegmentRange r(cx);
        if (!r.init(text))
            return false;

        bool textIsLatin1 = text->hasLatin1Chars();
        while (!r.empty()) {
            if (threshold-- == 0 ||
                r.front()->hasLatin1Chars() != textIsLatin1 ||
                !strings.append(r.front()))
            {
                JSLinearString* linear = text->ensureLinear(cx);
                if (!linear)
                    return false;

                *match = StringMatch(linear, pat);
                return true;
            }
            if (!r.popFront())
                return false;
        }
    }

    AutoCheckCannotGC nogc;
    if (text->hasLatin1Chars()) {
        if (pat->hasLatin1Chars())
            *match = RopeMatchImpl<Latin1Char>(nogc, strings, pat->latin1Chars(nogc), patLen);
        else
            *match = RopeMatchImpl<Latin1Char>(nogc, strings, pat->twoByteChars(nogc), patLen);
    } else {
        if (pat->hasLatin1Chars())
            *match = RopeMatchImpl<char16_t>(nogc, strings, pat->latin1Chars(nogc), patLen);
        else
            *match = RopeMatchImpl<char16_t>(nogc, strings, pat->twoByteChars(nogc), patLen);
    }

    return true;
}

/* ES6 draft rc4 21.1.3.7. */
bool
js::str_includes(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Steps 1, 2, and 3
    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    // Steps 4 and 5
    bool isRegExp;
    if (!IsRegExp(cx, args.get(0), &isRegExp))
        return false;

    // Step 6
    if (isRegExp) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INVALID_ARG_TYPE,
                                  "first", "", "Regular Expression");
        return false;
    }

    // Steps 7 and 8
    RootedLinearString searchStr(cx, ArgToRootedString(cx, args, 0));
    if (!searchStr)
        return false;

    // Steps 9 and 10
    uint32_t pos = 0;
    if (args.hasDefined(1)) {
        if (args[1].isInt32()) {
            int i = args[1].toInt32();
            pos = (i < 0) ? 0U : uint32_t(i);
        } else {
            double d;
            if (!ToInteger(cx, args[1], &d))
                return false;
            pos = uint32_t(Min(Max(d, 0.0), double(UINT32_MAX)));
        }
    }

    // Step 11
    uint32_t textLen = str->length();

    // Step 12
    uint32_t start = Min(Max(pos, 0U), textLen);

    // Steps 13 and 14
    JSLinearString* text = str->ensureLinear(cx);
    if (!text)
        return false;

    args.rval().setBoolean(StringMatch(text, searchStr, start) != -1);
    return true;
}

/* ES6 20120927 draft 15.5.4.7. */
bool
js::str_indexOf(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Steps 1, 2, and 3
    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    // Steps 4 and 5
    RootedLinearString searchStr(cx, ArgToRootedString(cx, args, 0));
    if (!searchStr)
        return false;

    // Steps 6 and 7
    uint32_t pos = 0;
    if (args.hasDefined(1)) {
        if (args[1].isInt32()) {
            int i = args[1].toInt32();
            pos = (i < 0) ? 0U : uint32_t(i);
        } else {
            double d;
            if (!ToInteger(cx, args[1], &d))
                return false;
            pos = uint32_t(Min(Max(d, 0.0), double(UINT32_MAX)));
        }
    }

   // Step 8
    uint32_t textLen = str->length();

    // Step 9
    uint32_t start = Min(Max(pos, 0U), textLen);

    // Steps 10 and 11
    JSLinearString* text = str->ensureLinear(cx);
    if (!text)
        return false;

    args.rval().setInt32(StringMatch(text, searchStr, start));
    return true;
}

template <typename TextChar, typename PatChar>
static int32_t
LastIndexOfImpl(const TextChar* text, size_t textLen, const PatChar* pat, size_t patLen,
                size_t start)
{
    MOZ_ASSERT(patLen > 0);
    MOZ_ASSERT(patLen <= textLen);
    MOZ_ASSERT(start <= textLen - patLen);

    const PatChar p0 = *pat;
    const PatChar* patNext = pat + 1;
    const PatChar* patEnd = pat + patLen;

    for (const TextChar* t = text + start; t >= text; --t) {
        if (*t == p0) {
            const TextChar* t1 = t + 1;
            for (const PatChar* p1 = patNext; p1 < patEnd; ++p1, ++t1) {
                if (*t1 != *p1)
                    goto break_continue;
            }

            return static_cast<int32_t>(t - text);
        }
      break_continue:;
    }

    return -1;
}

// ES2017 draft rev 6859bb9ccaea9c6ede81d71e5320e3833b92cb3e
// 21.1.3.9 String.prototype.lastIndexOf ( searchString [ , position ] )
bool
js::str_lastIndexOf(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Steps 1-2.
    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    // Step 3.
    RootedLinearString searchStr(cx, ArgToRootedString(cx, args, 0));
    if (!searchStr)
        return false;

    // Step 6.
    size_t len = str->length();

    // Step 8.
    size_t searchLen = searchStr->length();

    // Steps 4-5, 7.
    int start = len - searchLen; // Start searching here
    if (args.hasDefined(1)) {
        if (args[1].isInt32()) {
            int i = args[1].toInt32();
            if (i <= 0)
                start = 0;
            else if (i < start)
                start = i;
        } else {
            double d;
            if (!ToNumber(cx, args[1], &d))
                return false;
            if (!IsNaN(d)) {
                d = JS::ToInteger(d);
                if (d <= 0)
                    start = 0;
                else if (d < start)
                    start = int(d);
            }
        }
    }

    if (searchLen > len) {
        args.rval().setInt32(-1);
        return true;
    }

    if (searchLen == 0) {
        args.rval().setInt32(start);
        return true;
    }
    MOZ_ASSERT(0 <= start && size_t(start) < len);

    JSLinearString* text = str->ensureLinear(cx);
    if (!text)
        return false;

    // Step 9.
    int32_t res;
    AutoCheckCannotGC nogc;
    if (text->hasLatin1Chars()) {
        const Latin1Char* textChars = text->latin1Chars(nogc);
        if (searchStr->hasLatin1Chars())
            res = LastIndexOfImpl(textChars, len, searchStr->latin1Chars(nogc), searchLen, start);
        else
            res = LastIndexOfImpl(textChars, len, searchStr->twoByteChars(nogc), searchLen, start);
    } else {
        const char16_t* textChars = text->twoByteChars(nogc);
        if (searchStr->hasLatin1Chars())
            res = LastIndexOfImpl(textChars, len, searchStr->latin1Chars(nogc), searchLen, start);
        else
            res = LastIndexOfImpl(textChars, len, searchStr->twoByteChars(nogc), searchLen, start);
    }

    args.rval().setInt32(res);
    return true;
}

bool
js::HasSubstringAt(JSLinearString* text, JSLinearString* pat, size_t start)
{
    MOZ_ASSERT(start + pat->length() <= text->length());

    size_t patLen = pat->length();

    AutoCheckCannotGC nogc;
    if (text->hasLatin1Chars()) {
        const Latin1Char* textChars = text->latin1Chars(nogc) + start;
        if (pat->hasLatin1Chars())
            return PodEqual(textChars, pat->latin1Chars(nogc), patLen);

        return EqualChars(textChars, pat->twoByteChars(nogc), patLen);
    }

    const char16_t* textChars = text->twoByteChars(nogc) + start;
    if (pat->hasTwoByteChars())
        return PodEqual(textChars, pat->twoByteChars(nogc), patLen);

    return EqualChars(pat->latin1Chars(nogc), textChars, patLen);
}

/* ES6 draft rc3 21.1.3.18. */
bool
js::str_startsWith(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Steps 1, 2, and 3
    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    // Steps 4 and 5
    bool isRegExp;
    if (!IsRegExp(cx, args.get(0), &isRegExp))
        return false;

    // Step 6
    if (isRegExp) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INVALID_ARG_TYPE,
                                  "first", "", "Regular Expression");
        return false;
    }

    // Steps 7 and 8
    RootedLinearString searchStr(cx, ArgToRootedString(cx, args, 0));
    if (!searchStr)
        return false;

    // Steps 9 and 10
    uint32_t pos = 0;
    if (args.hasDefined(1)) {
        if (args[1].isInt32()) {
            int i = args[1].toInt32();
            pos = (i < 0) ? 0U : uint32_t(i);
        } else {
            double d;
            if (!ToInteger(cx, args[1], &d))
                return false;
            pos = uint32_t(Min(Max(d, 0.0), double(UINT32_MAX)));
        }
    }

    // Step 11
    uint32_t textLen = str->length();

    // Step 12
    uint32_t start = Min(Max(pos, 0U), textLen);

    // Step 13
    uint32_t searchLen = searchStr->length();

    // Step 14
    if (searchLen + start < searchLen || searchLen + start > textLen) {
        args.rval().setBoolean(false);
        return true;
    }

    // Steps 15 and 16
    JSLinearString* text = str->ensureLinear(cx);
    if (!text)
        return false;

    args.rval().setBoolean(HasSubstringAt(text, searchStr, start));
    return true;
}

/* ES6 draft rc3 21.1.3.6. */
bool
js::str_endsWith(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Steps 1, 2, and 3
    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    // Steps 4 and 5
    bool isRegExp;
    if (!IsRegExp(cx, args.get(0), &isRegExp))
        return false;

    // Step 6
    if (isRegExp) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_INVALID_ARG_TYPE,
                                  "first", "", "Regular Expression");
        return false;
    }

    // Steps 7 and 8
    RootedLinearString searchStr(cx, ArgToRootedString(cx, args, 0));
    if (!searchStr)
        return false;

    // Step 9
    uint32_t textLen = str->length();

    // Steps 10 and 11
    uint32_t pos = textLen;
    if (args.hasDefined(1)) {
        if (args[1].isInt32()) {
            int i = args[1].toInt32();
            pos = (i < 0) ? 0U : uint32_t(i);
        } else {
            double d;
            if (!ToInteger(cx, args[1], &d))
                return false;
            pos = uint32_t(Min(Max(d, 0.0), double(UINT32_MAX)));
        }
    }

    // Step 12
    uint32_t end = Min(Max(pos, 0U), textLen);

    // Step 13
    uint32_t searchLen = searchStr->length();

    // Step 15 (reordered)
    if (searchLen > end) {
        args.rval().setBoolean(false);
        return true;
    }

    // Step 14
    uint32_t start = end - searchLen;

    // Steps 16 and 17
    JSLinearString* text = str->ensureLinear(cx);
    if (!text)
        return false;

    args.rval().setBoolean(HasSubstringAt(text, searchStr, start));
    return true;
}

template <typename CharT>
static void
TrimString(const CharT* chars, bool trimLeft, bool trimRight, size_t length,
           size_t* pBegin, size_t* pEnd)
{
    size_t begin = 0, end = length;

    if (trimLeft) {
        while (begin < length && unicode::IsSpace(chars[begin]))
            ++begin;
    }

    if (trimRight) {
        while (end > begin && unicode::IsSpace(chars[end - 1]))
            --end;
    }

    *pBegin = begin;
    *pEnd = end;
}

static bool
TrimString(JSContext* cx, const CallArgs& args, bool trimLeft, bool trimRight)
{
    RootedString str(cx, ToStringForStringFunction(cx, args.thisv()));
    if (!str)
        return false;

    JSLinearString* linear = str->ensureLinear(cx);
    if (!linear)
        return false;

    size_t length = linear->length();
    size_t begin, end;
    if (linear->hasLatin1Chars()) {
        AutoCheckCannotGC nogc;
        TrimString(linear->latin1Chars(nogc), trimLeft, trimRight, length, &begin, &end);
    } else {
        AutoCheckCannotGC nogc;
        TrimString(linear->twoByteChars(nogc), trimLeft, trimRight, length, &begin, &end);
    }

    str = NewDependentString(cx, str, begin, end - begin);
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

bool
js::str_trim(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return TrimString(cx, args, true, true);
}

bool
js::str_trimLeft(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return TrimString(cx, args, true, false);
}

bool
js::str_trimRight(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    return TrimString(cx, args, false, true);
}

// Utility for building a rope (lazy concatenation) of strings.
class RopeBuilder {
    JSContext* cx;
    RootedString res;

    RopeBuilder(const RopeBuilder& other) = delete;
    void operator=(const RopeBuilder& other) = delete;

  public:
    explicit RopeBuilder(JSContext* cx)
      : cx(cx), res(cx, cx->runtime()->emptyString)
    {}

    inline bool append(HandleString str) {
        res = ConcatStrings<CanGC>(cx, res, str);
        return !!res;
    }

    inline JSString* result() {
        return res;
    }
};

namespace {

template <typename CharT>
static uint32_t
FindDollarIndex(const CharT* chars, size_t length)
{
    if (const CharT* p = js_strchr_limit(chars, '$', chars + length)) {
        uint32_t dollarIndex = p - chars;
        MOZ_ASSERT(dollarIndex < length);
        return dollarIndex;
    }
    return UINT32_MAX;
}

} /* anonymous namespace */

static JSString*
BuildFlatReplacement(JSContext* cx, HandleString textstr, HandleString repstr,
                     size_t match, size_t patternLength)
{
    RopeBuilder builder(cx);
    size_t matchEnd = match + patternLength;

    if (textstr->isRope()) {
        /*
         * If we are replacing over a rope, avoid flattening it by iterating
         * through it, building a new rope.
         */
        StringSegmentRange r(cx);
        if (!r.init(textstr))
            return nullptr;

        size_t pos = 0;
        while (!r.empty()) {
            RootedString str(cx, r.front());
            size_t len = str->length();
            size_t strEnd = pos + len;
            if (pos < matchEnd && strEnd > match) {
                /*
                 * We need to special-case any part of the rope that overlaps
                 * with the replacement string.
                 */
                if (match >= pos) {
                    /*
                     * If this part of the rope overlaps with the left side of
                     * the pattern, then it must be the only one to overlap with
                     * the first character in the pattern, so we include the
                     * replacement string here.
                     */
                    RootedString leftSide(cx, NewDependentString(cx, str, 0, match - pos));
                    if (!leftSide ||
                        !builder.append(leftSide) ||
                        !builder.append(repstr))
                    {
                        return nullptr;
                    }
                }

                /*
                 * If str runs off the end of the matched string, append the
                 * last part of str.
                 */
                if (strEnd > matchEnd) {
                    RootedString rightSide(cx, NewDependentString(cx, str, matchEnd - pos,
                                                                  strEnd - matchEnd));
                    if (!rightSide || !builder.append(rightSide))
                        return nullptr;
                }
            } else {
                if (!builder.append(str))
                    return nullptr;
            }
            pos += str->length();
            if (!r.popFront())
                return nullptr;
        }
    } else {
        RootedString leftSide(cx, NewDependentString(cx, textstr, 0, match));
        if (!leftSide)
            return nullptr;
        RootedString rightSide(cx);
        rightSide = NewDependentString(cx, textstr, match + patternLength,
                                       textstr->length() - match - patternLength);
        if (!rightSide ||
            !builder.append(leftSide) ||
            !builder.append(repstr) ||
            !builder.append(rightSide))
        {
            return nullptr;
        }
    }

    return builder.result();
}

template <typename CharT>
static bool
AppendDollarReplacement(StringBuffer& newReplaceChars, size_t firstDollarIndex,
                        size_t matchStart, size_t matchLimit, JSLinearString* text,
                        const CharT* repChars, size_t repLength)
{
    MOZ_ASSERT(firstDollarIndex < repLength);

    /* Move the pre-dollar chunk in bulk. */
    newReplaceChars.infallibleAppend(repChars, firstDollarIndex);

    /* Move the rest char-by-char, interpreting dollars as we encounter them. */
    const CharT* repLimit = repChars + repLength;
    for (const CharT* it = repChars + firstDollarIndex; it < repLimit; ++it) {
        if (*it != '$' || it == repLimit - 1) {
            if (!newReplaceChars.append(*it))
                return false;
            continue;
        }

        switch (*(it + 1)) {
          case '$': /* Eat one of the dollars. */
            if (!newReplaceChars.append(*it))
                return false;
            break;
          case '&':
            if (!newReplaceChars.appendSubstring(text, matchStart, matchLimit - matchStart))
                return false;
            break;
          case '`':
            if (!newReplaceChars.appendSubstring(text, 0, matchStart))
                return false;
            break;
          case '\'':
            if (!newReplaceChars.appendSubstring(text, matchLimit, text->length() - matchLimit))
                return false;
            break;
          default: /* The dollar we saw was not special (no matter what its mother told it). */
            if (!newReplaceChars.append(*it))
                return false;
            continue;
        }
        ++it; /* We always eat an extra char in the above switch. */
    }

    return true;
}

/*
 * Perform a linear-scan dollar substitution on the replacement text,
 * constructing a result string that looks like:
 *
 *      newstring = string[:matchStart] + dollarSub(replaceValue) + string[matchLimit:]
 */
static JSString*
BuildDollarReplacement(JSContext* cx, JSString* textstrArg, JSLinearString* repstr,
                       uint32_t firstDollarIndex, size_t matchStart, size_t patternLength)
{
    RootedLinearString textstr(cx, textstrArg->ensureLinear(cx));
    if (!textstr)
        return nullptr;

    size_t matchLimit = matchStart + patternLength;

    /*
     * Most probably:
     *
     *      len(newstr) >= len(orig) - len(match) + len(replacement)
     *
     * Note that dollar vars _could_ make the resulting text smaller than this.
     */
    StringBuffer newReplaceChars(cx);
    if (repstr->hasTwoByteChars() && !newReplaceChars.ensureTwoByteChars())
        return nullptr;

    if (!newReplaceChars.reserve(textstr->length() - patternLength + repstr->length()))
        return nullptr;

    bool res;
    if (repstr->hasLatin1Chars()) {
        AutoCheckCannotGC nogc;
        res = AppendDollarReplacement(newReplaceChars, firstDollarIndex, matchStart, matchLimit,
                                      textstr, repstr->latin1Chars(nogc), repstr->length());
    } else {
        AutoCheckCannotGC nogc;
        res = AppendDollarReplacement(newReplaceChars, firstDollarIndex, matchStart, matchLimit,
                                      textstr, repstr->twoByteChars(nogc), repstr->length());
    }
    if (!res)
        return nullptr;

    RootedString leftSide(cx, NewDependentString(cx, textstr, 0, matchStart));
    if (!leftSide)
        return nullptr;

    RootedString newReplace(cx, newReplaceChars.finishString());
    if (!newReplace)
        return nullptr;

    MOZ_ASSERT(textstr->length() >= matchLimit);
    RootedString rightSide(cx, NewDependentString(cx, textstr, matchLimit,
                                                  textstr->length() - matchLimit));
    if (!rightSide)
        return nullptr;

    RopeBuilder builder(cx);
    if (!builder.append(leftSide) || !builder.append(newReplace) || !builder.append(rightSide))
        return nullptr;

    return builder.result();
}

template <typename StrChar, typename RepChar>
static bool
StrFlatReplaceGlobal(JSContext *cx, JSLinearString *str, JSLinearString *pat, JSLinearString *rep,
                     StringBuffer &sb)
{
    MOZ_ASSERT(str->length() > 0);

    AutoCheckCannotGC nogc;
    const StrChar *strChars = str->chars<StrChar>(nogc);
    const RepChar *repChars = rep->chars<RepChar>(nogc);

    // The pattern is empty, so we interleave the replacement string in-between
    // each character.
    if (!pat->length()) {
        CheckedInt<uint32_t> strLength(str->length());
        CheckedInt<uint32_t> repLength(rep->length());
        CheckedInt<uint32_t> length = repLength * (strLength - 1) + strLength;
        if (!length.isValid()) {
            ReportAllocationOverflow(cx);
            return false;
        }

        if (!sb.reserve(length.value()))
            return false;

        for (unsigned i = 0; i < str->length() - 1; ++i, ++strChars) {
            sb.infallibleAppend(*strChars);
            sb.infallibleAppend(repChars, rep->length());
        }
        sb.infallibleAppend(*strChars);
        return true;
    }

    // If it's true, we are sure that the result's length is, at least, the same
    // length as |str->length()|.
    if (rep->length() >= pat->length()) {
        if (!sb.reserve(str->length()))
            return false;
    }

    uint32_t start = 0;
    for (;;) {
        int match = StringMatch(str, pat, start);
        if (match < 0)
            break;
        if (!sb.append(strChars + start, match - start))
            return false;
        if (!sb.append(repChars, rep->length()))
            return false;
        start = match + pat->length();
    }

    if (!sb.append(strChars + start, str->length() - start))
        return false;

    return true;
}

// This is identical to "str.split(pattern).join(replacement)" except that we
// do some deforestation optimization in Ion.
JSString *
js::str_flat_replace_string(JSContext *cx, HandleString string, HandleString pattern,
                            HandleString replacement)
{
    MOZ_ASSERT(string);
    MOZ_ASSERT(pattern);
    MOZ_ASSERT(replacement);

    if (!string->length())
        return string;

    RootedLinearString linearRepl(cx, replacement->ensureLinear(cx));
    if (!linearRepl)
        return nullptr;

    RootedLinearString linearPat(cx, pattern->ensureLinear(cx));
    if (!linearPat)
        return nullptr;

    RootedLinearString linearStr(cx, string->ensureLinear(cx));
    if (!linearStr)
        return nullptr;

    StringBuffer sb(cx);
    if (linearStr->hasTwoByteChars()) {
        if (!sb.ensureTwoByteChars())
            return nullptr;
        if (linearRepl->hasTwoByteChars()) {
            if (!StrFlatReplaceGlobal<char16_t, char16_t>(cx, linearStr, linearPat, linearRepl, sb))
                return nullptr;
        } else {
            if (!StrFlatReplaceGlobal<char16_t, Latin1Char>(cx, linearStr, linearPat, linearRepl, sb))
                return nullptr;
        }
    } else {
        if (linearRepl->hasTwoByteChars()) {
            if (!sb.ensureTwoByteChars())
                return nullptr;
            if (!StrFlatReplaceGlobal<Latin1Char, char16_t>(cx, linearStr, linearPat, linearRepl, sb))
                return nullptr;
        } else {
            if (!StrFlatReplaceGlobal<Latin1Char, Latin1Char>(cx, linearStr, linearPat, linearRepl, sb))
                return nullptr;
        }
    }

    JSString *str = sb.finishString();
    if (!str)
        return nullptr;

    return str;
}

JSString*
js::str_replace_string_raw(JSContext* cx, HandleString string, HandleString pattern,
                           HandleString replacement)
{
    RootedLinearString repl(cx, replacement->ensureLinear(cx));
    if (!repl)
        return nullptr;

    RootedAtom pat(cx, AtomizeString(cx, pattern));
    if (!pat)
        return nullptr;

    size_t patternLength = pat->length();
    int32_t match;
    uint32_t dollarIndex;

    {
        AutoCheckCannotGC nogc;
        dollarIndex = repl->hasLatin1Chars()
                      ? FindDollarIndex(repl->latin1Chars(nogc), repl->length())
                      : FindDollarIndex(repl->twoByteChars(nogc), repl->length());
    }

    /*
     * |string| could be a rope, so we want to avoid flattening it for as
     * long as possible.
     */
    if (string->isRope()) {
        if (!RopeMatch(cx, &string->asRope(), pat, &match))
            return nullptr;
    } else {
        match = StringMatch(&string->asLinear(), pat, 0);
    }

    if (match < 0)
        return string;

    if (dollarIndex != UINT32_MAX)
        return BuildDollarReplacement(cx, string, repl, dollarIndex, match, patternLength);
    return BuildFlatReplacement(cx, string, repl, match, patternLength);
}

// ES 2016 draft Mar 25, 2016 21.1.3.17 steps 4, 8, 12-18.
static JSObject*
SplitHelper(JSContext* cx, HandleLinearString str, uint32_t limit, HandleLinearString sep,
            HandleObjectGroup group)
{
    size_t strLength = str->length();
    size_t sepLength = sep->length();
    MOZ_ASSERT(sepLength != 0);

    // Step 12.
    if (strLength == 0) {
        // Step 12.a.
        int match = StringMatch(str, sep, 0);

        // Step 12.b.
        if (match != -1)
            return NewFullyAllocatedArrayTryUseGroup(cx, group, 0);

        // Steps 12.c-e.
        RootedValue v(cx, StringValue(str));
        return NewCopiedArrayTryUseGroup(cx, group, v.address(), 1);
    }

    // Step 3 (reordered).
    AutoValueVector splits(cx);

    // Step 8 (reordered).
    size_t lastEndIndex = 0;

    // Step 13.
    size_t index = 0;

    // Step 14.
    while (index != strLength) {
        // Step 14.a.
        int match = StringMatch(str, sep, index);

        // Step 14.b.
        //
        // Our match algorithm differs from the spec in that it returns the
        // next index at which a match happens.  If no match happens we're
        // done.
        //
        // But what if the match is at the end of the string (and the string is
        // not empty)?  Per 14.c.i this shouldn't be a match, so we have to
        // specially exclude it.  Thus this case should hold:
        //
        //   var a = "abc".split(/\b/);
        //   assertEq(a.length, 1);
        //   assertEq(a[0], "abc");
        if (match == -1)
            break;

        // Step 14.c.
        size_t endIndex = match + sepLength;

        // Step 14.c.i.
        if (endIndex == lastEndIndex) {
            index++;
            continue;
        }

        // Step 14.c.ii.
        MOZ_ASSERT(lastEndIndex < endIndex);
        MOZ_ASSERT(sepLength <= strLength);
        MOZ_ASSERT(lastEndIndex + sepLength <= endIndex);

        // Step 14.c.ii.1.
        size_t subLength = size_t(endIndex - sepLength - lastEndIndex);
        JSString* sub = NewDependentString(cx, str, lastEndIndex, subLength);

        // Steps 14.c.ii.2-4.
        if (!sub || !splits.append(StringValue(sub)))
            return nullptr;

        // Step 14.c.ii.5.
        if (splits.length() == limit)
            return NewCopiedArrayTryUseGroup(cx, group, splits.begin(), splits.length());

        // Step 14.c.ii.6.
        index = endIndex;

        // Step 14.c.ii.7.
        lastEndIndex = index;
    }

    // Step 15.
    JSString* sub = NewDependentString(cx, str, lastEndIndex, strLength - lastEndIndex);

    // Steps 16-17.
    if (!sub || !splits.append(StringValue(sub)))
        return nullptr;

    // Step 18.
    return NewCopiedArrayTryUseGroup(cx, group, splits.begin(), splits.length());
}

// Fast-path for splitting a string into a character array via split("").
static JSObject*
CharSplitHelper(JSContext* cx, HandleLinearString str, uint32_t limit, HandleObjectGroup group)
{
    size_t strLength = str->length();
    if (strLength == 0)
        return NewFullyAllocatedArrayTryUseGroup(cx, group, 0);

    js::StaticStrings& staticStrings = cx->staticStrings();
    uint32_t resultlen = (limit < strLength ? limit : strLength);

    AutoValueVector splits(cx);
    if (!splits.reserve(resultlen))
        return nullptr;

    for (size_t i = 0; i < resultlen; ++i) {
        JSString* sub = staticStrings.getUnitStringForElement(cx, str, i);
        if (!sub)
            return nullptr;
        splits.infallibleAppend(StringValue(sub));
    }

    return NewCopiedArrayTryUseGroup(cx, group, splits.begin(), splits.length());
}

template <typename TextChar>
static MOZ_ALWAYS_INLINE JSObject*
SplitSingleCharHelper(JSContext* cx, HandleLinearString str, const TextChar* text,
                      uint32_t textLen, char16_t patCh, HandleObjectGroup group)
{
    // Count the number of occurrences of patCh within text.
    uint32_t count = 0;
    for (size_t index = 0; index < textLen; index++) {
        if (static_cast<char16_t>(text[index]) == patCh)
            count++;
    }

    // Handle zero-occurrence case - return input string in an array.
    if (count == 0) {
        RootedValue strValue(cx, StringValue(str.get()));
        return NewCopiedArrayTryUseGroup(cx, group, &strValue.get(), 1);
    }

    // Reserve memory for substring values.
    AutoValueVector splits(cx);
    if (!splits.reserve(count + 1))
        return nullptr;

    // Add substrings.
    size_t lastEndIndex = 0;
    for (size_t index = 0; index < textLen; index++) {
        if (static_cast<char16_t>(text[index]) == patCh) {
            size_t subLength = size_t(index - lastEndIndex);
            JSString* sub = NewDependentString(cx, str, lastEndIndex, subLength);
            if (!sub || !splits.append(StringValue(sub)))
                return nullptr;
            lastEndIndex = index + 1;
        }
    }

    // Add substring for tail of string (after last match).
    JSString* sub = NewDependentString(cx, str, lastEndIndex, textLen - lastEndIndex);
    if (!sub || !splits.append(StringValue(sub)))
        return nullptr;

    return NewCopiedArrayTryUseGroup(cx, group, splits.begin(), splits.length());
}

// ES 2016 draft Mar 25, 2016 21.1.3.17 steps 4, 8, 12-18.
static JSObject*
SplitSingleCharHelper(JSContext* cx, HandleLinearString str, char16_t ch, HandleObjectGroup group) {

    // Step 12.
    size_t strLength = str->length();

    AutoStableStringChars linearChars(cx);
    if (!linearChars.init(cx, str))
        return nullptr;

    if (linearChars.isLatin1()) {
        return SplitSingleCharHelper(cx, str, linearChars.latin1Chars(), strLength, ch, group);
    } else {
        return SplitSingleCharHelper(cx, str, linearChars.twoByteChars(), strLength, ch, group);
    }
}

// ES 2016 draft Mar 25, 2016 21.1.3.17 steps 4, 8, 12-18.
JSObject*
js::str_split_string(JSContext* cx, HandleObjectGroup group, HandleString str, HandleString sep, uint32_t limit)

{
    RootedLinearString linearStr(cx, str->ensureLinear(cx));
    if (!linearStr)
        return nullptr;

    RootedLinearString linearSep(cx, sep->ensureLinear(cx));
    if (!linearSep)
        return nullptr;

    if (linearSep->length() == 0)
        return CharSplitHelper(cx, linearStr, limit, group);

    if (linearSep->length() == 1 && limit >= static_cast<uint32_t>(INT32_MAX)) {
        char16_t ch = linearSep->latin1OrTwoByteChar(0);
        return SplitSingleCharHelper(cx, linearStr, ch, group);
    }

    return SplitHelper(cx, linearStr, limit, linearSep, group);
}

/*
 * Python-esque sequence operations.
 */
bool
js::str_concat(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    JSString* str = ToStringForStringFunction(cx, args.thisv());
    if (!str)
        return false;

    for (unsigned i = 0; i < args.length(); i++) {
        JSString* argStr = ToString<NoGC>(cx, args[i]);
        if (!argStr) {
            RootedString strRoot(cx, str);
            argStr = ToString<CanGC>(cx, args[i]);
            if (!argStr)
                return false;
            str = strRoot;
        }

        JSString* next = ConcatStrings<NoGC>(cx, str, argStr);
        if (next) {
            str = next;
        } else {
            RootedString strRoot(cx, str), argStrRoot(cx, argStr);
            str = ConcatStrings<CanGC>(cx, strRoot, argStrRoot);
            if (!str)
                return false;
        }
    }

    args.rval().setString(str);
    return true;
}

static const JSFunctionSpec string_methods[] = {
#if JS_HAS_TOSOURCE
    JS_FN(js_toSource_str,     str_toSource,          0,0),
#endif

    /* Java-like methods. */
    JS_FN(js_toString_str,     str_toString,          0,0),
    JS_FN(js_valueOf_str,      str_toString,          0,0),
    JS_FN("toLowerCase",       str_toLowerCase,       0,0),
    JS_FN("toUpperCase",       str_toUpperCase,       0,0),
    JS_INLINABLE_FN("charAt",  str_charAt,            1,0, StringCharAt),
    JS_INLINABLE_FN("charCodeAt", str_charCodeAt,     1,0, StringCharCodeAt),
    JS_SELF_HOSTED_FN("substring", "String_substring", 2,0),
    JS_SELF_HOSTED_FN("padStart", "String_pad_start", 2,0),
    JS_SELF_HOSTED_FN("padEnd", "String_pad_end", 2,0),
    JS_SELF_HOSTED_FN("codePointAt", "String_codePointAt", 1,0),
    JS_FN("includes",          str_includes,          1,0),
    JS_FN("indexOf",           str_indexOf,           1,0),
    JS_FN("lastIndexOf",       str_lastIndexOf,       1,0),
    JS_FN("startsWith",        str_startsWith,        1,0),
    JS_FN("endsWith",          str_endsWith,          1,0),
    JS_FN("trim",              str_trim,              0,0),
    JS_FN("trimLeft",          str_trimLeft,          0,0),
    JS_FN("trimRight",         str_trimRight,         0,0),
#if EXPOSE_INTL_API
    JS_SELF_HOSTED_FN("toLocaleLowerCase", "String_toLocaleLowerCase", 0,0),
    JS_SELF_HOSTED_FN("toLocaleUpperCase", "String_toLocaleUpperCase", 0,0),
    JS_SELF_HOSTED_FN("localeCompare", "String_localeCompare", 1,0),
#else
    JS_FN("toLocaleLowerCase", str_toLocaleLowerCase, 0,0),
    JS_FN("toLocaleUpperCase", str_toLocaleUpperCase, 0,0),
    JS_FN("localeCompare",     str_localeCompare,     1,0),
#endif
    JS_SELF_HOSTED_FN("repeat", "String_repeat",      1,0),
#if EXPOSE_INTL_API
    JS_FN("normalize",         str_normalize,         0,0),
#endif

    /* Perl-ish methods (search is actually Python-esque). */
    JS_SELF_HOSTED_FN("match", "String_match",        1,0),
    JS_SELF_HOSTED_FN("search", "String_search",      1,0),
    JS_SELF_HOSTED_FN("replace", "String_replace",    2,0),
    JS_SELF_HOSTED_FN("split",  "String_split",       2,0),
    JS_SELF_HOSTED_FN("substr", "String_substr",      2,0),

    /* Python-esque sequence methods. */
    JS_FN("concat",            str_concat,            1,0),
    JS_SELF_HOSTED_FN("slice", "String_slice",        2,0),

    /* HTML string methods. */
    JS_SELF_HOSTED_FN("bold",     "String_bold",       0,0),
    JS_SELF_HOSTED_FN("italics",  "String_italics",    0,0),
    JS_SELF_HOSTED_FN("fixed",    "String_fixed",      0,0),
    JS_SELF_HOSTED_FN("strike",   "String_strike",     0,0),
    JS_SELF_HOSTED_FN("small",    "String_small",      0,0),
    JS_SELF_HOSTED_FN("big",      "String_big",        0,0),
    JS_SELF_HOSTED_FN("blink",    "String_blink",      0,0),
    JS_SELF_HOSTED_FN("sup",      "String_sup",        0,0),
    JS_SELF_HOSTED_FN("sub",      "String_sub",        0,0),
    JS_SELF_HOSTED_FN("anchor",   "String_anchor",     1,0),
    JS_SELF_HOSTED_FN("link",     "String_link",       1,0),
    JS_SELF_HOSTED_FN("fontcolor","String_fontcolor",  1,0),
    JS_SELF_HOSTED_FN("fontsize", "String_fontsize",   1,0),

    JS_SELF_HOSTED_SYM_FN(iterator, "String_iterator", 0,0),
    JS_FS_END
};

// ES6 rev 27 (2014 Aug 24) 21.1.1
bool
js::StringConstructor(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    RootedString str(cx);
    if (args.length() > 0) {
        if (!args.isConstructing() && args[0].isSymbol())
            return js::SymbolDescriptiveString(cx, args[0].toSymbol(), args.rval());

        str = ToString<CanGC>(cx, args[0]);
        if (!str)
            return false;
    } else {
        str = cx->runtime()->emptyString;
    }

    if (args.isConstructing()) {
        RootedObject proto(cx);
        RootedObject newTarget(cx, &args.newTarget().toObject());
        if (!GetPrototypeFromConstructor(cx, newTarget, &proto))
            return false;

        StringObject* strobj = StringObject::create(cx, str, proto);
        if (!strobj)
            return false;
        args.rval().setObject(*strobj);
        return true;
    }

    args.rval().setString(str);
    return true;
}

static bool
str_fromCharCode_few_args(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(args.length() <= JSFatInlineString::MAX_LENGTH_TWO_BYTE);

    char16_t chars[JSFatInlineString::MAX_LENGTH_TWO_BYTE];
    for (unsigned i = 0; i < args.length(); i++) {
        uint16_t code;
        if (!ToUint16(cx, args[i], &code))
            return false;
        chars[i] = char16_t(code);
    }
    JSString* str = NewStringCopyN<CanGC>(cx, chars, args.length());
    if (!str)
        return false;
    args.rval().setString(str);
    return true;
}

bool
js::str_fromCharCode(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    MOZ_ASSERT(args.length() <= ARGS_LENGTH_MAX);

    // Optimize the single-char case.
    if (args.length() == 1)
        return str_fromCharCode_one_arg(cx, args[0], args.rval());

    // Optimize the case where the result will definitely fit in an inline
    // string (thin or fat) and so we don't need to malloc the chars. (We could
    // cover some cases where args.length() goes up to
    // JSFatInlineString::MAX_LENGTH_LATIN1 if we also checked if the chars are
    // all Latin-1, but it doesn't seem worth the effort.)
    if (args.length() <= JSFatInlineString::MAX_LENGTH_TWO_BYTE)
        return str_fromCharCode_few_args(cx, args);

    char16_t* chars = cx->pod_malloc<char16_t>(args.length() + 1);
    if (!chars)
        return false;
    for (unsigned i = 0; i < args.length(); i++) {
        uint16_t code;
        if (!ToUint16(cx, args[i], &code)) {
            js_free(chars);
            return false;
        }
        chars[i] = char16_t(code);
    }
    chars[args.length()] = 0;
    JSString* str = NewString<CanGC>(cx, chars, args.length());
    if (!str) {
        js_free(chars);
        return false;
    }

    args.rval().setString(str);
    return true;
}

static inline bool
CodeUnitToString(JSContext* cx, uint16_t ucode, MutableHandleValue rval)
{
    if (StaticStrings::hasUnit(ucode)) {
        rval.setString(cx->staticStrings().getUnit(ucode));
        return true;
    }

    char16_t c = char16_t(ucode);
    JSString* str = NewStringCopyN<CanGC>(cx, &c, 1);
    if (!str)
        return false;

    rval.setString(str);
    return true;
}

bool
js::str_fromCharCode_one_arg(JSContext* cx, HandleValue code, MutableHandleValue rval)
{
    uint16_t ucode;

    if (!ToUint16(cx, code, &ucode))
        return false;

    return CodeUnitToString(cx, ucode, rval);
}

static MOZ_ALWAYS_INLINE bool
ToCodePoint(JSContext* cx, HandleValue code, uint32_t* codePoint)
{
    // String.fromCodePoint, Steps 5.a-b.
    double nextCP;
    if (!ToNumber(cx, code, &nextCP))
        return false;

    // String.fromCodePoint, Steps 5.c-d.
    if (JS::ToInteger(nextCP) != nextCP || nextCP < 0 || nextCP > unicode::NonBMPMax) {
        ToCStringBuf cbuf;
        if (char* numStr = NumberToCString(cx, &cbuf, nextCP))
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_NOT_A_CODEPOINT, numStr);
        return false;
    }

    *codePoint = uint32_t(nextCP);
    return true;
}

bool
js::str_fromCodePoint_one_arg(JSContext* cx, HandleValue code, MutableHandleValue rval)
{
    // Steps 1-4 (omitted).

    // Steps 5.a-d.
    uint32_t codePoint;
    if (!ToCodePoint(cx, code, &codePoint))
        return false;

    // Steps 5.e, 6.
    if (!unicode::IsSupplementary(codePoint))
        return CodeUnitToString(cx, uint16_t(codePoint), rval);

    char16_t chars[] = { unicode::LeadSurrogate(codePoint), unicode::TrailSurrogate(codePoint) };
    JSString* str = NewStringCopyNDontDeflate<CanGC>(cx, chars, 2);
    if (!str)
        return false;

    rval.setString(str);
    return true;
}

static bool
str_fromCodePoint_few_args(JSContext* cx, const CallArgs& args)
{
    MOZ_ASSERT(args.length() <= JSFatInlineString::MAX_LENGTH_TWO_BYTE / 2);

    // Steps 1-2 (omitted).

    // Step 3.
    char16_t elements[JSFatInlineString::MAX_LENGTH_TWO_BYTE];

    // Steps 4-5.
    unsigned length = 0;
    for (unsigned nextIndex = 0; nextIndex < args.length(); nextIndex++) {
        // Steps 5.a-d.
        uint32_t codePoint;
        if (!ToCodePoint(cx, args[nextIndex], &codePoint))
            return false;

        // Step 5.e.
        unicode::UTF16Encode(codePoint, elements, &length);
    }

    // Step 6.
    JSString* str = NewStringCopyN<CanGC>(cx, elements, length);
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

// ES2017 draft rev 40edb3a95a475c1b251141ac681b8793129d9a6d
// 21.1.2.2 String.fromCodePoint(...codePoints)
bool
js::str_fromCodePoint(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    // Optimize the single code-point case.
    if (args.length() == 1)
        return str_fromCodePoint_one_arg(cx, args[0], args.rval());

    // Optimize the case where the result will definitely fit in an inline
    // string (thin or fat) and so we don't need to malloc the chars. (We could
    // cover some cases where |args.length()| goes up to
    // JSFatInlineString::MAX_LENGTH_LATIN1 / 2 if we also checked if the chars
    // are all Latin-1, but it doesn't seem worth the effort.)
    if (args.length() <= JSFatInlineString::MAX_LENGTH_TWO_BYTE / 2)
        return str_fromCodePoint_few_args(cx, args);

    // Steps 1-2 (omitted).

    // Step 3.
    static_assert(ARGS_LENGTH_MAX < std::numeric_limits<size_t>::max() / 2,
                  "|args.length() * 2 + 1| does not overflow");
    char16_t* elements = cx->pod_malloc<char16_t>(args.length() * 2 + 1);
    if (!elements)
        return false;

    // Steps 4-5.
    unsigned length = 0;
    for (unsigned nextIndex = 0; nextIndex < args.length(); nextIndex++) {
        // Steps 5.a-d.
        uint32_t codePoint;
        if (!ToCodePoint(cx, args[nextIndex], &codePoint)) {
            js_free(elements);
            return false;
        }

        // Step 5.e.
        unicode::UTF16Encode(codePoint, elements, &length);
    }
    elements[length] = 0;

    // Step 6.
    JSString* str = NewString<CanGC>(cx, elements, length);
    if (!str) {
        js_free(elements);
        return false;
    }

    args.rval().setString(str);
    return true;
}

static const JSFunctionSpec string_static_methods[] = {
    JS_INLINABLE_FN("fromCharCode", js::str_fromCharCode, 1, 0, StringFromCharCode),
    JS_INLINABLE_FN("fromCodePoint", js::str_fromCodePoint, 1, 0, StringFromCodePoint),

    JS_SELF_HOSTED_FN("raw",             "String_static_raw",           2,0),
    JS_SELF_HOSTED_FN("substring",       "String_static_substring",     3,0),
    JS_SELF_HOSTED_FN("substr",          "String_static_substr",        3,0),
    JS_SELF_HOSTED_FN("slice",           "String_static_slice",         3,0),

    JS_SELF_HOSTED_FN("match",           "String_generic_match",        2,0),
    JS_SELF_HOSTED_FN("replace",         "String_generic_replace",      3,0),
    JS_SELF_HOSTED_FN("search",          "String_generic_search",       2,0),
    JS_SELF_HOSTED_FN("split",           "String_generic_split",        3,0),

    JS_SELF_HOSTED_FN("toLowerCase",     "String_static_toLowerCase",   1,0),
    JS_SELF_HOSTED_FN("toUpperCase",     "String_static_toUpperCase",   1,0),
    JS_SELF_HOSTED_FN("charAt",          "String_static_charAt",        2,0),
    JS_SELF_HOSTED_FN("charCodeAt",      "String_static_charCodeAt",    2,0),
    JS_SELF_HOSTED_FN("includes",        "String_static_includes",      2,0),
    JS_SELF_HOSTED_FN("indexOf",         "String_static_indexOf",       2,0),
    JS_SELF_HOSTED_FN("lastIndexOf",     "String_static_lastIndexOf",   2,0),
    JS_SELF_HOSTED_FN("startsWith",      "String_static_startsWith",    2,0),
    JS_SELF_HOSTED_FN("endsWith",        "String_static_endsWith",      2,0),
    JS_SELF_HOSTED_FN("trim",            "String_static_trim",          1,0),
    JS_SELF_HOSTED_FN("trimLeft",        "String_static_trimLeft",      1,0),
    JS_SELF_HOSTED_FN("trimRight",       "String_static_trimRight",     1,0),
    JS_SELF_HOSTED_FN("toLocaleLowerCase","String_static_toLocaleLowerCase",1,0),
    JS_SELF_HOSTED_FN("toLocaleUpperCase","String_static_toLocaleUpperCase",1,0),
#if EXPOSE_INTL_API
    JS_SELF_HOSTED_FN("normalize",       "String_static_normalize",     1,0),
#endif
    JS_SELF_HOSTED_FN("concat",          "String_static_concat",        2,0),

    JS_SELF_HOSTED_FN("localeCompare",   "String_static_localeCompare", 2,0),
    JS_FS_END
};

/* static */ Shape*
StringObject::assignInitialShape(JSContext* cx, Handle<StringObject*> obj)
{
    MOZ_ASSERT(obj->empty());

    return NativeObject::addDataProperty(cx, obj, cx->names().length, LENGTH_SLOT,
                                         JSPROP_PERMANENT | JSPROP_READONLY);
}

JSObject*
js::InitStringClass(JSContext* cx, HandleObject obj)
{
    MOZ_ASSERT(obj->isNative());

    Handle<GlobalObject*> global = obj.as<GlobalObject>();

    Rooted<JSString*> empty(cx, cx->runtime()->emptyString);
    RootedObject proto(cx, GlobalObject::createBlankPrototype(cx, global, &StringObject::class_));
    if (!proto)
        return nullptr;
    Handle<StringObject*> protoObj = proto.as<StringObject>();
    if (!StringObject::init(cx, protoObj, empty))
        return nullptr;

    /* Now create the String function. */
    RootedFunction ctor(cx);
    ctor = GlobalObject::createConstructor(cx, StringConstructor, cx->names().String, 1,
                                           AllocKind::FUNCTION, &jit::JitInfo_String);
    if (!ctor)
        return nullptr;

    if (!LinkConstructorAndPrototype(cx, ctor, proto))
        return nullptr;

    if (!DefinePropertiesAndFunctions(cx, proto, nullptr, string_methods) ||
        !DefinePropertiesAndFunctions(cx, ctor, nullptr, string_static_methods))
    {
        return nullptr;
    }

    /*
     * Define escape/unescape, the URI encode/decode functions, and maybe
     * uneval on the global object.
     */
    if (!JS_DefineFunctions(cx, global, string_functions))
        return nullptr;

    if (!GlobalObject::initBuiltinConstructor(cx, global, JSProto_String, ctor, proto))
        return nullptr;

    return proto;
}

const char*
js::ValueToPrintable(JSContext* cx, const Value& vArg, JSAutoByteString* bytes, bool asSource)
{
    RootedValue v(cx, vArg);
    JSString* str;
    if (asSource)
        str = ValueToSource(cx, v);
    else
        str = ToString<CanGC>(cx, v);
    if (!str)
        return nullptr;
    str = QuoteString(cx, str, 0);
    if (!str)
        return nullptr;
    return bytes->encodeLatin1(cx, str);
}

template <AllowGC allowGC>
JSString*
js::ToStringSlow(JSContext* cx, typename MaybeRooted<Value, allowGC>::HandleType arg)
{
    /* As with ToObjectSlow, callers must verify that |arg| isn't a string. */
    MOZ_ASSERT(!arg.isString());

    Value v = arg;
    if (!v.isPrimitive()) {
        MOZ_ASSERT(!cx->helperThread());
        if (!allowGC)
            return nullptr;
        RootedValue v2(cx, v);
        if (!ToPrimitive(cx, JSTYPE_STRING, &v2))
            return nullptr;
        v = v2;
    }

    JSString* str;
    if (v.isString()) {
        str = v.toString();
    } else if (v.isInt32()) {
        str = Int32ToString<allowGC>(cx, v.toInt32());
    } else if (v.isDouble()) {
        str = NumberToString<allowGC>(cx, v.toDouble());
    } else if (v.isBoolean()) {
        str = BooleanToString(cx, v.toBoolean());
    } else if (v.isNull()) {
        str = cx->names().null;
    } else if (v.isSymbol()) {
        MOZ_ASSERT(!cx->helperThread());
        if (allowGC) {
            JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr,
                                      JSMSG_SYMBOL_TO_STRING);
        }
        return nullptr;
    } else {
        MOZ_ASSERT(v.isUndefined());
        str = cx->names().undefined;
    }
    return str;
}

template JSString*
js::ToStringSlow<CanGC>(JSContext* cx, HandleValue arg);

template JSString*
js::ToStringSlow<NoGC>(JSContext* cx, const Value& arg);

JS_PUBLIC_API(JSString*)
js::ToStringSlow(JSContext* cx, HandleValue v)
{
    return ToStringSlow<CanGC>(cx, v);
}

static JSString*
SymbolToSource(JSContext* cx, Symbol* symbol)
{
    RootedString desc(cx, symbol->description());
    SymbolCode code = symbol->code();
    if (code != SymbolCode::InSymbolRegistry && code != SymbolCode::UniqueSymbol) {
        // Well-known symbol.
        MOZ_ASSERT(uint32_t(code) < JS::WellKnownSymbolLimit);
        return desc;
    }

    StringBuffer buf(cx);
    if (code == SymbolCode::InSymbolRegistry ? !buf.append("Symbol.for(") : !buf.append("Symbol("))
        return nullptr;
    if (desc) {
        desc = StringToSource(cx, desc);
        if (!desc || !buf.append(desc))
            return nullptr;
    }
    if (!buf.append(')'))
        return nullptr;
    return buf.finishString();
}

JSString*
js::ValueToSource(JSContext* cx, HandleValue v)
{
    if (!CheckRecursionLimit(cx))
        return nullptr;
    assertSameCompartment(cx, v);

    if (v.isUndefined())
        return cx->names().void0;
    if (v.isString())
        return StringToSource(cx, v.toString());
    if (v.isSymbol())
        return SymbolToSource(cx, v.toSymbol());
    if (v.isPrimitive()) {
        /* Special case to preserve negative zero, _contra_ toString. */
        if (v.isDouble() && IsNegativeZero(v.toDouble())) {
            /* NB: _ucNstr rather than _ucstr to indicate non-terminated. */
            static const char16_t js_negzero_ucNstr[] = {'-', '0'};

            return NewStringCopyN<CanGC>(cx, js_negzero_ucNstr, 2);
        }
        return ToString<CanGC>(cx, v);
    }

    RootedValue fval(cx);
    RootedObject obj(cx, &v.toObject());
    if (!GetProperty(cx, obj, obj, cx->names().toSource, &fval))
        return nullptr;
    if (IsCallable(fval)) {
        RootedValue v(cx);
        if (!js::Call(cx, fval, obj, &v))
            return nullptr;

        return ToString<CanGC>(cx, v);
    }

    return ObjectToSource(cx, obj);
}

JSString*
js::StringToSource(JSContext* cx, JSString* str)
{
    return QuoteString(cx, str, '"');
}

bool
js::EqualChars(JSLinearString* str1, JSLinearString* str2)
{
    MOZ_ASSERT(str1->length() == str2->length());

    size_t len = str1->length();

    AutoCheckCannotGC nogc;
    if (str1->hasTwoByteChars()) {
        if (str2->hasTwoByteChars())
            return PodEqual(str1->twoByteChars(nogc), str2->twoByteChars(nogc), len);

        return EqualChars(str2->latin1Chars(nogc), str1->twoByteChars(nogc), len);
    }

    if (str2->hasLatin1Chars())
        return PodEqual(str1->latin1Chars(nogc), str2->latin1Chars(nogc), len);

    return EqualChars(str1->latin1Chars(nogc), str2->twoByteChars(nogc), len);
}

bool
js::EqualStrings(JSContext* cx, JSString* str1, JSString* str2, bool* result)
{
    if (str1 == str2) {
        *result = true;
        return true;
    }

    size_t length1 = str1->length();
    if (length1 != str2->length()) {
        *result = false;
        return true;
    }

    JSLinearString* linear1 = str1->ensureLinear(cx);
    if (!linear1)
        return false;
    JSLinearString* linear2 = str2->ensureLinear(cx);
    if (!linear2)
        return false;

    *result = EqualChars(linear1, linear2);
    return true;
}

bool
js::EqualStrings(JSLinearString* str1, JSLinearString* str2)
{
    if (str1 == str2)
        return true;

    size_t length1 = str1->length();
    if (length1 != str2->length())
        return false;

    return EqualChars(str1, str2);
}

static int32_t
CompareStringsImpl(JSLinearString* str1, JSLinearString* str2)
{
    size_t len1 = str1->length();
    size_t len2 = str2->length();

    AutoCheckCannotGC nogc;
    if (str1->hasLatin1Chars()) {
        const Latin1Char* chars1 = str1->latin1Chars(nogc);
        return str2->hasLatin1Chars()
               ? CompareChars(chars1, len1, str2->latin1Chars(nogc), len2)
               : CompareChars(chars1, len1, str2->twoByteChars(nogc), len2);
    }

    const char16_t* chars1 = str1->twoByteChars(nogc);
    return str2->hasLatin1Chars()
           ? CompareChars(chars1, len1, str2->latin1Chars(nogc), len2)
           : CompareChars(chars1, len1, str2->twoByteChars(nogc), len2);
}

int32_t
js::CompareChars(const char16_t* s1, size_t len1, JSLinearString* s2)
{
    AutoCheckCannotGC nogc;
    return s2->hasLatin1Chars()
           ? CompareChars(s1, len1, s2->latin1Chars(nogc), s2->length())
           : CompareChars(s1, len1, s2->twoByteChars(nogc), s2->length());
}

bool
js::CompareStrings(JSContext* cx, JSString* str1, JSString* str2, int32_t* result)
{
    MOZ_ASSERT(str1);
    MOZ_ASSERT(str2);

    if (str1 == str2) {
        *result = 0;
        return true;
    }

    JSLinearString* linear1 = str1->ensureLinear(cx);
    if (!linear1)
        return false;

    JSLinearString* linear2 = str2->ensureLinear(cx);
    if (!linear2)
        return false;

    *result = CompareStringsImpl(linear1, linear2);
    return true;
}

int32_t
js::CompareAtoms(JSAtom* atom1, JSAtom* atom2)
{
    return CompareStringsImpl(atom1, atom2);
}

bool
js::StringEqualsAscii(JSLinearString* str, const char* asciiBytes)
{
    size_t length = strlen(asciiBytes);
#ifdef DEBUG
    for (size_t i = 0; i != length; ++i)
        MOZ_ASSERT(unsigned(asciiBytes[i]) <= 127);
#endif
    if (length != str->length())
        return false;

    const Latin1Char* latin1 = reinterpret_cast<const Latin1Char*>(asciiBytes);

    AutoCheckCannotGC nogc;
    return str->hasLatin1Chars()
           ? PodEqual(latin1, str->latin1Chars(nogc), length)
           : EqualChars(latin1, str->twoByteChars(nogc), length);
}

size_t
js_strlen(const char16_t* s)
{
    const char16_t* t;

    for (t = s; *t != 0; t++)
        continue;
    return (size_t)(t - s);
}

int32_t
js_strcmp(const char16_t* lhs, const char16_t* rhs)
{
    while (true) {
        if (*lhs != *rhs)
            return int32_t(*lhs) - int32_t(*rhs);
        if (*lhs == 0)
            return 0;
        ++lhs; ++rhs;
    }
}

int32_t
js_fputs(const char16_t* s, FILE* f)
{
    while (*s != 0) {
        if (fputwc(wchar_t(*s), f) == WEOF)
            return WEOF;
        s++;
    }
    return 1;
}

UniqueChars
js::DuplicateString(JSContext* cx, const char* s)
{
    size_t n = strlen(s) + 1;
    auto ret = cx->make_pod_array<char>(n);
    if (!ret)
        return ret;
    PodCopy(ret.get(), s, n);
    return ret;
}

UniqueTwoByteChars
js::DuplicateString(JSContext* cx, const char16_t* s)
{
    size_t n = js_strlen(s) + 1;
    auto ret = cx->make_pod_array<char16_t>(n);
    if (!ret)
        return ret;
    PodCopy(ret.get(), s, n);
    return ret;
}

UniqueChars
js::DuplicateString(const char* s)
{
    return UniqueChars(js_strdup(s));
}

UniqueChars
js::DuplicateString(const char* s, size_t n)
{
    UniqueChars ret(js_pod_malloc<char>(n + 1));
    if (!ret)
        return nullptr;
    PodCopy(ret.get(), s, n);
    ret[n] = 0;
    return ret;
}

UniqueTwoByteChars
js::DuplicateString(const char16_t* s)
{
    return DuplicateString(s, js_strlen(s));
}

UniqueTwoByteChars
js::DuplicateString(const char16_t* s, size_t n)
{
    UniqueTwoByteChars ret(js_pod_malloc<char16_t>(n + 1));
    if (!ret)
        return nullptr;
    PodCopy(ret.get(), s, n);
    ret[n] = 0;
    return ret;
}

template <typename CharT>
const CharT*
js_strchr_limit(const CharT* s, char16_t c, const CharT* limit)
{
    while (s < limit) {
        if (*s == c)
            return s;
        s++;
    }
    return nullptr;
}

template const Latin1Char*
js_strchr_limit(const Latin1Char* s, char16_t c, const Latin1Char* limit);

template const char16_t*
js_strchr_limit(const char16_t* s, char16_t c, const char16_t* limit);

char16_t*
js::InflateString(JSContext* cx, const char* bytes, size_t* lengthp)
{
    size_t nchars;
    char16_t* chars;
    size_t nbytes = *lengthp;

    nchars = nbytes;
    chars = cx->pod_malloc<char16_t>(nchars + 1);
    if (!chars)
        goto bad;
    for (size_t i = 0; i < nchars; i++)
        chars[i] = (unsigned char) bytes[i];
    *lengthp = nchars;
    chars[nchars] = 0;
    return chars;

  bad:
    // For compatibility with callers of JS_DecodeBytes we must zero lengthp
    // on errors.
    *lengthp = 0;
    return nullptr;
}

template <typename CharT>
bool
js::DeflateStringToBuffer(JSContext* maybecx, const CharT* src, size_t srclen,
                          char* dst, size_t* dstlenp)
{
    size_t dstlen = *dstlenp;
    if (srclen > dstlen) {
        for (size_t i = 0; i < dstlen; i++)
            dst[i] = char(src[i]);
        if (maybecx) {
            AutoSuppressGC suppress(maybecx);
            JS_ReportErrorNumberASCII(maybecx, GetErrorMessage, nullptr,
                                      JSMSG_BUFFER_TOO_SMALL);
        }
        return false;
    }
    for (size_t i = 0; i < srclen; i++)
        dst[i] = char(src[i]);
    *dstlenp = srclen;
    return true;
}

template bool
js::DeflateStringToBuffer(JSContext* maybecx, const Latin1Char* src, size_t srclen,
                          char* dst, size_t* dstlenp);

template bool
js::DeflateStringToBuffer(JSContext* maybecx, const char16_t* src, size_t srclen,
                          char* dst, size_t* dstlenp);

#define ____ false

/*
 * Identifier start chars:
 * -      36:    $
 * -  65..90: A..Z
 * -      95:    _
 * - 97..122: a..z
 */
const bool js_isidstart[] = {
/*       0     1     2     3     4     5     6     7     8     9  */
/*  0 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  1 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  2 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  3 */ ____, ____, ____, ____, ____, ____, true, ____, ____, ____,
/*  4 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  5 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  6 */ ____, ____, ____, ____, ____, true, true, true, true, true,
/*  7 */ true, true, true, true, true, true, true, true, true, true,
/*  8 */ true, true, true, true, true, true, true, true, true, true,
/*  9 */ true, ____, ____, ____, ____, true, ____, true, true, true,
/* 10 */ true, true, true, true, true, true, true, true, true, true,
/* 11 */ true, true, true, true, true, true, true, true, true, true,
/* 12 */ true, true, true, ____, ____, ____, ____, ____
};

/*
 * Identifier chars:
 * -      36:    $
 * -  48..57: 0..9
 * -  65..90: A..Z
 * -      95:    _
 * - 97..122: a..z
 */
const bool js_isident[] = {
/*       0     1     2     3     4     5     6     7     8     9  */
/*  0 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  1 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  2 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  3 */ ____, ____, ____, ____, ____, ____, true, ____, ____, ____,
/*  4 */ ____, ____, ____, ____, ____, ____, ____, ____, true, true,
/*  5 */ true, true, true, true, true, true, true, true, ____, ____,
/*  6 */ ____, ____, ____, ____, ____, true, true, true, true, true,
/*  7 */ true, true, true, true, true, true, true, true, true, true,
/*  8 */ true, true, true, true, true, true, true, true, true, true,
/*  9 */ true, ____, ____, ____, ____, true, ____, true, true, true,
/* 10 */ true, true, true, true, true, true, true, true, true, true,
/* 11 */ true, true, true, true, true, true, true, true, true, true,
/* 12 */ true, true, true, ____, ____, ____, ____, ____
};

/* Whitespace chars: '\t', '\n', '\v', '\f', '\r', ' '. */
const bool js_isspace[] = {
/*       0     1     2     3     4     5     6     7     8     9  */
/*  0 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, true,
/*  1 */ true, true, true, true, ____, ____, ____, ____, ____, ____,
/*  2 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  3 */ ____, ____, true, ____, ____, ____, ____, ____, ____, ____,
/*  4 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  5 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  6 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  7 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  8 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  9 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/* 10 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/* 11 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/* 12 */ ____, ____, ____, ____, ____, ____, ____, ____
};

/*
 * Uri reserved chars + #:
 * - 35: #
 * - 36: $
 * - 38: &
 * - 43: +
 * - 44: ,
 * - 47: /
 * - 58: :
 * - 59: ;
 * - 61: =
 * - 63: ?
 * - 64: @
 */
static const bool js_isUriReservedPlusPound[] = {
/*       0     1     2     3     4     5     6     7     8     9  */
/*  0 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  1 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  2 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  3 */ ____, ____, ____, ____, ____, true, true, ____, true, ____,
/*  4 */ ____, ____, ____, true, true, ____, ____, true, ____, ____,
/*  5 */ ____, ____, ____, ____, ____, ____, ____, ____, true, true,
/*  6 */ ____, true, ____, true, true, ____, ____, ____, ____, ____,
/*  7 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  8 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  9 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/* 10 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/* 11 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/* 12 */ ____, ____, ____, ____, ____, ____, ____, ____
};

/*
 * Uri unescaped chars:
 * -      33: !
 * -      39: '
 * -      40: (
 * -      41: )
 * -      42: *
 * -      45: -
 * -      46: .
 * -  48..57: 0-9
 * -  65..90: A-Z
 * -      95: _
 * - 97..122: a-z
 * -     126: ~
 */
static const bool js_isUriUnescaped[] = {
/*       0     1     2     3     4     5     6     7     8     9  */
/*  0 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  1 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  2 */ ____, ____, ____, ____, ____, ____, ____, ____, ____, ____,
/*  3 */ ____, ____, ____, true, ____, ____, ____, ____, ____, true,
/*  4 */ true, true, true, ____, ____, true, true, ____, true, true,
/*  5 */ true, true, true, true, true, true, true, true, ____, ____,
/*  6 */ ____, ____, ____, ____, ____, true, true, true, true, true,
/*  7 */ true, true, true, true, true, true, true, true, true, true,
/*  8 */ true, true, true, true, true, true, true, true, true, true,
/*  9 */ true, ____, ____, ____, ____, true, ____, true, true, true,
/* 10 */ true, true, true, true, true, true, true, true, true, true,
/* 11 */ true, true, true, true, true, true, true, true, true, true,
/* 12 */ true, true, true, ____, ____, ____, true, ____
};

#undef ____

#define URI_CHUNK 64U

static inline bool
TransferBufferToString(StringBuffer& sb, MutableHandleValue rval)
{
    JSString* str = sb.finishString();
    if (!str)
        return false;
    rval.setString(str);
    return true;
}

/*
 * ECMA 3, 15.1.3 URI Handling Function Properties
 *
 * The following are implementations of the algorithms
 * given in the ECMA specification for the hidden functions
 * 'Encode' and 'Decode'.
 */
enum EncodeResult { Encode_Failure, Encode_BadUri, Encode_Success };

template <typename CharT>
static EncodeResult
Encode(StringBuffer& sb, const CharT* chars, size_t length,
       const bool* unescapedSet, const bool* unescapedSet2)
{
    static const char HexDigits[] = "0123456789ABCDEF"; /* NB: uppercase */

    char16_t hexBuf[4];
    hexBuf[0] = '%';
    hexBuf[3] = 0;

    for (size_t k = 0; k < length; k++) {
        char16_t c = chars[k];
        if (c < 128 && (unescapedSet[c] || (unescapedSet2 && unescapedSet2[c]))) {
            if (!sb.append(c))
                return Encode_Failure;
        } else {
            if (unicode::IsTrailSurrogate(c))
                return Encode_BadUri;

            uint32_t v;
            if (!unicode::IsLeadSurrogate(c)) {
                v = c;
            } else {
                k++;
                if (k == length)
                    return Encode_BadUri;

                char16_t c2 = chars[k];
                if (!unicode::IsTrailSurrogate(c2))
                    return Encode_BadUri;

                v = unicode::UTF16Decode(c, c2);
            }
            uint8_t utf8buf[4];
            size_t L = OneUcs4ToUtf8Char(utf8buf, v);
            for (size_t j = 0; j < L; j++) {
                hexBuf[1] = HexDigits[utf8buf[j] >> 4];
                hexBuf[2] = HexDigits[utf8buf[j] & 0xf];
                if (!sb.append(hexBuf, 3))
                    return Encode_Failure;
            }
        }
    }

    return Encode_Success;
}

static bool
Encode(JSContext* cx, HandleLinearString str, const bool* unescapedSet,
       const bool* unescapedSet2, MutableHandleValue rval)
{
    size_t length = str->length();
    if (length == 0) {
        rval.setString(cx->runtime()->emptyString);
        return true;
    }

    StringBuffer sb(cx);
    if (!sb.reserve(length))
        return false;

    EncodeResult res;
    if (str->hasLatin1Chars()) {
        AutoCheckCannotGC nogc;
        res = Encode(sb, str->latin1Chars(nogc), str->length(), unescapedSet, unescapedSet2);
    } else {
        AutoCheckCannotGC nogc;
        res = Encode(sb, str->twoByteChars(nogc), str->length(), unescapedSet, unescapedSet2);
    }

    if (res == Encode_Failure)
        return false;

    if (res == Encode_BadUri) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_URI);
        return false;
    }

    MOZ_ASSERT(res == Encode_Success);
    return TransferBufferToString(sb, rval);
}

enum DecodeResult { Decode_Failure, Decode_BadUri, Decode_Success };

template <typename CharT>
static DecodeResult
Decode(StringBuffer& sb, const CharT* chars, size_t length, const bool* reservedSet)
{
    for (size_t k = 0; k < length; k++) {
        char16_t c = chars[k];
        if (c == '%') {
            size_t start = k;
            if ((k + 2) >= length)
                return Decode_BadUri;

            if (!JS7_ISHEX(chars[k+1]) || !JS7_ISHEX(chars[k+2]))
                return Decode_BadUri;

            uint32_t B = JS7_UNHEX(chars[k+1]) * 16 + JS7_UNHEX(chars[k+2]);
            k += 2;
            if (!(B & 0x80)) {
                c = char16_t(B);
            } else {
                int n = 1;
                while (B & (0x80 >> n))
                    n++;

                if (n == 1 || n > 4)
                    return Decode_BadUri;

                uint8_t octets[4];
                octets[0] = (uint8_t)B;
                if (k + 3 * (n - 1) >= length)
                    return Decode_BadUri;

                for (int j = 1; j < n; j++) {
                    k++;
                    if (chars[k] != '%')
                        return Decode_BadUri;

                    if (!JS7_ISHEX(chars[k+1]) || !JS7_ISHEX(chars[k+2]))
                        return Decode_BadUri;

                    B = JS7_UNHEX(chars[k+1]) * 16 + JS7_UNHEX(chars[k+2]);
                    if ((B & 0xC0) != 0x80)
                        return Decode_BadUri;

                    k += 2;
                    octets[j] = char(B);
                }
                uint32_t v = JS::Utf8ToOneUcs4Char(octets, n);
                if (v >= unicode::NonBMPMin) {
                    if (v > unicode::NonBMPMax)
                        return Decode_BadUri;

                    char16_t H = unicode::LeadSurrogate(v);
                    if (!sb.append(H))
                        return Decode_Failure;
                    c = unicode::TrailSurrogate(v);
                } else {
                    c = char16_t(v);
                }
            }
            if (c < 128 && reservedSet && reservedSet[c]) {
                if (!sb.append(chars + start, k - start + 1))
                    return Decode_Failure;
            } else {
                if (!sb.append(c))
                    return Decode_Failure;
            }
        } else {
            if (!sb.append(c))
                return Decode_Failure;
        }
    }

    return Decode_Success;
}

static bool
Decode(JSContext* cx, HandleLinearString str, const bool* reservedSet, MutableHandleValue rval)
{
    size_t length = str->length();
    if (length == 0) {
        rval.setString(cx->runtime()->emptyString);
        return true;
    }

    StringBuffer sb(cx);

    DecodeResult res;
    if (str->hasLatin1Chars()) {
        AutoCheckCannotGC nogc;
        res = Decode(sb, str->latin1Chars(nogc), str->length(), reservedSet);
    } else {
        AutoCheckCannotGC nogc;
        res = Decode(sb, str->twoByteChars(nogc), str->length(), reservedSet);
    }

    if (res == Decode_Failure)
        return false;

    if (res == Decode_BadUri) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_URI);
        return false;
    }

    MOZ_ASSERT(res == Decode_Success);
    return TransferBufferToString(sb, rval);
}

static bool
str_decodeURI(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedLinearString str(cx, ArgToRootedString(cx, args, 0));
    if (!str)
        return false;

    return Decode(cx, str, js_isUriReservedPlusPound, args.rval());
}

static bool
str_decodeURI_Component(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedLinearString str(cx, ArgToRootedString(cx, args, 0));
    if (!str)
        return false;

    return Decode(cx, str, nullptr, args.rval());
}

static bool
str_encodeURI(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedLinearString str(cx, ArgToRootedString(cx, args, 0));
    if (!str)
        return false;

    return Encode(cx, str, js_isUriUnescaped, js_isUriReservedPlusPound, args.rval());
}

static bool
str_encodeURI_Component(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    RootedLinearString str(cx, ArgToRootedString(cx, args, 0));
    if (!str)
        return false;

    return Encode(cx, str, js_isUriUnescaped, nullptr, args.rval());
}

bool
js::EncodeURI(JSContext* cx, StringBuffer& sb, const char* chars, size_t length)
{
    EncodeResult result = Encode(sb, chars, length, js_isUriUnescaped, js_isUriReservedPlusPound);
    if (result == EncodeResult::Encode_Failure)
        return false;
    if (result == EncodeResult::Encode_BadUri) {
        JS_ReportErrorNumberASCII(cx, GetErrorMessage, nullptr, JSMSG_BAD_URI);
        return false;
    }
    return true;
}

/*
 * Convert one UCS-4 char and write it into a UTF-8 buffer, which must be at
 * least 4 bytes long.  Return the number of UTF-8 bytes of data written.
 */
uint32_t
js::OneUcs4ToUtf8Char(uint8_t* utf8Buffer, uint32_t ucs4Char)
{
    MOZ_ASSERT(ucs4Char <= unicode::NonBMPMax);

    if (ucs4Char < 0x80) {
        utf8Buffer[0] = uint8_t(ucs4Char);
        return 1;
    }

    uint32_t a = ucs4Char >> 11;
    uint32_t utf8Length = 2;
    while (a) {
        a >>= 5;
        utf8Length++;
    }

    MOZ_ASSERT(utf8Length <= 4);

    uint32_t i = utf8Length;
    while (--i) {
        utf8Buffer[i] = uint8_t((ucs4Char & 0x3F) | 0x80);
        ucs4Char >>= 6;
    }

    utf8Buffer[0] = uint8_t(0x100 - (1 << (8 - utf8Length)) + ucs4Char);
    return utf8Length;
}

size_t
js::PutEscapedStringImpl(char* buffer, size_t bufferSize, GenericPrinter* out, JSLinearString* str,
                         uint32_t quote)
{
    size_t len = str->length();
    AutoCheckCannotGC nogc;
    return str->hasLatin1Chars()
           ? PutEscapedStringImpl(buffer, bufferSize, out, str->latin1Chars(nogc), len, quote)
           : PutEscapedStringImpl(buffer, bufferSize, out, str->twoByteChars(nogc), len, quote);
}

template <typename CharT>
size_t
js::PutEscapedStringImpl(char* buffer, size_t bufferSize, GenericPrinter* out, const CharT* chars,
                         size_t length, uint32_t quote)
{
    enum {
        STOP, FIRST_QUOTE, LAST_QUOTE, CHARS, ESCAPE_START, ESCAPE_MORE
    } state;

    MOZ_ASSERT(quote == 0 || quote == '\'' || quote == '"');
    MOZ_ASSERT_IF(!buffer, bufferSize == 0);
    MOZ_ASSERT_IF(out, !buffer);

    if (bufferSize == 0)
        buffer = nullptr;
    else
        bufferSize--;

    const CharT* charsEnd = chars + length;
    size_t n = 0;
    state = FIRST_QUOTE;
    unsigned shift = 0;
    unsigned hex = 0;
    unsigned u = 0;
    char c = 0;  /* to quell GCC warnings */

    for (;;) {
        switch (state) {
          case STOP:
            goto stop;
          case FIRST_QUOTE:
            state = CHARS;
            goto do_quote;
          case LAST_QUOTE:
            state = STOP;
          do_quote:
            if (quote == 0)
                continue;
            c = (char)quote;
            break;
          case CHARS:
            if (chars == charsEnd) {
                state = LAST_QUOTE;
                continue;
            }
            u = *chars++;
            if (u < ' ') {
                if (u != 0) {
                    const char* escape = strchr(js_EscapeMap, (int)u);
                    if (escape) {
                        u = escape[1];
                        goto do_escape;
                    }
                }
                goto do_hex_escape;
            }
            if (u < 127) {
                if (u == quote || u == '\\')
                    goto do_escape;
                c = (char)u;
            } else if (u < 0x100) {
                goto do_hex_escape;
            } else {
                shift = 16;
                hex = u;
                u = 'u';
                goto do_escape;
            }
            break;
          do_hex_escape:
            shift = 8;
            hex = u;
            u = 'x';
          do_escape:
            c = '\\';
            state = ESCAPE_START;
            break;
          case ESCAPE_START:
            MOZ_ASSERT(' ' <= u && u < 127);
            c = (char)u;
            state = ESCAPE_MORE;
            break;
          case ESCAPE_MORE:
            if (shift == 0) {
                state = CHARS;
                continue;
            }
            shift -= 4;
            u = 0xF & (hex >> shift);
            c = (char)(u + (u < 10 ? '0' : 'A' - 10));
            break;
        }
        if (buffer) {
            MOZ_ASSERT(n <= bufferSize);
            if (n != bufferSize) {
                buffer[n] = c;
            } else {
                buffer[n] = '\0';
                buffer = nullptr;
            }
        } else if (out) {
            if (!out->put(&c, 1))
                return size_t(-1);
        }
        n++;
    }
  stop:
    if (buffer)
        buffer[n] = '\0';
    return n;
}

template size_t
js::PutEscapedStringImpl(char* buffer, size_t bufferSize, GenericPrinter* out, const Latin1Char* chars,
                         size_t length, uint32_t quote);

template size_t
js::PutEscapedStringImpl(char* buffer, size_t bufferSize, GenericPrinter* out, const char* chars,
                         size_t length, uint32_t quote);

template size_t
js::PutEscapedStringImpl(char* buffer, size_t bufferSize, GenericPrinter* out, const char16_t* chars,
                         size_t length, uint32_t quote);

template size_t
js::PutEscapedString(char* buffer, size_t bufferSize, const Latin1Char* chars, size_t length,
                     uint32_t quote);

template size_t
js::PutEscapedString(char* buffer, size_t bufferSize, const char16_t* chars, size_t length,
                     uint32_t quote);

static bool
FlatStringMatchHelper(JSContext* cx, HandleString str, HandleString pattern, bool* isFlat, int32_t* match)
{
    RootedLinearString linearPattern(cx, pattern->ensureLinear(cx));
    if (!linearPattern)
        return false;

    static const size_t MAX_FLAT_PAT_LEN = 256;
    if (linearPattern->length() > MAX_FLAT_PAT_LEN || StringHasRegExpMetaChars(linearPattern)) {
        *isFlat = false;
        return true;
    }

    *isFlat = true;
    if (str->isRope()) {
        if (!RopeMatch(cx, &str->asRope(), linearPattern, match))
            return false;
    } else {
        *match = StringMatch(&str->asLinear(), linearPattern);
    }

    return true;
}

static bool
BuildFlatMatchArray(JSContext* cx, HandleString str, HandleString pattern, int32_t match,
                    MutableHandleValue rval)
{
    if (match < 0) {
        rval.setNull();
        return true;
    }

    /* Get the templateObject that defines the shape and type of the output object */
    JSObject* templateObject = cx->compartment()->regExps.getOrCreateMatchResultTemplateObject(cx);
    if (!templateObject)
        return false;

    RootedArrayObject arr(cx, NewDenseFullyAllocatedArrayWithTemplate(cx, 1, templateObject));
    if (!arr)
        return false;

    /* Store a Value for each pair. */
    arr->setDenseInitializedLength(1);
    arr->initDenseElement(0, StringValue(pattern));

    /* Set the |index| property. (TemplateObject positions it in slot 0) */
    arr->setSlot(0, Int32Value(match));

    /* Set the |input| property. (TemplateObject positions it in slot 1) */
    arr->setSlot(1, StringValue(str));

#ifdef DEBUG
    RootedValue test(cx);
    RootedId id(cx, NameToId(cx->names().index));
    if (!NativeGetProperty(cx, arr, id, &test))
        return false;
    MOZ_ASSERT(test == arr->getSlot(0));
    id = NameToId(cx->names().input);
    if (!NativeGetProperty(cx, arr, id, &test))
        return false;
    MOZ_ASSERT(test == arr->getSlot(1));
#endif

    rval.setObject(*arr);
    return true;
}

#ifdef DEBUG
static bool
CallIsStringOptimizable(JSContext* cx, const char* name, bool* result)
{
    JSAtom* atom = Atomize(cx, name, strlen(name));
    if (!atom)
        return false;
    RootedPropertyName propName(cx, atom->asPropertyName());

    RootedValue funcVal(cx);
    if (!GlobalObject::getSelfHostedFunction(cx, cx->global(), propName, propName, 0, &funcVal))
        return false;

    FixedInvokeArgs<0> args(cx);

    RootedValue rval(cx);
    if (!Call(cx, funcVal, UndefinedHandleValue, args, &rval))
        return false;

    *result = rval.toBoolean();
    return true;
}
#endif

bool
js::FlatStringMatch(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].isString());
    MOZ_ASSERT(args[1].isString());
#ifdef DEBUG
    bool isOptimizable = false;
    if (!CallIsStringOptimizable(cx, "IsStringMatchOptimizable", &isOptimizable))
        return false;
    MOZ_ASSERT(isOptimizable);
#endif

    RootedString str(cx,args[0].toString());
    RootedString pattern(cx, args[1].toString());

    bool isFlat = false;
    int32_t match = 0;
    if (!FlatStringMatchHelper(cx, str, pattern, &isFlat, &match))
        return false;

    if (!isFlat) {
        args.rval().setUndefined();
        return true;
    }

    return BuildFlatMatchArray(cx, str, pattern, match, args.rval());
}

bool
js::FlatStringSearch(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    MOZ_ASSERT(args.length() == 2);
    MOZ_ASSERT(args[0].isString());
    MOZ_ASSERT(args[1].isString());
#ifdef DEBUG
    bool isOptimizable = false;
    if (!CallIsStringOptimizable(cx, "IsStringSearchOptimizable", &isOptimizable))
        return false;
    MOZ_ASSERT(isOptimizable);
#endif

    RootedString str(cx,args[0].toString());
    RootedString pattern(cx, args[1].toString());

    bool isFlat = false;
    int32_t match = 0;
    if (!FlatStringMatchHelper(cx, str, pattern, &isFlat, &match))
        return false;

    if (!isFlat) {
        args.rval().setInt32(-2);
        return true;
    }

    args.rval().setInt32(match);
    return true;
}
