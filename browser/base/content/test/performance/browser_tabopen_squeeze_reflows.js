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
    "select@chrome://global/content/bindings/textbox.xml",
    "focusAndSelectUrlBar@chrome://browser/content/browser.js",
    "_adjustFocusAfterTabSwitch@chrome://browser/content/tabbrowser.xml",
  ],
];

/*
 * This test ensures that there are no unexpected
 * uninterruptible reflows when opening a new tab that will
 * cause the existing tabs to squeeze smaller.
 */
add_task(async function() {
  await ensureNoPreloadedBrowser();

  // Compute the number of tabs we can put into the strip without
  // overflowing, and remove one, so that we can create
  // TAB_COUNT_FOR_SQUEEE tabs, and then one more, which should
  // cause the tab to squeeze to a smaller size rather than overflow.
  const TAB_COUNT_FOR_SQUEEZE = computeMaxTabCount() - 1;

  await createTabs(TAB_COUNT_FOR_SQUEEZE);

  // Because the tab strip is a scrollable frame, we can't use the
  // default dirtying function from withReflowObserver and reliably
  // get reflows for the strip. Instead, we provide a node that's
  // already in the scrollable frame to dirty - in this case, the
  // original tab.
  let origTab = gBrowser.selectedTab;

  await withReflowObserver(async function() {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    BrowserOpenTab();
    await BrowserTestUtils.waitForEvent(gBrowser.selectedTab, "transitionend",
      false, e => e.propertyName === "max-width");
    await switchDone;
  }, EXPECTED_REFLOWS, window, origTab);

  await removeAllButFirstTab();
});
