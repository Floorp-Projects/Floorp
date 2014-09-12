/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// The test expects the about:accounts page to open in the current tab

"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "UITour", "resource:///modules/UITour.jsm");

let initialLocation = gBrowser.currentURI.spec;
let newTab = null;

function openAboutAccountsFromMenuPanel(entryPoint) {
  info("Check Sync button functionality");
  Services.prefs.setCharPref("identity.fxaccounts.remote.signup.uri", "http://example.com/");

  // add the Sync button to the panel
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);

  // check the button's functionality
  yield PanelUI.show();

  if (entryPoint == "uitour") {
    UITour.originTabs.set(window, new Set());
    UITour.originTabs.get(window).add(gBrowser.selectedTab);
  }

  let syncButton = document.getElementById("sync-button");
  ok(syncButton, "The Sync button was added to the Panel Menu");

  let deferred = Promise.defer();
  let handler = (e) => {
    if (e.originalTarget != gBrowser.selectedTab.linkedBrowser.contentDocument ||
        e.target.location.href == "about:blank") {
      info("Skipping spurious 'load' event for " + e.target.location.href);
      return;
    }
    gBrowser.selectedTab.linkedBrowser.removeEventListener("load", handler, true);
    deferred.resolve();
  }
  gBrowser.selectedTab.linkedBrowser.addEventListener("load", handler, true);

  syncButton.click();
  yield deferred.promise;
  newTab = gBrowser.selectedTab;

  is(gBrowser.currentURI.spec, "about:accounts?entrypoint=" + entryPoint,
    "Firefox Sync page opened with `menupanel` entrypoint");
  ok(!isPanelUIOpen(), "The panel closed");

  if(isPanelUIOpen()) {
    let panelHidePromise = promisePanelHidden(window);
    PanelUI.hide();
    yield panelHidePromise;
  }
}

function asyncCleanup() {
  Services.prefs.clearUserPref("identity.fxaccounts.remote.signup.uri");
  // reset the panel UI to the default state
  yield resetCustomization();
  ok(CustomizableUI.inDefaultState, "The panel UI is in default state again.");

  // restore the tabs
  gBrowser.addTab(initialLocation);
  gBrowser.removeTab(newTab);
  UITour.originTabs.delete(window);
}

add_task(() => openAboutAccountsFromMenuPanel("syncbutton"));
add_task(asyncCleanup);
// Test that uitour is in progress, the entrypoint is `uitour` and not `menupanel`
add_task(() => openAboutAccountsFromMenuPanel("uitour"));
add_task(asyncCleanup);
