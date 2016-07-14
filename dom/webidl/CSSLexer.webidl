/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The possible values for CSSToken.tokenType.
enum CSSTokenType {
  // Whitespace.
  "whitespace",
  // A CSS comment.
  "comment",
  // An identifier.  |text| holds the identifier text.
  "ident",
  // A function token.  |text| holds the function name.  Note that the
  // function token includes (i.e., consumes) the "(" -- but this is
  // not included in |text|.
  "function",
  // "@word".  |text| holds "word", without the "@".
  "at",
  // "#word".  |text| holds "word", without the "#".
  "id",
  // "#word".  ID is used when "word" would have been a valid IDENT
  // token without the "#"; otherwise, HASH is used.
  "hash",
  // A number.
  "number",
  // A dimensioned number.
  "dimension",
  // A percentage.
  "percentage",
  // A string.
  "string",
  // A "bad string".  This can only be returned when a string is
  // unterminated at EOF.  (However, currently the lexer returns
  // ordinary STRING tokens in this situation.)
  "bad_string",
  // A URL.  |text| holds the URL.
  "url",
  // A "bad URL".  This is a URL that either contains a bad_string or contains
  // garbage after the string or unquoted URL test.  |text| holds the URL and
  // potentially whatever garbage came after it, up to but not including the
  // following ')'.
  "bad_url",
  // A "symbol" is any one-character symbol.  This corresponds to the
  // DELIM token in the CSS specification.
  "symbol",
  // The "~=" token.
  "includes",
  // The "|=" token.
  "dashmatch",
  // The "^=" token.
  "beginsmatch",
  // The "$=" token.
  "endsmatch",
  // The "*=" token.
  "containsmatch",
  // A unicode-range token.  This is currently not fully represented
  // by CSSToken.
  "urange",
  // HTML comment delimiters, either "<!--" or "-->".  Note that each
  // is emitted as a separate token, and the intervening text is lexed
  // as normal; whereas ordinary CSS comments are lexed as a unit.
  "htmlcomment"
};

dictionary CSSToken {
  // The token type.
  CSSTokenType tokenType = "whitespace";

  // Offset of the first character of the token.
  unsigned long startOffset = 0;
  // Offset of the character after the final character of the token.
  // This is chosen so that the offsets can be passed to |substring|
  // to yield the exact contents of the token.
  unsigned long endOffset = 0;

  // If the token is a number, percentage, or dimension, this holds
  // the value.  This is not present for other token types.
  double number;
  // If the token is a number, percentage, or dimension, this is true
  // iff the number had an explicit sign.  This is not present for
  // other token types.
  boolean hasSign;
  // If the token is a number, percentage, or dimension, this is true
  // iff the number was specified as an integer.  This is not present
  // for other token types.
  boolean isInteger;

  // Text associated with the token.  This is not present for all
  // token types.  In particular it is:
  //
  // Token type    Meaning
  // ===============================
  //    ident      The identifier.
  //    function   The function name.  Note that the "(" is part
  //               of the token but is not present in |text|.
  //    at         The word.
  //    id         The word.
  //    hash       The word.
  //    dimension  The dimension.
  //    string     The string contents after escape processing.
  //    bad_string Ditto.
  //    url        The URL after escape processing.
  //    bad_url    Ditto.
  //    symbol     The symbol text.
  DOMString text;
};

/**
 * CSSLexer is an interface to the CSS lexer.  It tokenizes an
 * input stream and returns CSS tokens.
 *
 * @see inIDOMUtils.getCSSLexer to create an instance of the lexer.
 */
[ChromeOnly]
interface CSSLexer
{
  /**
   * The line number of the most recently returned token.  Line
   * numbers are 0-based.
   */
  readonly attribute unsigned long lineNumber;

  /**
   * The column number of the most recently returned token.  Column
   * numbers are 0-based.
   */
  readonly attribute unsigned long columnNumber;

  /**
   * When EOF is reached, the last token might be unterminated in some
   * ways.  This method takes an input string and appends the needed
   * terminators.  In particular:
   *
   * 1. If EOF occurs mid-string, this will append the correct quote.
   * 2. If EOF occurs in a url token, this will append the close paren.
   * 3. If EOF occurs in a comment this will append the comment closer.
   *
   * A trailing backslash might also have been present in the input
   * string.  This is handled in different ways, depending on the
   * context and arguments.
   *
   * If preserveBackslash is true, then the existing backslash at the
   * end of inputString is preserved, and a new backslash is appended.
   * That is, the input |\| is transformed to |\\|, and the
   * input |'\| is transformed to |'\\'|.
   *
   * Otherwise, preserveBackslash is false:
   * If the backslash appears in a string context, then the trailing
   * backslash is dropped from inputString.  That is, |"\| is
   * transformed to |""|.
   * If the backslash appears outside of a string context, then
   * U+FFFD is appended.  That is, |\| is transformed to a string
   * with two characters: backslash followed by U+FFFD.
   *
   * Passing false for preserveBackslash makes the result conform to
   * the CSS Syntax specification.  However, passing true may give
   * somewhat more intuitive behavior.
   *
   * @param inputString the input string
   * @param preserveBackslash how to handle trailing backslashes
   * @return the input string with the termination characters appended
   */
  DOMString performEOFFixup(DOMString inputString, boolean preserveBackslash);

  /**
   * Return the next token, or null at EOF.
   */
  CSSToken? nextToken();
};
