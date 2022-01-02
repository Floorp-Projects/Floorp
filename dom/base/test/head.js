async function newFocusedWindow(trigger) {
  let winPromise = BrowserTestUtils.domWindowOpenedAndLoaded();
  let delayedStartupPromise = BrowserTestUtils.waitForNewWindow();

  await trigger();

  let win = await winPromise;
  // New windows get focused after the first paint, see bug 1262946
  await BrowserTestUtils.waitForContentEvent(
    win.gBrowser.selectedBrowser,
    "MozAfterPaint"
  );
  await delayedStartupPromise;
  return win;
}
