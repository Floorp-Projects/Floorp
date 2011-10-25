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
 *   Nick Fitzgerald <nfitzgerald@mozilla.com>
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

#ifndef TokenStream_h__
#define TokenStream_h__

/*
 * JS lexical scanner interface.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include "jscntxt.h"
#include "jsversion.h"
#include "jsopcode.h"
#include "jsprvtd.h"
#include "jspubtd.h"

#include "js/Vector.h"

#define JS_KEYWORD(keyword, type, op, version) \
    extern const char js_##keyword##_str[];
#include "jskeyword.tbl"
#undef JS_KEYWORD

namespace js {

enum TokenKind {
    TOK_ERROR = -1,                     /* well-known as the only code < EOF */
    TOK_EOF = 0,                        /* end of file */
    TOK_EOL = 1,                        /* end of line; only returned by peekTokenSameLine() */
    TOK_SEMI = 2,                       /* semicolon */
    TOK_COMMA = 3,                      /* comma operator */
    TOK_ASSIGN = 4,                     /* assignment ops (= += -= etc.) */
    TOK_HOOK = 5, TOK_COLON = 6,        /* conditional (?:) */
    TOK_OR = 7,                         /* logical or (||) */
    TOK_AND = 8,                        /* logical and (&&) */
    TOK_BITOR = 9,                      /* bitwise-or (|) */
    TOK_BITXOR = 10,                    /* bitwise-xor (^) */
    TOK_BITAND = 11,                    /* bitwise-and (&) */
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
    TOK_UPVARS = 85,                    /* lexical dependencies as JSAtomDefnMap
                                           of definitions paired with a parse
                                           tree full of uses of those names */
    TOK_RESERVED,                       /* reserved keywords */
    TOK_STRICT_RESERVED,                /* reserved keywords in strict mode */

    /*
     * The following token types occupy contiguous ranges to enable easy
     * range-testing.
     */

    /* Equality operation tokens */
    TOK_STRICTEQ,
    TOK_EQUALITY_START = TOK_STRICTEQ,
    TOK_EQ,
    TOK_STRICTNE,
    TOK_NE,
    TOK_EQUALITY_LAST = TOK_NE,

    TOK_LIMIT                           /* domain size */
};

inline bool
TokenKindIsEquality(TokenKind tt)
{
    return TOK_EQUALITY_START <= tt && tt <= TOK_EQUALITY_LAST;
}

inline bool
TokenKindIsXML(TokenKind tt)
{
    return tt == TOK_AT || tt == TOK_DBLCOLON || tt == TOK_ANYNAME;
}

inline bool
TreeTypeIsXML(TokenKind tt)
{
    return tt == TOK_XMLCOMMENT || tt == TOK_XMLCDATA || tt == TOK_XMLPI ||
           tt == TOK_XMLELEM || tt == TOK_XMLLIST;
}

inline bool
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

    bool operator==(const TokenPtr& bptr) const {
        return index == bptr.index && lineno == bptr.lineno;
    }

    bool operator!=(const TokenPtr& bptr) const {
        return index != bptr.index || lineno != bptr.lineno;
    }

    bool operator <(const TokenPtr& bptr) const {
        return lineno < bptr.lineno ||
               (lineno == bptr.lineno && index < bptr.index);
    }

    bool operator <=(const TokenPtr& bptr) const {
        return lineno < bptr.lineno ||
               (lineno == bptr.lineno && index <= bptr.index);
    }

    bool operator >(const TokenPtr& bptr) const {
        return !(*this <= bptr);
    }

    bool operator >=(const TokenPtr& bptr) const {
        return !(*this < bptr);
    }
};

struct TokenPos {
    TokenPtr          begin;          /* first character and line of token */
    TokenPtr          end;            /* index 1 past last char, last line */

    static TokenPos make(const TokenPtr &begin, const TokenPtr &end) {
        // Assertions temporarily disabled by jorendorff. See bug 695922.
        //JS_ASSERT(begin <= end);
        TokenPos pos = {begin, end};
        return pos;
    }

