/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

const VIRTUAL_LIST_ENABLED_PREF = "browser.firefox-view.virtual-list.enabled";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [[VIRTUAL_LIST_ENABLED_PREF, true]],
  });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    clearHistory();
  });
});

add_task(async function test_max_render_count_on_win_resize() {
  const now = new Date();
  await PlacesUtils.history.insertMany([
    {
      url: "https://example.net/",
      visits: [{ date: now }],
    },
  ]);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(
      document.location.href,
      getFirefoxViewURL(),
      "Firefox View is loaded to the Recent Browsing page."
    );

    await navigateToCategoryAndWait(document, "history");

    let historyComponent = document.querySelector("view-history");
    let tabList = historyComponent.lists[0];
    let rootVirtualList = tabList.rootVirtualListEl;

    const initialHeight = window.innerHeight;
    const initialWidth = window.innerWidth;
    const initialMaxRenderCount = rootVirtualList.maxRenderCountEstimate;
    info(`The initial maxRenderCountEstimate is ${initialMaxRenderCount}`);

    // Resize window with new height value
    const newHeight = 540;
    const newMaxRenderCount = 40;
    let resizePromise = BrowserTestUtils.waitForEvent(window, "resize");
    window.resizeTo(initialWidth, newHeight);
    await resizePromise;
    info("The window has been resized with a height of 540px");

    await TestUtils.waitForCondition(
      () => rootVirtualList.maxRenderCountEstimate === newMaxRenderCount
    );

    is(
      rootVirtualList.maxRenderCountEstimate,
      newMaxRenderCount,
      `The maxRenderCountEstimate on the virtual-list is now ${newMaxRenderCount}`
    );

    // Restore initial window size
    let resizePromiseTwo = BrowserTestUtils.waitForEvent(window, "resize");
    window.resizeTo(initialWidth, initialHeight);
    await resizePromiseTwo;
    info("The window has been resized with initial dimensions");

    await TestUtils.waitForCondition(
      () => rootVirtualList.maxRenderCountEstimate === initialMaxRenderCount
    );

    is(
      rootVirtualList.maxRenderCountEstimate,
      initialMaxRenderCount,
      `The maxRenderCountEstimate on the virtual-list is restored to ${initialMaxRenderCount}`
    );
    gBrowser.removeTab(gBrowser.selectedTab);
  });
});
