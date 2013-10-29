/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// JS lexical scanner.

#include "frontend/TokenStream.h"

#include "mozilla/PodOperations.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "jsatom.h"
#include "jscntxt.h"
#include "jsexn.h"
#include "jsnum.h"
#include "jsworkers.h"

#include "frontend/BytecodeCompiler.h"
#include "js/CharacterEncoding.h"
#include "vm/Keywords.h"
#include "vm/StringBuffer.h"

using namespace js;
using namespace js::frontend;
using namespace js::unicode;

using mozilla::Maybe;
using mozilla::PodAssign;
using mozilla::PodCopy;
using mozilla::PodZero;

struct KeywordInfo {
    const char  *chars;         // C string with keyword text
    TokenKind   tokentype;
    JSVersion   version;
};

static const KeywordInfo keywords[] = {
#define KEYWORD_INFO(keyword, name, type, version) \
    {js_##keyword##_str, type, version},
    FOR_EACH_JAVASCRIPT_KEYWORD(KEYWORD_INFO)
#undef KEYWORD_INFO
};

// Returns a KeywordInfo for the specified characters, or nullptr if the string
// is not a keyword.
static const KeywordInfo *
FindKeyword(const jschar *s, size_t length)
{
    JS_ASSERT(length != 0);

    register size_t i;
    const KeywordInfo *kw;
    const char *chars;

#define JSKW_LENGTH()           length
#define JSKW_AT(column)         s[column]
#define JSKW_GOT_MATCH(index)   i = (index); goto got_match;
#define JSKW_TEST_GUESS(index)  i = (index); goto test_guess;
#define JSKW_NO_MATCH()         goto no_match;
#include "jsautokw.h"
#undef JSKW_NO_MATCH
#undef JSKW_TEST_GUESS
#undef JSKW_GOT_MATCH
#undef JSKW_AT
#undef JSKW_LENGTH

  got_match:
    return &keywords[i];

  test_guess:
    kw = &keywords[i];
    chars = kw->chars;
    do {
        if (*s++ != (unsigned char)(*chars++))
            goto no_match;
    } while (--length != 0);
    return kw;

  no_match:
    return nullptr;
}

bool
frontend::IsIdentifier(JSLinearString *str)
{
    const jschar *chars = str->chars();
    size_t length = str->length();

    if (length == 0)
        return false;
    jschar c = *chars;
    if (!IsIdentifierStart(c))
        return false;
    const jschar *end = chars + length;
    while (++chars != end) {
        c = *chars;
        if (!IsIdentifierPart(c))
            return false;
    }
    return true;
}

bool
frontend::IsKeyword(JSLinearString *str)
{
    return FindKeyword(str->chars(), str->length()) != nullptr;
}

TokenStream::SourceCoords::SourceCoords(ExclusiveContext *cx, uint32_t ln)
  : lineStartOffsets_(cx), initialLineNum_(ln), lastLineIndex_(0)
{
    // This is actually necessary!  Removing it causes compile errors on
    // GCC and clang.  You could try declaring this:
    //
    //   const uint32_t TokenStream::SourceCoords::MAX_PTR;
    //
    // which fixes the GCC/clang error, but causes bustage on Windows.  Sigh.
    //
    uint32_t maxPtr = MAX_PTR;

    // The first line begins at buffer offset 0.  MAX_PTR is the sentinel.  The
    // appends cannot fail because |lineStartOffsets_| has statically-allocated
    // elements.
    JS_ASSERT(lineStartOffsets_.capacity() >= 2);
    (void)lineStartOffsets_.reserve(2);
    lineStartOffsets_.infallibleAppend(0);
    lineStartOffsets_.infallibleAppend(maxPtr);
}

JS_ALWAYS_INLINE void
TokenStream::SourceCoords::add(uint32_t lineNum, uint32_t lineStartOffset)
{
    uint32_t lineIndex = lineNumToIndex(lineNum);
    uint32_t sentinelIndex = lineStartOffsets_.length() - 1;

    JS_ASSERT(lineStartOffsets_[0] == 0 && lineStartOffsets_[sentinelIndex] == MAX_PTR);

    if (lineIndex == sentinelIndex) {
        // We haven't seen this newline before.  Update lineStartOffsets_.
        // We ignore any failures due to OOM -- because we always have a
        // sentinel node, it'll just be like the newline wasn't present.  I.e.
        // the line numbers will be wrong, but the code won't crash or anything
        // like that.
        lineStartOffsets_[lineIndex] = lineStartOffset;

        uint32_t maxPtr = MAX_PTR;
        (void)lineStartOffsets_.append(maxPtr);

    } else {
        // We have seen this newline before (and ungot it).  Do nothing (other
        // than checking it hasn't mysteriously changed).
        JS_ASSERT(lineStartOffsets_[lineIndex] == lineStartOffset);
    }
}

JS_ALWAYS_INLINE void
TokenStream::SourceCoords::fill(const TokenStream::SourceCoords &other)
{
    JS_ASSERT(lineStartOffsets_.back() == MAX_PTR);
    JS_ASSERT(other.lineStartOffsets_.back() == MAX_PTR);

    if (lineStartOffsets_.length() >= other.lineStartOffsets_.length())
        return;

    uint32_t sentinelIndex = lineStartOffsets_.length() - 1;
    lineStartOffsets_[sentinelIndex] = other.lineStartOffsets_[sentinelIndex];

    for (size_t i = sentinelIndex + 1; i < other.lineStartOffsets_.length(); i++)
        (void)lineStartOffsets_.append(other.lineStartOffsets_[i]);
}

JS_ALWAYS_INLINE uint32_t
TokenStream::SourceCoords::lineIndexOf(uint32_t offset) const
{
    uint32_t iMin, iMax, iMid;

    if (lineStartOffsets_[lastLineIndex_] <= offset) {
        // If we reach here, offset is on a line the same as or higher than
        // last time.  Check first for the +0, +1, +2 cases, because they
        // typically cover 85--98% of cases.
        if (offset < lineStartOffsets_[lastLineIndex_ + 1])
            return lastLineIndex_;      // lineIndex is same as last time

        // If we reach here, there must be at least one more entry (plus the
        // sentinel).  Try it.
        lastLineIndex_++;
        if (offset < lineStartOffsets_[lastLineIndex_ + 1])
            return lastLineIndex_;      // lineIndex is one higher than last time

        // The same logic applies here.
        lastLineIndex_++;
        if (offset < lineStartOffsets_[lastLineIndex_ + 1]) {
            return lastLineIndex_;      // lineIndex is two higher than last time
        }

        // No luck.  Oh well, we have a better-than-default starting point for
        // the binary search.
        iMin = lastLineIndex_ + 1;
        JS_ASSERT(iMin < lineStartOffsets_.length() - 1);   // -1 due to the sentinel

    } else {
        iMin = 0;
    }

    // This is a binary search with deferred detection of equality, which was
    // marginally faster in this case than a standard binary search.
    // The -2 is because |lineStartOffsets_.length() - 1| is the sentinel, and we
    // want one before that.
    iMax = lineStartOffsets_.length() - 2;
    while (iMax > iMin) {
        iMid = iMin + (iMax - iMin) / 2;
        if (offset >= lineStartOffsets_[iMid + 1])
            iMin = iMid + 1;    // offset is above lineStartOffsets_[iMid]
        else
            iMax = iMid;        // offset is below or within lineStartOffsets_[iMid]
    }
    JS_ASSERT(iMax == iMin);
    JS_ASSERT(lineStartOffsets_[iMin] <= offset && offset < lineStartOffsets_[iMin + 1]);
    lastLineIndex_ = iMin;
    return iMin;
}

uint32_t
TokenStream::SourceCoords::lineNum(uint32_t offset) const
{
    uint32_t lineIndex = lineIndexOf(offset);
    return lineIndexToNum(lineIndex);
}

uint32_t
TokenStream::SourceCoords::columnIndex(uint32_t offset) const
{
    uint32_t lineIndex = lineIndexOf(offset);
    uint32_t lineStartOffset = lineStartOffsets_[lineIndex];
    JS_ASSERT(offset >= lineStartOffset);
    return offset - lineStartOffset;
}

void
TokenStream::SourceCoords::lineNumAndColumnIndex(uint32_t offset, uint32_t *lineNum,
                                                 uint32_t *columnIndex) const
{
    uint32_t lineIndex = lineIndexOf(offset);
    *lineNum = lineIndexToNum(lineIndex);
    uint32_t lineStartOffset = lineStartOffsets_[lineIndex];
    JS_ASSERT(offset >= lineStartOffset);
    *columnIndex = offset - lineStartOffset;
}

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4351)
#endif

