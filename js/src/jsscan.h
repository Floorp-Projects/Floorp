/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsscan_h___
#define jsscan_h___
/*
 * JS lexical scanner interface.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include "jsversion.h"
#include "jsopcode.h"
#include "jsprvtd.h"
#include "jspubtd.h"
#include "jsvector.h"

#define JS_KEYWORD(keyword, type, op, version) \
    extern const char js_##keyword##_str[];
#include "jskeyword.tbl"
#undef JS_KEYWORD

namespace js {

enum TokenKind {
    TOK_ERROR = -1,                     /* well-known as the only code < EOF */
    TOK_EOF = 0,                        /* end of file */
    TOK_EOL = 1,                        /* end of line */
    TOK_SEMI = 2,                       /* semicolon */
    TOK_COMMA = 3,                      /* comma operator */
    TOK_ASSIGN = 4,                     /* assignment ops (= += -= etc.) */
    TOK_HOOK = 5, TOK_COLON = 6,        /* conditional (?:) */
    TOK_OR = 7,                         /* logical or (||) */
    TOK_AND = 8,                        /* logical and (&&) */
    TOK_BITOR = 9,                      /* bitwise-or (|) */
    TOK_BITXOR = 10,                    /* bitwise-xor (^) */
    TOK_BITAND = 11,                    /* bitwise-and (&) */
    TOK_EQOP = 12,                      /* equality ops (== !=) */
    TOK_RELOP = 13,                     /* relational ops (< <= > >=) */
    TOK_SHOP = 14,                      /* shift ops (<< >> >>>) */
    TOK_PLUS = 15,                      /* plus */
    TOK_MINUS = 16,                     /* minus */
    TOK_STAR = 17, TOK_DIVOP = 18,      /* multiply/divide ops (* / %) */
    TOK_UNARYOP = 19,                   /* unary prefix operator */
    TOK_INC = 20, TOK_DEC = 21,         /* increment/decrement (++ --) */
    TOK_DOT = 22,                       /* member operator (.) */
    TOK_LB = 23, TOK_RB = 24,           /* left and right brackets */
    TOK_LC = 25, TOK_RC = 26,           /* left and right curlies (braces) */
    TOK_LP = 27, TOK_RP = 28,           /* left and right parentheses */
    TOK_NAME = 29,                      /* identifier */
    TOK_NUMBER = 30,                    /* numeric constant */
    TOK_STRING = 31,                    /* string constant */
    TOK_REGEXP = 32,                    /* RegExp constant */
    TOK_PRIMARY = 33,                   /* true, false, null, this, super */
    TOK_FUNCTION = 34,                  /* function keyword */
    TOK_IF = 35,                        /* if keyword */
    TOK_ELSE = 36,                      /* else keyword */
    TOK_SWITCH = 37,                    /* switch keyword */
    TOK_CASE = 38,                      /* case keyword */
    TOK_DEFAULT = 39,                   /* default keyword */
    TOK_WHILE = 40,                     /* while keyword */
    TOK_DO = 41,                        /* do keyword */
    TOK_FOR = 42,                       /* for keyword */
    TOK_BREAK = 43,                     /* break keyword */
    TOK_CONTINUE = 44,                  /* continue keyword */
    TOK_IN = 45,                        /* in keyword */
    TOK_VAR = 46,                       /* var keyword */
    TOK_WITH = 47,                      /* with keyword */
    TOK_RETURN = 48,                    /* return keyword */
    TOK_NEW = 49,                       /* new keyword */
    TOK_DELETE = 50,                    /* delete keyword */
    TOK_DEFSHARP = 51,                  /* #n= for object/array initializers */
    TOK_USESHARP = 52,                  /* #n# for object/array initializers */
    TOK_TRY = 53,                       /* try keyword */
    TOK_CATCH = 54,                     /* catch keyword */
    TOK_FINALLY = 55,                   /* finally keyword */
    TOK_THROW = 56,                     /* throw keyword */
    TOK_INSTANCEOF = 57,                /* instanceof keyword */
    TOK_DEBUGGER = 58,                  /* debugger keyword */
    TOK_XMLSTAGO = 59,                  /* XML start tag open (<) */
    TOK_XMLETAGO = 60,                  /* XML end tag open (</) */
    TOK_XMLPTAGC = 61,                  /* XML point tag close (/>) */
    TOK_XMLTAGC = 62,                   /* XML start or end tag close (>) */
    TOK_XMLNAME = 63,                   /* XML start-tag non-final fragment */
    TOK_XMLATTR = 64,                   /* XML quoted attribute value */
    TOK_XMLSPACE = 65,                  /* XML whitespace */
    TOK_XMLTEXT = 66,                   /* XML text */
    TOK_XMLCOMMENT = 67,                /* XML comment */
    TOK_XMLCDATA = 68,                  /* XML CDATA section */
    TOK_XMLPI = 69,                     /* XML processing instruction */
    TOK_AT = 70,                        /* XML attribute op (@) */
    TOK_DBLCOLON = 71,                  /* namespace qualified name op (::) */
    TOK_ANYNAME = 72,                   /* XML AnyName singleton (*) */
    TOK_DBLDOT = 73,                    /* XML descendant op (..) */
    TOK_FILTER = 74,                    /* XML filtering predicate op (.()) */
    TOK_XMLELEM = 75,                   /* XML element node type (no token) */
    TOK_XMLLIST = 76,                   /* XML list node type (no token) */
    TOK_YIELD = 77,                     /* yield from generator function */
    TOK_ARRAYCOMP = 78,                 /* array comprehension initialiser */
    TOK_ARRAYPUSH = 79,                 /* array push within comprehension */
    TOK_LEXICALSCOPE = 80,              /* block scope AST node label */
    TOK_LET = 81,                       /* let keyword */
    TOK_SEQ = 82,                       /* synthetic sequence of statements,
                                           not a block */
    TOK_FORHEAD = 83,                   /* head of for(;;)-style loop */
    TOK_ARGSBODY = 84,                  /* formal args in list + body at end */
    TOK_UPVARS = 85,                    /* lexical dependencies as JSAtomList
                                           of definitions paired with a parse
                                           tree full of uses of those names */
    TOK_RESERVED,                       /* reserved keywords */
    TOK_STRICT_RESERVED,                /* reserved keywords in strict mode */
    TOK_LIMIT                           /* domain size */
};