    /* Return a TokenPos that covers left, right, and anything in between. */
    static TokenPos box(const TokenPos &left, const TokenPos &right) {
        // Assertions temporarily disabled by jorendorff. See bug 695922.
        //JS_ASSERT(left.begin <= left.end);
        //JS_ASSERT(left.end <= right.begin);
        //JS_ASSERT(right.begin <= right.end);
        TokenPos pos = {left.begin, right.end};
        return pos;
    }

    bool operator==(const TokenPos& bpos) const {
        return begin == bpos.begin && end == bpos.end;
    }

    bool operator!=(const TokenPos& bpos) const {
        return begin != bpos.begin || end != bpos.end;
    }

    bool operator <(const TokenPos& bpos) const {
        return begin < bpos.begin;
    }

    bool operator <=(const TokenPos& bpos) const {
        return begin <= bpos.begin;
    }

    bool operator >(const TokenPos& bpos) const {
        return !(*this <= bpos);
    }

    bool operator >=(const TokenPos& bpos) const {
        return !(*this < bpos);
    }
};

struct Token {
    TokenKind           type;           /* char value or above enumerator */
    TokenPos            pos;            /* token position in file */
    const jschar        *ptr;           /* beginning of token in line buffer */
    union {
        struct {                        /* name or string literal */
            JSOp        op;             /* operator, for minimal parser */
            union {
              private:
                friend struct Token;
                PropertyName *name;     /* non-numeric atom */
                JSAtom       *atom;     /* potentially-numeric atom */
            } n;
        } s;

      private:
        friend struct Token;
        struct {                        /* pair for <?target data?> XML PI */
            JSAtom       *data;         /* auxiliary atom table entry */
            PropertyName *target;       /* main atom table entry */
        } xmlpi;
        uint16          sharpNumber;    /* sharp variable number: #1# or #1= */
        jsdouble        number;         /* floating point number */
        RegExpFlag      reflags;        /* regexp flags, use tokenbuf to access
                                           regexp chars */
    } u;

    /* Mutators */

    /*
     * FIXME: Init type early enough such that all mutators can assert
     *        type-safety.  See bug 697000.
     */

    void setName(JSOp op, PropertyName *name) {
        JS_ASSERT(op == JSOP_NAME);
        u.s.op = op;
        u.s.n.name = name;
    }

    void setAtom(JSOp op, JSAtom *atom) {
        JS_ASSERT(op == JSOP_STRING || op == JSOP_XMLCOMMENT || JSOP_XMLCDATA);
        u.s.op = op;
        u.s.n.atom = atom;
    }

    void setProcessingInstruction(PropertyName *target, JSAtom *data) {
        u.xmlpi.target = target;
        u.xmlpi.data = data;
    }

    void setRegExpFlags(js::RegExpFlag flags) {
        JS_ASSERT((flags & AllFlags) == flags);
        u.reflags = flags;
    }

    void setSharpNumber(uint16 sharpNum) {
        u.sharpNumber = sharpNum;
    }

    void setNumber(jsdouble n) {
        u.number = n;
    }

    /* Type-safe accessors */

    PropertyName *name() const {
        JS_ASSERT(type == TOK_NAME);
        return u.s.n.name->asPropertyName(); /* poor-man's type verification */
    }

    JSAtom *atom() const {
        JS_ASSERT(type == TOK_STRING ||
                  type == TOK_XMLNAME ||
                  type == TOK_XMLATTR ||
                  type == TOK_XMLTEXT ||
                  type == TOK_XMLCDATA ||
                  type == TOK_XMLSPACE ||
                  type == TOK_XMLCOMMENT);
        return u.s.n.atom;
    }

    PropertyName *xmlPITarget() const {
        JS_ASSERT(type == TOK_XMLPI);
        return u.xmlpi.target;
    }
    JSAtom *xmlPIData() const {
        JS_ASSERT(type == TOK_XMLPI);
        return u.xmlpi.data;
    }