// Initialize members that aren't initialized in |init|.
TokenStream::TokenStream(ExclusiveContext *cx, const CompileOptions &options,
                         const jschar *base, size_t length, StrictModeGetter *smg)
  : srcCoords(cx, options.lineno),
    options_(options),
    tokens(),
    cursor(),
    lookahead(),
    lineno(options.lineno),
    flags(),
    linebase(base - options.column),
    prevLinebase(nullptr),
    userbuf(cx, base - options.column, length + options.column), // See comment below
    filename(options.filename),
    sourceURL_(nullptr),
    sourceMapURL_(nullptr),
    tokenbuf(cx),
    cx(cx),
    originPrincipals(options.originPrincipals()),
    strictModeGetter(smg),
    tokenSkip(cx, &tokens),
    linebaseSkip(cx, &linebase),
    prevLinebaseSkip(cx, &prevLinebase)
{
    JS_ASSERT_IF(options.principals(), options.originPrincipals());

    // The caller must ensure that a reference is held on the supplied principals
    // throughout compilation.
    JS_ASSERT_IF(originPrincipals, originPrincipals->refcount);

    // Column numbers are computed as offsets from the current line's base, so the
    // initial line's base must be included in the buffer. linebase and userbuf
    // were adjusted above, and if we are starting tokenization part way through
    // this line then adjust the next character.
    userbuf.setAddressOfNextRawChar(base);

    // Nb: the following tables could be static, but initializing them here is
    // much easier.  Don't worry, the time to initialize them for each
    // TokenStream is trivial.  See bug 639420.

    // See getChar() for an explanation of maybeEOL[].
    memset(maybeEOL, 0, sizeof(maybeEOL));
    maybeEOL[unsigned('\n')] = true;
    maybeEOL[unsigned('\r')] = true;
    maybeEOL[unsigned(LINE_SEPARATOR & 0xff)] = true;
    maybeEOL[unsigned(PARA_SEPARATOR & 0xff)] = true;

    // See getTokenInternal() for an explanation of maybeStrSpecial[].
    memset(maybeStrSpecial, 0, sizeof(maybeStrSpecial));
    maybeStrSpecial[unsigned('"')] = true;
    maybeStrSpecial[unsigned('\'')] = true;
    maybeStrSpecial[unsigned('\\')] = true;
    maybeStrSpecial[unsigned('\n')] = true;
    maybeStrSpecial[unsigned('\r')] = true;
    maybeStrSpecial[unsigned(LINE_SEPARATOR & 0xff)] = true;
    maybeStrSpecial[unsigned(PARA_SEPARATOR & 0xff)] = true;
    maybeStrSpecial[unsigned(EOF & 0xff)] = true;

    // See Parser::assignExpr() for an explanation of isExprEnding[].
    memset(isExprEnding, 0, sizeof(isExprEnding));
    isExprEnding[TOK_COMMA] = 1;
    isExprEnding[TOK_SEMI]  = 1;
    isExprEnding[TOK_COLON] = 1;
    isExprEnding[TOK_RP]    = 1;
    isExprEnding[TOK_RB]    = 1;
    isExprEnding[TOK_RC]    = 1;
}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

TokenStream::~TokenStream()
{
    js_free(sourceURL_);
    js_free(sourceMapURL_);

    JS_ASSERT_IF(originPrincipals, originPrincipals->refcount);
}

// Use the fastest available getc.
#if defined(HAVE_GETC_UNLOCKED)
# define fast_getc getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
# define fast_getc _getc_nolock
#else
# define fast_getc getc
#endif

JS_ALWAYS_INLINE void
TokenStream::updateLineInfoForEOL()
{
    prevLinebase = linebase;
    linebase = userbuf.addressOfNextRawChar();
    lineno++;
    srcCoords.add(lineno, linebase - userbuf.base());
}

JS_ALWAYS_INLINE void
TokenStream::updateFlagsForEOL()
{
    flags.isDirtyLine = false;
}

// This gets the next char, normalizing all EOL sequences to '\n' as it goes.
int32_t
TokenStream::getChar()
{
    int32_t c;
    if (JS_LIKELY(userbuf.hasRawChars())) {
        c = userbuf.getRawChar();

        // Normalize the jschar if it was a newline.  We need to detect any of
        // these four characters:  '\n' (0x000a), '\r' (0x000d),
        // LINE_SEPARATOR (0x2028), PARA_SEPARATOR (0x2029).  Testing for each
        // one in turn is slow, so we use a single probabilistic check, and if
        // that succeeds, test for them individually.
        //
        // We use the bottom 8 bits to index into a lookup table, succeeding
        // when d&0xff is 0xa, 0xd, 0x28 or 0x29.  Among ASCII chars (which
        // are by the far the most common) this gives false positives for '('
        // (0x0028) and ')' (0x0029).  We could avoid those by incorporating
        // the 13th bit of d into the lookup, but that requires extra shifting
        // and masking and isn't worthwhile.  See TokenStream::TokenStream()
        // for the initialization of the relevant entries in the table.
        if (JS_UNLIKELY(maybeEOL[c & 0xff])) {
            if (c == '\n')
                goto eol;
            if (c == '\r') {
                // If it's a \r\n sequence: treat as a single EOL, skip over the \n.
                if (userbuf.hasRawChars())
                    userbuf.matchRawChar('\n');
                goto eol;
            }
            if (c == LINE_SEPARATOR || c == PARA_SEPARATOR)
                goto eol;
        }
        return c;
    }

    flags.isEOF = true;
    return EOF;

  eol:
    updateLineInfoForEOL();
    return '\n';
}

// This gets the next char. It does nothing special with EOL sequences, not
// even updating the line counters.  It can be used safely if (a) the
// resulting char is guaranteed to be ungotten (by ungetCharIgnoreEOL()) if
// it's an EOL, and (b) the line-related state (lineno, linebase) is not used
// before it's ungotten.
int32_t
TokenStream::getCharIgnoreEOL()
{
    if (JS_LIKELY(userbuf.hasRawChars()))
        return userbuf.getRawChar();

    flags.isEOF = true;
    return EOF;
}

void
TokenStream::ungetChar(int32_t c)
{
    if (c == EOF)
        return;
    JS_ASSERT(!userbuf.atStart());
    userbuf.ungetRawChar();
    if (c == '\n') {
#ifdef DEBUG
        int32_t c2 = userbuf.peekRawChar();
        JS_ASSERT(TokenBuf::isRawEOLChar(c2));
#endif

        // If it's a \r\n sequence, also unget the \r.
        if (!userbuf.atStart())
            userbuf.matchRawCharBackwards('\r');

        JS_ASSERT(prevLinebase);    // we should never get more than one EOL char
        linebase = prevLinebase;
        prevLinebase = nullptr;
        lineno--;
    } else {
        JS_ASSERT(userbuf.peekRawChar() == c);
    }
}

void
TokenStream::ungetCharIgnoreEOL(int32_t c)
{
    if (c == EOF)
        return;
    JS_ASSERT(!userbuf.atStart());
    userbuf.ungetRawChar();
}

