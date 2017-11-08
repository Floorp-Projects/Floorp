"use strict";

/**
 * WHOA THERE: We should never be adding new things to EXPECTED_*_REFLOWS. This
 * is a whitelist that should slowly go away as we improve the performance of
 * the front-end. Instead of adding more reflows to the whitelist, you should
 * be modifying your code to avoid the reflow.
 *
 * See https://developer.mozilla.org/en-US/Firefox/Performance_best_practices_for_Firefox_fe_engineers
 * for tips on how to do that.
 */
const EXPECTED_OVERFLOW_REFLOWS = [
  {
    stack: [
      "select@chrome://global/content/bindings/textbox.xml",
      "focusAndSelectUrlBar@chrome://browser/content/browser.js",
      "_adjustFocusAfterTabSwitch@chrome://browser/content/tabbrowser.xml",
    ]
  },
];

const EXPECTED_UNDERFLOW_REFLOWS = [
  /**
   * Nothing here! Please don't add anything new!
   */
];

/**
 * This test ensures that there are no unexpected uninterruptible reflows when
 * opening a new tab that will cause the existing tabs to overflow and the tab
 * strip to become scrollable. It also tests that there are no unexpected
 * uninterruptible reflows when closing that tab, which causes the tab strip to
 * underflow.
 */
add_task(async function() {
  await ensureNoPreloadedBrowser();

  const TAB_COUNT_FOR_OVERFLOW = computeMaxTabCount();

  await createTabs(TAB_COUNT_FOR_OVERFLOW);

  await withReflowObserver(async function(dirtyFrame) {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    BrowserOpenTab();
    await BrowserTestUtils.waitForEvent(gBrowser.selectedTab, "transitionend",
        false, e => e.propertyName === "max-width");
    await switchDone;
    await BrowserTestUtils.waitForCondition(() => {
      return gBrowser.tabContainer.arrowScrollbox.hasAttribute("scrolledtoend");
    });
  }, EXPECTED_OVERFLOW_REFLOWS, window);

  await withReflowObserver(async function() {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    let transitionPromise =
      BrowserTestUtils.waitForEvent(gBrowser.selectedTab,
                                    "transitionend", false,
                                    e => e.propertyName === "max-width");
    await BrowserTestUtils.removeTab(gBrowser.selectedTab, { animate: true });
    await transitionPromise;
    await switchDone;
  }, EXPECTED_UNDERFLOW_REFLOWS, window);

  await removeAllButFirstTab();
});