static inline bool
TokenKindIsXML(TokenKind tt)
{
    return tt == TOK_AT || tt == TOK_DBLCOLON || tt == TOK_ANYNAME;
}

static inline bool
TreeTypeIsXML(TokenKind tt)
{
    return tt == TOK_XMLCOMMENT || tt == TOK_XMLCDATA || tt == TOK_XMLPI ||
           tt == TOK_XMLELEM || tt == TOK_XMLLIST;
}

static inline bool
TokenKindIsDecl(TokenKind tt)
{
#if JS_HAS_BLOCK_SCOPE
    return tt == TOK_VAR || tt == TOK_LET;
#else
    return tt == TOK_VAR;
#endif
}

struct TokenPtr {
    uint32              index;          /* index of char in physical line */
    uint32              lineno;         /* physical line number */

    bool operator==(const TokenPtr& bptr) {
        return index == bptr.index && lineno == bptr.lineno;
    }

    bool operator!=(const TokenPtr& bptr) {
        return index != bptr.index || lineno != bptr.lineno;
    }

    bool operator <(const TokenPtr& bptr) {
        return lineno < bptr.lineno ||
               (lineno == bptr.lineno && index < bptr.index);
    }

    bool operator <=(const TokenPtr& bptr) {
        return lineno < bptr.lineno ||
               (lineno == bptr.lineno && index <= bptr.index);
    }

    bool operator >(const TokenPtr& bptr) {
        return !(*this <= bptr);
    }

    bool operator >=(const TokenPtr& bptr) {
        return !(*this < bptr);
    }
};

struct TokenPos {
    TokenPtr          begin;          /* first character and line of token */
    TokenPtr          end;            /* index 1 past last char, last line */

    bool operator==(const TokenPos& bpos) {
        return begin == bpos.begin && end == bpos.end;
    }

    bool operator!=(const TokenPos& bpos) {
        return begin != bpos.begin || end != bpos.end;
    }

    bool operator <(const TokenPos& bpos) {
        return begin < bpos.begin;
    }

    bool operator <=(const TokenPos& bpos) {
        return begin <= bpos.begin;
    }

    bool operator >(const TokenPos& bpos) {
        return !(*this <= bpos);
    }

    bool operator >=(const TokenPos& bpos) {
        return !(*this < bpos);
    }
};

struct Token {
    TokenKind           type;           /* char value or above enumerator */
    TokenPos            pos;            /* token position in file */
    jschar              *ptr;           /* beginning of token in line buffer */
    union {
        struct {                        /* name or string literal */
            JSOp        op;             /* operator, for minimal parser */
            JSAtom      *atom;          /* atom table entry */
        } s;
        uintN           reflags;        /* regexp flags, use tokenbuf to access
                                           regexp chars */
        struct {                        /* atom pair, for XML PIs */
            JSAtom      *atom2;         /* auxiliary atom table entry */
            JSAtom      *atom;          /* main atom table entry */
        } p;
        jsdouble        dval;           /* floating point number */
    } u;
};

