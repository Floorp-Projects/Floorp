/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

requestLongerTimeout(4);

// Test that markup view event bubbles show the correct event info for React
// events (React development version 15.4.1) using JSX.

const TEST_LIB = URL_ROOT_SSL + "lib_react_dom_15.4.1.js";
const TEST_LIB_BABEL = URL_ROOT_SSL + "lib_babel_6.21.0_min.js";
const TEST_EXTERNAL_LISTENERS = URL_ROOT_SSL + "react_external_listeners.js";
const TEST_URL =
  URL_ROOT_SSL + "doc_markup_events_react_development_15.4.1_jsx.html";

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "#inlinejsx",
    expected: [
      {
        type: "click",
        filename: TEST_LIB + ":17530:42",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: `function emptyFunction() {}`
      },
      {
        type: "onClick",
        filename: TEST_LIB_BABEL + ":10:41",
        attributes: [
          "Bubbling",
          "React"
        ],
        handler: `
          function inlineFunction() {
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
        filename: TEST_LIB + ":17530:42",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: `function emptyFunction() {}`
      },
      {
        type: "onClick",
        filename: TEST_EXTERNAL_LISTENERS + ":4:25",
        attributes: [
          "Bubbling",
          "React"
        ],
        handler: `
          function externalFunction() {
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
        filename: TEST_LIB + ":17530:42",
        attributes: [
          "Bubbling",
          "DOM2"
        ],
        handler: `function emptyFunction() {}`
      },
      {
        type: "onClick",
        filename: TEST_EXTERNAL_LISTENERS + ":4:25",
        attributes: [
          "Bubbling",
          "React"
        ],
        handler: `
          function externalFunction() {
            alert("externalFunction");
          }`
      },
      {
        type: "onMouseUp",
        filename: TEST_LIB_BABEL + ":10:41",
        attributes: [
          "Bubbling",
          "React"
        ],
        handler: `
          function inlineFunction() {
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
        filename: TEST_EXTERNAL_LISTENERS + ":8:34",
        attributes: [
          "Capturing",
          "React"
        ],
        handler: `
          function externalCapturingFunction() {
            alert("externalCapturingFunction");
          }`
      }
    ]
  }
];
/* eslint-enable */

add_task(async function() {
  info(
    "Switch to 2 pane inspector to avoid sidebar width issues with opening events"
  );
  await pushPref("devtools.inspector.three-pane-enabled", false);
  await pushPref("devtools.toolsidebar-width.inspector", 350);
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
