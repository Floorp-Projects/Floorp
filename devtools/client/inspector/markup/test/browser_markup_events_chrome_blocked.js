/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view chrome event bubbles are hidden when
// devtools.chrome.enabled = false.

const TEST_URL = URL_ROOT + "doc_markup_events_chrome_listeners.html";
const FRAMESCRIPT_URL = `data:,(${frameScript.toString()})()`;

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "div",
    expected: [],
  },
];

add_task(async function() {
  waitForExplicitFinish();
  await pushPref("devtools.chrome.enabled", false);

  const { tab, inspector, testActor } = await openInspectorForURL(TEST_URL);
  const browser = tab.linkedBrowser;
  const mm = browser.messageManager;

  const badgeEventAdded = inspector.markup.once("badge-added-event");

  info("Loading frame script");
  mm.loadFrameScript(`${FRAMESCRIPT_URL}`, false);

  // We need to check that the "badge-added-event" event is not triggered so we
  // need to wait for 5 seconds here.
  const result = await awaitWithTimeout(badgeEventAdded, 3000);
  is(result, "timeout", "Ensure that no event badges were added");

  for (const test of TEST_DATA) {
    await checkEventsForNode(test, inspector, testActor);
  }
});

function frameScript() {
  const div = content.document.querySelector("div");
  div.addEventListener("click", () => {
    /* Do nothing */
  });
}
