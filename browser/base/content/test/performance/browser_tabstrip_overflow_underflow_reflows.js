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

  Assert.ok(gBrowser.tabContainer.hasAttribute("overflow"),
            "Tabs should now be overflowed.");

  // Now test that opening and closing a tab while overflowed doesn't cause
  // us to reflow.
  await withReflowObserver(async function(dirtyFrame) {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    BrowserOpenTab();
    await switchDone;
    await BrowserTestUtils.waitForCondition(() => {
      return gBrowser.tabContainer.arrowScrollbox.hasAttribute("scrolledtoend");
    });
  }, [], window);

  await withReflowObserver(async function(dirtyFrame) {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    await BrowserTestUtils.removeTab(gBrowser.selectedTab, { animate: true });
    await switchDone;
  }, [], window);

  // At this point, we have an overflowed tab strip, and we've got the last tab
  // selected. This should mean that the first tab is scrolled out of view.
  // Let's test that we don't reflow when switching to that first tab.
  let lastTab = gBrowser.selectedTab;
  let arrowScrollbox = gBrowser.tabContainer.arrowScrollbox;

  // First, we'll check that the first tab is actually scrolled out of view.
  let firstTab = gBrowser.tabContainer.firstChild;
  let firstTabRect = firstTab.getBoundingClientRect();
  Assert.ok(firstTabRect.left + firstTabRect.width < arrowScrollbox.scrollPosition,
            "First tab should be scrolled out of view.");

  // Now switch to the first tab. We shouldn't flush layout at all.
  await withReflowObserver(async function(dirtyFrame) {
    await BrowserTestUtils.switchTab(gBrowser, firstTab);
    await BrowserTestUtils.waitForCondition(() => {
      return gBrowser.tabContainer.arrowScrollbox.hasAttribute("scrolledtostart");
    });
  }, [], window);

  // Okay, now close the last tab. The tabstrip should stay overflowed, but removing
  // one more after that should underflow it.
  await BrowserTestUtils.removeTab(lastTab);

  Assert.ok(gBrowser.tabContainer.hasAttribute("overflow"),
            "Tabs should still be overflowed.");

  lastTab = gBrowser.tabContainer.lastElementChild;
  await BrowserTestUtils.switchTab(gBrowser, lastTab);

  // ... and make sure we don't flush layout when closing it, and exiting
  // the overflowed state.
  await withReflowObserver(async function() {
    let switchDone = BrowserTestUtils.waitForEvent(window, "TabSwitchDone");
    await BrowserTestUtils.removeTab(lastTab, { animate: true });
    await switchDone;
  }, EXPECTED_UNDERFLOW_REFLOWS, window);

  Assert.ok(!gBrowser.tabContainer.hasAttribute("overflow"),
            "Tabs should no longer be overflowed.");

  await removeAllButFirstTab();
});
