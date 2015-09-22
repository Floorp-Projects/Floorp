/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci} = require("chrome");
loader.lazyGetter(this, "DOMUtils", () => {
  return Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
});

/**
 * A generator function that lexes a CSS source string, yielding the
 * CSS tokens.  Comment tokens are dropped.
 *
 * @param {String} CSS source string
 * @yield {CSSToken} The next CSSToken that is lexed
 * @see CSSToken for details about the returned tokens
 */
function* cssTokenizer(string) {
  let lexer = DOMUtils.getCSSLexer(string);
  while (true) {
    let token = lexer.nextToken();
    if (!token) {
      break;
    }
    // None of the existing consumers want comments.
    if (token.tokenType !== "comment") {
      yield token;
    }
  }
}

exports.cssTokenizer = cssTokenizer;

/**
 * Pass |string| to the CSS lexer and return an array of all the
 * returned tokens.  Comment tokens are not included.  In addition to
 * the usual information, each token will have starting and ending
 * line and column information attached.  Specifically, each token
 * has an additional "loc" attribute.  This attribute is an object
 * of the form {line: L, column: C}.  Lines and columns are both zero
 * based.
 *
 * It's best not to add new uses of this function.  In general it is
 * simpler and better to use the CSSToken offsets, rather than line
 * and column.  Also, this function lexes the entire input string at
 * once, rather than lazily yielding a token stream.  Use
 * |cssTokenizer| or |DOMUtils.getCSSLexer| instead.
 *
 * @param{String} string The input string.
 * @return {Array} An array of tokens (@see CSSToken) that have
 *        line and column information.
 */
function cssTokenizerWithLineColumn(string) {
  let lexer = DOMUtils.getCSSLexer(string);
  let result = [];
  let prevToken = undefined;
  while (true) {
    let token = lexer.nextToken();
    let lineNumber = lexer.lineNumber;
    let columnNumber = lexer.columnNumber;

    if (prevToken) {
      prevToken.loc.end = {
        line: lineNumber,
        column: columnNumber
      };
    }

    if (!token) {
      break;
    }

    if (token.tokenType === "comment") {
      // We've already dealt with the previous token's location.
      prevToken = undefined;
    } else {
      let startLoc = {
        line: lineNumber,
        column: columnNumber
      };
      token.loc = {start: startLoc};

      result.push(token);
      prevToken = token;
    }
  }

  return result;
}

exports.cssTokenizerWithLineColumn = cssTokenizerWithLineColumn;
