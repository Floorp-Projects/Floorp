/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const jsLexer = require("resource://devtools/shared/css/lexer.js");

add_task(function test_lexer() {
  const LEX_TESTS = [
    ["simple", ["ident:simple"], ["Ident:simple"]],
    [
      "simple: { hi; }",
      [
        "ident:simple",
        "symbol::",
        "whitespace",
        "symbol:{",
        "whitespace",
        "ident:hi",
        "symbol:;",
        "whitespace",
        "symbol:}",
      ],
      [
        "Ident:simple",
        "Colon::",
        "WhiteSpace: ",
        "CurlyBracketBlock:{",
        "WhiteSpace: ",
        "Ident:hi",
        "Semicolon:;",
        "WhiteSpace: ",
        "CloseCurlyBracket:}",
      ],
    ],
    ["/* whatever */", ["comment"], ["Comment:/* whatever */"]],
    ["'string'", ["string:string"], ["QuotedString:'string'"]],
    ['"string"', ["string:string"], [`QuotedString:"string"`]],
    [
      "rgb(1,2,3)",
      [
        "function:rgb",
        "number",
        "symbol:,",
        "number",
        "symbol:,",
        "number",
        "symbol:)",
      ],
      [
        "Function:rgb(",
        "Number:1",
        "Comma:,",
        "Number:2",
        "Comma:,",
        "Number:3",
        "CloseParenthesis:)",
      ],
    ],
    ["@media", ["at:media"], ["AtKeyword:@media"]],
    ["#hibob", ["id:hibob"], ["IDHash:#hibob"]],
    ["#123", ["hash:123"], ["Hash:#123"]],
    ["23px", ["dimension:px"], ["Dimension:23px"]],
    ["23%", ["percentage"], ["Percentage:23%"]],
    [
      "url(http://example.com)",
      ["url:http://example.com"],
      ["UnquotedUrl:url(http://example.com)"],
    ],
    [
      "url('http://example.com')",
      ["url:http://example.com"],
      [
        "Function:url(",
        "QuotedString:'http://example.com'",
        "CloseParenthesis:)",
      ],
    ],
    [
      "url(  'http://example.com'  )",
      ["url:http://example.com"],
      [
        "Function:url(",
        "WhiteSpace:  ",
        "QuotedString:'http://example.com'",
        "WhiteSpace:  ",
        "CloseParenthesis:)",
      ],
    ],
    // In CSS Level 3, this is an ordinary URL, not a BAD_URL.
    [
      "url(http://example.com",
      ["url:http://example.com"],
      ["UnquotedUrl:url(http://example.com"],
    ],
    [
      "url(http://example.com @",
      ["bad_url:http://example.com"],
      ["BadUrl:url(http://example.com @"],
    ],
    ["quo\\ting", ["ident:quoting"], ["Ident:quo\\ting"]],
    [
      "'bad string\n",
      ["bad_string:bad string", "whitespace"],
      ["BadString:'bad string", "WhiteSpace:\n"],
    ],
    ["~=", ["includes"], ["IncludeMatch:~="]],
    ["|=", ["dashmatch"], ["DashMatch:|="]],
    ["^=", ["beginsmatch"], ["PrefixMatch:^="]],
    ["$=", ["endsmatch"], ["SuffixMatch:$="]],
    ["*=", ["containsmatch"], ["SubstringMatch:*="]],

    [
      "<!-- html comment -->",
      [
        "htmlcomment",
        "whitespace",
        "ident:html",
        "whitespace",
        "ident:comment",
        "whitespace",
        "htmlcomment",
      ],
      [
        "CDO:<!--",
        "WhiteSpace: ",
        "Ident:html",
        "WhiteSpace: ",
        "Ident:comment",
        "WhiteSpace: ",
        "CDC:-->",
      ],
    ],

    // earlier versions of CSS had "bad comment" tokens, but in level 3,
    // unterminated comments are just comments.
    ["/* bad comment", ["comment"], ["Comment:/* bad comment"]],
  ];

  const test = (cssText, useInspectorCSSParser, tokenTypes) => {
    const lexer = jsLexer.getCSSLexer(cssText, useInspectorCSSParser);
    let reconstructed = "";
    let lastTokenEnd = 0;
    let i = 0;
    let token;
    while ((token = lexer.nextToken())) {
      let combined = token.tokenType;
      if (token.text) {
        combined += ":" + token.text;
      }
      equal(combined, tokenTypes[i]);
      Assert.greater(token.endOffset, token.startOffset);
      equal(token.startOffset, lastTokenEnd);
      lastTokenEnd = token.endOffset;
      reconstructed += cssText.substring(token.startOffset, token.endOffset);
      ++i;
    }
    // Ensure that we saw the correct number of tokens.
    equal(i, tokenTypes.length);
    // Ensure that the reported offsets cover all the text.
    equal(reconstructed, cssText);
  };

  for (const [cssText, jsTokenTypes, rustTokenTypes] of LEX_TESTS) {
    info(`Test "${cssText}" with js-based lexer`);
    test(cssText, false, jsTokenTypes);

    info(`Test "${cssText}" with rust-based lexer`);
    test(cssText, true, rustTokenTypes);
  }
});

