/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function init() {
  // We use an extension that shows a page action so that we can test the
  // "remove extension" item in the context menu.
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      name: "Test contextMenu",
      page_action: { show_matches: ["<all_urls>"] },
    },

    useAddonManager: "temporary",
  });

  await extension.startup();
  let actionId = ExtensionCommon.makeWidgetId(extension.id);

  let win = await BrowserTestUtils.openNewBrowserWindow();
  await SimpleTest.promiseFocus(win);
  let originalWindowWidth = win.outerWidth;
  Assert.greater(originalWindowWidth, 700, "window is bigger than 700px");
  BrowserTestUtils.loadURI(win.gBrowser, "data:text/html,<h1>A Page</h1>");
  await BrowserTestUtils.browserLoaded(win.gBrowser.selectedBrowser);

  // The pageAction implementation enables the button at the next animation
  // frame, so before we look for the button we should wait one animation frame
  // as well.
  await promiseAnimationFrame(win);

  registerCleanupFunction(async () => {
    win.resizeTo(originalWindowWidth, win.outerHeight);
    await BrowserTestUtils.closeWindow(win);
  });

  info("Check page action buttons are visible, the meatball button is not");
  let addonButton = win.BrowserPageActions.urlbarButtonNodeForActionID(
    actionId
  );
  Assert.ok(BrowserTestUtils.is_visible(addonButton));
  let starButton = win.BrowserPageActions.urlbarButtonNodeForActionID(
    "bookmark"
  );
  Assert.ok(BrowserTestUtils.is_visible(starButton));
  let meatballButton = win.document.getElementById("pageActionButton");
  Assert.ok(!BrowserTestUtils.is_visible(meatballButton));

  info(
    "Shrink the window, check page action buttons are not visible, the meatball menu is visible"
  );
  await promiseStableResize(500, win);
  Assert.ok(!BrowserTestUtils.is_visible(addonButton));
  Assert.ok(!BrowserTestUtils.is_visible(starButton));
  Assert.ok(BrowserTestUtils.is_visible(meatballButton));

  info(
    "Remove the extension, check the only page action button is visible, the meatball menu is not visible"
  );
  let promiseUninstalled = promiseAddonUninstalled(extension.id);
  await extension.unload();
  await promiseUninstalled;
  Assert.ok(BrowserTestUtils.is_visible(starButton));
  Assert.ok(!BrowserTestUtils.is_visible(meatballButton));
  Assert.deepEqual(
    win.BrowserPageActions.urlbarButtonNodeForActionID(actionId),
    null
  );
});

// TODO (Bug 1700780): Why is this necessary? Without this trick the test
// fails intermittently on Ubuntu.
function promiseStableResize(expectedWidth, win = window) {
  let deferred = PromiseUtils.defer();
  let id;
  function listener() {
    win.clearTimeout(id);
    info(`Got resize event: ${win.innerWidth} x ${win.innerHeight}`);
    if (win.innerWidth <= expectedWidth) {
      id = win.setTimeout(() => {
        win.removeEventListener("resize", listener);
        deferred.resolve();
      }, 100);
    }
  }
  win.addEventListener("resize", listener);
  win.resizeTo(expectedWidth, win.outerHeight);
  return deferred.promise;
}
