/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_setup(async function () {
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    url: "http://example.com/",
    title: "Test1",
  });

  registerCleanupFunction(async () => {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_menu_in_hidden_window() {
  let hwDoc = Services.appShell.hiddenDOMWindow.document;
  let bmPopup = hwDoc.getElementById("bookmarksMenuPopup");
  var popupEvent = hwDoc.createEvent("MouseEvent");
  popupEvent.initMouseEvent(
    "popupshowing",
    true,
    true,
    Services.appShell.hiddenDOMWindow,
    0,
    0,
    0,
    0,
    0,
    false,
    false,
    false,
    false,
    0,
    null
  );
  bmPopup.dispatchEvent(popupEvent);

  let testMenuitem = [...bmPopup.children].find(
    node => node.getAttribute("label") == "Test1"
  );
  Assert.ok(
    testMenuitem,
    "Should have found the test bookmark in the hidden window bookmark menu"
  );
});
