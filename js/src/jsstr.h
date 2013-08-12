/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsstr_h
#define jsstr_h

#include "mozilla/HashFunctions.h"
#include "mozilla/PodOperations.h"

#include "jsapi.h"
#include "jsutil.h"

#include "js/RootingAPI.h"
#include "vm/Unicode.h"

class JSFlatString;
class JSLinearString;
class JSStableString;

namespace js {

class StringBuffer;

class MutatingRopeSegmentRange;

template <AllowGC allowGC>
extern JSString *
ConcatStrings(ThreadSafeContext *cx,
              typename MaybeRooted<JSString*, allowGC>::HandleType left,
              typename MaybeRooted<JSString*, allowGC>::HandleType right);

// Return s advanced past any Unicode white space characters.
static inline const jschar *
SkipSpace(const jschar *s, const jschar *end)
{
    JS_ASSERT(s <= end);

    while (s < end && unicode::IsSpace(*s))
        s++;

    return s;
}

// Return less than, equal to, or greater than zero depending on whether
// s1 is less than, equal to, or greater than s2.
inline bool
CompareChars(const jschar *s1, size_t l1, const jschar *s2, size_t l2, int32_t *result)
{
    size_t n = Min(l1, l2);
    for (size_t i = 0; i < n; i++) {
        if (int32_t cmp = s1[i] - s2[i]) {
            *result = cmp;
            return true;
        }
    }

    *result = (int32_t)(l1 - l2);
    return true;
}

}  /* namespace js */

extern JSString * JS_FASTCALL
js_toLowerCase(JSContext *cx, JSString *str);

extern JSString * JS_FASTCALL
js_toUpperCase(JSContext *cx, JSString *str);

struct JSSubString {
    size_t          length;
    const jschar    *chars;
};

extern const jschar js_empty_ucstr[];
extern const JSSubString js_EmptySubString;

/*
 * Shorthands for ASCII (7-bit) decimal and hex conversion.
 * Manually inline isdigit for performance; MSVC doesn't do this for us.
 */
#define JS7_ISDEC(c)    ((((unsigned)(c)) - '0') <= 9)
#define JS7_UNDEC(c)    ((c) - '0')
#define JS7_ISHEX(c)    ((c) < 128 && isxdigit(c))
#define JS7_UNHEX(c)    (unsigned)(JS7_ISDEC(c) ? (c) - '0' : 10 + tolower(c) - 'a')
#define JS7_ISLET(c)    ((c) < 128 && isalpha(c))

/* Initialize the String class, returning its prototype object. */
extern JSObject *
js_InitStringClass(JSContext *cx, js::HandleObject obj);

extern const char js_escape_str[];
extern const char js_unescape_str[];
extern const char js_uneval_str[];
extern const char js_decodeURI_str[];
extern const char js_encodeURI_str[];
extern const char js_decodeURIComponent_str[];
extern const char js_encodeURIComponent_str[];

/* GC-allocate a string descriptor for the given malloc-allocated chars. */
template <js::AllowGC allowGC>
extern JSStableString *
js_NewString(js::ThreadSafeContext *cx, jschar *chars, size_t length);

extern JSLinearString *
js_NewDependentString(JSContext *cx, JSString *base, size_t start, size_t length);

/* Copy a counted string and GC-allocate a descriptor for it. */
template <js::AllowGC allowGC>
extern JSFlatString *
js_NewStringCopyN(js::ExclusiveContext *cx, const jschar *s, size_t n);

template <js::AllowGC allowGC>
extern JSFlatString *
js_NewStringCopyN(js::ThreadSafeContext *cx, const char *s, size_t n);

/* Copy a C string and GC-allocate a descriptor for it. */
template <js::AllowGC allowGC>
extern JSFlatString *
js_NewStringCopyZ(js::ExclusiveContext *cx, const jschar *s);

template <js::AllowGC allowGC>
extern JSFlatString *
js_NewStringCopyZ(js::ThreadSafeContext *cx, const char *s);

/*
 * Convert a value to a printable C string.
 */
extern const char *
js_ValueToPrintable(JSContext *cx, const js::Value &,
                    JSAutoByteString *bytes, bool asSource = false);

namespace js {

/*
 * Convert a non-string value to a string, returning null after reporting an
 * error, otherwise returning a new string reference.
 */
template <AllowGC allowGC>
extern JSString *
ToStringSlow(ExclusiveContext *cx, typename MaybeRooted<Value, allowGC>::HandleType arg);

/*
 * Convert the given value to a string.  This method includes an inline
 * fast-path for the case where the value is already a string; if the value is
 * known not to be a string, use ToStringSlow instead.
 */
template <AllowGC allowGC>
static JS_ALWAYS_INLINE JSString *
ToString(JSContext *cx, JS::HandleValue v)
{
#ifdef DEBUG
    if (allowGC) {
        SkipRoot skip(cx, &v);
        MaybeCheckStackRoots(cx);
    }
#endif

    if (v.isString())
        return v.toString();
    return ToStringSlow<allowGC>(cx, v);
}

/*
 * This function implements E-262-3 section 9.8, toString. Convert the given
 * value to a string of jschars appended to the given buffer. On error, the
 * passed buffer may have partial results appended.
 */
inline bool
ValueToStringBuffer(JSContext *cx, const Value &v, StringBuffer &sb);

/*
 * Convert a value to its source expression, returning null after reporting
 * an error, otherwise returning a new string reference.
 */
extern JSString *
ValueToSource(JSContext *cx, HandleValue v);

/*
 * Convert a JSString to its source expression; returns null after reporting an
 * error, otherwise returns a new string reference. No Handle needed since the
 * input is dead after the GC.
 */
extern JSString *
StringToSource(JSContext *cx, JSString *str);

/*
 * Test if strings are equal. The caller can call the function even if str1
 * or str2 are not GC-allocated things.
 */
extern bool
EqualStrings(JSContext *cx, JSString *str1, JSString *str2, bool *result);

/* Use the infallible method instead! */
extern bool
EqualStrings(JSContext *cx, JSLinearString *str1, JSLinearString *str2, bool *result) MOZ_DELETE;

/* EqualStrings is infallible on linear strings. */
extern bool
EqualStrings(JSLinearString *str1, JSLinearString *str2);

/*
 * Return less than, equal to, or greater than zero depending on whether
 * str1 is less than, equal to, or greater than str2.
 */
extern bool
CompareStrings(JSContext *cx, JSString *str1, JSString *str2, int32_t *result);

/*
 * Return true if the string matches the given sequence of ASCII bytes.
 */
extern bool
StringEqualsAscii(JSLinearString *str, const char *asciiBytes);

/* Return true if the string contains a pattern anywhere inside it. */
extern bool
StringHasPattern(const jschar *text, uint32_t textlen,
                 const jschar *pat, uint32_t patlen);

} /* namespace js */

