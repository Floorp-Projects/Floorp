/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */
"use strict";

requestLongerTimeout(4);

// Test that markup view event bubbles show the correct event info for React
// events (React production version 16.2.0) without JSX.

const TEST_LIB = URL_ROOT_SSL + "lib_react_dom_16.2.0_min.js";
const TEST_EXTERNAL_LISTENERS = URL_ROOT_SSL + "react_external_listeners.js";
const TEST_URL =
  URL_ROOT_SSL + "doc_markup_events_react_production_16.2.0.html";

loadHelperScript("helper_events_test_runner.js");

/*eslint-disable */
const TEST_DATA = [
  {
    selector: "#inline",
    expected: [
      {
        type: "click",
        filename: TEST_LIB + ":93:417",
        attributes: ["Bubbling"],
        handler: `function() {}`,
      },
      {
        type: "onClick",
        filename: TEST_URL + ":21:22",
        attributes: ["React", "Bubbling"],
        handler: `
          inlineFunction() {
            alert("inlineFunction");
          }`,
      },
    ],
  },
  {
    selector: "#external",
    expected: [
      {
        type: "click",
        filename: TEST_LIB + ":93:417",
        attributes: ["Bubbling"],
        handler: `function() {}`,
      },
      {
        type: "onClick",
        filename: TEST_EXTERNAL_LISTENERS + ":4:25",
        attributes: ["React", "Bubbling"],
        handler: `
          function externalFunction() {
            alert("externalFunction");
          }`,
      },
    ],
  },
  {
    selector: "#externalinline",
    expected: [
      {
        type: "click",
        filename: TEST_LIB + ":93:417",
        attributes: ["Bubbling"],
        handler: `function() {}`,
      },
      {
        type: "onClick",
        filename: TEST_EXTERNAL_LISTENERS + ":4:25",
        attributes: ["React", "Bubbling"],
        handler: `
          function externalFunction() {
            alert("externalFunction");
          }`,
      },
      {
        type: "onMouseUp",
        filename: TEST_URL + ":21:22",
        attributes: ["React", "Bubbling"],
        handler: `
          inlineFunction() {
            alert("inlineFunction");
          }`,
      },
    ],
  },
  {
    selector: "#externalcapturing",
    expected: [
      {
        type: "onClickCapture",
        filename: TEST_EXTERNAL_LISTENERS + ":8:34",
        attributes: ["React", "Capturing"],
        handler: `
          function externalCapturingFunction() {
            alert("externalCapturingFunction");
          }`,
      },
    ],
  },
  {
    selector: "#doublebind",
    expected: [
      {
        type: "click",
        filename: TEST_LIB + ":93:417",
        attributes: ["Bubbling"],
        handler: `function() {}`,
      },
      {
        type: "onClick",
        filename: TEST_URL + ":21:22",
        attributes: ["React", "Bubbling"],
        handler: `
          function() {
            alert("inlineFunction");
          }`,
      },
    ],
  },
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