// Return true iff |n| raw characters can be read from this without reading past
// EOF or a newline, and copy those characters into |cp| if so.  The characters
// are not consumed: use skipChars(n) to do so after checking that the consumed
// characters had appropriate values.
bool
TokenStream::peekChars(int n, jschar *cp)
{
    int i, j;
    int32_t c;

    for (i = 0; i < n; i++) {
        c = getCharIgnoreEOL();
        if (c == EOF)
            break;
        if (c == '\n') {
            ungetCharIgnoreEOL(c);
            break;
        }
        cp[i] = jschar(c);
    }
    for (j = i - 1; j >= 0; j--)
        ungetCharIgnoreEOL(cp[j]);
    return i == n;
}

const jschar *
TokenStream::TokenBuf::findEOLMax(const jschar *p, size_t max)
{
    JS_ASSERT(base_ <= p && p <= limit_);

    size_t n = 0;
    while (true) {
        if (p >= limit_)
            break;
        if (n >= max)
            break;
        if (TokenBuf::isRawEOLChar(*p++))
            break;
        n++;
    }
    return p;
}

void
TokenStream::advance(size_t position)
{
    const jschar *end = userbuf.base() + position;
    while (userbuf.addressOfNextRawChar() < end)
        getChar();

    Token *cur = &tokens[cursor];
    cur->pos.begin = userbuf.addressOfNextRawChar() - userbuf.base();
    cur->type = TOK_ERROR;
    lookahead = 0;
}

void
TokenStream::tell(Position *pos)
{
    pos->buf = userbuf.addressOfNextRawChar(/* allowPoisoned = */ true);
    pos->flags = flags;
    pos->lineno = lineno;
    pos->linebase = linebase;
    pos->prevLinebase = prevLinebase;
    pos->lookahead = lookahead;
    pos->currentToken = currentToken();
    for (unsigned i = 0; i < lookahead; i++)
        pos->lookaheadTokens[i] = tokens[(cursor + 1 + i) & ntokensMask];
}

void
TokenStream::seek(const Position &pos)
{
    userbuf.setAddressOfNextRawChar(pos.buf, /* allowPoisoned = */ true);
    flags = pos.flags;
    lineno = pos.lineno;
    linebase = pos.linebase;
    prevLinebase = pos.prevLinebase;
    lookahead = pos.lookahead;

    tokens[cursor] = pos.currentToken;
    for (unsigned i = 0; i < lookahead; i++)
        tokens[(cursor + 1 + i) & ntokensMask] = pos.lookaheadTokens[i];
}

void
TokenStream::seek(const Position &pos, const TokenStream &other)
{
    srcCoords.fill(other.srcCoords);
    seek(pos);
}

bool
TokenStream::reportStrictModeErrorNumberVA(uint32_t offset, bool strictMode, unsigned errorNumber,
                                           va_list args)
{
    // In strict mode code, this is an error, not merely a warning.
    unsigned flags = JSREPORT_STRICT;
    if (strictMode)
        flags |= JSREPORT_ERROR;
    else if (options().extraWarningsOption)
        flags |= JSREPORT_WARNING;
    else
        return true;

    return reportCompileErrorNumberVA(offset, flags, errorNumber, args);
}

void
CompileError::throwError(JSContext *cx)
{
    // If there's a runtime exception type associated with this error
    // number, set that as the pending exception.  For errors occuring at
    // compile time, this is very likely to be a JSEXN_SYNTAXERR.
    //
    // If an exception is thrown but not caught, the JSREPORT_EXCEPTION
    // flag will be set in report.flags.  Proper behavior for an error
    // reporter is to ignore a report with this flag for all but top-level
    // compilation errors.  The exception will remain pending, and so long
    // as the non-top-level "load", "eval", or "compile" native function
    // returns false, the top-level reporter will eventually receive the
    // uncaught exception report.
    if (!js_ErrorToException(cx, message, &report, nullptr, nullptr)) {
        // If debugErrorHook is present then we give it a chance to veto
        // sending the error on to the regular error reporter.
        bool reportError = true;
        if (JSDebugErrorHook hook = cx->runtime()->debugHooks.debugErrorHook) {
            reportError = hook(cx, message, &report, cx->runtime()->debugHooks.debugErrorHookData);
        }

        // Report the error.
        if (reportError && cx->errorReporter)
            cx->errorReporter(cx, message, &report);
    }
}

CompileError::~CompileError()
{
    js_free((void*)report.uclinebuf);
    js_free((void*)report.linebuf);
    js_free((void*)report.ucmessage);
    js_free(message);
    message = nullptr;

    if (report.messageArgs) {
        if (argumentsType == ArgumentsAreASCII) {
            unsigned i = 0;
            while (report.messageArgs[i])
                js_free((void*)report.messageArgs[i++]);
        }
        js_free(report.messageArgs);
    }

    PodZero(&report);
}

bool
TokenStream::reportCompileErrorNumberVA(uint32_t offset, unsigned flags, unsigned errorNumber,
                                        va_list args)
{
    bool warning = JSREPORT_IS_WARNING(flags);

    if (warning && options().werrorOption) {
        flags &= ~JSREPORT_WARNING;
        warning = false;
    }

    // On the main thread, report the error immediately. When compiling off
    // thread, save the error so that the main thread can report it later.
    CompileError tempErr;
    CompileError &err = cx->isJSContext() ? tempErr : cx->addPendingCompileError();

    err.report.flags = flags;
    err.report.errorNumber = errorNumber;
    err.report.filename = filename;
    err.report.originPrincipals = originPrincipals;
    if (offset == NoOffset) {
        err.report.lineno = 0;
        err.report.column = 0;
    } else {
        err.report.lineno = srcCoords.lineNum(offset);
        err.report.column = srcCoords.columnIndex(offset);
    }

    err.argumentsType = (flags & JSREPORT_UC) ? ArgumentsAreUnicode : ArgumentsAreASCII;

    if (!js_ExpandErrorArguments(cx, js_GetErrorMessage, nullptr, errorNumber, &err.message,
                                 &err.report, err.argumentsType, args))
    {
        return false;
    }

    // Given a token, T, that we want to complain about: if T's (starting)
    // lineno doesn't match TokenStream's lineno, that means we've scanned past
    // the line that T starts on, which makes it hard to print some or all of
    // T's (starting) line for context.
    //
    // So we don't even try, leaving report.linebuf and friends zeroed.  This
    // means that any error involving a multi-line token (e.g. an unterminated
    // multi-line string literal) won't have a context printed.
    if (offset != NoOffset && err.report.lineno == lineno) {
        const jschar *tokenStart = userbuf.base() + offset;

        // We show only a portion (a "window") of the line around the erroneous
        // token -- the first char in the token, plus |windowRadius| chars
        // before it and |windowRadius - 1| chars after it.  This is because
        // lines can be very long and printing the whole line is (a) not that
        // helpful, and (b) can waste a lot of memory.  See bug 634444.
        static const size_t windowRadius = 60;

        // Truncate at the front if necessary.
        const jschar *windowBase = (linebase + windowRadius < tokenStart)
                                 ? tokenStart - windowRadius
                                 : linebase;
        uint32_t windowOffset = tokenStart - windowBase;

        // Find EOL, or truncate at the back if necessary.
        const jschar *windowLimit = userbuf.findEOLMax(tokenStart, windowRadius);
        size_t windowLength = windowLimit - windowBase;
        JS_ASSERT(windowLength <= windowRadius * 2);

        // Create the windowed strings.
        StringBuffer windowBuf(cx);
        if (!windowBuf.append(windowBase, windowLength) || !windowBuf.append((jschar)0))
            return false;

        // Unicode and char versions of the window into the offending source
        // line, without final \n.
        err.report.uclinebuf = windowBuf.extractWellSized();
        if (!err.report.uclinebuf)
            return false;
        TwoByteChars tbchars(err.report.uclinebuf, windowLength);
        err.report.linebuf = LossyTwoByteCharsToNewLatin1CharsZ(cx, tbchars).c_str();
        if (!err.report.linebuf)
            return false;

        err.report.tokenptr = err.report.linebuf + windowOffset;
        err.report.uctokenptr = err.report.uclinebuf + windowOffset;
    }

    if (cx->isJSContext())
        err.throwError(cx->asJSContext());

    return warning;
}

