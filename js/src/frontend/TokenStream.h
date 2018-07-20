/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Streaming access to the raw tokens of JavaScript source.
 *
 * Because JS tokenization is context-sensitive -- a '/' could be either a
 * regular expression *or* a division operator depending on context -- the
 * various token stream classes are mostly not useful outside of the Parser
 * where they reside.  We should probably eventually merge the two concepts.
 */
#ifndef frontend_TokenStream_h
#define frontend_TokenStream_h

/*
 * [SMDOC] Parser Token Stream
 *
 * A token stream exposes the raw tokens -- operators, names, numbers,
 * keywords, and so on -- of JavaScript source code.
 *
 * These are the components of the overall token stream concept:
 * TokenStreamShared, TokenStreamAnyChars, TokenStreamCharsBase<CharT>,
 * TokenStreamChars<CharT>, and TokenStreamSpecific<CharT, AnyCharsAccess>.
 *
 * == TokenStreamShared → ∅ ==
 *
 * Certain aspects of tokenizing are used everywhere:
 *
 *   * modifiers (used to select which context-sensitive interpretation of a
 *     character should be used to decide what token it is), modifier
 *     exceptions, and modifier assertion handling;
 *   * flags on the overall stream (have we encountered any characters on this
 *     line?  have we hit a syntax error?  and so on);
 *   * and certain token-count constants.
 *
 * These are all defined in TokenStreamShared.  (They could be namespace-
 * scoped, but it seems tentatively better not to clutter the namespace.)
 *
 * == TokenStreamAnyChars → TokenStreamShared ==
 *
 * Certain aspects of tokenizing have meaning independent of the character type
 * of the source text being tokenized: line/column number information, tokens
 * in lookahead from determining the meaning of a prior token, compilation
 * options, the filename, flags, source map URL, access to details of the
 * current and next tokens (is the token of the given type?  what name or
 * number is contained in the token?  and other queries), and others.
 *
 * All this data/functionality *could* be duplicated for both single-byte and
 * double-byte tokenizing, but there are two problems.  First, it's potentially
 * wasteful if the compiler doesnt recognize it can unify the concepts.  (And
 * if any-character concepts are intermixed with character-specific concepts,
 * potentially the compiler *can't* unify them because offsets into the
 * hypothetical TokenStream<CharT>s would differ.)  Second, some of this stuff
 * needs to be accessible in ParserBase, the aspects of JS language parsing
 * that have meaning independent of the character type of the source text being
 * parsed.  So we need a separate data structure that ParserBase can hold on to
 * for it.  (ParserBase isn't the only instance of this, but it's certainly the
 * biggest case of it.)  Ergo, TokenStreamAnyChars.
 *
 * == TokenStreamCharsShared → ∅ ==
 *
 * Some functionality has meaning independent of character type, yet has no use
 * *unless* you know the character type in actual use.  It *could* live in
 * TokenStreamAnyChars, but it makes more sense to live in a separate class
 * that character-aware token information can simply inherit.
 *
 * This class currently exists only to contain a char16_t buffer, transiently
 * used to accumulate strings in tricky cases that can't just be read directly
 * from source text.  It's not used outside character-aware tokenizing, so it
 * doesn't make sense in TokenStreamAnyChars.
 *
 * == TokenStreamCharsBase<CharT> → TokenStreamCharsShared ==
 *
 * Certain data structures in tokenizing are character-type-specific: namely,
 * the various pointers identifying the source text (including current offset
 * and end).
 *
 * Additionally, some functions operating on this data are defined the same way
 * no matter what character type you have (e.g. current offset in code units
 * into the source text) or share a common interface regardless of character
 * type (e.g. consume the next code unit if it has a given value).
 *
 * All such functionality lives in TokenStreamCharsBase<CharT>.
 *
 * == SpecializedTokenStreamCharsBase<CharT> → TokenStreamCharsBase<CharT> ==
 *
 * Certain tokenizing functionality is specific to a single character type.
 * For example, JS's UTF-16 encoding recognizes no coding errors, because lone
 * surrogates are not an error; but a UTF-8 encoding must recognize a variety
 * of validation errors.  Such functionality is defined only in the appropriate
 * SpecializedTokenStreamCharsBase specialization.
 *
 * == GeneralTokenStreamChars<CharT, AnyCharsAccess> →
 *    SpecializedTokenStreamCharsBase<CharT> ==
 *
 * Some functionality operates differently on different character types, just
 * as for TokenStreamCharsBase, but additionally requires access to character-
 * type-agnostic information in TokenStreamAnyChars.  For example, getting the
 * next character performs different steps for different character types and
 * must access TokenStreamAnyChars to update line break information.
 *
 * Such functionality, if it can be defined using the same algorithm for all
 * character types, lives in GeneralTokenStreamChars<CharT, AnyCharsAccess>.
 * The AnyCharsAccess parameter provides a way for a GeneralTokenStreamChars
 * instance to access its corresponding TokenStreamAnyChars, without inheriting
 * from it.
 *
 * GeneralTokenStreamChars<CharT, AnyCharsAccess> is just functionality, no
 * actual member data.
 *
 * Such functionality all lives in TokenStreamChars<CharT, AnyCharsAccess>, a
 * declared-but-not-defined template class whose specializations have a common
 * public interface (plus whatever private helper functions are desirable).
 *
 * == TokenStreamChars<CharT, AnyCharsAccess> →
 *    GeneralTokenStreamChars<CharT, AnyCharsAccess> ==
 *
 * Some functionality is like that in GeneralTokenStreamChars, *but* it's
 * defined entirely differently for different character types.
 *
 * For example, consider "match a multi-code unit code point" (hypothetically:
 * we've only implemented two-byte tokenizing right now):
 *
 *   * For two-byte text, there must be two code units to get, the leading code
 *     unit must be a UTF-16 lead surrogate, and the trailing code unit must be
 *     a UTF-16 trailing surrogate.  (If any of these fail to hold, a next code
 *     unit encodes that code point and is not multi-code unit.)
 *   * For single-byte Latin-1 text, there are no multi-code unit code points.
 *   * For single-byte UTF-8 text, the first code unit must have N > 1 of its
 *     highest bits set (and the next unset), and |N - 1| successive code units
 *     must have their high bit set and next-highest bit unset, *and*
 *     concatenating all unconstrained bits together must not produce a code
 *     point value that could have been encoded in fewer code units.
 *
 * This functionality can't be implemented as member functions in
 * GeneralTokenStreamChars because we'd need to *partially specialize* those
 * functions -- hold CharT constant while letting AnyCharsAccess vary.  But
 * C++ forbids function template partial specialization like this: either you
 * fix *all* parameters or you fix none of them.
 *
 * Fortunately, C++ *does* allow *class* template partial specialization.  So
 * TokenStreamChars is a template class with one specialization per CharT.
 * Functions can be defined differently in the different specializations,
 * because AnyCharsAccess as the only template parameter on member functions
 * *can* vary.
 *
 * All TokenStreamChars<CharT, AnyCharsAccess> specializations, one per CharT,
 * are just functionality, no actual member data.
 *
 * == TokenStreamSpecific<CharT, AnyCharsAccess> →
 *    TokenStreamChars<CharT, AnyCharsAccess>, TokenStreamShared ==
 *
 * TokenStreamSpecific is operations that are parametrized on character type
 * but implement the *general* idea of tokenizing, without being intrinsically
 * tied to character type.  Notably, this includes all operations that can
 * report warnings or errors at particular offsets, because we include a line
 * of context with such errors -- and that necessarily accesses the raw
 * characters of their specific type.
 *
 * Much TokenStreamSpecific operation depends on functionality in
 * TokenStreamAnyChars.  The obvious solution is to inherit it -- but this
 * doesn't work in Parser: its ParserBase base class needs some
 * TokenStreamAnyChars functionality without knowing character type.
 *
 * The AnyCharsAccess type parameter is a class that statically converts from a
 * TokenStreamSpecific* to its corresponding TokenStreamAnyChars.  The
 * TokenStreamSpecific in Parser<ParseHandler, CharT> can then specify a class
 * that properly converts from TokenStreamSpecific Parser::tokenStream to
 * TokenStreamAnyChars ParserBase::anyChars.
 *
 * Could we hardcode one set of offset calculations for this and eliminate
 * AnyCharsAccess?  No.  Offset calculations possibly could be hardcoded if
 * TokenStreamSpecific were present in Parser before Parser::handler, assuring
 * the same offsets in all Parser-related cases.  But there's still a separate
 * TokenStream class, that requires different offset calculations.  So even if
 * we wanted to hardcode this (it's not clear we would, because forcing the
 * TokenStreamSpecific declarer to specify this is more explicit), we couldn't.
 */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Casting.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TextUtils.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Unused.h"
