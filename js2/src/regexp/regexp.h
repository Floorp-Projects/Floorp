/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
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


typedef enum RE_Flags {
    RE_IGNORECASE = 0x1,
    RE_GLOBAL = 0x2,
    RE_MULTILINE = 0x4
} RE_Flags;

typedef enum RE_Version {
    RE_VERSION_1,              /* octal literal support */
    RE_VERSION_2
} RE_Version;

typedef enum RE_Error {
    RE_NO_ERROR,
    RE_TRAILING_SLASH,         /* a backslash just before the end of the RE */
    RE_UNCLOSED_PAREN,         /* mis-matched parens */
    RE_UNCLOSED_BRACKET,       /* mis-matched parens */
    RE_UNCLOSED_CLASS,         /* '[' missing ']' */
    RE_BACKREF_IN_CLASS,       /* used '\<digit>' in '[..]' */
    RE_BAD_FLAG,               /* unrecognized flag (not i, g or m) */
    RE_WRONG_RANGE,            /* range lo > range hi */
    RE_OUT_OF_MEMORY
} RE_Error;

typedef struct RENode RENode;

typedef struct RECharSet {
    REuint8 *bits;
    REuint32 length;
    unsigned char sense;
} RECharSet;


typedef struct REState {
    REchar *srcStart;           /* copy of source text */
    REchar *src;                /* current parse position */
    REchar *srcEnd;             /* end of source text */
    REuint32 parenCount;        /* # capturing parens */
    REuint32 flags;             /* union of flags from regexp */
    RE_Version version;         
    RE_Error error;             /* parse-time or runtime error */
    REuint32 classCount;        /* number of contained []'s */
    RECharSet *classList;       /* data for []'s */
    RENode *result;             /* head of result tree */
    REint32 codeLength;         /* length of bytecode */
    REuint8 *pc;                /* start of bytecode */
} REState;

typedef struct RECapture {
    REint32 index;              /* start of contents of this capture, -1 for empty  */
    REint32 length;             /* length of capture */
} RECapture;

typedef struct REMatchState {
    REint32 startIndex;         /* beginning of succesful match */     
    REint32 endIndex;           /* character beyond end of succesful match */
    REint32 parenCount;         /* set to (n - 1), i.e. for /((a)b)/, this field is 1 */
    RECapture parens[1];        /* first of 'n' captures, allocated at end of this struct */
} REMatchState;



/*
 *  Compiles the flags source text into a union of flag values. Returns RE_NO_ERROR
 *  or RE_BAD_FLAG.
 *
 */
RE_Error parseFlags(const REchar *flagsSource, REint32 flagsLength, REuint32 *flags);

/* 
 *  Compiles the RegExp source into a stream of REByteCodes and fills in the REState struct.
 *  Errors are recorded in the state 'error' field and signalled by a NULL return.
 *  The RegExp source does not have any delimiters.
 */
REState *REParse(const REchar *source, REint32 sourceLength, REuint32 flags, RE_Version version);


/*
 *  Execute the RegExp against the supplied text.
 *  The return value is NULL for no match, otherwise an REMatchState struct.
 *
 */
REMatchState *REExecute(REState *pState, const REchar *text, REint32 offset, REint32 length, int globalMulitline);


/*
 *  The [[Match]] implementation, applies the regexp at the start of the text
 *  only (i.e. it does not search repeatedly through the text for a match).
 *  NULL return for no match.
 *
 */
REMatchState *REMatch(REState *pState, const REchar *text, REint32 length);



/*
 *  Throw away the RegExp and all data associated with it.
 */
void REfreeRegExp(REState *pState);




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

/*
 *  The host should also provide a definition of whitespace to match the following:
 *
 */
#ifndef RE_ISSPACE
#define RE_ISSPACE(c) ( (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r')  || (c == '\v')  || (c == '\f') )
#endif

