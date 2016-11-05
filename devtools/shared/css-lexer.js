/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A CSS Lexer.  This file is a bit unusual -- it is a more or less
// direct translation of layout/style/nsCSSScanner.cpp and
// layout/style/CSSLexer.cpp into JS.  This implements the
// CSSLexer.webidl interface, and the intent is to try to keep it in
// sync with changes to the platform CSS lexer.  Due to this goal,
// this file violates some naming conventions and consequently locally
// disables some eslint rules.

/* eslint-disable camelcase, no-inline-comments, mozilla/no-aArgs */
/* eslint-disable no-else-return */

"use strict";

// White space of any kind.  No value fields are used.  Note that
// comments do *not* count as white space; comments separate tokens
// but are not themselves tokens.
const eCSSToken_Whitespace = "whitespace";     //
// A comment.
const eCSSToken_Comment = "comment";        // /*...*/

// Identifier-like tokens.  mIdent is the text of the identifier.
// The difference between ID and Hash is: if the text after the #
// would have been a valid Ident if the # hadn't been there, the
// scanner produces an ID token.  Otherwise it produces a Hash token.
// (This distinction is required by css3-selectors.)
const eCSSToken_Ident = "ident";          // word
const eCSSToken_Function = "function";       // word(
const eCSSToken_AtKeyword = "at";      // @word
const eCSSToken_ID = "id";             // #word
const eCSSToken_Hash = "hash";           // #0word

// Numeric tokens.  mNumber is the floating-point value of the
// number, and mHasSign indicates whether there was an explicit sign
// (+ or -) in front of the number.  If mIntegerValid is true, the
// number had the lexical form of an integer, and mInteger is its
// integer value.  Lexically integer values outside the range of a
// 32-bit signed number are clamped to the maximum values; mNumber
// will indicate a 'truer' value in that case.  Percentage tokens
// are always considered not to be integers, even if their numeric
// value is integral (100% => mNumber = 1.0).  For Dimension
// tokens, mIdent holds the text of the unit.
const eCSSToken_Number = "number";         // 1 -5 +2e3 3.14159 7.297352e-3
const eCSSToken_Dimension = "dimension";      // 24px 8.5in
const eCSSToken_Percentage = "percentage";     // 85% 1280.4%

// String-like tokens.  In all cases, mIdent holds the text
// belonging to the string, and mSymbol holds the delimiter
// character, which may be ', ", or zero (only for unquoted URLs).
// Bad_String and Bad_URL tokens are emitted when the closing
// delimiter or parenthesis was missing.
const eCSSToken_String = "string";         // 'foo bar' "foo bar"
const eCSSToken_Bad_String = "bad_string";     // 'foo bar
const eCSSToken_URL = "url";            // url(foobar) url("foo bar")
const eCSSToken_Bad_URL = "bad_url";        // url(foo

// Any one-character symbol.  mSymbol holds the character.
const eCSSToken_Symbol = "symbol";         // . ; { } ! *

// Match operators.  These are single tokens rather than pairs of
// Symbol tokens because css3-selectors forbids the presence of
// comments between the two characters.  No value fields are used;
// the token type indicates which operator.
const eCSSToken_Includes = "includes";       // ~=
const eCSSToken_Dashmatch = "dashmatch";      // |=
const eCSSToken_Beginsmatch = "beginsmatch";    // ^=
const eCSSToken_Endsmatch = "endsmatch";      // $=
const eCSSToken_Containsmatch = "containsmatch";  // *=

// Unicode-range token: currently used only in @font-face.
// The lexical rule for this token includes several forms that are
// semantically invalid.  Therefore, mIdent always holds the
// complete original text of the token (so we can print it
// accurately in diagnostics), and mIntegerValid is true iff the
// token is semantically valid.  In that case, mInteger holds the
// lowest value included in the range, and mInteger2 holds the
// highest value included in the range.
const eCSSToken_URange = "urange";         // U+007e U+01?? U+2000-206F

// HTML comment delimiters, ignored as a unit when they appear at
// the top level of a style sheet, for compatibility with websites
// written for compatibility with pre-CSS browsers.  This token type
// subsumes the css2.1 CDO and CDC tokens, which are always treated
// the same by the parser.  mIdent holds the text of the token, for
// diagnostics.
const eCSSToken_HTMLComment = "htmlcomment";    // <!-- -->

const eEOFCharacters_None = 0x0000;

// to handle \<EOF> inside strings
const eEOFCharacters_DropBackslash = 0x0001;

// to handle \<EOF> outside strings
const eEOFCharacters_ReplacementChar = 0x0002;

// to close comments
const eEOFCharacters_Asterisk = 0x0004;
const eEOFCharacters_Slash = 0x0008;

// to close double-quoted strings
const eEOFCharacters_DoubleQuote = 0x0010;

// to close single-quoted strings
const eEOFCharacters_SingleQuote = 0x0020;

// to close URLs
const eEOFCharacters_CloseParen = 0x0040;

