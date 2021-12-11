/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
const {
  parseDeclarations,
  _parseCommentDeclarations,
  parseNamedDeclarations,
} = require("devtools/shared/css/parsing-utils");
const { isCssPropertyKnown } = require("devtools/server/actors/css-properties");

const TEST_DATA = [
  // Simple test
  {
    input: "p:v;",
    expected: [{ name: "p", value: "v", priority: "", offsets: [0, 4] }],
  },
  // Simple test
  {
    input: "this:is;a:test;",
    expected: [
      { name: "this", value: "is", priority: "", offsets: [0, 8] },
      { name: "a", value: "test", priority: "", offsets: [8, 15] },
    ],
  },
  // Test a single declaration with semi-colon
  {
    input: "name:value;",
    expected: [
      { name: "name", value: "value", priority: "", offsets: [0, 11] },
    ],
  },
  // Test a single declaration without semi-colon
  {
    input: "name:value",
    expected: [
      { name: "name", value: "value", priority: "", offsets: [0, 10] },
    ],
  },
  // Test multiple declarations separated by whitespaces and carriage
  // returns and tabs
  {
    input: "p1 : v1 ; \t\t  \n p2:v2;   \n\n\n\n\t  p3    :   v3;",
    expected: [
      { name: "p1", value: "v1", priority: "", offsets: [0, 9] },
      { name: "p2", value: "v2", priority: "", offsets: [16, 22] },
      { name: "p3", value: "v3", priority: "", offsets: [32, 45] },
    ],
  },
  // Test simple priority
  {
    input: "p1: v1; p2: v2 !important;",
    expected: [
      { name: "p1", value: "v1", priority: "", offsets: [0, 7] },
      { name: "p2", value: "v2", priority: "important", offsets: [8, 26] },
    ],
  },
  // Test simple priority
  {
    input: "p1: v1 !important; p2: v2",
    expected: [
      { name: "p1", value: "v1", priority: "important", offsets: [0, 18] },
      { name: "p2", value: "v2", priority: "", offsets: [19, 25] },
    ],
  },
  // Test simple priority
  {
    input: "p1: v1 !  important; p2: v2 ! important;",
    expected: [
      { name: "p1", value: "v1", priority: "important", offsets: [0, 20] },
      { name: "p2", value: "v2", priority: "important", offsets: [21, 40] },
    ],
  },
  // Test simple priority
  {
    input: "p1: v1 !/*comment*/important;",
    expected: [
      { name: "p1", value: "v1", priority: "important", offsets: [0, 29] },
    ],
  },
  // Test priority without terminating ";".
  {
    input: "p1: v1 !important",
    expected: [
      { name: "p1", value: "v1", priority: "important", offsets: [0, 17] },
    ],
  },
  // Test trailing "!" without terminating ";".
  {
    input: "p1: v1 !",
    expected: [{ name: "p1", value: "v1 !", priority: "", offsets: [0, 8] }],
  },
  // Test invalid priority
  {
    input: "p1: v1 important;",
    expected: [
      { name: "p1", value: "v1 important", priority: "", offsets: [0, 17] },
    ],
  },
  // Test invalid priority (in the middle of the declaration).
  // See bug 1462553.
  {
    input: "p1: v1 !important v2;",
    expected: [
      { name: "p1", value: "v1 !important v2", priority: "", offsets: [0, 21] },
    ],
  },
  {
    input: "p1: v1 !    important v2;",
    expected: [
      {
        name: "p1",
        value: "v1 ! important v2",
        priority: "",
        offsets: [0, 25],
      },
    ],
  },
  {
    input: "p1: v1 !  /*comment*/  important v2;",
    expected: [
      {
        name: "p1",
        value: "v1 ! important v2",
        priority: "",
        offsets: [0, 36],
      },
    ],
  },
  {
    input: "p1: v1 !/*hi*/important v2;",
    expected: [
      {
        name: "p1",
        value: "v1 ! important v2",
        priority: "",
        offsets: [0, 27],
      },
    ],
  },
  // Test various types of background-image urls
  {
    input: "background-image: url(../../relative/image.png)",
    expected: [
      {
        name: "background-image",
        value: "url(../../relative/image.png)",
        priority: "",
        offsets: [0, 47],
      },
    ],
  },
  {
    input: "background-image: url(http://site.com/test.png)",
    expected: [
      {
        name: "background-image",
        value: "url(http://site.com/test.png)",
        priority: "",
        offsets: [0, 47],
      },
    ],
  },
  {
    input: "background-image: url(wow.gif)",
    expected: [
      {
        name: "background-image",
        value: "url(wow.gif)",
        priority: "",
        offsets: [0, 30],
      },
    ],
  },
  // Test that urls with :;{} characters in them are parsed correctly
  {
    input:
      'background: red url("http://site.com/image{}:;.png?id=4#wat") ' +
      "repeat top right",
    expected: [
      {
        name: "background",
        value:
          'red url("http://site.com/image{}:;.png?id=4#wat") ' +
          "repeat top right",
        priority: "",
        offsets: [0, 78],
      },
    ],
  },
  // Test that an empty string results in an empty array
  { input: "", expected: [] },
  // Test that a string comprised only of whitespaces results in an empty array
  { input: "       \n \n   \n   \n \t  \t\t\t  ", expected: [] },
  // Test that a null input throws an exception
  { input: null, throws: true },
  // Test that a undefined input throws an exception
  { input: undefined, throws: true },
  // Test that :;{} characters in quoted content are not parsed as multiple
  // declarations
  {
    input: 'content: ";color:red;}selector{color:yellow;"',
    expected: [
      {
        name: "content",
        value: '";color:red;}selector{color:yellow;"',
        priority: "",
        offsets: [0, 45],
      },
    ],
  },
  // Test that rules aren't parsed, just declarations. So { and } found after a
  // property name should be part of the property name, same for values.
  {
    input: "body {color:red;} p {color: blue;}",
    expected: [
      { name: "body {color", value: "red", priority: "", offsets: [0, 16] },
      { name: "} p {color", value: "blue", priority: "", offsets: [16, 33] },
      { name: "}", value: "", priority: "", offsets: [33, 34] },
    ],
  },
  // Test unbalanced : and ;
  {
    input: "color :red : font : arial;",
    expected: [
      {
        name: "color",
        value: "red : font : arial",
        priority: "",
        offsets: [0, 26],
      },
    ],
  },
  {
    input: "background: red;;;;;",
    expected: [
      { name: "background", value: "red", priority: "", offsets: [0, 16] },
    ],
  },
  {
    input: "background:;",
    expected: [
      { name: "background", value: "", priority: "", offsets: [0, 12] },
    ],
  },
  { input: ";;;;;", expected: [] },
  { input: ":;:;", expected: [] },
  // Test name only
  {
    input: "color",
    expected: [{ name: "color", value: "", priority: "", offsets: [0, 5] }],
  },
  // Test trailing name without :
  {
    input: "color:blue;font",
    expected: [
      { name: "color", value: "blue", priority: "", offsets: [0, 11] },
      { name: "font", value: "", priority: "", offsets: [11, 15] },
    ],
  },
  // Test trailing name with :
  {
    input: "color:blue;font:",
    expected: [
      { name: "color", value: "blue", priority: "", offsets: [0, 11] },
      { name: "font", value: "", priority: "", offsets: [11, 16] },
    ],
  },
  // Test leading value
  {
    input: "Arial;color:blue;",
    expected: [
      { name: "", value: "Arial", priority: "", offsets: [0, 6] },
      { name: "color", value: "blue", priority: "", offsets: [6, 17] },
    ],
  },
  // Test hex colors
  {
    input: "color: #333",
    expected: [
      { name: "color", value: "#333", priority: "", offsets: [0, 11] },
    ],
  },
  {
    input: "color: #456789",
    expected: [
      { name: "color", value: "#456789", priority: "", offsets: [0, 14] },
    ],
  },
  {
    input: "wat: #XYZ",
    expected: [{ name: "wat", value: "#XYZ", priority: "", offsets: [0, 9] }],
  },
  // Test string/url quotes escaping
  {
    input: "content: \"this is a 'string'\"",
    expected: [
      {
        name: "content",
        value: "\"this is a 'string'\"",
        priority: "",
        offsets: [0, 29],
      },
    ],
  },
  {
    input: 'content: "this is a \\"string\\""',
    expected: [
      {
        name: "content",
        value: '"this is a \\"string\\""',
        priority: "",
        offsets: [0, 31],
      },
    ],
  },
  {
    input: "content: 'this is a \"string\"'",
    expected: [
      {
        name: "content",
        value: "'this is a \"string\"'",
        priority: "",
        offsets: [0, 29],
      },
    ],
  },
  {
    input: "content: 'this is a \\'string\\''",
    expected: [
      {
        name: "content",
        value: "'this is a \\'string\\''",
        priority: "",
        offsets: [0, 31],
      },
    ],
  },
  {
    input: "content: 'this \\' is a \" really strange string'",
    expected: [
      {
        name: "content",
        value: "'this \\' is a \" really strange string'",
        priority: "",
        offsets: [0, 47],
      },
    ],
  },
  {
    input: 'content: "a not s\\          o very long title"',
    expected: [
      {
        name: "content",
        value: '"a not s\\          o very long title"',
        priority: "",
        offsets: [0, 46],
      },
    ],
  },
  // Test calc with nested parentheses
  {
    input: "width: calc((100% - 3em) / 2)",
    expected: [
      {
        name: "width",
        value: "calc((100% - 3em) / 2)",
        priority: "",
        offsets: [0, 29],
      },
    ],
  },

  // Simple embedded comment test.
  {
    parseComments: true,
    input: "width: 5; /* background: green; */ background: red;",
    expected: [
      { name: "width", value: "5", priority: "", offsets: [0, 9] },
      {
        name: "background",
        value: "green",
        priority: "",
        offsets: [13, 31],
        commentOffsets: [10, 34],
      },
      { name: "background", value: "red", priority: "", offsets: [35, 51] },
    ],
  },

  // Embedded comment where the parsing heuristic fails.
  {
    parseComments: true,
    input: "width: 5; /* background something: green; */ background: red;",
    expected: [
      { name: "width", value: "5", priority: "", offsets: [0, 9] },
      { name: "background", value: "red", priority: "", offsets: [45, 61] },
    ],
  },

  // Embedded comment where the parsing heuristic is a bit funny.
  {
    parseComments: true,
    input: "width: 5; /* background: */ background: red;",
    expected: [
      { name: "width", value: "5", priority: "", offsets: [0, 9] },
      {
        name: "background",
        value: "",
        priority: "",
        offsets: [13, 24],
        commentOffsets: [10, 27],
      },
      { name: "background", value: "red", priority: "", offsets: [28, 44] },
    ],
  },

  // Another case where the parsing heuristic says not to bother; note
  // that there is no ";" in the comment.
  {
    parseComments: true,
    input: "width: 5; /* background: yellow */ background: red;",
    expected: [
      { name: "width", value: "5", priority: "", offsets: [0, 9] },
      {
        name: "background",
        value: "yellow",
        priority: "",
        offsets: [13, 31],
        commentOffsets: [10, 34],
      },
      { name: "background", value: "red", priority: "", offsets: [35, 51] },
    ],
  },

  // Parsing a comment should yield text that has been unescaped, and
  // the offsets should refer to the original text.
  {
    parseComments: true,
    input: "/* content: '*\\/'; */",
    expected: [
      {
        name: "content",
        value: "'*/'",
        priority: "",
        offsets: [3, 18],
        commentOffsets: [0, 21],
      },
    ],
  },

  // Parsing a comment should yield text that has been unescaped, and
  // the offsets should refer to the original text.  This variant
  // tests the no-semicolon path.
  {
    parseComments: true,
    input: "/* content: '*\\/' */",
    expected: [
      {
        name: "content",
        value: "'*/'",
        priority: "",
        offsets: [3, 17],
        commentOffsets: [0, 20],
      },
    ],
  },

  // A comment-in-a-comment should yield the correct offsets.
  {
    parseComments: true,
    input: "/* color: /\\* comment *\\/ red; */",
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [3, 30],
        commentOffsets: [0, 33],
      },
    ],
  },

  // HTML comments are not special -- they are just ordinary tokens.
  {
    parseComments: true,
    input: "<!-- color: red; --> color: blue;",
    expected: [
      { name: "<!-- color", value: "red", priority: "", offsets: [0, 16] },
      { name: "--> color", value: "blue", priority: "", offsets: [17, 33] },
    ],
  },

  // Don't error on an empty comment.
  {
    parseComments: true,
    input: "/**/",
    expected: [],
  },

  // Parsing our special comments skips the name-check heuristic.
  {
    parseComments: true,
    input: "/*! walrus: zebra; */",
    expected: [
      {
        name: "walrus",
        value: "zebra",
        priority: "",
        offsets: [4, 18],
        commentOffsets: [0, 21],
      },
    ],
  },

  // Regression test for bug 1287620.
  {
    input: "color: blue \\9 no\\_need",
    expected: [
      {
        name: "color",
        value: "blue \\9 no_need",
        priority: "",
        offsets: [0, 23],
      },
    ],
  },

  // Regression test for bug 1297890 - don't paste tokens.
  {
    parseComments: true,
    input: "stroke-dasharray: 1/*ThisIsAComment*/2;",
    expected: [
      {
        name: "stroke-dasharray",
        value: "1 2",
        priority: "",
        offsets: [0, 39],
      },
    ],
  },

  // Regression test for bug 1384463 - don't trim significant
  // whitespace.
  {
    // \u00a0 is non-breaking space.
    input: "\u00a0vertical-align: top",
    expected: [
      {
        name: "\u00a0vertical-align",
        value: "top",
        priority: "",
        offsets: [0, 20],
      },
    ],
  },

  // Regression test for bug 1544223 - take CSS blocks into consideration before handling
  // ; and : (i.e. don't advance to the property name or value automatically).
  {
    input: `--foo: (
      :doodle {
        @grid: 30x1 / 18vmin;
      }
      :container {
        perspective: 30vmin;
      }

      @place-cell: center;
      @size: 100%;

      :after, :before {
        content: '';
        @size: 100%;
        position: absolute;
        border: 2.4vmin solid var(--color);
        border-image: radial-gradient(
          var(--color), transparent 60%
        );
        border-image-width: 4;
        transform: rotate(@pn(0, 5deg));
      }

      will-change: transform, opacity;
      animation: scale-up 15s linear infinite;
      animation-delay: calc(-15s / @size() * @i());
      box-shadow: inset 0 0 1em var(--color);
      border-radius: 50%;
      filter: var(--filter);

      @keyframes scale-up {
        0%, 100% {
          transform: translateZ(0) scale(0) rotate(0);
          opacity: 0;
        }
        50% {
          opacity: 1;
        }
        99% {
          transform:
            translateZ(30vmin)
            scale(1)
            rotate(-270deg);
        }
      }
    );`,
    expected: [
      {
        name: "--foo",
        value:
          "( :doodle { @grid: 30x1 / 18vmin; } :container { perspective: 30vmin; } " +
          "@place-cell: center; @size: 100%; :after, :before { content: ''; @size: " +
          "100%; position: absolute; border: 2.4vmin solid var(--color); " +
          "border-image: radial-gradient( var(--color), transparent 60% ); " +
          "border-image-width: 4; transform: rotate(@pn(0, 5deg)); } will-change: " +
          "transform, opacity; animation: scale-up 15s linear infinite; " +
          "animation-delay: calc(-15s / @size() * @i()); box-shadow: inset 0 0 1em " +
          "var(--color); border-radius: 50%; filter: var(--filter); @keyframes " +
          "scale-up { 0%, 100% { transform: translateZ(0) scale(0) rotate(0); " +
          "opacity: 0; } 50% { opacity: 1; } 99% { transform: translateZ(30vmin) " +
          "scale(1) rotate(-270deg); } } )",
        priority: "",
        offsets: [0, 1036],
      },
    ],
  },
];

