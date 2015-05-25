/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource://services-common/utils.js");
const { LOOP_ROOMS_CACHE_FILENAME } = Cu.import("resource:///modules/loop/LoopRoomsCache.jsm", {});

const kContextEnabledPref = "loop.contextInConverations.enabled";

const kFxAKey = "uGIs-kGbYt1hBBwjyW7MLQ";

// Rooms details as responded by the server.
const kRoomsResponses = new Map([
  ["_nxD4V4FflQ", {
    // Encrypted with roomKey "FliIGLUolW-xkKZVWstqKw".
    // roomKey is wrapped with kFxAKey.
    context: {
      wrappedKey: "F3V27oPB+FgjFbVPML2PupONYqoIZ53XRU4BqG46Lr3eyIGumgCEqgjSe/MXAXiQ//8=",
      value: "df7B4SNxhOI44eJjQavCevADyCCxz6/DEZbkOkRUMVUxzS42FbzN6C2PqmCKDYUGyCJTwJ0jln8TLw==",
      alg: "AES-GCM"
    },
    roomToken: "_nxD4V4FflQ",
    roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ"
  }],
  ["QzBbvGmIZWU", {
    context: {
      wrappedKey: "AFu7WwFNjhWR5J6L8ks7S6H/1ktYVEw3yt1eIIWVaMabZaB3vh5612/FNzua4oS2oCM=",
      value: "sqj+xRNEty8K3Q1gSMd5bIUYKu34JfiO2+LIMlJrOetFIbJdBoQ+U8JZNaTFl6Qp3RULZ41x0zeSBSk=",
      alg: "AES-GCM"
    },
    roomToken: "QzBbvGmIZWU",
    roomUrl: "http://localhost:3000/rooms/QzBbvGmIZWU"
  }]
]);

const kExpectedRooms = new Map([
  ["_nxD4V4FflQ", {
    context: {
      wrappedKey: "F3V27oPB+FgjFbVPML2PupONYqoIZ53XRU4BqG46Lr3eyIGumgCEqgjSe/MXAXiQ//8=",
      value: "df7B4SNxhOI44eJjQavCevADyCCxz6/DEZbkOkRUMVUxzS42FbzN6C2PqmCKDYUGyCJTwJ0jln8TLw==",
      alg: "AES-GCM"
    },
    decryptedContext: {
      roomName: "First Room Name"
    },
    roomKey: "FliIGLUolW-xkKZVWstqKw",
    roomToken: "_nxD4V4FflQ",
    roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ#FliIGLUolW-xkKZVWstqKw"
  }],
  ["QzBbvGmIZWU", {
    context: {
      wrappedKey: "AFu7WwFNjhWR5J6L8ks7S6H/1ktYVEw3yt1eIIWVaMabZaB3vh5612/FNzua4oS2oCM=",
      value: "sqj+xRNEty8K3Q1gSMd5bIUYKu34JfiO2+LIMlJrOetFIbJdBoQ+U8JZNaTFl6Qp3RULZ41x0zeSBSk=",
      alg: "AES-GCM"
    },
    decryptedContext: {
      roomName: "Loopy Discussion"
    },
    roomKey: "h2H8Sa9QxLCTTiXNmJVtRA",
    roomToken: "QzBbvGmIZWU",
    roomUrl: "http://localhost:3000/rooms/QzBbvGmIZWU"
  }]
]);

const kCreateRoomProps = {
  decryptedContext: {
    roomName: "Say Hello"
  },
  roomOwner: "Gavin",
  maxSize: 2
};

const kCreateRoomData = {
  roomToken: "Vo2BFQqIaAM",
  roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ",
  expiresAt: 1405534180
};

function getCachePath() {
  return OS.Path.join(OS.Constants.Path.profileDir, LOOP_ROOMS_CACHE_FILENAME);
}

function readRoomsCache() {
  return CommonUtils.readJSON(getCachePath());
}

function saveRoomsCache(contents) {
  delete LoopRoomsInternal.roomsCache._cache;
  return CommonUtils.writeJSON(contents, getCachePath());
}

function clearRoomsCache() {
  return LoopRoomsInternal.roomsCache.clear();
}

