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
  // A "bad URL".  This is a URL that is unterminated at EOF.  |text|
  // holds the URL.
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
   * Return the next token, or null at EOF.
   */
  CSSToken? nextToken();
};