bool
TokenStream::reportStrictModeError(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = reportStrictModeErrorNumberVA(currentToken().pos.begin, strictMode(),
                                                errorNumber, args);
    va_end(args);
    return result;
}

bool
TokenStream::reportError(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = reportCompileErrorNumberVA(currentToken().pos.begin, JSREPORT_ERROR, errorNumber,
                                             args);
    va_end(args);
    return result;
}

bool
TokenStream::reportWarning(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    bool result = reportCompileErrorNumberVA(currentToken().pos.begin, JSREPORT_WARNING,
                                             errorNumber, args);
    va_end(args);
    return result;
}

bool
TokenStream::reportStrictWarningErrorNumberVA(uint32_t offset, unsigned errorNumber, va_list args)
{
    if (!options().extraWarningsOption)
        return true;

    return reportCompileErrorNumberVA(offset, JSREPORT_STRICT|JSREPORT_WARNING, errorNumber, args);
}

void
TokenStream::reportAsmJSError(uint32_t offset, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);
    reportCompileErrorNumberVA(offset, JSREPORT_WARNING, errorNumber, args);
    va_end(args);
}

// We have encountered a '\': check for a Unicode escape sequence after it.
// Return 'true' and the character code value (by value) if we found a
// Unicode escape sequence.  Otherwise, return 'false'.  In both cases, do not
// advance along the buffer.
bool
TokenStream::peekUnicodeEscape(int *result)
{
    jschar cp[5];

    if (peekChars(5, cp) && cp[0] == 'u' &&
        JS7_ISHEX(cp[1]) && JS7_ISHEX(cp[2]) &&
        JS7_ISHEX(cp[3]) && JS7_ISHEX(cp[4]))
    {
        *result = (((((JS7_UNHEX(cp[1]) << 4)
                + JS7_UNHEX(cp[2])) << 4)
              + JS7_UNHEX(cp[3])) << 4)
            + JS7_UNHEX(cp[4]);
        return true;
    }
    return false;
}

bool
TokenStream::matchUnicodeEscapeIdStart(int32_t *cp)
{
    if (peekUnicodeEscape(cp) && IsIdentifierStart(*cp)) {
        skipChars(5);
        return true;
    }
    return false;
}

bool
TokenStream::matchUnicodeEscapeIdent(int32_t *cp)
{
    if (peekUnicodeEscape(cp) && IsIdentifierPart(*cp)) {
        skipChars(5);
        return true;
    }
    return false;
}

// Helper function which returns true if the first length(q) characters in p are
// the same as the characters in q.
static bool
CharsMatch(const jschar *p, const char *q) {
    while (*q) {
        if (*p++ != *q++)
            return false;
    }
    return true;
}

bool
TokenStream::getDirectives(bool isMultiline, bool shouldWarnDeprecated)
{
    // Match directive comments used in debugging, such as "//# sourceURL" and
    // "//# sourceMappingURL". Use of "//@" instead of "//#" is deprecated.
    //
    // To avoid a crashing bug in IE, several JavaScript transpilers wrap single
    // line comments containing a source mapping URL inside a multiline
    // comment. To avoid potentially expensive lookahead and backtracking, we
    // only check for this case if we encounter a '#' character.

    if (!getSourceURL(isMultiline, shouldWarnDeprecated))
        return false;
    if (!getSourceMappingURL(isMultiline, shouldWarnDeprecated))
        return false;

    return true;
}

bool
TokenStream::getDirective(bool isMultiline, bool shouldWarnDeprecated,
                          const char *directive, int directiveLength,
                          const char *errorMsgPragma, jschar **destination) {
    JS_ASSERT(directiveLength <= 18);
    jschar peeked[18];
    int32_t c;

    if (peekChars(directiveLength, peeked) && CharsMatch(peeked, directive)) {
        if (shouldWarnDeprecated &&
            !reportWarning(JSMSG_DEPRECATED_PRAGMA, errorMsgPragma))
            return false;

        skipChars(directiveLength);
        tokenbuf.clear();

        while ((c = peekChar()) && c != EOF && !IsSpaceOrBOM2(c)) {
            getChar();
            // Debugging directives can occur in both single- and multi-line
            // comments. If we're currently inside a multi-line comment, we also
            // need to recognize multi-line comment terminators.
            if (isMultiline && c == '*' && peekChar() == '/') {
                ungetChar('*');
                break;
            }
            tokenbuf.append(c);
        }

        if (tokenbuf.empty())
            // The directive's URL was missing, but this is not quite an
            // exception that we should stop and drop everything for.
            return true;

        size_t length = tokenbuf.length();

        js_free(*destination);
        *destination = cx->pod_malloc<jschar>(length + 1);
        if (!*destination)
            return false;

        PodCopy(*destination, tokenbuf.begin(), length);
        (*destination)[length] = '\0';
    }

    return true;
}

bool
TokenStream::getSourceURL(bool isMultiline, bool shouldWarnDeprecated)
{
    // Match comments of the form "//# sourceURL=<url>" or
    // "/\* //# sourceURL=<url> *\/"

    return getDirective(isMultiline, shouldWarnDeprecated, " sourceURL=", 11,
                        "sourceURL", &sourceURL_);
}

bool
TokenStream::getSourceMappingURL(bool isMultiline, bool shouldWarnDeprecated)
{
    // Match comments of the form "//# sourceMappingURL=<url>" or
    // "/\* //# sourceMappingURL=<url> *\/"

    return getDirective(isMultiline, shouldWarnDeprecated, " sourceMappingURL=", 18,
                        "sourceMappingURL", &sourceMapURL_);
}

JS_ALWAYS_INLINE Token *
TokenStream::newToken(ptrdiff_t adjust)
{
    cursor = (cursor + 1) & ntokensMask;
    Token *tp = &tokens[cursor];
    tp->pos.begin = userbuf.addressOfNextRawChar() + adjust - userbuf.base();

    // NOTE: tp->pos.end is not set until the very end of getTokenInternal().
    MOZ_MAKE_MEM_UNDEFINED(&tp->pos.end, sizeof(tp->pos.end));

    return tp;
}

JS_ALWAYS_INLINE JSAtom *
TokenStream::atomize(ExclusiveContext *cx, CharBuffer &cb)
{
    return AtomizeChars<CanGC>(cx, cb.begin(), cb.length());
}

#ifdef DEBUG
static bool
IsTokenSane(Token *tp)
{
    // Nb: TOK_EOL should never be used in an actual Token;  it should only be
    // returned as a TokenKind from peekTokenSameLine().
    if (tp->type < TOK_ERROR || tp->type >= TOK_LIMIT || tp->type == TOK_EOL)
        return false;

    if (tp->pos.end < tp->pos.begin)
        return false;

    return true;
}
#endif

bool
TokenStream::putIdentInTokenbuf(const jschar *identStart)
{
    int32_t c, qc;
    const jschar *tmp = userbuf.addressOfNextRawChar();
    userbuf.setAddressOfNextRawChar(identStart);

    tokenbuf.clear();
    for (;;) {
        c = getCharIgnoreEOL();
        if (!IsIdentifierPart(c)) {
            if (c != '\\' || !matchUnicodeEscapeIdent(&qc))
                break;
            c = qc;
        }
        if (!tokenbuf.append(c)) {
            userbuf.setAddressOfNextRawChar(tmp);
            return false;
        }
    }
    userbuf.setAddressOfNextRawChar(tmp);
    return true;
}

