/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const chrome_base = "chrome://mochitests/content/browser/browser/base/content/test/general/";
Services.scriptloader.loadSubScript(chrome_base + "head.js", this);
/* import-globals-from ../general/head.js */

const fxaDevices = [
  {id: 1, name: "Foo", availableCommands: {"https://identity.mozilla.com/cmd/open-uri": "baz"}},
  {id: 2, name: "Bar", clientRecord: "bar"}, // Legacy send tab target (no availableCommands).
  {id: 3, name: "Homer"}, // Incompatible target.
];

let [testTab] = gBrowser.visibleTabs;

function updateTabContextMenu(tab = gBrowser.selectedTab) {
  let menu = document.getElementById("tabContextMenu");
  var evt = new Event("");
  tab.dispatchEvent(evt);
  // The TabContextMenu initializes its strings only on a focus or mouseover event.
  // Calls focus event on the TabContextMenu early in the test
  gBrowser.selectedTab.focus();
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
  const sandbox = setupSendTabMocks({fxaDevices});
  let expectation = sandbox.mock(gSync)
                           .expects("sendTabToDevice")
                           .once()
                           .withExactArgs("about:mozilla", [fxaDevices[1]], "The Book of Mozilla, 11:14");

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
  const sandbox = setupSendTabMocks({state: UIState.STATUS_NOT_CONFIGURED});

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_not_sendable() {
  const sandbox = setupSendTabMocks({fxaDevices, isSendableURI: false});

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, true, "Send tab to device is disabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_not_synced_yet() {
  const sandbox = setupSendTabMocks({fxaDevices: null});

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, true, "Send tab to device is disabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_sync_not_ready_configured() {
  const sandbox = setupSendTabMocks({syncReady: false});

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, true, "Send tab to device is disabled");

  sandbox.restore();
});

add_task(async function test_tab_contextmenu_sync_not_ready_other_state() {
  const sandbox = setupSendTabMocks({syncReady: false, state: UIState.STATUS_NOT_VERIFIED});

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

add_task(async function teardown() {
  Weave.Service.clientsEngine.getClientType.restore();
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