enum TokenStreamFlags
{
    TSF_ERROR = 0x01,           /* fatal error while compiling */
    TSF_EOF = 0x02,             /* hit end of file */
    TSF_NEWLINES = 0x04,        /* tokenize newlines */
    TSF_OPERAND = 0x08,         /* looking for operand, not operator */
    TSF_UNEXPECTED_EOF = 0x10,  /* unexpected end of input, i.e. TOK_EOF not at top-level. */
    TSF_KEYWORD_IS_NAME = 0x20, /* Ignore keywords and return TOK_NAME instead to the parser. */
    TSF_STRICT_MODE_CODE = 0x40,/* Tokenize as appropriate for strict mode code. */
    TSF_DIRTYLINE = 0x80,       /* non-whitespace since start of line */
    TSF_OWNFILENAME = 0x100,    /* ts->filename is malloc'd */
    TSF_XMLTAGMODE = 0x200,     /* scanning within an XML tag in E4X */
    TSF_XMLTEXTMODE = 0x400,    /* scanning XMLText terminal from E4X */
    TSF_XMLONLYMODE = 0x800,    /* don't scan {expr} within text/tag */
    TSF_OCTAL_CHAR = 0x1000,    /* observed a octal character escape */

    /*
     * To handle the hard case of contiguous HTML comments, we want to clear the
     * TSF_DIRTYINPUT flag at the end of each such comment.  But we'd rather not
     * scan for --> within every //-style comment unless we have to.  So we set
     * TSF_IN_HTML_COMMENT when a <!-- is scanned as an HTML begin-comment, and
     * clear it (and TSF_DIRTYINPUT) when we scan --> either on a clean line, or
     * only if (ts->flags & TSF_IN_HTML_COMMENT), in a //-style comment.
     *
     * This still works as before given a malformed comment hiding hack such as:
     *
     *    <script>
     *      <!-- comment hiding hack #1
     *      code goes here
     *      // --> oops, markup for script-unaware browsers goes here!
     *    </script>
     *
     * It does not cope with malformed comment hiding hacks where --> is hidden
     * by C-style comments, or on a dirty line.  Such cases are already broken.
     */
    TSF_IN_HTML_COMMENT = 0x2000
};

#define t_op            u.s.op
#define t_reflags       u.reflags
#define t_atom          u.s.atom
#define t_atom2         u.p.atom2
#define t_dval          u.dval

class TokenStream
{
    static const size_t ntokens = 4;                /* 1 current + 2 lookahead, rounded
                                                       to power of 2 to avoid divmod by 3 */
    static const uintN ntokensMask = ntokens - 1;

  public:
    typedef Vector<jschar, 32> CharBuffer;

    /*
     * To construct a TokenStream, first call the constructor, which is
     * infallible, then call |init|, which can fail. To destroy a TokenStream,
     * first call |close| then call the destructor. If |init| fails, do not call
     * |close|.
     *
     * This class uses JSContext.tempPool to allocate internal buffers. The
     * caller should JS_ARENA_MARK before calling |init| and JS_ARENA_RELEASE
     * after calling |close|.
     */
    TokenStream(JSContext *);

    /*
     * Create a new token stream from an input buffer.
     * Return false on memory-allocation failure.
     */
    bool init(const jschar *base, size_t length, const char *filename, uintN lineno,
              JSVersion version);
    void close();
    ~TokenStream() {}

    /* Accessors. */
    JSContext *getContext() const { return cx; }
    bool onCurrentLine(const TokenPos &pos) const { return lineno == pos.end.lineno; }
    const Token &currentToken() const { return tokens[cursor]; }
    const CharBuffer &getTokenbuf() const { return tokenbuf; }
    const char *getFilename() const { return filename; }
    uintN getLineno() const { return lineno; }
    /* Note that the version and hasXML can get out of sync via setXML. */
    JSVersion versionNumber() const { return VersionNumber(version); }
    JSVersion versionWithFlags() const { return version; }
    bool hasAnonFunFix() const { return VersionHasAnonFunFix(version); }
    bool hasXML() const { return xml || VersionShouldParseXML(versionNumber()); }
    void setXML(bool enabled) { xml = enabled; }

