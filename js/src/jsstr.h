/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsstr_h___
#define jsstr_h___

#include "mozilla/PodOperations.h"

#include <ctype.h>
#include "jsapi.h"
#include "jsatom.h"
#include "jslock.h"
#include "jsutil.h"

#include "js/HashTable.h"
#include "vm/Unicode.h"

class JSFlatString;
class JSStableString;

namespace js {

/* Implemented in jsstrinlines.h */
class StringBuffer;

/*
 * When an algorithm does not need a string represented as a single linear
 * array of characters, this range utility may be used to traverse the string a
 * sequence of linear arrays of characters. This avoids flattening ropes.
 *
 * Implemented in jsstrinlines.h.
 */
class StringSegmentRange;
class MutatingRopeSegmentRange;

/*
 * Utility for building a rope (lazy concatenation) of strings.
 */
class RopeBuilder;

template <AllowGC allowGC>
extern JSString *
ConcatStrings(JSContext *cx,
              typename MaybeRooted<JSString*, allowGC>::HandleType left,
              typename MaybeRooted<JSString*, allowGC>::HandleType right);

}  /* namespace js */

extern JSString * JS_FASTCALL
js_toLowerCase(JSContext *cx, JSString *str);

extern JSString * JS_FASTCALL
js_toUpperCase(JSContext *cx, JSString *str);

struct JSSubString {
    size_t          length;
    const jschar    *chars;
};

extern jschar      js_empty_ucstr[];
extern JSSubString js_EmptySubString;

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
js_NewString(JSContext *cx, jschar *chars, size_t length);

extern JSLinearString *
js_NewDependentString(JSContext *cx, JSString *base, size_t start, size_t length);

/* Copy a counted string and GC-allocate a descriptor for it. */
template <js::AllowGC allowGC>
extern JSFlatString *
js_NewStringCopyN(JSContext *cx, const jschar *s, size_t n);

template <js::AllowGC allowGC>
extern JSFlatString *
js_NewStringCopyN(JSContext *cx, const char *s, size_t n);

/* Copy a C string and GC-allocate a descriptor for it. */
template <js::AllowGC allowGC>
extern JSFlatString *
js_NewStringCopyZ(JSContext *cx, const jschar *s);

template <js::AllowGC allowGC>
extern JSFlatString *
js_NewStringCopyZ(JSContext *cx, const char *s);

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
ToStringSlow(JSContext *cx, const Value &v);

/*
 * Convert the given value to a string.  This method includes an inline
 * fast-path for the case where the value is already a string; if the value is
 * known not to be a string, use ToStringSlow instead.
 */
template <AllowGC allowGC>
static JS_ALWAYS_INLINE JSString *
ToString(JSContext *cx, const js::Value &v)
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

} /* namespace js */

namespace js {
/*
 * Convert a value to its source expression, returning null after reporting
 * an error, otherwise returning a new string reference.
 */
extern JSString *
ValueToSource(JSContext *cx, const js::Value &v);

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
 * new string (in jschars).
 */
extern jschar *
InflateString(JSContext *cx, const char *bytes, size_t *length);

/*
 * Inflate bytes in UTF-8 encoding to jschars. Return null on error, otherwise
 * return the jschar vector that was malloc'ed. length is updated to the length
 * of the new string (in jschars).
 */
extern jschar *
InflateUTF8String(JSContext *cx, const char *bytes, size_t *length);

/*
 * Inflate bytes to JS chars in an existing buffer. 'chars' must be large
 * enough for 'length' jschars. The buffer is NOT null-terminated.
 *
 * charsLength must be be initialized with the destination buffer size and, on
 * return, will contain on return the number of copied chars.
 */
extern bool
InflateStringToBuffer(JSContext *cx, const char *bytes, size_t length,
                      jschar *chars, size_t *charsLength);

extern bool
InflateUTF8StringToBuffer(JSContext *cx, const char *bytes, size_t length,
                          jschar *chars, size_t *charsLength);

/*
 * The same as InflateUTF8StringToBuffer(), except that any malformed UTF-8
 * characters will be replaced by \uFFFD. No exception will be thrown for
 * malformed UTF-8 input.
 */
extern bool
InflateUTF8StringToBufferReplaceInvalid(JSContext *cx, const char *bytes,
                                        size_t length, jschar *chars,
                                        size_t *charsLength);

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
extern JSBool
str_replace(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
str_fromCharCode(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

extern JSBool
js_str_toString(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
js_str_charAt(JSContext *cx, unsigned argc, js::Value *vp);

extern JSBool
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

JSBool
str_match(JSContext *cx, unsigned argc, Value *vp);

JSBool
str_search(JSContext *cx, unsigned argc, Value *vp);

JSBool
str_split(JSContext *cx, unsigned argc, Value *vp);

} /* namespace js */

extern JSBool
js_String(JSContext *cx, unsigned argc, js::Value *vp);

#endif /* jsstr_h___ */
