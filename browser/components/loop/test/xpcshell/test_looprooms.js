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

const kRoomUpdates = {
  "1": {
    participants: []
  },
  "2": {
    participants: [{
      displayName: "Alexis",
      account: "alexis@example.com",
      roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
    }]
  },
  "3": {
    participants: [{
      displayName: "Adam",
      roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
    }]
  },
  "4": {
    participants: [{
      displayName: "Adam",
      roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
    }, {
      displayName: "Alexis",
      account: "alexis@example.com",
      roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
    },  {
      displayName: "Ruharb",
      roomConnectionId: "5de6281c-6568-455f-af08-c0b0a973100e"
    }]
  }
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

// LoopRooms emits various events. Test if they work as expected here.
let gExpectedAdds = [];
let gExpectedUpdates = [];
let gExpectedJoins = {};
let gExpectedLeaves = {};

const onRoomAdded = function(e, room) {
  let expectedIds = gExpectedAdds.map(room => room.roomToken);
  let idx = expectedIds.indexOf(room.roomToken);
  Assert.ok(idx > -1, "Added room should be expected");
  let expected = gExpectedAdds[idx];
  compareRooms(room, expected);
  gExpectedAdds.splice(idx, 1);
};

const onRoomUpdated = function(e, room) {
  let idx = gExpectedUpdates.indexOf(room.roomToken);
  Assert.ok(idx > -1, "Updated room should be expected");
  gExpectedUpdates.splice(idx, 1);
};

const onRoomJoined = function(e, roomToken, participant) {
  let participants = gExpectedJoins[roomToken];
  Assert.ok(participants, "Participant should be expected to join");
  let idx = participants.indexOf(participant.roomConnectionId);
  Assert.ok(idx > -1, "Participant should be expected to join");
  participants.splice(idx, 1);
  if (!participants.length) {
    delete gExpectedJoins[roomToken];
  }
};

const onRoomLeft = function(e, roomToken, participant) {
  let participants = gExpectedLeaves[roomToken];
  Assert.ok(participants, "Participant should be expected to leave");
  let idx = participants.indexOf(participant.roomConnectionId);
  Assert.ok(idx > -1, "Participant should be expected to leave");
  participants.splice(idx, 1);
  if (!participants.length) {
    delete gExpectedLeaves[roomToken];
  }
};

const parseQueryString = function(qs) {
  let map = {};
  let parts = qs.split("=");
  for (let i = 0, l = parts.length; i < l; ++i) {
    if (i % 2 === 1) {
      map[parts[i - 1]] = parts[i];
    }
  }
  return map;
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
      if (req.queryString) {
        let qs = parseQueryString(req.queryString);
        let room = kRooms.get("_nxD4V4FflQ");
        room.participants = kRoomUpdates[qs.version].participants;
        res.write(JSON.stringify([room]));
      } else {
        res.write(JSON.stringify([...kRooms.values()]));
      }
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
      if (req.method == "POST") {
        let body = CommonUtils.readBytesFromInputStream(req.bodyInputStream);
        let data = JSON.parse(body);
        res.setStatusLine(null, 200, "OK");
        res.write(JSON.stringify(data));
        res.processAsync();
        res.finish();
      } else {
        returnRoomDetails(res, room.roomName);
      }
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

  mockPushHandler.registrationPushURL = kEndPointUrl;

  yield MozLoopService.promiseRegisteredWithServers();
});

// Test if fetching a list of all available rooms works correctly.
add_task(function* test_getAllRooms() {
  gExpectedAdds.push(...kRooms.values());
  let rooms = yield LoopRooms.promise("getAll");
  Assert.equal(rooms.length, 3);
  for (let room of rooms) {
    compareRooms(kRooms.get(room.roomToken), room);
  }
});

// Test if fetching a room works correctly.
add_task(function* test_getRoom() {
  let roomToken = "_nxD4V4FflQ";
  let room = yield LoopRooms.promise("get", roomToken);
  Assert.deepEqual(room, kRooms.get(roomToken));
});

// Disabled on Aurora/35 because .rejects() isn't available (bug 984172)
// Test if fetching a room with incorrect token or return values yields an error.
//add_task(function* test_errorStates() {
//  yield Assert.rejects(LoopRooms.promise("get", "error401"), /Not Found/, "Fetching a non-existent room should fail");
//  yield Assert.rejects(LoopRooms.promise("get", "errorMalformed"), /SyntaxError/, "Wrong message format should reject");
//});

