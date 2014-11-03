/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://services-common/utils.js");
Cu.import("resource:///modules/loop/LoopRooms.jsm");
Cu.import("resource:///modules/Chat.jsm");

let openChatOrig = Chat.open;

const kRooms = new Map([
  ["_nxD4V4FflQ", {
    roomToken: "_nxD4V4FflQ",
    roomName: "First Room Name",
    roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
    maxSize: 2,
    currSize: 0,
    ctime: 1405517546
  }],
  ["QzBbvGmIZWU", {
    roomToken: "QzBbvGmIZWU",
    roomName: "Second Room Name",
    roomUrl: "http://localhost:3000/rooms/QzBbvGmIZWU",
    maxSize: 2,
    currSize: 0,
    ctime: 140551741
  }],
  ["3jKS_Els9IU", {
    roomToken: "3jKS_Els9IU",
    roomName: "Third Room Name",
    roomUrl: "http://localhost:3000/rooms/3jKS_Els9IU",
    maxSize: 3,
    clientMaxSize: 2,
    currSize: 1,
    ctime: 1405518241
  }]
]);

let roomDetail = {
  roomName: "First Room Name",
  roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
  roomOwner: "Alexis",
  maxSize: 2,
  clientMaxSize: 2,
  creationTime: 1405517546,
  expiresAt: 1405534180,
  participants: [{
    displayName: "Alexis",
    account: "alexis@example.com",
    roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
  }, {
    displayName: "Adam",
    roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
  }]
};

const kCreateRoomProps = {
  roomName: "UX Discussion",
  expiresIn: 5,
  roomOwner: "Alexis",
  maxSize: 2
};

const kCreateRoomData = {
  roomToken: "_nxD4V4FflQ",
  roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
  expiresAt: 1405534180
};

add_task(function* setup_server() {
  loopServer.registerPathHandler("/registration", (req, res) => {
    res.setStatusLine(null, 200, "OK");
    res.processAsync();
    res.finish();
  });

  loopServer.registerPathHandler("/rooms", (req, res) => {
    res.setStatusLine(null, 200, "OK");

    if (req.method == "POST") {
      Assert.ok(req.bodyInputStream, "POST request should have a payload");
      let body = CommonUtils.readBytesFromInputStream(req.bodyInputStream);
      let data = JSON.parse(body);
      Assert.deepEqual(data, kCreateRoomProps);

      res.write(JSON.stringify(kCreateRoomData));
    } else {
      res.write(JSON.stringify([...kRooms.values()]));
    }

    res.processAsync();
    res.finish();
  });

  function returnRoomDetails(res, roomName) {
    roomDetail.roomName = roomName;
    res.setStatusLine(null, 200, "OK");
    res.write(JSON.stringify(roomDetail));
    res.processAsync();
    res.finish();
  }

  // Add a request handler for each room in the list.
  [...kRooms.values()].forEach(function(room) {
    loopServer.registerPathHandler("/rooms/" + encodeURIComponent(room.roomToken), (req, res) => {
      returnRoomDetails(res, room.roomName);
    });
  });

  loopServer.registerPathHandler("/rooms/error401", (req, res) => {
    res.setStatusLine(null, 401, "Not Found");
    res.processAsync();
    res.finish();
  });

  loopServer.registerPathHandler("/rooms/errorMalformed", (req, res) => {
    res.setStatusLine(null, 200, "OK");
    res.write("{\"some\": \"Syntax Error!\"}}}}}}");
    res.processAsync();
    res.finish();
  });
});

const normalizeRoom = function(room) {
  delete room.currSize;
  if (!("participants" in room)) {
    let name = room.roomName;
    for (let key of Object.getOwnPropertyNames(roomDetail)) {
      room[key] = roomDetail[key];
    }
    room.roomName = name;
  }
  return room;
};

const compareRooms = function(room1, room2) {
  Assert.deepEqual(normalizeRoom(room1), normalizeRoom(room2));
};

add_task(function* test_getAllRooms() {
  yield MozLoopService.promiseRegisteredWithServers();

  let rooms = yield LoopRooms.promise("getAll");
  Assert.equal(rooms.length, 3);
  for (let room of rooms) {
    compareRooms(kRooms.get(room.roomToken), room);
  }
});

add_task(function* test_getRoom() {
  yield MozLoopService.promiseRegisteredWithServers();

  let roomToken = "_nxD4V4FflQ";
  let room = yield LoopRooms.promise("get", roomToken);
  Assert.deepEqual(room, kRooms.get(roomToken));
});

add_task(function* test_errorStates() {
  yield Assert.rejects(LoopRooms.promise("get", "error401"), /Not Found/, "Fetching a non-existent room should fail");
  yield Assert.rejects(LoopRooms.promise("get", "errorMalformed"), /SyntaxError/, "Wrong message format should reject");
});

add_task(function* test_createRoom() {
  let eventCalled = false;
  LoopRooms.once("add", (e, room) => {
    compareRooms(room, kCreateRoomProps);
    eventCalled = true;
  });
  let room = yield LoopRooms.promise("create", kCreateRoomProps);
  compareRooms(room, kCreateRoomProps);
  Assert.ok(eventCalled, "Event should have fired");
});

add_task(function* test_openRoom() {
  let openedUrl;
  Chat.open = function(contentWindow, origin, title, url) {
    openedUrl = url;
  };

  LoopRooms.open("fakeToken");

  Assert.ok(openedUrl, "should open a chat window");

  // Stop the busy kicking in for following tests.
  let windowId = openedUrl.match(/about:loopconversation\#(\d+)$/)[1];
  let windowData = MozLoopService.getConversationWindowData(windowId);

  Assert.equal(windowData.type, "room", "window data should contain room as the type");
  Assert.equal(windowData.roomToken, "fakeToken", "window data should have the roomToken");
});

function run_test() {
  do_register_cleanup(function() {
    // Revert original Chat.open implementation
    Chat.open = openChatOrig;
  });

  setupFakeLoopServer();

  run_next_test();
}