    js::RegExpFlag regExpFlags() const {
        JS_ASSERT(type == TOK_REGEXP);
        JS_ASSERT((u.reflags & AllFlags) == u.reflags);
        return u.reflags;
    }

    uint16 sharpNumber() const {
        JS_ASSERT(type == TOK_DEFSHARP || type == TOK_USESHARP);
        return u.sharpNumber;
    }

    jsdouble number() const {
        JS_ASSERT(type == TOK_NUMBER);
        return u.number;
    }
};

#define t_op            u.s.op

enum TokenStreamFlags
{
    TSF_EOF = 0x02,             /* hit end of file */
    TSF_EOL = 0x04,             /* an EOL was hit in whitespace or a multi-line comment */
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

class TokenStream
{
    /* Unicode separators that are treated as line terminators, in addition to \n, \r */
    enum {
        LINE_SEPARATOR = 0x2028,
        PARA_SEPARATOR = 0x2029
    };

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
     * This class uses JSContext.tempLifoAlloc to allocate internal buffers. The
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
    ~TokenStream();

    /* Accessors. */
    JSContext *getContext() const { return cx; }
    bool onCurrentLine(const TokenPos &pos) const { return lineno == pos.end.lineno; }
    const Token &currentToken() const { return tokens[cursor]; }
    bool isCurrentTokenType(TokenKind type) const {
        return currentToken().type == type;
    }
    bool isCurrentTokenType(TokenKind type1, TokenKind type2) const {
        TokenKind type = currentToken().type;
        return type == type1 || type == type2;
    }
    const CharBuffer &getTokenbuf() const { return tokenbuf; }
    const char *getFilename() const { return filename; }
    uintN getLineno() const { return lineno; }
    /* Note that the version and hasXML can get out of sync via setXML. */
    JSVersion versionNumber() const { return VersionNumber(version); }
    JSVersion versionWithFlags() const { return version; }
    bool hasXML() const { return xml || VersionShouldParseXML(versionNumber()); }
    void setXML(bool enabled) { xml = enabled; }

    bool isCurrentTokenEquality() const {
        return TokenKindIsEquality(currentToken().type);
    }

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
    bool hasOctalCharacterEscape() const { return flags & TSF_OCTAL_CHAR; }

    /* Mutators. */
    bool reportCompileErrorNumberVA(ParseNode *pn, uintN flags, uintN errorNumber, va_list ap);
    void mungeCurrentToken(TokenKind newKind) { tokens[cursor].type = newKind; }
    void mungeCurrentToken(JSOp newOp) { tokens[cursor].t_op = newOp; }
    void mungeCurrentToken(TokenKind newKind, JSOp newOp) {
        mungeCurrentToken(newKind);
        mungeCurrentToken(newOp);
    }

  private:
    static JSAtom *atomize(JSContext *cx, CharBuffer &cb);
    bool putIdentInTokenbuf(const jschar *identStart);

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
        if (lookahead != 0) {
            JS_ASSERT(!(flags & TSF_XMLTEXTMODE));
            lookahead--;
            cursor = (cursor + 1) & ntokensMask;
            TokenKind tt = currentToken().type;
            JS_ASSERT(tt != TOK_EOL);
            return tt;
        }

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

    TokenKind peekToken() {
        if (lookahead != 0) {
            JS_ASSERT(lookahead == 1);
            return tokens[(cursor + lookahead) & ntokensMask].type;
        }
        TokenKind tt = getTokenInternal();
        ungetToken();
        return tt;
    }

    TokenKind peekToken(uintN withFlags) {
        Flagger flagger(this, withFlags);
        return peekToken();
    }

