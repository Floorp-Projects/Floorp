/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_TokenStream_h
#define frontend_TokenStream_h

// JS lexical scanner interface.

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Unused.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "jscntxt.h"
#include "jspubtd.h"

#include "frontend/ErrorReporter.h"
#include "frontend/TokenKind.h"
#include "js/UniquePtr.h"
#include "js/Vector.h"
#include "vm/ErrorReporting.h"
#include "vm/RegExpShared.h"
#include "vm/String.h"
#include "vm/Unicode.h"

struct KeywordInfo;

namespace js {
namespace frontend {

struct TokenPos {
    uint32_t    begin;  // Offset of the token's first char.
    uint32_t    end;    // Offset of 1 past the token's last char.

    TokenPos() {}
    TokenPos(uint32_t begin, uint32_t end) : begin(begin), end(end) {}

    // Return a TokenPos that covers left, right, and anything in between.
    static TokenPos box(const TokenPos& left, const TokenPos& right) {
        MOZ_ASSERT(left.begin <= left.end);
        MOZ_ASSERT(left.end <= right.begin);
        MOZ_ASSERT(right.begin <= right.end);
        return TokenPos(left.begin, right.end);
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

    bool encloses(const TokenPos& pos) const {
        return begin <= pos.begin && pos.end <= end;
    }
};

enum DecimalPoint { NoDecimal = false, HasDecimal = true };

enum class InvalidEscapeType {
    // No invalid character escapes.
    None,
    // A malformed \x escape.
    Hexadecimal,
    // A malformed \u escape.
    Unicode,
    // An otherwise well-formed \u escape which represents a
    // codepoint > 10FFFF.
    UnicodeOverflow,
    // An octal escape in a template token.
    Octal
};

class TokenStreamAnyChars;

struct Token
{
  private:
    // Sometimes the parser needs to inform the tokenizer to interpret
    // subsequent text in a particular manner: for example, to tokenize a
    // keyword as an identifier, not as the actual keyword, on the right-hand
    // side of a dotted property access.  Such information is communicated to
    // the tokenizer as a Modifier when getting the next token.
    //
    // Ideally this definition would reside in TokenStream as that's the real
    // user, but the debugging-use of it here causes a cyclic dependency (and
    // C++ provides no way to forward-declare an enum inside a class).  So
    // define it here, then typedef it into TokenStream with static consts to
    // bring the initializers into scope.
    enum Modifier
    {
        // Normal operation.
        None,

        // Looking for an operand, not an operator.  In practice, this means
        // that when '/' is seen, we look for a regexp instead of just returning
        // TOK_DIV.
        Operand,

        // Treat subsequent characters as the tail of a template literal, after
        // a template substitution, beginning with a "}", continuing with zero
        // or more template literal characters, and ending with either "${" or
        // the end of the template literal.  For example:
        //
        //   var entity = "world";
        //   var s = `Hello ${entity}!`;
        //                          ^ TemplateTail context
        TemplateTail,
    };
    enum ModifierException
    {
        NoException,

        // Used in following 2 cases:
        // a) After |yield| we look for a token on the same line that starts an
        // expression (Operand): |yield <expr>|.  If no token is found, the
        // |yield| stands alone, and the next token on a subsequent line must
        // be: a comma continuing a comma expression, a semicolon terminating
        // the statement that ended with |yield|, or the start of another
        // statement (possibly an expression statement).  The comma/semicolon
        // cases are gotten as operators (None), contrasting with Operand
        // earlier.
        // b) After an arrow function with a block body in an expression
        // statement, the next token must be: a colon in a conditional
        // expression, a comma continuing a comma expression, a semicolon
        // terminating the statement, or the token on a subsequent line that is
        // the start of another statement (possibly an expression statement).
        // Colon is gotten as operator (None), and it should only be gotten in
        // conditional expression and missing it results in SyntaxError.
        // Comma/semicolon cases are also gotten as operators (None), and 4th
        // case is gotten after them.  If no comma/semicolon found but EOL,
        // the next token should be gotten as operand in 4th case (especially if
        // '/' is the first character).  So we should peek the token as
        // operand before try getting colon/comma/semicolon.
        // See also the comment in Parser::assignExpr().
        NoneIsOperand,

        // If a semicolon is inserted automatically, the next token is already
        // gotten with None, but we expect Operand.
        OperandIsNone,
    };
    friend class TokenStreamAnyChars;

  public:
    TokenKind           type;           // char value or above enumerator
    TokenPos            pos;            // token position in file
    union {
      private:
        friend struct Token;
        PropertyName*   name;          // non-numeric atom
        JSAtom*         atom;          // potentially-numeric atom
        struct {
            double      value;          // floating point number
            DecimalPoint decimalPoint;  // literal contains '.'
        } number;
        RegExpFlag      reflags;        // regexp flags; use tokenbuf to access
                                        //   regexp chars
    } u;
#ifdef DEBUG
    Modifier modifier;                  // Modifier used to get this token
    ModifierException modifierException; // Exception for this modifier
#endif

