/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported run_test */

Cu.import("resource://services-common/utils.js");
Cu.import("chrome://loop/content/modules/LoopRooms.jsm");
Cu.import("resource:///modules/Chat.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

timerHandlers.startTimer = callback => callback();

var openChatOrig = Chat.open;

const kKey = "uGIs-kGbYt1hBBwjyW7MLQ";

// Rooms details as responded by the server.
const kRoomsResponses = new Map([
  ["_nxD4V4FflQ", {
    roomToken: "_nxD4V4FflQ",
    // Encrypted with roomKey "FliIGLUolW-xkKZVWstqKw".
    // roomKey is wrapped with kKey.
    context: {
      wrappedKey: "F3V27oPB+FgjFbVPML2PupONYqoIZ53XRU4BqG46Lr3eyIGumgCEqgjSe/MXAXiQ//8=",
      value: "df7B4SNxhOI44eJjQavCevADyCCxz6/DEZbkOkRUMVUxzS42FbzN6C2PqmCKDYUGyCJTwJ0jln8TLw==",
      alg: "AES-GCM"
    },
    roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
    maxSize: 2,
    ctime: 1405517546,
    participants: [{
      displayName: "Alexis",
      account: "alexis@example.com",
      roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
    }, {
      displayName: "Adam",
      roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
    }]
  }],
  ["QzBbvGmIZWU", {
    roomToken: "QzBbvGmIZWU",
    roomName: "Second Room Name",
    roomUrl: "http://localhost:3000/rooms/QzBbvGmIZWU",
    maxSize: 2,
    ctime: 140551741
  }],
  ["3jKS_Els9IU", {
    roomToken: "3jKS_Els9IU",
    roomName: "Third Room Name",
    roomUrl: "http://localhost:3000/rooms/3jKS_Els9IU",
    maxSize: 3,
    clientMaxSize: 2,
    ctime: 1405518241
  }]
]);

const kExpectedRooms = new Map([
  ["_nxD4V4FflQ", {
    roomToken: "_nxD4V4FflQ",
    context: {
      wrappedKey: "F3V27oPB+FgjFbVPML2PupONYqoIZ53XRU4BqG46Lr3eyIGumgCEqgjSe/MXAXiQ//8=",
      value: "df7B4SNxhOI44eJjQavCevADyCCxz6/DEZbkOkRUMVUxzS42FbzN6C2PqmCKDYUGyCJTwJ0jln8TLw==",
      alg: "AES-GCM"
    },
    decryptedContext: {
      roomName: "First Room Name"
    },
    roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ#FliIGLUolW-xkKZVWstqKw",
    roomKey: "FliIGLUolW-xkKZVWstqKw",
    maxSize: 2,
    ctime: 1405517546,
    participants: [{
      displayName: "Alexis",
      account: "alexis@example.com",
      roomConnectionId: "2a1787a6-4a73-43b5-ae3e-906ec1e763cb"
    }, {
      displayName: "Adam",
      roomConnectionId: "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
    }]
  }],
  ["QzBbvGmIZWU", {
    roomToken: "QzBbvGmIZWU",
    decryptedContext: {
      roomName: "Second Room Name"
    },
    roomUrl: "http://localhost:3000/rooms/QzBbvGmIZWU",
    maxSize: 2,
    ctime: 140551741
  }],
  ["3jKS_Els9IU", {
    roomToken: "3jKS_Els9IU",
    decryptedContext: {
      roomName: "Third Room Name"
    },
    roomUrl: "http://localhost:3000/rooms/3jKS_Els9IU",
    maxSize: 3,
    clientMaxSize: 2,
    ctime: 1405518241
  }]
]);

const kRoomDetail = {
  decryptedContext: {
    roomName: "First Room Name"
  },
  context: {
    wrappedKey: "wrappedKey",
    value: "encryptedValue",
    alg: "AES-GCM"
  },
  roomKey: "fakeKey",
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
    }, {
      displayName: "Ruharb",
      roomConnectionId: "5de6281c-6568-455f-af08-c0b0a973100e"
    }]
  },
  "5": {
    deleted: true
  }
};