function run_test() {
  run_basic_tests();
  run_comment_tests();
  run_named_tests();
}

// Test parseDeclarations.
function run_basic_tests() {
  for (const test of TEST_DATA) {
    info("Test input string " + test.input);
    let output;
    try {
      output = parseDeclarations(
        isCssPropertyKnown,
        test.input,
        test.parseComments
      );
    } catch (e) {
      info(
        "parseDeclarations threw an exception with the given input " + "string"
      );
      if (test.throws) {
        info("Exception expected");
        Assert.ok(true);
      } else {
        info("Exception unexpected\n" + e);
        Assert.ok(false);
      }
    }
    if (output) {
      assertOutput(output, test.expected);
    }
  }
}

const COMMENT_DATA = [
  {
    input: "content: 'hi",
    expected: [
      {
        name: "content",
        value: "'hi",
        priority: "",
        terminator: "';",
        offsets: [2, 14],
        colonOffsets: [9, 11],
        commentOffsets: [0, 16],
      },
    ],
  },
  {
    input: "text that once confounded the parser;",
    expected: [],
  },
];

// Test parseCommentDeclarations.
function run_comment_tests() {
  for (const test of COMMENT_DATA) {
    info("Test input string " + test.input);
    const output = _parseCommentDeclarations(
      isCssPropertyKnown,
      test.input,
      0,
      test.input.length + 4
    );
    deepEqual(output, test.expected);
  }
}

