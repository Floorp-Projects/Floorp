/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */
const EXPECTED_REFLOWS = [
  // selection change notification may cause querying the focused editor content
  // by IME and that will cause reflow.
  {
    stack: [
      "select@chrome://global/content/bindings/textbox.xml",
    ],
  }
];

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening new tabs.
 */
add_task(async function() {
  await ensureNoPreloadedBrowser();

  // Add a reflow observer and open a new tab.
  await withReflowObserver(async function() {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    BrowserOpenTab();
    await BrowserTestUtils.waitForEvent(gBrowser.selectedTab, "transitionend",
        false, e => e.propertyName === "max-width");
    await switchDone;
  }, EXPECTED_REFLOWS);

  let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
  await BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await switchDone;
});