const kCreateRoomProps = {
  decryptedContext: {
    roomName: "UX Discussion"
  },
  roomOwner: "Alexis",
  maxSize: 2
};

const kCreateRoomData = {
  roomToken: "_nxD4V4FflQ",
  roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
  expiresAt: 1405534180
};

const kChannelGuest = MozLoopService.channelIDs.roomsGuest;
const kChannelFxA = MozLoopService.channelIDs.roomsFxA;

const normalizeRoom = function(room) {
  let newRoom = extend({}, room);
  let name = newRoom.decryptedContext.roomName;

  for (let key of Object.getOwnPropertyNames(kRoomDetail)) {
    // Handle sub-objects if necessary (e.g. context, decryptedContext).
    if (typeof kRoomDetail[key] == "object") {
      newRoom[key] = extend({}, kRoomDetail[key]);
    } else {
      newRoom[key] = kRoomDetail[key];
    }
  }

  newRoom.decryptedContext.roomName = name;
  return newRoom;
};

// This compares rooms by normalizing the room fields so that the contents
// are the same between the two rooms - except for the room name
// (see normalizeRoom). This means we can detect if fields are missing, but
// we don't need to worry about the values being different, for example, in the
// case of expiry times.
const compareRooms = function(room1, room2) {
  Assert.deepEqual(normalizeRoom(room1), normalizeRoom(room2));
};

// LoopRooms emits various events. Test if they work as expected here.
var gExpectedAdds = [];
var gExpectedUpdates = [];
var gExpectedDeletes = [];
var gExpectedJoins = {};
var gExpectedLeaves = {};
var gExpectedRefresh = false;

