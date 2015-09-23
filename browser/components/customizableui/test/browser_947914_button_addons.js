/* This Source Code Form is subject to the terms of the Mozilla Public
  * License, v. 2.0. If a copy of the MPL was not distributed with this
  * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var initialLocation = gBrowser.currentURI.spec;
var newTab = null;

add_task(function() {
  info("Check addons button existence and functionality");

  yield PanelUI.show();
  info("Menu panel was opened");

  let addonsButton = document.getElementById("add-ons-button");
  ok(addonsButton, "Add-ons button exists in Panel Menu");
  addonsButton.click();

  newTab = gBrowser.selectedTab;
  yield waitForCondition(() => gBrowser.currentURI &&
                               gBrowser.currentURI.spec == "about:addons");

  let addonsPage = gBrowser.selectedBrowser.contentWindow.document.
                            getElementById("addons-page");
  ok(addonsPage, "Add-ons page was opened");
});

add_task(function asyncCleanup() {
  gBrowser.addTab(initialLocation);
  gBrowser.removeTab(gBrowser.selectedTab);
  info("Tabs were restored");
});
