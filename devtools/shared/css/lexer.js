/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EEOFCHARACTERS_NONE = 0x0000;

// to handle \<EOF> inside strings
const EEOFCHARACTERS_DROPBACKSLASH = 0x0001;

// to handle \<EOF> outside strings
const EEOFCHARACTERS_REPLACEMENTCHAR = 0x0002;

// to close comments
const EEOFCHARACTERS_ASTERISK = 0x0004;
const EEOFCHARACTERS_SLASH = 0x0008;

// to close double-quoted strings
const EEOFCHARACTERS_DOUBLEQUOTE = 0x0010;

// to close single-quoted strings
const EEOFCHARACTERS_SINGLEQUOTE = 0x0020;

// to close URLs
const EEOFCHARACTERS_CLOSEPAREN = 0x0040;

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
  #eofCharacters = EEOFCHARACTERS_NONE;

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
          this.#eofCharacters = EEOFCHARACTERS_SLASH;
        } else {
          this.#eofCharacters = EEOFCHARACTERS_ASTERISK | EEOFCHARACTERS_SLASH;
        }
      } else if (tokenType === "QuotedString" || tokenType === "BadString") {
        if (lastChar === "\\") {
          this.#eofCharacters =
            this.#eofCharacters | EEOFCHARACTERS_DROPBACKSLASH;
        }
        if (text[0] !== lastChar || text.length === 1) {
          this.#eofCharacters =
            this.#eofCharacters |
            (text[0] === `"`
              ? EEOFCHARACTERS_DOUBLEQUOTE
              : EEOFCHARACTERS_SINGLEQUOTE);
        }
      } else {
        if (lastChar === "\\") {
          this.#eofCharacters = EEOFCHARACTERS_REPLACEMENTCHAR;
        }

        // For some reason, we only automatically close `url`, other functions
        // will have their opening parenthesis escaped.
        if (
          (tokenType === "Function" && token.value === "url") ||
          tokenType === "BadUrl" ||
          (tokenType === "UnquotedUrl" && lastChar !== ")")
        ) {
          this.#eofCharacters = this.#eofCharacters | EEOFCHARACTERS_CLOSEPAREN;
        }

        if (tokenType === "CloseParenthesis") {
          this.#eofCharacters =
            this.#eofCharacters & ~EEOFCHARACTERS_CLOSEPAREN;
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
   * The existing backslash at the end of inputString is preserved, and a new backslash
   * is appended.
   * That is, the input |\| is transformed to |\\|, and the
   * input |'\| is transformed to |'\\'|.
   *
   * @param inputString the input string
   * @return the input string with the termination characters appended
   */
  performEOFFixup(inputString) {
    let result = inputString;

    let eofChars = this.#eofCharacters;
    if (
      (eofChars &
        (EEOFCHARACTERS_DROPBACKSLASH | EEOFCHARACTERS_REPLACEMENTCHAR)) !=
      0
    ) {
      eofChars &= ~(
        EEOFCHARACTERS_DROPBACKSLASH | EEOFCHARACTERS_REPLACEMENTCHAR
      );
      result += "\\";
    }

    if (
      (eofChars & EEOFCHARACTERS_DROPBACKSLASH) != 0 &&
      !!result.length &&
      result.endsWith("\\")
    ) {
      result = result.slice(0, -1);
    }

    // First, ignore EEOFCHARACTERS_DROPBACKSLASH.
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
