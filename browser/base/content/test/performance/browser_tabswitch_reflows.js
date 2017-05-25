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
  [
    "_adjustFocusAfterTabSwitch@chrome://browser/content/tabbrowser.xml",
  ],
];

if (gMultiProcessBrowser) {
  EXPECTED_REFLOWS.push(
    [
      "_adjustFocusAfterTabSwitch@chrome://browser/content/tabbrowser.xml",
    ]
  );
}

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when switching between two
 * tabs that are both fully visible.
 */
add_task(async function() {
  await ensureNoPreloadedBrowser();

  // Because the tab strip is a scrollable frame, we can't use the
  // default dirtying function from withReflowObserver and reliably
  // get reflows for the strip. Instead, we provide a node that's
  // already in the scrollable frame to dirty - in this case, the
  // original tab.
  let origTab = gBrowser.selectedTab;

  let firstSwitchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
  let otherTab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await firstSwitchDone;

  await withReflowObserver(async function() {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    gBrowser.selectedTab = origTab;
    await switchDone;
  }, EXPECTED_REFLOWS, window, origTab);

  await BrowserTestUtils.removeTab(otherTab);
});