const onRoomAdded = function(e, room) {
  let expectedIds = gExpectedAdds.map(expectedRoom => expectedRoom.roomToken);
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

const onRoomDeleted = function(e, room) {
  let idx = gExpectedDeletes.indexOf(room.roomToken);
  Assert.ok(idx > -1, "Deleted room should be expected");
  gExpectedDeletes.splice(idx, 1);
};

const onRoomJoined = function(e, room, participant) {
  let participants = gExpectedJoins[room.roomToken];
  Assert.ok(participants, "Participant should be expected to join");
  let idx = participants.indexOf(participant.roomConnectionId);
  Assert.ok(idx > -1, "Participant should be expected to join");
  participants.splice(idx, 1);
  if (!participants.length) {
    delete gExpectedJoins[room.roomToken];
  }
};

const onRoomLeft = function(e, room, participant) {
  let participants = gExpectedLeaves[room.roomToken];
  Assert.ok(participants, "Participant should be expected to leave");
  let idx = participants.indexOf(participant.roomConnectionId);
  Assert.ok(idx > -1, "Participant should be expected to leave");
  participants.splice(idx, 1);
  if (!participants.length) {
    delete gExpectedLeaves[room.roomToken];
  }
};

const onRefresh = function() {
  Assert.ok(gExpectedRefresh, "A refresh event should've been expected");
  gExpectedRefresh = false;
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

      Assert.equal(data.roomOwner, kCreateRoomProps.roomOwner);
      Assert.equal(data.maxSize, kCreateRoomProps.maxSize);
      Assert.ok(!("decryptedContext" in data), "should not have any decrypted data");
      Assert.ok("context" in data, "should have context");

      res.write(JSON.stringify(kCreateRoomData));
    } else if (req.queryString) {
      let qs = parseQueryString(req.queryString);
      let room = kRoomsResponses.get("_nxD4V4FflQ");
      room.participants = kRoomUpdates[qs.version].participants;
      room.deleted = kRoomUpdates[qs.version].deleted;
      res.write(JSON.stringify([room]));
    } else {
      res.write(JSON.stringify([...kRoomsResponses.values()]));
    }

    res.processAsync();
    res.finish();
  });

  function returnRoomDetails(res, roomDetail, roomName) {
    roomDetail.roomName = roomName;
    // The decrypted context and roomKey are never part of the server response.
    delete roomDetail.decryptedContext;
    delete roomDetail.roomKey;
    res.setStatusLine(null, 200, "OK");
    res.write(JSON.stringify(roomDetail));
    res.processAsync();
    res.finish();
  }

  function getJSONData(body) {
    return JSON.parse(CommonUtils.readBytesFromInputStream(body));
  }

  // Add a request handler for each room in the list.
  [...kRoomsResponses.values()].forEach(function(room) {
    loopServer.registerPathHandler("/rooms/" + encodeURIComponent(room.roomToken), (req, res) => {
      let roomDetail = extend({}, kRoomDetail);
      if (req.method == "POST") {
        let data = getJSONData(req.bodyInputStream);
        res.setStatusLine(null, 200, "OK");
        res.write(JSON.stringify(data));
        res.processAsync();
        res.finish();
      } else if (req.method == "PATCH") {
        let data = getJSONData(req.bodyInputStream);

        Assert.ok("context" in data, "should have encrypted context");
        // We return a fake encrypted name here as the context is
        // encrypted.
        returnRoomDetails(res, roomDetail, "fakeEncrypted");
      } else {
        roomDetail.context = room.context;
        res.setStatusLine(null, 200, "OK");
        res.write(JSON.stringify(roomDetail));
        res.processAsync();
        res.finish();
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
  gExpectedAdds.push(...kExpectedRooms.values());
  let rooms = yield LoopRooms.promise("getAll");
  Assert.equal(rooms.length, 3);
  for (let room of rooms) {
    compareRooms(kExpectedRooms.get(room.roomToken), room);
  }
});

// Test if fetching a room works correctly.
add_task(function* test_getRoom() {
  let roomToken = "_nxD4V4FflQ";
  let room = yield LoopRooms.promise("get", roomToken);
  Assert.deepEqual(room, kExpectedRooms.get(roomToken));
});

// Test if fetching a room with incorrect token or return values yields an error.
add_task(function* test_errorStates() {
  yield Assert.rejects(LoopRooms.promise("get", "error401"), /Not Found/, "Fetching a non-existent room should fail");
  yield Assert.rejects(LoopRooms.promise("get", "errorMalformed"), /SyntaxError/, "Wrong message format should reject");
});

// Test if creating a new room works as expected.
add_task(function* test_createRoom() {
  var expectedRoom = extend({}, kCreateRoomProps);
  expectedRoom.roomToken = kCreateRoomData.roomToken;

  gExpectedAdds.push(expectedRoom);
  let room = yield LoopRooms.promise("create", kCreateRoomProps);

  // We can't check the value of the key, but check we've got a # which indicates
  // there should be one.
  Assert.ok(room.roomUrl.includes("#"), "Created room url should have a key");
  var key = room.roomUrl.split("#")[1];
  Assert.ok(key.length, "Created room url should have non-zero length key");

  compareRooms(room, expectedRoom);
});

// Test if opening a new room window works correctly.
add_task(function* test_openRoom() {
  let openedUrl;
  Chat.open = function(contentWindow, options) {
    openedUrl = options.url;
  };

  LoopRooms.open("fakeToken");

  Assert.ok(openedUrl, "should open a chat window");

  // Stop the busy kicking in for following tests. (note: windowId can be 'fakeToken')
  let windowId = openedUrl.match(/about:loopconversation#(\w+)$/)[1];
  let windowData = MozLoopService.getConversationWindowData(windowId);

  Assert.equal(windowData.type, "room", "window data should contain room as the type");
  Assert.equal(windowData.roomToken, "fakeToken", "window data should have the roomToken");
  Assert.equal(LoopRooms.getNumParticipants("fakeToken"), 0, "LoopRooms getNumParticipants should contain no participants before join");
});

// Test if the rooms cache is refreshed after FxA signin or signout.
add_task(function* test_refresh() {
  gExpectedAdds.push(...kExpectedRooms.values());
  gExpectedRefresh = true;

  // Make the switch.
  MozLoopServiceInternal.fxAOAuthTokenData = { token_type: "bearer" };
  MozLoopServiceInternal.fxAOAuthProfile = {
    email: "fake@invalid.com",
    uid: "fake"
  };
  Services.prefs.setCharPref("loop.key.fxa", kKey);

  yield waitForCondition(() => !gExpectedRefresh);
  yield waitForCondition(() => gExpectedAdds.length === 0);

  gExpectedAdds.push(...kExpectedRooms.values());
  gExpectedRefresh = true;
  // Simulate a logout.
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  MozLoopServiceInternal.fxAOAuthProfile = null;

  yield waitForCondition(() => !gExpectedRefresh);
  yield waitForCondition(() => gExpectedAdds.length === 0);

  // Simulating a logout again shouldn't yield a refresh event.
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  MozLoopServiceInternal.fxAOAuthProfile = null;
});

// Test if push updates function as expected.
add_task(function* test_roomUpdates() {
  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedLeaves._nxD4V4FflQ = [
    "2a1787a6-4a73-43b5-ae3e-906ec1e763cb",
    "781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"
  ];
  roomsPushNotification("1", kChannelGuest);
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedLeaves).length === 0 &&
    gExpectedUpdates.length === 0);

  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedJoins._nxD4V4FflQ = ["2a1787a6-4a73-43b5-ae3e-906ec1e763cb"];
  roomsPushNotification("2", kChannelGuest);
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedJoins).length === 0 &&
    gExpectedUpdates.length === 0);

  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedJoins._nxD4V4FflQ = ["781f012b-f1ea-4ce1-9105-7cfc36fb4ec7"];
  gExpectedLeaves._nxD4V4FflQ = ["2a1787a6-4a73-43b5-ae3e-906ec1e763cb"];
  roomsPushNotification("3", kChannelGuest);
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedLeaves).length === 0 &&
    Object.getOwnPropertyNames(gExpectedJoins).length === 0 &&
    gExpectedUpdates.length === 0);

  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedJoins._nxD4V4FflQ = [
    "2a1787a6-4a73-43b5-ae3e-906ec1e763cb",
    "5de6281c-6568-455f-af08-c0b0a973100e"];
  roomsPushNotification("4", kChannelGuest);
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedJoins).length === 0 &&
    gExpectedUpdates.length === 0);
});

