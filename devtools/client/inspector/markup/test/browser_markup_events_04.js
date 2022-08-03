/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for DOM
// events.

const TEST_URL = URL_ROOT_SSL + "doc_markup_events_04.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "html",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":59:67",
        attributes: ["Bubbling"],
        handler:
          "function(foo2, bar2) {\n" +
          '  alert("documentElement event listener clicked");\n' +
          "}",
      },
      {
        type: "click",
        filename: TEST_URL + ":55:51",
        attributes: ["Bubbling"],
        handler:
          "function(foo, bar) {\n" +
          '  alert("document event listener clicked");\n' +
          "}",
      },
      {
        type: "load",
        filename: TEST_URL,
        attributes: ["Bubbling"],
        handler: "function onload(event) {\n" + "  init();\n" + "}",
      },
    ],
  },
  {
    selector: "#constructed-function",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":1:0",
        attributes: ["Bubbling"],
        handler: "function anonymous() {\n" + "\n" + "}",
      },
    ],
  },
  {
    selector: "#constructed-function-with-body-string",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":1:0",
        attributes: ["Bubbling"],
        handler:
          "function anonymous(a, b, c) {\n" +
          '  alert("constructedFuncWithBodyString");\n' +
          "}",
      },
    ],
  },
  {
    selector: "#multiple-assignment",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":26:47",
        attributes: ["Bubbling"],
        handler:
          "function multi() {\n" + '  alert("multipleAssignment");\n' + "}",
      },
    ],
  },
  {
    selector: "#promise",
    expected: [
      {
        type: "click",
        filename: "[native code]",
        attributes: ["Bubbling"],
        handler: "function() {\n" + "  [native code]\n" + "}",
      },
    ],
  },
  {
    selector: "#math-pow",
    expected: [
      {
        type: "click",
        filename: "[native code]",
        attributes: ["Bubbling"],
        handler: "function pow(, ) {\n" + "  [native code]\n" + "}",
      },
    ],
  },
  {
    selector: "#handleEvent",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":81:29",
        attributes: ["Bubbling"],
        handler:
          "function(event) {\n" +
          "  switch (event.type) {\n" +
          '    case "click":\n' +
          '      alert("handleEvent click");\n' +
          "  }\n" +
          "}",
      },
    ],
  },
];

add_task(async function() {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
