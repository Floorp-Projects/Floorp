/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for the infobar that is shown whilst sharing.
 */
"use strict";

const ROOM_TOKEN = "fake1234";

var fakeRoomListGuestOnly = new Map([[ROOM_TOKEN, {
  roomToken: ROOM_TOKEN,
  participants: [{
    roomConnectionId: "3ff0a2e1-f73f-43c6-bb4f-154cc847xy1a",
    displayName: "Guest",
    account: "fake.user@null.com",
    owner: true
  }]
}]]);

var fakeRoomListwithParticipants = new Map([[ROOM_TOKEN, {
  roomToken: ROOM_TOKEN,
  participants: [{
    roomConnectionId: "3ff0a2e1-f73f-43c6-bb4f-154cc847xy1a",
    displayName: "Guest",
    account: "fake.user@null.com",
    owner: true
  }, {
    roomConnectionId: "3ff0a2e1-f73f-43c6-bb4f-123456789112",
    displayName: "Guest",
    owner: false
  }]
}]]);

function promiseStartBrowserSharing() {
  return new Promise(resolve => {
      LoopAPI.stub([{
      sendAsyncMessage: function(messageName, data) {
        let [name] = data;
        if (name == "BrowserSwitch") {
          LoopAPI.restore();
          resolve();
        }
      }
    }]);
  window.LoopUI.startBrowserSharing(ROOM_TOKEN);
});
}

function stopBrowserSharing() {
  window.LoopUI.stopBrowserSharing();
}

var createdTabs = [];

function* removeTabs() {
  for (let createdTab of createdTabs) {
    yield BrowserTestUtils.removeTab(createdTab);
  }

  createdTabs = [];
}

function getInfoBar() {
  let box = gBrowser.getNotificationBox(gBrowser.selectedBrowser);
  return box.getNotificationWithValue("loop-sharing-notification");
}

function assertInfoBarVisible(bar) {
  Assert.ok(bar, "The notification bar should be visible");
  Assert.strictEqual(bar.hidden, false, "Again, the notification bar should be visible");
}

registerCleanupFunction(() => {
  LoopRooms._setRoomsCache();
  stopBrowserSharing();

  // Ensure an XUL alert windows are cleaned up.
  var alerts = Services.wm.getEnumerator("alert:alert");
  while (alerts.hasMoreElements()) {
    alerts.getNext().close();
  }
});

add_task(function* test_infobar_with_only_owner() {
  LoopRooms._setRoomsCache(fakeRoomListGuestOnly);

  yield promiseStartBrowserSharing();

  let bar = getInfoBar();

  assertInfoBarVisible(bar);

  Assert.equal(bar.label, getLoopString("infobar_screenshare_no_guest_message"),
    "The bar label should be for no guests");

  stopBrowserSharing();
});

add_task(function* test_infobar_with_guest() {
  LoopRooms._setRoomsCache(fakeRoomListwithParticipants);

  yield promiseStartBrowserSharing();

  let bar = getInfoBar();

  assertInfoBarVisible(bar);

  Assert.equal(bar.label, getLoopString("infobar_screenshare_browser_message3"),
    "The bar label should be assuming guests are in the room");

  stopBrowserSharing();
});

add_task(function* test_infobar_room_join() {
  LoopRooms._setRoomsCache(fakeRoomListGuestOnly);

  yield promiseStartBrowserSharing();

  let bar = getInfoBar();

  assertInfoBarVisible(bar);

  Assert.equal(bar.label, getLoopString("infobar_screenshare_no_guest_message"),
    "The bar label should be for no guests");

  LoopRooms._setRoomsCache(fakeRoomListwithParticipants,
    fakeRoomListGuestOnly.get(ROOM_TOKEN));

  // Currently the bar gets recreated, so we need to re-get the bar element.
  bar = getInfoBar();

  Assert.equal(bar.label, getLoopString("infobar_screenshare_browser_message3"),
    "The bar label should be assuming guests are in the room");

  stopBrowserSharing();
});

add_task(function* test_infobar_room_leave() {
  LoopRooms._setRoomsCache(fakeRoomListwithParticipants);

  yield promiseStartBrowserSharing();

  let bar = getInfoBar();

  assertInfoBarVisible(bar);

  Assert.equal(bar.label, getLoopString("infobar_screenshare_browser_message3"),
    "The bar label should be assuming guests are in the room");

  LoopRooms._setRoomsCache(fakeRoomListGuestOnly,
    fakeRoomListwithParticipants.get(ROOM_TOKEN));

  // Currently the bar gets recreated, so we need to re-get the bar element.
  bar = getInfoBar();

  Assert.equal(bar.label, getLoopString("infobar_screenshare_no_guest_message"),
    "The bar label should be for no guests");

  stopBrowserSharing();
});

add_task(function* test_infobar_with_only_owner_and_pause_sharing() {
  LoopRooms._setRoomsCache(fakeRoomListGuestOnly);

  yield promiseStartBrowserSharing();

  let bar = getInfoBar();

  assertInfoBarVisible(bar);

  let button = bar.querySelector(".notification-button");
  Assert.ok(button, "There should be a pause button present");
  Assert.equal(button.type, "pause", "The bar button should be type pause");

  button.click();

  Assert.equal(bar.label, getLoopString("infobar_screenshare_stop_no_guest_message"),
    "The bar label should be for no guests and paused");

  stopBrowserSharing();
});

add_task(function* test_infobar_with_guest_and_pause_sharing() {
  LoopRooms._setRoomsCache(fakeRoomListwithParticipants);

  yield promiseStartBrowserSharing();

  let bar = getInfoBar();

  assertInfoBarVisible(bar);

  let button = bar.querySelector(".notification-button");

  button.click();

  Assert.equal(bar.label, getLoopString("infobar_screenshare_stop_sharing_message2"),
    "The bar label should be assuming guests are in the room and sharing paused");

  stopBrowserSharing();
});

add_task(function* test_infobar_disconnect() {
  LoopRooms._setRoomsCache(fakeRoomListwithParticipants);

  yield promiseStartBrowserSharing();

  let bar = getInfoBar();

  assertInfoBarVisible(bar);

  let button = bar.querySelector(".notification-button-default");
  Assert.ok(button, "There should be a stop button present");
  Assert.equal(button.type, "stop", "The button should be type stop");

  button.click();

  Assert.equal(getInfoBar(), null, "The notification should be hidden now");

  stopBrowserSharing();
});

add_task(function* test_infobar_switch_tabs() {
  // First we add two tabs.
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:home");
  createdTabs.push(tab);
  tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  createdTabs.push(tab);

  Assert.strictEqual(gBrowser.selectedTab, createdTabs[1],
    "The second tab created should be selected now");

  yield promiseStartBrowserSharing();

  let bar = getInfoBar();

  assertInfoBarVisible(bar);

  // When we switch tabs, the infobar should move along with it. We use `selectedIndex`
  // here, because that's the setter that triggers the 'select' event. This event
  // is what LoopUI listens to and moves the infobar.
  gBrowser.selectedIndex = Array.indexOf(gBrowser.tabs, createdTabs[0]);

  // We now know that the second tab is selected and should still be displaying the
  // infobar.
  assertInfoBarVisible(bar);

  // Cleanup.
  yield removeTabs();

  stopBrowserSharing();
});
