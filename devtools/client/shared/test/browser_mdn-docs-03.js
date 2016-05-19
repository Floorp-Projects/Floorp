/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the CSS syntax highlighter in the MdnDocsWidget object.
 *
 * The CSS syntax highlighter accepts:
 * - a string containing CSS
 * - a DOM node
 *
 * It parses the string and creates a collection of DOM nodes for different
 * CSS token types. These DOM nodes have CSS classes applied to them,
 * to apply the right style for that particular token type. The DOM nodes
 * are returned as children of the node that was passed to the function.
 *
 * This test code defines a number of different strings containing valid and
 * invalid CSS in various forms. For each string it defines the DOM nodes
 * that it expects to get from the syntax highlighter.
 *
 * It then calls the syntax highlighter, and checks that the resulting
 * collection of DOM nodes is what we expected.
 */

"use strict";

const {appendSyntaxHighlightedCSS} = require("devtools/client/shared/widgets/MdnDocsWidget");

/**
 * An array containing the actual test cases.
 *
 * The test code tests every case in the array. If you want to add more
 * test cases, just add more items to the array.
 *
 * Each test case consists of:
 * - description: string describing the salient features of this test case
 * - example: the string to test
 * - expected: an array of objects, one for each DOM node we expect, that
 * captures the information about the node that we expect to test.
 */
const TEST_DATA = [{
  description: "Valid syntax, string value.",
  example: "name: stringValue;",
  expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, numeric value.",
    example: "name: 1;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "1"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, url value.",
    example: "name: url(./name);",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "url(./name)"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, space before ':'.",
    example: "name : stringValue;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: " "},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, space before ';'.",
    example: "name: stringValue ;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: " "},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, trailing space.",
    example: "name: stringValue; ",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"},
             {type: "text", text: " "}
  ]}, {
    description: "Valid syntax, leading space.",
    example: " name: stringValue;",
    expected: [{type: "text", text: " "},
             {type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, two spaces.",
    example: "name:  stringValue;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: "  "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, no spaces.",
    example: "name:stringValue;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, two-part value.",
    example: "name: stringValue 1;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: " "},
             {type: "property-value", text: "1"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, two declarations.",
    example: "name: stringValue;\n" +
           "name: 1;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"},
             {type: "text", text: "\n"},
             {type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "1"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, commented, numeric value.",
    example: "/* comment */\n" +
           "name: 1;",
    expected: [{type: "comment", text: "/* comment */"},
             {type: "text", text: "\n"},
             {type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "1"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, multiline commented, string value.",
    example: "/* multiline \n" +
           "comment */\n" +
           "name: stringValue;",
    expected: [{type: "comment", text: "/* multiline \ncomment */"},
             {type: "text", text: "\n"},
             {type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, commented, two declarations.",
    example: "/* comment 1 */\n" +
           "name: 1;\n" +
           "/* comment 2 */\n" +
           "name: stringValue;",
    expected: [{type: "comment", text: "/* comment 1 */"},
             {type: "text", text: "\n"},
             {type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "1"},
             {type: "text", text: ";"},
             {type: "text", text: "\n"},
             {type: "comment", text: "/* comment 2 */"},
             {type: "text", text: "\n"},
             {type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, multiline.",
    example: "name: \n" +
           "stringValue;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " \n"},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Valid syntax, multiline, two declarations.",
    example: "name: \n" +
           "stringValue \n" +
           "stringValue2;",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " \n"},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: " \n"},
             {type: "property-value", text: "stringValue2"},
             {type: "text", text: ";"}
  ]}, {
    description: "Invalid: not CSS at all.",
    example: "not CSS at all",
    expected: [{type: "property-name", text: "not"},
             {type: "text", text: " "},
             {type: "property-name", text: "CSS"},
             {type: "text", text: " "},
             {type: "property-name", text: "at"},
             {type: "text", text: " "},
             {type: "property-name", text: "all"}
  ]}, {
    description: "Invalid: switched ':' and ';'.",
    example: "name; stringValue:",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ";"},
             {type: "text", text: " "},
             {type: "property-name", text: "stringValue"},
             {type: "text", text: ":"}
  ]}, {
    description: "Invalid: unterminated comment.",
    example: "/* unterminated comment\n" +
           "name: stringValue;",
    expected: [{type: "comment", text: "/* unterminated comment\nname: stringValue;"}
  ]}, {
    description: "Invalid: bad comment syntax.",
    example: "// invalid comment\n" +
           "name: stringValue;",
    expected: [{type: "text", text: "/"},
             {type: "text", text: "/"},
             {type: "text", text: " "},
             {type: "property-name", text: "invalid"},
             {type: "text", text: " "},
             {type: "property-name", text: "comment"},
             {type: "text", text: "\n"},
             {type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: ";"}
  ]}, {
    description: "Invalid: no trailing ';'.",
    example: "name: stringValue\n" +
           "name: stringValue2",
    expected: [{type: "property-name", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue"},
             {type: "text", text: "\n"},
             {type: "property-value", text: "name"},
             {type: "text", text: ":"},
             {type: "text", text: " "},
             {type: "property-value", text: "stringValue2"},
  ]}
];

/**
 * Iterate through every test case, calling the syntax highlighter,
 * then calling a helper function to check the output.
 */
add_task(function* () {
  let doc = gBrowser.selectedTab.ownerDocument;
  let parent = doc.createElement("div");
  info("Testing all CSS syntax highlighter test cases");
  for (let {description, example, expected} of TEST_DATA) {
    info("Testing: " + description);
    appendSyntaxHighlightedCSS(example, parent);
    checkCssSyntaxHighlighterOutput(expected, parent);
    while (parent.firstChild) {
      parent.firstChild.remove();
    }
  }
});