// Bridge the char/string divide.
const APOSTROPHE = "'".charCodeAt(0);
const ASTERISK = "*".charCodeAt(0);
const CARRIAGE_RETURN = "\r".charCodeAt(0);
const CIRCUMFLEX_ACCENT = "^".charCodeAt(0);
const COMMERCIAL_AT = "@".charCodeAt(0);
const DIGIT_NINE = "9".charCodeAt(0);
const DIGIT_ZERO = "0".charCodeAt(0);
const DOLLAR_SIGN = "$".charCodeAt(0);
const EQUALS_SIGN = "=".charCodeAt(0);
const EXCLAMATION_MARK = "!".charCodeAt(0);
const FULL_STOP = ".".charCodeAt(0);
const GREATER_THAN_SIGN = ">".charCodeAt(0);
const HYPHEN_MINUS = "-".charCodeAt(0);
const LATIN_CAPITAL_LETTER_E = "E".charCodeAt(0);
const LATIN_CAPITAL_LETTER_U = "U".charCodeAt(0);
const LATIN_SMALL_LETTER_E = "e".charCodeAt(0);
const LATIN_SMALL_LETTER_U = "u".charCodeAt(0);
const LEFT_PARENTHESIS = "(".charCodeAt(0);
const LESS_THAN_SIGN = "<".charCodeAt(0);
const LINE_FEED = "\n".charCodeAt(0);
const NUMBER_SIGN = "#".charCodeAt(0);
const PERCENT_SIGN = "%".charCodeAt(0);
const PLUS_SIGN = "+".charCodeAt(0);
const QUESTION_MARK = "?".charCodeAt(0);
const QUOTATION_MARK = "\"".charCodeAt(0);
const REVERSE_SOLIDUS = "\\".charCodeAt(0);
const RIGHT_PARENTHESIS = ")".charCodeAt(0);
const SOLIDUS = "/".charCodeAt(0);
const TILDE = "~".charCodeAt(0);
const VERTICAL_LINE = "|".charCodeAt(0);

const UCS2_REPLACEMENT_CHAR = 0xFFFD;

const kImpliedEOFCharacters = [
  UCS2_REPLACEMENT_CHAR,
  ASTERISK,
  SOLIDUS,
  QUOTATION_MARK,
  APOSTROPHE,
  RIGHT_PARENTHESIS,
  0
];

/**
 * Ensure that the character is valid.  If it is valid, return it;
 * otherwise, return the replacement character.
 *
 * @param {Number} c the character to check
 * @return {Number} the character or its replacement
 */
function ensureValidChar(c) {
  if (c >= 0x00110000 || (c & 0xFFF800) == 0xD800) {
    // Out of range or a surrogate.
    return UCS2_REPLACEMENT_CHAR;
  }
  return c;
}

/**
 * Turn a string into an array of character codes.
 *
 * @param {String} str the input string
 * @return {Array} an array of character codes, one per character in
 *         the input string.
 */
function stringToCodes(str) {
  return Array.prototype.map.call(str, (c) => c.charCodeAt(0));
}

const IS_HEX_DIGIT = 0x01;
const IS_IDSTART = 0x02;
const IS_IDCHAR = 0x04;
const IS_URL_CHAR = 0x08;
const IS_HSPACE = 0x10;
const IS_VSPACE = 0x20;
const IS_SPACE = IS_HSPACE | IS_VSPACE;
const IS_STRING = 0x40;

const H = IS_HSPACE;
const V = IS_VSPACE;
const I = IS_IDCHAR;
const J = IS_IDSTART;
const U = IS_URL_CHAR;
const S = IS_STRING;
const X = IS_HEX_DIGIT;

const SH = S | H;
const SU = S | U;
const SUI = S | U | I;
const SUIJ = S | U | I | J;
const SUIX = S | U | I | X;
const SUIJX = S | U | I | J | X;

/* eslint-disable indent, no-multi-spaces, comma-spacing, spaced-comment */
const gLexTable = [
// 00    01    02    03    04    05    06    07
    0,    S,    S,    S,    S,    S,    S,    S,
// 08   TAB    LF    0B    FF    CR    0E    0F
    S,   SH,    V,    S,    V,    V,    S,    S,
// 10    11    12    13    14    15    16    17
    S,    S,    S,    S,    S,    S,    S,    S,
// 18    19    1A    1B    1C    1D    1E    1F
    S,    S,    S,    S,    S,    S,    S,    S,
//SPC     !     "     #     $     %     &     '
   SH,   SU,    0,   SU,   SU,   SU,   SU,    0,
//  (     )     *     +     ,     -     .     /
    S,    S,   SU,   SU,   SU,  SUI,   SU,   SU,
//  0     1     2     3     4     5     6     7
 SUIX, SUIX, SUIX, SUIX, SUIX, SUIX, SUIX, SUIX,
//  8     9     :     ;     <     =     >     ?
 SUIX, SUIX,   SU,   SU,   SU,   SU,   SU,   SU,
//  @     A     B     C     D     E     F     G
   SU,SUIJX,SUIJX,SUIJX,SUIJX,SUIJX,SUIJX, SUIJ,
//  H     I     J     K     L     M     N     O
 SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ,
//  P     Q     R     S     T     U     V     W
 SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ,
//  X     Y     Z     [     \     ]     ^     _
 SUIJ, SUIJ, SUIJ,   SU,    J,   SU,   SU, SUIJ,
//  `     a     b     c     d     e     f     g
   SU,SUIJX,SUIJX,SUIJX,SUIJX,SUIJX,SUIJX, SUIJ,
//  h     i     j     k     l     m     n     o
 SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ,
//  p     q     r     s     t     u     v     w
 SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ, SUIJ,
//  x     y     z     {     |     }     ~    7F
 SUIJ, SUIJ, SUIJ,   SU,   SU,   SU,   SU,    S,
];
/* eslint-enable indent, no-multi-spaces, comma-spacing, spaced-comment */