const NAMED_DATA = [
  {
    input: "position:absolute;top50px;height:50px;",
    expected: [
      {
        name: "position",
        value: "absolute",
        priority: "",
        terminator: "",
        offsets: [0, 18],
        colonOffsets: [8, 9],
      },
      {
        name: "height",
        value: "50px",
        priority: "",
        terminator: "",
        offsets: [26, 38],
        colonOffsets: [32, 33],
      },
    ],
  },
];

// Test parseNamedDeclarations.
function run_named_tests() {
  for (const test of NAMED_DATA) {
    info("Test input string " + test.input);
    const output = parseNamedDeclarations(isCssPropertyKnown, test.input, true);
    info(JSON.stringify(output));
    deepEqual(output, test.expected);
  }
}

function assertOutput(actual, expected) {
  if (actual.length === expected.length) {
    for (let i = 0; i < expected.length; i++) {
      Assert.ok(!!actual[i]);
      info(
        "Check that the output item has the expected name, " +
          "value and priority"
      );
      Assert.equal(expected[i].name, actual[i].name);
      Assert.equal(expected[i].value, actual[i].value);
      Assert.equal(expected[i].priority, actual[i].priority);
      deepEqual(expected[i].offsets, actual[i].offsets);
      if ("commentOffsets" in expected[i]) {
        deepEqual(expected[i].commentOffsets, actual[i].commentOffsets);
      }
    }
  } else {
    for (const prop of actual) {
      info(
        "Actual output contained: {name: " +
          prop.name +
          ", value: " +
          prop.value +
          ", priority: " +
          prop.priority +
          "}"
      );
    }
    Assert.equal(actual.length, expected.length);
  }
}
