/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// JS lexical scanner.

#include "frontend/TokenStream.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/IntegerTypeTraits.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/PodOperations.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/TextUtils.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "jsexn.h"
#include "jsnum.h"

#include "frontend/BytecodeCompiler.h"
#include "frontend/Parser.h"
#include "frontend/ReservedWords.h"
#include "js/CharacterEncoding.h"
#include "js/UniquePtr.h"
#include "util/StringBuffer.h"
#include "util/Unicode.h"
#include "vm/HelperThreads.h"
#include "vm/JSAtom.h"
#include "vm/JSCompartment.h"
#include "vm/JSContext.h"

using mozilla::ArrayLength;
using mozilla::IsAsciiAlpha;
using mozilla::IsAsciiDigit;
using mozilla::MakeScopeExit;
using mozilla::PodArrayZero;
using mozilla::PodCopy;

struct ReservedWordInfo
{
    const char* chars;         // C string with reserved word text
    js::frontend::TokenKind tokentype;
};

static const ReservedWordInfo reservedWords[] = {
#define RESERVED_WORD_INFO(word, name, type) \
    {js_##word##_str, js::frontend::type},
    FOR_EACH_JAVASCRIPT_RESERVED_WORD(RESERVED_WORD_INFO)
#undef RESERVED_WORD_INFO
};

// Returns a ReservedWordInfo for the specified characters, or nullptr if the
// string is not a reserved word.
template <typename CharT>
static const ReservedWordInfo*
FindReservedWord(const CharT* s, size_t length)
{
    MOZ_ASSERT(length != 0);

    size_t i;
    const ReservedWordInfo* rw;
    const char* chars;

#define JSRW_LENGTH()           length
#define JSRW_AT(column)         s[column]
#define JSRW_GOT_MATCH(index)   i = (index); goto got_match;
#define JSRW_TEST_GUESS(index)  i = (index); goto test_guess;
#define JSRW_NO_MATCH()         goto no_match;
#include "frontend/ReservedWordsGenerated.h"
#undef JSRW_NO_MATCH
#undef JSRW_TEST_GUESS
#undef JSRW_GOT_MATCH
#undef JSRW_AT
#undef JSRW_LENGTH

  got_match:
    return &reservedWords[i];

  test_guess:
    rw = &reservedWords[i];
    chars = rw->chars;
    do {
        if (*s++ != (unsigned char)(*chars++))
            goto no_match;
    } while (--length != 0);
    return rw;

  no_match:
    return nullptr;
}

static const ReservedWordInfo*
FindReservedWord(JSLinearString* str)
{
    JS::AutoCheckCannotGC nogc;
    return str->hasLatin1Chars()
           ? FindReservedWord(str->latin1Chars(nogc), str->length())
           : FindReservedWord(str->twoByteChars(nogc), str->length());
}

template <typename CharT>
static bool
IsIdentifier(const CharT* chars, size_t length)
{
    using namespace js;

    if (length == 0)
        return false;

    if (!unicode::IsIdentifierStart(char16_t(*chars)))
        return false;

    const CharT* end = chars + length;
    while (++chars != end) {
        if (!unicode::IsIdentifierPart(char16_t(*chars)))
            return false;
    }

    return true;
}

static uint32_t
GetSingleCodePoint(const char16_t** p, const char16_t* end)
{
    using namespace js;

    uint32_t codePoint;
    if (MOZ_UNLIKELY(unicode::IsLeadSurrogate(**p)) && *p + 1 < end) {
        char16_t lead = **p;
        char16_t maybeTrail = *(*p + 1);
        if (unicode::IsTrailSurrogate(maybeTrail)) {
            *p += 2;
            return unicode::UTF16Decode(lead, maybeTrail);
        }
    }

    codePoint = **p;
    (*p)++;
    return codePoint;
}

static bool
IsIdentifierMaybeNonBMP(const char16_t* chars, size_t length)
{
    using namespace js;

    if (IsIdentifier(chars, length))
        return true;

    if (length == 0)
        return false;

    const char16_t* p = chars;
    const char16_t* end = chars + length;
    uint32_t codePoint;

    codePoint = GetSingleCodePoint(&p, end);
    if (!unicode::IsIdentifierStart(codePoint))
        return false;

    while (p < end) {
        codePoint = GetSingleCodePoint(&p, end);
        if (!unicode::IsIdentifierPart(codePoint))
            return false;
    }

    return true;
}

namespace js {

namespace frontend {

bool
IsIdentifier(JSLinearString* str)
{
    JS::AutoCheckCannotGC nogc;
    MOZ_ASSERT(str);
    if (str->hasLatin1Chars())
        return ::IsIdentifier(str->latin1Chars(nogc), str->length());
    return ::IsIdentifierMaybeNonBMP(str->twoByteChars(nogc), str->length());
}

bool
IsIdentifier(const char* chars, size_t length)
{
    return ::IsIdentifier(chars, length);
}

bool
IsIdentifier(const char16_t* chars, size_t length)
{
    return ::IsIdentifier(chars, length);
}

bool
IsKeyword(JSLinearString* str)
{
    if (const ReservedWordInfo* rw = FindReservedWord(str))
        return TokenKindIsKeyword(rw->tokentype);

    return false;
}

TokenKind
ReservedWordTokenKind(PropertyName* str)
{
    if (const ReservedWordInfo* rw = FindReservedWord(str))
        return rw->tokentype;

    return TokenKind::Name;
}

const char*
ReservedWordToCharZ(PropertyName* str)
{
    if (const ReservedWordInfo* rw = FindReservedWord(str))
        return ReservedWordToCharZ(rw->tokentype);

    return nullptr;
}

const char*
ReservedWordToCharZ(TokenKind tt)
{
    MOZ_ASSERT(tt != TokenKind::Name);
    switch (tt) {
#define EMIT_CASE(word, name, type) case type: return js_##word##_str;
      FOR_EACH_JAVASCRIPT_RESERVED_WORD(EMIT_CASE)
#undef EMIT_CASE
      default:
        MOZ_ASSERT_UNREACHABLE("Not a reserved word PropertyName.");
    }
    return nullptr;
}

PropertyName*
TokenStreamAnyChars::reservedWordToPropertyName(TokenKind tt) const
{
    MOZ_ASSERT(tt != TokenKind::Name);
    switch (tt) {
#define EMIT_CASE(word, name, type) case type: return cx->names().name;
      FOR_EACH_JAVASCRIPT_RESERVED_WORD(EMIT_CASE)
#undef EMIT_CASE
      default:
        MOZ_ASSERT_UNREACHABLE("Not a reserved word TokenKind.");
    }
    return nullptr;
}

TokenStreamAnyChars::SourceCoords::SourceCoords(JSContext* cx, uint32_t ln, uint32_t col,
                                                uint32_t initialLineOffset)
  : lineStartOffsets_(cx), initialLineNum_(ln), initialColumn_(col), lastLineIndex_(0)
{
    // This is actually necessary!  Removing it causes compile errors on
    // GCC and clang.  You could try declaring this:
    //
    //   const uint32_t TokenStreamAnyChars::SourceCoords::MAX_PTR;
    //
    // which fixes the GCC/clang error, but causes bustage on Windows.  Sigh.
    //
    uint32_t maxPtr = MAX_PTR;

    // The first line begins at buffer offset |initialLineOffset|.  MAX_PTR is
    // the sentinel.  The appends cannot fail because |lineStartOffsets_| has
    // statically-allocated elements.
    MOZ_ASSERT(lineStartOffsets_.capacity() >= 2);
    MOZ_ALWAYS_TRUE(lineStartOffsets_.reserve(2));
    lineStartOffsets_.infallibleAppend(initialLineOffset);
    lineStartOffsets_.infallibleAppend(maxPtr);
}

MOZ_ALWAYS_INLINE bool
TokenStreamAnyChars::SourceCoords::add(uint32_t lineNum, uint32_t lineStartOffset)
{
    uint32_t lineIndex = lineNumToIndex(lineNum);
    uint32_t sentinelIndex = lineStartOffsets_.length() - 1;

    MOZ_ASSERT(lineStartOffsets_[0] <= lineStartOffset &&
               lineStartOffsets_[sentinelIndex] == MAX_PTR);

    if (lineIndex == sentinelIndex) {
        // We haven't seen this newline before.  Update lineStartOffsets_
        // only if lineStartOffsets_.append succeeds, to keep sentinel.
        // Otherwise return false to tell TokenStream about OOM.
        uint32_t maxPtr = MAX_PTR;
        if (!lineStartOffsets_.append(maxPtr)) {
            static_assert(mozilla::IsSame<decltype(lineStartOffsets_.allocPolicy()),
                                          TempAllocPolicy&>::value,
                          "this function's caller depends on it reporting an "
                          "error on failure, as TempAllocPolicy ensures");
            return false;
        }

        lineStartOffsets_[lineIndex] = lineStartOffset;
    } else {
        // We have seen this newline before (and ungot it).  Do nothing (other
        // than checking it hasn't mysteriously changed).
        // This path can be executed after hitting OOM, so check lineIndex.
        MOZ_ASSERT_IF(lineIndex < sentinelIndex, lineStartOffsets_[lineIndex] == lineStartOffset);
    }
    return true;
}

MOZ_ALWAYS_INLINE bool
TokenStreamAnyChars::SourceCoords::fill(const TokenStreamAnyChars::SourceCoords& other)
{
    MOZ_ASSERT(lineStartOffsets_[0] == other.lineStartOffsets_[0]);
    MOZ_ASSERT(lineStartOffsets_.back() == MAX_PTR);
    MOZ_ASSERT(other.lineStartOffsets_.back() == MAX_PTR);

    if (lineStartOffsets_.length() >= other.lineStartOffsets_.length())
        return true;

    uint32_t sentinelIndex = lineStartOffsets_.length() - 1;
    lineStartOffsets_[sentinelIndex] = other.lineStartOffsets_[sentinelIndex];

    for (size_t i = sentinelIndex + 1; i < other.lineStartOffsets_.length(); i++) {
        if (!lineStartOffsets_.append(other.lineStartOffsets_[i]))
            return false;
    }
    return true;
}

MOZ_ALWAYS_INLINE uint32_t
TokenStreamAnyChars::SourceCoords::lineIndexOf(uint32_t offset) const
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
        MOZ_ASSERT(iMin < lineStartOffsets_.length() - 1);   // -1 due to the sentinel

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
    MOZ_ASSERT(iMax == iMin);
    MOZ_ASSERT(lineStartOffsets_[iMin] <= offset && offset < lineStartOffsets_[iMin + 1]);
    lastLineIndex_ = iMin;
    return iMin;
}

uint32_t
TokenStreamAnyChars::SourceCoords::lineNum(uint32_t offset) const
{
    uint32_t lineIndex = lineIndexOf(offset);
    return lineIndexToNum(lineIndex);
}

uint32_t
TokenStreamAnyChars::SourceCoords::columnIndex(uint32_t offset) const
{
    return lineIndexAndOffsetToColumn(lineIndexOf(offset), offset);
}

void
TokenStreamAnyChars::SourceCoords::lineNumAndColumnIndex(uint32_t offset, uint32_t* lineNum,
                                                         uint32_t* column) const
{
    uint32_t lineIndex = lineIndexOf(offset);
    *lineNum = lineIndexToNum(lineIndex);
    *column = lineIndexAndOffsetToColumn(lineIndex, offset);
}

TokenStreamAnyChars::TokenStreamAnyChars(JSContext* cx, const ReadOnlyCompileOptions& options,
                                         StrictModeGetter* smg)
  : srcCoords(cx, options.lineno, options.column, options.scriptSourceOffset),
    options_(options),
    tokens(),
    cursor_(0),
    lookahead(),
    lineno(options.lineno),
    flags(),
    linebase(0),
    prevLinebase(size_t(-1)),
    filename_(options.filename()),
    displayURL_(nullptr),
    sourceMapURL_(nullptr),
    cx(cx),
    mutedErrors(options.mutedErrors()),
    strictModeGetter(smg)
{
    // Nb: the following tables could be static, but initializing them here is
    // much easier.  Don't worry, the time to initialize them for each
    // TokenStream is trivial.  See bug 639420.

    // See Parser::assignExpr() for an explanation of isExprEnding[].
    PodArrayZero(isExprEnding);
    isExprEnding[size_t(TokenKind::Comma)] = 1;
    isExprEnding[size_t(TokenKind::Semi)] = 1;
    isExprEnding[size_t(TokenKind::Colon)] = 1;
    isExprEnding[size_t(TokenKind::Rp)] = 1;
    isExprEnding[size_t(TokenKind::Rb)] = 1;
    isExprEnding[size_t(TokenKind::Rc)] = 1;
}

template<typename CharT>
TokenStreamCharsBase<CharT>::TokenStreamCharsBase(JSContext* cx, const CharT* chars, size_t length,
                                                  size_t startOffset)
  : sourceUnits(chars, length, startOffset),
    tokenbuf(cx)
{}

template<typename CharT, class AnyCharsAccess>
TokenStreamSpecific<CharT, AnyCharsAccess>::TokenStreamSpecific(JSContext* cx,
                                                                const ReadOnlyCompileOptions& options,
                                                                const CharT* base, size_t length)
  : TokenStreamChars<CharT, AnyCharsAccess>(cx, base, length, options.scriptSourceOffset)
{}

bool
TokenStreamAnyChars::checkOptions()
{
    // Constrain starting columns to half of the range of a signed 32-bit value,
    // to avoid overflow.
    if (options().column >= mozilla::MaxValue<int32_t>::value / 2 + 1) {
        reportErrorNoOffset(JSMSG_BAD_COLUMN_NUMBER);
        return false;
    }

    return true;
}

// Use the fastest available getc.
#if defined(HAVE_GETC_UNLOCKED)
# define fast_getc getc_unlocked
#elif defined(HAVE__GETC_NOLOCK)
# define fast_getc _getc_nolock
#else
# define fast_getc getc
#endif

MOZ_MUST_USE MOZ_ALWAYS_INLINE bool
TokenStreamAnyChars::internalUpdateLineInfoForEOL(uint32_t lineStartOffset)
{
    prevLinebase = linebase;
    linebase = lineStartOffset;
    lineno++;
    return srcCoords.add(lineno, linebase);
}

void
TokenStreamAnyChars::undoInternalUpdateLineInfoForEOL()
{
    MOZ_ASSERT(prevLinebase != size_t(-1)); // we should never get more than one EOL
    linebase = prevLinebase;
    prevLinebase = size_t(-1);
    lineno--;
}

template<typename CharT, class AnyCharsAccess>
MOZ_MUST_USE MOZ_ALWAYS_INLINE bool
TokenStreamSpecific<CharT, AnyCharsAccess>::updateLineInfoForEOL()
{
    return anyCharsAccess().internalUpdateLineInfoForEOL(sourceUnits.offset());
}

MOZ_ALWAYS_INLINE void
TokenStreamAnyChars::updateFlagsForEOL()
{
    flags.isDirtyLine = false;
}

// This gets the next char, normalizing all EOL sequences to '\n' as it goes.
template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::getChar(int32_t* cp)
{
    TokenStreamAnyChars& anyChars = anyCharsAccess();

    if (MOZ_UNLIKELY(!sourceUnits.hasRawChars())) {
        anyChars.flags.isEOF = true;
        *cp = EOF;
        return true;
    }

    int32_t c = sourceUnits.getCodeUnit();

    do {
        // Normalize the char16_t if it was a newline.
        if (MOZ_UNLIKELY(c == '\n'))
            break;

        if (MOZ_UNLIKELY(c == '\r')) {
            // If it's a \r\n sequence: treat as a single EOL, skip over the \n.
            if (MOZ_LIKELY(sourceUnits.hasRawChars()))
                sourceUnits.matchCodeUnit('\n');

            break;
        }

        if (MOZ_UNLIKELY(c == unicode::LINE_SEPARATOR || c == unicode::PARA_SEPARATOR))
            break;

        *cp = c;
        return true;
    } while (false);

    if (!updateLineInfoForEOL())
        return false;

    *cp = '\n';
    return true;
}

// This gets the next char. It does nothing special with EOL sequences, not
// even updating the line counters.  It can be used safely if (a) the
// resulting char is guaranteed to be ungotten (by ungetCharIgnoreEOL()) if
// it's an EOL, and (b) the line-related state (lineno, linebase) is not used
// before it's ungotten.
template<typename CharT, class AnyCharsAccess>
int32_t
GeneralTokenStreamChars<CharT, AnyCharsAccess>::getCharIgnoreEOL()
{
    if (MOZ_LIKELY(sourceUnits.hasRawChars()))
        return sourceUnits.getCodeUnit();

    anyCharsAccess().flags.isEOF = true;
    return EOF;
}

template<typename CharT, class AnyCharsAccess>
void
GeneralTokenStreamChars<CharT, AnyCharsAccess>::ungetChar(int32_t c)
{
    if (c == EOF)
        return;

    MOZ_ASSERT(!sourceUnits.atStart());
    sourceUnits.ungetCodeUnit();
    if (c == '\n') {
        int32_t c2 = sourceUnits.peekCodeUnit();
        MOZ_ASSERT(SourceUnits::isRawEOLChar(c2));

        // If it's a \r\n sequence, also unget the \r.
        if (c2 == CharT('\n') && !sourceUnits.atStart())
            sourceUnits.ungetOptionalCRBeforeLF();

        anyCharsAccess().undoInternalUpdateLineInfoForEOL();
    } else {
        MOZ_ASSERT(sourceUnits.peekCodeUnit() == c);
    }
}

template<typename CharT>
void
TokenStreamCharsBase<CharT>::ungetCharIgnoreEOL(int32_t c)
{
    if (c == EOF)
        return;

    MOZ_ASSERT(!sourceUnits.atStart());
    sourceUnits.ungetCodeUnit();
}

template<class AnyCharsAccess>
void
TokenStreamChars<char16_t, AnyCharsAccess>::ungetCodePointIgnoreEOL(uint32_t codePoint)
{
    MOZ_ASSERT(!sourceUnits.atStart());

    unsigned numUnits = 0;
    char16_t units[2];
    unicode::UTF16Encode(codePoint, units, &numUnits);

    MOZ_ASSERT(numUnits == 1 || numUnits == 2);

    while (numUnits-- > 0)
        ungetCharIgnoreEOL(units[numUnits]);
}

// Return true iff |n| raw characters can be read from this without reading past
// EOF, and copy those characters into |cp| if so.  The characters are not
// consumed: use skipChars(n) to do so after checking that the consumed
// characters had appropriate values.
template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::peekChars(int n, CharT* cp)
{
    int i;
    for (i = 0; i < n; i++) {
        int32_t c = getCharIgnoreEOL();
        if (c == EOF)
            break;

        cp[i] = char16_t(c);
    }

    for (int j = i - 1; j >= 0; j--)
        ungetCharIgnoreEOL(cp[j]);

    return i == n;
}

template<typename CharT>
size_t
SourceUnits<CharT>::findEOLMax(size_t start, size_t max)
{
    const CharT* p = codeUnitPtrAt(start);

    size_t n = 0;
    while (true) {
        if (p >= limit_)
            break;
        if (n >= max)
            break;
        n++;
        if (isRawEOLChar(*p++))
            break;
    }
    return start + n;
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::advance(size_t position)
{
    const CharT* end = sourceUnits.codeUnitPtrAt(position);
    while (sourceUnits.addressOfNextCodeUnit() < end) {
        int32_t c;
        if (!getChar(&c))
            return false;
    }

    TokenStreamAnyChars& anyChars = anyCharsAccess();
    Token* cur = const_cast<Token*>(&anyChars.currentToken());
    cur->pos.begin = sourceUnits.offset();
    MOZ_MAKE_MEM_UNDEFINED(&cur->type, sizeof(cur->type));
    anyChars.lookahead = 0;
    return true;
}

template<typename CharT, class AnyCharsAccess>
void
TokenStreamSpecific<CharT, AnyCharsAccess>::seek(const Position& pos)
{
    TokenStreamAnyChars& anyChars = anyCharsAccess();

    sourceUnits.setAddressOfNextCodeUnit(pos.buf, /* allowPoisoned = */ true);
    anyChars.flags = pos.flags;
    anyChars.lineno = pos.lineno;
    anyChars.linebase = pos.linebase;
    anyChars.prevLinebase = pos.prevLinebase;
    anyChars.lookahead = pos.lookahead;

    anyChars.tokens[anyChars.cursor()] = pos.currentToken;
    for (unsigned i = 0; i < anyChars.lookahead; i++)
        anyChars.tokens[anyChars.aheadCursor(1 + i)] = pos.lookaheadTokens[i];
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::seek(const Position& pos,
                                                 const TokenStreamAnyChars& other)
{
    if (!anyCharsAccess().srcCoords.fill(other.srcCoords))
        return false;

    seek(pos);
    return true;
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::reportStrictModeErrorNumberVA(UniquePtr<JSErrorNotes> notes,
                                                                          uint32_t offset,
                                                                          bool strictMode,
                                                                          unsigned errorNumber,
                                                                          va_list* args)
{
    TokenStreamAnyChars& anyChars = anyCharsAccess();
    if (!strictMode && !anyChars.options().extraWarningsOption)
        return true;

    ErrorMetadata metadata;
    if (!computeErrorMetadata(&metadata, offset))
        return false;

    if (strictMode) {
        ReportCompileError(anyChars.cx, Move(metadata), Move(notes), JSREPORT_ERROR, errorNumber,
                           *args);
        return false;
    }

    return anyChars.compileWarning(Move(metadata), Move(notes), JSREPORT_WARNING | JSREPORT_STRICT,
                                   errorNumber, *args);
}

bool
TokenStreamAnyChars::compileWarning(ErrorMetadata&& metadata, UniquePtr<JSErrorNotes> notes,
                                    unsigned flags, unsigned errorNumber, va_list args)
{
    if (options().werrorOption) {
        flags &= ~JSREPORT_WARNING;
        ReportCompileError(cx, Move(metadata), Move(notes), flags, errorNumber, args);
        return false;
    }

    return ReportCompileWarning(cx, Move(metadata), Move(notes), flags, errorNumber, args);
}

void
TokenStreamAnyChars::computeErrorMetadataNoOffset(ErrorMetadata* err)
{
    err->isMuted = mutedErrors;
    err->filename = filename_;
    err->lineNumber = 0;
    err->columnNumber = 0;

    MOZ_ASSERT(err->lineOfContext == nullptr);
}

bool
TokenStreamAnyChars::fillExcludingContext(ErrorMetadata* err, uint32_t offset)
{
    err->isMuted = mutedErrors;

    // If this TokenStreamAnyChars doesn't have location information, try to
    // get it from the caller.
    if (!filename_ && !cx->helperThread()) {
        NonBuiltinFrameIter iter(cx,
                                 FrameIter::FOLLOW_DEBUGGER_EVAL_PREV_LINK,
                                 cx->compartment()->principals());
        if (!iter.done() && iter.filename()) {
            err->filename = iter.filename();
            err->lineNumber = iter.computeLine(&err->columnNumber);
            return false;
        }
    }

    // Otherwise use this TokenStreamAnyChars's location information.
    err->filename = filename_;
    srcCoords.lineNumAndColumnIndex(offset, &err->lineNumber, &err->columnNumber);
    return true;
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::hasTokenizationStarted() const
{
    const TokenStreamAnyChars& anyChars = anyCharsAccess();
    return anyChars.isCurrentTokenType(TokenKind::Eof) && !anyChars.isEOF();
}

void
TokenStreamAnyChars::lineAndColumnAt(size_t offset, uint32_t* line, uint32_t* column) const
{
    srcCoords.lineNumAndColumnIndex(offset, line, column);
}

template<typename CharT, class AnyCharsAccess>
void
TokenStreamSpecific<CharT, AnyCharsAccess>::currentLineAndColumn(uint32_t* line, uint32_t* column) const
{
    const TokenStreamAnyChars& anyChars = anyCharsAccess();
    uint32_t offset = anyChars.currentToken().pos.begin;
    anyChars.srcCoords.lineNumAndColumnIndex(offset, line, column);
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::computeErrorMetadata(ErrorMetadata* err,
                                                                 uint32_t offset)
{
    if (offset == NoOffset) {
        anyCharsAccess().computeErrorMetadataNoOffset(err);
        return true;
    }

    // This function's return value isn't a success/failure indication: it
    // returns true if this TokenStream's location information could be used,
    // and it returns false when that information can't be used (and so we
    // can't provide a line of context).
    if (!anyCharsAccess().fillExcludingContext(err, offset))
        return true;

    // Add a line of context from this TokenStream to help with debugging.
    return computeLineOfContext(err, offset);
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::computeLineOfContext(ErrorMetadata* err,
                                                                 uint32_t offset)
{
    // This function presumes |err| is filled in *except* for line-of-context
    // fields.  It exists to make |TokenStreamSpecific::computeErrorMetadata|,
    // above, more readable.
    TokenStreamAnyChars& anyChars = anyCharsAccess();

    // We only have line-start information for the current line.  If the error
    // is on a different line, we can't easily provide context.  (This means
    // any error in a multi-line token, e.g. an unterminated multiline string
    // literal, won't have context.)
    if (err->lineNumber != anyChars.lineno)
        return true;

    constexpr size_t windowRadius = ErrorMetadata::lineOfContextRadius;

    // The window must start within the current line, no earlier than
    // |windowRadius| characters before |offset|.
    MOZ_ASSERT(offset >= anyChars.linebase);
    size_t windowStart = (offset - anyChars.linebase > windowRadius) ?
                         offset - windowRadius :
                         anyChars.linebase;

    // The window must start within the portion of the current line that we
    // actually have in our buffer.
    if (windowStart < sourceUnits.startOffset())
        windowStart = sourceUnits.startOffset();

    // The window must end within the current line, no later than
    // windowRadius after offset.
    size_t windowEnd = sourceUnits.findEOLMax(offset, windowRadius);
    size_t windowLength = windowEnd - windowStart;
    MOZ_ASSERT(windowLength <= windowRadius * 2);

    // Create the windowed string, not including the potential line
    // terminator.
    StringBuffer windowBuf(anyChars.cx);
    if (!windowBuf.append(codeUnitPtrAt(windowStart), windowLength) ||
        !windowBuf.append('\0'))
    {
        return false;
    }

    err->lineOfContext.reset(windowBuf.stealChars());
    if (!err->lineOfContext)
        return false;

    err->lineLength = windowLength;
    err->tokenOffset = offset - windowStart;
    return true;
}


template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::reportStrictModeError(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);

    TokenStreamAnyChars& anyChars = anyCharsAccess();
    bool result = reportStrictModeErrorNumberVA(nullptr, anyChars.currentToken().pos.begin,
                                                anyChars.strictMode(), errorNumber, &args);

    va_end(args);
    return result;
}

template<typename CharT, class AnyCharsAccess>
void
TokenStreamSpecific<CharT, AnyCharsAccess>::reportError(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);

    TokenStreamAnyChars& anyChars = anyCharsAccess();
    ErrorMetadata metadata;
    if (computeErrorMetadata(&metadata, anyChars.currentToken().pos.begin)) {
        ReportCompileError(anyChars.cx, Move(metadata), nullptr, JSREPORT_ERROR, errorNumber,
                           args);
    }

    va_end(args);
}

void
TokenStreamAnyChars::reportErrorNoOffset(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);

    reportErrorNoOffsetVA(errorNumber, args);

    va_end(args);
}

void
TokenStreamAnyChars::reportErrorNoOffsetVA(unsigned errorNumber, va_list args)
{
    ErrorMetadata metadata;
    computeErrorMetadataNoOffset(&metadata);

    ReportCompileError(cx, Move(metadata), nullptr, JSREPORT_ERROR, errorNumber, args);
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::warning(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);

    ErrorMetadata metadata;
    bool result =
        computeErrorMetadata(&metadata, anyCharsAccess().currentToken().pos.begin) &&
        anyCharsAccess().compileWarning(Move(metadata), nullptr, JSREPORT_WARNING, errorNumber,
                                        args);

    va_end(args);
    return result;
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::reportExtraWarningErrorNumberVA(UniquePtr<JSErrorNotes> notes,
                                                                            uint32_t offset,
                                                                            unsigned errorNumber,
                                                                            va_list* args)
{
    TokenStreamAnyChars& anyChars = anyCharsAccess();
    if (!anyChars.options().extraWarningsOption)
        return true;

    ErrorMetadata metadata;
    if (!computeErrorMetadata(&metadata, offset))
        return false;

    return anyChars.compileWarning(Move(metadata), Move(notes), JSREPORT_STRICT | JSREPORT_WARNING,
                                   errorNumber, *args);
}

template<typename CharT, class AnyCharsAccess>
void
TokenStreamSpecific<CharT, AnyCharsAccess>::error(unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);

    ErrorMetadata metadata;
    if (computeErrorMetadata(&metadata, sourceUnits.offset())) {
        TokenStreamAnyChars& anyChars = anyCharsAccess();
        ReportCompileError(anyChars.cx, Move(metadata), nullptr, JSREPORT_ERROR, errorNumber,
                           args);
    }

    va_end(args);
}

template<typename CharT, class AnyCharsAccess>
void
TokenStreamSpecific<CharT, AnyCharsAccess>::errorAtVA(uint32_t offset, unsigned errorNumber, va_list *args)
{
    ErrorMetadata metadata;
    if (computeErrorMetadata(&metadata, offset)) {
        TokenStreamAnyChars& anyChars = anyCharsAccess();
        ReportCompileError(anyChars.cx, Move(metadata), nullptr, JSREPORT_ERROR, errorNumber,
                           *args);
    }
}


template<typename CharT, class AnyCharsAccess>
void
TokenStreamSpecific<CharT, AnyCharsAccess>::errorAt(uint32_t offset, unsigned errorNumber, ...)
{
    va_list args;
    va_start(args, errorNumber);

    errorAtVA(offset, errorNumber, &args);

    va_end(args);
}

// We have encountered a '\': check for a Unicode escape sequence after it.
// Return the length of the escape sequence and the character code point (by
// value) if we found a Unicode escape sequence.  Otherwise, return 0.  In both
// cases, do not advance along the buffer.
template<typename CharT, class AnyCharsAccess>
uint32_t
TokenStreamSpecific<CharT, AnyCharsAccess>::peekUnicodeEscape(uint32_t* codePoint)
{
    int32_t c = getCharIgnoreEOL();
    if (c != 'u') {
        ungetCharIgnoreEOL(c);
        return 0;
    }

    CharT cp[3];
    uint32_t length;
    c = getCharIgnoreEOL();
    if (JS7_ISHEX(c) && peekChars(3, cp) &&
        JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1]) && JS7_ISHEX(cp[2]))
    {
        *codePoint = (JS7_UNHEX(c) << 12) |
                     (JS7_UNHEX(cp[0]) << 8) |
                     (JS7_UNHEX(cp[1]) << 4) |
                     JS7_UNHEX(cp[2]);
        length = 5;
    } else if (c == '{') {
        length = peekExtendedUnicodeEscape(codePoint);
    } else {
        length = 0;
    }

    ungetCharIgnoreEOL(c);
    ungetCharIgnoreEOL('u');
    return length;
}

template<typename CharT, class AnyCharsAccess>
uint32_t
TokenStreamSpecific<CharT, AnyCharsAccess>::peekExtendedUnicodeEscape(uint32_t* codePoint)
{
    // The opening brace character was already read.
    int32_t c = getCharIgnoreEOL();

    // Skip leading zeros.
    uint32_t leadingZeros = 0;
    while (c == '0') {
        leadingZeros++;
        c = getCharIgnoreEOL();
    }

    CharT cp[6];
    size_t i = 0;
    uint32_t code = 0;
    while (JS7_ISHEX(c) && i < 6) {
        cp[i++] = c;
        code = code << 4 | JS7_UNHEX(c);
        c = getCharIgnoreEOL();
    }

    uint32_t length;
    if (c == '}' && (leadingZeros > 0 || i > 0) && code <= unicode::NonBMPMax) {
        *codePoint = code;
        length = leadingZeros + i + 3;
    } else {
        length = 0;
    }

    ungetCharIgnoreEOL(c);
    while (i--)
        ungetCharIgnoreEOL(cp[i]);
    while (leadingZeros--)
        ungetCharIgnoreEOL('0');

    return length;
}

template<typename CharT, class AnyCharsAccess>
uint32_t
TokenStreamSpecific<CharT, AnyCharsAccess>::matchUnicodeEscapeIdStart(uint32_t* codePoint)
{
    uint32_t length = peekUnicodeEscape(codePoint);
    if (length > 0 && unicode::IsIdentifierStart(*codePoint)) {
        skipChars(length);
        return length;
    }
    return 0;
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::matchUnicodeEscapeIdent(uint32_t* codePoint)
{
    uint32_t length = peekUnicodeEscape(codePoint);
    if (length > 0 && unicode::IsIdentifierPart(*codePoint)) {
        skipChars(length);
        return true;
    }
    return false;
}

// Helper function which returns true if the first length(q) characters in p are
// the same as the characters in q.
template<typename CharT>
static bool
CharsMatch(const CharT* p, const char* q)
{
    while (*q) {
        if (*p++ != *q++)
            return false;
    }

    return true;
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::getDirectives(bool isMultiline,
                                                          bool shouldWarnDeprecated)
{
    // Match directive comments used in debugging, such as "//# sourceURL" and
    // "//# sourceMappingURL". Use of "//@" instead of "//#" is deprecated.
    //
    // To avoid a crashing bug in IE, several JavaScript transpilers wrap single
    // line comments containing a source mapping URL inside a multiline
    // comment. To avoid potentially expensive lookahead and backtracking, we
    // only check for this case if we encounter a '#' character.

    bool res = getDisplayURL(isMultiline, shouldWarnDeprecated) &&
               getSourceMappingURL(isMultiline, shouldWarnDeprecated);
    if (!res)
        badToken();

    return res;
}

template<>
MOZ_MUST_USE bool
TokenStreamCharsBase<char16_t>::copyTokenbufTo(JSContext* cx,
                                               UniquePtr<char16_t[], JS::FreePolicy>* destination)
{
    size_t length = tokenbuf.length();

    *destination = cx->make_pod_array<char16_t>(length + 1);
    if (!*destination)
        return false;

    PodCopy(destination->get(), tokenbuf.begin(), length);
    (*destination)[length] = '\0';
    return true;
}

template<typename CharT, class AnyCharsAccess>
MOZ_MUST_USE bool
TokenStreamSpecific<CharT, AnyCharsAccess>::getDirective(bool isMultiline,
                                                         bool shouldWarnDeprecated,
                                                         const char* directive,
                                                         uint8_t directiveLength,
                                                         const char* errorMsgPragma,
                                                         UniquePtr<char16_t[], JS::FreePolicy>* destination)
{
    MOZ_ASSERT(directiveLength <= 18);
    char16_t peeked[18];

    // If there aren't enough characters left, it can't be the desired
    // directive.
    if (!peekChars(directiveLength, peeked))
        return true;

    // It's also not the desired directive if the characters don't match.
    if (!CharsMatch(peeked, directive))
        return true;

    if (shouldWarnDeprecated) {
        if (!warning(JSMSG_DEPRECATED_PRAGMA, errorMsgPragma))
            return false;
    }

    skipChars(directiveLength);
    tokenbuf.clear();

    do {
        int32_t c;
        if (!peekChar(&c))
            return false;

        if (c == EOF || unicode::IsSpaceOrBOM2(c))
            break;

        consumeKnownChar(c);

        // Debugging directives can occur in both single- and multi-line
        // comments. If we're currently inside a multi-line comment, we also
        // need to recognize multi-line comment terminators.
        if (isMultiline && c == '*' && matchChar('/')) {
            ungetCharIgnoreEOL('/');
            ungetCharIgnoreEOL('*');
            break;
        }

        if (!tokenbuf.append(c))
            return false;
    } while (true);

    if (tokenbuf.empty()) {
        // The directive's URL was missing, but this is not quite an
        // exception that we should stop and drop everything for.
        return true;
    }

    return copyTokenbufTo(anyCharsAccess().cx, destination);
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::getDisplayURL(bool isMultiline,
                                                          bool shouldWarnDeprecated)
{
    // Match comments of the form "//# sourceURL=<url>" or
    // "/\* //# sourceURL=<url> *\/"
    //
    // Note that while these are labeled "sourceURL" in the source text,
    // internally we refer to it as a "displayURL" to distinguish what the
    // developer would like to refer to the source as from the source's actual
    // URL.

    static const char sourceURLDirective[] = " sourceURL=";
    constexpr uint8_t sourceURLDirectiveLength = ArrayLength(sourceURLDirective) - 1;
    return getDirective(isMultiline, shouldWarnDeprecated,
                        sourceURLDirective, sourceURLDirectiveLength,
                        "sourceURL", &anyCharsAccess().displayURL_);
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::getSourceMappingURL(bool isMultiline,
                                                                bool shouldWarnDeprecated)
{
    // Match comments of the form "//# sourceMappingURL=<url>" or
    // "/\* //# sourceMappingURL=<url> *\/"

    static const char sourceMappingURLDirective[] = " sourceMappingURL=";
    constexpr uint8_t sourceMappingURLDirectiveLength = ArrayLength(sourceMappingURLDirective) - 1;
    return getDirective(isMultiline, shouldWarnDeprecated,
                        sourceMappingURLDirective, sourceMappingURLDirectiveLength,
                        "sourceMappingURL", &anyCharsAccess().sourceMapURL_);
}

template<typename CharT, class AnyCharsAccess>
MOZ_ALWAYS_INLINE Token*
GeneralTokenStreamChars<CharT, AnyCharsAccess>::newTokenInternal(TokenKind kind, TokenStart start,
                                                                 TokenKind* out)
{
    MOZ_ASSERT(kind < TokenKind::Limit);
    MOZ_ASSERT(kind != TokenKind::Eol,
               "TokenKind::Eol should never be used in an actual Token, only "
               "returned by peekTokenSameLine()");

    TokenStreamAnyChars& anyChars = anyCharsAccess();
    anyChars.flags.isDirtyLine = true;

    Token* token = anyChars.allocateToken();

    *out = token->type = kind;
    token->pos = TokenPos(start.offset(), this->sourceUnits.offset());
    MOZ_ASSERT(token->pos.begin <= token->pos.end);

    // NOTE: |token->modifier| and |token->modifierException| are set in
    //       |newToken()| so that optimized, non-debug code won't do any work
    //       to pass a modifier-argument that will never be used.

    return token;
}

template<typename CharT, class AnyCharsAccess>
MOZ_COLD bool
GeneralTokenStreamChars<CharT, AnyCharsAccess>::badToken()
{
    // We didn't get a token, so don't set |flags.isDirtyLine|.
    anyCharsAccess().flags.hadError = true;

    // Poisoning sourceUnits on error establishes an invariant: once an
    // erroneous token has been seen, sourceUnits will not be consulted again.
    // This is true because the parser will deal with the illegal token by
    // aborting parsing immediately.
    sourceUnits.poisonInDebug();

    return false;
};

template<>
MOZ_MUST_USE bool
TokenStreamCharsBase<char16_t>::appendCodePointToTokenbuf(uint32_t codePoint)
{
    char16_t units[2];
    unsigned numUnits = 0;
    unicode::UTF16Encode(codePoint, units, &numUnits);

    MOZ_ASSERT(numUnits == 1 || numUnits == 2,
               "UTF-16 code points are only encoded in one or two units");

    if (!tokenbuf.append(units[0]))
        return false;

    if (numUnits == 1)
        return true;

    return tokenbuf.append(units[1]);
}

template<class AnyCharsAccess>
void
TokenStreamChars<char16_t, AnyCharsAccess>::matchMultiUnitCodePointSlow(char16_t lead,
                                                                        uint32_t* codePoint)
{
    MOZ_ASSERT(unicode::IsLeadSurrogate(lead),
               "matchMultiUnitCodepoint should have ensured |lead| is a lead "
               "surrogate");

    int32_t maybeTrail = getCharIgnoreEOL();
    if (MOZ_LIKELY(unicode::IsTrailSurrogate(maybeTrail))) {
        *codePoint = unicode::UTF16Decode(lead, maybeTrail);
    } else {
        ungetCharIgnoreEOL(maybeTrail);
        *codePoint = 0;
    }
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::putIdentInTokenbuf(const CharT* identStart)
{
    const CharT* const originalAddress = sourceUnits.addressOfNextCodeUnit();
    sourceUnits.setAddressOfNextCodeUnit(identStart);

    auto restoreNextRawCharAddress =
        MakeScopeExit([this, originalAddress]() {
            this->sourceUnits.setAddressOfNextCodeUnit(originalAddress);
        });

    tokenbuf.clear();
    for (;;) {
        int32_t c = getCharIgnoreEOL();

        uint32_t codePoint;
        if (!matchMultiUnitCodePoint(c, &codePoint))
            return false;
        if (codePoint) {
            if (!unicode::IsIdentifierPart(codePoint))
                break;
        } else {
            if (unicode::IsIdentifierPart(char16_t(c))) {
                codePoint = c;
            } else {
                if (c != '\\' || !matchUnicodeEscapeIdent(&codePoint))
                    break;
            }
        }

        if (!appendCodePointToTokenbuf(codePoint))
            return false;
    }

    return true;
}

template<typename CharT, class AnyCharsAccess>
MOZ_MUST_USE bool
TokenStreamSpecific<CharT, AnyCharsAccess>::identifierName(TokenStart start,
                                                           const CharT* identStart,
                                                           IdentifierEscapes escaping,
                                                           Modifier modifier, TokenKind* out)
{
    // Run the bad-token code for every path out of this function except the
    // two success-cases.
    auto noteBadToken = MakeScopeExit([this]() {
        this->badToken();
    });

    int c;
    while (true) {
        c = getCharIgnoreEOL();
        if (c == EOF)
            break;

        uint32_t codePoint;
        if (!matchMultiUnitCodePoint(c, &codePoint))
            return false;
        if (codePoint) {
            if (!unicode::IsIdentifierPart(codePoint))
                break;

            continue;
        }

        if (!unicode::IsIdentifierPart(char16_t(c))) {
            uint32_t qc;
            if (c != '\\' || !matchUnicodeEscapeIdent(&qc))
                break;
            escaping = IdentifierEscapes::SawUnicodeEscape;
        }
    }
    ungetCharIgnoreEOL(c);

    const CharT* chars;
    size_t length;
    if (escaping == IdentifierEscapes::SawUnicodeEscape) {
        // Identifiers containing Unicode escapes have to be converted into
        // tokenbuf before atomizing.
        if (!putIdentInTokenbuf(identStart))
            return false;

        chars = tokenbuf.begin();
        length = tokenbuf.length();
    } else {
        // Escape-free identifiers can be created directly from sourceUnits.
        chars = identStart;
        length = sourceUnits.addressOfNextCodeUnit() - identStart;

        // Represent reserved words lacking escapes as reserved word tokens.
        if (const ReservedWordInfo* rw = FindReservedWord(chars, length)) {
            noteBadToken.release();
            newSimpleToken(rw->tokentype, start, modifier, out);
            return true;
        }
    }

    JSAtom* atom = atomizeChars(anyCharsAccess().cx, chars, length);
    if (!atom)
        return false;

    noteBadToken.release();
    newNameToken(atom->asPropertyName(), start, modifier, out);
    return true;
}

enum FirstCharKind {
    // A char16_t has the 'OneChar' kind if it, by itself, constitutes a valid
    // token that cannot also be a prefix of a longer token.  E.g. ';' has the
    // OneChar kind, but '+' does not, because '++' and '+=' are valid longer tokens
    // that begin with '+'.
    //
    // The few token kinds satisfying these properties cover roughly 35--45%
    // of the tokens seen in practice.
    //
    // We represent the 'OneChar' kind with any positive value less than
    // TokenKind::Limit.  This representation lets us associate
    // each one-char token char16_t with a TokenKind and thus avoid
    // a subsequent char16_t-to-TokenKind conversion.
    OneChar_Min = 0,
    OneChar_Max = size_t(TokenKind::Limit) - 1,

    Space = size_t(TokenKind::Limit),
    Ident,
    Dec,
    String,
    EOL,
    ZeroDigit,
    Other,

    LastCharKind = Other
};

// OneChar: 40,  41,  44,  58,  59,  63,  91,  93,  123, 125, 126:
//          '(', ')', ',', ':', ';', '?', '[', ']', '{', '}', '~'
// Ident:   36, 65..90, 95, 97..122: '$', 'A'..'Z', '_', 'a'..'z'
// Dot:     46: '.'
// Equals:  61: '='
// String:  34, 39, 96: '"', '\'', '`'
// Dec:     49..57: '1'..'9'
// Plus:    43: '+'
// ZeroDigit:  48: '0'
// Space:   9, 11, 12, 32: '\t', '\v', '\f', ' '
// EOL:     10, 13: '\n', '\r'
//
#define T_COMMA     size_t(TokenKind::Comma)
#define T_COLON     size_t(TokenKind::Colon)
#define T_BITNOT    size_t(TokenKind::BitNot)
#define T_LP        size_t(TokenKind::Lp)
#define T_RP        size_t(TokenKind::Rp)
#define T_SEMI      size_t(TokenKind::Semi)
#define T_HOOK      size_t(TokenKind::Hook)
#define T_LB        size_t(TokenKind::Lb)
#define T_RB        size_t(TokenKind::Rb)
#define T_LC        size_t(TokenKind::Lc)
#define T_RC        size_t(TokenKind::Rc)
#define _______     Other
static const uint8_t firstCharKinds[] = {
/*         0        1        2        3        4        5        6        7        8        9    */
/*   0+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______,   Space,
/*  10+ */     EOL,   Space,   Space,     EOL, _______, _______, _______, _______, _______, _______,
/*  20+ */ _______, _______, _______, _______, _______, _______, _______, _______, _______, _______,
/*  30+ */ _______, _______,   Space, _______,  String, _______,   Ident, _______, _______,  String,
/*  40+ */    T_LP,    T_RP, _______, _______, T_COMMA, _______, _______, _______,ZeroDigit,    Dec,
/*  50+ */     Dec,     Dec,     Dec,     Dec,     Dec,     Dec,     Dec,     Dec, T_COLON,  T_SEMI,
/*  60+ */ _______, _______, _______,  T_HOOK, _______,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  70+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  80+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/*  90+ */   Ident,    T_LB, _______,    T_RB, _______,   Ident,  String,   Ident,   Ident,   Ident,
/* 100+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/* 110+ */   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,   Ident,
/* 120+ */   Ident,   Ident,   Ident,    T_LC, _______,    T_RC,T_BITNOT, _______
};
#undef T_COMMA
#undef T_COLON
#undef T_BITNOT
#undef T_LP
#undef T_RP
#undef T_SEMI
#undef T_HOOK
#undef T_LB
#undef T_RB
#undef T_LC
#undef T_RC
#undef _______

static_assert(LastCharKind < (1 << (sizeof(firstCharKinds[0]) * 8)),
              "Elements of firstCharKinds[] are too small");

template<typename CharT, class AnyCharsAccess>
void
GeneralTokenStreamChars<CharT, AnyCharsAccess>::consumeRestOfSingleLineComment()
{
    int32_t c;
    do {
        c = getCharIgnoreEOL();
    } while (c != EOF && !SourceUnits::isRawEOLChar(c));

    ungetCharIgnoreEOL(c);
}

template<typename CharT, class AnyCharsAccess>
MOZ_MUST_USE bool
TokenStreamSpecific<CharT, AnyCharsAccess>::decimalNumber(int c, TokenStart start,
                                                          const CharT* numStart,
                                                          Modifier modifier, TokenKind* out)
{
    // Run the bad-token code for every path out of this function except the
    // one success-case.
    auto noteBadToken = MakeScopeExit([this]() {
        this->badToken();
    });

    // Consume integral component digits.
    while (IsAsciiDigit(c))
        c = getCharIgnoreEOL();

    // Numbers contain no escapes, so we can read directly from |sourceUnits|.
    double dval;
    DecimalPoint decimalPoint = NoDecimal;
    if (c != '.' && c != 'e' && c != 'E') {
        ungetCharIgnoreEOL(c);

        // Most numbers are pure decimal integers without fractional component
        // or exponential notation.  Handle that with optimized code.
        if (!GetDecimalInteger(anyCharsAccess().cx, numStart, sourceUnits.addressOfNextCodeUnit(),
                               &dval))
        {
            return false;
        }
    } else {
        // Consume any decimal dot and fractional component.
        if (c == '.') {
            decimalPoint = HasDecimal;
            do {
                c = getCharIgnoreEOL();
            } while (IsAsciiDigit(c));
        }

        // Consume any exponential notation.
        if (c == 'e' || c == 'E') {
            c = getCharIgnoreEOL();
            if (c == '+' || c == '-')
                c = getCharIgnoreEOL();

            // Exponential notation must contain at least one digit.
            if (!IsAsciiDigit(c)) {
                ungetCharIgnoreEOL(c);
                error(JSMSG_MISSING_EXPONENT);
                return false;
            }

            // Consume exponential digits.
            do {
                c = getCharIgnoreEOL();
            } while (IsAsciiDigit(c));
        }

        ungetCharIgnoreEOL(c);

        const CharT* dummy;
        if (!js_strtod(anyCharsAccess().cx, numStart, sourceUnits.addressOfNextCodeUnit(), &dummy,
                       &dval))
        {
           return false;
        }
    }

    // Number followed by IdentifierStart is an error.  (This is the only place
    // in ECMAScript where token boundary is inadequate to properly separate
    // two tokens, necessitating this unaesthetic lookahead.)
    if (c != EOF) {
        if (unicode::IsIdentifierStart(char16_t(c))) {
            error(JSMSG_IDSTART_AFTER_NUMBER);
            return false;
        }

        consumeKnownCharIgnoreEOL(c);

        uint32_t codePoint;
        if (!matchMultiUnitCodePoint(c, &codePoint))
            return false;

        if (codePoint) {
            // In all cases revert the get of the overall code point.
            ungetCodePointIgnoreEOL(codePoint);

            if (unicode::IsIdentifierStart(codePoint)) {
                // This will properly point at the start of the code point.
                error(JSMSG_IDSTART_AFTER_NUMBER);
                return false;
            }
        } else {
            // If not a multi-unit code point, we only need to unget the single
            // code unit consumed.
            ungetCharIgnoreEOL(c);
        }
    }

    noteBadToken.release();
    newNumberToken(dval, decimalPoint, start, modifier, out);
    return true;
}

template<typename CharT, class AnyCharsAccess>
MOZ_MUST_USE bool
TokenStreamSpecific<CharT, AnyCharsAccess>::getTokenInternal(TokenKind* const ttp,
                                                             const Modifier modifier)
{
    // Assume we'll fail: success cases will overwrite this.
#ifdef DEBUG
    *ttp = TokenKind::Limit;
#endif
    MOZ_MAKE_MEM_UNDEFINED(ttp, sizeof(*ttp));

    // Check if in the middle of a template string. Have to get this out of
    // the way first.
    if (MOZ_UNLIKELY(modifier == TemplateTail))
        return getStringOrTemplateToken('`', modifier, ttp);

    // This loop runs more than once only when whitespace or comments are
    // encountered.
    do {
        if (MOZ_UNLIKELY(!sourceUnits.hasRawChars())) {
            anyCharsAccess().flags.isEOF = true;
            TokenStart start(sourceUnits, 0);
            newSimpleToken(TokenKind::Eof, start, modifier, ttp);
            return true;
        }

        int c = sourceUnits.getCodeUnit();
        MOZ_ASSERT(c != EOF);

        // Chars not in the range 0..127 are rare.  Getting them out of the way
        // early allows subsequent checking to be faster.
        if (MOZ_UNLIKELY(c >= 128)) {
            if (unicode::IsSpaceOrBOM2(c)) {
                if (c == unicode::LINE_SEPARATOR ||
                    c == unicode::PARA_SEPARATOR)
                {
                    if (!updateLineInfoForEOL())
                        return badToken();

                    anyCharsAccess().updateFlagsForEOL();
                }

                continue;
            }

            // If there's an identifier here (and no error occurs), it starts
            // at the previous code unit.
            TokenStart start(sourceUnits, -1);
            const CharT* identStart = sourceUnits.addressOfNextCodeUnit() - 1;

            static_assert('$' < 128,
                          "IdentifierStart contains '$', but as "
                          "!IsUnicodeIDStart('$'), ensure that '$' is never "
                          "handled here");
            static_assert('_' < 128,
                          "IdentifierStart contains '_', but as "
                          "!IsUnicodeIDStart('_'), ensure that '_' is never "
                          "handled here");
            if (unicode::IsUnicodeIDStart(char16_t(c)))
                return identifierName(start, identStart, IdentifierEscapes::None, modifier, ttp);

            uint32_t codePoint = c;
            if (!matchMultiUnitCodePoint(c, &codePoint))
                return badToken();

            if (codePoint && unicode::IsUnicodeIDStart(codePoint))
                return identifierName(start, identStart, IdentifierEscapes::None, modifier, ttp);

            ungetCodePointIgnoreEOL(codePoint);
            error(JSMSG_ILLEGAL_CHARACTER);
            return badToken();
        }

        // Get the token kind, based on the first char.  The ordering of c1kind
        // comparison is based on the frequency of tokens in real code:
        // Parsemark (which represents typical JS code on the web) and the
        // Unreal demo (which represents asm.js code).
        //
        //                  Parsemark   Unreal
        //  OneChar         32.9%       39.7%
        //  Space           25.0%        0.6%
        //  Ident           19.2%       36.4%
        //  Dec              7.2%        5.1%
        //  String           7.9%        0.0%
        //  EOL              1.7%        0.0%
        //  ZeroDigit        0.4%        4.9%
        //  Other            5.7%       13.3%
        //
        // The ordering is based mostly only Parsemark frequencies, with Unreal
        // frequencies used to break close categories (e.g. |Dec| and
        // |String|).  |Other| is biggish, but no other token kind is common
        // enough for it to be worth adding extra values to FirstCharKind.
        FirstCharKind c1kind = FirstCharKind(firstCharKinds[c]);

        // Look for an unambiguous single-char token.
        //
        if (c1kind <= OneChar_Max) {
            TokenStart start(sourceUnits, -1);
            newSimpleToken(TokenKind(c1kind), start, modifier, ttp);
            return true;
        }

        // Skip over non-EOL whitespace chars.
        //
        if (c1kind == Space)
            continue;

        // Look for an identifier.
        //
        if (c1kind == Ident) {
            TokenStart start(sourceUnits, -1);
            return identifierName(start, sourceUnits.addressOfNextCodeUnit() - 1,
                                  IdentifierEscapes::None, modifier, ttp);
        }

        // Look for a decimal number.
        //
        if (c1kind == Dec) {
            TokenStart start(sourceUnits, -1);
            const CharT* numStart = sourceUnits.addressOfNextCodeUnit() - 1;
            return decimalNumber(c, start, numStart, modifier, ttp);
        }

        // Look for a string or a template string.
        //
        if (c1kind == String)
            return getStringOrTemplateToken(static_cast<char>(c), modifier, ttp);

        // Skip over EOL chars, updating line state along the way.
        //
        if (c1kind == EOL) {
            // If it's a \r\n sequence, consume it as a single EOL.
            if (c == '\r' && sourceUnits.hasRawChars())
                sourceUnits.matchCodeUnit('\n');

            if (!updateLineInfoForEOL())
                return badToken();

            anyCharsAccess().updateFlagsForEOL();
            continue;
        }

        // From a '0', look for a hexadecimal, binary, octal, or "noctal" (a
        // number starting with '0' that contains '8' or '9' and is treated as
        // decimal) number.
        //
        if (c1kind == ZeroDigit) {
            TokenStart start(sourceUnits, -1);

            int radix;
            const CharT* numStart;
            c = getCharIgnoreEOL();
            if (c == 'x' || c == 'X') {
                radix = 16;
                c = getCharIgnoreEOL();
                if (!JS7_ISHEX(c)) {
                    ungetCharIgnoreEOL(c);
                    reportError(JSMSG_MISSING_HEXDIGITS);
                    return badToken();
                }

                // one past the '0x'
                numStart = sourceUnits.addressOfNextCodeUnit() - 1;

                while (JS7_ISHEX(c))
                    c = getCharIgnoreEOL();
            } else if (c == 'b' || c == 'B') {
                radix = 2;
                c = getCharIgnoreEOL();
                if (c != '0' && c != '1') {
                    ungetCharIgnoreEOL(c);
                    reportError(JSMSG_MISSING_BINARY_DIGITS);
                    return badToken();
                }

                // one past the '0b'
                numStart = sourceUnits.addressOfNextCodeUnit() - 1;

                while (c == '0' || c == '1')
                    c = getCharIgnoreEOL();
            } else if (c == 'o' || c == 'O') {
                radix = 8;
                c = getCharIgnoreEOL();
                if (c < '0' || c > '7') {
                    ungetCharIgnoreEOL(c);
                    reportError(JSMSG_MISSING_OCTAL_DIGITS);
                    return badToken();
                }

                // one past the '0o'
                numStart = sourceUnits.addressOfNextCodeUnit() - 1;

                while ('0' <= c && c <= '7')
                    c = getCharIgnoreEOL();
            } else if (IsAsciiDigit(c)) {
                radix = 8;
                // one past the '0'
                numStart = sourceUnits.addressOfNextCodeUnit() - 1;

                do {
                    // Octal integer literals are not permitted in strict mode
                    // code.
                    if (!reportStrictModeError(JSMSG_DEPRECATED_OCTAL))
                        return badToken();

                    // Outside strict mode, we permit 08 and 09 as decimal
                    // numbers, which makes our behaviour a superset of the
                    // ECMA numeric grammar. We might not always be so
                    // permissive, so we warn about it.
                    if (c >= '8') {
                        if (!warning(JSMSG_BAD_OCTAL, c == '8' ? "08" : "09"))
                            return badToken();

                        // Use the decimal scanner for the rest of the number.
                        return decimalNumber(c, start, numStart, modifier, ttp);
                    }

                    c = getCharIgnoreEOL();
                } while (IsAsciiDigit(c));
            } else {
                // '0' not followed by [XxBbOo0-9];  scan as a decimal number.
                numStart = sourceUnits.addressOfNextCodeUnit() - 1;

                return decimalNumber(c, start, numStart, modifier, ttp);
            }
            ungetCharIgnoreEOL(c);

            if (c != EOF) {
                if (unicode::IsIdentifierStart(char16_t(c))) {
                    error(JSMSG_IDSTART_AFTER_NUMBER);
                    return badToken();
                }

                consumeKnownCharIgnoreEOL(c);

                uint32_t codePoint;
                if (!matchMultiUnitCodePoint(c, &codePoint))
                    return badToken();

                if (codePoint) {
                    // In all cases revert the get of the overall code point.
                    ungetCodePointIgnoreEOL(codePoint);

                    if (unicode::IsIdentifierStart(codePoint)) {
                        // This will properly point at the start of the code
                        // point.
                        error(JSMSG_IDSTART_AFTER_NUMBER);
                        return badToken();
                    }
                } else {
                    // If not a multi-unit code point, we only need to unget
                    // the single code unit consumed.
                    ungetCharIgnoreEOL(c);
                }
            }

            double dval;
            const char16_t* dummy;
            if (!GetPrefixInteger(anyCharsAccess().cx, numStart,
                                  sourceUnits.addressOfNextCodeUnit(), radix, &dummy, &dval))
            {
                return badToken();
            }

            newNumberToken(dval, NoDecimal, start, modifier, ttp);
            return true;
        }

        MOZ_ASSERT(c1kind == Other);

        // This handles everything else.  Simple tokens distinguished solely by
        // TokenKind should set |simpleKind| and break, to share simple-token
        // creation code for all such tokens.  All other tokens must be handled
        // by returning (or by continuing from the loop enclosing this).
        //
        TokenStart start(sourceUnits, -1);
        TokenKind simpleKind;
#ifdef DEBUG
        simpleKind = TokenKind::Limit; // sentinel value for code after switch
#endif
        switch (c) {
          case '.':
            c = getCharIgnoreEOL();
            if (IsAsciiDigit(c)) {
                return decimalNumber('.', start, sourceUnits.addressOfNextCodeUnit() - 2, modifier,
                                     ttp);
            }

            if (c == '.') {
                if (matchChar('.')) {
                    simpleKind = TokenKind::TripleDot;
                    break;
                }
            }
            ungetCharIgnoreEOL(c);

            simpleKind = TokenKind::Dot;
            break;

          case '=':
            if (matchChar('='))
                simpleKind = matchChar('=') ? TokenKind::StrictEq : TokenKind::Eq;
            else if (matchChar('>'))
                simpleKind = TokenKind::Arrow;
            else
                simpleKind = TokenKind::Assign;
            break;

          case '+':
            if (matchChar('+'))
                simpleKind = TokenKind::Inc;
            else
                simpleKind = matchChar('=') ? TokenKind::AddAssign : TokenKind::Add;
            break;

          case '\\': {
            uint32_t qc;
            if (uint32_t escapeLength = matchUnicodeEscapeIdStart(&qc)) {
                return identifierName(start,
                                      sourceUnits.addressOfNextCodeUnit() - escapeLength - 1,
                                      IdentifierEscapes::SawUnicodeEscape, modifier, ttp);
            }

            // We could point "into" a mistyped escape, e.g. for "\u{41H}" we
            // could point at the 'H'.  But we don't do that now, so the
            // character after the '\' isn't necessarily bad, so just point at
            // the start of the actually-invalid escape.
            ungetCharIgnoreEOL('\\');
            error(JSMSG_BAD_ESCAPE);
            return badToken();
          }

          case '|':
            if (matchChar('|'))
                simpleKind = TokenKind::Or;
#ifdef ENABLE_PIPELINE_OPERATOR
            else if (matchChar('>'))
                simpleKind = TokenKind::Pipeline;
#endif
            else
                simpleKind = matchChar('=') ? TokenKind::BitOrAssign : TokenKind::BitOr;
            break;

          case '^':
            simpleKind = matchChar('=') ? TokenKind::BitXorAssign : TokenKind::BitXor;
            break;

          case '&':
            if (matchChar('&'))
                simpleKind = TokenKind::And;
            else
                simpleKind = matchChar('=') ? TokenKind::BitAndAssign : TokenKind::BitAnd;
            break;

          case '!':
            if (matchChar('='))
                simpleKind = matchChar('=') ? TokenKind::StrictNe : TokenKind::Ne;
            else
                simpleKind = TokenKind::Not;
            break;

          case '<':
            if (anyCharsAccess().options().allowHTMLComments) {
                // Treat HTML begin-comment as comment-till-end-of-line.
                if (matchChar('!')) {
                    if (matchChar('-')) {
                        if (matchChar('-')) {
                            consumeRestOfSingleLineComment();
                            continue;
                        }
                        ungetCharIgnoreEOL('-');
                    }
                    ungetCharIgnoreEOL('!');
                }
            }
            if (matchChar('<'))
                simpleKind = matchChar('=') ? TokenKind::LshAssign : TokenKind::Lsh;
            else
                simpleKind = matchChar('=') ? TokenKind::Le : TokenKind::Lt;
            break;

          case '>':
            if (matchChar('>')) {
                if (matchChar('>'))
                    simpleKind = matchChar('=') ? TokenKind::UrshAssign : TokenKind::Ursh;
                else
                    simpleKind = matchChar('=') ? TokenKind::RshAssign : TokenKind::Rsh;
            } else {
                simpleKind = matchChar('=') ? TokenKind::Ge : TokenKind::Gt;
            }
            break;

          case '*':
            if (matchChar('*'))
                simpleKind = matchChar('=') ? TokenKind::PowAssign : TokenKind::Pow;
            else
                simpleKind = matchChar('=') ? TokenKind::MulAssign : TokenKind::Mul;
            break;

          case '/':
            // Look for a single-line comment.
            if (matchChar('/')) {
                c = getCharIgnoreEOL();
                if (c == '@' || c == '#') {
                    bool shouldWarn = c == '@';
                    if (!getDirectives(false, shouldWarn))
                        return false;
                } else {
                    ungetCharIgnoreEOL(c);
                }

                consumeRestOfSingleLineComment();
                continue;
            }

            // Look for a multi-line comment.
            if (matchChar('*')) {
                TokenStreamAnyChars& anyChars = anyCharsAccess();
                unsigned linenoBefore = anyChars.lineno;

                do {
                    if (!getChar(&c))
                        return badToken();

                    if (c == EOF) {
                        reportError(JSMSG_UNTERMINATED_COMMENT);
                        return badToken();
                    }

                    if (c == '*' && matchChar('/'))
                        break;

                    if (c == '@' || c == '#') {
                        bool shouldWarn = c == '@';
                        if (!getDirectives(true, shouldWarn))
                            return false;
                    }
                } while (true);

                if (linenoBefore != anyChars.lineno)
                    anyChars.updateFlagsForEOL();

                continue;
            }

            // Look for a regexp.
            if (modifier == Operand) {
                tokenbuf.clear();

                bool inCharClass = false;
                do {
                    if (!getChar(&c))
                        return badToken();

                    if (c == '\\') {
                        if (!tokenbuf.append(c))
                            return badToken();

                        if (!getChar(&c))
                            return badToken();
                    } else if (c == '[') {
                        inCharClass = true;
                    } else if (c == ']') {
                        inCharClass = false;
                    } else if (c == '/' && !inCharClass) {
                        // For IE compat, allow unescaped / in char classes.
                        break;
                    }

                    if (c == '\n' || c == EOF) {
                        ungetChar(c);
                        reportError(JSMSG_UNTERMINATED_REGEXP);
                        return badToken();
                    }

                    if (!tokenbuf.append(c))
                        return badToken();
                } while (true);

                RegExpFlag reflags = NoFlags;
                while (true) {
                    RegExpFlag flag;
                    c = getCharIgnoreEOL();
                    if (c == 'g')
                        flag = GlobalFlag;
                    else if (c == 'i')
                        flag = IgnoreCaseFlag;
                    else if (c == 'm')
                        flag = MultilineFlag;
                    else if (c == 'y')
                        flag = StickyFlag;
                    else if (c == 'u')
                        flag = UnicodeFlag;
                    else if (IsAsciiAlpha(c))
                        flag = NoFlags;
                    else
                        break;

                    if ((reflags & flag) || flag == NoFlags) {
                        MOZ_ASSERT(sourceUnits.offset() > 0);
                        char buf[2] = { char(c), '\0' };
                        errorAt(sourceUnits.offset() - 1, JSMSG_BAD_REGEXP_FLAG, buf);
                        return badToken();
                    }

                    reflags = RegExpFlag(reflags | flag);
                }
                ungetCharIgnoreEOL(c);

                newRegExpToken(reflags, start, modifier, ttp);
                return true;
            }

            simpleKind = matchChar('=') ? TokenKind::DivAssign : TokenKind::Div;
            break;

          case '%':
            simpleKind = matchChar('=') ? TokenKind::ModAssign : TokenKind::Mod;
            break;

          case '-':
            if (matchChar('-')) {
                if (anyCharsAccess().options().allowHTMLComments &&
                    !anyCharsAccess().flags.isDirtyLine)
                {
                    if (matchChar('>')) {
                        consumeRestOfSingleLineComment();
                        continue;
                    }
                }

                simpleKind = TokenKind::Dec;
            } else {
                simpleKind = matchChar('=') ? TokenKind::SubAssign : TokenKind::Sub;
            }
            break;

          default:
            // We consumed a bad character/code point.  Put it back so the
            // error location is the bad character.
            ungetCodePointIgnoreEOL(c);
            error(JSMSG_ILLEGAL_CHARACTER);
            return badToken();
        }

        MOZ_ASSERT(simpleKind != TokenKind::Limit,
                   "switch-statement should have set |simpleKind| before "
                   "breaking");

        newSimpleToken(simpleKind, start, modifier, ttp);
        return true;
    } while (true);
}

template<typename CharT, class AnyCharsAccess>
bool
TokenStreamSpecific<CharT, AnyCharsAccess>::getStringOrTemplateToken(char untilChar,
                                                                     Modifier modifier,
                                                                     TokenKind* out)
{
    MOZ_ASSERT(untilChar == '\'' || untilChar == '"' || untilChar == '`',
               "unexpected string/template literal delimiter");

    int c;

    bool parsingTemplate = (untilChar == '`');
    bool templateHead = false;

    TokenStart start(sourceUnits, -1);
    tokenbuf.clear();

    // Run the bad-token code for every path out of this function except the
    // one success-case.
    auto noteBadToken = MakeScopeExit([this]() {
        this->badToken();
    });

    // We need to detect any of these chars:  " or ', \n (or its
    // equivalents), \\, EOF.  Because we detect EOL sequences here and
    // put them back immediately, we can use getCharIgnoreEOL().
    while ((c = getCharIgnoreEOL()) != untilChar) {
        if (c == EOF) {
            ungetCharIgnoreEOL(c);
            const char delimiters[] = { untilChar, untilChar, '\0' };
            error(JSMSG_EOF_BEFORE_END_OF_LITERAL, delimiters);
            return false;
        }

        if (c == '\\') {
            // When parsing templates, we don't immediately report errors for
            // invalid escapes; these are handled by the parser.
            // In those cases we don't append to tokenbuf, since it won't be
            // read.
            if (!getChar(&c))
                return false;

            if (c == EOF) {
                const char delimiters[] = { untilChar, untilChar, '\0' };
                error(JSMSG_EOF_IN_ESCAPE_IN_LITERAL, delimiters);
                return false;
            }

            switch (static_cast<CharT>(c)) {
              case 'b': c = '\b'; break;
              case 'f': c = '\f'; break;
              case 'n': c = '\n'; break;
              case 'r': c = '\r'; break;
              case 't': c = '\t'; break;
              case 'v': c = '\v'; break;

              case '\n':
                // ES5 7.8.4: an escaped line terminator represents
                // no character.
                continue;

              // Unicode character specification.
              case 'u': {
                int32_t c2 = getCharIgnoreEOL();
                if (c2 == '{') {
                    uint32_t start = sourceUnits.offset() - 3;
                    uint32_t code = 0;
                    bool first = true;
                    bool valid = true;
                    do {
                        int32_t c = getCharIgnoreEOL();
                        if (c == EOF) {
                            if (parsingTemplate) {
                                TokenStreamAnyChars& anyChars = anyCharsAccess();
                                anyChars.setInvalidTemplateEscape(start,
                                                                  InvalidEscapeType::Unicode);
                                valid = false;
                                break;
                            }
                            reportInvalidEscapeError(start, InvalidEscapeType::Unicode);
                            return false;
                        }
                        if (c == '}') {
                            if (first) {
                                if (parsingTemplate) {
                                    TokenStreamAnyChars& anyChars = anyCharsAccess();
                                    anyChars.setInvalidTemplateEscape(start,
                                                                      InvalidEscapeType::Unicode);
                                    valid = false;
                                    break;
                                }
                                reportInvalidEscapeError(start, InvalidEscapeType::Unicode);
                                return false;
                            }
                            break;
                        }

                        if (!JS7_ISHEX(c)) {
                            if (parsingTemplate) {
                                // We put the character back so that we read
                                // it on the next pass, which matters if it
                                // was '`' or '\'.
                                ungetCharIgnoreEOL(c);

                                TokenStreamAnyChars& anyChars = anyCharsAccess();
                                anyChars.setInvalidTemplateEscape(start,
                                                                  InvalidEscapeType::Unicode);
                                valid = false;
                                break;
                            }
                            reportInvalidEscapeError(start, InvalidEscapeType::Unicode);
                            return false;
                        }

                        code = (code << 4) | JS7_UNHEX(c);
                        if (code > unicode::NonBMPMax) {
                            if (parsingTemplate) {
                                TokenStreamAnyChars& anyChars = anyCharsAccess();
                                anyChars.setInvalidTemplateEscape(start + 3,
                                                                  InvalidEscapeType::UnicodeOverflow);
                                valid = false;
                                break;
                            }
                            reportInvalidEscapeError(start + 3, InvalidEscapeType::UnicodeOverflow);
                            return false;
                        }

                        first = false;
                    } while (true);

                    if (!valid)
                        continue;

                    MOZ_ASSERT(code <= unicode::NonBMPMax);
                    if (code < unicode::NonBMPMin) {
                        c = code;
                    } else {
                        if (!tokenbuf.append(unicode::LeadSurrogate(code)))
                            return false;
                        c = unicode::TrailSurrogate(code);
                    }
                    break;
                }

                CharT cp[3];
                if (JS7_ISHEX(c2) && peekChars(3, cp) &&
                    JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1]) && JS7_ISHEX(cp[2]))
                {
                    c = (JS7_UNHEX(c2) << 12) |
                        (JS7_UNHEX(cp[0]) << 8) |
                        (JS7_UNHEX(cp[1]) << 4) |
                        JS7_UNHEX(cp[2]);
                    skipChars(3);
                } else {
                    ungetCharIgnoreEOL(c2);
                    uint32_t start = sourceUnits.offset() - 2;
                    if (parsingTemplate) {
                        TokenStreamAnyChars& anyChars = anyCharsAccess();
                        anyChars.setInvalidTemplateEscape(start, InvalidEscapeType::Unicode);
                        continue;
                    }
                    reportInvalidEscapeError(start, InvalidEscapeType::Unicode);
                    return false;
                }
                break;
              }

              // Hexadecimal character specification.
              case 'x': {
                CharT cp[2];
                if (peekChars(2, cp) && JS7_ISHEX(cp[0]) && JS7_ISHEX(cp[1])) {
                    c = (JS7_UNHEX(cp[0]) << 4) + JS7_UNHEX(cp[1]);
                    skipChars(2);
                } else {
                    uint32_t start = sourceUnits.offset() - 2;
                    if (parsingTemplate) {
                        TokenStreamAnyChars& anyChars = anyCharsAccess();
                        anyChars.setInvalidTemplateEscape(start, InvalidEscapeType::Hexadecimal);
                        continue;
                    }
                    reportInvalidEscapeError(start, InvalidEscapeType::Hexadecimal);
                    return false;
                }
                break;
              }

              default:
                // Octal character specification.
                if (JS7_ISOCT(c)) {
                    int32_t val = JS7_UNOCT(c);

                    if (!peekChar(&c))
                        return false;

                    // Strict mode code allows only \0, then a non-digit.
                    if (val != 0 || IsAsciiDigit(c)) {
                        TokenStreamAnyChars& anyChars = anyCharsAccess();
                        if (parsingTemplate) {
                            anyChars.setInvalidTemplateEscape(sourceUnits.offset() - 2,
                                                              InvalidEscapeType::Octal);
                            continue;
                        }
                        if (!reportStrictModeError(JSMSG_DEPRECATED_OCTAL))
                            return false;
                        anyChars.flags.sawOctalEscape = true;
                    }

                    if (JS7_ISOCT(c)) {
                        val = 8 * val + JS7_UNOCT(c);
                        consumeKnownChar(c);
                        if (!peekChar(&c))
                            return false;
                        if (JS7_ISOCT(c)) {
                            int32_t save = val;
                            val = 8 * val + JS7_UNOCT(c);
                            if (val <= 0xFF)
                                consumeKnownChar(c);
                            else
                                val = save;
                        }
                    }

                    c = char16_t(val);
                }
                break;
            }
        } else if (SourceUnits::isRawEOLChar(c)) {
            if (!parsingTemplate) {
                ungetCharIgnoreEOL(c);
                const char delimiters[] = { untilChar, untilChar, '\0' };
                error(JSMSG_EOL_BEFORE_END_OF_STRING, delimiters);
                return false;
            }
            if (c == '\r') {
                c = '\n';

                // If it's a \r\n sequence: treat as a single EOL, skip over the \n.
                if (sourceUnits.hasRawChars())
                    sourceUnits.matchCodeUnit('\n');
            }

            if (!updateLineInfoForEOL())
                return false;

            anyCharsAccess().updateFlagsForEOL();
        } else if (parsingTemplate && c == '$' && matchChar('{')) {
            templateHead = true;
            break;
        }

        if (!tokenbuf.append(c)) {
            ReportOutOfMemory(anyCharsAccess().cx);
            return false;
        }
    }

    JSAtom* atom = atomizeChars(anyCharsAccess().cx, tokenbuf.begin(), tokenbuf.length());
    if (!atom)
        return false;

    noteBadToken.release();

    MOZ_ASSERT_IF(!parsingTemplate, !templateHead);

    TokenKind kind = !parsingTemplate
                     ? TokenKind::String
                     : templateHead
                     ? TokenKind::TemplateHead
                     : TokenKind::NoSubsTemplate;
    newAtomToken(kind, atom, start, modifier, out);
    return true;
}

const char*
TokenKindToDesc(TokenKind tt)
{
    switch (tt) {
#define EMIT_CASE(name, desc) case TokenKind::name: return desc;
      FOR_EACH_TOKEN_KIND(EMIT_CASE)
#undef EMIT_CASE
      case TokenKind::Limit:
        MOZ_ASSERT_UNREACHABLE("TokenKind::Limit should not be passed.");
        break;
    }

    return "<bad TokenKind>";
}

#ifdef DEBUG
const char*
TokenKindToString(TokenKind tt)
{
    switch (tt) {
#define EMIT_CASE(name, desc) case TokenKind::name: return "TokenKind::" #name;
      FOR_EACH_TOKEN_KIND(EMIT_CASE)
#undef EMIT_CASE
      case TokenKind::Limit: break;
    }

    return "<bad TokenKind>";
}
#endif

template class frontend::TokenStreamCharsBase<char16_t>;

template class frontend::TokenStreamChars<char16_t, frontend::TokenStreamAnyCharsAccess>;
template class frontend::TokenStreamSpecific<char16_t, frontend::TokenStreamAnyCharsAccess>;

template class
frontend::TokenStreamChars<char16_t, frontend::ParserAnyCharsAccess<frontend::GeneralParser<frontend::FullParseHandler, char16_t>>>;
template class
frontend::TokenStreamChars<char16_t, frontend::ParserAnyCharsAccess<frontend::GeneralParser<frontend::SyntaxParseHandler, char16_t>>>;

template class
frontend::TokenStreamSpecific<char16_t, frontend::ParserAnyCharsAccess<frontend::GeneralParser<frontend::FullParseHandler, char16_t>>>;
template class
frontend::TokenStreamSpecific<char16_t, frontend::ParserAnyCharsAccess<frontend::GeneralParser<frontend::SyntaxParseHandler, char16_t>>>;

} // namespace frontend

} // namespace js


JS_FRIEND_API(int)
js_fgets(char* buf, int size, FILE* file)
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
