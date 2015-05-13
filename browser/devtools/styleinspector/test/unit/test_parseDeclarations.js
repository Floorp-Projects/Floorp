/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const Cu = Components.utils;
Cu.import("resource://gre/modules/devtools/Loader.jsm");
const {parseDeclarations} = devtools.require("devtools/styleinspector/css-parsing-utils");

const TEST_DATA = [
  // Simple test
  {
    input: "p:v;",
    expected: [{name: "p", value: "v", priority: ""}]
  },
  // Simple test
  {
    input: "this:is;a:test;",
    expected: [
      {name: "this", value: "is", priority: ""},
      {name: "a", value: "test", priority: ""}
    ]
  },
  // Test a single declaration with semi-colon
  {
    input: "name:value;",
    expected: [{name: "name", value: "value", priority: ""}]
  },
  // Test a single declaration without semi-colon
  {
    input: "name:value",
    expected: [{name: "name", value: "value", priority: ""}]
  },
  // Test multiple declarations separated by whitespaces and carriage returns and tabs
  {
    input: "p1 : v1 ; \t\t  \n p2:v2;   \n\n\n\n\t  p3    :   v3;",
    expected: [
      {name: "p1", value: "v1", priority: ""},
      {name: "p2", value: "v2", priority: ""},
      {name: "p3", value: "v3", priority: ""},
    ]
  },
  // Test simple priority
  {
    input: "p1: v1; p2: v2 !important;",
    expected: [
      {name: "p1", value: "v1", priority: ""},
      {name: "p2", value: "v2", priority: "important"}
    ]
  },
  // Test simple priority
  {
    input: "p1: v1 !important; p2: v2",
    expected: [
      {name: "p1", value: "v1", priority: "important"},
      {name: "p2", value: "v2", priority: ""}
    ]
  },
  // Test simple priority
  {
    input: "p1: v1 !  important; p2: v2 ! important;",
    expected: [
      {name: "p1", value: "v1", priority: "important"},
      {name: "p2", value: "v2", priority: "important"}
    ]
  },
  // Test invalid priority
  {
    input: "p1: v1 important;",
    expected: [
      {name: "p1", value: "v1 important", priority: ""}
    ]
  },
  // Test various types of background-image urls
  {
    input: "background-image: url(../../relative/image.png)",
    expected: [{name: "background-image", value: "url(../../relative/image.png)", priority: ""}]
  },
  {
    input: "background-image: url(http://site.com/test.png)",
    expected: [{name: "background-image", value: "url(http://site.com/test.png)", priority: ""}]
  },
  {
    input: "background-image: url(wow.gif)",
    expected: [{name: "background-image", value: "url(wow.gif)", priority: ""}]
  },
  // Test that urls with :;{} characters in them are parsed correctly
  {
    input: "background: red url(\"http://site.com/image{}:;.png?id=4#wat\") repeat top right",
    expected: [
      {name: "background", value: "red url(\"http://site.com/image{}:;.png?id=4#wat\") repeat top right", priority: ""}
    ]
  },
  // Test that an empty string results in an empty array
  {input: "", expected: []},
  // Test that a string comprised only of whitespaces results in an empty array
  {input: "       \n \n   \n   \n \t  \t\t\t  ", expected: []},
  // Test that a null input throws an exception
  {input: null, throws: true},
  // Test that a undefined input throws an exception
  {input: undefined, throws: true},
  // Test that :;{} characters in quoted content are not parsed as multiple declarations
  {
    input: "content: \";color:red;}selector{color:yellow;\"",
    expected: [
      {name: "content", value: "\";color:red;}selector{color:yellow;\"", priority: ""}
    ]
  },
  // Test that rules aren't parsed, just declarations. So { and } found after a
  // property name should be part of the property name, same for values.
  {
    input: "body {color:red;} p {color: blue;}",
    expected: [
      {name: "body {color", value: "red", priority: ""},
      {name: "} p {color", value: "blue", priority: ""},
      {name: "}", value: "", priority: ""}
    ]
  },
  // Test unbalanced : and ;
  {
    input: "color :red : font : arial;",
    expected : [
      {name: "color", value: "red : font : arial", priority: ""}
    ]
  },
  {input: "background: red;;;;;", expected: [{name: "background", value: "red", priority: ""}]},
  {input: "background:;", expected: [{name: "background", value: "", priority: ""}]},
  {input: ";;;;;", expected: []},
  {input: ":;:;", expected: []},
  // Test name only
  {input: "color", expected: [
    {name: "color", value: "", priority: ""}
  ]},
  // Test trailing name without :
  {input: "color:blue;font", expected: [
    {name: "color", value: "blue", priority: ""},
    {name: "font", value: "", priority: ""}
  ]},
  // Test trailing name with :
  {input: "color:blue;font:", expected: [
    {name: "color", value: "blue", priority: ""},
    {name: "font", value: "", priority: ""}
  ]},
  // Test leading value
  {input: "Arial;color:blue;", expected: [
    {name: "", value: "Arial", priority: ""},
    {name: "color", value: "blue", priority: ""}
  ]},
  // Test hex colors
  {input: "color: #333", expected: [{name: "color", value: "#333", priority: ""}]},
  {input: "color: #456789", expected: [{name: "color", value: "#456789", priority: ""}]},
  {input: "wat: #XYZ", expected: [{name: "wat", value: "#XYZ", priority: ""}]},
  // Test string/url quotes escaping
  {input: "content: \"this is a 'string'\"", expected: [{name: "content", value: "\"this is a 'string'\"", priority: ""}]},
  {input: 'content: "this is a \\"string\\""', expected: [{name: "content", value: '"this is a \\"string\\""', priority: ""}]},
  {input: "content: 'this is a \"string\"'", expected: [{name: "content", value: '\'this is a "string"\'', priority: ""}]},
  {input: "content: 'this is a \\'string\\''", expected: [{name: "content", value: "'this is a \\'string\\''", priority: ""}]},
  {input: "content: 'this \\' is a \" really strange string'", expected: [{name: "content", value: "'this \\' is a \" really strange string'", priority: ""}]},
  {
    input: "content: \"a not s\\\
          o very long title\"",
    expected: [
      {name: "content", value: '"a not s\\\
          o very long title"', priority: ""}
    ]
  },
  // Test calc with nested parentheses
  {input: "width: calc((100% - 3em) / 2)", expected: [{name: "width", value: "calc((100% - 3em) / 2)", priority: ""}]},
];

function run_test() {
  for (let test of TEST_DATA) {
    do_print("Test input string " + test.input);
    let output;
    try {
      output = parseDeclarations(test.input);
    } catch (e) {
      do_print("parseDeclarations threw an exception with the given input string");
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

function assertOutput(actual, expected) {
  if (actual.length === expected.length) {
    for (let i = 0; i < expected.length; i ++) {
      do_check_true(!!actual[i]);
      do_print("Check that the output item has the expected name, value and priority");
      do_check_eq(expected[i].name, actual[i].name);
      do_check_eq(expected[i].value, actual[i].value);
      do_check_eq(expected[i].priority, actual[i].priority);
    }
  } else {
    for (let prop of actual) {
      do_print("Actual output contained: {name: "+prop.name+", value: "+prop.value+", priority: "+prop.priority+"}");
    }
    do_check_eq(actual.length, expected.length);
  }
}
