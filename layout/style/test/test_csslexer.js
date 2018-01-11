/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

Components.utils.importGlobalProperties(["InspectorUtils"]);

function test_lexer(cssText, tokenTypes) {
  let lexer = InspectorUtils.getCSSLexer(cssText);
  let reconstructed = '';
  let lastTokenEnd = 0;
  let i = 0;
  while (true) {
    let token = lexer.nextToken();
    if (!token) {
      break;
    }
    let combined = token.tokenType;
    if (token.text)
      combined += ":" + token.text;
    equal(combined, tokenTypes[i]);
    ok(token.endOffset > token.startOffset);
    equal(token.startOffset, lastTokenEnd);
    lastTokenEnd = token.endOffset;
    reconstructed += cssText.substring(token.startOffset, token.endOffset);
    ++i;
  }
  // Ensure that we saw the correct number of tokens.
  equal(i, tokenTypes.length);
  // Ensure that the reported offsets cover all the text.
  equal(reconstructed, cssText);
}

var LEX_TESTS = [
  ["simple", ["ident:simple"]],
  ["simple: { hi; }",
             ["ident:simple", "symbol::",
              "whitespace", "symbol:{",
              "whitespace", "ident:hi",
              "symbol:;", "whitespace",
              "symbol:}"]],
  ["/* whatever */", ["comment"]],
  ["'string'", ["string:string"]],
  ['"string"', ["string:string"]],
  ["rgb(1,2,3)", ["function:rgb", "number",
                                      "symbol:,", "number",
                                      "symbol:,", "number",
                                      "symbol:)"]],
  ["@media", ["at:media"]],
  ["#hibob", ["id:hibob"]],
  ["#123", ["hash:123"]],
  ["23px", ["dimension:px"]],
  ["23%", ["percentage"]],
  ["url(http://example.com)", ["url:http://example.com"]],
  ["url('http://example.com')", ["url:http://example.com"]],
  ["url(  'http://example.com'  )",
             ["url:http://example.com"]],
  // In CSS Level 3, this is an ordinary URL, not a BAD_URL.
  ["url(http://example.com", ["url:http://example.com"]],
  ["url(http://example.com @", ["bad_url:http://example.com"]],
  ["quo\\ting", ["ident:quoting"]],
  ["'bad string\n", ["bad_string:bad string", "whitespace"]],
  ["~=", ["includes"]],
  ["|=", ["dashmatch"]],
  ["^=", ["beginsmatch"]],
  ["$=", ["endsmatch"]],
  ["*=", ["containsmatch"]],

  // URANGE may be on the way out, and it isn't used by devutils, so
  // let's skip it.

  ["<!-- html comment -->", ["htmlcomment", "whitespace", "ident:html",
                             "whitespace", "ident:comment", "whitespace",
                             "htmlcomment"]],

  // earlier versions of CSS had "bad comment" tokens, but in level 3,
  // unterminated comments are just comments.
  ["/* bad comment", ["comment"]]
];

function test_lexer_linecol(cssText, locations) {
  let lexer = InspectorUtils.getCSSLexer(cssText);
  let i = 0;
  while (true) {
    let token = lexer.nextToken();
    let startLine = lexer.lineNumber;
    let startColumn = lexer.columnNumber;

    // We do this in a bit of a funny way so that we can also test the
    // location of the EOF.
    let combined = ":" + startLine + ":" + startColumn;
    if (token)
      combined = token.tokenType + combined;

    equal(combined, locations[i]);
    ++i;

    if (!token) {
      break;
    }
  }
  // Ensure that we saw the correct number of tokens.
  equal(i, locations.length);
}

function test_lexer_eofchar(cssText, argText, expectedAppend,
                            expectedNoAppend) {
  let lexer = InspectorUtils.getCSSLexer(cssText);
  while (lexer.nextToken()) {
    // Nothing.
  }

  info("EOF char test, input = " + cssText);

  let result = lexer.performEOFFixup(argText, true);
  equal(result, expectedAppend);

  result = lexer.performEOFFixup(argText, false);
  equal(result, expectedNoAppend);
}

var LINECOL_TESTS = [
  ["simple", ["ident:0:0", ":0:6"]],
  ["\n    stuff", ["whitespace:0:0", "ident:1:4", ":1:9"]],
  ['"string with \\\nnewline"    \r\n', ["string:0:0", "whitespace:1:8",
                                         ":2:0"]]
];

var EOFCHAR_TESTS = [
  ["hello", "hello"],
  ["hello \\", "hello \\\\", "hello \\\uFFFD"],
  ["'hello", "'hello'"],
  ["\"hello", "\"hello\""],
  ["'hello\\", "'hello\\\\'", "'hello'"],
  ["\"hello\\", "\"hello\\\\\"", "\"hello\""],
  ["/*hello", "/*hello*/"],
  ["/*hello*", "/*hello*/"],
  ["/*hello\\", "/*hello\\*/"],
  ["url(hello", "url(hello)"],
  ["url('hello", "url('hello')"],
  ["url(\"hello", "url(\"hello\")"],
  ["url(hello\\", "url(hello\\\\)", "url(hello\\\uFFFD)"],
  ["url('hello\\", "url('hello\\\\')", "url('hello')"],
  ["url(\"hello\\", "url(\"hello\\\\\")", "url(\"hello\")"],
];

function run_test()
{
  let text, result;
  for ([text, result] of LEX_TESTS) {
    test_lexer(text, result);
  }

  for ([text, result] of LINECOL_TESTS) {
    test_lexer_linecol(text, result);
  }

  for ([text, expectedAppend, expectedNoAppend] of EOFCHAR_TESTS) {
    if (!expectedNoAppend) {
      expectedNoAppend = expectedAppend;
    }
    test_lexer_eofchar(text, text, expectedAppend, expectedNoAppend);
  }

  // Ensure that passing a different inputString to performEOFFixup
  // doesn't cause an assertion trying to strip a backslash from the
  // end of an empty string.
  test_lexer_eofchar("'\\", "", "\\'", "'");
}
