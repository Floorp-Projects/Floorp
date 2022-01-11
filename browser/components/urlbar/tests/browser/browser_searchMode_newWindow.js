/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests immediately entering search mode in a new window and then exiting it.
// No errors should be thrown and search mode should be exited successfully.

"use strict";

add_task(async function escape() {
  await doTest(win =>
    EventUtils.synthesizeKey("KEY_Escape", { repeat: 2 }, win)
  );
});

add_task(async function backspace() {
  await doTest(win => EventUtils.synthesizeKey("KEY_Backspace", {}, win));
});

async function doTest(exitSearchMode) {
  let win = await BrowserTestUtils.openNewBrowserWindow();

  // Press accel+K to enter search mode.
  await UrlbarTestUtils.promisePopupOpen(win, () =>
    EventUtils.synthesizeKey("k", { accelKey: true }, win)
  );
  await UrlbarTestUtils.assertSearchMode(win, {
    engineName: Services.search.defaultEngine.name,
    isGeneralPurposeEngine: true,
    source: UrlbarUtils.RESULT_SOURCE.SEARCH,
    isPreview: false,
    entry: "shortcut",
  });

  // Exit search mode.
  await exitSearchMode(win);
  await UrlbarTestUtils.assertSearchMode(win, null);

  await UrlbarTestUtils.promisePopupClose(win);
  await BrowserTestUtils.closeWindow(win);
}
