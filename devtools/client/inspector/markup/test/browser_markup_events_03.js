/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for DOM
// events.

const TEST_URL = URL_ROOT_SSL + "doc_markup_events_03.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "#es6-method",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":69:17",
        attributes: ["Bubbling"],
        handler:
          "es6Method(foo, bar) {\n" + '  alert("obj.es6Method");\n' + "}",
      },
    ],
  },
  {
    selector: "#generator",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":89:25",
        attributes: ["Bubbling"],
        handler: "function* generator() {\n" + '  alert("generator");\n' + "}",
      },
    ],
  },
  {
    selector: "#anon-generator",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":46:58",
        attributes: ["Bubbling"],
        handler: "function*() {\n" + '  alert("anonGenerator");\n' + "}",
      },
    ],
  },
  {
    selector: "#named-function-expression",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":22:18",
        attributes: ["Bubbling"],
        handler:
          "function foo() {\n" + '  alert("namedFunctionExpression");\n' + "}",
      },
    ],
  },
  {
    selector: "#anon-function-expression",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":26:45",
        attributes: ["Bubbling"],
        handler:
          "function() {\n" + '  alert("anonFunctionExpression");\n' + "}",
      },
    ],
  },
  {
    selector: "#returned-function",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":31:27",
        attributes: ["Bubbling"],
        handler: "function bar() {\n" + '  alert("returnedFunction");\n' + "}",
      },
    ],
  },
];

add_task(async function() {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
