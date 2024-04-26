/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-disable camelcase */

"use strict";

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
const QUOTATION_MARK = '"'.charCodeAt(0);
const RIGHT_PARENTHESIS = ")".charCodeAt(0);
const SOLIDUS = "/".charCodeAt(0);

const UCS2_REPLACEMENT_CHAR = 0xfffd;

const kImpliedEOFCharacters = [
  UCS2_REPLACEMENT_CHAR,
  ASTERISK,
  SOLIDUS,
  QUOTATION_MARK,
  APOSTROPHE,
  RIGHT_PARENTHESIS,
  0,
];

/**
 * Wrapper around InspectorCSSParser.
 * Once/if https://github.com/servo/rust-cssparser/pull/374 lands, we can remove this class.
 */
class InspectorCSSParserWrapper {
  #offset = 0;
  #trackEOFChars;
  #eofCharacters = eEOFCharacters_None;

  /**
   *
   * @param {String} input: The CSS text to lex
   * @param {Object} options
   * @param {Boolean} options.trackEOFChars: Set to true if performEOFFixup will be called.
   */
  constructor(input, options = {}) {
    this.parser = new InspectorCSSParser(input);
    this.#trackEOFChars = options.trackEOFChars;
  }

  get lineNumber() {
    return this.parser.lineNumber;
  }

  get columnNumber() {
    return this.parser.columnNumber;
  }

  nextToken() {
    const token = this.parser.nextToken();
    if (!token) {
      return token;
    }

    if (this.#trackEOFChars) {
      const { tokenType, text } = token;
      const lastChar = text[text.length - 1];
      if (tokenType === "Comment" && lastChar !== `/`) {
        if (lastChar === `*`) {
          this.#eofCharacters = eEOFCharacters_Slash;
        } else {
          this.#eofCharacters = eEOFCharacters_Asterisk | eEOFCharacters_Slash;
        }
      } else if (tokenType === "QuotedString" || tokenType === "BadString") {
        if (lastChar === "\\") {
          this.#eofCharacters =
            this.#eofCharacters | eEOFCharacters_DropBackslash;
        }
        if (text[0] !== lastChar) {
          this.#eofCharacters =
            this.#eofCharacters |
            (text[0] === `"`
              ? eEOFCharacters_DoubleQuote
              : eEOFCharacters_SingleQuote);
        }
      } else {
        if (lastChar === "\\") {
          this.#eofCharacters = eEOFCharacters_ReplacementChar;
        }

        // For some reason, we only automatically close `url`, other functions
        // will have their opening parenthesis escaped.
        if (
          (tokenType === "Function" && token.value === "url") ||
          tokenType === "BadUrl" ||
          (tokenType === "UnquotedUrl" && lastChar !== ")")
        ) {
          this.#eofCharacters = this.#eofCharacters | eEOFCharacters_CloseParen;
        }

        if (tokenType === "CloseParenthesis") {
          this.#eofCharacters =
            this.#eofCharacters & ~eEOFCharacters_CloseParen;
        }
      }
    }

    // At the moment, InspectorCSSParser doesn't expose offsets, so we need to compute
    // them manually here.
    // We can do that because we are retrieving every token in the input string, and so the
    // end offset of the last token is the start offset of the new token.
    token.startOffset = this.#offset;
    this.#offset += token.text.length;
    token.endOffset = this.#offset;
    return token;
  }

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
  performEOFFixup(inputString, preserveBackslash) {
    let result = inputString;

    let eofChars = this.#eofCharacters;
    if (
      preserveBackslash &&
      (eofChars &
        (eEOFCharacters_DropBackslash | eEOFCharacters_ReplacementChar)) !=
        0
    ) {
      eofChars &= ~(
        eEOFCharacters_DropBackslash | eEOFCharacters_ReplacementChar
      );
      result += "\\";
    }

    if (
      (eofChars & eEOFCharacters_DropBackslash) != 0 &&
      !!result.length &&
      result.endsWith("\\")
    ) {
      result = result.slice(0, -1);
    }

    // First, ignore eEOFCharacters_DropBackslash.
    let c = eofChars >> 1;

    // All of the remaining EOFCharacters bits represent appended characters,
    // and the bits are in the order that they need appending.
    for (const p of kImpliedEOFCharacters) {
      if (c & 1) {
        result += String.fromCharCode(p);
      }
      c >>= 1;
    }

    return result;
  }
}

exports.InspectorCSSParserWrapper = InspectorCSSParserWrapper;