// This is a cut-down version of the one in test_looprooms.js.
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

      Assert.ok(!("decryptedContext" in data), "should not have any decrypted data");
      Assert.ok("context" in data, "should have context");

      res.write(JSON.stringify(kCreateRoomData));
    } else {
      res.write(JSON.stringify([...kRoomsResponses.values()]));
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

  function getJSONData(body) {
    return JSON.parse(CommonUtils.readBytesFromInputStream(body));
  }

  // Add a request handler for each room in the list.
  [...kRoomsResponses.values()].forEach(function(room) {
    loopServer.registerPathHandler("/rooms/" + encodeURIComponent(room.roomToken), (req, res) => {
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
        returnRoomDetails(res, "fakeEncrypted");
      } else {
        res.setStatusLine(null, 200, "OK");
        res.write(JSON.stringify(room));
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


// Test if getting rooms saves unknown keys correctly.
add_task(function* test_get_rooms_saves_unknown_keys() {
  let rooms = yield LoopRooms.promise("getAll");

  // Check that we've saved the encryption keys correctly.
  let roomsCache = yield readRoomsCache();
  for (let room of [...kExpectedRooms.values()]) {
    if (room.context.wrappedKey) {
      Assert.equal(roomsCache[LOOP_SESSION_TYPE.FXA][room.roomToken].key, room.roomKey);
    }
  }

  yield clearRoomsCache();
});

// Test that when we get a room it updates the saved key if it is different.
add_task(function* test_get_rooms_saves_different_keys() {
  let roomsCache = {};
  roomsCache[LOOP_SESSION_TYPE.FXA] = {
    QzBbvGmIZWU: {key: "fakeKey"}
  };
  yield saveRoomsCache(roomsCache);

  const kRoomToken = "QzBbvGmIZWU";

  let room = yield LoopRooms.promise("get", kRoomToken);

  // Check that we've saved the encryption keys correctly.
  roomsCache = yield readRoomsCache();

  Assert.notEqual(roomsCache[LOOP_SESSION_TYPE.FXA][kRoomToken].key, "fakeKey");
  Assert.equal(roomsCache[LOOP_SESSION_TYPE.FXA][kRoomToken].key, room.roomKey);

  yield clearRoomsCache();
});

// Test that if roomKey decryption fails, the saved key is used for decryption.
add_task(function* test_get_rooms_uses_saved_key() {
  const kRoomToken = "_nxD4V4FflQ";
  const kExpected = kExpectedRooms.get(kRoomToken);

  let roomsCache = {};
  roomsCache[LOOP_SESSION_TYPE.FXA] = {
    "_nxD4V4FflQ": {key: kExpected.roomKey}
  };
  yield saveRoomsCache(roomsCache);

  // Change the encryption key for FxA, so that decoding the room key will break.
  Services.prefs.setCharPref("loop.key.fxa", "invalidKey");

  let room = yield LoopRooms.promise("get", kRoomToken);

  Assert.deepEqual(room, kExpected);

  Services.prefs.setCharPref("loop.key.fxa", kFxAKey);
  yield clearRoomsCache();
});

// Test that when a room is created the new key is saved.
add_task(function* test_create_room_saves_key() {
  let room = yield LoopRooms.promise("create", kCreateRoomProps);

  let roomsCache = yield readRoomsCache();

  Assert.equal(roomsCache[LOOP_SESSION_TYPE.FXA][room.roomToken].key, room.roomKey);

  yield clearRoomsCache();
});

function run_test() {
  setupFakeLoopServer();

  Services.prefs.setCharPref("loop.key.fxa", kFxAKey);
  Services.prefs.setBoolPref(kContextEnabledPref, true);

  // Pretend we're signed into FxA.
  MozLoopServiceInternal.fxAOAuthTokenData = { token_type: "bearer" };
  MozLoopServiceInternal.fxAOAuthProfile = { email: "fake@invalid.com" };

  do_register_cleanup(function () {
    Services.prefs.clearUserPref(kContextEnabledPref);
    Services.prefs.clearUserPref("loop.key.fxa");

    MozLoopServiceInternal.fxAOAuthTokenData = null;
    MozLoopServiceInternal.fxAOAuthProfile = null;
  });

  run_next_test();
}
