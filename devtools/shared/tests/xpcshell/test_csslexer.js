/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

const jsLexer = require("resource://devtools/shared/css/lexer.js");

add_task(function test_lexer() {
  const LEX_TESTS = [
    [
      "simple",
      ["ident:simple"],
      [{ tokenType: "Ident", text: "simple", value: "simple" }],
    ],
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
        { tokenType: "Ident", text: "simple", value: "simple" },
        { tokenType: "Colon", text: ":" },
        { tokenType: "WhiteSpace", text: " " },
        { tokenType: "CurlyBracketBlock", text: "{" },
        { tokenType: "WhiteSpace", text: " " },
        { tokenType: "Ident", text: "hi", value: "hi" },
        { tokenType: "Semicolon", text: ";" },
        { tokenType: "WhiteSpace", text: " " },
        { tokenType: "CloseCurlyBracket", text: "}" },
      ],
    ],
    [
      "/* whatever */",
      ["comment"],
      [{ tokenType: "Comment", text: "/* whatever */", value: " whatever " }],
    ],
    [
      "'string'",
      ["string:string"],
      [{ tokenType: "QuotedString", text: "'string'", value: "string" }],
    ],
    [
      '"string"',
      ["string:string"],
      [{ tokenType: "QuotedString", text: `"string"`, value: "string" }],
    ],
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
        { tokenType: "Function", text: "rgb(", value: "rgb" },
        { tokenType: "Number", text: "1", number: 1 },
        { tokenType: "Comma", text: "," },
        { tokenType: "Number", text: "2", number: 2 },
        { tokenType: "Comma", text: "," },
        { tokenType: "Number", text: "3", number: 3 },
        { tokenType: "CloseParenthesis", text: ")" },
      ],
    ],
    [
      "@media",
      ["at:media"],
      [{ tokenType: "AtKeyword", text: "@media", value: "media" }],
    ],
    [
      "#hibob",
      ["id:hibob"],
      [{ tokenType: "IDHash", text: "#hibob", value: "hibob" }],
    ],
    ["#123", ["hash:123"], [{ tokenType: "Hash", text: "#123", value: "123" }]],
    [
      "23px",
      ["dimension:px"],
      [{ tokenType: "Dimension", text: "23px", number: 23, unit: "px" }],
    ],
    [
      "23%",
      ["percentage"],
      [{ tokenType: "Percentage", text: "23%", number: 0.23 }],
    ],
    [
      "url(http://example.com)",
      ["url:http://example.com"],
      [
        {
          tokenType: "UnquotedUrl",
          text: "url(http://example.com)",
          value: "http://example.com",
        },
      ],
    ],
    [
      "url('http://example.com')",
      ["url:http://example.com"],
      [
        { tokenType: "Function", text: "url(", value: "url" },
        {
          tokenType: "QuotedString",
          text: "'http://example.com'",
          value: "http://example.com",
        },
        { tokenType: "CloseParenthesis", text: ")" },
      ],
    ],
    [
      "url(  'http://example.com'  )",
      ["url:http://example.com"],
      [
        { tokenType: "Function", text: "url(", value: "url" },
        { tokenType: "WhiteSpace", text: "  " },
        {
          tokenType: "QuotedString",
          text: "'http://example.com'",
          value: "http://example.com",
        },
        { tokenType: "WhiteSpace", text: "  " },
        { tokenType: "CloseParenthesis", text: ")" },
      ],
    ],
    // In CSS Level 3, this is an ordinary URL, not a BAD_URL.
    [
      "url(http://example.com",
      ["url:http://example.com"],
      [
        {
          tokenType: "UnquotedUrl",
          text: "url(http://example.com",
          value: "http://example.com",
        },
      ],
    ],
    [
      "url(http://example.com @",
      ["bad_url:http://example.com"],
      [
        {
          tokenType: "BadUrl",
          text: "url(http://example.com @",
          value: "http://example.com @",
        },
      ],
    ],
    [
      "quo\\ting",
      ["ident:quoting"],
      [{ tokenType: "Ident", text: "quo\\ting", value: "quoting" }],
    ],
    [
      "'bad string\n",
      ["bad_string:bad string", "whitespace"],
      [
        { tokenType: "BadString", text: "'bad string", value: "bad string" },
        { tokenType: "WhiteSpace", text: "\n" },
      ],
    ],
    ["~=", ["includes"], [{ tokenType: "IncludeMatch", text: "~=" }]],
    ["|=", ["dashmatch"], [{ tokenType: "DashMatch", text: "|=" }]],
    ["^=", ["beginsmatch"], [{ tokenType: "PrefixMatch", text: "^=" }]],
    ["$=", ["endsmatch"], [{ tokenType: "SuffixMatch", text: "$=" }]],
    ["*=", ["containsmatch"], [{ tokenType: "SubstringMatch", text: "*=" }]],

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
        { tokenType: "CDO", text: "<!--" },
        { tokenType: "WhiteSpace", text: " " },
        { tokenType: "Ident", text: "html", value: "html" },
        { tokenType: "WhiteSpace", text: " " },
        { tokenType: "Ident", text: "comment", value: "comment" },
        { tokenType: "WhiteSpace", text: " " },
        { tokenType: "CDC", text: "-->" },
      ],
    ],

    // earlier versions of CSS had "bad comment" tokens, but in level 3,
    // unterminated comments are just comments.
    [
      "/* bad comment",
      ["comment"],
      [{ tokenType: "Comment", text: "/* bad comment", value: " bad comment" }],
    ],
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
      if (useInspectorCSSParser) {
        const expectedToken = tokenTypes[i];
        Assert.deepEqual(
          {
            tokenType: token.tokenType,
            text: token.text,
            value: token.value,
            number: token.number,
            unit: token.unit,
          },
          {
            tokenType: expectedToken.tokenType,
            text: expectedToken.text,
            value: expectedToken.value ?? null,
            number: expectedToken.number ?? null,
            unit: expectedToken.unit ?? null,
          },
          `Got expected token #${i} for "${cssText}"`
        );
      } else {
        equal(combined, tokenTypes[i]);
      }
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

  const test = (
    cssText,
    useInspectorCSSParser,
    expectedAppend,
    expectedNoAppend,
    argText
  ) => {
    if (!expectedNoAppend) {
      expectedNoAppend = expectedAppend;
    }
    const lexer = jsLexer.getCSSLexer(cssText, useInspectorCSSParser, true);
    while (lexer.nextToken()) {
      // We don't need to do anything with the tokens. We only want to consume the iterator
      // so we can safely call performEOFFixup.
    }

    info("EOF char test, input = " + cssText);

    let result = lexer.performEOFFixup(argText, true);
    equal(result, expectedAppend);

    result = lexer.performEOFFixup(argText, false);
    equal(result, expectedNoAppend);
  };

  for (const [
    cssText,
    expectedAppend,
    expectedNoAppend,
    argText = cssText,
  ] of EOFCHAR_TESTS) {
    info(`Test "${cssText}" with js-based lexer`);
    test(cssText, false, expectedAppend, expectedNoAppend, argText);

    info(`Test "${cssText}" with rust-based lexer`);
    test(cssText, true, expectedAppend, expectedNoAppend, argText);
  }
});
