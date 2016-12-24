/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

requestLongerTimeout(4);

// Test that markup view event bubbles show the correct event info for React
// events (React production version 15.3.1) using JSX.

const TEST_LIB = URL_ROOT + "lib_react_with_addons_15.3.1_min.js";
const TEST_EXTERNAL_LISTENERS = URL_ROOT + "react_external_listeners.js";
const TEST_URL = URL_ROOT + "doc_markup_events_react_production_15.3.1_jsx.html";

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "#inlinejsx",
    expected: [
      {
        type: "click",
        filename: TEST_LIB + ":16",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function() {}"
      },
      {
        type: "onClick",
        filename: TEST_URL + ":10",
        attributes: [
          "Bubbling",
          "React"
        ],
        handler:
`function() {
  alert("inlineFunction");
}`
      }
    ]
  },
  {
    selector: "#externaljsx",
    expected: [
      {
        type: "click",
        filename: TEST_LIB + ":16",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function() {}"
      },
      {
        type: "onClick",
        filename: TEST_EXTERNAL_LISTENERS + ":4",
        attributes: [
          "Bubbling",
          "React"
        ],
        handler:
`function externalFunction() {
  alert("externalFunction");
}`
      }
    ]
  },
  {
    selector: "#externalinlinejsx",
    expected: [
      {
        type: "click",
        filename: TEST_LIB + ":16",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: "function() {}"
      },
      {
        type: "onClick",
        filename: TEST_EXTERNAL_LISTENERS + ":4",
        attributes: [
          "Bubbling",
          "React"
        ],
        handler:
`function externalFunction() {
  alert("externalFunction");
}`
      },
      {
        type: "onMouseUp",
        filename: TEST_URL + ":10",
        attributes: [
          "Bubbling",
          "React"
        ],
        handler:
`function() {
  alert("inlineFunction");
}`
      }
    ]
  },
  {
    selector: "#externalcapturingjsx",
    expected: [
      {
        type: "onClickCapture",
        filename: TEST_EXTERNAL_LISTENERS + ":8",
        attributes: [
          "Capturing",
          "React"
        ],
        handler:
`function externalCapturingFunction() {
  alert("externalCapturingFunction");
}`
      }
    ]
  }
];
/*eslint-enable */

add_task(function* () {
  yield runEventPopupTests(TEST_URL, TEST_DATA);
});