    TokenKind peekTokenSameLine(uintN withFlags = 0) {
        if (!onCurrentLine(currentToken().pos))
            return TOK_EOL;

        if (lookahead != 0) {
            JS_ASSERT(lookahead == 1);
            return tokens[(cursor + lookahead) & ntokensMask].type;
        }

        /*
         * This is the only place TOK_EOL is produced.  No token with TOK_EOL
         * is created, just a TOK_EOL TokenKind is returned.
         */
        flags &= ~TSF_EOL;
        TokenKind tt = getToken(withFlags);
        if (flags & TSF_EOL) {
            tt = TOK_EOL;
            flags &= ~TSF_EOL;
        }
        ungetToken();
        return tt;
    }

    /*
     * Get the next token from the stream if its kind is |tt|.
     */
    bool matchToken(TokenKind tt) {
        if (getToken() == tt)
            return true;
        ungetToken();
        return false;
    }

    bool matchToken(TokenKind tt, uintN withFlags) {
        Flagger flagger(this, withFlags);
        return matchToken(tt);
    }

    /*
     * Give up responsibility for managing the sourceMap filename's memory.
     */
    const jschar *releaseSourceMap() {
        const jschar* sm = sourceMap;
        sourceMap = NULL;
        return sm;
    }

    /*
     * If the name at s[0:length] is not a keyword in this version, return
     * true with *ttp and *topp unchanged.
     *
     * If it is a reserved word in this version and strictness mode, and thus
     * can't be present in correct code, report a SyntaxError and return false.
     *
     * If it is a keyword, like "if", the behavior depends on ttp/topp. If ttp
     * and topp are null, report a SyntaxError ("if is a reserved identifier")
     * and return false. If ttp and topp are non-null, return true with the
     * keyword's TokenKind in *ttp and its JSOp in *topp.
     *
     * ttp and topp must be either both null or both non-null.
     */
    bool checkForKeyword(const jschar *s, size_t length, TokenKind *ttp, JSOp *topp);

  private:
    /*
     * This is the low-level interface to the JS source code buffer.  It just
     * gets raw chars, basically.  TokenStreams functions are layered on top
     * and do some extra stuff like converting all EOL sequences to '\n',
     * tracking the line number, and setting the TSF_EOF flag.  (The "raw" in
     * "raw chars" refers to the lack of EOL sequence normalization.)
     */
    class TokenBuf {
      public:
        TokenBuf() : base(NULL), limit(NULL), ptr(NULL) { }

        void init(const jschar *buf, size_t length) {
            base = ptr = buf;
            limit = base + length;
        }

        bool hasRawChars() const {
            return ptr < limit;
        }

        bool atStart() const {
            return ptr == base;
        }

        jschar getRawChar() {
            return *ptr++;      /* this will NULL-crash if poisoned */
        }

        jschar peekRawChar() const {
            return *ptr;        /* this will NULL-crash if poisoned */
        }

        bool matchRawChar(jschar c) {
            if (*ptr == c) {    /* this will NULL-crash if poisoned */
                ptr++;
                return true;
            }
            return false;
        }

        bool matchRawCharBackwards(jschar c) {
            JS_ASSERT(ptr);     /* make sure haven't been poisoned */
            if (*(ptr - 1) == c) {
                ptr--;
                return true;
            }
            return false;
        }

        void ungetRawChar() {
            JS_ASSERT(ptr);     /* make sure haven't been poisoned */
            ptr--;
        }

        const jschar *addressOfNextRawChar() {
            JS_ASSERT(ptr);     /* make sure haven't been poisoned */
            return ptr;
        }

        /* Use this with caution! */
        void setAddressOfNextRawChar(const jschar *a) {
            JS_ASSERT(a);
            ptr = a;
        }

#ifdef DEBUG
        /*
         * Poison the TokenBuf so it cannot be accessed again.  There's one
         * exception to this rule -- see findEOL() -- which is why
         * ptrWhenPoisoned exists.
         */
        void poison() {
            ptrWhenPoisoned = ptr;
            ptr = NULL;
        }
#endif

        static bool isRawEOLChar(int32 c) {
            return (c == '\n' || c == '\r' || c == LINE_SEPARATOR || c == PARA_SEPARATOR);
        }

