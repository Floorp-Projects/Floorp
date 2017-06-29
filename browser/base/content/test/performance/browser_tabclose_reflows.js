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
  /**
   * Nothing here! Please don't add anything new!
   */
];

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when closing new tabs.
 */
add_task(async function() {
  await ensureNoPreloadedBrowser();

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  await BrowserTestUtils.waitForCondition(() => tab._fullyOpen);

  // Add a reflow observer and open a new tab.
  await withReflowObserver(async function() {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    gBrowser.removeTab(tab, { animate: true });
    await BrowserTestUtils.waitForEvent(tab, "transitionend",
        false, e => e.propertyName === "max-width");
    await switchDone;
  }, EXPECTED_REFLOWS);
  is(EXPECTED_REFLOWS.length, 0, "No reflows are expected when closing a tab");
});
