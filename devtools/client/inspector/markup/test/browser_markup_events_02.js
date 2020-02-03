/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for DOM
// events.

const TEST_URL = URL_ROOT + "doc_markup_events_02.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "#fatarrow",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":42:43",
        attributes: ["Bubbling", "DOM2"],
        handler: "() => {\n" + '  alert("Fat arrow without params!");\n' + "}",
      },
      {
        type: "click",
        filename: TEST_URL + ":46:43",
        attributes: ["Bubbling", "DOM2"],
        handler: "event => {\n" + '  alert("Fat arrow with 1 param!");\n' + "}",
      },
      {
        type: "click",
        filename: TEST_URL + ":50:43",
        attributes: ["Bubbling", "DOM2"],
        handler:
          "(event, foo, bar) => {\n" +
          '  alert("Fat arrow with 3 params!");\n' +
          "}",
      },
      {
        type: "click",
        filename: TEST_URL + ":54:43",
        attributes: ["Bubbling", "DOM2"],
        handler: "b => b",
      },
    ],
  },
  {
    selector: "#bound",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":65:32",
        attributes: ["Bubbling", "DOM2"],
        handler: "function(event) {\n" + '  alert("Bound event");\n' + "}",
      },
    ],
  },
  {
    selector: "#boundhe",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":88:29",
        attributes: ["Bubbling", "DOM2"],
        handler: "function() {\n" + '  alert("boundHandleEvent");\n' + "}",
      },
    ],
  },
  {
    selector: "#comment-inline",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":94:47",
        attributes: ["Bubbling", "DOM2"],
        handler:
          "function functionProceededByInlineComment() {\n" +
          '  alert("comment-inline");\n' +
          "}",
      },
    ],
  },
  {
    selector: "#comment-streaming",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":99:50",
        attributes: ["Bubbling", "DOM2"],
        handler:
          "function functionProceededByStreamingComment() {\n" +
          '  alert("comment-streaming");\n' +
          "}",
      },
    ],
  },
  {
    selector: "#anon-object-method",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":74:34",
        attributes: ["Bubbling", "DOM2"],
        handler: "function() {\n" + '  alert("obj.anonObjectMethod");\n' + "}",
      },
    ],
  },
  {
    selector: "#object-method",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":78:34",
        attributes: ["Bubbling", "DOM2"],
        handler: "function kay() {\n" + '  alert("obj.objectMethod");\n' + "}",
      },
    ],
  },
];

add_task(async function() {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
