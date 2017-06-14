/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const remoteClientsFixture = [ { id: 1, name: "Foo"}, { id: 2, name: "Bar"} ];

const origRemoteClients = mockReturn(gSync, "remoteClients", remoteClientsFixture);
const origSyncReady = mockReturn(gSync, "syncReady", true);
const origIsSendableURI = mockReturn(gSync, "isSendableURI", true);

add_task(async function setup() {
  await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
});

add_task(async function test_page_contextmenu() {
  await updateContentContextMenu("#moztext", "context-sendpagetodevice");
  is(document.getElementById("context-sendpagetodevice").hidden, false, "Send tab to device is shown");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");
  let devices = document.getElementById("context-sendpagetodevice-popup").childNodes;
  is(devices[0].getAttribute("label"), "Foo", "Foo target is present");
  is(devices[1].getAttribute("label"), "Bar", "Bar target is present");
  is(devices[3].getAttribute("label"), "Send to All Devices", "All Devices target is present");
});

add_task(async function test_page_contextmenu_notsendable() {
  const isSendableURIMock = mockReturn(gSync, "isSendableURI", false);

  await updateContentContextMenu("#moztext");
  is(document.getElementById("context-sendpagetodevice").hidden, true, "Send tab to device is hidden");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");

  isSendableURIMock.restore();
});

add_task(async function test_page_contextmenu_sendtab_no_remote_clients() {
  let remoteClientsMock = mockReturn(gSync, "remoteClients", []);

  await updateContentContextMenu("#moztext");
  is(document.getElementById("context-sendpagetodevice").hidden, true, "Send tab to device is hidden");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");

  remoteClientsMock.restore();
});

add_task(async function test_page_contextmenu_sync_not_ready() {
  const syncReadyMock = mockReturn(gSync, "syncReady", false);

  await updateContentContextMenu("#moztext");
  is(document.getElementById("context-sendpagetodevice").hidden, true, "Send tab to device is hidden");
  is(document.getElementById("context-sendpagetodevice").disabled, false, "Send tab to device is enabled");

  syncReadyMock.restore();
});

// We are not going to bother testing the states of context-sendlinktodevice since they use
// the exact same code.
// However, browser_contextmenu.js contains tests that verify the menu item is present.

add_task(async function cleanup() {
  gBrowser.removeCurrentTab();
  origSyncReady.restore();
  origRemoteClients.restore();
  origIsSendableURI.restore();
});

async function updateContentContextMenu(selector, openSubmenuId = null) {
  let contextMenu = document.getElementById("contentAreaContextMenu");
  is(contextMenu.state, "closed", "checking if popup is closed");

  let awaitPopupShown = BrowserTestUtils.waitForEvent(contextMenu, "popupshown");
  await BrowserTestUtils.synthesizeMouse(selector, 0, 0, {
      type: "contextmenu",
      button: 2,
      shiftkey: false,
      centered: true
    },
    gBrowser.selectedBrowser);
  await awaitPopupShown;

  if (openSubmenuId) {
    let menuPopup = document.getElementById(openSubmenuId).menupopup;
    let menuPopupPromise = BrowserTestUtils.waitForEvent(menuPopup, "popupshown");
    menuPopup.showPopup();
    await menuPopupPromise;
  }

  let awaitPopupHidden = BrowserTestUtils.waitForEvent(contextMenu, "popuphidden");

  contextMenu.hidePopup();
  await awaitPopupHidden;
}