    // Mutators

    void setName(PropertyName* name) {
        MOZ_ASSERT(type == TOK_NAME);
        u.name = name;
    }

    void setAtom(JSAtom* atom) {
        MOZ_ASSERT(type == TOK_STRING ||
                   type == TOK_TEMPLATE_HEAD ||
                   type == TOK_NO_SUBS_TEMPLATE);
        u.atom = atom;
    }

    void setRegExpFlags(RegExpFlag flags) {
        MOZ_ASSERT(type == TOK_REGEXP);
        MOZ_ASSERT((flags & AllFlags) == flags);
        u.reflags = flags;
    }

    void setNumber(double n, DecimalPoint decimalPoint) {
        MOZ_ASSERT(type == TOK_NUMBER);
        u.number.value = n;
        u.number.decimalPoint = decimalPoint;
    }

    // Type-safe accessors

    PropertyName* name() const {
        MOZ_ASSERT(type == TOK_NAME);
        return u.name->JSAtom::asPropertyName(); // poor-man's type verification
    }

    JSAtom* atom() const {
        MOZ_ASSERT(type == TOK_STRING ||
                   type == TOK_TEMPLATE_HEAD ||
                   type == TOK_NO_SUBS_TEMPLATE);
        return u.atom;
    }

    RegExpFlag regExpFlags() const {
        MOZ_ASSERT(type == TOK_REGEXP);
        MOZ_ASSERT((u.reflags & AllFlags) == u.reflags);
        return u.reflags;
    }

    double number() const {
        MOZ_ASSERT(type == TOK_NUMBER);
        return u.number.value;
    }

    DecimalPoint decimalPoint() const {
        MOZ_ASSERT(type == TOK_NUMBER);
        return u.number.decimalPoint;
    }
};

extern TokenKind
ReservedWordTokenKind(PropertyName* str);

extern const char*
ReservedWordToCharZ(PropertyName* str);

extern const char*
ReservedWordToCharZ(TokenKind tt);

// Ideally, tokenizing would be entirely independent of context.  But the
// strict mode flag, which is in SharedContext, affects tokenizing, and
// TokenStream needs to see it.
//
// This class is a tiny back-channel from TokenStream to the strict mode flag
// that avoids exposing the rest of SharedContext to TokenStream.
//
class StrictModeGetter {
  public:
    virtual bool strictMode() = 0;
};

class TokenStreamAnyChars: public ErrorReporter
{
  protected:
    TokenStreamAnyChars(JSContext* cx, const ReadOnlyCompileOptions& options, StrictModeGetter* smg);

    static const size_t ntokens = 4;                // 1 current + 2 lookahead, rounded
                                                    // to power of 2 to avoid divmod by 3
    static const unsigned maxLookahead = 2;
    static const unsigned ntokensMask = ntokens - 1;

  public:
    // Accessors.
    const Token& currentToken() const { return tokens[cursor]; }
    bool isCurrentTokenType(TokenKind type) const {
        return currentToken().type == type;
    }

    bool getMutedErrors() const { return mutedErrors; }

    MOZ_MUST_USE bool checkOptions();

  protected:
    PropertyName* reservedWordToPropertyName(TokenKind tt) const;

  public:
    PropertyName* currentName() const {
        if (isCurrentTokenType(TOK_NAME))
            return currentToken().name();

        MOZ_ASSERT(TokenKindIsPossibleIdentifierName(currentToken().type));
        return reservedWordToPropertyName(currentToken().type);
    }

    bool currentNameHasEscapes() const {
        if (isCurrentTokenType(TOK_NAME)) {
            TokenPos pos = currentToken().pos;
            return (pos.end - pos.begin) != currentToken().name()->length();
        }

        MOZ_ASSERT(TokenKindIsPossibleIdentifierName(currentToken().type));
        return false;
    }

    PropertyName* nextName() const {
        if (nextToken().type != TOK_NAME)
            return nextToken().name();

        MOZ_ASSERT(TokenKindIsPossibleIdentifierName(nextToken().type));
        return reservedWordToPropertyName(nextToken().type);
    }

    bool isCurrentTokenAssignment() const {
        return TokenKindIsAssignment(currentToken().type);
    }

    // Flag methods.
    bool isEOF() const { return flags.isEOF; }
    bool sawOctalEscape() const { return flags.sawOctalEscape; }
    bool hadError() const { return flags.hadError; }
    void clearSawOctalEscape() { flags.sawOctalEscape = false; }

    bool hasInvalidTemplateEscape() const {
        return invalidTemplateEscapeType != InvalidEscapeType::None;
    }
    void clearInvalidTemplateEscape() {
        invalidTemplateEscapeType = InvalidEscapeType::None;
    }

    static const uint32_t NoOffset = UINT32_MAX;

  protected:
    // This is protected because it should only be called by the tokenizer
    // while tokenizing not by, for example, BytecodeEmitter.
    bool strictMode() const { return strictModeGetter && strictModeGetter->strictMode(); }

