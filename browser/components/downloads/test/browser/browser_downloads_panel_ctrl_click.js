/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_downloads_panel() {
  // On macOS, ctrl-click shouldn't open the panel because this normally opens
  // the context menu. This happens via the `contextmenu` event which is created
  // by widget code, so our simulated clicks do not do so, so we can't test
  // anything on macOS.
  if (AppConstants.platform == "macosx") {
    ok(true, "The test is ignored on Mac");
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [["browser.download.autohideButton", false]],
  });
  await promiseButtonShown("downloads-button");

  const button = document.getElementById("downloads-button");
  let shownPromise = promisePanelOpened();
  // Should still open the panel when Ctrl key is pressed.
  EventUtils.synthesizeMouseAtCenter(button, { ctrlKey: true });
  await shownPromise;
  is(DownloadsPanel.panel.state, "open", "Check that panel state is 'open'");

  // Close download panel
  DownloadsPanel.hidePanel();
  is(
    DownloadsPanel.panel.state,
    "closed",
    "Check that panel state is 'closed'"
  );
});