// Test if push updates' channelIDs are respected.
add_task(function* test_channelIdsRespected() {
  function badRoomJoin() {
    Assert.ok(false, "Unexpected 'joined' event emitted!");
  }
  LoopRooms.on("join", badRoomJoin);
  roomsPushNotification("4", kChannelFxA);
  LoopRooms.off("join", badRoomJoin);

  // Set the userProfile to look like we're logged into FxA.
  MozLoopServiceInternal.fxAOAuthTokenData = { token_type: "bearer" };
  MozLoopServiceInternal.fxAOAuthProfile = { email: "fake@invalid.com" };

  gExpectedUpdates.push("_nxD4V4FflQ");
  gExpectedLeaves._nxD4V4FflQ = [
    "2a1787a6-4a73-43b5-ae3e-906ec1e763cb",
    "5de6281c-6568-455f-af08-c0b0a973100e"
  ];
  roomsPushNotification("3", kChannelFxA);
  yield waitForCondition(() => Object.getOwnPropertyNames(gExpectedLeaves).length === 0 &&
    gExpectedUpdates.length === 0);
});

// Test if joining a room as Guest works as expected.
add_task(function* test_joinRoomGuest() {
  MozLoopServiceInternal.fxAOAuthTokenData = null;
  MozLoopServiceInternal.fxAOAuthProfile = null;

  let roomToken = "_nxD4V4FflQ";
  let joinedData = yield LoopRooms.promise("join", roomToken, "guest");
  Assert.equal(joinedData.action, "join");
  Assert.equal(LoopRooms.getNumParticipants("_nxD4V4FflQ"), 1, "LoopRooms getNumParticipants should have participant after join");
  Assert.equal(LoopRoomsInternal.getNumParticipants("_nxD4V4FflQ"), 1, "LoopRoomsInternal getNumParticipants should have participant after join");
});