/**
 * True if 'ch' is in character class 'cls', which should be one of
 * the constants above or some combination of them.  All characters
 * above U+007F are considered to be in 'cls'.  EOF is never in 'cls'.
 */
function IsOpenCharClass(ch, cls) {
  return ch >= 0 && (ch >= 128 || (gLexTable[ch] & cls) != 0);
}

/**
 * True if 'ch' is in character class 'cls', which should be one of
 * the constants above or some combination of them.  No characters
 * above U+007F are considered to be in 'cls'. EOF is never in 'cls'.
 */
function IsClosedCharClass(ch, cls) {
  return ch >= 0 && ch < 128 && (gLexTable[ch] & cls) != 0;
}

/**
 * True if 'ch' is CSS whitespace, i.e. any of the ASCII characters
 * TAB, LF, FF, CR, or SPC.
 */
function IsWhitespace(ch) {
  return IsClosedCharClass(ch, IS_SPACE);
}

/**
 * True if 'ch' is horizontal whitespace, i.e. TAB or SPC.
 */
function IsHorzSpace(ch) {
  return IsClosedCharClass(ch, IS_HSPACE);
}

/**
 * True if 'ch' is vertical whitespace, i.e. LF, FF, or CR.  Vertical
 * whitespace requires special handling when consumed, see AdvanceLine.
 */
function IsVertSpace(ch) {
  return IsClosedCharClass(ch, IS_VSPACE);
}

/**
 * True if 'ch' is a character that can appear in the middle of an identifier.
 * This includes U+0000 since it is handled as U+FFFD, but for purposes of
 * GatherText it should not be included in IsOpenCharClass.
 */
function IsIdentChar(ch) {
  return IsOpenCharClass(ch, IS_IDCHAR) || ch == 0;
}

/**
 * True if 'ch' is a character that by itself begins an identifier.
 * This includes U+0000 since it is handled as U+FFFD, but for purposes of
 * GatherText it should not be included in IsOpenCharClass.
 * (This is a subset of IsIdentChar.)
 */
function IsIdentStart(ch) {
  return IsOpenCharClass(ch, IS_IDSTART) || ch == 0;
}

/**
 * True if the two-character sequence aFirstChar+aSecondChar begins an
 * identifier.
 */
function StartsIdent(aFirstChar, aSecondChar) {
  return IsIdentStart(aFirstChar) ||
    (aFirstChar == HYPHEN_MINUS && (aSecondChar == HYPHEN_MINUS ||
                                    IsIdentStart(aSecondChar)));
}

/**
 * True if 'ch' is a decimal digit.
 */
function IsDigit(ch) {
  return (ch >= DIGIT_ZERO) && (ch <= DIGIT_NINE);
}

/**
 * True if 'ch' is a hexadecimal digit.
 */
function IsHexDigit(ch) {
  return IsClosedCharClass(ch, IS_HEX_DIGIT);
}

/**
 * Assuming that 'ch' is a decimal digit, return its numeric value.
 */
function DecimalDigitValue(ch) {
  return ch - DIGIT_ZERO;
}

/**
 * Assuming that 'ch' is a hexadecimal digit, return its numeric value.
 */
function HexDigitValue(ch) {
  if (IsDigit(ch)) {
    return DecimalDigitValue(ch);
  } else {
    // Note: c&7 just keeps the low three bits which causes
    // upper and lower case alphabetics to both yield their
    // "relative to 10" value for computing the hex value.
    return (ch & 0x7) + 9;
  }
}

/**
 * If 'ch' can be the first character of a two-character match operator
 * token, return the token type code for that token, otherwise return
 * eCSSToken_Symbol to indicate that it can't.
 */
function MatchOperatorType(ch) {
  switch (ch) {
    case TILDE: return eCSSToken_Includes;
    case VERTICAL_LINE: return eCSSToken_Dashmatch;
    case CIRCUMFLEX_ACCENT: return eCSSToken_Beginsmatch;
    case DOLLAR_SIGN: return eCSSToken_Endsmatch;
    case ASTERISK: return eCSSToken_Containsmatch;
    default: return eCSSToken_Symbol;
  }
}

function Scanner(buffer) {
  this.mBuffer = buffer || "";
  this.mOffset = 0;
  this.mCount = this.mBuffer.length;
  this.mLineNumber = 1;
  this.mLineOffset = 0;
  this.mTokenLineOffset = 0;
  this.mTokenOffset = 0;
  this.mTokenLineNumber = 1;
  this.mEOFCharacters = eEOFCharacters_None;
}

