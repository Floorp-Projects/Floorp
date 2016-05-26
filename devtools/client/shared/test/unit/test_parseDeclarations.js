/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cu = Components.utils;
const {require} = Cu.import("resource://devtools/shared/Loader.jsm", {});
const {parseDeclarations, _parseCommentDeclarations} = require("devtools/shared/css-parsing-utils");

const TEST_DATA = [
  // Simple test
  {
    input: "p:v;",
    expected: [{name: "p", value: "v", priority: "", offsets: [0, 4]}]
  },
  // Simple test
  {
    input: "this:is;a:test;",
    expected: [
      {name: "this", value: "is", priority: "", offsets: [0, 8]},
      {name: "a", value: "test", priority: "", offsets: [8, 15]}
    ]
  },
  // Test a single declaration with semi-colon
  {
    input: "name:value;",
    expected: [{name: "name", value: "value", priority: "", offsets: [0, 11]}]
  },
  // Test a single declaration without semi-colon
  {
    input: "name:value",
    expected: [{name: "name", value: "value", priority: "", offsets: [0, 10]}]
  },
  // Test multiple declarations separated by whitespaces and carriage
  // returns and tabs
  {
    input: "p1 : v1 ; \t\t  \n p2:v2;   \n\n\n\n\t  p3    :   v3;",
    expected: [
      {name: "p1", value: "v1", priority: "", offsets: [0, 9]},
      {name: "p2", value: "v2", priority: "", offsets: [16, 22]},
      {name: "p3", value: "v3", priority: "", offsets: [32, 45]},
    ]
  },
  // Test simple priority
  {
    input: "p1: v1; p2: v2 !important;",
    expected: [
      {name: "p1", value: "v1", priority: "", offsets: [0, 7]},
      {name: "p2", value: "v2", priority: "important", offsets: [8, 26]}
    ]
  },
  // Test simple priority
  {
    input: "p1: v1 !important; p2: v2",
    expected: [
      {name: "p1", value: "v1", priority: "important", offsets: [0, 18]},
      {name: "p2", value: "v2", priority: "", offsets: [19, 25]}
    ]
  },
  // Test simple priority
  {
    input: "p1: v1 !  important; p2: v2 ! important;",
    expected: [
      {name: "p1", value: "v1", priority: "important", offsets: [0, 20]},
      {name: "p2", value: "v2", priority: "important", offsets: [21, 40]}
    ]
  },
  // Test invalid priority
  {
    input: "p1: v1 important;",
    expected: [
      {name: "p1", value: "v1 important", priority: "", offsets: [0, 17]}
    ]
  },
  // Test various types of background-image urls
  {
    input: "background-image: url(../../relative/image.png)",
    expected: [{
      name: "background-image",
      value: "url(../../relative/image.png)",
      priority: "",
      offsets: [0, 47]
    }]
  },
  {
    input: "background-image: url(http://site.com/test.png)",
    expected: [{
      name: "background-image",
      value: "url(http://site.com/test.png)",
      priority: "",
      offsets: [0, 47]
    }]
  },
  {
    input: "background-image: url(wow.gif)",
    expected: [{
      name: "background-image",
      value: "url(wow.gif)",
      priority: "",
      offsets: [0, 30]
    }]
  },
  // Test that urls with :;{} characters in them are parsed correctly
  {
    input: "background: red url(\"http://site.com/image{}:;.png?id=4#wat\") "
      + "repeat top right",
    expected: [{
      name: "background",
      value: "red url(\"http://site.com/image{}:;.png?id=4#wat\") " +
        "repeat top right",
      priority: "",
      offsets: [0, 78]
    }]
  },
  // Test that an empty string results in an empty array
  {input: "", expected: []},
  // Test that a string comprised only of whitespaces results in an empty array
  {input: "       \n \n   \n   \n \t  \t\t\t  ", expected: []},
  // Test that a null input throws an exception
  {input: null, throws: true},
  // Test that a undefined input throws an exception
  {input: undefined, throws: true},
  // Test that :;{} characters in quoted content are not parsed as multiple
  // declarations
  {
    input: "content: \";color:red;}selector{color:yellow;\"",
    expected: [{
      name: "content",
      value: "\";color:red;}selector{color:yellow;\"",
      priority: "",
      offsets: [0, 45]
    }]
  },
  // Test that rules aren't parsed, just declarations. So { and } found after a
  // property name should be part of the property name, same for values.
  {
    input: "body {color:red;} p {color: blue;}",
    expected: [
      {name: "body {color", value: "red", priority: "", offsets: [0, 16]},
      {name: "} p {color", value: "blue", priority: "", offsets: [16, 33]},
      {name: "}", value: "", priority: "", offsets: [33, 34]}
    ]
  },
  // Test unbalanced : and ;
  {
    input: "color :red : font : arial;",
    expected: [
      {name: "color", value: "red : font : arial", priority: "",
       offsets: [0, 26]}
    ]
  },
  {
    input: "background: red;;;;;",
    expected: [{name: "background", value: "red", priority: "",
                offsets: [0, 16]}]
  },
  {
    input: "background:;",
    expected: [{name: "background", value: "", priority: "",
                offsets: [0, 12]}]
  },
  {input: ";;;;;", expected: []},
  {input: ":;:;", expected: []},
  // Test name only
  {input: "color", expected: [
    {name: "color", value: "", priority: "", offsets: [0, 5]}
  ]},
  // Test trailing name without :
  {input: "color:blue;font", expected: [
    {name: "color", value: "blue", priority: "", offsets: [0, 11]},
    {name: "font", value: "", priority: "", offsets: [11, 15]}
  ]},
  // Test trailing name with :
  {input: "color:blue;font:", expected: [
    {name: "color", value: "blue", priority: "", offsets: [0, 11]},
    {name: "font", value: "", priority: "", offsets: [11, 16]}
  ]},
  // Test leading value
  {input: "Arial;color:blue;", expected: [
    {name: "", value: "Arial", priority: "", offsets: [0, 6]},
    {name: "color", value: "blue", priority: "", offsets: [6, 17]}
  ]},
  // Test hex colors
  {
    input: "color: #333",
    expected: [{name: "color", value: "#333", priority: "", offsets: [0, 11]}]
  },
  {
    input: "color: #456789",
    expected: [{name: "color", value: "#456789", priority: "",
                offsets: [0, 14]}]
  },
  {
    input: "wat: #XYZ",
    expected: [{name: "wat", value: "#XYZ", priority: "", offsets: [0, 9]}]
  },
  // Test string/url quotes escaping
  {
    input: "content: \"this is a 'string'\"",
    expected: [{name: "content", value: "\"this is a 'string'\"", priority: "",
                offsets: [0, 29]}]
  },
  {
    input: 'content: "this is a \\"string\\""',
    expected: [{
      name: "content",
      value: '"this is a \\"string\\""',
      priority: "",
      offsets: [0, 31]}]
  },
  {
    input: "content: 'this is a \"string\"'",
    expected: [{
      name: "content",
      value: '\'this is a "string"\'',
      priority: "",
      offsets: [0, 29]
    }]
  },
  {
    input: "content: 'this is a \\'string\\''",
    expected: [{
      name: "content",
      value: "'this is a \\'string\\''",
      priority: "",
      offsets: [0, 31],
    }]
  },
  {
    input: "content: 'this \\' is a \" really strange string'",
    expected: [{
      name: "content",
      value: "'this \\' is a \" really strange string'",
      priority: "",
      offsets: [0, 47]
    }]
  },
  {
    input: "content: \"a not s\\\
          o very long title\"",
    expected: [
      {name: "content", value: '"a not s\\\
          o very long title"', priority: "", offsets: [0, 46]}
    ]
  },
  // Test calc with nested parentheses
  {
    input: "width: calc((100% - 3em) / 2)",
    expected: [{name: "width", value: "calc((100% - 3em) / 2)", priority: "",
                offsets: [0, 29]}]
  },

  // Simple embedded comment test.
  {
    parseComments: true,
    input: "width: 5; /* background: green; */ background: red;",
    expected: [{name: "width", value: "5", priority: "", offsets: [0, 9]},
               {name: "background", value: "green", priority: "",
                offsets: [13, 31], commentOffsets: [10, 34]},
               {name: "background", value: "red", priority: "",
                offsets: [35, 51]}]
  },

  // Embedded comment where the parsing heuristic fails.
  {
    parseComments: true,
    input: "width: 5; /* background something: green; */ background: red;",
    expected: [{name: "width", value: "5", priority: "", offsets: [0, 9]},
               {name: "background", value: "red", priority: "",
                offsets: [45, 61]}]
  },

  // Embedded comment where the parsing heuristic is a bit funny.
  {
    parseComments: true,
    input: "width: 5; /* background: */ background: red;",
    expected: [{name: "width", value: "5", priority: "", offsets: [0, 9]},
               {name: "background", value: "", priority: "",
                offsets: [13, 24], commentOffsets: [10, 27]},
               {name: "background", value: "red", priority: "",
                offsets: [28, 44]}]
  },

  // Another case where the parsing heuristic says not to bother; note
  // that there is no ";" in the comment.
  {
    parseComments: true,
    input: "width: 5; /* background: yellow */ background: red;",
    expected: [{name: "width", value: "5", priority: "", offsets: [0, 9]},
               {name: "background", value: "yellow", priority: "",
                offsets: [13, 31], commentOffsets: [10, 34]},
               {name: "background", value: "red", priority: "",
                offsets: [35, 51]}]
  },

  // Parsing a comment should yield text that has been unescaped, and
  // the offsets should refer to the original text.
  {
    parseComments: true,
    input: "/* content: '*\\/'; */",
    expected: [{name: "content", value: "'*/'", priority: "",
                offsets: [3, 18], commentOffsets: [0, 21]}]
  },

  // Parsing a comment should yield text that has been unescaped, and
  // the offsets should refer to the original text.  This variant
  // tests the no-semicolon path.
  {
    parseComments: true,
    input: "/* content: '*\\/' */",
    expected: [{name: "content", value: "'*/'", priority: "",
                offsets: [3, 17], commentOffsets: [0, 20]}]
  },

  // A comment-in-a-comment should yield the correct offsets.
  {
    parseComments: true,
    input: "/* color: /\\* comment *\\/ red; */",
    expected: [{name: "color", value: "red", priority: "",
                offsets: [3, 30], commentOffsets: [0, 33]}]
  },

  // HTML comments are ignored.
  {
    parseComments: true,
    input: "<!-- color: red; --> color: blue;",
    expected: [{name: "color", value: "red", priority: "",
                offsets: [5, 16]},
               {name: "color", value: "blue", priority: "",
                offsets: [21, 33]}]
  },

  // Don't error on an empty comment.
  {
    parseComments: true,
    input: "/**/",
    expected: []
  },

  // Parsing our special comments skips the name-check heuristic.
  {
    parseComments: true,
    input: "/*! walrus: zebra; */",
    expected: [{name: "walrus", value: "zebra", priority: "",
                offsets: [4, 18], commentOffsets: [0, 21]}]
  }
];

