/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This test verifies that when a tab is dragged out to a
// new window, the tab is not in a modal state (bug 1754759)
add_task(async function () {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://www.example.com"
  );
  let tab2 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com/browser/browser/components/tabbrowser/test/browser/tabs/tab_that_opens_dialog.html"
  );

  let delayedStartupPromise = BrowserTestUtils.waitForNewWindow();
  let win = gBrowser.replaceTabWithWindow(tab2);
  await delayedStartupPromise;

  await SpecialPowers.spawn(win.gBrowser.selectedBrowser, [], () => {
    ok(
      !content.windowUtils.isInModalState(),
      "content should not be in modal state"
    );
  });

  await BrowserTestUtils.closeWindow(win);
  BrowserTestUtils.removeTab(tab1);
});