#include "mozilla/Utf8.h"

#include <algorithm>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

#include "jspubtd.h"

#include "frontend/ErrorReporter.h"
#include "frontend/TokenKind.h"
#include "js/UniquePtr.h"
#include "js/Vector.h"
#include "util/Text.h"
#include "util/Unicode.h"
#include "vm/ErrorReporting.h"
#include "vm/JSContext.h"
#include "vm/RegExpShared.h"
#include "vm/StringType.h"

struct KeywordInfo;

namespace js {
namespace frontend {

struct TokenPos {
    uint32_t    begin;  // Offset of the token's first code unit.
    uint32_t    end;    // Offset of 1 past the token's last code unit.

    TokenPos()
      : begin(0),
        end(0)
    {}
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

// The only escapes found in IdentifierName are of the Unicode flavor.
enum class IdentifierEscapes { None, SawUnicodeEscape };

class TokenStreamShared;

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
        // Div.
        Operand,

        // Treat subsequent code units as the tail of a template literal, after
        // a template substitution, beginning with a "}", continuing with zero
        // or more template literal code units, and ending with either "${" or
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
        // the next token should be gotten as operand in 4th case (especially
        // if '/' is the first code unit).  So we should peek the token as
        // operand before try getting colon/comma/semicolon.
        // See also the comment in Parser::assignExpr().
        NoneIsOperand,

        // If a semicolon is inserted automatically, the next token is already
        // gotten with None, but we expect Operand.
        OperandIsNone,
    };
    friend class TokenStreamShared;

  public:
    // WARNING: TokenStreamPosition assumes that the only GC things a Token
    //          includes are atoms.  DON'T ADD NON-ATOM GC THING POINTERS HERE
    //          UNLESS YOU ADD ADDITIONAL ROOTING TO THAT CLASS.

    /** The type of this token. */
    TokenKind type;

    /** The token's position in the overall script. */
    TokenPos pos;

    union {
      private:
        friend struct Token;

        /** Non-numeric atom. */
        PropertyName* name;

        /** Potentially-numeric atom. */
        JSAtom* atom;

        struct {
            /** Numeric literal's value. */
            double value;

            /** Does the numeric literal contain a '.'? */
            DecimalPoint decimalPoint;
        } number;

        /** Regular expression flags; use charBuffer to access source chars. */
        RegExpFlag reflags;
    } u;

#ifdef DEBUG
    /** The modifier used to get this token. */
    Modifier modifier;

    /**
     * Exception for this modifier to permit modifier mismatches in certain
     * situations.
     */
    ModifierException modifierException;
#endif

    // Mutators

    void setName(PropertyName* name) {
        MOZ_ASSERT(type == TokenKind::Name);
        u.name = name;
    }

    void setAtom(JSAtom* atom) {
        MOZ_ASSERT(type == TokenKind::String ||
                   type == TokenKind::TemplateHead ||
                   type == TokenKind::NoSubsTemplate);
        u.atom = atom;
    }

    void setRegExpFlags(RegExpFlag flags) {
        MOZ_ASSERT(type == TokenKind::RegExp);
        MOZ_ASSERT((flags & AllFlags) == flags);
        u.reflags = flags;
    }

    void setNumber(double n, DecimalPoint decimalPoint) {
        MOZ_ASSERT(type == TokenKind::Number);
        u.number.value = n;
        u.number.decimalPoint = decimalPoint;
    }

    // Type-safe accessors

    PropertyName* name() const {
        MOZ_ASSERT(type == TokenKind::Name);
        return u.name->JSAtom::asPropertyName(); // poor-man's type verification
    }

    JSAtom* atom() const {
        MOZ_ASSERT(type == TokenKind::String ||
                   type == TokenKind::TemplateHead ||
                   type == TokenKind::NoSubsTemplate);
        return u.atom;
    }

    RegExpFlag regExpFlags() const {
        MOZ_ASSERT(type == TokenKind::RegExp);
        MOZ_ASSERT((u.reflags & AllFlags) == u.reflags);
        return u.reflags;
    }

    double number() const {
        MOZ_ASSERT(type == TokenKind::Number);
        return u.number.value;
    }

