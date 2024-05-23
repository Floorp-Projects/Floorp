/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Ensure that we don't use an entirely-blank (non-printable) document title
 * as the tab label.
 */
add_task(async function test_ensure_printable_label() {
  const TEST_DOC = `
  <!DOCTYPE html>
  <meta charset="utf-8">
  <!-- Title is NO-BREAK SPACE, COMBINING ACUTE ACCENT, ARABIC LETTER MARK -->
  <title>&nbsp;&%23x0301;&%23x061C;</title>
  Is my title blank?`;

  let newTab;
  function tabLabelChecker() {
    Assert.stringMatches(
      newTab.label,
      /\p{L}|\p{N}|\p{P}|\p{S}/u,
      "Tab label should contain printable character."
    );
  }
  let mutationObserver = new MutationObserver(tabLabelChecker);
  registerCleanupFunction(() => mutationObserver.disconnect());

  gBrowser.tabContainer.addEventListener(
    "TabOpen",
    event => {
      newTab = event.target;
      tabLabelChecker();
      mutationObserver.observe(newTab, {
        attributeFilter: ["label"],
      });
    },
    { once: true }
  );

  await BrowserTestUtils.withNewTab("data:text/html," + TEST_DOC, async () => {
    // Wait another longer-than-tick to ensure more mutation observer things have
    // come in.
    await new Promise(executeSoon);

    // Check one last time for good measure, for the final label:
    tabLabelChecker();
  });
});