    void setInvalidTemplateEscape(uint32_t offset, InvalidEscapeType type) {
        MOZ_ASSERT(type != InvalidEscapeType::None);
        if (invalidTemplateEscapeType != InvalidEscapeType::None)
            return;
        invalidTemplateEscapeOffset = offset;
        invalidTemplateEscapeType = type;
    }

    uint32_t invalidTemplateEscapeOffset = 0;
    InvalidEscapeType invalidTemplateEscapeType = InvalidEscapeType::None;

  protected:
    struct Flags
    {
        bool isEOF:1;           // Hit end of file.
        bool isDirtyLine:1;     // Non-whitespace since start of line.
        bool sawOctalEscape:1;  // Saw an octal character escape.
        bool hadError:1;        // Hit a syntax error, at start or during a
                                // token.

        Flags()
          : isEOF(), isDirtyLine(), sawOctalEscape(), hadError()
        {}
    };

  public:
    typedef Token::Modifier Modifier;
    static constexpr Modifier None = Token::None;
    static constexpr Modifier Operand = Token::Operand;
    static constexpr Modifier TemplateTail = Token::TemplateTail;

    typedef Token::ModifierException ModifierException;
    static constexpr ModifierException NoException = Token::NoException;
    static constexpr ModifierException NoneIsOperand = Token::NoneIsOperand;
    static constexpr ModifierException OperandIsNone = Token::OperandIsNone;

    void addModifierException(ModifierException modifierException) {
#ifdef DEBUG
        const Token& next = nextToken();

        // Permit adding the same exception multiple times.  This is important
        // particularly for Parser::assignExpr's early fast-path cases and
        // arrow function parsing: we want to add modifier exceptions in the
        // fast paths, then potentially (but not necessarily) duplicate them
        // after parsing all of an arrow function.
        if (next.modifierException == modifierException)
            return;

        if (next.modifierException == NoneIsOperand) {
            // Token after yield expression without operand already has
            // NoneIsOperand exception.
            MOZ_ASSERT(modifierException == OperandIsNone);
            MOZ_ASSERT(next.type != TOK_DIV,
                       "next token requires contextual specifier to be parsed unambiguously");

            // Do not update modifierException.
            return;
        }

        MOZ_ASSERT(next.modifierException == NoException);
        switch (modifierException) {
          case NoneIsOperand:
            MOZ_ASSERT(next.modifier == Operand);
            MOZ_ASSERT(next.type != TOK_DIV,
                       "next token requires contextual specifier to be parsed unambiguously");
            break;
          case OperandIsNone:
            MOZ_ASSERT(next.modifier == None);
            MOZ_ASSERT(next.type != TOK_DIV && next.type != TOK_REGEXP,
                       "next token requires contextual specifier to be parsed unambiguously");
            break;
          default:
            MOZ_CRASH("unexpected modifier exception");
        }
        tokens[(cursor + 1) & ntokensMask].modifierException = modifierException;
#endif
    }

    void
    verifyConsistentModifier(Modifier modifier, Token lookaheadToken) {
#ifdef DEBUG
        // Easy case: modifiers match.
        if (modifier == lookaheadToken.modifier)
            return;

        if (lookaheadToken.modifierException == OperandIsNone) {
            // getToken(Operand) permissibly following getToken().
            if (modifier == Operand && lookaheadToken.modifier == None)
                return;
        }

        if (lookaheadToken.modifierException == NoneIsOperand) {
            // getToken() permissibly following getToken(Operand).
            if (modifier == None && lookaheadToken.modifier == Operand)
                return;
        }

        MOZ_ASSERT_UNREACHABLE("this token was previously looked up with a "
                               "different modifier, potentially making "
                               "tokenization non-deterministic");
#endif
    }

#ifdef DEBUG
    inline bool debugHasNoLookahead() const {
        return lookahead == 0;
    }
#endif

    bool hasDisplayURL() const {
        return displayURL_ != nullptr;
    }

    char16_t* displayURL() {
        return displayURL_.get();
    }

    bool hasSourceMapURL() const {
        return sourceMapURL_ != nullptr;
    }

    char16_t* sourceMapURL() {
        return sourceMapURL_.get();
    }

    // This class maps a userbuf offset (which is 0-indexed) to a line number
    // (which is 1-indexed) and a column index (which is 0-indexed).
    class SourceCoords
    {
        // For a given buffer holding source code, |lineStartOffsets_| has one
        // element per line of source code, plus one sentinel element.  Each
        // non-sentinel element holds the buffer offset for the start of the
        // corresponding line of source code.  For this example script,
        // assuming an initialLineOffset of 0:
        //
        // 1  // xyz            [line starts at offset 0]
        // 2  var x;            [line starts at offset 7]
        // 3                    [line starts at offset 14]
        // 4  var y;            [line starts at offset 15]
        //
        // |lineStartOffsets_| is:
        //
        //   [0, 7, 14, 15, MAX_PTR]
        //
        // To convert a "line number" to a "line index" (i.e. an index into
        // |lineStartOffsets_|), subtract |initialLineNum_|.  E.g. line 3's
        // line index is (3 - initialLineNum_), which is 2.  Therefore
        // lineStartOffsets_[2] holds the buffer offset for the start of line 3,
        // which is 14.  (Note that |initialLineNum_| is often 1, but not
        // always.)
        //
        // The first element is always initialLineOffset, passed to the
        // constructor, and the last element is always the MAX_PTR sentinel.
        //
        // offset-to-line/column lookups are O(log n) in the worst case (binary
        // search), but in practice they're heavily clustered and we do better
        // than that by using the previous lookup's result (lastLineIndex_) as
        // a starting point.
        //
        // Checking if an offset lies within a particular line number
        // (isOnThisLine()) is O(1).
        //
        Vector<uint32_t, 128> lineStartOffsets_;
        uint32_t            initialLineNum_;
        uint32_t            initialColumn_;

