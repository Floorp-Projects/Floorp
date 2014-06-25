/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let initialLocation = gBrowser.currentURI.spec;
let newTab = null;

add_task(function() {
  info("Check Sync button functionality");
  Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri", "http://example.com/");

  // add the Sync button to the panel
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);

  // check the button's functionality
  yield PanelUI.show();

  let syncButton = document.getElementById("sync-button");
  ok(syncButton, "The Sync button was added to the Panel Menu");
  syncButton.click();

  newTab = gBrowser.selectedTab;
  yield promiseTabLoadEvent(newTab, "about:accounts");

  is(gBrowser.currentURI.spec, "about:accounts", "Firefox Sync page opened");
  ok(!isPanelUIOpen(), "The panel closed");

  if(isPanelUIOpen()) {
    let panelHidePromise = promisePanelHidden(window);
    PanelUI.hide();
    yield panelHidePromise;
  }
});

add_task(function asyncCleanup() {
  Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");
  // reset the panel UI to the default state
  yield resetCustomization();
  ok(CustomizableUI.inDefaultState, "The panel UI is in default state again.");

  // restore the tabs
  gBrowser.addTab(initialLocation);
  gBrowser.removeTab(newTab);
});
