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

#ifndef jsscan_h___
#define jsscan_h___
/*
 * JS lexical scanner interface.
 */
#include <stddef.h>
#ifdef JSFILE
#include <stdio.h>
#endif
#include "jsopcode.h"
#include "jsprvtd.h"
#include "jspubtd.h"

PR_BEGIN_EXTERN_C

typedef enum JSTokenType {
    TOK_ERROR = -1,                     /* well-known as the only code < EOF */
    TOK_EOF,                            /* end of file */
    TOK_EOL,                            /* end of line */
    TOK_SEMI,                           /* semicolon */
    TOK_LB, TOK_RB,                     /* left and right brackets */
    TOK_LC, TOK_RC,                     /* left and right curlies (braces) */
    TOK_LP, TOK_RP,                     /* left and right parentheses */
    TOK_COMMA,                          /* comma operator */
    TOK_ASSIGN,                         /* assignment ops (= += -= etc.) */
    TOK_HOOK, TOK_COLON,                /* conditional (?:) */
    TOK_OR,                             /* logical or (||) */
    TOK_AND,                            /* logical and (&&) */
    TOK_BITOR,                          /* bitwise-or (|) */
    TOK_BITXOR,                         /* bitwise-xor (^) */
    TOK_BITAND,                         /* bitwise-and (&) */
    TOK_EQOP,                           /* equality ops (== !=) */
    TOK_RELOP,                          /* relational ops (< <= > >=) */
    TOK_SHOP,                           /* shift ops (<< >> >>>) */
    TOK_PLUS,                           /* plus */
    TOK_MINUS,                          /* minus */
    TOK_STAR, TOK_DIVOP,                /* multiply/divide ops (* / %) */
    TOK_UNARYOP,                        /* unary prefix operator */
    TOK_INC, TOK_DEC,                   /* increment/decrement (++ --) */
    TOK_DOT,                            /* member operator (.) */
    TOK_NAME,                           /* identifier */
    TOK_NUMBER,                         /* numeric constant */
    TOK_STRING,                         /* string constant */
    TOK_OBJECT,                         /* RegExp or other object constant */
    TOK_PRIMARY,                        /* true, false, null, this, super */
    TOK_FUNCTION,                       /* function keyword */
    TOK_EXPORT, TOK_IMPORT,             /* export keyword */
    TOK_IF,                             /* if keyword */
    TOK_ELSE,                           /* else keyword */
    TOK_SWITCH,                         /* switch keyword */
    TOK_CASE,                           /* case keyword */
    TOK_DEFAULT,                        /* default keyword */
    TOK_WHILE,                          /* while keyword */
    TOK_DO,                             /* do keyword */
    TOK_FOR,                            /* for keyword */
    TOK_BREAK,                          /* break keyword */
    TOK_CONTINUE,                       /* continue keyword */
    TOK_IN,                             /* in keyword */
    TOK_VAR,                            /* var keyword */
    TOK_WITH,                           /* with keyword */
    TOK_RETURN,                         /* return keyword */
    TOK_NEW,                            /* new keyword */
    TOK_DELETE,                         /* delete keyword */
    TOK_DEFSHARP, TOK_USESHARP,         /* #n=, #n# for object/array literals */
    TOK_RESERVED,                       /* reserved keywords */
    TOK_LIMIT                           /* domain size */
} JSTokenType;

#define IS_PRIMARY_TOKEN(tt) (TOK_NAME <= (tt) && (tt) <= TOK_PRIMARY)

struct JSToken {
    JSTokenType         type;           /* char value or above enumerator */
    jschar              *ptr;           /* beginning of token in line buffer */
    union {
        JSAtom          *atom;          /* atom table entry */
        jsdouble        dval;           /* floating point number */
        JSOp            op;             /* operator, for minimal parser */
    } u;
};

typedef struct JSTokenBuf {
    jschar              *base;          /* base of line or stream buffer */
    jschar              *limit;         /* limit for quick bounds check */
    jschar              *ptr;           /* next char to get, or slot to use */
} JSTokenBuf;

#define JS_LINE_LIMIT   256             /* logical line buffer size limit --
                                           physical line length is unlimited */

