/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_appMenu_mainView() {
  // On macOS, ctrl-click shouldn't open the panel because this normally opens
  // the context menu. This happens via the `contextmenu` event which is created
  // by widget code, so our simulated clicks do not do so, so we can't test
  // anything on macOS:
  if (AppConstants.platform == "macosx") {
    ok(true, "The test is ignored on Mac");
    return;
  }

  const mainView = document.getElementById("appMenu-mainView");

  let shownPromise = BrowserTestUtils.waitForEvent(mainView, "ViewShown");
  // Should still open the panel when Ctrl key is pressed.
  EventUtils.synthesizeMouseAtCenter(PanelUI.menuButton, { ctrlKey: true });
  await shownPromise;
  ok(true, "Main menu shown after button pressed");

  // Close the main panel.
  let hiddenPromise = BrowserTestUtils.waitForEvent(document, "popuphidden");
  mainView.closest("panel").hidePopup();
  await hiddenPromise;
});

add_task(async function test_appMenu_libraryView() {
  // On macOS, ctrl-click shouldn't open the panel because this normally opens
  // the context menu. This happens via the `contextmenu` event which is created
  // by widget code, so our simulated clicks do not do so, so we can't test
  // anything on macOS:
  if (AppConstants.platform == "macosx") {
    ok(true, "The test is ignored on Mac");
    return;
  }

  const libraryView = document.getElementById("appMenu-libraryView");
  const button = document.getElementById("library-button");

  let shownPromise = BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
  // Should still open the panel when Ctrl key is pressed.
  EventUtils.synthesizeMouseAtCenter(button, { ctrlKey: true });
  await shownPromise;
  ok(true, "Library menu shown after button pressed");

  // Close the Library panel.
  let hiddenPromise = BrowserTestUtils.waitForEvent(document, "popuphidden");
  libraryView.closest("panel").hidePopup();
  await hiddenPromise;
});
