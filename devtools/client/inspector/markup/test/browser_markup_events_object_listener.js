/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles show the correct event info for object
// style event listeners and that no bubbles are shown for objects without any
// handleEvent method.

const TEST_URL = URL_ROOT + "doc_markup_events_object_listener.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [ // eslint-disable-line
  {
    selector: "#valid-object-listener",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":17",
        attributes: ["Bubbling", "DOM2"],
        handler: `() => {\n` + `  console.log("handleEvent");\n` + `}`,
      },
    ],
  },
  {
    selector: "#valid-invalid-object-listeners",
    expected: [
      {
        type: "click",
        filename: TEST_URL + ":24",
        attributes: ["Bubbling", "DOM2"],
        handler: `() => {\n` + `  console.log("handleEvent");\n` + `}`,
      },
    ],
  },
];

add_task(async function() {
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