bool
TokenStream::checkForKeyword(const jschar *s, size_t length, TokenKind *ttp)
{
    const KeywordInfo *kw = FindKeyword(s, length);
    if (!kw)
        return true;

    if (kw->tokentype == TOK_RESERVED)
        return reportError(JSMSG_RESERVED_ID, kw->chars);

    if (kw->tokentype != TOK_STRICT_RESERVED) {
        if (kw->version <= versionNumber()) {
            // Working keyword.
            if (ttp) {
                *ttp = kw->tokentype;
                return true;
            }
            return reportError(JSMSG_RESERVED_ID, kw->chars);
        }

        // The keyword is not in this version. Treat it as an identifier, unless
        // it is let which we treat as TOK_STRICT_RESERVED by falling through to
        // the code below (ES5 forbids it in strict mode).
        if (kw->tokentype != TOK_LET)
            return true;
    }

    // Strict reserved word.
    return reportStrictModeError(JSMSG_RESERVED_ID, kw->chars);
}

enum FirstCharKind {
    // A jschar has the 'OneChar' kind if it, by itself, constitutes a valid
    // token that cannot also be a prefix of a longer token.  E.g. ';' has the
    // OneChar kind, but '+' does not, because '++' and '+=' are valid longer tokens
    // that begin with '+'.
    //
    // The few token kinds satisfying these properties cover roughly 35--45%
    // of the tokens seen in practice.
    //
    // We represent the 'OneChar' kind with any positive value less than
    // TOK_LIMIT.  This representation lets us associate each one-char token
    // jschar with a TokenKind and thus avoid a subsequent jschar-to-TokenKind
    // conversion.
    OneChar_Min = 0,
    OneChar_Max = TOK_LIMIT - 1,

    Space = TOK_LIMIT,
    Ident,
    Dec,
    String,
    EOL,
    BasePrefix,
    Other,

    LastCharKind = Other
};

// OneChar: 40,  41,  44,  58,  59,  63,  91,  93,  123, 125, 126:
//          '(', ')', ',', ':', ';', '?', '[', ']', '{', '}', '~'
// Ident:   36, 65..90, 95, 97..122: '$', 'A'..'Z', '_', 'a'..'z'
// Dot:     46: '.'
// Equals:  61: '='
// String:  34, 39: '"', '\''
// Dec:     49..57: '1'..'9'
// Plus:    43: '+'
// BasePrefix:  48: '0'
// Space:   9, 11, 12, 32: '\t', '\v', '\f', ' '
// EOL:     10, 13: '\n', '\r'
//
#define T_COMMA     TOK_COMMA
#define T_COLON     TOK_COLON
#define T_BITNOT    TOK_BITNOT
#define _______ Other
static const uint8_t firstCharKinds[] = {
/*         0        1        2        3        4        5        6        7        8        9    */
/*   0+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______,   Space,
/*  10+ */     EOL,   Space,   Space,     EOL, _______, _______, _______, _______, _______, _______,
/*  20+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  30+ */ _______, _______,   Space, _______,  String, _______,   Ident, _______, _______,  String,
/*  40+ */  TOK_LP,  TOK_RP, _______, _______, T_COMMA,_______,  _______, _______,BasePrefix,  Dec,
/*  50+ */     Dec,     Dec,     Dec,     Dec,     Dec,     Dec,     Dec,    Dec,  T_COLON,TOK_SEMI,
/*  60+ */ _______, _______, _______,TOK_HOOK, _______,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  70+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  80+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  90+ */   Ident,  TOK_LB, _______,  TOK_RB, _______,   Ident, _______,   Ident,   Ident,   Ident,
/* 100+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/* 110+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/* 120+ */   Ident,   Ident,   Ident,  TOK_LC, _______,  TOK_RC,T_BITNOT, _______
};
#undef T_COMMA
#undef T_COLON
#undef T_BITNOT
#undef _______

static_assert(LastCharKind < (1 << (sizeof(firstCharKinds[0]) * 8)),
              "Elements of firstCharKinds[] are too small");

