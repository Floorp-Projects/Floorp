/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

"use strict";

// Test that markup view chrome event bubbles are hidden when
// devtools.chrome.enabled = false.

const TEST_URL = URL_ROOT + "doc_markup_events_chrome_listeners.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "div",
    expected: [ ],
  },
];

add_task(async function() {
  await pushPref("devtools.chrome.enabled", false);

  const {tab, inspector, testActor} = await openInspectorForURL(TEST_URL);
  const browser = tab.linkedBrowser;
  const mm = browser.messageManager;

  await mm.loadFrameScript(
    `data:,const div = content.document.querySelector("div");` +
    `div.addEventListener("click", () => {` +
    ` /* Do nothing */` +
    `});`, false);

  await inspector.markup.expandAll();

  for (const test of TEST_DATA) {
    await checkEventsForNode(test, inspector, testActor);
  }
});
