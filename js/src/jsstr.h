/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef jsstr_h___
#define jsstr_h___
/*
 * JS string type implementation.
 *
 * A JS string is a counted array of unicode characters.  To support handoff
 * of API client memory, the chars are allocated separately from the length,
 * necessitating a pointer after the count, to form a string header.  String
 * headers are GC'ed while string bytes are allocated from the malloc heap.
 *
 * When a string is treated as an object (by following it with . or []), the
 * runtime wraps it with a JSObject whose valueOf method returns the unwrapped
 * string header.
 */
#include <ctype.h>
#include "jspubtd.h"
#include "jsprvtd.h"

PR_BEGIN_EXTERN_C

struct JSString {
    size_t          length;
    jschar          *chars;
};

struct JSSubString {
    size_t          length;
    const jschar    *chars;
};

extern jschar      js_empty_ucstr[];
extern JSSubString js_EmptySubString;

/* Unicode character attribute lookup tables. */
extern const uint8 js_X[];
extern const uint8 js_Y[];
extern const uint32 js_A[];

/* Enumerated Unicode general category types. */
typedef enum JSCharType {
    JSCT_UNASSIGNED             = 0,
    JSCT_UPPERCASE_LETTER       = 1,
    JSCT_LOWERCASE_LETTER       = 2,
    JSCT_TITLECASE_LETTER       = 3,
    JSCT_MODIFIER_LETTER        = 4,
    JSCT_OTHER_LETTER           = 5,
    JSCT_NON_SPACING_MARK       = 6,
    JSCT_ENCLOSING_MARK         = 7,
    JSCT_COMBINING_SPACING_MARK = 8,
    JSCT_DECIMAL_DIGIT_NUMBER   = 9,
    JSCT_LETTER_NUMBER          = 10,
    JSCT_OTHER_NUMBER           = 11,
    JSCT_SPACE_SEPARATOR        = 12,
    JSCT_LINE_SEPARATOR         = 13,
    JSCT_PARAGRAPH_SEPARATOR    = 14,
    JSCT_CONTROL                = 15,
    JSCT_FORMAT                 = 16,
    JSCT_PRIVATE_USE            = 18,
    JSCT_SURROGATE              = 19,
    JSCT_DASH_PUNCTUATION       = 20,
    JSCT_START_PUNCTUATION      = 21,
    JSCT_END_PUNCTUATION        = 22,
    JSCT_CONNECTOR_PUNCTUATION  = 23,
    JSCT_OTHER_PUNCTUATION      = 24,
    JSCT_MATH_SYMBOL            = 25,
    JSCT_CURRENCY_SYMBOL        = 26,
    JSCT_MODIFIER_SYMBOL        = 27,
    JSCT_OTHER_SYMBOL           = 28
} JSCharType;

/* Character classifying and mapping macros, based on java.lang.Character. */
#define JS_CCODE(c)     (js_A[js_Y[(js_X[(uint16)(c)>>6]<<6)|((c)&0x3F)]])
#define JS_CTYPE(c)     (JS_CCODE(c) & 0x1F)

#define JS_ISALPHA(c)   ((((1 << JSCT_UPPERCASE_LETTER) |                     \
			   (1 << JSCT_LOWERCASE_LETTER) |                     \
			   (1 << JSCT_TITLECASE_LETTER) |                     \
			   (1 << JSCT_MODIFIER_LETTER) |                      \
			   (1 << JSCT_OTHER_LETTER))                          \
			  >> JS_CTYPE(c)) & 1)

#define JS_ISALNUM(c)   ((((1 << JSCT_UPPERCASE_LETTER) |                     \
			   (1 << JSCT_LOWERCASE_LETTER) |                     \
			   (1 << JSCT_TITLECASE_LETTER) |                     \
			   (1 << JSCT_MODIFIER_LETTER) |                      \
			   (1 << JSCT_OTHER_LETTER) |                         \
			   (1 << JSCT_DECIMAL_DIGIT_NUMBER))                  \
			  >> JS_CTYPE(c)) & 1)

#define JS_ISWORD(c)    (JS_ISALNUM(c) || (c) == '_')

/* XXXbe unify on A/X/Y tbls, avoid ctype.h? */
#define JS_ISIDENT(c)   ((c) < 128 && (isalpha(c) || (c) == '_' || (c) == '$'))
#define JS_ISIDENT2(c)  ((c) < 128 && (isalnum(c) || (c) == '_' || (c) == '$'))

#define JS_ISDIGIT(c)   (JS_CTYPE(c) == JSCT_DECIMAL_DIGIT_NUMBER)

/* XXXbe fs, etc. ? */
#define JS_ISSPACE(c)   ((JS_CCODE(c) & 0x00070000) == 0x00040000)
#define JS_ISPRINT(c)   ((c) < 128 && isprint(c))

#define JS_ISUPPER(c)   (JS_CTYPE(c) == JSCT_UPPERCASE_LETTER)
#define JS_ISLOWER(c)   (JS_CTYPE(c) == JSCT_LOWERCASE_LETTER)

