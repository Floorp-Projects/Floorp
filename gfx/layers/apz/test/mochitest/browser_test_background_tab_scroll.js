add_task(async function test_main() {
  // Load page in the background. This will cause the first-paint of the
  // tab (which has ScrollPositionUpdate instances) to get sent to the
  // compositor, but the parent process RefLayer won't be pointing to this
  // tab so APZ never sees the ScrollPositionUpdate instances.

  let url =
    "http://mochi.test:8888/browser/gfx/layers/apz/test/mochitest/helper_background_tab_scroll.html#scrolltarget";
  let backgroundTab = BrowserTestUtils.addTab(gBrowser, url);
  let browser = backgroundTab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser, false, url);
  dump("Done loading background tab\n");

  // Switch to the foreground, to let the APZ tree get built.
  await BrowserTestUtils.switchTab(gBrowser, backgroundTab);
  dump("Switched background tab to foreground\n");

  // Verify main-thread scroll position is where we expect
  let scrollPos = await ContentTask.spawn(browser, null, function() {
    return content.window.scrollY;
  });
  is(scrollPos, 5000, "Expected background tab to be at scroll pos 5000");

  // Trigger an APZ-side scroll via native wheel event, followed by some code
  // to ensure APZ's repaint requests to arrive at the main-thread. If
  // things are working properly, the main thread will accept the repaint
  // requests and update the main-thread scroll position. If the APZ side
  // is sending incorrect scroll generations in the repaint request, then
  // the main thread will fail to clear the main-thread scroll origin (which
  // was set by the scroll to the #scrolltarget anchor), and so will not
  // accept APZ's scroll position updates.
  let contentScrollFunction = async function() {
    await content.window.wrappedJSObject.promiseNativeWheelAndWaitForWheelEvent(
      content.window,
      100,
      100,
      0,
      200
    );

    // Advance some/all frames of the APZ wheel animation
    let utils = content.window.SpecialPowers.getDOMWindowUtils(content.window);
    for (var i = 0; i < 10; i++) {
      utils.advanceTimeAndRefresh(16);
    }
    utils.restoreNormalRefresh();
    // Flush pending APZ repaints, then read the main-thread scroll
    // position
    await content.window.wrappedJSObject.promiseApzRepaintsFlushed(
      content.window
    );
    return content.window.scrollY;
  };
  scrollPos = await ContentTask.spawn(browser, null, contentScrollFunction);

  // Verify main-thread scroll position has changed
  ok(
    scrollPos < 5000,
    `Expected background tab to have scrolled up, is at ${scrollPos}`
  );

  // Cleanup
  let tabClosed = BrowserTestUtils.waitForTabClosing(backgroundTab);
  BrowserTestUtils.removeTab(backgroundTab);
  await tabClosed;
});
