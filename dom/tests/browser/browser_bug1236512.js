/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

const testPageURL =
  "http://mochi.test:8888/browser/dom/tests/browser/dummy.html";

async function testContentVisibilityState(aIsHidden, aBrowser) {
  await SpecialPowers.spawn(
    aBrowser.selectedBrowser,
    [aIsHidden],
    aExpectedResult => {
      is(content.document.hidden, aExpectedResult, "document.hidden");
      is(
        content.document.visibilityState,
        aExpectedResult ? "hidden" : "visible",
        "document.visibilityState"
      );
    }
  );
}

async function waitContentVisibilityChange(aIsHidden, aBrowser) {
  await SpecialPowers.spawn(
    aBrowser.selectedBrowser,
    [aIsHidden],
    async function (aExpectedResult) {
      let visibilityState = aExpectedResult ? "hidden" : "visible";
      if (
        content.document.hidden === aExpectedResult &&
        content.document.visibilityState === visibilityState
      ) {
        ok(true, "already changed to expected visibility state");
        return;
      }

      info("wait visibilitychange event");
      await ContentTaskUtils.waitForEvent(
        content.document,
        "visibilitychange",
        true /* capture */,
        aEvent => {
          info(
            `visibilitychange: ${content.document.hidden} ${content.document.visibilityState}`
          );
          return (
            content.document.hidden === aExpectedResult &&
            content.document.visibilityState === visibilityState
          );
        }
      );
    }
  );
}

/**
 * This test is to test the visibility state will change to "hidden" when browser
 * window is fully covered by another non-translucent application. Note that we
 * only support this on Mac for now, other platforms don't support reporting
 * occlusion state.
 */
add_task(async function () {
  info("creating test window");
  let winTest = await BrowserTestUtils.openNewBrowserWindow();
  // Specify the width, height, left and top, so that the new window can be
  // fully covered by "window".
  let resizePromise = BrowserTestUtils.waitForEvent(
    winTest,
    "resize",
    false,
    e => {
      return winTest.innerHeight <= 500 && winTest.innerWidth <= 500;
    }
  );
  winTest.moveTo(200, 200);
  winTest.resizeTo(500, 500);
  await resizePromise;

  let browserTest = winTest.gBrowser;

  info(`loading test page: ${testPageURL}`);
  BrowserTestUtils.startLoadingURIString(
    browserTest.selectedBrowser,
    testPageURL
  );
  await BrowserTestUtils.browserLoaded(browserTest.selectedBrowser);

  info("test init visibility state");
  await testContentVisibilityState(false /* isHidden */, browserTest);

  info(
    "test window should report 'hidden' if it is fully covered by another " +
      "window"
  );
  await new Promise(resolve => waitForFocus(resolve, window));
  await waitContentVisibilityChange(true /* isHidden */, browserTest);

  info(
    "test window should still report 'hidden' since it is still fully covered " +
      "by another window"
  );
  let tab = BrowserTestUtils.addTab(browserTest);
  await BrowserTestUtils.switchTab(browserTest, tab);
  BrowserTestUtils.removeTab(browserTest.selectedTab);
  await testContentVisibilityState(true /* isHidden */, browserTest);

  info(
    "test window should report 'visible' if it is not fully covered by " +
      "another window"
  );
  await new Promise(resolve => waitForFocus(resolve, winTest));
  await waitContentVisibilityChange(false /* isHidden */, browserTest);

  info("closing test window");
  await BrowserTestUtils.closeWindow(winTest);
});