        // This is mutable because it's modified on every search, but that fact
        // isn't visible outside this class.
        mutable uint32_t    lastLineIndex_;

        uint32_t lineIndexOf(uint32_t offset) const;

        static const uint32_t MAX_PTR = UINT32_MAX;

        uint32_t lineIndexToNum(uint32_t lineIndex) const { return lineIndex + initialLineNum_; }
        uint32_t lineNumToIndex(uint32_t lineNum)   const { return lineNum   - initialLineNum_; }
        uint32_t lineIndexAndOffsetToColumn(uint32_t lineIndex, uint32_t offset) const {
            uint32_t lineStartOffset = lineStartOffsets_[lineIndex];
            MOZ_RELEASE_ASSERT(offset >= lineStartOffset);
            uint32_t column = offset - lineStartOffset;
            if (lineIndex == 0)
                return column + initialColumn_;
            return column;
        }

      public:
        SourceCoords(JSContext* cx, uint32_t ln, uint32_t col, uint32_t initialLineOffset);

        MOZ_MUST_USE bool add(uint32_t lineNum, uint32_t lineStartOffset);
        MOZ_MUST_USE bool fill(const SourceCoords& other);

        bool isOnThisLine(uint32_t offset, uint32_t lineNum, bool* onThisLine) const {
            uint32_t lineIndex = lineNumToIndex(lineNum);
            if (lineIndex + 1 >= lineStartOffsets_.length()) // +1 due to sentinel
                return false;
            *onThisLine = lineStartOffsets_[lineIndex] <= offset &&
                          offset < lineStartOffsets_[lineIndex + 1];
            return true;
        }

        uint32_t lineNum(uint32_t offset) const;
        uint32_t columnIndex(uint32_t offset) const;
        void lineNumAndColumnIndex(uint32_t offset, uint32_t* lineNum, uint32_t* column) const;
    };

    SourceCoords srcCoords;

    JSAtomState& names() const {
        return cx->names();
    }

    JSContext* context() const {
        return cx;
    }

    virtual const ReadOnlyCompileOptions& options() const override final {
        return options_;
    }

    /**
     * Fill in |err|, excepting line-of-context-related fields.  If the token
     * stream has location information, use that and return true.  If it does
     * not, use the caller's location information and return false.
     */
    bool fillExcludingContext(ErrorMetadata* err, uint32_t offset);

    void updateFlagsForEOL();

    const Token& nextToken() const {
        MOZ_ASSERT(hasLookahead());
        return tokens[(cursor + 1) & ntokensMask];
    }

    bool hasLookahead() const { return lookahead > 0; }

  public:
    MOZ_MUST_USE bool compileWarning(ErrorMetadata&& metadata, UniquePtr<JSErrorNotes> notes,
                                     unsigned flags, unsigned errorNumber, va_list args);

    // Compute error metadata for an error at no offset.
    void computeErrorMetadataNoOffset(ErrorMetadata* err);

    virtual const char* getFilename() const override { return filename; }

    virtual void lineAndColumnAt(size_t offset, uint32_t* line, uint32_t* column) const override;
    virtual void currentLineAndColumn(uint32_t* line, uint32_t* column) const override;

    virtual bool hasTokenizationStarted() const override;
    virtual void reportErrorNoOffsetVA(unsigned errorNumber, va_list args) override;

  protected:
    // Options used for parsing/tokenizing.
    const ReadOnlyCompileOptions& options_;

