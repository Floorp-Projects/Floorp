/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view event bubbles are hidden for <video> tags in the
// content process when devtools.chrome.enabled=false.
// <video> tags have 22 chrome listeners.

const TEST_URL = URL_ROOT + "doc_markup_events_chrome_listeners.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "video",
    expected: [ ],
  },
];

add_task(async function() {
  await pushPref("devtools.chrome.enabled", false);
  await runEventPopupTests(TEST_URL, TEST_DATA);
});