Scanner.prototype = {
  /**
   * @see CSSLexer.lineNumber
   */
  get lineNumber() {
    return this.mTokenLineNumber - 1;
  },

  /**
   * @see CSSLexer.columnNumber
   */
  get columnNumber() {
    return this.mTokenOffset - this.mTokenLineOffset;
  },

  /**
   * @see CSSLexer.performEOFFixup
   */
  performEOFFixup: function (aInputString, aPreserveBackslash) {
    let result = aInputString;

    let eofChars = this.mEOFCharacters;

    if (aPreserveBackslash &&
        (eofChars & (eEOFCharacters_DropBackslash |
                     eEOFCharacters_ReplacementChar)) != 0) {
      eofChars &= ~(eEOFCharacters_DropBackslash |
                    eEOFCharacters_ReplacementChar);
      result += "\\";
    }

    if ((eofChars & eEOFCharacters_DropBackslash) != 0 &&
        result.length > 0 && result.endsWith("\\")) {
      result = result.slice(0, -1);
    }

    let extra = [];
    this.AppendImpliedEOFCharacters(eofChars, extra);
    let asString = String.fromCharCode.apply(null, extra);

    return result + asString;
  },

  /**
   * @see CSSLexer.nextToken
   */
  nextToken: function () {
    let token = {};
    if (!this.Next(token)) {
      return null;
    }

    let resultToken = {};
    resultToken.tokenType = token.mType;
    resultToken.startOffset = this.mTokenOffset;
    resultToken.endOffset = this.mOffset;

    let constructText = () => {
      return String.fromCharCode.apply(null, token.mIdent);
    };

    switch (token.mType) {
      case eCSSToken_Whitespace:
        break;

      case eCSSToken_Ident:
      case eCSSToken_Function:
      case eCSSToken_AtKeyword:
      case eCSSToken_ID:
      case eCSSToken_Hash:
        resultToken.text = constructText();
        break;

      case eCSSToken_Dimension:
        resultToken.text = constructText();
        /* Fall through.  */
      case eCSSToken_Number:
      case eCSSToken_Percentage:
        resultToken.number = token.mNumber;
        resultToken.hasSign = token.mHasSign;
        resultToken.isInteger = token.mIntegerValid;
        break;

      case eCSSToken_String:
      case eCSSToken_Bad_String:
      case eCSSToken_URL:
      case eCSSToken_Bad_URL:
        resultToken.text = constructText();
        /* Don't bother emitting the delimiter, as it is readily extracted
           from the source string when needed.  */
        break;

      case eCSSToken_Symbol:
        resultToken.text = String.fromCharCode(token.mSymbol);
        break;

      case eCSSToken_Includes:
      case eCSSToken_Dashmatch:
      case eCSSToken_Beginsmatch:
      case eCSSToken_Endsmatch:
      case eCSSToken_Containsmatch:
      case eCSSToken_URange:
        break;

      case eCSSToken_Comment:
      case eCSSToken_HTMLComment:
        /* The comment text is easily extracted from the source string,
           and is rarely useful.  */
        break;
    }

    return resultToken;
  },

  /**
   * Return the raw UTF-16 code unit at position |this.mOffset + n| within
   * the read buffer.  If that is beyond the end of the buffer, returns
   * -1 to indicate end of input.
   */
  Peek: function (n = 0) {
    if (this.mOffset + n >= this.mCount) {
      return -1;
    }
    return this.mBuffer.charCodeAt(this.mOffset + n);
  },

  /**
   * Advance |this.mOffset| over |n| code units.  Advance(0) is a no-op.
   * If |n| is greater than the distance to end of input, will silently
   * stop at the end.  May not be used to advance over a line boundary;
   * AdvanceLine() must be used instead.
   */
  Advance: function (n = 1) {
    if (this.mOffset + n >= this.mCount || this.mOffset + n < this.mOffset) {
      this.mOffset = this.mCount;
    } else {
      this.mOffset += n;
    }
  },

  /**
   * Advance |this.mOffset| over a line boundary.
   */
  AdvanceLine: function () {
    // Advance over \r\n as a unit.
    if (this.mBuffer.charCodeAt(this.mOffset) == CARRIAGE_RETURN &&
        this.mOffset + 1 < this.mCount &&
        this.mBuffer.charCodeAt(this.mOffset + 1) == LINE_FEED) {
      this.mOffset += 2;
    } else {
      this.mOffset += 1;
    }
    // 0 is a magical line number meaning that we don't know (i.e., script)
    if (this.mLineNumber != 0) {
      this.mLineNumber++;
    }
    this.mLineOffset = this.mOffset;
  },

  /**
   * Skip over a sequence of whitespace characters (vertical or
   * horizontal) starting at the current read position.
   */
  SkipWhitespace: function () {
    for (;;) {
      let ch = this.Peek();
      if (!IsWhitespace(ch)) { // EOF counts as non-whitespace
        break;
      }
      if (IsVertSpace(ch)) {
        this.AdvanceLine();
      } else {
        this.Advance();
      }
    }
  },

  /**
   * Skip over one CSS comment starting at the current read position.
   */
  SkipComment: function () {
    this.Advance(2);
    for (;;) {
      let ch = this.Peek();
      if (ch < 0) {
        this.SetEOFCharacters(eEOFCharacters_Asterisk | eEOFCharacters_Slash);
        return;
      }
      if (ch == ASTERISK) {
        this.Advance();
        ch = this.Peek();
        if (ch < 0) {
          this.SetEOFCharacters(eEOFCharacters_Slash);
          return;
        }
        if (ch == SOLIDUS) {
          this.Advance();
          return;
        }
      } else if (IsVertSpace(ch)) {
        this.AdvanceLine();
      } else {
        this.Advance();
      }
    }
  },

  /**
   * If there is a valid escape sequence starting at the current read
   * position, consume it, decode it, append the result to |aOutput|,
   * and return true.  Otherwise, consume nothing, leave |aOutput|
   * unmodified, and return false.  If |aInString| is true, accept the
   * additional form of escape sequence allowed within string-like tokens.
   */
  GatherEscape: function (aOutput, aInString) {
    let ch = this.Peek(1);
    if (ch < 0) {
      // If we are in a string (or a url() containing a string), we want to drop
      // the backslash on the floor.  Otherwise, we want to treat it as a U+FFFD
      // character.
      this.Advance();
      if (aInString) {
        this.SetEOFCharacters(eEOFCharacters_DropBackslash);
      } else {
        aOutput.push(UCS2_REPLACEMENT_CHAR);
        this.SetEOFCharacters(eEOFCharacters_ReplacementChar);
      }
      return true;
    }
    if (IsVertSpace(ch)) {
      if (aInString) {
        // In strings (and in url() containing a string), escaped
        // newlines are completely removed, to allow splitting over
        // multiple lines.
        this.Advance();
        this.AdvanceLine();
        return true;
      }
      // Outside of strings, backslash followed by a newline is not an escape.
      return false;
    }

    if (!IsHexDigit(ch)) {
      // "Any character (except a hexadecimal digit, linefeed, carriage
      // return, or form feed) can be escaped with a backslash to remove
      // its special meaning." -- CSS2.1 section 4.1.3
      this.Advance(2);
      if (ch == 0) {
        aOutput.push(UCS2_REPLACEMENT_CHAR);
      } else {
        aOutput.push(ch);
      }
      return true;
    }

    // "[at most six hexadecimal digits following a backslash] stand
    // for the ISO 10646 character with that number, which must not be
    // zero. (It is undefined in CSS 2.1 what happens if a style sheet
    // does contain a character with Unicode codepoint zero.)"
    //   -- CSS2.1 section 4.1.3

    // At this point we know we have \ followed by at least one
    // hexadecimal digit, therefore the escape sequence is valid and we
    // can go ahead and consume the backslash.
    this.Advance();
    let val = 0;
    let i = 0;
    do {
      val = val * 16 + HexDigitValue(ch);
      i++;
      this.Advance();
      ch = this.Peek();
    } while (i < 6 && IsHexDigit(ch));

    // "Interpret the hex digits as a hexadecimal number. If this
    // number is zero, or is greater than the maximum allowed
    // codepoint, return U+FFFD REPLACEMENT CHARACTER" -- CSS Syntax
    // Level 3
    if (val == 0) {
      aOutput.push(UCS2_REPLACEMENT_CHAR);
    } else {
      aOutput.push(ensureValidChar(val));
    }

    // Consume exactly one whitespace character after a
    // hexadecimal escape sequence.
    if (IsVertSpace(ch)) {
      this.AdvanceLine();
    } else if (IsHorzSpace(ch)) {
      this.Advance();
    }
    return true;
  },

  /**
   * Consume a run of "text" beginning with the current read position,
   * consisting of characters in the class |aClass| (which must be a
   * suitable argument to IsOpenCharClass) plus escape sequences.
   * Append the text to |aText|, after decoding escape sequences.
   *
   * Returns true if at least one character was appended to |aText|,
   * false otherwise.
   */
  GatherText: function (aClass, aText) {
    let start = this.mOffset;
    let inString = aClass == IS_STRING;

    for (;;) {
      // Consume runs of unescaped characters in one go.
      let n = this.mOffset;
      while (n < this.mCount && IsOpenCharClass(this.mBuffer.charCodeAt(n),
                                                aClass)) {
        n++;
      }
      if (n > this.mOffset) {
        let substr = this.mBuffer.slice(this.mOffset, n);
        Array.prototype.push.apply(aText, stringToCodes(substr));
        this.mOffset = n;
      }
      if (n == this.mCount) {
        break;
      }

      let ch = this.Peek();
      if (ch == 0) {
        this.Advance();
        aText.push(UCS2_REPLACEMENT_CHAR);
        continue;
      }

      if (ch != REVERSE_SOLIDUS) {
        break;
      }
      if (!this.GatherEscape(aText, inString)) {
        break;
      }
    }

    return this.mOffset > start;
  },

  /**
   * Scan an Ident token.  This also handles Function and URL tokens,
   * both of which begin indistinguishably from an identifier.  It can
   * produce a Symbol token when an apparent identifier actually led
   * into an invalid escape sequence.
   */
  ScanIdent: function (aToken) {
    if (!this.GatherText(IS_IDCHAR, aToken.mIdent)) {
      aToken.mSymbol = this.Peek();
      this.Advance();
      return true;
    }

    if (this.Peek() != LEFT_PARENTHESIS) {
      aToken.mType = eCSSToken_Ident;
      return true;
    }

    this.Advance();
    aToken.mType = eCSSToken_Function;

    let asString = String.fromCharCode.apply(null, aToken.mIdent);
    if (asString.toLowerCase() === "url") {
      this.NextURL(aToken);
    }
    return true;
  },

  /**
   * Scan an AtKeyword token.  Also handles production of Symbol when
   * an '@' is not followed by an identifier.
   */
  ScanAtKeyword: function (aToken) {
    // Fall back for when '@' isn't followed by an identifier.
    aToken.mSymbol = COMMERCIAL_AT;
    this.Advance();

    let ch = this.Peek();
    if (StartsIdent(ch, this.Peek(1))) {
      if (this.GatherText(IS_IDCHAR, aToken.mIdent)) {
        aToken.mType = eCSSToken_AtKeyword;
      }
    }
    return true;
  },

  /**
   * Scan a Hash token.  Handles the distinction between eCSSToken_ID
   * and eCSSToken_Hash, and handles production of Symbol when a '#'
   * is not followed by identifier characters.
   */
  ScanHash: function (aToken) {
    // Fall back for when '#' isn't followed by identifier characters.
    aToken.mSymbol = NUMBER_SIGN;
    this.Advance();

    let ch = this.Peek();
    if (IsIdentChar(ch) || ch == REVERSE_SOLIDUS) {
      let type =
          StartsIdent(ch, this.Peek(1)) ? eCSSToken_ID : eCSSToken_Hash;
      aToken.mIdent.length = 0;
      if (this.GatherText(IS_IDCHAR, aToken.mIdent)) {
        aToken.mType = type;
      }
    }

    return true;
  },

  /**
   * Scan a Number, Percentage, or Dimension token (all of which begin
   * like a Number).  Can produce a Symbol when a '.' is not followed by
   * digits, or when '+' or '-' are not followed by either a digit or a
   * '.' and then a digit.  Can also produce a HTMLComment when it
   * encounters '-->'.
   */
  ScanNumber: function (aToken) {
    let c = this.Peek();

    // Sign of the mantissa (-1 or 1).
    let sign = c == HYPHEN_MINUS ? -1 : 1;
    // Absolute value of the integer part of the mantissa.  This is a double so
    // we don't run into overflow issues for consumers that only care about our
    // floating-point value while still being able to express the full int32_t
    // range for consumers who want integers.
    let intPart = 0;
    // Fractional part of the mantissa.  This is a double so that when
    // we convert to float at the end we'll end up rounding to nearest
    // float instead of truncating down (as we would if fracPart were
    // a float and we just effectively lost the last several digits).
    let fracPart = 0;
    // Absolute value of the power of 10 that we should multiply by
    // (only relevant for numbers in scientific notation).  Has to be
    // a signed integer, because multiplication of signed by unsigned
    // converts the unsigned to signed, so if we plan to actually
    // multiply by expSign...
    let exponent = 0;
    // Sign of the exponent.
    let expSign = 1;

    aToken.mHasSign = (c == PLUS_SIGN || c == HYPHEN_MINUS);
    if (aToken.mHasSign) {
      this.Advance();
      c = this.Peek();
    }

    let gotDot = (c == FULL_STOP);

    if (!gotDot) {
      // Scan the integer part of the mantissa.
      do {
        intPart = 10 * intPart + DecimalDigitValue(c);
        this.Advance();
        c = this.Peek();
      } while (IsDigit(c));

      gotDot = (c == FULL_STOP) && IsDigit(this.Peek(1));
    }

    if (gotDot) {
      // Scan the fractional part of the mantissa.
      this.Advance();
      c = this.Peek();
      // Power of ten by which we need to divide our next digit
      let divisor = 10;
      do {
        fracPart += DecimalDigitValue(c) / divisor;
        divisor *= 10;
        this.Advance();
        c = this.Peek();
      } while (IsDigit(c));
    }

    let gotE = false;
    if (c == LATIN_SMALL_LETTER_E || c == LATIN_CAPITAL_LETTER_E) {
      let expSignChar = this.Peek(1);
      let nextChar = this.Peek(2);
      if (IsDigit(expSignChar) ||
          ((expSignChar == HYPHEN_MINUS || expSignChar == PLUS_SIGN) &&
           IsDigit(nextChar))) {
        gotE = true;
        if (expSignChar == HYPHEN_MINUS) {
          expSign = -1;
        }
        this.Advance(); // consumes the E
        if (expSignChar == HYPHEN_MINUS || expSignChar == PLUS_SIGN) {
          this.Advance();
          c = nextChar;
        } else {
          c = expSignChar;
        }
        do {
          exponent = 10 * exponent + DecimalDigitValue(c);
          this.Advance();
          c = this.Peek();
        } while (IsDigit(c));
      }
    }

    let type = eCSSToken_Number;

    // Set mIntegerValid for all cases (except %, below) because we need
    // it for the "2n" in :nth-child(2n).
    aToken.mIntegerValid = false;

    // Time to reassemble our number.
    // Do all the math in double precision so it's truncated only once.
    let value = sign * (intPart + fracPart);
    if (gotE) {
      // Explicitly cast expSign*exponent to double to avoid issues with
      // overloaded pow() on Windows.
      value *= Math.pow(10.0, expSign * exponent);
    } else if (!gotDot) {
      // Clamp values outside of integer range.
      if (sign > 0) {
        aToken.mInteger = Math.min(intPart, Number.MAX_SAFE_INTEGER);
      } else {
        aToken.mInteger = Math.max(-intPart, Number.MIN_SAFE_INTEGER);
      }
      aToken.mIntegerValid = true;
    }

    let ident = aToken.mIdent;

    // Check for Dimension and Percentage tokens.
    if (c >= 0) {
      if (StartsIdent(c, this.Peek(1))) {
        if (this.GatherText(IS_IDCHAR, ident)) {
          type = eCSSToken_Dimension;
        }
      } else if (c == PERCENT_SIGN) {
        this.Advance();
        type = eCSSToken_Percentage;
        value = value / 100.0;
        aToken.mIntegerValid = false;
      }
    }
    aToken.mNumber = value;
    aToken.mType = type;
    return true;
  },

  /**
   * Scan a string constant ('foo' or "foo").  Will always produce
   * either a String or a Bad_String token; the latter occurs when the
   * close quote is missing.  Always returns true (for convenience in Next()).
   */
  ScanString: function (aToken) {
    let aStop = this.Peek();
    aToken.mType = eCSSToken_String;
    aToken.mSymbol = aStop; // Remember how it's quoted.
    this.Advance();

    for (;;) {
      this.GatherText(IS_STRING, aToken.mIdent);

      let ch = this.Peek();
      if (ch == -1) {
        this.AddEOFCharacters(aStop == QUOTATION_MARK ?
                              eEOFCharacters_DoubleQuote :
                              eEOFCharacters_SingleQuote);
        break; // EOF ends a string token with no error.
      }
      if (ch == aStop) {
        this.Advance();
        break;
      }
      // Both " and ' are excluded from IS_STRING.
      if (ch == QUOTATION_MARK || ch == APOSTROPHE) {
        aToken.mIdent.push(ch);
        this.Advance();
        continue;
      }

      aToken.mType = eCSSToken_Bad_String;
      break;
    }
    return true;
  },

  /**
   * Scan a unicode-range token.  These match the regular expression
   *
   *     u\+[0-9a-f?]{1,6}(-[0-9a-f]{1,6})?
   *
   * However, some such tokens are "invalid".  There are three valid forms:
   *
   *     u+[0-9a-f]{x}              1 <= x <= 6
   *     u+[0-9a-f]{x}\?{y}         1 <= x+y <= 6
   *     u+[0-9a-f]{x}-[0-9a-f]{y}  1 <= x <= 6, 1 <= y <= 6
   *
   * All unicode-range tokens have their text recorded in mIdent; valid ones
   * are also decoded into mInteger and mInteger2, and mIntegerValid is set.
   * Note that this does not validate the numeric range, only the syntactic
   * form.
   */
  ScanURange: function (aResult) {
    let intro1 = this.Peek();
    let intro2 = this.Peek(1);
    let ch = this.Peek(2);

    aResult.mIdent.push(intro1);
    aResult.mIdent.push(intro2);
    this.Advance(2);

    let valid = true;
    let haveQues = false;
    let low = 0;
    let high = 0;
    let i = 0;

    do {
      aResult.mIdent.push(ch);
      if (IsHexDigit(ch)) {
        if (haveQues) {
          valid = false; // All question marks should be at the end.
        }
        low = low * 16 + HexDigitValue(ch);
        high = high * 16 + HexDigitValue(ch);
      } else {
        haveQues = true;
        low = low * 16 + 0x0;
        high = high * 16 + 0xF;
      }

      i++;
      this.Advance();
      ch = this.Peek();
    } while (i < 6 && (IsHexDigit(ch) || ch == QUESTION_MARK));

    if (ch == HYPHEN_MINUS && IsHexDigit(this.Peek(1))) {
      if (haveQues) {
        valid = false;
      }

      aResult.mIdent.push(ch);
      this.Advance();
      ch = this.Peek();
      high = 0;
      i = 0;
      do {
        aResult.mIdent.push(ch);
        high = high * 16 + HexDigitValue(ch);

        i++;
        this.Advance();
        ch = this.Peek();
      } while (i < 6 && IsHexDigit(ch));
    }

    aResult.mInteger = low;
    aResult.mInteger2 = high;
    aResult.mIntegerValid = valid;
    aResult.mType = eCSSToken_URange;
    return true;
  },

  SetEOFCharacters: function (aEOFCharacters) {
    this.mEOFCharacters = aEOFCharacters;
  },

  AddEOFCharacters: function (aEOFCharacters) {
    this.mEOFCharacters = this.mEOFCharacters | aEOFCharacters;
  },

  AppendImpliedEOFCharacters: function (aEOFCharacters, aResult) {
    // First, ignore eEOFCharacters_DropBackslash.
    let c = aEOFCharacters >> 1;

    // All of the remaining EOFCharacters bits represent appended characters,
    // and the bits are in the order that they need appending.
    for (let p of kImpliedEOFCharacters) {
      if (c & 1) {
        aResult.push(p);
      }
      c >>= 1;
    }
  },

  /**
   * Consume the part of an URL token after the initial 'url('.  Caller
   * is assumed to have consumed 'url(' already.  Will always produce
   * either an URL or a Bad_URL token.
   *
   * Exposed for use by nsCSSParser::ParseMozDocumentRule, which applies
   * the special lexical rules for URL tokens in a nonstandard context.
   */
  NextURL: function (aToken) {
    this.SkipWhitespace();

    // aToken.mIdent may be "url" at this point; clear that out
    aToken.mIdent.length = 0;

    let ch = this.Peek();
    // Do we have a string?
    if (ch == QUOTATION_MARK || ch == APOSTROPHE) {
      this.ScanString(aToken);
      if (aToken.mType == eCSSToken_Bad_String) {
        aToken.mType = eCSSToken_Bad_URL;
        return;
      }
    } else {
      // Otherwise, this is the start of a non-quoted url (which may be empty).
      aToken.mSymbol = 0;
      this.GatherText(IS_URL_CHAR, aToken.mIdent);
    }

    // Consume trailing whitespace and then look for a close parenthesis.
    this.SkipWhitespace();
    ch = this.Peek();
    // ch can be less than zero indicating EOF
    if (ch < 0 || ch == RIGHT_PARENTHESIS) {
      this.Advance();
      aToken.mType = eCSSToken_URL;
      if (ch < 0) {
        this.AddEOFCharacters(eEOFCharacters_CloseParen);
      }
    } else {
      aToken.mType = eCSSToken_Bad_URL;
    }
  },

  /**
   * Primary scanner entry point.  Consume one token and fill in
   * |aToken| accordingly.  Will skip over any number of comments first,
   * and will also skip over rather than return whitespace and comment
   * tokens, depending on the value of |aSkip|.
   *
   * Returns true if it successfully consumed a token, false if EOF has
   * been reached.  Will always advance the current read position by at
   * least one character unless called when already at EOF.
   */
  Next: function (aToken, aSkip) {
    let ch;

    // do this here so we don't have to do it in dozens of other places
    aToken.mIdent = [];
    aToken.mType = eCSSToken_Symbol;

    this.mTokenOffset = this.mOffset;
    this.mTokenLineOffset = this.mLineOffset;
    this.mTokenLineNumber = this.mLineNumber;

    ch = this.Peek();
    if (IsWhitespace(ch)) {
      this.SkipWhitespace();
      aToken.mType = eCSSToken_Whitespace;
      return true;
    }
    if (ch == SOLIDUS && // !IsSVGMode() &&
        this.Peek(1) == ASTERISK) {
      this.SkipComment();
      aToken.mType = eCSSToken_Comment;
      return true;
    }

    // EOF
    if (ch < 0) {
      return false;
    }

    // 'u' could be UNICODE-RANGE or an identifier-family token
    if (ch == LATIN_SMALL_LETTER_U || ch == LATIN_CAPITAL_LETTER_U) {
      let c2 = this.Peek(1);
      let c3 = this.Peek(2);
      if (c2 == PLUS_SIGN && (IsHexDigit(c3) || c3 == QUESTION_MARK)) {
        return this.ScanURange(aToken);
      }
      return this.ScanIdent(aToken);
    }

    // identifier family
    if (IsIdentStart(ch)) {
      return this.ScanIdent(aToken);
    }

    // number family
    if (IsDigit(ch)) {
      return this.ScanNumber(aToken);
    }

    if (ch == FULL_STOP && IsDigit(this.Peek(1))) {
      return this.ScanNumber(aToken);
    }

    if (ch == PLUS_SIGN) {
      let c2 = this.Peek(1);
      if (IsDigit(c2) || (c2 == FULL_STOP && IsDigit(this.Peek(2)))) {
        return this.ScanNumber(aToken);
      }
    }

    // HYPHEN_MINUS can start an identifier-family token, a number-family token,
    // or an HTML-comment
    if (ch == HYPHEN_MINUS) {
      let c2 = this.Peek(1);
      let c3 = this.Peek(2);
      if (IsIdentStart(c2) || (c2 == HYPHEN_MINUS && c3 != GREATER_THAN_SIGN)) {
        return this.ScanIdent(aToken);
      }
      if (IsDigit(c2) || (c2 == FULL_STOP && IsDigit(c3))) {
        return this.ScanNumber(aToken);
      }
      if (c2 == HYPHEN_MINUS && c3 == GREATER_THAN_SIGN) {
        this.Advance(3);
        aToken.mType = eCSSToken_HTMLComment;
        aToken.mIdent = stringToCodes("-->");
        return true;
      }
    }

    // the other HTML-comment token
    if (ch == LESS_THAN_SIGN &&
        this.Peek(1) == EXCLAMATION_MARK &&
        this.Peek(2) == HYPHEN_MINUS &&
        this.Peek(3) == HYPHEN_MINUS) {
      this.Advance(4);
      aToken.mType = eCSSToken_HTMLComment;
      aToken.mIdent = stringToCodes("<!--");
      return true;
    }

    // AT_KEYWORD
    if (ch == COMMERCIAL_AT) {
      return this.ScanAtKeyword(aToken);
    }

    // HASH
    if (ch == NUMBER_SIGN) {
      return this.ScanHash(aToken);
    }

    // STRING
    if (ch == QUOTATION_MARK || ch == APOSTROPHE) {
      return this.ScanString(aToken);
    }

    // Match operators: ~= |= ^= $= *=
    let opType = MatchOperatorType(ch);
    if (opType != eCSSToken_Symbol && this.Peek(1) == EQUALS_SIGN) {
      aToken.mType = opType;
      this.Advance(2);
      return true;
    }

    // Otherwise, a symbol (DELIM).
    aToken.mSymbol = ch;
    this.Advance();
    return true;
  },
};

/**
 * Create and return a new CSS lexer, conforming to the @see CSSLexer
 * webidl interface.
 *
 * @param {String} input the CSS text to lex
 * @return {CSSLexer} the new lexer
 */
function getCSSLexer(input) {
  return new Scanner(input);
}

exports.getCSSLexer = getCSSLexer;