    DecimalPoint decimalPoint() const {
        MOZ_ASSERT(type == TokenKind::Number);
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

struct TokenStreamFlags
{
    bool isEOF:1;           // Hit end of file.
    bool isDirtyLine:1;     // Non-whitespace since start of line.
    bool sawOctalEscape:1;  // Saw an octal character escape.
    bool hadError:1;        // Hit a syntax error, at start or during a
                            // token.

    TokenStreamFlags()
      : isEOF(), isDirtyLine(), sawOctalEscape(), hadError()
    {}
};

template<typename CharT>
class TokenStreamPosition;

/**
 * TokenStream types and constants that are used in both TokenStreamAnyChars
 * and TokenStreamSpecific.  Do not add any non-static data members to this
 * class!
 */
class TokenStreamShared
{
  protected:
    static constexpr size_t ntokens = 4; // 1 current + 2 lookahead, rounded
                                         // to power of 2 to avoid divmod by 3

    static constexpr unsigned ntokensMask = ntokens - 1;

    template<typename CharT> friend class TokenStreamPosition;

  public:
    static constexpr unsigned maxLookahead = 2;

    static constexpr uint32_t NoOffset = UINT32_MAX;

    using Modifier = Token::Modifier;
    static constexpr Modifier None = Token::None;
    static constexpr Modifier Operand = Token::Operand;
    static constexpr Modifier TemplateTail = Token::TemplateTail;

    using ModifierException = Token::ModifierException;
    static constexpr ModifierException NoException = Token::NoException;
    static constexpr ModifierException NoneIsOperand = Token::NoneIsOperand;
    static constexpr ModifierException OperandIsNone = Token::OperandIsNone;

    static void
    verifyConsistentModifier(Modifier modifier, Token lookaheadToken)
    {
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
};

static_assert(mozilla::IsEmpty<TokenStreamShared>::value,
              "TokenStreamShared shouldn't bloat classes that inherit from it");

template<typename CharT, class AnyCharsAccess>
class TokenStreamSpecific;

template<typename CharT>
class MOZ_STACK_CLASS TokenStreamPosition final
{
  public:
    // The JS_HAZ_ROOTED is permissible below because: 1) the only field in
    // TokenStreamPosition that can keep GC things alive is Token, 2) the only
    // GC things Token can keep alive are atoms, and 3) the AutoKeepAtoms&
    // passed to the constructor here represents that collection of atoms
    // is disabled while atoms in Tokens in this Position are alive.  DON'T
    // ADD NON-ATOM GC THING POINTERS HERE!  They would create a rooting
    // hazard that JS_HAZ_ROOTED will cause to be ignored.
    template<class AnyCharsAccess>
    inline TokenStreamPosition(AutoKeepAtoms& keepAtoms,
                               TokenStreamSpecific<CharT, AnyCharsAccess>& tokenStream);

  private:
    TokenStreamPosition(const TokenStreamPosition&) = delete;

    // Technically only TokenStreamSpecific<CharT, AnyCharsAccess>::seek with
    // CharT constant and AnyCharsAccess varying must be friended, but 1) it's
    // hard to friend one function in template classes, and 2) C++ doesn't
    // allow partial friend specialization to target just that single class.
    template<typename Char, class AnyCharsAccess> friend class TokenStreamSpecific;

    const CharT* buf;
    TokenStreamFlags flags;
    unsigned lineno;
    size_t linebase;
    size_t prevLinebase;
    Token currentToken;
    unsigned lookahead;
    Token lookaheadTokens[TokenStreamShared::maxLookahead];
} JS_HAZ_ROOTED;

class TokenStreamAnyChars
  : public TokenStreamShared
{
  public:
    TokenStreamAnyChars(JSContext* cx, const ReadOnlyCompileOptions& options,
                        StrictModeGetter* smg);

    template<typename CharT, class AnyCharsAccess> friend class GeneralTokenStreamChars;
    template<typename CharT, class AnyCharsAccess> friend class TokenStreamChars;
    template<typename CharT, class AnyCharsAccess> friend class TokenStreamSpecific;

    template<typename CharT> friend class TokenStreamPosition;

    // Accessors.
    unsigned cursor() const { return cursor_; }
    unsigned nextCursor() const { return (cursor_ + 1) & ntokensMask; }
    unsigned aheadCursor(unsigned steps) const { return (cursor_ + steps) & ntokensMask; }

    const Token& currentToken() const { return tokens[cursor()]; }
    bool isCurrentTokenType(TokenKind type) const {
        return currentToken().type == type;
    }

    MOZ_MUST_USE bool checkOptions();

  private:
    PropertyName* reservedWordToPropertyName(TokenKind tt) const;

  public:
    PropertyName* currentName() const {
        if (isCurrentTokenType(TokenKind::Name))
            return currentToken().name();

        MOZ_ASSERT(TokenKindIsPossibleIdentifierName(currentToken().type));
        return reservedWordToPropertyName(currentToken().type);
    }

    bool currentNameHasEscapes() const {
        if (isCurrentTokenType(TokenKind::Name)) {
            TokenPos pos = currentToken().pos;
            return (pos.end - pos.begin) != currentToken().name()->length();
        }

        MOZ_ASSERT(TokenKindIsPossibleIdentifierName(currentToken().type));
        return false;
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

  private:
    // This is private because it should only be called by the tokenizer while
    // tokenizing not by, for example, BytecodeEmitter.
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

  public:
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
            MOZ_ASSERT(next.type != TokenKind::Div,
                       "next token requires contextual specifier to be parsed unambiguously");

            // Do not update modifierException.
            return;
        }

        MOZ_ASSERT(next.modifierException == NoException);
        switch (modifierException) {
          case NoneIsOperand:
            MOZ_ASSERT(next.modifier == Operand);
            MOZ_ASSERT(next.type != TokenKind::Div,
                       "next token requires contextual specifier to be parsed unambiguously");
            break;
          case OperandIsNone:
            MOZ_ASSERT(next.modifier == None);
            MOZ_ASSERT(next.type != TokenKind::Div && next.type != TokenKind::RegExp,
                       "next token requires contextual specifier to be parsed unambiguously");
            break;
          default:
            MOZ_CRASH("unexpected modifier exception");
        }
        tokens[nextCursor()].modifierException = modifierException;
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

    // This class maps a sourceUnits offset (which is 0-indexed) to a line
    // number (which is 1-indexed) and a column index (which is 0-indexed).
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

    JSContext* context() const {
        return cx;
    }

    /**
     * Fill in |err|, excepting line-of-context-related fields.  If the token
     * stream has location information, use that and return true.  If it does
     * not, use the caller's location information and return false.
     */
    bool fillExcludingContext(ErrorMetadata* err, uint32_t offset);

    MOZ_ALWAYS_INLINE void updateFlagsForEOL() {
        flags.isDirtyLine = false;
    }

  private:
    MOZ_MUST_USE MOZ_ALWAYS_INLINE bool internalUpdateLineInfoForEOL(uint32_t lineStartOffset);

    void undoInternalUpdateLineInfoForEOL();

  public:
    const Token& nextToken() const {
        MOZ_ASSERT(hasLookahead());
        return tokens[nextCursor()];
    }

    bool hasLookahead() const { return lookahead > 0; }

    void advanceCursor() {
        cursor_ = (cursor_ + 1) & ntokensMask;
    }

    void retractCursor() {
        cursor_ = (cursor_ - 1) & ntokensMask;
    }

    Token* allocateToken() {
        advanceCursor();

        Token* tp = &tokens[cursor()];
        MOZ_MAKE_MEM_UNDEFINED(tp, sizeof(*tp));

        return tp;
    }

    // Push the last scanned token back into the stream.
    void ungetToken() {
        MOZ_ASSERT(lookahead < maxLookahead);
        lookahead++;
        retractCursor();
    }

  public:
    MOZ_MUST_USE bool compileWarning(ErrorMetadata&& metadata, UniquePtr<JSErrorNotes> notes,
                                     unsigned flags, unsigned errorNumber, va_list args);

    // Compute error metadata for an error at no offset.
    void computeErrorMetadataNoOffset(ErrorMetadata* err);

    // ErrorReporter API Helpers

    void lineAndColumnAt(size_t offset, uint32_t *line, uint32_t *column) const;

    // This is just straight up duplicated from TokenStreamSpecific's inheritance of
    // ErrorReporter's reportErrorNoOffset. varargs delenda est.
    void reportErrorNoOffset(unsigned errorNumber, ...);
    void reportErrorNoOffsetVA(unsigned errorNumber, va_list args);

    const JS::ReadOnlyCompileOptions& options() const {
        return options_;
    }

    const char* getFilename() const {
        return filename_;
    }

  protected:
    // Options used for parsing/tokenizing.
    const ReadOnlyCompileOptions& options_;

    Token               tokens[ntokens];    // circular token buffer
  private:
    unsigned            cursor_;            // index of last parsed token
  protected:
    unsigned            lookahead;          // count of lookahead tokens
    unsigned            lineno;             // current line number
    TokenStreamFlags    flags;              // flags -- see above
    size_t              linebase;           // start of current line
    size_t              prevLinebase;       // start of previous line;  size_t(-1) if on the first line
    const char*         filename_;          // input filename or null
    UniqueTwoByteChars  displayURL_;        // the user's requested source URL or null
    UniqueTwoByteChars  sourceMapURL_;      // source map's filename or null

    /**
     * An array storing whether a TokenKind observed while attempting to extend
     * a valid AssignmentExpression into an even longer AssignmentExpression
     * (e.g., extending '3' to '3 + 5') will terminate it without error.
     *
     * For example, ';' always ends an AssignmentExpression because it ends a
     * Statement or declaration.  '}' always ends an AssignmentExpression
     * because it terminates BlockStatement, FunctionBody, and embedded
     * expressions in TemplateLiterals.  Therefore both entries are set to true
     * in TokenStreamAnyChars construction.
     *
     * But e.g. '+' *could* extend an AssignmentExpression, so its entry here
     * is false.  Meanwhile 'this' can't extend an AssignmentExpression, but
     * it's only valid after a line break, so its entry here must be false.
     *
     * NOTE: This array could be static, but without C99's designated
     *       initializers it's easier zeroing here and setting the true entries
     *       in the constructor body.  (Having this per-instance might also aid
     *       locality.)  Don't worry!  Initialization time for each TokenStream
     *       is trivial.  See bug 639420.
     */
    bool isExprEnding[size_t(TokenKind::Limit)] = {}; // all-false initially

    JSContext* const    cx;
    bool                mutedErrors;
    StrictModeGetter*   strictModeGetter;  // used to test for strict mode
};

constexpr char16_t
CodeUnitValue(char16_t unit)
{
    return unit;
}

constexpr uint8_t
CodeUnitValue(mozilla::Utf8Unit unit)
{
    return unit.toUint8();
}

template<typename CharT>
class TokenStreamCharsBase;

// This is the low-level interface to the JS source code buffer.  It just gets
// raw Unicode code units -- 16-bit char16_t units of source text that are not
// (always) full code points, and 8-bit units of UTF-8 source text soon.
// TokenStreams functions are layered on top and do some extra stuff like
// converting all EOL sequences to '\n', tracking the line number, and setting
// |flags.isEOF|.  (The "raw" in "raw Unicode code units" refers to the lack of
// EOL sequence normalization.)
//
// buf[0..length-1] often represents a substring of some larger source,
// where we have only the substring in memory. The |startOffset| argument
// indicates the offset within this larger string at which our string
// begins, the offset of |buf[0]|.
template<typename CharT>
class SourceUnits
{
  public:
    SourceUnits(const CharT* buf, size_t length, size_t startOffset)
      : base_(buf),
        startOffset_(startOffset),
        limit_(buf + length),
        ptr(buf)
    { }

    bool atStart() const {
        MOZ_ASSERT(ptr, "shouldn't be using if poisoned");
        return ptr == base_;
    }

    bool atEnd() const {
        MOZ_ASSERT(ptr <= limit_, "shouldn't have overrun");
        return ptr >= limit_;
    }

    size_t remaining() const {
        MOZ_ASSERT(ptr, "can't get a count of remaining code units if poisoned");
        return mozilla::PointerRangeSize(ptr, limit_);
    }

    size_t startOffset() const {
        return startOffset_;
    }

    size_t offset() const {
        return startOffset_ + mozilla::PointerRangeSize(base_, ptr);
    }

    const CharT* codeUnitPtrAt(size_t offset) const {
        MOZ_ASSERT(startOffset_ <= offset);
        MOZ_ASSERT(offset - startOffset_ <= mozilla::PointerRangeSize(base_, limit_));
        return base_ + (offset - startOffset_);
    }

    const CharT* limit() const {
        return limit_;
    }

    CharT previousCodeUnit() {
        MOZ_ASSERT(ptr, "can't get previous code unit if poisoned");
        MOZ_ASSERT(!atStart(), "must have a previous code unit to get");
        return *(ptr - 1);
    }

    CharT getCodeUnit() {
        return *ptr++;      // this will nullptr-crash if poisoned
    }

    CharT peekCodeUnit() const {
        return *ptr;        // this will nullptr-crash if poisoned
    }

    /** Match |n| hexadecimal digits and store their value in |*out|. */
    bool matchHexDigits(uint8_t n, char16_t* out) {
        MOZ_ASSERT(ptr, "shouldn't peek into poisoned SourceUnits");
        MOZ_ASSERT(n <= 4, "hexdigit value can't overflow char16_t");
        if (n > remaining())
            return false;

        char16_t v = 0;
        for (uint8_t i = 0; i < n; i++) {
            auto unit = CodeUnitValue(ptr[i]);
            if (!JS7_ISHEX(unit))
                return false;

            v = (v << 4) | JS7_UNHEX(unit);
        }

        *out = v;
        ptr += n;
        return true;
    }

    bool matchCodeUnits(const char* chars, uint8_t length) {
        MOZ_ASSERT(ptr, "shouldn't match into poisoned SourceUnits");
        if (length > remaining())
            return false;

        const CharT* start = ptr;
        const CharT* end = ptr + length;
        while (ptr < end) {
            if (*ptr++ != CharT(*chars++)) {
                ptr = start;
                return false;
            }
        }

        return true;
    }

    void skipCodeUnits(uint32_t n) {
        MOZ_ASSERT(ptr, "shouldn't use poisoned SourceUnits");
        MOZ_ASSERT(n <= remaining(),
                   "shouldn't skip beyond end of SourceUnits");
        ptr += n;
    }

    void unskipCodeUnits(uint32_t n) {
        MOZ_ASSERT(ptr, "shouldn't use poisoned SourceUnits");
        MOZ_ASSERT(n <= mozilla::PointerRangeSize(base_, ptr),
                   "shouldn't unskip beyond start of SourceUnits");
        ptr -= n;
    }

  private:
    friend class TokenStreamCharsBase<CharT>;

    bool internalMatchCodeUnit(CharT c) {
        MOZ_ASSERT(ptr, "shouldn't use poisoned SourceUnits");
        if (MOZ_LIKELY(!atEnd()) && *ptr == c) {
            ptr++;
            return true;
        }
        return false;
    }

  public:
    void consumeKnownCodeUnit(CharT c) {
        MOZ_ASSERT(ptr, "shouldn't use poisoned SourceUnits");
        MOZ_ASSERT(*ptr == c, "consuming the wrong code unit");
        ptr++;
    }

    /**
     * Unget the '\n' (CR) that precedes a '\n' (LF), when ungetting a line
     * terminator that's a full "\r\n" sequence.  If the prior code unit isn't
     * '\r', do nothing.
     */
    void ungetOptionalCRBeforeLF() {
        MOZ_ASSERT(ptr, "shouldn't unget a '\\r' from poisoned SourceUnits");
        MOZ_ASSERT(*ptr == CharT('\n'),
                   "function should only be called when a '\\n' was just "
                   "ungotten, and any '\\r' preceding it must also be "
                   "ungotten");
        if (*(ptr - 1) == CharT('\r'))
            ptr--;
    }

    /** Unget U+2028 LINE SEPARATOR or U+2029 PARAGRAPH SEPARATOR. */
    inline void ungetLineOrParagraphSeparator();

    void ungetCodeUnit() {
        MOZ_ASSERT(!atStart(), "can't unget if currently at start");
        MOZ_ASSERT(ptr);     // make sure it hasn't been poisoned
        ptr--;
    }

    const CharT* addressOfNextCodeUnit(bool allowPoisoned = false) const {
        MOZ_ASSERT_IF(!allowPoisoned, ptr);     // make sure it hasn't been poisoned
        return ptr;
    }

    // Use this with caution!
    void setAddressOfNextCodeUnit(const CharT* a, bool allowPoisoned = false) {
        MOZ_ASSERT_IF(!allowPoisoned, a);
        ptr = a;
    }

    // Poison the SourceUnits so they can't be accessed again.
    void poisonInDebug() {
#ifdef DEBUG
        ptr = nullptr;
#endif
    }

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
    /** Base of buffer. */
    const CharT* base_;

    /** Offset of base_[0]. */
    uint32_t startOffset_;

    /** Limit for quick bounds check. */
    const CharT* limit_;

    /** Next char to get. */
    const CharT* ptr;
};

template<>
inline void
SourceUnits<char16_t>::ungetLineOrParagraphSeparator()
{
#ifdef DEBUG
    char16_t prev = previousCodeUnit();
#endif
    MOZ_ASSERT(prev == unicode::LINE_SEPARATOR || prev == unicode::PARA_SEPARATOR);

    ungetCodeUnit();
}

template<>
inline void
SourceUnits<mozilla::Utf8Unit>::ungetLineOrParagraphSeparator()
{
    unskipCodeUnits(3);

    MOZ_ASSERT(ptr[0].toUint8() == 0xE2);
    MOZ_ASSERT(ptr[1].toUint8() == 0x80);

#ifdef DEBUG
    uint8_t last = ptr[2].toUint8();
#endif
    MOZ_ASSERT(last == 0xA8 || last == 0xA9);
}

class TokenStreamCharsShared
{
    // Using char16_t (not CharT) is a simplifying decision that hopefully
    // eliminates the need for a UTF-8 regular expression parser and makes
    // |copyCharBufferTo| markedly simpler.
    using CharBuffer = Vector<char16_t, 32>;

  protected:
    /**
     * Buffer transiently used to store sequences of identifier or string code
     * points when such can't be directly processed from the original source
     * text (e.g. because it contains escapes).
     */
    CharBuffer charBuffer;

  protected:
    explicit TokenStreamCharsShared(JSContext* cx)
      : charBuffer(cx)
    {}

    MOZ_MUST_USE bool appendCodePointToCharBuffer(uint32_t codePoint);

    MOZ_MUST_USE bool copyCharBufferTo(JSContext* cx,
                                       UniquePtr<char16_t[], JS::FreePolicy>* destination);

    /**
     * Determine whether a code unit constitutes a complete ASCII code point.
     * (The code point's exact value might not be used, however, if subsequent
     * code observes that |unit| is part of a LineTerminatorSequence.)
     */
    static constexpr MOZ_ALWAYS_INLINE MOZ_MUST_USE bool isAsciiCodePoint(int32_t unit) {
        return mozilla::IsAscii(unit);
    }

    JSAtom* drainCharBufferIntoAtom(JSContext* cx) {
        JSAtom* atom = AtomizeChars(cx, charBuffer.begin(), charBuffer.length());
        charBuffer.clear();
        return atom;
    }

  public:
    CharBuffer& getCharBuffer() { return charBuffer; }
};

template<typename CharT>
class TokenStreamCharsBase
  : public TokenStreamCharsShared
{
  protected:
    TokenStreamCharsBase(JSContext* cx, const CharT* chars, size_t length, size_t startOffset);

    /**
     * Convert a non-EOF code unit returned by |getCodeUnit()| or
     * |peekCodeUnit()| to a CharT code unit.
     */
    inline CharT toCharT(int32_t codeUnitValue);

    void ungetCodeUnit(int32_t c) {
        if (c == EOF)
            return;

        sourceUnits.ungetCodeUnit();
    }

    static MOZ_ALWAYS_INLINE JSAtom*
    atomizeSourceChars(JSContext* cx, const CharT* chars, size_t length);

    using SourceUnits = frontend::SourceUnits<CharT>;

    /**
     * Try to match a non-LineTerminator ASCII code point.  Return true iff it
     * was matched.
     */
    bool matchCodeUnit(char expect) {
        MOZ_ASSERT(mozilla::IsAscii(expect));
        MOZ_ASSERT(expect != '\r');
        MOZ_ASSERT(expect != '\n');
        return this->sourceUnits.internalMatchCodeUnit(CharT(expect));
    }

    /**
     * Try to match an ASCII LineTerminator code point.  Return true iff it was
     * matched.
     */
    bool matchLineTerminator(char expect) {
        MOZ_ASSERT(expect == '\r' || expect == '\n');
        return this->sourceUnits.internalMatchCodeUnit(CharT(expect));
    }

    template<typename T> bool matchCodeUnit(T) = delete;
    template<typename T> bool matchLineTerminator(T) = delete;

    int32_t peekCodeUnit() {
        return MOZ_LIKELY(!sourceUnits.atEnd()) ? CodeUnitValue(sourceUnits.peekCodeUnit()) : EOF;
    }

    /** Consume a known, non-EOF code unit. */
    inline void consumeKnownCodeUnit(int32_t unit);

    // Forbid accidental calls to consumeKnownCodeUnit *not* with the single
    // unit-or-EOF type.  CharT should use SourceUnits::consumeKnownCodeUnit;
    // CodeUnitValue() results should go through toCharT(), or better yet just
    // use the original CharT.
    template<typename T> inline void consumeKnownCodeUnit(T) = delete;

    /**
     * Accumulate the provided range of already-validated (i.e. valid UTF-8, or
     * anything if CharT is char16_t because JS permits lone and mispaired
     * surrogates) raw template literal text (i.e. containing no escapes or
     * substitutions) into |charBuffer|.
     */
    MOZ_MUST_USE bool fillCharBufferWithTemplateStringContents(const CharT* cur, const CharT* end);

  protected:
    /** Code units in the source code being tokenized. */
    SourceUnits sourceUnits;
};

template<>
inline char16_t
TokenStreamCharsBase<char16_t>::toCharT(int32_t codeUnitValue)
{
    MOZ_ASSERT(codeUnitValue != EOF, "EOF is not a CharT");
    return mozilla::AssertedCast<char16_t>(codeUnitValue);
}

template<>
inline mozilla::Utf8Unit
TokenStreamCharsBase<mozilla::Utf8Unit>::toCharT(int32_t value)
{
    MOZ_ASSERT(value != EOF, "EOF is not a CharT");
    return mozilla::Utf8Unit(static_cast<unsigned char>(value));
}

template<typename CharT>
inline void
TokenStreamCharsBase<CharT>::consumeKnownCodeUnit(int32_t unit)
{
    sourceUnits.consumeKnownCodeUnit(toCharT(unit));
}

template<>
/* static */ MOZ_ALWAYS_INLINE JSAtom*
TokenStreamCharsBase<char16_t>::atomizeSourceChars(JSContext* cx, const char16_t* chars,
                                                   size_t length)
{
    return AtomizeChars(cx, chars, length);
}

template<typename CharT>
class SpecializedTokenStreamCharsBase;

template<>
class SpecializedTokenStreamCharsBase<char16_t>
  : public TokenStreamCharsBase<char16_t>
{
    using CharsBase = TokenStreamCharsBase<char16_t>;

  protected:
    using TokenStreamCharsShared::isAsciiCodePoint;
    // Deliberately don't |using| |sourceUnits| because of bug 1472569.  :-(

    using typename CharsBase::SourceUnits;

  protected:
    // These APIs are only usable by UTF-16-specific code.

    /**
     * Consume the rest of a single-line comment (but not the EOL/EOF that
     * terminates it) -- infallibly because no 16-bit code unit sequence in a
     * comment is an error.
     */
    void infallibleConsumeRestOfSingleLineComment();

    /**
     * Given |lead| already consumed, consume and return the code point encoded
     * starting from it.  Infallible because lone surrogates in JS encode a
     * "code point" of the same value.
     */
    char32_t infallibleGetNonAsciiCodePointDontNormalize(char16_t lead) {
        MOZ_ASSERT(!isAsciiCodePoint(lead));
        MOZ_ASSERT(this->sourceUnits.previousCodeUnit() == lead);

        // Handle single-unit code points and lone trailing surrogates.
        if (MOZ_LIKELY(!unicode::IsLeadSurrogate(lead)) ||
            // Or handle lead surrogates not paired with trailing surrogates.
            MOZ_UNLIKELY(this->sourceUnits.atEnd() ||
                         !unicode::IsTrailSurrogate(this->sourceUnits.peekCodeUnit())))
        {
            return lead;
        }

        // Otherwise it's a multi-unit code point.
        return unicode::UTF16Decode(lead, this->sourceUnits.getCodeUnit());
    }

  protected:
    // These APIs are in both SpecializedTokenStreamCharsBase specializations
    // and so are usable in subclasses no matter what CharT is.

    using CharsBase::CharsBase;
};

template<>
class SpecializedTokenStreamCharsBase<mozilla::Utf8Unit>
  : public TokenStreamCharsBase<mozilla::Utf8Unit>
{
    using CharsBase = TokenStreamCharsBase<mozilla::Utf8Unit>;

  protected:
    // Deliberately don't |using| |sourceUnits| because of bug 1472569.  :-(

  protected:
    // These APIs are only usable by UTF-8-specific code.

  protected:
    // These APIs are in both SpecializedTokenStreamCharsBase specializations
    // and so are usable in subclasses no matter what CharT is.

    using CharsBase::CharsBase;
};

/** A small class encapsulating computation of the start-offset of a Token. */
class TokenStart
{
    uint32_t startOffset_;

  public:
    /**
     * Compute a starting offset that is the current offset of |sourceUnits|,
     * offset by |adjust|.  (For example, |adjust| of -1 indicates the code
     * unit one backwards from |sourceUnits|'s current offset.)
     */
    template<class SourceUnits>
    TokenStart(const SourceUnits& sourceUnits, ptrdiff_t adjust)
      : startOffset_(sourceUnits.offset() + adjust)
    {}

    TokenStart(const TokenStart&) = default;

    uint32_t offset() const { return startOffset_; }
};

template<typename CharT, class AnyCharsAccess>
class GeneralTokenStreamChars
  : public SpecializedTokenStreamCharsBase<CharT>
{
    using CharsBase = TokenStreamCharsBase<CharT>;
    using SpecializedCharsBase = SpecializedTokenStreamCharsBase<CharT>;

    Token* newTokenInternal(TokenKind kind, TokenStart start, TokenKind* out);

    /**
     * Allocates a new Token from the given offset to the current offset,
     * ascribes it the given kind, and sets |*out| to that kind.
     */
    Token* newToken(TokenKind kind, TokenStart start, TokenStreamShared::Modifier modifier,
                    TokenKind* out)
    {
        Token* token = newTokenInternal(kind, start, out);

#ifdef DEBUG
        // Save the modifier used to get this token, so that if an ungetToken()
        // occurs and then the token is re-gotten (or peeked, etc.), we can
        // assert both gets used compatible modifiers.
        token->modifier = modifier;
        token->modifierException = TokenStreamShared::NoException;
#endif

        return token;
    }

    uint32_t matchUnicodeEscape(uint32_t* codePoint);
    uint32_t matchExtendedUnicodeEscape(uint32_t* codePoint);

  protected:
    using TokenStreamCharsShared::drainCharBufferIntoAtom;
    using CharsBase::fillCharBufferWithTemplateStringContents;

    using typename CharsBase::SourceUnits;

  protected:
    using SpecializedCharsBase::SpecializedCharsBase;

    TokenStreamAnyChars& anyCharsAccess() {
        return AnyCharsAccess::anyChars(this);
    }

    const TokenStreamAnyChars& anyCharsAccess() const {
        return AnyCharsAccess::anyChars(this);
    }

    using TokenStreamSpecific = frontend::TokenStreamSpecific<CharT, AnyCharsAccess>;

    TokenStreamSpecific* asSpecific() {
        static_assert(mozilla::IsBaseOf<GeneralTokenStreamChars, TokenStreamSpecific>::value,
                      "static_cast below presumes an inheritance relationship");

        return static_cast<TokenStreamSpecific*>(this);
    }

    void newSimpleToken(TokenKind kind, TokenStart start, TokenStreamShared::Modifier modifier,
                        TokenKind* out)
    {
        newToken(kind, start, modifier, out);
    }

    void newNumberToken(double dval, DecimalPoint decimalPoint, TokenStart start,
                        TokenStreamShared::Modifier modifier, TokenKind* out)
    {
        Token* token = newToken(TokenKind::Number, start, modifier, out);
        token->setNumber(dval, decimalPoint);
    }

    void newAtomToken(TokenKind kind, JSAtom* atom, TokenStart start,
                      TokenStreamShared::Modifier modifier, TokenKind* out)
    {
        MOZ_ASSERT(kind == TokenKind::String ||
                   kind == TokenKind::TemplateHead ||
                   kind == TokenKind::NoSubsTemplate);

        Token* token = newToken(kind, start, modifier, out);
        token->setAtom(atom);
    }

    void newNameToken(PropertyName* name, TokenStart start, TokenStreamShared::Modifier modifier,
                      TokenKind* out)
    {
        Token* token = newToken(TokenKind::Name, start, modifier, out);
        token->setName(name);
    }

    void newRegExpToken(RegExpFlag reflags, TokenStart start, TokenKind* out)
    {
        Token* token = newToken(TokenKind::RegExp, start, TokenStreamShared::Operand, out);
        token->setRegExpFlags(reflags);
    }

    MOZ_COLD bool badToken();

    /**
     * Get the next code unit -- the next numeric sub-unit of source text,
     * possibly smaller than a full code point -- without updating line/column
     * counters or consuming LineTerminatorSequences.
     *
     * Because of these limitations, only use this if (a) the resulting code
     * unit is guaranteed to be ungotten (by ungetCodeUnit()) if it's an EOL,
     * and (b) the line-related state (lineno, linebase) is not used before
     * it's ungotten.
     */
    int32_t getCodeUnit() {
        if (MOZ_LIKELY(!this->sourceUnits.atEnd()))
            return this->sourceUnits.getCodeUnit();

        anyCharsAccess().flags.isEOF = true;
        return EOF;
    }

    void ungetCodeUnit(int32_t c) {
        MOZ_ASSERT_IF(c == EOF, anyCharsAccess().flags.isEOF);

        CharsBase::ungetCodeUnit(c);
    }

    MOZ_MUST_USE MOZ_ALWAYS_INLINE bool updateLineInfoForEOL() {
        return anyCharsAccess().internalUpdateLineInfoForEOL(this->sourceUnits.offset());
    }

    uint32_t matchUnicodeEscapeIdStart(uint32_t* codePoint);
    bool matchUnicodeEscapeIdent(uint32_t* codePoint);

  public:
    JSAtom* getRawTemplateStringAtom() {
        TokenStreamAnyChars& anyChars = anyCharsAccess();

        MOZ_ASSERT(anyChars.currentToken().type == TokenKind::TemplateHead ||
                   anyChars.currentToken().type == TokenKind::NoSubsTemplate);
        const CharT* cur = this->sourceUnits.codeUnitPtrAt(anyChars.currentToken().pos.begin + 1);
        const CharT* end;
        if (anyChars.currentToken().type == TokenKind::TemplateHead) {
            // Of the form    |`...${|   or   |}...${|
            end = this->sourceUnits.codeUnitPtrAt(anyChars.currentToken().pos.end - 2);
        } else {
            // NO_SUBS_TEMPLATE is of the form   |`...`|   or   |}...`|
            end = this->sourceUnits.codeUnitPtrAt(anyChars.currentToken().pos.end - 1);
        }

        if (!fillCharBufferWithTemplateStringContents(cur, end))
            return nullptr;

        return drainCharBufferIntoAtom(anyChars.cx);
    }
};

template<typename CharT, class AnyCharsAccess> class TokenStreamChars;

template<class AnyCharsAccess>
class TokenStreamChars<char16_t, AnyCharsAccess>
  : public GeneralTokenStreamChars<char16_t, AnyCharsAccess>
{
  private:
    using CharsBase = TokenStreamCharsBase<char16_t>;
    using SpecializedCharsBase = SpecializedTokenStreamCharsBase<char16_t>;
    using GeneralCharsBase = GeneralTokenStreamChars<char16_t, AnyCharsAccess>;
    using Self = TokenStreamChars<char16_t, AnyCharsAccess>;

    using GeneralCharsBase::asSpecific;

    using typename GeneralCharsBase::TokenStreamSpecific;

  protected:
    using GeneralCharsBase::anyCharsAccess;
    using GeneralCharsBase::getCodeUnit;
    using SpecializedCharsBase::infallibleConsumeRestOfSingleLineComment;
    using SpecializedCharsBase::infallibleGetNonAsciiCodePointDontNormalize;
    using TokenStreamCharsShared::isAsciiCodePoint;
    using CharsBase::matchLineTerminator;
    // Deliberately don't |using| |sourceUnits| because of bug 1472569.  :-(
    using GeneralCharsBase::ungetCodeUnit;
    using GeneralCharsBase::updateLineInfoForEOL;

    using typename GeneralCharsBase::SourceUnits;

  protected:
    using GeneralCharsBase::GeneralCharsBase;

    /**
     * Given the non-ASCII |lead| code unit just consumed, consume and return a
     * complete non-ASCII code point.  Line/column updates are not performed,
     * and line breaks are returned as-is without normalization.
     */
    MOZ_MUST_USE bool getNonAsciiCodePointDontNormalize(char16_t lead, char32_t* codePoint) {
        // There are no encoding errors in 16-bit JS, so implement this so that
        // the compiler knows it, too.
        *codePoint = infallibleGetNonAsciiCodePointDontNormalize(lead);
        return true;
    }

    /**
     * Get the next code point, converting LineTerminatorSequences to '\n' and
     * updating internal line-counter state if needed.  Return true on success
     * and store the code point in |*c|.  Return false and leave |*c| undefined
     * on failure.
     */
    MOZ_MUST_USE bool getCodePoint(int32_t* cp);

    /**
     * Given a just-consumed ASCII code unit/point |lead|, consume a full code
     * point or LineTerminatorSequence (normalizing it to '\n') and store it in
     * |*codePoint|.  Return true on success, otherwise return false and leave
     * |*codePoint| undefined on failure.
     *
     * If a LineTerminatorSequence was consumed, also update line/column info.
     *
     * This may change the current |sourceUnits| offset.
     */
    MOZ_MUST_USE bool getFullAsciiCodePoint(int32_t lead, int32_t* codePoint) {
        MOZ_ASSERT(isAsciiCodePoint(lead),
                   "non-ASCII code units must be handled separately");
        // NOTE: |this->|-qualify to avoid a gcc bug: see bug 1472569.
        MOZ_ASSERT(lead == this->sourceUnits.previousCodeUnit(),
                   "getFullAsciiCodePoint called incorrectly");

        if (MOZ_UNLIKELY(lead == '\r')) {
            matchLineTerminator('\n');
        } else if (MOZ_LIKELY(lead != '\n')) {
            *codePoint = lead;
            return true;
        }

        *codePoint = '\n';
        bool ok = updateLineInfoForEOL();
        if (!ok) {
#ifdef DEBUG
            *codePoint = EOF; // sentinel value to hopefully cause errors
#endif
            MOZ_MAKE_MEM_UNDEFINED(codePoint, sizeof(*codePoint));
        }
        return ok;
    }

    /**
     * Given a just-consumed non-ASCII code unit |lead| (which may also be a
     * full code point, for UTF-16), consume a full code point or
     * LineTerminatorSequence (normalizing it to '\n') and store it in
     * |*codePoint|.  Return true on success, otherwise return false and leave
     * |*codePoint| undefined on failure.
     *
     * If a LineTerminatorSequence was consumed, also update line/column info.
     *
     * This may change the current |sourceUnits| offset.
     */
    MOZ_MUST_USE bool getNonAsciiCodePoint(int32_t lead, int32_t* codePoint);

    /**
     * Unget a full code point (ASCII or not) without altering line/column
     * state.  If line/column state must be updated, this must happen manually.
     * This method ungets a single code point, not a LineTerminatorSequence
     * that is multiple code points.  (Generally you shouldn't be in a state
     * where you've just consumed "\r\n" and want to unget that full sequence.)
     *
     * This function ordinarily should be used to unget code points that have
     * been consumed *without* line/column state having been updated.
     */
    void ungetCodePointIgnoreEOL(uint32_t codePoint);

    /**
     * Unget an originally non-ASCII, normalized code point, including undoing
     * line/column updates that were performed for it.  Don't use this if the
     * code point was gotten *without* line/column state being updated!
     */
    void ungetNonAsciiNormalizedCodePoint(int32_t codePoint) {
        MOZ_ASSERT_IF(isAsciiCodePoint(codePoint),
                      codePoint == '\n');
        MOZ_ASSERT(codePoint != unicode::LINE_SEPARATOR,
                   "should not be ungetting un-normalized code points");
        MOZ_ASSERT(codePoint != unicode::PARA_SEPARATOR,
                   "should not be ungetting un-normalized code points");

        ungetCodePointIgnoreEOL(codePoint);
        if (codePoint == '\n')
            anyCharsAccess().undoInternalUpdateLineInfoForEOL();
    }

    /**
     * Consume code points til EOL/EOF following the start of a single-line
     * comment, without consuming the EOL/EOF.
     */
    MOZ_MUST_USE bool consumeRestOfSingleLineComment() {
        // This operation is infallible for UTF-16 -- and this implementation
        // approach lets the compiler boil away call-side fallibility handling.
        infallibleConsumeRestOfSingleLineComment();
        return true;
    }
};

// TokenStream is the lexical scanner for JavaScript source text.
//
// It takes a buffer of CharT code units (currently only char16_t encoding
// UTF-16, but we're adding either UTF-8 or Latin-1 single-byte text soon) and
// linearly scans it into |Token|s.
//
// Internally the class uses a four element circular buffer |tokens| of
// |Token|s. As an index for |tokens|, the member |cursor_| points to the
// current token. Calls to getToken() increase |cursor_| by one and return the
// new current token. If a TokenStream was just created, the current token is
// uninitialized. It's therefore important that one of the first four member
// functions listed below is called first. The circular buffer lets us go back
// up to two tokens from the last scanned token. Internally, the relative
// number of backward steps that were taken (via ungetToken()) after the last
// token was scanned is stored in |lookahead|.
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
// The method seek() allows rescanning from a previously visited location of
// the buffer, initially computed by constructing a Position local variable.
//
template<typename CharT, class AnyCharsAccess>
class MOZ_STACK_CLASS TokenStreamSpecific
  : public TokenStreamChars<CharT, AnyCharsAccess>,
    public TokenStreamShared,
    public ErrorReporter
{
  public:
    using CharsBase = TokenStreamCharsBase<CharT>;
    using SpecializedCharsBase = SpecializedTokenStreamCharsBase<CharT>;
    using GeneralCharsBase = GeneralTokenStreamChars<CharT, AnyCharsAccess>;
    using SpecializedChars = TokenStreamChars<CharT, AnyCharsAccess>;

    using Position = TokenStreamPosition<CharT>;

    // Anything inherited through a base class whose type depends upon this
    // class's template parameters can only be accessed through a dependent
    // name: prefixed with |this|, by explicit qualification, and so on.  (This
    // is so that references to inherited fields are statically distinguishable
    // from references to names outside of the class.)  This is tedious and
    // onerous.
    //
    // As an alternative, we directly add every one of these functions to this
    // class, using explicit qualification to address the dependent-name
    // problem.  |this| or other qualification is no longer necessary -- at
    // cost of this ever-changing laundry list of |using|s.  So it goes.
  public:
    using GeneralCharsBase::anyCharsAccess;

  private:
    using typename CharsBase::SourceUnits;

  private:
    using TokenStreamCharsShared::appendCodePointToCharBuffer;
    using CharsBase::atomizeSourceChars;
    using GeneralCharsBase::badToken;
    // Deliberately don't |using| |charBuffer| because of bug 1472569.  :-(
    using CharsBase::consumeKnownCodeUnit;
    using SpecializedChars::consumeRestOfSingleLineComment;
    using TokenStreamCharsShared::copyCharBufferTo;
    using TokenStreamCharsShared::drainCharBufferIntoAtom;
    using CharsBase::fillCharBufferWithTemplateStringContents;
    using SpecializedChars::getCodePoint;
    using GeneralCharsBase::getCodeUnit;
    using SpecializedChars::getFullAsciiCodePoint;
    using SpecializedChars::getNonAsciiCodePoint;
    using SpecializedChars::getNonAsciiCodePointDontNormalize;
    using TokenStreamCharsShared::isAsciiCodePoint;
    using CharsBase::matchCodeUnit;
    using CharsBase::matchLineTerminator;
    using GeneralCharsBase::matchUnicodeEscapeIdent;
    using GeneralCharsBase::matchUnicodeEscapeIdStart;
    using GeneralCharsBase::newAtomToken;
    using GeneralCharsBase::newNameToken;
    using GeneralCharsBase::newNumberToken;
    using GeneralCharsBase::newRegExpToken;
    using GeneralCharsBase::newSimpleToken;
    using CharsBase::peekCodeUnit;
    // Deliberately don't |using| |sourceUnits| because of bug 1472569.  :-(
    using SpecializedChars::ungetCodePointIgnoreEOL;
    using GeneralCharsBase::ungetCodeUnit;
    using SpecializedChars::ungetNonAsciiNormalizedCodePoint;
    using GeneralCharsBase::updateLineInfoForEOL;

    template<typename CharU> friend class TokenStreamPosition;

  public:
    TokenStreamSpecific(JSContext* cx, const ReadOnlyCompileOptions& options,
                        const CharT* base, size_t length);

    // If there is an invalid escape in a template, report it and return false,
    // otherwise return true.
    bool checkForInvalidTemplateEscapeError() {
        if (anyCharsAccess().invalidTemplateEscapeType == InvalidEscapeType::None)
            return true;

        reportInvalidEscapeError(anyCharsAccess().invalidTemplateEscapeOffset,
                                 anyCharsAccess().invalidTemplateEscapeType);
        return false;
    }

    // ErrorReporter API.

    const JS::ReadOnlyCompileOptions& options() const final {
        return anyCharsAccess().options();
    }

    void
    lineAndColumnAt(size_t offset, uint32_t* line, uint32_t* column) const final {
        anyCharsAccess().lineAndColumnAt(offset, line, column);
    }

    void currentLineAndColumn(uint32_t* line, uint32_t* column) const final;

    bool isOnThisLine(size_t offset, uint32_t lineNum, bool *onThisLine) const final {
        return anyCharsAccess().srcCoords.isOnThisLine(offset, lineNum, onThisLine);
    }
    uint32_t lineAt(size_t offset) const final {
        return anyCharsAccess().srcCoords.lineNum(offset);
    }
    uint32_t columnAt(size_t offset) const final {
        return anyCharsAccess().srcCoords.columnIndex(offset);
    }

    bool hasTokenizationStarted() const final;

    void reportErrorNoOffsetVA(unsigned errorNumber, va_list args) final {
        anyCharsAccess().reportErrorNoOffsetVA(errorNumber, args);
    }

    const char* getFilename() const final {
        return anyCharsAccess().getFilename();
    }

    // TokenStream-specific error reporters.
    void reportError(unsigned errorNumber, ...);

    // Report the given error at the current offset.
    void error(unsigned errorNumber, ...);

    // Report the given error at the given offset.
    void errorAt(uint32_t offset, unsigned errorNumber, ...);
    void errorAtVA(uint32_t offset, unsigned errorNumber, va_list* args);

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

    MOZ_MUST_USE bool putIdentInCharBuffer(const CharT* identStart);

    /**
     * Tokenize a decimal number that begins at |numStart| into the provided
     * token.
     *
     * |unit| must be one of these values:
     *
     *   1. The first decimal digit in the integral part of a decimal number
     *      not starting with '0' or '.', e.g. '1' for "17", '3' for "3.14", or
     *      '8' for "8.675309e6".
     *
     *   In this case, the next |getCodeUnit()| must return the code unit after
     *   |unit| in the overall number.
     *
     *   2. The '.' in a "."/"0."-prefixed decimal number or the 'e'/'E' in a
     *      "0e"/"0E"-prefixed decimal number, e.g. ".17", "0.42", or "0.1e3".
     *
     *   In this case, the next |getCodeUnit()| must return the code unit
     *   *after* the first decimal digit *after* the '.'.  So the next code
     *   unit would be '7' in ".17", '2' in "0.42", 'e' in "0.4e+8", or '/' in
     *   "0.5/2" (three separate tokens).
     *
     *   3. The code unit after the '0' where "0" is the entire number token.
     *
     *   In this case, the next |getCodeUnit()| would return the code unit
     *   after |unit|, but this function will never perform such call.
     *
     *   4. (Non-strict mode code only)  The first '8' or '9' in a "noctal"
     *      number that begins with a '0' but contains a non-octal digit in its
     *      integer part so is interpreted as decimal, e.g. '9' in "09.28" or
     *      '8' in "0386" or '9' in "09+7" (three separate tokens").
     *
     *   In this case, the next |getCodeUnit()| returns the code unit after
     *   |unit|: '.', '6', or '+' in the examples above.
     *
     * This interface is super-hairy and horribly stateful.  Unfortunately, its
     * hair merely reflects the intricacy of ECMAScript numeric literal syntax.
     * And incredibly, it *improves* on the goto-based horror that predated it.
     */
    MOZ_MUST_USE bool decimalNumber(int32_t unit, TokenStart start, const CharT* numStart,
                                    Modifier modifier, TokenKind* out);

    /** Tokenize a regular expression literal beginning at |start|. */
    MOZ_MUST_USE bool regexpLiteral(TokenStart start, TokenKind* out);

  public:
    // Advance to the next token.  If the token stream encountered an error,
    // return false.  Otherwise return true and store the token kind in |*ttp|.
    MOZ_MUST_USE bool getToken(TokenKind* ttp, Modifier modifier = None) {
        // Check for a pushed-back token resulting from mismatching lookahead.
        TokenStreamAnyChars& anyChars = anyCharsAccess();
        if (anyChars.lookahead != 0) {
            MOZ_ASSERT(!anyChars.flags.hadError);
            anyChars.lookahead--;
            anyChars.advanceCursor();
            TokenKind tt = anyChars.currentToken().type;
            MOZ_ASSERT(tt != TokenKind::Eol);
            verifyConsistentModifier(modifier, anyChars.currentToken());
            *ttp = tt;
            return true;
        }

        return getTokenInternal(ttp, modifier);
    }

    MOZ_MUST_USE bool peekToken(TokenKind* ttp, Modifier modifier = None) {
        TokenStreamAnyChars& anyChars = anyCharsAccess();
        if (anyChars.lookahead > 0) {
            MOZ_ASSERT(!anyChars.flags.hadError);
            verifyConsistentModifier(modifier, anyChars.nextToken());
            *ttp = anyChars.nextToken().type;
            return true;
        }
        if (!getTokenInternal(ttp, modifier))
            return false;
        anyChars.ungetToken();
        return true;
    }

    MOZ_MUST_USE bool peekTokenPos(TokenPos* posp, Modifier modifier = None) {
        TokenStreamAnyChars& anyChars = anyCharsAccess();
        if (anyChars.lookahead == 0) {
            TokenKind tt;
            if (!getTokenInternal(&tt, modifier))
                return false;
            anyChars.ungetToken();
            MOZ_ASSERT(anyChars.hasLookahead());
        } else {
            MOZ_ASSERT(!anyChars.flags.hadError);
            verifyConsistentModifier(modifier, anyChars.nextToken());
        }
        *posp = anyChars.nextToken().pos;
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
    // return true and store Eol in |*ttp|.  In that case, no token with
    // Eol is actually created, just a Eol TokenKind is returned, and
    // currentToken() shouldn't be consulted.  (This is the only place Eol
    // is produced.)
    MOZ_ALWAYS_INLINE MOZ_MUST_USE bool
    peekTokenSameLine(TokenKind* ttp, Modifier modifier = None) {
        TokenStreamAnyChars& anyChars = anyCharsAccess();
        const Token& curr = anyChars.currentToken();

        // If lookahead != 0, we have scanned ahead at least one token, and
        // |lineno| is the line that the furthest-scanned token ends on.  If
        // it's the same as the line that the current token ends on, that's a
        // stronger condition than what we are looking for, and we don't need
        // to return Eol.
        if (anyChars.lookahead != 0) {
            bool onThisLine;
            if (!anyChars.srcCoords.isOnThisLine(curr.pos.end, anyChars.lineno, &onThisLine)) {
                reportError(JSMSG_OUT_OF_MEMORY);
                return false;
            }

            if (onThisLine) {
                MOZ_ASSERT(!anyChars.flags.hadError);
                verifyConsistentModifier(modifier, anyChars.nextToken());
                *ttp = anyChars.nextToken().type;
                return true;
            }
        }

        // The above check misses two cases where we don't have to return
        // Eol.
        // - The next token starts on the same line, but is a multi-line token.
        // - The next token starts on the same line, but lookahead==2 and there
        //   is a newline between the next token and the one after that.
        // The following test is somewhat expensive but gets these cases (and
        // all others) right.
        TokenKind tmp;
        if (!getToken(&tmp, modifier))
            return false;
        const Token& next = anyChars.currentToken();
        anyChars.ungetToken();

        const auto& srcCoords = anyChars.srcCoords;
        *ttp = srcCoords.lineNum(curr.pos.end) == srcCoords.lineNum(next.pos.begin)
             ? next.type
             : TokenKind::Eol;
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
            anyCharsAccess().ungetToken();
            *matchedp = false;
        }
        return true;
    }

    void consumeKnownToken(TokenKind tt, Modifier modifier = None) {
        bool matched;
        MOZ_ASSERT(anyCharsAccess().hasLookahead());
        MOZ_ALWAYS_TRUE(matchToken(&matched, tt, modifier));
        MOZ_ALWAYS_TRUE(matched);
    }

    MOZ_MUST_USE bool nextTokenEndsExpr(bool* endsExpr) {
        TokenKind tt;
        if (!peekToken(&tt))
            return false;

        *endsExpr = anyCharsAccess().isExprEnding[size_t(tt)];
        if (*endsExpr) {
            // If the next token ends an overall Expression, we'll parse this
            // Expression without ever invoking Parser::orExpr().  But we need
            // that function's side effect of adding this modifier exception,
            // so we have to do it manually here.
            anyCharsAccess().addModifierException(OperandIsNone);
        }
        return true;
    }

    MOZ_MUST_USE bool advance(size_t position);

    void seek(const Position& pos);
    MOZ_MUST_USE bool seek(const Position& pos, const TokenStreamAnyChars& other);

    const CharT* codeUnitPtrAt(size_t offset) const {
        return this->sourceUnits.codeUnitPtrAt(offset);
    }

    const CharT* rawLimit() const {
        return this->sourceUnits.limit();
    }

    MOZ_MUST_USE bool identifierName(TokenStart start, const CharT* identStart,
                                     IdentifierEscapes escaping, Modifier modifier,
                                     TokenKind* out);

    MOZ_MUST_USE bool getTokenInternal(TokenKind* const ttp, const Modifier modifier);

    MOZ_MUST_USE bool getStringOrTemplateToken(char untilChar, Modifier modifier, TokenKind* out);

    MOZ_MUST_USE bool getDirectives(bool isMultiline, bool shouldWarnDeprecated);
    MOZ_MUST_USE bool getDirective(bool isMultiline, bool shouldWarnDeprecated,
                                   const char* directive, uint8_t directiveLength,
                                   const char* errorMsgPragma,
                                   UniquePtr<char16_t[], JS::FreePolicy>* destination);
    MOZ_MUST_USE bool getDisplayURL(bool isMultiline, bool shouldWarnDeprecated);
    MOZ_MUST_USE bool getSourceMappingURL(bool isMultiline, bool shouldWarnDeprecated);
};

// It's preferable to define this in TokenStream.cpp, but its template-ness
// means we'd then have to *instantiate* this constructor for all possible
// (CharT, AnyCharsAccess) pairs -- and that gets super-messy as AnyCharsAccess
// *itself* is templated.  This symbol really isn't that huge compared to some
// defined inline in TokenStreamSpecific, so just rely on the linker commoning
// stuff up.
template<typename CharT>
template<class AnyCharsAccess>
inline
TokenStreamPosition<CharT>::TokenStreamPosition(AutoKeepAtoms& keepAtoms,
                                                TokenStreamSpecific<CharT, AnyCharsAccess>& tokenStream)
{
    TokenStreamAnyChars& anyChars = tokenStream.anyCharsAccess();

    buf = tokenStream.sourceUnits.addressOfNextCodeUnit(/* allowPoisoned = */ true);
    flags = anyChars.flags;
    lineno = anyChars.lineno;
    linebase = anyChars.linebase;
    prevLinebase = anyChars.prevLinebase;
    lookahead = anyChars.lookahead;
    currentToken = anyChars.currentToken();
    for (unsigned i = 0; i < anyChars.lookahead; i++)
        lookaheadTokens[i] = anyChars.tokens[anyChars.aheadCursor(1 + i)];
}

class TokenStreamAnyCharsAccess
{
  public:
    template<class TokenStreamSpecific>
    static inline TokenStreamAnyChars& anyChars(TokenStreamSpecific* tss);

    template<class TokenStreamSpecific>
    static inline const TokenStreamAnyChars& anyChars(const TokenStreamSpecific* tss);
};

class MOZ_STACK_CLASS TokenStream final
  : public TokenStreamAnyChars,
    public TokenStreamSpecific<char16_t, TokenStreamAnyCharsAccess>
{
    using CharT = char16_t;

  public:
    TokenStream(JSContext* cx, const ReadOnlyCompileOptions& options,
                const CharT* base, size_t length, StrictModeGetter* smg)
    : TokenStreamAnyChars(cx, options, smg),
      TokenStreamSpecific<CharT, TokenStreamAnyCharsAccess>(cx, options, base, length)
    {}
};

template<class TokenStreamSpecific>
/* static */ inline TokenStreamAnyChars&
TokenStreamAnyCharsAccess::anyChars(TokenStreamSpecific* tss)
{
    auto* ts = static_cast<TokenStream*>(tss);
    return *static_cast<TokenStreamAnyChars*>(ts);
}

template<class TokenStreamSpecific>
/* static */ inline const TokenStreamAnyChars&
TokenStreamAnyCharsAccess::anyChars(const TokenStreamSpecific* tss)
{
    const auto* ts = static_cast<const TokenStream*>(tss);
    return *static_cast<const TokenStreamAnyChars*>(ts);
}

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
