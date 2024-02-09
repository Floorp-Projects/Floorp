/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

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

    const initialHeight = window.outerHeight;
    const initialWidth = window.outerWidth;
    const initialMaxRenderCount = rootVirtualList.maxRenderCountEstimate;
    info(`The initial maxRenderCountEstimate is ${initialMaxRenderCount}`);
    info(`The initial innerHeight is ${window.innerHeight}`);

    // Resize window with new height value
    const newHeight = 540;
    window.resizeTo(initialWidth, newHeight);
    await TestUtils.waitForCondition(
      () => window.outerHeight >= newHeight,
      `The window has been resized with outer height of ${window.outerHeight} instead of ${newHeight}.`
    );
    await TestUtils.waitForCondition(
      () =>
        rootVirtualList.updateComplete &&
        rootVirtualList.maxRenderCountEstimate < initialMaxRenderCount,
      `Max render count ${rootVirtualList.maxRenderCountEstimate} is not less than initial max render count ${initialMaxRenderCount}`
    );
    const newMaxRenderCount = rootVirtualList.maxRenderCountEstimate;

    Assert.strictEqual(
      rootVirtualList.maxRenderCountEstimate,
      newMaxRenderCount,
      `The maxRenderCountEstimate on the virtual-list is now ${newMaxRenderCount}`
    );

    // Restore initial window size
    resizeTo(initialWidth, initialHeight);
    await TestUtils.waitForCondition(
      () =>
        window.outerWidth >= initialHeight && window.outerWidth >= initialWidth,
      `The window has been resized with outer height of ${window.outerHeight} instead of ${initialHeight}.`
    );
    info(`The final innerHeight is ${window.innerHeight}`);
    await TestUtils.waitForCondition(
      () =>
        rootVirtualList.updateComplete &&
        rootVirtualList.maxRenderCountEstimate > newMaxRenderCount,
      `Max render count ${rootVirtualList.maxRenderCountEstimate} is not greater than new max render count ${newMaxRenderCount}`
    );

    info(
      `The maxRenderCountEstimate on the virtual-list is greater than ${newMaxRenderCount} after window resize`
    );
    gBrowser.removeTab(gBrowser.selectedTab);
  });
});
