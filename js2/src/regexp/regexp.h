/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the JavaScript 2 Prototype.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

#ifdef __GNUC__
 /* GCC's wchar_t is 32 bits, so we can't use it. */
 typedef uint16 char16;
 typedef uint16 uchar16;
#else
 typedef wchar_t char16;
 typedef wchar_t uchar16;
#endif
typedef char16 REchar;

typedef unsigned int REuint32;
typedef int REint32;
typedef unsigned char REuint8;

#ifdef STANDALONE
typedef unsigned char REbool;
enum { false, true };
#else
typedef bool REbool;
#endif

typedef enum RE_FLAGS {
    MULTILINE = 0x1,
    IGNORECASE = 0x2,
    GLOBAL = 0x4
} RE_FLAGS;

typedef enum REError {
    NO_ERROR,
    TRAILING_SLASH,         /* an backslash just before the end of the RE */
    UNCLOSED_PAREN,         /* mis-matched parens */
    UNCLOSED_BRACKET,       /* mis-matched parens */
    UNCLOSED_CLASS,         /* '[' missing ']' */
    BACKREF_IN_CLASS,       /* used '\<digit>' in '[..]' */
    BAD_FLAG,               /* unrecognized flag (not i, g or m) */
    WRONG_RANGE,            /* range lo > range hi */
    OUT_OF_MEMORY
} REError;

typedef struct RENode RENode;

typedef struct CharSet {
    REuint8 *bits;
    REuint32 length;
    REbool sense;
} CharSet;


typedef struct REParseState {
    REchar *srcStart;           /* copy of source text */
    REchar *src;                /* current parse position */
    REchar *srcEnd;             /* end of source text */
    RENode *result;             /* head of result tree */
    REuint32 parenCount;        /* # capturing parens */
    REuint32 flags;             /* flags from regexp */
    REint32 lastIndex;          /* position from last match (for 'g' flag) */
    REbool oldSyntax;           /* backward compatibility for octal chars */
    REError error;              /* parse-time error */
    REuint32 classCount;        /* number of contained []'s */
    CharSet *classList;         /* data for []'s */
    REint32 codeLength;         /* length of bytecode */
    REuint8 *pc;                /* start of bytecode */
} REParseState;

typedef struct RECapture {
    REint32 index;              /* start of contents of this capture, -1 for empty  */
    REint32 length;             /* length of capture */
} RECapture;

typedef struct REState {
    REint32 startIndex;           
    REint32 endIndex;
    REint32 n;                  /* set to (n - 1), i.e. for /((a)b)/, this field is 1 */
    RECapture parens[1];        /* first of 'n' captures, allocated at end of this struct */
} REState;


/* 
 *  Compiles the RegExp source into a tree of RENodes in the returned
 *  parse state result field. Errors are flagged via the error reporter
 *  function and signalled via a NULL return.
 *  The RegExp source does not have any delimiters and the flag string is
 *  to be supplied separately (can be NULL, with a 0 length)
 */
REParseState *REParse(const REchar *source, REint32 sourceLength, const REchar *flags, REint32 flagsLength, REbool oldSyntax);

/*
 *  The [[Match]] implementation
 *
 */
REState *REMatch(REParseState *parseState, const REchar *text, REint32 length);

/*
 *  Execute the RegExp against the supplied text, filling in the REState.
 *
 *
 */
REState *REExecute(REParseState *parseState, const REchar *text, REint32 length);


/*
 *  Throw away the RegExp and all data associated with it.
 */
void freeRegExp(REParseState *parseState);

/*
 *  Needs to be provided by the host, following these specs:
 *
 *
 * [1. If IgnoreCase is false, return ch. - not necessary in implementation]
 *
 * 2. Let u be ch converted to upper case as if by calling 
 *    String.prototype.toUpperCase on the one-character string ch.
 * 3. If u does not consist of a single character, return ch.
 * 4. Let cu be u's character.
 * 5. If ch's code point value is greater than or equal to decimal 128 and cu's
 *    code point value is less than decimal 128, then return ch.
 * 6. Return cu.
 */
extern REchar canonicalize(REchar ch);
