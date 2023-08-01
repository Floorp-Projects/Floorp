/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Tests that the right-click menu works correctly for the one-off buttons.
 */

const TEST_ENGINE_BASENAME = "searchSuggestionEngine.xml";

let gMaxResults;

ChromeUtils.defineLazyGetter(this, "oneOffSearchButtons", () => {
  return UrlbarTestUtils.getOneOffSearchButtons(window);
});

let originalEngine;
let newEngine;

// The one-off context menu should not be shown.
add_task(async function contextMenu_not_shown() {
  // Add a popupshown listener on the context menu that sets this
  // popupshownFired boolean.
  let popupshownFired = false;
  let onPopupshown = () => {
    popupshownFired = true;
  };
  let contextMenu = oneOffSearchButtons.querySelector(
    ".search-one-offs-context-menu"
  );
  contextMenu.addEventListener("popupshown", onPopupshown);

  // Do a search to open the view.
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: "foo",
  });

  // First, try to open the context menu on a remote engine.
  let allOneOffs = oneOffSearchButtons.getSelectableButtons(true);
  Assert.greater(allOneOffs.length, 0, "There should be at least one one-off");
  Assert.ok(
    allOneOffs[0].engine,
    "The first one-off should be a remote one-off"
  );
  EventUtils.synthesizeMouseAtCenter(allOneOffs[0], {
    type: "contextmenu",
    button: 2,
  });
  let timeout = 500;
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, timeout));
  Assert.ok(
    !popupshownFired,
    "popupshown should not be fired on a remote one-off"
  );

  // Now try to open the context menu on a local one-off.
  let localOneOffs = oneOffSearchButtons.localButtons;
  Assert.greater(
    localOneOffs.length,
    0,
    "There should be at least one local one-off"
  );
  EventUtils.synthesizeMouseAtCenter(localOneOffs[0], {
    type: "contextmenu",
    button: 2,
  });
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, timeout));
  Assert.ok(
    !popupshownFired,
    "popupshown should not be fired on a local one-off"
  );

  contextMenu.removeEventListener("popupshown", onPopupshown);
  await UrlbarTestUtils.promisePopupClose(window);
  await SpecialPowers.popPrefEnv();
});
