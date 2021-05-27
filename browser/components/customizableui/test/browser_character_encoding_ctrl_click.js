/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_character_encoding_menu() {
  CustomizableUI.addWidgetToArea(
    "characterencoding-button",
    CustomizableUI.AREA_NAVBAR,
    4
  );

  const button = document.getElementById("characterencoding-button");
  EventUtils.synthesizeMouseAtCenter(button, { ctrlKey: true });

  const view = document.getElementById("PanelUI-characterEncodingView");
  let shownPromise = subviewShown(view);
  await shownPromise;
  ok(true, "Character encoding menu shown after button pressed");

  // Close character encoding popup.
  let hiddenPromise = subviewHidden(view);
  view.closest("panel").hidePopup();
  await hiddenPromise;

  CustomizableUI.reset();
});
