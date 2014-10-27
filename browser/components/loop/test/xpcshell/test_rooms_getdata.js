/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://services-common/utils.js");

XPCOMUtils.defineLazyModuleGetter(this, "Chat",
                                  "resource:///modules/Chat.jsm");
let hasTheseProps = function(a, b) {
  for (let prop in a) {
    if (a[prop] != b[prop]) {
      do_print("hasTheseProps fail: prop = " + prop);
      return false;
    }
  }
  return true;
}

let openChatOrig = Chat.open;

add_test(function test_getAllRooms() {

 let roomList = [
   { roomToken: "_nxD4V4FflQ",
     roomName: "First Room Name",
     roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
     maxSize: 2,
     currSize: 0,
     ctime: 1405517546 },
   { roomToken: "QzBbvGmIZWU",
     roomName: "Second Room Name",
     roomUrl: "http://localhost:3000/rooms/QzBbvGmIZWU",
     maxSize: 2,
     currSize: 0,
     ctime: 140551741 },
   { roomToken: "3jKS_Els9IU",
     roomName: "Third Room Name",
     roomUrl: "http://localhost:3000/rooms/3jKS_Els9IU",
     maxSize: 3,
     clientMaxSize: 2,
     currSize: 1,
     ctime: 1405518241 }
  ]

  let roomDetail = {
    roomName: "First Room Name",
    roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
    roomOwner: "Alexis",
    maxSize: 2,
    clientMaxSize: 2,
    creationTime: 1405517546,
    expiresAt: 1405534180,
    participants: [
       { displayName: "Alexis", account: "alexis@example.com", roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb" },
       { displayName: "Adam", roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7" }
     ]
  }

  loopServer.registerPathHandler("/rooms", (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.write(JSON.stringify(roomList));
    response.processAsync();
    response.finish();
  });

  let returnRoomDetails = function(response, roomName) {
    roomDetail.roomName = roomName;
    response.setStatusLine(null, 200, "OK");
    response.write(JSON.stringify(roomDetail));
    response.processAsync();
    response.finish();
  }

  loopServer.registerPathHandler("/rooms/_nxD4V4FflQ", (request, response) => {
    returnRoomDetails(response, "First Room Name");
  });

  loopServer.registerPathHandler("/rooms/QzBbvGmIZWU", (request, response) => {
    returnRoomDetails(response, "Second Room Name");
  });

  loopServer.registerPathHandler("/rooms/3jKS_Els9IU", (request, response) => {
    returnRoomDetails(response, "Third Room Name");
  });

  MozLoopService.register().then(() => {

    LoopRooms.getAll((error, rooms) => {
      do_check_false(error);
      do_check_true(rooms);
      do_check_eq(rooms.length, 3);
      do_check_eq(rooms[0].roomName, "First Room Name");
      do_check_eq(rooms[1].roomName, "Second Room Name");
      do_check_eq(rooms[2].roomName, "Third Room Name");

      let room = rooms[0];
      do_check_true(room.localRoomId);
      do_check_false(room.currSize);
      delete roomList[0].currSize;
      do_check_true(hasTheseProps(roomList[0], room));
      delete roomDetail.roomName;
      delete room.participants;
      delete roomDetail.participants;
      do_check_true(hasTheseProps(roomDetail, room));

      LoopRooms.getRoomData(room.localRoomId, (error, roomData) => {
        do_check_false(error);
        do_check_true(hasTheseProps(room, roomData));

        run_next_test();
      });
    });
  });
});

function run_test() {
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
