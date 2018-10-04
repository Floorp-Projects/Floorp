/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const chrome_base = "chrome://mochitests/content/browser/browser/base/content/test/general/";
Services.scriptloader.loadSubScript(chrome_base + "head.js", this);
/* import-globals-from ../general/head.js */

const remoteClientsFixture = [ { id: 1, name: "Foo"}, { id: 2, name: "Bar"} ];

let [testTab] = gBrowser.visibleTabs;

function updateTabContextMenu(tab) {
  let menu = document.getElementById("tabContextMenu");
  if (!tab)
    tab = gBrowser.selectedTab;
  var evt = new Event("");
  tab.dispatchEvent(evt);
  menu.openPopup(tab, "end_after", 0, 0, true, false, evt);
  is(window.TabContextMenu.contextTab, tab, "TabContextMenu context is the expected tab");
  menu.hidePopup();
}

add_task(async function setup() {
  await promiseSyncReady();
  // gSync.init() is called in a requestIdleCallback. Force its initialization.
  gSync.init();
  sinon.stub(Weave.Service.clientsEngine, "getClientType").returns("desktop");
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  registerCleanupFunction(() => {
    gBrowser.removeCurrentTab();
  });
  is(gBrowser.visibleTabs.length, 2, "there are two visible tabs");
});

add_task(async function test_tab_contextmenu() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, remoteClients: remoteClientsFixture,
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });
  let expectation = sandbox.mock(gSync)
                           .expects("sendTabToDevice")
                           .once()
                           .withExactArgs("about:mozilla", [{id: 1, name: "Foo"}], "The Book of Mozilla, 11:14");

  updateTabContextMenu(testTab);
  await openTabContextMenu("context_sendTabToDevice");
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");

  document.getElementById("context_sendTabToDevicePopupMenu").querySelector("menuitem").click();

  await hideTabContextMenu();
  expectation.verify();
  sandbox.restore();
});

add_task(async function test_tab_contextmenu_unconfigured() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, remoteClients: remoteClientsFixture,
                                      state: UIState.STATUS_NOT_CONFIGURED, isSendableURI: true });

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_not_sendable() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, remoteClients: [{ id: 1, name: "Foo"}],
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: false });

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, true, "Send tab to device is disabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_not_synced_yet() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: false, remoteClients: [],
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, true, "Send tab to device is disabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_sync_not_ready_configured() {
  const sandbox = setupSendTabMocks({ syncReady: false, clientsSynced: false, remoteClients: null,
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, true, "Send tab to device is disabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_sync_not_ready_other_state() {
  const sandbox = setupSendTabMocks({ syncReady: false, clientsSynced: false, remoteClients: null,
                                      state: UIState.STATUS_NOT_VERIFIED, isSendableURI: true });

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_fxa_disabled() {
  const getter = sinon.stub(gSync, "SYNC_ENABLED").get(() => false);
  // Simulate onSyncDisabled() being called on window open.
  gSync.onSyncDisabled();

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, true, "Send tab to device is hidden");

  getter.restore();
  [...document.querySelectorAll(".sync-ui-item")].forEach(e => e.hidden = false);
});

async function openTabContextMenu(openSubmenuId = null) {
  const contextMenu = document.getElementById("tabContextMenu");
  is(contextMenu.state, "closed", "checking if popup is closed");

  const awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  EventUtils.synthesizeMouseAtCenter(gBrowser.selectedTab, {type: "contextmenu", button: 2});
  await awaitPopupShown;

  if (openSubmenuId) {
    const menuPopup = document.getElementById(openSubmenuId).menupopup;
    const menuPopupPromise = BrowserTestUtils.waitForEvent(menuPopup, "popupshown");
    menuPopup.openPopup();
    await menuPopupPromise;
  }
}

async function hideTabContextMenu() {
  const contextMenu = document.getElementById("tabContextMenu");
  const awaitPopupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");
  contextMenu.hidePopup();
  await awaitPopupHidden;
}