add_task(function test_lexer_linecol() {
  const LINECOL_TESTS = [
    ["simple", ["ident:0:0", ":0:6"], ["Ident:0:0", ":0:6"]],
    [
      "\n    stuff",
      ["whitespace:0:0", "ident:1:4", ":1:9"],
      ["WhiteSpace:0:0", "Ident:1:4", ":1:9"],
    ],
    [
      '"string with \\\nnewline"    \r\n',
      ["string:0:0", "whitespace:1:8", ":2:0"],
      ["QuotedString:0:0", "WhiteSpace:1:8", ":2:0"],
    ],
  ];

  const test = (cssText, useInspectorCSSParser, locations) => {
    const lexer = jsLexer.getCSSLexer(cssText, useInspectorCSSParser);
    let i = 0;
    let token;
    const testLocation = () => {
      const startLine = useInspectorCSSParser
        ? lexer.parser.lineNumber
        : lexer.lineNumber;
      const startColumn = useInspectorCSSParser
        ? lexer.parser.columnNumber
        : lexer.columnNumber;

      // We do this in a bit of a funny way so that we can also test the
      // location of the EOF.
      let combined = ":" + startLine + ":" + startColumn;
      if (token) {
        combined = token.tokenType + combined;
      }

      equal(combined, locations[i]);
      ++i;
    };
    while ((token = lexer.nextToken())) {
      testLocation();
    }
    // Collect location after we consumed all the tokens
    testLocation();
    // Ensure that we saw the correct number of tokens.
    equal(i, locations.length);
  };

  for (const [cssText, jsLocations, rustLocations] of LINECOL_TESTS) {
    info(`Test "${cssText}" with js-based lexer`);
    test(cssText, false, jsLocations);

    info(`Test "${cssText}" with rust-based lexer`);
    test(cssText, true, rustLocations);
  }
});

add_task(function test_lexer_eofchar() {
  const EOFCHAR_TESTS = [
    ["hello", "hello"],
    ["hello \\", "hello \\\\", "hello \\\uFFFD"],
    ["'hello", "'hello'"],
    ['"hello', '"hello"'],
    ["'hello\\", "'hello\\\\'", "'hello'"],
    ['"hello\\', '"hello\\\\"', '"hello"'],
    ["/*hello", "/*hello*/"],
    ["/*hello*", "/*hello*/"],
    ["/*hello\\", "/*hello\\*/"],
    ["url(hello", "url(hello)"],
    ["url('hello", "url('hello')"],
    ['url("hello', 'url("hello")'],
    ["url(hello\\", "url(hello\\\\)", "url(hello\\\uFFFD)"],
    ["url('hello\\", "url('hello\\\\')", "url('hello')"],
    ['url("hello\\', 'url("hello\\\\")', 'url("hello")'],
    // Ensure that passing a different inputString to performEOFFixup
    // doesn't cause an assertion trying to strip a backslash from the
    // end of an empty string.
    ["'\\", "\\'", "'", ""],
  ];

  for (let [
    cssText,
    expectedAppend,
    expectedNoAppend,
    argText = cssText,
  ] of EOFCHAR_TESTS) {
    if (!expectedNoAppend) {
      expectedNoAppend = expectedAppend;
    }
    const lexer = jsLexer.getCSSLexer(cssText);
    while (lexer.nextToken()) {
      // We don't need to do anything with the tokens. We only want to consume the iterator
      // so we can safely call performEOFFixup.
    }

    info("EOF char test, input = " + cssText);

    let result = lexer.performEOFFixup(argText, true);
    equal(result, expectedAppend);

    result = lexer.performEOFFixup(argText, false);
    equal(result, expectedNoAppend);
  }
});
