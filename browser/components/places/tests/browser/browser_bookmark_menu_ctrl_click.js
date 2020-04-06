/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function onPopupEvent(popup, evt) {
  let fullEvent = "popup" + evt;
  return BrowserTestUtils.waitForEvent(popup, fullEvent, false, e => {
    return e.target == popup;
  });
}

add_task(async function test_bookmarks_menu() {
  // On macOS, ctrl-click shouldn't open the panel because this normally opens
  // the context menu. This happens via the `contextmenu` event which is created
  // by widget code, so our simulated clicks do not do so, so we can't test
  // anything on macOS.
  if (AppConstants.platform == "macosx") {
    ok(true, "The test is ignored on Mac");
    return;
  }

  CustomizableUI.addWidgetToArea(
    "bookmarks-menu-button",
    CustomizableUI.AREA_NAVBAR,
    4
  );

  const button = document.getElementById("bookmarks-menu-button");
  const popup = document.getElementById("BMB_bookmarksPopup");

  let shownPromise = onPopupEvent(popup, "shown");
  EventUtils.synthesizeMouseAtCenter(button, { ctrlKey: true });
  await shownPromise;
  ok(true, "Bookmarks menu shown after button pressed");

  // Close bookmarks popup.
  let hiddenPromise = onPopupEvent(popup, "hidden");
  popup.hidePopup();
  await hiddenPromise;

  CustomizableUI.reset();
});
