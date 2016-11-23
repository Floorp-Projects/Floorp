/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for DOM
// events.

const TEST_URL = URL_ROOT + "doc_markup_events3.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [ // eslint-disable-line
  {
    selector: "#es6-method",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":91",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "es6Method() {\n" +
                 "  alert(\"obj.es6Method\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#generator",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":96",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function* generator() {\n" +
                 "  alert(\"generator\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#anon-generator",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":55",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function*() {\n" +
                 "  alert(\"anonGenerator\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#named-function-expression",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":23",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "let namedFunctionExpression =\n" +
                 "  function foo() {\n" +
                 "    alert(\"namedFunctionExpression\");\n" +
                 "  }"
      }
    ]
  },
  {
    selector: "#anon-function-expression",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":27",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "let anonFunctionExpression = function() {\n" +
                 "  alert(\"anonFunctionExpression\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#returned-function",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":32",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function bar() {\n" +
                 "  alert(\"returnedFunction\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#constructed-function",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":0",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: ""
      }
    ]
  },
  {
    selector: "#constructed-function-with-body-string",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":0",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "alert(\"constructedFuncWithBodyString\");"
      }
    ]
  },
  {
    selector: "#multiple-assignment",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":42",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "let multipleAssignment = foo = bar = function multi() {\n" +
                 "  alert(\"multipleAssignment\");\n" +
                 "}"
      }
    ]
  },
];

add_task(function* () {
  yield runEventPopupTests(TEST_URL, TEST_DATA);
});
