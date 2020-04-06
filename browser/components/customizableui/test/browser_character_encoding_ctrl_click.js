/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_character_encoding_menu() {
  // On macOS, ctrl-click shouldn't open the panel because this normally opens
  // the context menu. This happens via the `contextmenu` event which is created
  // by widget code, so our simulated clicks do not do so, so we can't test
  // anything on macOS.
  if (AppConstants.platform == "macosx") {
    ok(true, "The test is ignored on Mac");
    return;
  }

  CustomizableUI.addWidgetToArea(
    "characterencoding-button",
    CustomizableUI.AREA_NAVBAR,
    4
  );

  const button = document.getElementById("characterencoding-button");
  const view = document.getElementById("PanelUI-characterEncodingView");

  let shownPromise = subviewShown(view);
  EventUtils.synthesizeMouseAtCenter(button, { ctrlKey: true });
  await shownPromise;
  ok(true, "Character encoding menu shown after button pressed");

  // Close character encoding popup.
  let hiddenPromise = subviewHidden(view);
  view.closest("panel").hidePopup();
  await hiddenPromise;

  CustomizableUI.reset();
});
