/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for DOM
// events.

const TEST_URL = URL_ROOT + "doc_markup_events2.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [ // eslint-disable-line
  {
    selector: "#fatarrow",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":39",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "() => {\n" +
                 "  alert(\"Fat arrow without params!\");\n" +
                 "}"
      },
      {
        type: "click",
        filename: TEST_URL + ":43",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "event => {\n" +
                 "  alert(\"Fat arrow with 1 param!\");\n" +
                 "}"
      },
      {
        type: "click",
        filename: TEST_URL + ":47",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "(event, foo, bar) => {\n" +
                 "  alert(\"Fat arrow with 3 params!\");\n" +
                 "}"
      },
      {
        type: "click",
        filename: TEST_URL + ":51",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "b => b"
      }
    ]
  },
  {
    selector: "#bound",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":62",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function boundClickHandler(event) {\n" +
                 "  alert(\"Bound event\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#boundhe",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":85",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "handleEvent: function() {\n" +
                 "  alert(\"boundHandleEvent\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#comment-inline",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":91",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function functionProceededByInlineComment() {\n" +
                 "  alert(\"comment-inline\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#comment-streaming",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":96",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function functionProceededByStreamingComment() {\n" +
                 "  alert(\"comment-streaming\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#anon-object-method",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":71",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "anonObjectMethod: function() {\n" +
                 "  alert(\"obj.anonObjectMethod\");\n" +
                 "}"
      }
    ]
  },
  {
    selector: "#object-method",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":75",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "objectMethod: function kay() {\n" +
                 "  alert(\"obj.objectMethod\");\n" +
                 "}"
      }
    ]
  }
];

add_task(function* () {
  yield runEventPopupTests(TEST_URL, TEST_DATA);
});
