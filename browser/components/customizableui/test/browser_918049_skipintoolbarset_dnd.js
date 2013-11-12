/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let navbar;
let skippedItem;
let gTests = [
  {
    desc: "Attempting to drag a skipintoolbarset item should work.",
    setup: function() {
      navbar = document.getElementById("nav-bar");
      skippedItem = document.createElement("toolbarbutton");
      skippedItem.id = "test-skipintoolbarset-item";
      skippedItem.setAttribute("label", "Test");
      skippedItem.setAttribute("skipintoolbarset", "true");
      navbar.customizationTarget.appendChild(skippedItem);
    },
    run: function() {
      let downloadsButton = document.getElementById("downloads-button");
      yield startCustomizing();
      ok(CustomizableUI.inDefaultState, "Should still be in default state");
      simulateItemDrag(skippedItem, downloadsButton);
      ok(CustomizableUI.inDefaultState, "Should still be in default state");
      let skippedItemWrapper = skippedItem.parentNode;
      is(skippedItemWrapper.nextSibling && skippedItemWrapper.nextSibling.id,
         downloadsButton.parentNode.id, "Should be next to downloads button");
      simulateItemDrag(downloadsButton, skippedItem);
      let downloadWrapper = downloadsButton.parentNode;
      is(downloadWrapper.nextSibling && downloadWrapper.nextSibling.id,
         skippedItem.parentNode.id, "Should be next to skipintoolbarset item");
      ok(CustomizableUI.inDefaultState, "Should still be in default state");
    }
  },
];
function asyncCleanup() {
  yield endCustomizing();
  skippedItem.remove();
  Services.prefs.clearUserPref("browser.uiCustomization.skipSourceNodeCheck");
  yield resetCustomization();
}

function test() {
  Services.prefs.setBoolPref("browser.uiCustomization.skipSourceNodeCheck", true);
  waitForExplicitFinish();
  runTests(gTests, asyncCleanup);
}