// Test if refreshing a room works as expected.
add_task(function* test_refreshMembership() {
  let roomToken = "_nxD4V4FflQ";
  let refreshedData = yield LoopRooms.promise("refreshMembership", roomToken,
    "fakeSessionToken");
  Assert.equal(refreshedData.action, "refresh");
  Assert.equal(refreshedData.sessionToken, "fakeSessionToken");
  Assert.equal(LoopRooms.getNumParticipants("_nxD4V4FflQ"), 1, "LoopRooms getNumParticipants should still have participant after refresh");
});

// Test if leaving a room works as expected.
add_task(function* test_leaveRoom() {
  let roomToken = "_nxD4V4FflQ";
  let leaveData = yield LoopRooms.promise("leave", roomToken, "fakeLeaveSessionToken");
  Assert.equal(leaveData.action, "leave");
  Assert.equal(leaveData.sessionToken, "fakeLeaveSessionToken");
});

// Test if sending connection status works as expected.
add_task(function* test_sendConnectionStatus() {
  let roomToken = "_nxD4V4FflQ";
  let extraData = {
    event: "Session.connectionDestroyed",
    state: "test",
    connections: 1,
    sendStreams: 2,
    recvStreams: 3
  };
  let statusData = yield LoopRooms.promise("sendConnectionStatus", roomToken,
    "fakeStatusSessionToken", extraData
  );
  Assert.equal(statusData.sessionToken, "fakeStatusSessionToken");

  extraData.action = "status";
  extraData.sessionToken = "fakeStatusSessionToken";
  Assert.deepEqual(statusData, extraData);
});

// Test if updating a room works as expected.
add_task(function* test_updateRoom() {
  let roomToken = "_nxD4V4FflQ";
  let fakeContext = {
    description: "Hello, is it me you're looking for?",
    location: "https://example.com",
    thumbnail: "https://example.com/empty.gif"
  };
  let updateData = yield LoopRooms.promise("update", roomToken, {
    roomName: "fakeEncrypted",
    urls: [fakeContext]
  });
  Assert.equal(updateData.roomName, "fakeEncrypted", "should have set the new name");
  let contextURL = updateData.decryptedContext.urls[0];
  Assert.equal(contextURL.description, contextURL.description,
    "should have set the new context URL description");
  Assert.equal(contextURL.location, contextURL.location,
    "should have set the new context URL location");
  Assert.equal(contextURL.thumbnail, contextURL.thumbnail,
    "should have set the new context URL thumbnail");
});

add_task(function* test_updateRoom_nameOnly() {
  let roomToken = "_nxD4V4FflQ";
  let updateData = yield LoopRooms.promise("update", roomToken, {
    roomName: "fakeEncrypted"
  });
  Assert.equal(updateData.roomName, "fakeEncrypted", "should have set the new name");
});

add_task(function* test_updateRoom_domains() {
  Services.prefs.setBoolPref("loop.logDomains", true);

  let roomToken = "_nxD4V4FflQ";
  let addContext = url => LoopRooms.promise("update", roomToken, {
    urls: [{ location: url }]
  });

  // Make sure one domain context is counted.
  yield addContext("https://www.mozilla.org");
  Assert.deepEqual(yield LoopRooms.promise("logDomains", roomToken), {
    action: "logDomain",
    domains: [{ count: 1, domain: "mozilla.org" }]
  });

  // Make sure multiple pages on a domain are counted.
  yield addContext("https://www.mozilla.org/foo");
  yield addContext("https://www.mozilla.org/bar");
  yield addContext("https://www.mozilla.org/baz");
  Assert.deepEqual(yield LoopRooms.promise("logDomains", roomToken), {
    action: "logDomain",
    domains: [{ count: 3, domain: "mozilla.org" }]
  });

  // Make sure multiple subdomains combine to one domain are counted.
  yield addContext("https://foo.mozilla.org");
  yield addContext("https://bar.baz.mozilla.org");
  Assert.deepEqual(yield LoopRooms.promise("logDomains", roomToken), {
    action: "logDomain",
    domains: [{ count: 2, domain: "mozilla.org" }]
  });

  // Make sure multiple domains are counted, e.g., switching tabs.
  yield addContext("https://www.yahoo.com");
  yield addContext("https://www.mozilla.org");
  yield addContext("https://www.yahoo.com");
  Assert.deepEqual(yield LoopRooms.promise("logDomains", roomToken), {
    action: "logDomain",
    domains: [{ count: 2, domain: "yahoo.com" },
              { count: 1, domain: "mozilla.org" }]
  });

  // Make sure only whitelisted domains are counted.
  yield addContext("https://www.mozilla.org");
  yield addContext("https://some.obscure.fakedomain.net");
  yield addContext("http://localhost");
  yield addContext("http://127.0.0.1");
  yield addContext("http://blogspot.com");
  yield addContext("http://com");
  Assert.deepEqual(yield LoopRooms.promise("logDomains", roomToken), {
    action: "logDomain",
    domains: [{ count: 1, domain: "mozilla.org" }]
  });

  // Make sure switching room tokens only keeps the latest.
  roomToken = "QzBbvGmIZWU";
  yield addContext("https://www.mozilla.org");
  roomToken = "_nxD4V4FflQ";
  yield addContext("https://www.yahoo.com");
  Assert.deepEqual(yield LoopRooms.promise("logDomains", roomToken), {
    action: "logDomain",
    domains: [{ count: 1, domain: "yahoo.com" }]
  });

  // Make sure nothing is logged if disabled.
  Services.prefs.setBoolPref("loop.logDomains", false);
  yield addContext("https://www.mozilla.org");
  Assert.equal(yield LoopRooms.promise("logDomains", roomToken), null);

  Services.prefs.clearUserPref("loop.logDomains");
});

