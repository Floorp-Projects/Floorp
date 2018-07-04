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
  is(gBrowser.visibleTabs.length, 1, "there is one visible tab");
});

// We are not testing the devices popup contents, since it is already tested by
// browser_contextmenu_sendpage.js and the code to populate it is the same.

add_task(async function test_tab_contextmenu() {
  const sandbox = setupSendTabMocks({ syncReady: true, clientsSynced: true, remoteClients: remoteClientsFixture,
                                      state: UIState.STATUS_SIGNED_IN, isSendableURI: true });

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");

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
  is(document.getElementById("context_sendTabToDevice_separator").hidden, true, "Separator is also hidden");

  getter.restore();
  [...document.querySelectorAll(".sync-ui-item")].forEach(e => e.hidden = false);
});
