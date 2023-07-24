/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  parseDeclarations,
  _parseCommentDeclarations,
  parseNamedDeclarations,
} = require("resource://devtools/shared/css/parsing-utils.js");
const {
  isCssPropertyKnown,
} = require("resource://devtools/server/actors/css-properties.js");

const TEST_DATA = [
  // Simple test
  {
    input: "p:v;",
    expected: [
      {
        name: "p",
        value: "v",
        priority: "",
        offsets: [0, 4],
        declarationText: "p:v;",
      },
    ],
  },
  // Simple test
  {
    input: "this:is;a:test;",
    expected: [
      {
        name: "this",
        value: "is",
        priority: "",
        offsets: [0, 8],
        declarationText: "this:is;",
      },
      {
        name: "a",
        value: "test",
        priority: "",
        offsets: [8, 15],
        declarationText: "a:test;",
      },
    ],
  },
  // Test a single declaration with semi-colon
  {
    input: "name:value;",
    expected: [
      {
        name: "name",
        value: "value",
        priority: "",
        offsets: [0, 11],
        declarationText: "name:value;",
      },
    ],
  },
  // Test a single declaration without semi-colon
  {
    input: "name:value",
    expected: [
      {
        name: "name",
        value: "value",
        priority: "",
        offsets: [0, 10],
        declarationText: "name:value",
      },
    ],
  },
  // Test multiple declarations separated by whitespaces and carriage
  // returns and tabs
  {
    input: "p1 : v1 ; \t\t  \n p2:v2;   \n\n\n\n\t  p3    :   v3;",
    expected: [
      {
        name: "p1",
        value: "v1",
        priority: "",
        offsets: [0, 9],
        declarationText: "p1 : v1 ;",
      },
      {
        name: "p2",
        value: "v2",
        priority: "",
        offsets: [16, 22],
        declarationText: "p2:v2;",
      },
      {
        name: "p3",
        value: "v3",
        priority: "",
        offsets: [32, 45],
        declarationText: "p3    :   v3;",
      },
    ],
  },
  // Test simple priority
  {
    input: "p1: v1; p2: v2 !important;",
    expected: [
      {
        name: "p1",
        value: "v1",
        priority: "",
        offsets: [0, 7],
        declarationText: "p1: v1;",
      },
      {
        name: "p2",
        value: "v2",
        priority: "important",
        offsets: [8, 26],
        declarationText: "p2: v2 !important;",
      },
    ],
  },
  // Test simple priority
  {
    input: "p1: v1 !important; p2: v2",
    expected: [
      {
        name: "p1",
        value: "v1",
        priority: "important",
        offsets: [0, 18],
        declarationText: "p1: v1 !important;",
      },
      {
        name: "p2",
        value: "v2",
        priority: "",
        offsets: [19, 25],
        declarationText: "p2: v2",
      },
    ],
  },
  // Test simple priority
  {
    input: "p1: v1 !  important; p2: v2 ! important;",
    expected: [
      {
        name: "p1",
        value: "v1",
        priority: "important",
        offsets: [0, 20],
        declarationText: "p1: v1 !  important;",
      },
      {
        name: "p2",
        value: "v2",
        priority: "important",
        offsets: [21, 40],
        declarationText: "p2: v2 ! important;",
      },
    ],
  },
  // Test simple priority
  {
    input: "p1: v1 !/*comment*/important;",
    expected: [
      {
        name: "p1",
        value: "v1",
        priority: "important",
        offsets: [0, 29],
        declarationText: "p1: v1 !/*comment*/important;",
      },
    ],
  },
  // Test priority without terminating ";".
  {
    input: "p1: v1 !important",
    expected: [
      {
        name: "p1",
        value: "v1",
        priority: "important",
        offsets: [0, 17],
        declarationText: "p1: v1 !important",
      },
    ],
  },
  // Test trailing "!" without terminating ";".
  {
    input: "p1: v1 !",
    expected: [
      {
        name: "p1",
        value: "v1 !",
        priority: "",
        offsets: [0, 8],
        declarationText: "p1: v1 !",
      },
    ],
  },
  // Test invalid priority
  {
    input: "p1: v1 important;",
    expected: [
      {
        name: "p1",
        value: "v1 important",
        priority: "",
        offsets: [0, 17],
        declarationText: "p1: v1 important;",
      },
    ],
  },
  // Test invalid priority (in the middle of the declaration).
  // See bug 1462553.
  {
    input: "p1: v1 !important v2;",
    expected: [
      {
        name: "p1",
        value: "v1 !important v2",
        priority: "",
        offsets: [0, 21],
        declarationText: "p1: v1 !important v2;",
      },
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
        declarationText: "p1: v1 !    important v2;",
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
        declarationText: "p1: v1 !  /*comment*/  important v2;",
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
        declarationText: "p1: v1 !/*hi*/important v2;",
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
        declarationText: "background-image: url(../../relative/image.png)",
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
        declarationText: "background-image: url(http://site.com/test.png)",
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
        declarationText: "background-image: url(wow.gif)",
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
        declarationText:
          'background: red url("http://site.com/image{}:;.png?id=4#wat") ' +
          "repeat top right",
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
        declarationText: 'content: ";color:red;}selector{color:yellow;"',
      },
    ],
  },
  // Test that rules aren't parsed, just declarations.
  {
    input: "body {color:red;} p {color: blue;}",
    expected: [],
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
      {
        name: "background",
        value: "red",
        priority: "",
        offsets: [0, 16],
        declarationText: "background: red;",
      },
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
      {
        name: "color",
        value: "blue",
        priority: "",
        offsets: [0, 11],
        declarationText: "color:blue;",
      },
      { name: "font", value: "", priority: "", offsets: [11, 15] },
    ],
  },
  // Test trailing name with :
  {
    input: "color:blue;font:",
    expected: [
      {
        name: "color",
        value: "blue",
        priority: "",
        offsets: [0, 11],
        declarationText: "color:blue;",
      },
      { name: "font", value: "", priority: "", offsets: [11, 16] },
    ],
  },
  // Test leading value
  {
    input: "Arial;color:blue;",
    expected: [
      { name: "", value: "Arial", priority: "", offsets: [0, 6] },
      {
        name: "color",
        value: "blue",
        priority: "",
        offsets: [6, 17],
        declarationText: "color:blue;",
      },
    ],
  },
  // Test hex colors
  {
    input: "color: #333",
    expected: [
      {
        name: "color",
        value: "#333",
        priority: "",
        offsets: [0, 11],
        declarationText: "color: #333",
      },
    ],
  },
  {
    input: "color: #456789",
    expected: [
      {
        name: "color",
        value: "#456789",
        priority: "",
        offsets: [0, 14],
        declarationText: "color: #456789",
      },
    ],
  },
  {
    input: "wat: #XYZ",
    expected: [
      {
        name: "wat",
        value: "#XYZ",
        priority: "",
        offsets: [0, 9],
        declarationText: "wat: #XYZ",
      },
    ],
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
        declarationText: "content: \"this is a 'string'\"",
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
        declarationText: 'content: "this is a \\"string\\""',
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
        declarationText: "content: 'this is a \"string\"'",
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
        declarationText: "content: 'this is a \\'string\\''",
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
        declarationText: "content: 'this \\' is a \" really strange string'",
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
        declarationText: 'content: "a not s\\          o very long title"',
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
        declarationText: "width: calc((100% - 3em) / 2)",
      },
    ],
  },

  // Simple embedded comment test.
  {
    parseComments: true,
    input: "width: 5; /* background: green; */ background: red;",
    expected: [
      {
        name: "width",
        value: "5",
        priority: "",
        offsets: [0, 9],
        declarationText: "width: 5;",
      },
      {
        name: "background",
        value: "green",
        priority: "",
        offsets: [13, 31],
        declarationText: "background: green;",
        commentOffsets: [10, 34],
      },
      {
        name: "background",
        value: "red",
        priority: "",
        offsets: [35, 51],
        declarationText: "background: red;",
      },
    ],
  },

  // Embedded comment where the parsing heuristic fails.
  {
    parseComments: true,
    input: "width: 5; /* background something: green; */ background: red;",
    expected: [
      {
        name: "width",
        value: "5",
        priority: "",
        offsets: [0, 9],
        declarationText: "width: 5;",
      },
      {
        name: "background",
        value: "red",
        priority: "",
        offsets: [45, 61],
        declarationText: "background: red;",
      },
    ],
  },

  // Embedded comment where the parsing heuristic is a bit funny.
  {
    parseComments: true,
    input: "width: 5; /* background: */ background: red;",
    expected: [
      {
        name: "width",
        value: "5",
        priority: "",
        offsets: [0, 9],
        declarationText: "width: 5;",
      },
      {
        name: "background",
        value: "",
        priority: "",
        offsets: [13, 24],
        commentOffsets: [10, 27],
      },
      {
        name: "background",
        value: "red",
        priority: "",
        offsets: [28, 44],
        declarationText: "background: red;",
      },
    ],
  },

  // Another case where the parsing heuristic says not to bother; note
  // that there is no ";" in the comment.
  {
    parseComments: true,
    input: "width: 5; /* background: yellow */ background: red;",
    expected: [
      {
        name: "width",
        value: "5",
        priority: "",
        offsets: [0, 9],
        declarationText: "width: 5;",
      },
      {
        name: "background",
        value: "yellow",
        priority: "",
        offsets: [13, 31],
        declarationText: "background: yellow",
        commentOffsets: [10, 34],
      },
      {
        name: "background",
        value: "red",
        priority: "",
        offsets: [35, 51],
        declarationText: "background: red;",
      },
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
        declarationText: "content: '*\\/';",
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
        declarationText: "content: '*\\/'",
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
        declarationText: "color: /\\* comment *\\/ red;",
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
        declarationText: "walrus: zebra;",
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
        declarationText: "color: blue \\9 no\\_need",
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
        declarationText: "stroke-dasharray: 1/*ThisIsAComment*/2;",
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
        declarationText: "\u00a0vertical-align: top",
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

  /***** Testing nested rules *****/

  // Testing basic nesting with tagname selector
  {
    input: `
      color: red;
      div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with tagname + pseudo selector
  {
    input: `
      color: red;
      div:hover {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with id selector
  {
    input: `
      color: red;
      #myEl {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with class selector
  {
    input: `
      color: red;
      .myEl {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with & + ident selector
  {
    input: `
      color: red;
      & div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with direct child selector
  {
    input: `
      color: red;
      > div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with & and :hover pseudo-class selector
  {
    input: `
      color: red;
      &:hover {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with :not pseudo-class selector and non-leading &
  {
    input: `
      color: red;
      :not(&) {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with attribute selector
  {
    input: `
      color: red;
      [class] {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with & compound selector
  {
    input: `
      color: red;
      &div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with relative + selector
  {
    input: `
      color: red;
      + div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with relative ~ selector
  {
    input: `
      color: red;
      ~ div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting with relative * selector
  {
    input: `
      color: red;
      * div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing basic nesting after a property with !important
  {
    input: `
      color: red !important;
      & div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "important",
        offsets: [7, 29],
        declarationText: "color: red !important;",
      },
    ],
  },

  // Testing basic nesting after a comment
  {
    input: `
      color: red;
      /* nested rules */
      & div {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing at-rules (with condition) nesting
  {
    input: `
      color: red;
      @media (orientation: landscape) {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing at-rules (without condition) nesting
  {
    input: `
      color: red;
      @media screen {
        background: blue;
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing multi-level nesting
  {
    input: `
      color: red;
      &div {
        &.active {
          border: 1px;
        }
        padding: 10px;
      }
      background: gold;
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
      {
        name: "background",
        value: "gold",
        priority: "",
        offsets: [121, 138],
        declarationText: "background: gold;",
      },
    ],
  },

  // Testing multi-level nesting with at-rules
  {
    input: `
      color: red;
      @layer {
        background: yellow;
        @media screen {
          & {
            border-color: blue;
          }
        }
      }
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
    ],
  },

  // Testing sibling nested rules
  {
    input: `
      color: red;
      .active {
        border: 1px;
      }
      &div {
        padding: 10px;
      }
      border-color: cyan
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
      {
        name: "border-color",
        value: "cyan",
        priority: "",
        offsets: [114, 132],
        declarationText: "border-color: cyan",
      },
    ],
  },

  // Testing nesting interwined between property declarations
  {
    input: `
      color: red;
      .active {
        border: 1px;
      }
      background: gold;
      &div {
        padding: 10px;
      }
      border-color: cyan
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
      {
        name: "background",
        value: "gold",
        priority: "",
        offsets: [70, 87],
        declarationText: "background: gold;",
      },
      {
        name: "border-color",
        value: "cyan",
        priority: "",
        offsets: [138, 156],
        declarationText: "border-color: cyan",
      },
    ],
  },

  // Testing that "}" in content property does not impact the nested state
  {
    input: `
      color: red;
      &div {
        content: "}"
        color: blue;
      }
      background: gold;
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
      {
        name: "background",
        value: "gold",
        priority: "",
        offsets: [88, 105],
        declarationText: "background: gold;",
      },
    ],
  },

  // Testing that "}" in attribute selector does not impact the nested state
  {
    input: `
      color: red;
      + .foo {
        [class="}"] {
          padding: 10px;
        }
      }
      background: gold;
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
      {
        name: "background",
        value: "gold",
        priority: "",
        offsets: [105, 122],
        declarationText: "background: gold;",
      },
    ],
  },

  // Testing that in function does not impact the nested state
  {
    input: `
      color: red;
      + .foo {
        background: url("img.png?x=}")
      }
      background: gold;
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
      {
        name: "background",
        value: "gold",
        priority: "",
        offsets: [87, 104],
        declarationText: "background: gold;",
      },
    ],
  },

  // Testing that "}" in comment does not impact the nested state
  {
    input: `
      color: red;
      + .foo {
        /* Check } */
        padding: 10px;
      }
      background: gold;
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
      },
      {
        name: "background",
        value: "gold",
        priority: "",
        offsets: [93, 110],
        declarationText: "background: gold;",
      },
    ],
  },

  // Testing that nested rules in comments aren't reported
  {
    parseComments: true,
    input: "width: 5; /* div { color: cyan; } */ background: red;",
    expected: [
      {
        name: "width",
        value: "5",
        priority: "",
        offsets: [0, 9],
        declarationText: "width: 5;",
      },
      {
        name: "background",
        value: "red",
        priority: "",
        offsets: [37, 53],
        declarationText: "background: red;",
      },
    ],
  },

  // Testing that declarations in comments are still handled while nested rule in same comment is ignored
  {
    parseComments: true,
    input:
      "width: 5; /* padding: 12px; div { color: cyan; } margin: 1em; */ background: red;",
    expected: [
      {
        name: "width",
        value: "5",
        priority: "",
        offsets: [0, 9],
        declarationText: "width: 5;",
      },
      {
        name: "padding",
        value: "12px",
        priority: "",
        offsets: [13, 27],
        declarationText: "padding: 12px;",
      },
      {
        name: "margin",
        value: "1em",
        priority: "",
        offsets: [49, 61],
        declarationText: "margin: 1em;",
      },
      {
        name: "background",
        value: "red",
        priority: "",
        offsets: [65, 81],
        declarationText: "background: red;",
      },
    ],
  },

  // Testing nesting without closing bracket
  {
    input: `
      color: red;
      & div {
        background: blue;
    `,
    expected: [
      {
        name: "color",
        value: "red",
        priority: "",
        offsets: [7, 18],
        declarationText: "color: red;",
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
      assertOutput(test.input, output, test.expected);
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

function assertOutput(input, actual, expected) {
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

      if (expected[i].declarationText) {
        Assert.equal(
          input.substring(expected[i].offsets[0], expected[i].offsets[1]),
          expected[i].declarationText
        );
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