    /* Flag methods. */
    void setStrictMode(bool enabled = true) { setFlag(enabled, TSF_STRICT_MODE_CODE); }
    void setXMLTagMode(bool enabled = true) { setFlag(enabled, TSF_XMLTAGMODE); }
    void setXMLOnlyMode(bool enabled = true) { setFlag(enabled, TSF_XMLONLYMODE); }
    void setUnexpectedEOF(bool enabled = true) { setFlag(enabled, TSF_UNEXPECTED_EOF); }
    void setOctalCharacterEscape(bool enabled = true) { setFlag(enabled, TSF_OCTAL_CHAR); }

    bool isStrictMode() { return !!(flags & TSF_STRICT_MODE_CODE); }
    bool isXMLTagMode() { return !!(flags & TSF_XMLTAGMODE); }
    bool isXMLOnlyMode() { return !!(flags & TSF_XMLONLYMODE); }
    bool isUnexpectedEOF() { return !!(flags & TSF_UNEXPECTED_EOF); }
    bool isEOF() const { return !!(flags & TSF_EOF); }
    bool isError() const { return !!(flags & TSF_ERROR); }
    bool hasOctalCharacterEscape() const { return flags & TSF_OCTAL_CHAR; }

    /* Mutators. */
    bool reportCompileErrorNumberVA(JSParseNode *pn, uintN flags, uintN errorNumber, va_list ap);
    void mungeCurrentToken(TokenKind newKind) { tokens[cursor].type = newKind; }
    void mungeCurrentToken(JSOp newOp) { tokens[cursor].t_op = newOp; }
    void mungeCurrentToken(TokenKind newKind, JSOp newOp) {
        mungeCurrentToken(newKind);
        mungeCurrentToken(newOp);
    }

  private:
    static JSAtom *atomize(JSContext *cx, CharBuffer &cb);

    /*
     * Enables flags in the associated tokenstream for the object lifetime.
     * Useful for lexically-scoped flag toggles.
     */
    class Flagger {
        TokenStream * const parent;
        uintN       flags;
      public:
        Flagger(TokenStream *parent, uintN withFlags) : parent(parent), flags(withFlags) {
            parent->flags |= flags;
        }

        ~Flagger() { parent->flags &= ~flags; }
    };
    friend class Flagger;

    void setFlag(bool enabled, TokenStreamFlags flag) {
        if (enabled)
            flags |= flag;
        else
            flags &= ~flag;
    }

  public:
    /*
     * Get the next token from the stream, make it the current token, and
     * return its kind.
     */
    TokenKind getToken() {
        /* Check for a pushed-back token resulting from mismatching lookahead. */
        while (lookahead != 0) {
            JS_ASSERT(!(flags & TSF_XMLTEXTMODE));
            lookahead--;
            cursor = (cursor + 1) & ntokensMask;
            TokenKind tt = currentToken().type;
            JS_ASSERT(!(flags & TSF_NEWLINES));
            if (tt != TOK_EOL)
                return tt;
        }

        /* If there was a fatal error, keep returning TOK_ERROR. */
        if (flags & TSF_ERROR)
            return TOK_ERROR;

        return getTokenInternal();
    }

    /* Similar, but also sets flags. */
    TokenKind getToken(uintN withFlags) {
        Flagger flagger(this, withFlags);
        return getToken();
    }

    /*
     * Push the last scanned token back into the stream.
     */
    void ungetToken() {
        JS_ASSERT(lookahead < ntokensMask);
        lookahead++;
        cursor = (cursor - 1) & ntokensMask;
    }

    TokenKind peekToken(uintN withFlags = 0) {
        Flagger flagger(this, withFlags);
        if (lookahead != 0) {
            JS_ASSERT(lookahead == 1);
            return tokens[(cursor + lookahead) & ntokensMask].type;
        }
        TokenKind tt = getToken();
        ungetToken();
        return tt;
    }

    TokenKind peekTokenSameLine(uintN withFlags = 0) {
        Flagger flagger(this, withFlags);
        if (!onCurrentLine(currentToken().pos))
            return TOK_EOL;
        TokenKind tt = peekToken(TSF_NEWLINES);
        return tt;
    }

    /*
     * Get the next token from the stream if its kind is |tt|.
     */
    JSBool matchToken(TokenKind tt, uintN withFlags = 0) {
        Flagger flagger(this, withFlags);
        if (getToken() == tt)
            return JS_TRUE;
        ungetToken();
        return JS_FALSE;
    }

  private:
    typedef struct TokenBuf {
        jschar              *base;      /* base of line or stream buffer */
        jschar              *limit;     /* limit for quick bounds check */
        jschar              *ptr;       /* next char to get, or slot to use */
    } TokenBuf;

    TokenKind getTokenInternal();     /* doesn't check for pushback or error flag. */