#ifdef __GNUC__

#define JS_TOUPPER(c)   ({uint32 _v = JS_CCODE(c);                            \
			  (_v & 0x00100000) ? (c) - ((int32)_v >> 22) : (c);})
#define JS_TOLOWER(c)   ({uint32 _v = JS_CCODE(c);                            \
			  (_v & 0x00200000) ? (c) + ((int32)_v >> 22) : (c);})

#else  /* !__GNUC__ */

#define JS_TOUPPER(c)   js_ToUpper((jschar)c)
#define JS_TOLOWER(c)   js_ToLower((jschar)c)

extern jschar js_ToUpper(jschar c);
extern jschar js_ToLower(jschar c);

#endif /* !__GNUC__ */

#define JS_TOCTRL(c)    ((c) ^ 64)      /* XXX unsafe! requires uppercase c */

/* Shorthands for ASCII (7-bit) decimal and hex conversion. */
#define JS7_ISDEC(c)    ((c) < 128 && isdigit(c))
#define JS7_UNDEC(c)    ((c) - '0')
#define JS7_ISHEX(c)    ((c) < 128 && isxdigit(c))
#define JS7_UNHEX(c)    (uintN)(isdigit(c) ? (c) - '0' : 10 + tolower(c) - 'a')
#define JS7_ISLET(c)    ((c) < 128 && isalpha(c))

/* Initialize truly global state associated with JS strings. */
extern JSBool
js_InitStringGlobals(void);

extern void
js_FreeStringGlobals(void);

/* Initialize the String class, returning its prototype object. */
extern JSObject *
js_InitStringClass(JSContext *cx, JSObject *obj);

/* GC-allocate a string descriptor for the given malloc-allocated chars. */
extern JSString *
js_NewString(JSContext *cx, jschar *chars, size_t length, uintN gcflag);

/* Copy a counted string and GC-allocate a descriptor for it. */
extern JSString *
js_NewStringCopyN(JSContext *cx, const jschar *s, size_t n, uintN gcflag);

/* Copy a C string and GC-allocate a descriptor for it. */
extern JSString *
js_NewStringCopyZ(JSContext *cx, const jschar *s, uintN gcflag);

/* Free the chars held by str when it is finalized by the GC. */
extern void
js_FinalizeString(JSContext *cx, JSString *str);

/* Wrap a string value in a String object. */
extern JSObject *
js_StringToObject(JSContext *cx, JSString *str);

/*
 * Convert a value to a string, returning null after reporting an error,
 * otherwise returning a new string reference.
 */
extern JSString *
js_ValueToString(JSContext *cx, jsval v);

/*
 * Convert a value to its source expression, returning null after reporting
 * an error, otherwise returning a new string reference.
 */
extern JSString *
js_ValueToSource(JSContext *cx, jsval v);

#ifdef HT_ENUMERATE_NEXT	/* XXX don't require prhash.h */
/*
 * Compute a hash function from str.
 */
extern PRHashNumber
js_HashString(const JSString *str);
#endif

/*
 * Return less than, equal to, or greater than zero depending on whether
 * str1 is less than, equal to, or greater than str2.
 */
extern intN
js_CompareStrings(const JSString *str1, const JSString *str2);

/*
 * Boyer-Moore-Horspool superlinear search for pat:patlen in text:textlen.
 * The patlen argument must be positive and no greater than BMH_PATLEN_MAX.
 * The start argument tells where in text to begin the search.
 *
 * Return the index of pat in text, or -1 if not found.
 */
#define BMH_CHARSET_SIZE 256    /* ISO-Latin-1 */
#define BMH_PATLEN_MAX   255    /* skip table element is uint8 */

#define BMH_BAD_PATTERN  (-2)   /* return value if pat is not ISO-Latin-1 */

extern jsint
js_BoyerMooreHorspool(const jschar *text, jsint textlen,
		      const jschar *pat, jsint patlen,
		      jsint start);

extern size_t
js_strlen(const jschar *s);

extern jschar *
js_strchr(const jschar *s, jschar c);

extern jschar *
js_strncpy(jschar *t, const jschar *s, size_t n);

/*
 * Inflate bytes to JS chars and vice versa.  Report out of memory via cx
 * and return null on error, otherwise return the jschar or byte vector that
 * was JS_malloc'ed.
 */
extern jschar *
js_InflateString(JSContext *cx, const char *bytes, size_t length);

extern char *
js_DeflateString(JSContext *cx, const jschar *chars, size_t length);

/*
 * Associate bytes with str in the deflated string cache, returning true on
 * successful association, false on out of memory.
 */
extern JSBool
js_SetStringBytes(JSString *str, char *bytes, size_t length);

/*
 * Find or create a deflated string cache entry for str that contains its
 * characters chopped from Unicode code points into bytes.
 */
extern char *
js_GetStringBytes(JSString *str);

PR_END_EXTERN_C

#endif /* jsstr_h___ */