// Test if creating a new room works as expected.
add_task(function* test_createRoom() {
  gExpectedAdds.push(kCreateRoomProps);
  let room = yield LoopRooms.promise("create", kCreateRoomProps);
  compareRooms(room, kCreateRoomProps);
});

// Test if deleting a room works as expected.
add_task(function* test_deleteRoom() {
  let roomToken = "QzBbvGmIZWU";
  let deletedRoom = yield LoopRooms.promise("delete", roomToken);
  Assert.equal(deletedRoom.roomToken, roomToken);
  let rooms = yield LoopRooms.promise("getAll");
  Assert.ok(!rooms.some((room) => room.roomToken == roomToken));
});

// Test if opening a new room window works correctly.
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

// Test if push updates function as expected.
add_task(function* test_roomUpdates() {
  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedLeaves["_nxD4V4FflQ"] = [
    "2a1787a6-4a73-43b5-ae3e-906ec1e763cb",
    "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
  ];
  roomsPushNotification("1");
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedLeaves).length === 0);

  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedJoins["_nxD4V4FflQ"] = ["2a1787a6-4a73-43b5-ae3e-906ec1e763cb"];
  roomsPushNotification("2");
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedJoins).length === 0);

  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedJoins["_nxD4V4FflQ"] = ["781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"];
  gExpectedLeaves["_nxD4V4FflQ"] = ["2a1787a6-4a73-43b5-ae3e-906ec1e763cb"];
  roomsPushNotification("3");
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedLeaves).length === 0);

  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedJoins["_nxD4V4FflQ"] = [
    "2a1787a6-4a73-43b5-ae3e-906ec1e763cb",
    "5de6281c-6568-455f-af08-c0b0a973100e"];
  roomsPushNotification("4");
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedJoins).length === 0);
});

// Test if joining a room works as expected.
add_task(function* test_joinRoom() {
  // We need these set up for getting the email address.
  Services.prefs.setCharPref("loop.fxa_oauth.profile", JSON.stringify({
    email: "fake@invalid.com"
  }));
  Services.prefs.setCharPref("loop.fxa_oauth.tokendata", JSON.stringify({
    token_type: "bearer"
  }));

  let roomToken = "_nxD4V4FflQ";
  let joinedData = yield LoopRooms.promise("join", roomToken);
  Assert.equal(joinedData.action, "join");
  Assert.equal(joinedData.displayName, "fake@invalid.com");
});

// Test if refreshing a room works as expected.
add_task(function* test_refreshMembership() {
  let roomToken = "_nxD4V4FflQ";
  let refreshedData = yield LoopRooms.promise("refreshMembership", roomToken,
    "fakeSessionToken");
  Assert.equal(refreshedData.action, "refresh");
  Assert.equal(refreshedData.sessionToken, "fakeSessionToken");
});

// Test if leaving a room works as expected.
add_task(function* test_leaveRoom() {
  let roomToken = "_nxD4V4FflQ";
  let leaveData = yield LoopRooms.promise("leave", roomToken, "fakeLeaveSessionToken");
  Assert.equal(leaveData.action, "leave");
  Assert.equal(leaveData.sessionToken, "fakeLeaveSessionToken");
});

// Test if the event emitter implementation doesn't leak and is working as expected.
add_task(function* () {
  Assert.strictEqual(gExpectedAdds.length, 0, "No room additions should be expected anymore");
  Assert.strictEqual(gExpectedUpdates.length, 0, "No room updates should be expected anymore");
 });

function run_test() {
  setupFakeLoopServer();

  LoopRooms.on("add", onRoomAdded);
  LoopRooms.on("update", onRoomUpdated);
  LoopRooms.on("joined", onRoomJoined);
  LoopRooms.on("left", onRoomLeft);

  do_register_cleanup(function () {
    // Revert original Chat.open implementation
    Chat.open = openChatOrig;

    Services.prefs.clearUserPref("loop.fxa_oauth.profile");
    Services.prefs.clearUserPref("loop.fxa_oauth.tokendata");

    LoopRooms.off("add", onRoomAdded);
    LoopRooms.off("update", onRoomUpdated);
    LoopRooms.off("joined", onRoomJoined);
    LoopRooms.off("left", onRoomLeft);
  });

  run_next_test();
}