    Token               tokens[ntokens];    // circular token buffer
    unsigned            cursor;             // index of last parsed token
    unsigned            lookahead;          // count of lookahead tokens
    unsigned            lineno;             // current line number
    Flags               flags;              // flags -- see above
    size_t              linebase;           // start of current line
    size_t              prevLinebase;       // start of previous line;  size_t(-1) if on the first line
    const char*         filename;           // input filename or null
    UniqueTwoByteChars  displayURL_;        // the user's requested source URL or null
    UniqueTwoByteChars  sourceMapURL_;      // source map's filename or null
    uint8_t             isExprEnding[TOK_LIMIT];// which tokens definitely terminate exprs?
    JSContext* const    cx;
    bool                mutedErrors;
    StrictModeGetter*   strictModeGetter;  // used to test for strict mode
};

// TokenStream is the lexical scanner for Javascript source text.
//
// It takes a buffer of char16_t characters and linearly scans it into |Token|s.
// Internally the class uses a four element circular buffer |tokens| of
// |Token|s. As an index for |tokens|, the member |cursor| points to the
// current token.
// Calls to getToken() increase |cursor| by one and return the new current
// token. If a TokenStream was just created, the current token is initialized
// with random data (i.e. not initialized). It is therefore important that
// one of the first four member functions listed below is called first.
// The circular buffer lets us go back up to two tokens from the last
// scanned token. Internally, the relative number of backward steps that were
// taken (via ungetToken()) after the last token was scanned is stored in
// |lookahead|.
//
// The following table lists in which situations it is safe to call each listed
// function. No checks are made by the functions in non-debug builds.
//
// Function Name     | Precondition; changes to |lookahead|
// ------------------+---------------------------------------------------------
// getToken          | none; if |lookahead > 0| then |lookahead--|
// peekToken         | none; if |lookahead == 0| then |lookahead == 1|
// peekTokenSameLine | none; if |lookahead == 0| then |lookahead == 1|
// matchToken        | none; if |lookahead > 0| and the match succeeds then
//                   |       |lookahead--|
// consumeKnownToken | none; if |lookahead > 0| then |lookahead--|
// ungetToken        | 0 <= |lookahead| <= |maxLookahead - 1|; |lookahead++|
//
// The behavior of the token scanning process (see getTokenInternal()) can be
// modified by calling one of the first four above listed member functions with
// an optional argument of type Modifier.  However, the modifier will be
// ignored unless |lookahead == 0| holds.  Due to constraints of the grammar,
// this turns out not to be a problem in practice. See the
// mozilla.dev.tech.js-engine.internals thread entitled 'Bug in the scanner?'
// for more details:
// https://groups.google.com/forum/?fromgroups=#!topic/mozilla.dev.tech.js-engine.internals/2JLH5jRcr7E).
//
// The methods seek() and tell() allow to rescan from a previous visited
// location of the buffer.
//
class MOZ_STACK_CLASS TokenStream final : public TokenStreamAnyChars
{
  public:
    using CharT = char16_t;
    using CharBuffer = Vector<CharT, 32>;

    TokenStream(JSContext* cx, const ReadOnlyCompileOptions& options,
                const CharT* base, size_t length, StrictModeGetter* smg);

    const CharBuffer& getTokenbuf() const { return tokenbuf; }

    // If there is an invalid escape in a template, report it and return false,
    // otherwise return true.
    bool checkForInvalidTemplateEscapeError() {
        if (invalidTemplateEscapeType == InvalidEscapeType::None)
            return true;

        reportInvalidEscapeError(invalidTemplateEscapeOffset, invalidTemplateEscapeType);
        return false;
    }

    // TokenStream-specific error reporters.
    void reportError(unsigned errorNumber, ...);

    // Report the given error at the current offset.
    void error(unsigned errorNumber, ...);

    // Report the given error at the given offset.
    void errorAt(uint32_t offset, unsigned errorNumber, ...);

    // Warn at the current offset.
    MOZ_MUST_USE bool warning(unsigned errorNumber, ...);

  private:
    // Compute a line of context for an otherwise-filled-in |err| at the given
    // offset in this token stream.  (This function basically exists to make
    // |computeErrorMetadata| more readable and shouldn't be called elsewhere.)
    MOZ_MUST_USE bool computeLineOfContext(ErrorMetadata* err, uint32_t offset);

  public:
    // Compute error metadata for an error at the given offset.
    MOZ_MUST_USE bool computeErrorMetadata(ErrorMetadata* err, uint32_t offset);

    // General-purpose error reporters.  You should avoid calling these
    // directly, and instead use the more succinct alternatives (error(),
    // warning(), &c.) in TokenStream, Parser, and BytecodeEmitter.
    //
    // These functions take a |va_list*| parameter, not a |va_list| parameter,
    // to hack around bug 1363116.  (Longer-term, the right fix is of course to
    // not use ellipsis functions or |va_list| at all in error reporting.)
    bool reportStrictModeErrorNumberVA(UniquePtr<JSErrorNotes> notes, uint32_t offset,
                                       bool strictMode, unsigned errorNumber, va_list* args);
    bool reportExtraWarningErrorNumberVA(UniquePtr<JSErrorNotes> notes, uint32_t offset,
                                         unsigned errorNumber, va_list* args);

