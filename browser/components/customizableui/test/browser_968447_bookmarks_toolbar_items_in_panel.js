/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Bug 968447 - The Bookmarks Toolbar Items doesn't appear as a
// normal menu panel button in new windows.
add_task(function() {
  const buttonId = "bookmarks-toolbar-placeholder";
  yield startCustomizing();
  CustomizableUI.addWidgetToArea("personal-bookmarks", CustomizableUI.AREA_PANEL);
  yield endCustomizing();
  yield PanelUI.show();
  let bookmarksToolbarPlaceholder = document.getElementById(buttonId);
  ok(bookmarksToolbarPlaceholder.classList.contains("toolbarbutton-1"),
     "Button should have toolbarbutton-1 class");
  is(bookmarksToolbarPlaceholder.getAttribute("wrap"), "true",
     "Button should have the 'wrap' attribute");
  yield PanelUI.hide();

  let newWin = yield openAndLoadWindow();
  yield newWin.PanelUI.show();
  let newWinBookmarksToolbarPlaceholder = newWin.document.getElementById(buttonId);
  ok(newWinBookmarksToolbarPlaceholder.classList.contains("toolbarbutton-1"),
     "Button in new window should have toolbarbutton-1 class");
  is(newWinBookmarksToolbarPlaceholder.getAttribute("wrap"), "true",
     "Button in new window should have 'wrap' attribute");
  yield newWin.PanelUI.hide();
  newWin.close();
  CustomizableUI.reset();
});
