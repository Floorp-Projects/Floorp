/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let tempElements = [];
// Shouldn't insert multiple separators into the developer tools subview
add_task(function testMultipleDevtoolsSeparators() {
  let devtoolsSubMenu = document.getElementById("menuWebDeveloperPopup");
  // Last element is null to insert at the end:
  let beforeEls = [devtoolsSubMenu.firstChild, devtoolsSubMenu.lastChild, null];
  for (let i = 0; i < beforeEls.length; i++) {
    let sep = document.createElement("menuseparator");
    tempElements.push(sep);
    devtoolsSubMenu.insertBefore(sep, beforeEls[i]);
    let menu = document.createElement("menu");
    tempElements.push(menu);
    devtoolsSubMenu.insertBefore(menu, beforeEls[i]);
    // And another separator for good measure:
    sep = document.createElement("menuseparator");
    tempElements.push(sep);
    devtoolsSubMenu.insertBefore(sep, beforeEls[i]);
  }
  yield PanelUI.show();

  let devtoolsButton = document.getElementById("developer-button");
  devtoolsButton.click();
  yield waitForCondition(() => !PanelUI.multiView.hasAttribute("transitioning"));
  let subview = document.getElementById("PanelUI-developerItems");
  ok(subview.firstChild, "Subview should have a kid");
  is(subview.firstChild.localName, "toolbarbutton", "There should be no separators to start with");

  for (let kid of subview.children) {
    if (kid.localName == "menuseparator") {
      ok(kid.previousSibling && kid.previousSibling.localName != "menuseparator",
         "Separators should never have another separator next to them, and should never be the first node.");
    }
  }

  let panelHiddenPromise = promisePanelHidden(window);
  PanelUI.hide();
  yield panelHiddenPromise;
});

registerCleanupFunction(function() {
  for (let el of tempElements) {
    el.remove();
  }
  tempElements = null;
});
