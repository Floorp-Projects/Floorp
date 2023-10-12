/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */

async function detachTab(tab) {
  let winPromise = BrowserTestUtils.waitForNewWindow();
  info("Detaching tab");
  let win = gBrowser.replaceTabWithWindow(tab, {});
  info("Waiting for new window");
  await winPromise;

  // Wait an extra tick for good measure since the code itself also waits for
  // `delayedStartupPromise`.
  info("Waiting for delayed startup in new window");
  await win.delayedStartupPromise;
  info("Waiting for tick");
  await TestUtils.waitForTick();

  return win;
}

add_task(async function detach() {
  // After detaching a tab into a new window, the input value in the new window
  // should be formatted.

  // Sometimes the value isn't formatted on Mac when running in verify chaos
  // mode. The usual, proper front-end code path is hit, and the path that
  // removes formatting is not hit, so it seems like some kind of race in the
  // editor or selection code in Gecko. Since this has only been observed on Mac
  // in chaos mode and doesn't seem to be a problem in urlbar code, skip the
  // test in that case.
  if (AppConstants.platform == "macosx" && Services.env.get("MOZ_CHAOSMODE")) {
    Assert.ok(true, "Skipping test in chaos mode on Mac");
    return;
  }

  let originalTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "https://example.com/original-tab",
  });

  let tabToDetach = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "https://example.com/detach",
  });

  let win = await detachTab(tabToDetach);

  UrlbarTestUtils.checkFormatting(
    win,
    UrlbarTestUtils.trimURL("<https://>example.com</detach>")
  );
  await BrowserTestUtils.closeWindow(win);

  UrlbarTestUtils.checkFormatting(
    window,
    UrlbarTestUtils.trimURL("<https://>example.com</original-tab>")
  );
  gBrowser.removeTab(originalTab);
});

add_task(async function detach_emptyTab() {
  // After detaching an empty tab into a new window, the input value in the
  // original window should be formatted.

  let originalTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "https://example.com/original-tab",
  });

  let emptyTab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    url: "about:blank",
  });
  gURLBar.focus();
  ok(gURLBar.focused, "urlbar is focused");
  is(gURLBar.value, "", "urlbar is empty");

  let focusPromise = BrowserTestUtils.waitForEvent(
    originalTab.linkedBrowser,
    "focus"
  );
  let win = await detachTab(emptyTab);
  await BrowserTestUtils.closeWindow(win);
  await focusPromise;

  ok(!gURLBar.focused, "urlbar is not focused");
  UrlbarTestUtils.checkFormatting(
    window,
    UrlbarTestUtils.trimURL("<https://>example.com</original-tab>")
  );
  gBrowser.removeTab(originalTab);
});