    JSAtom* getRawTemplateStringAtom() {
        MOZ_ASSERT(currentToken().type == TOK_TEMPLATE_HEAD ||
                   currentToken().type == TOK_NO_SUBS_TEMPLATE);
        const CharT* cur = userbuf.rawCharPtrAt(currentToken().pos.begin + 1);
        const CharT* end;
        if (currentToken().type == TOK_TEMPLATE_HEAD) {
            // Of the form    |`...${|   or   |}...${|
            end = userbuf.rawCharPtrAt(currentToken().pos.end - 2);
        } else {
            // NO_SUBS_TEMPLATE is of the form   |`...`|   or   |}...`|
            end = userbuf.rawCharPtrAt(currentToken().pos.end - 1);
        }

        CharBuffer charbuf(cx);
        while (cur < end) {
            CharT ch = *cur;
            if (ch == '\r') {
                ch = '\n';
                if ((cur + 1 < end) && (*(cur + 1) == '\n'))
                    cur++;
            }
            if (!charbuf.append(ch))
                return nullptr;
            cur++;
        }
        return AtomizeChars(cx, charbuf.begin(), charbuf.length());
    }

  private:
    // This is private because it should only be called by the tokenizer while
    // tokenizing not by, for example, BytecodeEmitter.
    bool reportStrictModeError(unsigned errorNumber, ...);

    void reportInvalidEscapeError(uint32_t offset, InvalidEscapeType type) {
        switch (type) {
            case InvalidEscapeType::None:
                MOZ_ASSERT_UNREACHABLE("unexpected InvalidEscapeType");
                return;
            case InvalidEscapeType::Hexadecimal:
                errorAt(offset, JSMSG_MALFORMED_ESCAPE, "hexadecimal");
                return;
            case InvalidEscapeType::Unicode:
                errorAt(offset, JSMSG_MALFORMED_ESCAPE, "Unicode");
                return;
            case InvalidEscapeType::UnicodeOverflow:
                errorAt(offset, JSMSG_UNICODE_OVERFLOW, "escape sequence");
                return;
            case InvalidEscapeType::Octal:
                errorAt(offset, JSMSG_DEPRECATED_OCTAL);
                return;
        }
    }

    static JSAtom* atomize(JSContext* cx, CharBuffer& cb);
    MOZ_MUST_USE bool putIdentInTokenbuf(const CharT* identStart);

  public:
    // Advance to the next token.  If the token stream encountered an error,
    // return false.  Otherwise return true and store the token kind in |*ttp|.
    MOZ_MUST_USE bool getToken(TokenKind* ttp, Modifier modifier = None) {
        // Check for a pushed-back token resulting from mismatching lookahead.
        if (lookahead != 0) {
            MOZ_ASSERT(!flags.hadError);
            lookahead--;
            cursor = (cursor + 1) & ntokensMask;
            TokenKind tt = currentToken().type;
            MOZ_ASSERT(tt != TOK_EOL);
            verifyConsistentModifier(modifier, currentToken());
            *ttp = tt;
            return true;
        }

        return getTokenInternal(ttp, modifier);
    }

    // Push the last scanned token back into the stream.
    void ungetToken() {
        MOZ_ASSERT(lookahead < maxLookahead);
        lookahead++;
        cursor = (cursor - 1) & ntokensMask;
    }

    MOZ_MUST_USE bool peekToken(TokenKind* ttp, Modifier modifier = None) {
        if (lookahead > 0) {
            MOZ_ASSERT(!flags.hadError);
            verifyConsistentModifier(modifier, nextToken());
            *ttp = nextToken().type;
            return true;
        }
        if (!getTokenInternal(ttp, modifier))
            return false;
        ungetToken();
        return true;
    }

    MOZ_MUST_USE bool peekTokenPos(TokenPos* posp, Modifier modifier = None) {
        if (lookahead == 0) {
            TokenKind tt;
            if (!getTokenInternal(&tt, modifier))
                return false;
            ungetToken();
            MOZ_ASSERT(hasLookahead());
        } else {
            MOZ_ASSERT(!flags.hadError);
            verifyConsistentModifier(modifier, nextToken());
        }
        *posp = nextToken().pos;
        return true;
    }

    MOZ_MUST_USE bool peekOffset(uint32_t* offset, Modifier modifier = None) {
        TokenPos pos;
        if (!peekTokenPos(&pos, modifier))
            return false;
        *offset = pos.begin;
        return true;
    }

    // This is like peekToken(), with one exception:  if there is an EOL
    // between the end of the current token and the start of the next token, it
    // return true and store TOK_EOL in |*ttp|.  In that case, no token with
    // TOK_EOL is actually created, just a TOK_EOL TokenKind is returned, and
    // currentToken() shouldn't be consulted.  (This is the only place TOK_EOL
    // is produced.)
    MOZ_ALWAYS_INLINE MOZ_MUST_USE bool
    peekTokenSameLine(TokenKind* ttp, Modifier modifier = None) {
        const Token& curr = currentToken();

        // If lookahead != 0, we have scanned ahead at least one token, and
        // |lineno| is the line that the furthest-scanned token ends on.  If
        // it's the same as the line that the current token ends on, that's a
        // stronger condition than what we are looking for, and we don't need
        // to return TOK_EOL.
        if (lookahead != 0) {
            bool onThisLine;
            if (!srcCoords.isOnThisLine(curr.pos.end, lineno, &onThisLine)) {
                reportError(JSMSG_OUT_OF_MEMORY);
                return false;
            }

            if (onThisLine) {
                MOZ_ASSERT(!flags.hadError);
                verifyConsistentModifier(modifier, nextToken());
                *ttp = nextToken().type;
                return true;
            }
        }

        // The above check misses two cases where we don't have to return
        // TOK_EOL.
        // - The next token starts on the same line, but is a multi-line token.
        // - The next token starts on the same line, but lookahead==2 and there
        //   is a newline between the next token and the one after that.
        // The following test is somewhat expensive but gets these cases (and
        // all others) right.
        TokenKind tmp;
        if (!getToken(&tmp, modifier))
            return false;
        const Token& next = currentToken();
        ungetToken();

        *ttp = srcCoords.lineNum(curr.pos.end) == srcCoords.lineNum(next.pos.begin)
             ? next.type
             : TOK_EOL;
        return true;
    }