TokenKind
TokenStream::getTokenInternal(Modifier modifier)
{
    int c, qc;
    Token *tp;
    FirstCharKind c1kind;
    const jschar *numStart;
    bool hasExp;
    DecimalPoint decimalPoint;
    const jschar *identStart;
    bool hadUnicodeEscape;

  retry:
    if (JS_UNLIKELY(!userbuf.hasRawChars())) {
        tp = newToken(0);
        tp->type = TOK_EOF;
        flags.isEOF = true;
        goto out;
    }

    c = userbuf.getRawChar();
    JS_ASSERT(c != EOF);

    // Chars not in the range 0..127 are rare.  Getting them out of the way
    // early allows subsequent checking to be faster.
    if (JS_UNLIKELY(c >= 128)) {
        if (IsSpaceOrBOM2(c)) {
            if (c == LINE_SEPARATOR || c == PARA_SEPARATOR) {
                updateLineInfoForEOL();
                updateFlagsForEOL();
            }

            goto retry;
        }

        tp = newToken(-1);

        // '$' and '_' don't pass IsLetter, but they're < 128 so never appear here.
        JS_STATIC_ASSERT('$' < 128 && '_' < 128);
        if (IsLetter(c)) {
            identStart = userbuf.addressOfNextRawChar() - 1;
            hadUnicodeEscape = false;
            goto identifier;
        }

        goto badchar;
    }

    // Get the token kind, based on the first char.  The ordering of c1kind
    // comparison is based on the frequency of tokens in real code -- Parsemark
    // (which represents typical JS code on the web) and the Unreal demo (which
    // represents asm.js code).
    //
    //                  Parsemark   Unreal
    //  OneChar         32.9%       39.7%
    //  Space           25.0%        0.6%
    //  Ident           19.2%       36.4%
    //  Dec              7.2%        5.1%
    //  String           7.9%        0.0%
    //  EOL              1.7%        0.0%
    //  BasePrefix       0.4%        4.9%
    //  Other            5.7%       13.3%
    //
    // The ordering is based mostly only Parsemark frequencies, with Unreal
    // frequencies used to break close categories (e.g. |Dec| and |String|).
    // |Other| is biggish, but no other token kind is common enough for it to
    // be worth adding extra values to FirstCharKind.
    //
    c1kind = FirstCharKind(firstCharKinds[c]);

    // Look for an unambiguous single-char token.
    //
    if (c1kind <= OneChar_Max) {
        tp = newToken(-1);
        tp->type = TokenKind(c1kind);
        goto out;
    }

    // Skip over non-EOL whitespace chars.
    //
    if (c1kind == Space)
        goto retry;

    // Look for an identifier.
    //
    if (c1kind == Ident) {
        tp = newToken(-1);
        identStart = userbuf.addressOfNextRawChar() - 1;
        hadUnicodeEscape = false;

      identifier:
        for (;;) {
            c = getCharIgnoreEOL();
            if (c == EOF)
                break;
            if (!IsIdentifierPart(c)) {
                if (c != '\\' || !matchUnicodeEscapeIdent(&qc))
                    break;
                hadUnicodeEscape = true;
            }
        }
        ungetCharIgnoreEOL(c);

        // Identifiers containing no Unicode escapes can be processed directly
        // from userbuf.  The rest must use the escapes converted via tokenbuf
        // before atomizing.
        const jschar *chars;
        size_t length;
        if (hadUnicodeEscape) {
            if (!putIdentInTokenbuf(identStart))
                goto error;

            chars = tokenbuf.begin();
            length = tokenbuf.length();
        } else {
            chars = identStart;
            length = userbuf.addressOfNextRawChar() - identStart;
        }

        // Check for keywords unless the parser told us not to.
        if (modifier != KeywordIsName) {
            tp->type = TOK_NAME;
            if (!checkForKeyword(chars, length, &tp->type))
                goto error;
            if (tp->type != TOK_NAME)
                goto out;
        }

        JSAtom *atom = AtomizeChars<CanGC>(cx, chars, length);
        if (!atom)
            goto error;
        tp->type = TOK_NAME;
        tp->setName(atom->asPropertyName());
        goto out;
    }

    // Look for a decimal number.
    //
    if (c1kind == Dec) {
        tp = newToken(-1);
        numStart = userbuf.addressOfNextRawChar() - 1;

      decimal:
        decimalPoint = NoDecimal;
        hasExp = false;
        while (JS7_ISDEC(c))
            c = getCharIgnoreEOL();

        if (c == '.') {
            decimalPoint = HasDecimal;
          decimal_dot:
            do {
                c = getCharIgnoreEOL();
            } while (JS7_ISDEC(c));
        }
        if (c == 'e' || c == 'E') {
            hasExp = true;
            c = getCharIgnoreEOL();
            if (c == '+' || c == '-')
                c = getCharIgnoreEOL();
            if (!JS7_ISDEC(c)) {
                ungetCharIgnoreEOL(c);
                reportError(JSMSG_MISSING_EXPONENT);
                goto error;
            }
            do {
                c = getCharIgnoreEOL();
            } while (JS7_ISDEC(c));
        }
        ungetCharIgnoreEOL(c);

        if (c != EOF && IsIdentifierStart(c)) {
            reportError(JSMSG_IDSTART_AFTER_NUMBER);
            goto error;
        }

        // Unlike identifiers and strings, numbers cannot contain escaped
        // chars, so we don't need to use tokenbuf.  Instead we can just
        // convert the jschars in userbuf directly to the numeric value.
        double dval;
        if (!((decimalPoint == HasDecimal) || hasExp)) {
            if (!GetDecimalInteger(cx, numStart, userbuf.addressOfNextRawChar(), &dval))
                goto error;
        } else {
            const jschar *dummy;
            if (!js_strtod(cx, numStart, userbuf.addressOfNextRawChar(), &dummy, &dval))
                goto error;
        }
        tp->type = TOK_NUMBER;
        tp->setNumber(dval, decimalPoint);
        goto out;
    }

    // Look for a string.
    //
    if (c1kind == String) {
        tp = newToken(-1);
        qc = c;
        tokenbuf.clear();
        while (true) {
            // We need to detect any of these chars:  " or ', \n (or its
            // equivalents), \\, EOF.  We use maybeStrSpecial[] in a manner
            // similar to maybeEOL[], see above.  Because we detect EOL
            // sequences here and put them back immediately, we can use
            // getCharIgnoreEOL().
            c = getCharIgnoreEOL();
            if (maybeStrSpecial[c & 0xff]) {
                if (c == qc)
                    break;
                if (c == '\\') {
                    switch (c = getChar()) {
                      case 'b': c = '\b'; break;
                      case 'f': c = '\f'; break;
                      case 'n': c = '\n'; break;
                      case 'r': c = '\r'; break;
                      case 't': c = '\t'; break;
                      case 'v': c = '\v'; break;

                      default:
                        if ('0' <= c && c < '8') {
                            int32_t val = JS7_UNDEC(c);

                            c = peekChar();
                            // Strict mode code allows only \0, then a non-digit.
                            if (val != 0 || JS7_ISDEC(c)) {
                                if (!reportStrictModeError(JSMSG_DEPRECATED_OCTAL))
                                    goto error;
                                flags.sawOctalEscape = true;
                            }
                            if ('0' <= c && c < '8') {
                                val = 8 * val + JS7_UNDEC(c);
                                getChar();
                                c = peekChar();
                                if ('0' <= c && c < '8') {
                                    int32_t save = val;
                                    val = 8 * val + JS7_UNDEC(c);
                                    if (val <= 0377)
                                        getChar();
                                    else
                                        val = save;
                                }
                            }

                            c = jschar(val);
                        } else if (c == 'u') {
                            jschar cp[4];
                            if (peekChars(4, cp) &&
                                JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1]) &&
                                JS7_ISHEX(cp[2]) && JS7_ISHEX(cp[3])) {
                                c = (((((JS7_UNHEX(cp[0]) << 4)
                                        + JS7_UNHEX(cp[1])) << 4)
                                      + JS7_UNHEX(cp[2])) << 4)
                                    + JS7_UNHEX(cp[3]);
                                skipChars(4);
                            } else {
                                reportError(JSMSG_MALFORMED_ESCAPE, "Unicode");
                                goto error;
                            }
                        } else if (c == 'x') {
                            jschar cp[2];
                            if (peekChars(2, cp) &&
                                JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1])) {
                                c = (JS7_UNHEX(cp[0]) << 4) + JS7_UNHEX(cp[1]);
                                skipChars(2);
                            } else {
                                reportError(JSMSG_MALFORMED_ESCAPE, "hexadecimal");
                                goto error;
                            }
                        } else if (c == '\n') {
                            // ES5 7.8.4: an escaped line terminator represents
                            // no character.
                            continue;
                        }
                        break;
                    }
                } else if (TokenBuf::isRawEOLChar(c) || c == EOF) {
                    ungetCharIgnoreEOL(c);
                    reportError(JSMSG_UNTERMINATED_STRING);
                    goto error;
                }
            }
            if (!tokenbuf.append(c))
                goto error;
        }
        JSAtom *atom = atomize(cx, tokenbuf);
        if (!atom)
            goto error;
        tp->type = TOK_STRING;
        tp->setAtom(atom);
        goto out;
    }

    // Skip over EOL chars, updating line state along the way.
    //
    if (c1kind == EOL) {
        // If it's a \r\n sequence: treat as a single EOL, skip over the \n.
        if (c == '\r' && userbuf.hasRawChars())
            userbuf.matchRawChar('\n');
        updateLineInfoForEOL();
        updateFlagsForEOL();
        goto retry;
    }

    // Look for a hexadecimal, octal, or binary number.
    //
    if (c1kind == BasePrefix) {
        tp = newToken(-1);
        int radix;
        c = getCharIgnoreEOL();
        if (c == 'x' || c == 'X') {
            radix = 16;
            c = getCharIgnoreEOL();
            if (!JS7_ISHEX(c)) {
                ungetCharIgnoreEOL(c);
                reportError(JSMSG_MISSING_HEXDIGITS);
                goto error;
            }
            numStart = userbuf.addressOfNextRawChar() - 1;  // one past the '0x'
            while (JS7_ISHEX(c))
                c = getCharIgnoreEOL();
        } else if (c == 'b' || c == 'B') {
            radix = 2;
            c = getCharIgnoreEOL();
            if (c != '0' && c != '1') {
                ungetCharIgnoreEOL(c);
                reportError(JSMSG_MISSING_BINARY_DIGITS);
                goto error;
            }
            numStart = userbuf.addressOfNextRawChar() - 1;  // one past the '0b'
            while (c == '0' || c == '1')
                c = getCharIgnoreEOL();
        } else if (c == 'o' || c == 'O') {
            radix = 8;
            c = getCharIgnoreEOL();
            if (c < '0' || c > '7') {
                ungetCharIgnoreEOL(c);
                reportError(JSMSG_MISSING_OCTAL_DIGITS);
                goto error;
            }
            numStart = userbuf.addressOfNextRawChar() - 1;  // one past the '0o'
            while ('0' <= c && c <= '7')
                c = getCharIgnoreEOL();
        } else if (JS7_ISDEC(c)) {
            radix = 8;
            numStart = userbuf.addressOfNextRawChar() - 1;  // one past the '0'
            while (JS7_ISDEC(c)) {
                // Octal integer literals are not permitted in strict mode code.
                if (!reportStrictModeError(JSMSG_DEPRECATED_OCTAL))
                    goto error;

                // Outside strict mode, we permit 08 and 09 as decimal numbers,
                // which makes our behaviour a superset of the ECMA numeric
                // grammar. We might not always be so permissive, so we warn
                // about it.
                if (c >= '8') {
                    if (!reportWarning(JSMSG_BAD_OCTAL, c == '8' ? "08" : "09")) {
                        goto error;
                    }
                    goto decimal;   // use the decimal scanner for the rest of the number
                }
                c = getCharIgnoreEOL();
            }
        } else {
            // '0' not followed by 'x', 'X' or a digit;  scan as a decimal number.
            numStart = userbuf.addressOfNextRawChar() - 1;
            goto decimal;
        }
        ungetCharIgnoreEOL(c);

        if (c != EOF && IsIdentifierStart(c)) {
            reportError(JSMSG_IDSTART_AFTER_NUMBER);
            goto error;
        }

        double dval;
        const jschar *dummy;
        if (!GetPrefixInteger(cx, numStart, userbuf.addressOfNextRawChar(), radix, &dummy, &dval))
            goto error;
        tp->type = TOK_NUMBER;
        tp->setNumber(dval, NoDecimal);
        goto out;
    }

    // This handles everything else.
    //
    JS_ASSERT(c1kind == Other);
    tp = newToken(-1);
    switch (c) {
      case '.':
        c = getCharIgnoreEOL();
        if (JS7_ISDEC(c)) {
            numStart = userbuf.addressOfNextRawChar() - 2;
            decimalPoint = HasDecimal;
            hasExp = false;
            goto decimal_dot;
        }
        if (c == '.') {
            if (matchChar('.')) {
                tp->type = TOK_TRIPLEDOT;
                goto out;
            }
        }
        ungetCharIgnoreEOL(c);
        tp->type = TOK_DOT;
        goto out;

      case '=':
        if (matchChar('='))
            tp->type = matchChar('=') ? TOK_STRICTEQ : TOK_EQ;
        else if (matchChar('>'))
            tp->type = TOK_ARROW;
        else
            tp->type = TOK_ASSIGN;
        goto out;

      case '+':
        if (matchChar('+'))
            tp->type = TOK_INC;
        else
            tp->type = matchChar('=') ? TOK_ADDASSIGN : TOK_ADD;
        goto out;

      case '\\':
        hadUnicodeEscape = matchUnicodeEscapeIdStart(&qc);
        if (hadUnicodeEscape) {
            identStart = userbuf.addressOfNextRawChar() - 6;
            goto identifier;
        }
        goto badchar;

      case '|':
        if (matchChar('|'))
            tp->type = TOK_OR;
        else
            tp->type = matchChar('=') ? TOK_BITORASSIGN : TOK_BITOR;
        goto out;

      case '^':
        tp->type = matchChar('=') ? TOK_BITXORASSIGN : TOK_BITXOR;
        goto out;

      case '&':
        if (matchChar('&'))
            tp->type = TOK_AND;
        else
            tp->type = matchChar('=') ? TOK_BITANDASSIGN : TOK_BITAND;
        goto out;

      case '!':
        if (matchChar('='))
            tp->type = matchChar('=') ? TOK_STRICTNE : TOK_NE;
        else
            tp->type = TOK_NOT;
        goto out;

      case '<':
        // NB: treat HTML begin-comment as comment-till-end-of-line.
        if (matchChar('!')) {
            if (matchChar('-')) {
                if (matchChar('-'))
                    goto skipline;
                ungetChar('-');
            }
            ungetChar('!');
        }
        if (matchChar('<')) {
            tp->type = matchChar('=') ? TOK_LSHASSIGN : TOK_LSH;
        } else {
            tp->type = matchChar('=') ? TOK_LE : TOK_LT;
        }
        goto out;

      case '>':
        if (matchChar('>')) {
            if (matchChar('>'))
                tp->type = matchChar('=') ? TOK_URSHASSIGN : TOK_URSH;
            else
                tp->type = matchChar('=') ? TOK_RSHASSIGN : TOK_RSH;
        } else {
            tp->type = matchChar('=') ? TOK_GE : TOK_GT;
        }
        goto out;

      case '*':
        tp->type = matchChar('=') ? TOK_MULASSIGN : TOK_MUL;
        goto out;

      case '/':
        // Look for a single-line comment.
        if (matchChar('/')) {
            c = peekChar();
            if (c == '@' || c == '#') {
                bool shouldWarn = getChar() == '@';
                if (!getDirectives(false, shouldWarn))
                    goto error;
            }

        skipline:
            while ((c = getChar()) != EOF && c != '\n')
                continue;
            ungetChar(c);
            cursor = (cursor - 1) & ntokensMask;
            goto retry;
        }

        // Look for a multi-line comment.
        if (matchChar('*')) {
            unsigned linenoBefore = lineno;
            while ((c = getChar()) != EOF &&
                   !(c == '*' && matchChar('/'))) {
                if (c == '@' || c == '#') {
                    bool shouldWarn = c == '@';
                    if (!getDirectives(true, shouldWarn))
                        goto error;
                }
            }
            if (c == EOF) {
                reportError(JSMSG_UNTERMINATED_COMMENT);
                goto error;
            }
            if (linenoBefore != lineno)
                updateFlagsForEOL();
            cursor = (cursor - 1) & ntokensMask;
            goto retry;
        }

        // Look for a regexp.
        if (modifier == Operand) {
            tokenbuf.clear();

            bool inCharClass = false;
            for (;;) {
                c = getChar();
                if (c == '\\') {
                    if (!tokenbuf.append(c))
                        goto error;
                    c = getChar();
                } else if (c == '[') {
                    inCharClass = true;
                } else if (c == ']') {
                    inCharClass = false;
                } else if (c == '/' && !inCharClass) {
                    // For compat with IE, allow unescaped / in char classes.
                    break;
                }
                if (c == '\n' || c == EOF) {
                    ungetChar(c);
                    reportError(JSMSG_UNTERMINATED_REGEXP);
                    goto error;
                }
                if (!tokenbuf.append(c))
                    goto error;
            }

            RegExpFlag reflags = NoFlags;
            unsigned length = tokenbuf.length() + 1;
            while (true) {
                c = peekChar();
                if (c == 'g' && !(reflags & GlobalFlag))
                    reflags = RegExpFlag(reflags | GlobalFlag);
                else if (c == 'i' && !(reflags & IgnoreCaseFlag))
                    reflags = RegExpFlag(reflags | IgnoreCaseFlag);
                else if (c == 'm' && !(reflags & MultilineFlag))
                    reflags = RegExpFlag(reflags | MultilineFlag);
                else if (c == 'y' && !(reflags & StickyFlag))
                    reflags = RegExpFlag(reflags | StickyFlag);
                else
                    break;
                getChar();
                length++;
            }

            c = peekChar();
            if (JS7_ISLET(c)) {
                char buf[2] = { '\0', '\0' };
                tp->pos.begin += length + 1;
                buf[0] = char(c);
                reportError(JSMSG_BAD_REGEXP_FLAG, buf);
                (void) getChar();
                goto error;
            }
            tp->type = TOK_REGEXP;
            tp->setRegExpFlags(reflags);
            goto out;
        }

        tp->type = matchChar('=') ? TOK_DIVASSIGN : TOK_DIV;
        goto out;

      case '%':
        tp->type = matchChar('=') ? TOK_MODASSIGN : TOK_MOD;
        goto out;

      case '-':
        if (matchChar('-')) {
            if (peekChar() == '>' && !flags.isDirtyLine)
                goto skipline;
            tp->type = TOK_DEC;
        } else {
            tp->type = matchChar('=') ? TOK_SUBASSIGN : TOK_SUB;
        }
        goto out;

      badchar:
      default:
        reportError(JSMSG_ILLEGAL_CHARACTER);
        goto error;
    }

    MOZ_ASSUME_UNREACHABLE("should have jumped to |out| or |error|");

  out:
    flags.isDirtyLine = true;
    tp->pos.end = userbuf.addressOfNextRawChar() - userbuf.base();
    JS_ASSERT(IsTokenSane(tp));
    return tp->type;

  error:
    flags.isDirtyLine = true;
    tp->pos.end = userbuf.addressOfNextRawChar() - userbuf.base();
    tp->type = TOK_ERROR;
    JS_ASSERT(IsTokenSane(tp));
    onError();
    return TOK_ERROR;
}

