/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
"use strict";

let { Service } = ChromeUtils.importESModule(
  "resource://services-sync/service.sys.mjs"
);
const { UIState } = ChromeUtils.importESModule(
  "resource://services-sync/UIState.sys.mjs"
);

let getState;
let originalSync;
let syncWasCalled = false;

// TODO: This test should probably be re-written, we don't really test much here.
add_task(async function testSyncRemoteTabsButtonFunctionality() {
  info("Test the Sync Remote Tabs button in the panel");
  storeInitialValues();
  mockFunctions();

  // Force UI update.
  Services.obs.notifyObservers(null, UIState.ON_UPDATE);

  // add the sync remote tabs button to the panel
  CustomizableUI.addWidgetToArea(
    "sync-button",
    CustomizableUI.AREA_FIXED_OVERFLOW_PANEL
  );

  await waitForOverflowButtonShown();

  // check the button's functionality
  await document.getElementById("nav-bar").overflowable.show();
  info("The panel menu was opened");

  let syncRemoteTabsBtn = document.getElementById("sync-button");
  ok(
    syncRemoteTabsBtn,
    "The sync remote tabs button was added to the Panel Menu"
  );
  // click the button - the panel should open.
  syncRemoteTabsBtn.click();
  let remoteTabsPanel = document.getElementById("PanelUI-remotetabs");
  let viewShown = BrowserTestUtils.waitForEvent(remoteTabsPanel, "ViewShown");
  await viewShown;
  ok(remoteTabsPanel.getAttribute("visible"), "Sync Panel is in view");

  // Find and click the "setup" button.
  let syncNowButton = document.getElementById("PanelUI-remotetabs-syncnow");
  syncNowButton.click();
  info("The sync now button was clicked");

  await TestUtils.waitForCondition(() => syncWasCalled);

  // We need to stop the Syncing animation manually otherwise the button
  // will be disabled at the beginning of a next test.
  gSync._onActivityStop();
});

add_task(async function asyncCleanup() {
  // reset the panel UI to the default state
  await resetCustomization();
  ok(CustomizableUI.inDefaultState, "The panel UI is in default state again.");

  if (isOverflowOpen()) {
    let panelHidePromise = promiseOverflowHidden(window);
    PanelUI.overflowPanel.hidePopup();
    await panelHidePromise;
  }

  restoreValues();
});

function mockFunctions() {
  // mock UIState.get()
  UIState.get = () => ({
    status: UIState.STATUS_SIGNED_IN,
    lastSync: new Date(),
    email: "user@mozilla.com",
  });

  Service.sync = mocked_sync;
}

function mocked_sync() {
  syncWasCalled = true;
}

function restoreValues() {
  UIState.get = getState;
  Service.sync = originalSync;
}

function storeInitialValues() {
  getState = UIState.get;
  originalSync = Service.sync;
}
