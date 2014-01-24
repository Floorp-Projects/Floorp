/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function() {
  info("Check addons button existence and functionality");

  let initialLocation = gBrowser.currentURI.spec;

  yield PanelUI.show();

  let addonsButton = document.getElementById("add-ons-button");
  ok(addonsButton, "Add-ons button exists in Panel Menu");
  addonsButton.click();

  yield waitForCondition(function() gBrowser.currentURI &&
                                    gBrowser.currentURI.spec == "about:addons");

  let addonsPage = gBrowser.selectedBrowser.contentWindow.document.
                            getElementById("addons-page");
  ok(addonsPage, "Add-ons page was opened");

  // close the add-ons tab
  if(gBrowser.tabs.length > 1) {
    gBrowser.removeTab(gBrowser.selectedTab);
  } else {
    var tabToRemove = gBrowser.selectedTab;
    gBrowser.addTab(initialLocation);
    gBrowser.removeTab(tabToRemove);
  }
});