extern size_t
js_strlen(const jschar *s);

extern jschar *
js_strchr(const jschar *s, jschar c);

extern jschar *
js_strchr_limit(const jschar *s, jschar c, const jschar *limit);

static JS_ALWAYS_INLINE void
js_strncpy(jschar *dst, const jschar *src, size_t nelem)
{
    return mozilla::PodCopy(dst, src, nelem);
}

extern jschar *
js_strdup(JSContext *cx, const jschar *s);

namespace js {

/*
 * Inflate bytes in ASCII encoding to jschars. Return null on error, otherwise
 * return the jschar that was malloc'ed. length is updated to the length of the
 * new string (in jschars). A null char is appended, but it is not included in
 * the length.
 */
extern jschar *
InflateString(ThreadSafeContext *cx, const char *bytes, size_t *length);

/*
 * Inflate bytes to JS chars in an existing buffer. 'chars' must be large
 * enough for 'length' jschars. The buffer is NOT null-terminated.
 *
 * charsLength must be be initialized with the destination buffer size and, on
 * return, will contain on return the number of copied chars.
 */
extern bool
InflateStringToBuffer(JSContext *maybecx, const char *bytes, size_t length,
                      jschar *chars, size_t *charsLength);

/*
 * Deflate JS chars to bytes into a buffer. 'bytes' must be large enough for
 * 'length chars. The buffer is NOT null-terminated. The destination length
 * must to be initialized with the buffer size and will contain on return the
 * number of copied bytes.
 */
extern bool
DeflateStringToBuffer(JSContext *cx, const jschar *chars,
                      size_t charsLength, char *bytes, size_t *length);

/*
 * The String.prototype.replace fast-native entry point is exported for joined
 * function optimization in js{interp,tracer}.cpp.
 */
extern bool
str_replace(JSContext *cx, unsigned argc, js::Value *vp);

extern bool
str_fromCharCode(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

extern bool
js_str_toString(JSContext *cx, unsigned argc, js::Value *vp);

extern bool
js_str_charAt(JSContext *cx, unsigned argc, js::Value *vp);

extern bool
js_str_charCodeAt(JSContext *cx, unsigned argc, js::Value *vp);

/*
 * Convert one UCS-4 char and write it into a UTF-8 buffer, which must be at
 * least 6 bytes long.  Return the number of UTF-8 bytes of data written.
 */
extern int
js_OneUcs4ToUtf8Char(uint8_t *utf8Buffer, uint32_t ucs4Char);

namespace js {

extern size_t
PutEscapedStringImpl(char *buffer, size_t size, FILE *fp, JSLinearString *str, uint32_t quote);

extern size_t
PutEscapedStringImpl(char *buffer, size_t bufferSize, FILE *fp, const jschar *chars,
                     size_t length, uint32_t quote);

/*
 * Write str into buffer escaping any non-printable or non-ASCII character
 * using \escapes for JS string literals.
 * Guarantees that a NUL is at the end of the buffer unless size is 0. Returns
 * the length of the written output, NOT including the NUL. Thus, a return
 * value of size or more means that the output was truncated. If buffer
 * is null, just returns the length of the output. If quote is not 0, it must
 * be a single or double quote character that will quote the output.
*/
inline size_t
PutEscapedString(char *buffer, size_t size, JSLinearString *str, uint32_t quote)
{
    size_t n = PutEscapedStringImpl(buffer, size, NULL, str, quote);

    /* PutEscapedStringImpl can only fail with a file. */
    JS_ASSERT(n != size_t(-1));
    return n;
}

inline size_t
PutEscapedString(char *buffer, size_t bufferSize, const jschar *chars, size_t length, uint32_t quote)
{
    size_t n = PutEscapedStringImpl(buffer, bufferSize, NULL, chars, length, quote);

    /* PutEscapedStringImpl can only fail with a file. */
    JS_ASSERT(n != size_t(-1));
    return n;
}

/*
 * Write str into file escaping any non-printable or non-ASCII character.
 * If quote is not 0, it must be a single or double quote character that
 * will quote the output.
*/
inline bool
FileEscapedString(FILE *fp, JSLinearString *str, uint32_t quote)
{
    return PutEscapedStringImpl(NULL, 0, fp, str, quote) != size_t(-1);
}

bool
str_match(JSContext *cx, unsigned argc, Value *vp);

bool
str_search(JSContext *cx, unsigned argc, Value *vp);

bool
str_split(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

extern bool
js_String(JSContext *cx, unsigned argc, js::Value *vp);

#endif /* jsstr_h */