        const jschar *findEOL();

      private:
        const jschar *base;             /* base of buffer */
        const jschar *limit;            /* limit for quick bounds check */
        const jschar *ptr;              /* next char to get */
        const jschar *ptrWhenPoisoned;  /* |ptr| when poison() was called */
    };

    TokenKind getTokenInternal();     /* doesn't check for pushback or error flag. */

    int32 getChar();
    int32 getCharIgnoreEOL();
    void ungetChar(int32 c);
    void ungetCharIgnoreEOL(int32 c);
    Token *newToken(ptrdiff_t adjust);
    bool peekUnicodeEscape(int32 *c);
    bool matchUnicodeEscapeIdStart(int32 *c);
    bool matchUnicodeEscapeIdent(int32 *c);
    bool peekChars(intN n, jschar *cp);
    bool getAtLine();
    bool getAtSourceMappingURL();

    bool getXMLEntity();
    bool getXMLTextOrTag(TokenKind *ttp, Token **tpp);
    bool getXMLMarkup(TokenKind *ttp, Token **tpp);

    bool matchChar(int32 expect) {
        int32 c = getChar();
        if (c == expect)
            return true;
        ungetChar(c);
        return false;
    }

    void consumeKnownChar(int32 expect) {
        mozilla::DebugOnly<int32> c = getChar();
        JS_ASSERT(c == expect);
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

    void updateLineInfoForEOL();
    void updateFlagsForEOL();

    JSContext           * const cx;
    Token               tokens[ntokens];/* circular token buffer */
    uintN               cursor;         /* index of last parsed token */
    uintN               lookahead;      /* count of lookahead tokens */
    uintN               lineno;         /* current line number */
    uintN               flags;          /* flags -- see above */
    const jschar        *linebase;      /* start of current line;  points into userbuf */
    const jschar        *prevLinebase;  /* start of previous line;  NULL if on the first line */
    TokenBuf            userbuf;        /* user input buffer */
    const char          *filename;      /* input filename or null */
    jschar              *sourceMap;     /* source map's filename or null */
    void                *listenerTSData;/* listener data for this TokenStream */
    CharBuffer          tokenbuf;       /* current token string buffer */
    int8                oneCharTokens[128];  /* table of one-char tokens */
    JSPackedBool        maybeEOL[256];       /* probabilistic EOL lookup table */
    JSPackedBool        maybeStrSpecial[256];/* speeds up string scanning */
    JSVersion           version;        /* (i.e. to identify keywords) */
    bool                xml;            /* see JSOPTION_XML */
};

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
const KeywordInfo *
FindKeyword(const jschar *s, size_t length);

/*
 * Check that str forms a valid JS identifier name. The function does not
 * check if str is a JS keyword.
 */
JSBool
IsIdentifier(JSLinearString *str);

/*
 * Steal one JSREPORT_* bit (see jsapi.h) to tell that arguments to the error
 * message have const jschar* type, not const char*.
 */
#define JSREPORT_UC 0x100

/*
 * Report a compile-time error by its number. Return true for a warning, false
 * for an error. When pn is not null, use it to report error's location.
 * Otherwise use ts, which must not be null.
 */
bool
ReportCompileErrorNumber(JSContext *cx, TokenStream *ts, ParseNode *pn, uintN flags,
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
 * in the TreeContext structure for that code; we would have to change
 * the ~120 ReportCompileErrorNumber calls to pass the additional
 * argument, even though many of those sites would never use it.  Using
 * ts's TSF_STRICT_MODE_CODE flag instead of tc's would be brittle: at some
 * points ts's flags don't correspond to those of the tc relevant to the
 * error.
 */
bool
ReportStrictModeError(JSContext *cx, TokenStream *ts, TreeContext *tc, ParseNode *pn,
                      uintN errorNumber, ...);

} /* namespace js */

extern JS_FRIEND_API(int)
js_fgets(char *buf, int size, FILE *file);

#endif /* TokenStream_h__ */