void
TokenStream::onError()
{
    flags.hadError = true;
#ifdef DEBUG
    // Poisoning userbuf on error establishes an invariant: once an erroneous
    // token has been seen, userbuf will not be consulted again.  This is true
    // because the parser will either (a) deal with the TOK_ERROR token by
    // aborting parsing immediately; or (b) if the TOK_ERROR token doesn't
    // match what it expected, it will unget the token, and the next getToken()
    // call will immediately return the just-gotten TOK_ERROR token again
    // without consulting userbuf, thanks to the lookahead buffer.
    userbuf.poison();
#endif
}

JS_FRIEND_API(int)
js_fgets(char *buf, int size, FILE *file)
{
    int n, i, c;
    bool crflag;

    n = size - 1;
    if (n < 0)
        return -1;

    crflag = false;
    for (i = 0; i < n && (c = fast_getc(file)) != EOF; i++) {
        buf[i] = c;
        if (c == '\n') {        // any \n ends a line
            i++;                // keep the \n; we know there is room for \0
            break;
        }
        if (crflag) {           // \r not followed by \n ends line at the \r
            ungetc(c, file);
            break;              // and overwrite c in buf with \0
        }
        crflag = (c == '\r');
    }

    buf[i] = '\0';
    return i;
}

#ifdef DEBUG
const char *
TokenKindToString(TokenKind tt)
{
    switch (tt) {
      case TOK_ERROR:           return "TOK_ERROR";
      case TOK_EOF:             return "TOK_EOF";
      case TOK_EOL:             return "TOK_EOL";
      case TOK_SEMI:            return "TOK_SEMI";
      case TOK_COMMA:           return "TOK_COMMA";
      case TOK_HOOK:            return "TOK_HOOK";
      case TOK_COLON:           return "TOK_COLON";
      case TOK_OR:              return "TOK_OR";
      case TOK_AND:             return "TOK_AND";
      case TOK_BITOR:           return "TOK_BITOR";
      case TOK_BITXOR:          return "TOK_BITXOR";
      case TOK_BITAND:          return "TOK_BITAND";
      case TOK_ADD:             return "TOK_ADD";
      case TOK_SUB:             return "TOK_SUB";
      case TOK_MUL:             return "TOK_MUL";
      case TOK_DIV:             return "TOK_DIV";
      case TOK_MOD:             return "TOK_MOD";
      case TOK_INC:             return "TOK_INC";
      case TOK_DEC:             return "TOK_DEC";
      case TOK_DOT:             return "TOK_DOT";
      case TOK_TRIPLEDOT:       return "TOK_TRIPLEDOT";
      case TOK_LB:              return "TOK_LB";
      case TOK_RB:              return "TOK_RB";
      case TOK_LC:              return "TOK_LC";
      case TOK_RC:              return "TOK_RC";
      case TOK_LP:              return "TOK_LP";
      case TOK_RP:              return "TOK_RP";
      case TOK_ARROW:           return "TOK_ARROW";
      case TOK_NAME:            return "TOK_NAME";
      case TOK_NUMBER:          return "TOK_NUMBER";
      case TOK_STRING:          return "TOK_STRING";
      case TOK_REGEXP:          return "TOK_REGEXP";
      case TOK_TRUE:            return "TOK_TRUE";
      case TOK_FALSE:           return "TOK_FALSE";
      case TOK_NULL:            return "TOK_NULL";
      case TOK_THIS:            return "TOK_THIS";
      case TOK_FUNCTION:        return "TOK_FUNCTION";
      case TOK_IF:              return "TOK_IF";
      case TOK_ELSE:            return "TOK_ELSE";
      case TOK_SWITCH:          return "TOK_SWITCH";
      case TOK_CASE:            return "TOK_CASE";
      case TOK_DEFAULT:         return "TOK_DEFAULT";
      case TOK_WHILE:           return "TOK_WHILE";
      case TOK_DO:              return "TOK_DO";
      case TOK_FOR:             return "TOK_FOR";
      case TOK_BREAK:           return "TOK_BREAK";
      case TOK_CONTINUE:        return "TOK_CONTINUE";
      case TOK_IN:              return "TOK_IN";
      case TOK_VAR:             return "TOK_VAR";
      case TOK_CONST:           return "TOK_CONST";
      case TOK_WITH:            return "TOK_WITH";
      case TOK_RETURN:          return "TOK_RETURN";
      case TOK_NEW:             return "TOK_NEW";
      case TOK_DELETE:          return "TOK_DELETE";
      case TOK_TRY:             return "TOK_TRY";
      case TOK_CATCH:           return "TOK_CATCH";
      case TOK_FINALLY:         return "TOK_FINALLY";
      case TOK_THROW:           return "TOK_THROW";
      case TOK_INSTANCEOF:      return "TOK_INSTANCEOF";
      case TOK_DEBUGGER:        return "TOK_DEBUGGER";
      case TOK_YIELD:           return "TOK_YIELD";
      case TOK_LET:             return "TOK_LET";
      case TOK_RESERVED:        return "TOK_RESERVED";
      case TOK_STRICT_RESERVED: return "TOK_STRICT_RESERVED";
      case TOK_STRICTEQ:        return "TOK_STRICTEQ";
      case TOK_EQ:              return "TOK_EQ";
      case TOK_STRICTNE:        return "TOK_STRICTNE";
      case TOK_NE:              return "TOK_NE";
      case TOK_TYPEOF:          return "TOK_TYPEOF";
      case TOK_VOID:            return "TOK_VOID";
      case TOK_NOT:             return "TOK_NOT";
      case TOK_BITNOT:          return "TOK_BITNOT";
      case TOK_LT:              return "TOK_LT";
      case TOK_LE:              return "TOK_LE";
      case TOK_GT:              return "TOK_GT";
      case TOK_GE:              return "TOK_GE";
      case TOK_LSH:             return "TOK_LSH";
      case TOK_RSH:             return "TOK_RSH";
      case TOK_URSH:            return "TOK_URSH";
      case TOK_ASSIGN:          return "TOK_ASSIGN";
      case TOK_ADDASSIGN:       return "TOK_ADDASSIGN";
      case TOK_SUBASSIGN:       return "TOK_SUBASSIGN";
      case TOK_BITORASSIGN:     return "TOK_BITORASSIGN";
      case TOK_BITXORASSIGN:    return "TOK_BITXORASSIGN";
      case TOK_BITANDASSIGN:    return "TOK_BITANDASSIGN";
      case TOK_LSHASSIGN:       return "TOK_LSHASSIGN";
      case TOK_RSHASSIGN:       return "TOK_RSHASSIGN";
      case TOK_URSHASSIGN:      return "TOK_URSHASSIGN";
      case TOK_MULASSIGN:       return "TOK_MULASSIGN";
      case TOK_DIVASSIGN:       return "TOK_DIVASSIGN";
      case TOK_MODASSIGN:       return "TOK_MODASSIGN";
      case TOK_EXPORT:          return "TOK_EXPORT";
      case TOK_IMPORT:          return "TOK_IMPORT";
      case TOK_LIMIT:           break;
    }

    return "<bad TokenKind>";
}
#endif
