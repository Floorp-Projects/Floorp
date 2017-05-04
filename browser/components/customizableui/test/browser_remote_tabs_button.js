/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

let syncService = {};
Components.utils.import("resource://services-sync/service.js", syncService);
const service = syncService.Service;
Components.utils.import("resource://services-sync/UIState.jsm");

let getState;
let originalSync;
let syncWasCalled = false;

// TODO: This test should probably be re-written, we don't really test much here.
add_task(async function testSyncRemoteTabsButtonFunctionality() {
  info("Test the Sync Remote Tabs button in the PanelUI");
  storeInitialValues();
  mockFunctions();

  // Force UI update.
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  // add the sync remote tabs button to the panel
  CustomizableUI.addWidgetToArea("sync-button", CustomizableUI.AREA_PANEL);

  // check the button's functionality
  await PanelUI.show();
  info("The panel menu was opened");

  let syncRemoteTabsBtn = document.getElementById("sync-button");
  ok(syncRemoteTabsBtn, "The sync remote tabs button was added to the Panel Menu");
  // click the button - the panel should open.
  syncRemoteTabsBtn.click();
  let remoteTabsPanel = document.getElementById("PanelUI-remotetabs");
  ok(remoteTabsPanel.getAttribute("current"), "Sync Panel is in view");

  // Find and click the "setup" button.
  let syncNowButton = document.getElementById("PanelUI-remotetabs-syncnow");
  syncNowButton.click();
  info("The sync now button was clicked");

  await waitForCondition(() => syncWasCalled);
});

add_task(async function asyncCleanup() {
  // reset the panel UI to the default state
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "The panel UI is in default state again.");

  if (isPanelUIOpen()) {
    let panelHidePromise = promisePanelHidden(window);
    PanelUI.hide();
    await panelHidePromise;
  }

  restoreValues();
});

function mockFunctions() {
  // mock UIState.get()
  UIState.get = () => ({
    status: UIState.STATUS_SIGNED_IN,
    email: "user@mozilla.com"
  });

  // mock service.errorHandler.syncAndReportErrors()
  service.errorHandler.syncAndReportErrors = mocked_syncAndReportErrors;
}

function mocked_syncAndReportErrors() {
  syncWasCalled = true;
}

function restoreValues() {
  UIState.get = getState;
  service.syncAndReportErrors = originalSync;
}

function storeInitialValues() {
  getState = UIState.get;
  originalSync = service.syncAndReportErrors;
}