add_task(function* test_roomDeleteNotifications() {
  gExpectedDeletes.push("_nxD4V4FflQ");
  roomsPushNotification("5", kChannelGuest);
  yield waitForCondition(() => gExpectedDeletes.length === 0);
});

// Test if deleting a room works as expected.
add_task(function* test_deleteRoom() {
  let roomToken = "QzBbvGmIZWU";
  gExpectedDeletes.push(roomToken);
  let deletedRoom = yield LoopRooms.promise("delete", roomToken);
  Assert.equal(deletedRoom.roomToken, roomToken);
  let rooms = yield LoopRooms.promise("getAll");
  Assert.ok(!rooms.some((room) => room.roomToken == roomToken));
});

// Test if the event emitter implementation doesn't leak and is working as expected.
add_task(function* () {
  Assert.strictEqual(gExpectedAdds.length, 0, "No room additions should be expected anymore");
  Assert.strictEqual(gExpectedUpdates.length, 0, "No room updates should be expected anymore");
  Assert.strictEqual(gExpectedDeletes.length, 0, "No room deletes should be expected anymore");
  Assert.strictEqual(Object.getOwnPropertyNames(gExpectedJoins).length, 0,
                     "No room joins should be expected anymore");
  Assert.strictEqual(Object.getOwnPropertyNames(gExpectedLeaves).length, 0,
                     "No room leaves should be expected anymore");
  Assert.ok(!gExpectedRefresh, "No refreshes should be expected anymore");
 });

function run_test() {
  setupFakeLoopServer();

  Services.prefs.setCharPref("loop.key", kKey);

  LoopRooms.on("add", onRoomAdded);
  LoopRooms.on("update", onRoomUpdated);
  LoopRooms.on("delete", onRoomDeleted);
  LoopRooms.on("joined", onRoomJoined);
  LoopRooms.on("left", onRoomLeft);
  LoopRooms.on("refresh", onRefresh);

  do_register_cleanup(function() {
    // Revert original Chat.open implementation
    Chat.open = openChatOrig;
    Services.prefs.clearUserPref("loop.key");
    Services.prefs.clearUserPref("loop.key.fxa");

    MozLoopServiceInternal.fxAOAuthTokenData = null;
    MozLoopServiceInternal.fxAOAuthProfile = null;

    LoopRooms.off("add", onRoomAdded);
    LoopRooms.off("update", onRoomUpdated);
    LoopRooms.off("delete", onRoomDeleted);
    LoopRooms.off("joined", onRoomJoined);
    LoopRooms.off("left", onRoomLeft);
    LoopRooms.off("refresh", onRefresh);
  });

  run_next_test();
}
