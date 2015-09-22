/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * This file contains tests for checking the channel from the standalone to
 * LoopRooms works for checking if rooms can be opened within the conversation
 * window.
 */
"use strict";

var {WebChannel} = Cu.import("resource://gre/modules/WebChannel.jsm", {});
var {Chat} = Cu.import("resource:///modules/Chat.jsm", {});

const TEST_URI =
  "example.com/browser/browser/components/loop/test/mochitest/test_loopLinkClicker_channel.html";
const TEST_URI_GOOD = Services.io.newURI("https://" + TEST_URI, null, null);
const TEST_URI_BAD = Services.io.newURI("http://" + TEST_URI, null, null);

const ROOM_TOKEN = "fake1234";
const LINKCLICKER_URL_PREFNAME = "loop.linkClicker.url";

var openChatOrig = Chat.open;

var fakeRoomList = new Map([[ ROOM_TOKEN, { roomToken: ROOM_TOKEN } ]]);

function BackChannel(uri) {
  this.channel = new WebChannel("test-loop-link-clicker-backchannel", uri);

  this.channel.listen((id, data) => {
    if (this.pendingResolve) {
      this.pendingResolve(data);
      return;
    }

    this.receivedData = data;
  });
}

BackChannel.prototype = {
  channel: null,
  receivedData: null,
  pendingResolve: null,

  tearDown: function() {
    this.channel.stopListening();
  }
};

var gGoodBackChannel;
var gBadBackChannel;

// Loads the specified URI in a new tab and waits for it to send us data on our
// test web-channel and resolves with that data.
function promiseNewChannelResponse(uri, channel, hash) {
  let waitForChannelPromise = new Promise((resolve, reject) => {
    if (channel.receivedData) {
      resolve(channel.receivedData);
      return;
    }

    channel.pendingResolve = resolve;
  });

  return BrowserTestUtils.withNewTab({
    gBrowser: gBrowser,
    url: uri.spec + "#" + hash
  }, () => waitForChannelPromise);
}

add_task(function* setup() {
  gGoodBackChannel = new BackChannel(TEST_URI_GOOD);
  gBadBackChannel = new BackChannel(TEST_URI_BAD);

  registerCleanupFunction(() => {
    gGoodBackChannel.tearDown();
    gBadBackChannel.tearDown();
  });

  yield undefined;
});

add_task(function* test_loopRooms_webChannel_permissions() {
  // We haven't set the allowed web page yet - so even the "good" URI should fail.
  let got = yield promiseNewChannelResponse(TEST_URI_GOOD, gGoodBackChannel, "checkWillOpenRoom");
  // Should have no data.
  Assert.ok(got.message === undefined, "should have failed to get any data");

  // Add a permission manager entry for our URI.
  Services.prefs.setCharPref(LINKCLICKER_URL_PREFNAME, TEST_URI_GOOD.spec);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref(LINKCLICKER_URL_PREFNAME);
  });

  // Try again - now we are expecting a response with actual data.
  got = yield promiseNewChannelResponse(TEST_URI_GOOD, gGoodBackChannel, "checkWillOpenRoom");

  // The room doesn't exist, so we should get a negative response.
  Assert.equal(got.message.response, false, "should have got a response of false");

  // Now a http:// URI - should get nothing even with the permission setup.
  got = yield promiseNewChannelResponse(TEST_URI_BAD, gBadBackChannel, "checkWillOpenRoom");
  Assert.ok(got.message === undefined, "should have failed to get any data");
});

add_task(function* test_loopRooms_webchannel_checkWillOpenRoom() {
  // We've already tested if the room doesn't exist above, so here we add the
  // room and check the result.
  LoopRooms._setRoomsCache(fakeRoomList);

  let got = yield promiseNewChannelResponse(TEST_URI_GOOD, gGoodBackChannel, "checkWillOpenRoom");

  Assert.equal(got.message.response, true, "should have got a response of true");
});

add_task(function* test_loopRooms_webchannel_openRoom() {
 let openedUrl;
  Chat.open = function(contentWindow, origin, title, url) {
    openedUrl = url;
  };
  registerCleanupFunction(() => {
    Chat.open = openChatOrig;
  });

  // Test when the room doesn't exist
  LoopRooms._setRoomsCache();

  let got = yield promiseNewChannelResponse(TEST_URI_GOOD, gGoodBackChannel, "openRoom");

  Assert.ok(!openedUrl, "should not open a chat window");
  Assert.equal(got.message.response, false, "should have got a response of false");

  // Now add a room & check it.
  LoopRooms._setRoomsCache(fakeRoomList);

  got = yield promiseNewChannelResponse(TEST_URI_GOOD, gGoodBackChannel, "openRoom");

  // Check the room was opened.
  Assert.ok(openedUrl, "should open a chat window");

  let windowId = openedUrl.match(/about:loopconversation\#(\w+)$/)[1];
  let windowData = MozLoopService.getConversationWindowData(windowId);

  Assert.equal(windowData.type, "room", "window data should contain room as the type");
  Assert.equal(windowData.roomToken, ROOM_TOKEN, "window data should have the roomToken");

  Assert.equal(got.message.response, true, "should have got a response of true");
});