struct JSTokenStream {
    JSToken             token;          /* last token scanned */
    JSToken             pushback;       /* pushed-back already-scanned token */
    uint16              newlines;       /* newlines skipped during lookahead */
    uint16              flags;          /* flags -- see below */
    uint16              lineno;         /* current line number */
    uintN               ungetpos;       /* next free char slot in ungetbuf */
    jschar              ungetbuf[4];    /* at most 4, for \uXXXX lookahead */
    JSTokenBuf          linebuf;        /* line buffer for diagnostics */
    JSTokenBuf          userbuf;        /* user input buffer if !file */
    JSTokenBuf          tokenbuf;       /* current token string buffer */
    const char          *filename;      /* input filename or null */
#ifdef JSFILE
    FILE                *file;          /* stdio stream if reading from file */
#endif
    JSPrincipals        *principals;    /* principals associated with given input */
};

/* JSTokenStream flags */
#define TSF_ERROR       0x0001          /* fatal error while scanning */
#define TSF_EOF         0x0002          /* hit end of file */
#define TSF_NEWLINES    0x0004          /* tokenize newlines */
#define TSF_FUNCTION    0x0008          /* scanning inside function body */
#define TSF_RETURN_EXPR 0x0010          /* function has 'return expr;' */
#define TSF_RETURN_VOID 0x0020          /* function has 'return;' */
#define TSF_INTERACTIVE 0x0040          /* interactive parsing mode */
#define TSF_COMMAND     0x0080          /* command parsing mode */
#define TSF_LOOKAHEAD   0x0100          /* looking ahead for a token */
#define TSF_REGEXP      0x0200          /* looking for a regular expression */

/*
 * At most one non-EOF token can be pushed back onto a TokenStream between
 * Get or successful Match operations.  These macros manipulate the pushback
 * token to clear it when changing scanning modes (upon initialzation, after
 * errors, or between newline-sensitive and insensitive states).
 */
#define CLEAR_PUSHBACK(ts)  ((ts)->pushback.type = TOK_EOF)
#define SCAN_NEWLINES(ts)   ((ts)->flags |= TSF_NEWLINES)
#define HIDE_NEWLINES(ts)                                                     \
    PR_BEGIN_MACRO                                                            \
	(ts)->flags &= ~TSF_NEWLINES;                                         \
	if ((ts)->pushback.type == TOK_EOL)                                   \
	    (ts)->pushback.type = TOK_EOF;                                    \
    PR_END_MACRO

/*
 * Return the current line number by subtracted newlines skipped during
 * lookahead that was pushed back and has yet to be rescanned.
 */
#define TRUE_LINENO(ts) ((ts)->lineno - (ts)->newlines)

/*
 * Create a new token stream, either from an input buffer or from a file.
 * Return null on file-open or memory-allocation failure.
 *
 * NB: Both js_New{Buffer,File}TokenStream() return a pointer to transient
 * memory in the current context's temp pool.  This memory is deallocated via
 * PR_ARENA_RELEASE() after parsing is finished.
 */
extern JSTokenStream *
js_NewTokenStream(JSContext *cx, const jschar *base, size_t length,
		  const char *filename, uintN lineno, JSPrincipals *principals);

extern JS_FRIEND_API(JSTokenStream *)
js_NewBufferTokenStream(JSContext *cx, const jschar *base, size_t length);

extern JS_FRIEND_API(JSTokenStream *)
js_NewFileTokenStream(JSContext *cx, const char *filename);

extern JS_FRIEND_API(JSBool)
js_CloseTokenStream(JSContext *cx, JSTokenStream *ts);

/*
 * Initialize the scanner, installing JS keywords into cx's global scope.
 */
extern JSBool
js_InitScanner(JSContext *cx);

/*
 * Friend-exported API entry point to call a mapping function on each reserved
 * identifier in the scanner's keyword table.
 */
extern JS_FRIEND_API(void)
js_MapKeywords(void (*mapfun)(const char *));

/*
 * Report an error found while scanning ts to a window or other output device
 * associated with cx.
 */
extern void
js_ReportCompileError(JSContext *cx, JSTokenStream *ts, const char *format,
		      ...);

/*
 * Look ahead one token and return its type.
 */
extern JSTokenType
js_PeekToken(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg);

extern JSTokenType
js_PeekTokenSameLine(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg);

/*
 * Get the next token from ts.
 */
extern JSTokenType
js_GetToken(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg);

/*
 * Push back the last scanned token onto ts.
 */
extern void
js_UngetToken(JSTokenStream *ts);

/*
 * Get the next token from ts if its type is tt.
 */
extern JSBool
js_MatchToken(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg,
	      JSTokenType tt);

/*
 * Emit pending newline source notes counted while (mis-)matching lookahead.
 */
extern JSBool
js_FlushNewlines(JSContext *cx, JSTokenStream *ts, JSCodeGenerator *cg);

PR_END_EXTERN_C

#endif /* jsscan_h___ */
