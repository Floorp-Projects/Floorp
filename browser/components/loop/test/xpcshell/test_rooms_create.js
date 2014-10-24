/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "Chat",
                                  "resource:///modules/Chat.jsm");
let hasTheseProps = function(a, b) {
  for (let prop in a) {
    if (a[prop] != b[prop]) {
      return false;
    }
  }
  return true;
}

let openChatOrig = Chat.open;

add_test(function test_openRoomsWindow() {
  let roomProps = {roomName: "UX Discussion",
                   expiresIn: 5,
                   roomOwner: "Alexis",
                   maxSize: 2}

  let roomData = {roomToken: "_nxD4V4FflQ",
                  roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
                  expiresAt: 1405534180}

  loopServer.registerPathHandler("/rooms", (request, response) => {
    if (!request.bodyInputStream) {
      do_throw("empty request body");
    }
    let body = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
    let data = JSON.parse(body);
    do_check_true(hasTheseProps(roomProps, data));

    response.setStatusLine(null, 200, "OK");
    response.write(JSON.stringify(roomData));
    response.processAsync();
    response.finish();
  });

  MozLoopService.register(mockPushHandler).then(() => {
    let opened = false;
    let created = false;
    let urlPieces = [];

    Chat.open = function(contentWindow, origin, title, url) {
      urlPieces = url.split('/');
      do_check_eq(urlPieces[0], "about:loopconversation#room");
      opened = true;
    };

    let returnedID = LoopRooms.createRoom(roomProps, (error, data) => {
      do_check_false(error);
      do_check_true(data);
      do_check_true(hasTheseProps(roomData, data));
      do_check_eq(data.localRoomId, urlPieces[1]);
      created = true;
    });

    waitForCondition(function() created && opened).then(() => {
      do_check_true(opened, "should open a chat window");
      do_check_eq(returnedID, urlPieces[1]);

      // Verify that a delayed callback, when attached,
      // received the same data.
      LoopRooms.addCallback(
        urlPieces[1], "RoomCreated",
        (error, data) => {
          do_check_false(error);
          do_check_true(data);
          do_check_true(hasTheseProps(roomData, data));
          do_check_eq(data.localRoomId, urlPieces[1]);
        });

      run_next_test();
    }, () => {
      do_throw("should have opened a chat window");
    });

  });
});

function run_test()
{
  setupFakeLoopServer();
  mockPushHandler.registrationPushURL = kEndPointUrl;

  loopServer.registerPathHandler("/registration", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.processAsync();
    response.finish();
  });

  do_register_cleanup(function() {
    // Revert original Chat.open implementation
    Chat.open = openChatOrig;
  });

  run_next_test();
}
