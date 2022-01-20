/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */
/* import-globals-from helper_events_test_runner.js */

("use strict");

// Test that markup view chrome event bubbles are shown when
// devtools.chrome.enabled = true.

const TEST_URL = URL_ROOT + "doc_markup_events_chrome_listeners.html";

loadHelperScript("helper_events_test_runner.js");

const TEST_DATA = [
  {
    selector: "div",
    expected: [
      {
        type: "click",
        filename:
          getRootDirectory(gTestPath) +
          "browser_markup_events_chrome_not_blocked.js:45:34",
        attributes: ["Bubbling", "DOM2"],
        handler: `() => {
          /* Do nothing */
        }`,
      },
    ],
  },
];

add_task(async function() {
  waitForExplicitFinish();
  await pushPref("devtools.chrome.enabled", true);

  const { tab, inspector } = await openInspectorForURL(TEST_URL);
  const browser = tab.linkedBrowser;

  const eventBadgeAdded = inspector.markup.once("badge-added-event");
  info("Loading frame script");

  await SpecialPowers.spawn(browser, [], () => {
    const div = content.document.querySelector("div");
    div.addEventListener("click", () => {
      /* Do nothing */
    });
  });
  await eventBadgeAdded;

  for (const test of TEST_DATA) {
    await checkEventsForNode(test, inspector);
  }
});