    // Get the next token from the stream if its kind is |tt|.
    MOZ_MUST_USE bool matchToken(bool* matchedp, TokenKind tt, Modifier modifier = None) {
        TokenKind token;
        if (!getToken(&token, modifier))
            return false;
        if (token == tt) {
            *matchedp = true;
        } else {
            ungetToken();
            *matchedp = false;
        }
        return true;
    }

    void consumeKnownToken(TokenKind tt, Modifier modifier = None) {
        bool matched;
        MOZ_ASSERT(hasLookahead());
        MOZ_ALWAYS_TRUE(matchToken(&matched, tt, modifier));
        MOZ_ALWAYS_TRUE(matched);
    }

    MOZ_MUST_USE bool nextTokenEndsExpr(bool* endsExpr) {
        TokenKind tt;
        if (!peekToken(&tt))
            return false;

        *endsExpr = isExprEnding[tt];
        if (*endsExpr) {
            // If the next token ends an overall Expression, we'll parse this
            // Expression without ever invoking Parser::orExpr().  But we need
            // that function's side effect of adding this modifier exception,
            // so we have to do it manually here.
            addModifierException(OperandIsNone);
        }
        return true;
    }

    class MOZ_STACK_CLASS Position {
      public:
        // The Token fields may contain pointers to atoms, so for correct
        // rooting we must ensure collection of atoms is disabled while objects
        // of this class are live.  Do this by requiring a dummy AutoKeepAtoms
        // reference in the constructor.
        //
        // This class is explicity ignored by the analysis, so don't add any
        // more pointers to GC things here!
        explicit Position(AutoKeepAtoms&) { }
      private:
        Position(const Position&) = delete;
        friend class TokenStream;
        const CharT* buf;
        Flags flags;
        unsigned lineno;
        size_t linebase;
        size_t prevLinebase;
        Token currentToken;
        unsigned lookahead;
        Token lookaheadTokens[maxLookahead];
    };

    MOZ_MUST_USE bool advance(size_t position);
    void tell(Position*);
    void seek(const Position& pos);
    MOZ_MUST_USE bool seek(const Position& pos, const TokenStream& other);

    const CharT* rawCharPtrAt(size_t offset) const {
        return userbuf.rawCharPtrAt(offset);
    }

    const CharT* rawLimit() const {
        return userbuf.limit();
    }

  private:
    // This is the low-level interface to the JS source code buffer.  It just
    // gets raw chars, basically.  TokenStreams functions are layered on top
    // and do some extra stuff like converting all EOL sequences to '\n',
    // tracking the line number, and setting |flags.isEOF|.  (The "raw" in "raw
    // chars" refers to the lack of EOL sequence normalization.)
    //
    // buf[0..length-1] often represents a substring of some larger source,
    // where we have only the substring in memory. The |startOffset| argument
    // indicates the offset within this larger string at which our string
    // begins, the offset of |buf[0]|.
    class TokenBuf {
      public:
        TokenBuf(const CharT* buf, size_t length, size_t startOffset)
          : base_(buf),
            startOffset_(startOffset),
            limit_(buf + length),
            ptr(buf)
        { }

        bool hasRawChars() const {
            return ptr < limit_;
        }

        bool atStart() const {
            return offset() == 0;
        }

        size_t startOffset() const {
            return startOffset_;
        }

        size_t offset() const {
            return startOffset_ + mozilla::PointerRangeSize(base_, ptr);
        }

        const CharT* rawCharPtrAt(size_t offset) const {
            MOZ_ASSERT(startOffset_ <= offset);
            MOZ_ASSERT(offset - startOffset_ <= mozilla::PointerRangeSize(base_, limit_));
            return base_ + (offset - startOffset_);
        }

        const CharT* limit() const {
            return limit_;
        }

        CharT getRawChar() {
            return *ptr++;      // this will nullptr-crash if poisoned
        }

        CharT peekRawChar() const {
            return *ptr;        // this will nullptr-crash if poisoned
        }

        bool matchRawChar(CharT c) {
            if (*ptr == c) {    // this will nullptr-crash if poisoned
                ptr++;
                return true;
            }
            return false;
        }

