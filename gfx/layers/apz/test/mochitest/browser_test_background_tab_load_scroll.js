Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_utils.js",
  this
);

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/gfx/layers/apz/test/mochitest/apz_test_native_event_utils.js",
  this
);

add_task(async function test_main() {
  // Open a specific page in a background tab, then switch to the tab, check if
  // the visual and layout scroll offsets have diverged.
  // Then change to another tab so it's background again. Then reload it. Then
  // change back to it and check again if the visual and layout scroll offsets
  // have diverged.
  // The page has a couple important properties to trigger the bug. We need to
  // be restoring a non-zero scroll position so that we call ScrollToImpl with
  // origin restore so that we do not set the visual viewport offset. We then
  // need to call ScrollToImpl with a origin that does not get clobber by apz
  // so that we (wrongly) set the visual viewport offset.

  requestLongerTimeout(2);

  async function twoRafsInContent(browser) {
    await SpecialPowers.spawn(browser, [], async function () {
      await new Promise(r =>
        content.requestAnimationFrame(() => content.requestAnimationFrame(r))
      );
    });
  }

  async function waitForApzInContent(browser) {
    await SpecialPowers.spawn(browser, [], async () => {
      await content.wrappedJSObject.waitUntilApzStable();
      await content.wrappedJSObject.promiseApzFlushedRepaints();
    });
  }

  async function checkScrollPosInContent(browser, iter, num) {
    let visualScrollPos = await SpecialPowers.spawn(browser, [], function () {
      const offsetX = {};
      const offsetY = {};
      SpecialPowers.getDOMWindowUtils(content).getVisualViewportOffset(
        offsetX,
        offsetY
      );
      return offsetY.value;
    });

    let scrollPos = await SpecialPowers.spawn(browser, [], function () {
      return content.window.scrollY;
    });

    // When this fails the difference is at least 10000.
    ok(
      Math.abs(scrollPos - visualScrollPos) < 2,
      "expect scroll position and visual scroll position to be the same: visual " +
        visualScrollPos +
        " scroll " +
        scrollPos +
        " (" +
        iter +
        "," +
        num +
        ")"
    );
  }

  for (let i = 0; i < 5; i++) {
    let blankurl = "about:blank";
    let blankTab = BrowserTestUtils.addTab(gBrowser, blankurl);
    let blankbrowser = blankTab.linkedBrowser;
    await BrowserTestUtils.browserLoaded(blankbrowser, false, blankurl);

    let url =
      "http://mochi.test:8888/browser/gfx/layers/apz/test/mochitest/helper_background_tab_load_scroll.html";
    let backgroundTab = BrowserTestUtils.addTab(gBrowser, url);
    let browser = backgroundTab.linkedBrowser;
    await BrowserTestUtils.browserLoaded(browser, false, url);
    dump("Done loading background tab\n");

    await twoRafsInContent(browser);

    // Switch to the foreground.
    await BrowserTestUtils.switchTab(gBrowser, backgroundTab);
    dump("Switched background tab to foreground\n");

    await waitForApzInContent(browser);

    await checkScrollPosInContent(browser, i, 1);

    await BrowserTestUtils.switchTab(gBrowser, blankTab);

    browser.reload();
    await BrowserTestUtils.browserLoaded(browser, false, url);

    await twoRafsInContent(browser);

    // Switch to the foreground.
    await BrowserTestUtils.switchTab(gBrowser, backgroundTab);
    dump("Switched background tab to foreground\n");

    await waitForApzInContent(browser);

    await checkScrollPosInContent(browser, i, 2);

    // Cleanup
    let tabClosed = BrowserTestUtils.waitForTabClosing(backgroundTab);
    BrowserTestUtils.removeTab(backgroundTab);
    await tabClosed;

    let blanktabClosed = BrowserTestUtils.waitForTabClosing(blankTab);
    BrowserTestUtils.removeTab(blankTab);
    await blanktabClosed;
  }
});