function run_test() {
  run_basic_tests();
  run_comment_tests();
}

// Test parseDeclarations.
function run_basic_tests() {
  for (let test of TEST_DATA) {
    do_print("Test input string " + test.input);
    let output;
    try {
      output = parseDeclarations(test.input, test.parseComments);
    } catch (e) {
      do_print("parseDeclarations threw an exception with the given input " +
        "string");
      if (test.throws) {
        do_print("Exception expected");
        do_check_true(true);
      } else {
        do_print("Exception unexpected\n" + e);
        do_check_true(false);
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
    expected: [{name: "content", value: "'hi", priority: "", terminator: "';",
                offsets: [2, 14], colonOffsets: [9, 11],
                commentOffsets: [0, 16]}],
  },
  {
    input: "text that once confounded the parser;",
    expected: []
  },
];

// Test parseCommentDeclarations.
function run_comment_tests() {
  for (let test of COMMENT_DATA) {
    do_print("Test input string " + test.input);
    let output = _parseCommentDeclarations(test.input, 0,
                                           test.input.length + 4);
    deepEqual(output, test.expected);
  }
}

function assertOutput(actual, expected) {
  if (actual.length === expected.length) {
    for (let i = 0; i < expected.length; i++) {
      do_check_true(!!actual[i]);
      do_print("Check that the output item has the expected name, " +
        "value and priority");
      do_check_eq(expected[i].name, actual[i].name);
      do_check_eq(expected[i].value, actual[i].value);
      do_check_eq(expected[i].priority, actual[i].priority);
      deepEqual(expected[i].offsets, actual[i].offsets);
      if ("commentOffsets" in expected[i]) {
        deepEqual(expected[i].commentOffsets, actual[i].commentOffsets);
      }
    }
  } else {
    for (let prop of actual) {
      do_print("Actual output contained: {name: " + prop.name + ", value: " +
        prop.value + ", priority: " + prop.priority + "}");
    }
    do_check_eq(actual.length, expected.length);
  }
}