        bool matchRawCharBackwards(CharT c) {
            MOZ_ASSERT(ptr);     // make sure it hasn't been poisoned
            if (*(ptr - 1) == c) {
                ptr--;
                return true;
            }
            return false;
        }

        void ungetRawChar() {
            MOZ_ASSERT(ptr);     // make sure it hasn't been poisoned
            ptr--;
        }

        const CharT* addressOfNextRawChar(bool allowPoisoned = false) const {
            MOZ_ASSERT_IF(!allowPoisoned, ptr);     // make sure it hasn't been poisoned
            return ptr;
        }

        // Use this with caution!
        void setAddressOfNextRawChar(const CharT* a, bool allowPoisoned = false) {
            MOZ_ASSERT_IF(!allowPoisoned, a);
            ptr = a;
        }

#ifdef DEBUG
        // Poison the TokenBuf so it cannot be accessed again.
        void poison() {
            ptr = nullptr;
        }
#endif

        static bool isRawEOLChar(int32_t c) {
            return c == '\n' ||
                   c == '\r' ||
                   c == unicode::LINE_SEPARATOR ||
                   c == unicode::PARA_SEPARATOR;
        }

        // Returns the offset of the next EOL, but stops once 'max' characters
        // have been scanned (*including* the char at startOffset_).
        size_t findEOLMax(size_t start, size_t max);

      private:
        const CharT* base_;          // base of buffer
        uint32_t startOffset_;          // offset of base_[0]
        const CharT* limit_;         // limit for quick bounds check
        const CharT* ptr;            // next char to get
    };

    MOZ_MUST_USE bool getTokenInternal(TokenKind* ttp, Modifier modifier);

    MOZ_MUST_USE bool getStringOrTemplateToken(int untilChar, Token** tp);

    // Try to get the next character, normalizing '\r', '\r\n', and '\n' into
    // '\n'.  Also updates internal line-counter state.  Return true on success
    // and store the character in |*c|.  Return false and leave |*c| undefined
    // on failure.
    MOZ_MUST_USE bool getChar(int32_t* cp);
    int32_t getCharIgnoreEOL();

    void ungetChar(int32_t c);
    void ungetCharIgnoreEOL(int32_t c);
    Token* newToken(ptrdiff_t adjust);
    uint32_t peekUnicodeEscape(uint32_t* codePoint);
    uint32_t peekExtendedUnicodeEscape(uint32_t* codePoint);
    uint32_t matchUnicodeEscapeIdStart(uint32_t* codePoint);
    bool matchUnicodeEscapeIdent(uint32_t* codePoint);
    bool matchTrailForLeadSurrogate(char16_t lead, char16_t* trail, uint32_t* codePoint);
    bool peekChars(int n, CharT* cp);

    MOZ_MUST_USE bool getDirectives(bool isMultiline, bool shouldWarnDeprecated);
    MOZ_MUST_USE bool getDirective(bool isMultiline, bool shouldWarnDeprecated,
                                   const char* directive, uint8_t directiveLength,
                                   const char* errorMsgPragma,
                                   UniquePtr<CharT[], JS::FreePolicy>* destination);
    MOZ_MUST_USE bool getDisplayURL(bool isMultiline, bool shouldWarnDeprecated);
    MOZ_MUST_USE bool getSourceMappingURL(bool isMultiline, bool shouldWarnDeprecated);

    // |expect| cannot be an EOL char.
    bool matchChar(int32_t expect) {
        MOZ_ASSERT(!TokenBuf::isRawEOLChar(expect));
        return MOZ_LIKELY(userbuf.hasRawChars()) &&
               userbuf.matchRawChar(expect);
    }

    void consumeKnownChar(int32_t expect) {
        int32_t c;
        MOZ_ALWAYS_TRUE(getChar(&c));
        MOZ_ASSERT(c == expect);
    }

    MOZ_MUST_USE bool peekChar(int32_t* c) {
        if (!getChar(c))
            return false;
        ungetChar(*c);
        return true;
    }

    void skipChars(uint32_t n) {
        while (n-- > 0) {
            MOZ_ASSERT(userbuf.hasRawChars());
            mozilla::DebugOnly<int32_t> c = getCharIgnoreEOL();
            MOZ_ASSERT(c != '\n');
        }
    }

    void skipCharsIgnoreEOL(uint8_t n) {
        while (n-- > 0) {
            MOZ_ASSERT(userbuf.hasRawChars());
            getCharIgnoreEOL();
        }
    }

    MOZ_MUST_USE MOZ_ALWAYS_INLINE bool updateLineInfoForEOL();

    TokenBuf            userbuf;            // user input buffer
    CharBuffer          tokenbuf;           // current token string buffer
};

extern const char*
TokenKindToDesc(TokenKind tt);

} // namespace frontend
} // namespace js

extern JS_FRIEND_API(int)
js_fgets(char* buf, int size, FILE* file);

#ifdef DEBUG
extern const char*
TokenKindToString(js::frontend::TokenKind tt);
#endif

#endif /* frontend_TokenStream_h */
