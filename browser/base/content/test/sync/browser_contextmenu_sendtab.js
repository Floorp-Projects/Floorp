/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const chrome_base = "chrome://mochitests/content/browser/browser/base/content/test/general/";
Services.scriptloader.loadSubScript(chrome_base + "head.js", this);
/* import-globals-from ../general/head.js */

const remoteClientsFixture = [ { id: 1, name: "Foo"}, { id: 2, name: "Bar"} ];

const origRemoteClients = mockReturn(gSync, "remoteClients", remoteClientsFixture);
const origSyncReady = mockReturn(gSync, "syncReady", true);
const origIsSendableURI = mockReturn(gSync, "isSendableURI", true);
let [testTab] = gBrowser.visibleTabs;

add_task(async function setup() {
  is(gBrowser.visibleTabs.length, 1, "there is one visible tab");
});

add_task(async function test_tab_contextmenu() {
  await updateTabContextMenu(testTab, openSendTabTargetsSubmenu);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");
  let devices = document.getElementById("context_sendTabToDevicePopupMenu").childNodes;
  is(devices[0].getAttribute("label"), "Foo", "Foo target is present");
  is(devices[1].getAttribute("label"), "Bar", "Bar target is present");
  is(devices[3].getAttribute("label"), "Send to All Devices", "All Devices target is present");
});

add_task(async function test_tab_contextmenu_only_one_remote_device() {
  const remoteClientsMock = mockReturn(gSync, "remoteClients", [{ id: 1, name: "Foo"}]);

  await updateTabContextMenu(testTab, openSendTabTargetsSubmenu);
  is(document.getElementById("context_sendTabToDevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");
  let devices = document.getElementById("context_sendTabToDevicePopupMenu").childNodes;
  is(devices.length, 1, "There should not be any separator or All Devices item");
  is(devices[0].getAttribute("label"), "Foo", "Foo target is present");

  remoteClientsMock.restore();
});

add_task(async function test_tab_contextmenu_not_sendable() {
  const isSendableURIMock = mockReturn(gSync, "isSendableURI", false);

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, true, "Send tab to device is hidden");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");

  isSendableURIMock.restore();
});

add_task(async function test_tab_contextmenu_no_remote_clients() {
  let remoteClientsMock = mockReturn(gSync, "remoteClients", []);

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, true, "Send tab to device is hidden");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");

  remoteClientsMock.restore();
});

add_task(async function test_tab_contextmenu_sync_not_ready() {
  const syncReadyMock = mockReturn(gSync, "syncReady", false);

  updateTabContextMenu(testTab);
  is(document.getElementById("context_sendTabToDevice").hidden, true, "Send tab to device is hidden");
  is(document.getElementById("context_sendTabToDevice").disabled, false, "Send tab to device is enabled");

  syncReadyMock.restore();
});

add_task(async function cleanup() {
  origSyncReady.restore();
  origRemoteClients.restore();
  origIsSendableURI.restore();
});

async function openSendTabTargetsSubmenu() {
  let menuPopup = document.getElementById("context_sendTabToDevice").menupopup;
  let menuPopupPromise = BrowserTestUtils.waitForEvent(menuPopup, "popupshown");
  menuPopup.showPopup();
  await menuPopupPromise;
}