    int32 getChar();
    int32 getCharIgnoreEOL();
    void ungetChar(int32 c);
    void ungetCharIgnoreEOL(int32 c);
    Token *newToken(ptrdiff_t adjust);
    bool peekUnicodeEscape(int32 *c);
    bool matchUnicodeEscapeIdStart(int32 *c);
    bool matchUnicodeEscapeIdent(int32 *c);
    JSBool peekChars(intN n, jschar *cp);
    JSBool getXMLEntity();
    jschar *findEOL();

    JSBool matchChar(int32 expect) {
        int32 c = getChar();
        if (c == expect)
            return JS_TRUE;
        ungetChar(c);
        return JS_FALSE;
    }

    int32 peekChar() {
        int32 c = getChar();
        ungetChar(c);
        return c;
    }

    void skipChars(intN n) {
        while (--n >= 0)
            getChar();
    }

    JSContext           * const cx;
    Token               tokens[ntokens];/* circular token buffer */
    uintN               cursor;         /* index of last parsed token */
    uintN               lookahead;      /* count of lookahead tokens */
    uintN               lineno;         /* current line number */
    uintN               flags;          /* flags -- see above */
    jschar              *linebase;      /* start of current line;  points into userbuf */
    jschar              *prevLinebase;  /* start of previous line;  NULL if on the first line */
    TokenBuf            userbuf;        /* user input buffer */
    const char          *filename;      /* input filename or null */
    JSSourceHandler     listener;       /* callback for source; eg debugger */
    void                *listenerData;  /* listener 'this' data */
    void                *listenerTSData;/* listener data for this TokenStream */
    CharBuffer          tokenbuf;       /* current token string buffer */
    bool                maybeEOL[256];  /* probabilistic EOL lookup table */
    bool                maybeStrSpecial[256];/* speeds up string scanning */
    JSVersion           version;        /* (i.e. to identify keywords) */
    bool                xml;            /* see JSOPTION_XML */
};

} /* namespace js */

/* Unicode separators that are treated as line terminators, in addition to \n, \r */
#define LINE_SEPARATOR  0x2028
#define PARA_SEPARATOR  0x2029

extern void
js_CloseTokenStream(JSContext *cx, js::TokenStream *ts);

extern JS_FRIEND_API(int)
js_fgets(char *buf, int size, FILE *file);

namespace js {

struct KeywordInfo {
    const char  *chars;         /* C string with keyword text */
    TokenKind   tokentype;
    JSOp        op;             /* JSOp */
    JSVersion   version;        /* JSVersion */
};

/*
 * Returns a KeywordInfo for the specified characters, or NULL if the string is
 * not a keyword.
 */
extern const KeywordInfo *
FindKeyword(const jschar *s, size_t length);

} // namespace js

/*
 * Friend-exported API entry point to call a mapping function on each reserved
 * identifier in the scanner's keyword table.
 */
typedef void (*JSMapKeywordFun)(const char *);

/*
 * Check that str forms a valid JS identifier name. The function does not
 * check if str is a JS keyword.
 */
extern JSBool
js_IsIdentifier(JSLinearString *str);

/*
 * Steal one JSREPORT_* bit (see jsapi.h) to tell that arguments to the error
 * message have const jschar* type, not const char*.
 */
#define JSREPORT_UC 0x100

namespace js {

/*
 * Report a compile-time error by its number. Return true for a warning, false
 * for an error. When pn is not null, use it to report error's location.
 * Otherwise use ts, which must not be null.
 */
bool
ReportCompileErrorNumber(JSContext *cx, TokenStream *ts, JSParseNode *pn, uintN flags,
                         uintN errorNumber, ...);

/*
 * Report a condition that should elicit a warning with JSOPTION_STRICT,
 * or an error if ts or tc is handling strict mode code.  This function
 * defers to ReportCompileErrorNumber to do the real work.  Either tc
 * or ts may be NULL, if there is no tree context or token stream state
 * whose strictness should affect the report.
 *
 * One could have ReportCompileErrorNumber recognize the
 * JSREPORT_STRICT_MODE_ERROR flag instead of having a separate function
 * like this one.  However, the strict mode code flag we need to test is
 * in the JSTreeContext structure for that code; we would have to change
 * the ~120 ReportCompileErrorNumber calls to pass the additional
 * argument, even though many of those sites would never use it.  Using
 * ts's TSF_STRICT_MODE_CODE flag instead of tc's would be brittle: at some
 * points ts's flags don't correspond to those of the tc relevant to the
 * error.
 */
bool
ReportStrictModeError(JSContext *cx, TokenStream *ts, JSTreeContext *tc, JSParseNode *pn,
                      uintN errorNumber, ...);

} /* namespace js */

#endif /* jsscan_h___ */
