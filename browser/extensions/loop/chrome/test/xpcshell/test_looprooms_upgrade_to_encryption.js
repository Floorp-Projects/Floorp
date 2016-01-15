/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported run_test */

Cu.import("resource://services-common/utils.js");

const loopCrypto = Cu.import("chrome://loop/content/shared/js/crypto.js", {}).LoopCrypto;

var gTimerArgs = [];

timerHandlers.startTimer = function(callback, delay) {
  gTimerArgs.push({ callback, delay });
  return gTimerArgs.length;
};

var gRoomPatches = [];

const kContextEnabledPref = "loop.contextInConverations.enabled";

const kFxAKey = "uGIs-kGbYt1hBBwjyW7MLQ";

// Rooms details as responded by the server.
const kRoomsResponses = new Map([
  ["_nxD4V4FflQ", {
    roomToken: "_nxD4V4FflQ",
    roomName: "First Room Name",
    roomUrl: "http://localhost:3000/rooms/_nxD4V4FflQ"
  }],
  ["QzBbvGmIZWU", {
    roomToken: "QzBbvGmIZWU",
    roomName: "Loopy Discussion",
    roomUrl: "http://localhost:3000/rooms/QzBbvGmIZWU"
  }]
]);

// This is a cut-down version of the one in test_looprooms.js.
add_task(function* setup_server() {
  loopServer.registerPathHandler("/registration", (req, res) => {
    res.setStatusLine(null, 200, "OK");
    res.processAsync();
    res.finish();
  });

  loopServer.registerPathHandler("/rooms", (req, res) => {
    res.setStatusLine(null, 200, "OK");

    res.write(JSON.stringify([...kRoomsResponses.values()]));

    res.processAsync();
    res.finish();
  });

  function getJSONData(body) {
    return JSON.parse(CommonUtils.readBytesFromInputStream(body));
  }

  // Add a request handler for each room in the list.
  [...kRoomsResponses.values()].forEach(function(room) {
    loopServer.registerPathHandler("/rooms/" + encodeURIComponent(room.roomToken), (req, res) => {
      let roomDetail = extend({}, room);
      if (req.method == "PATCH") {
        let data = getJSONData(req.bodyInputStream);
        Assert.ok("context" in data, "should have encrypted context");
        gRoomPatches.push(data);
        delete roomDetail.roomName;
        roomDetail.context = data.context;
        res.setStatusLine(null, 200, "OK");
        res.write(JSON.stringify(roomDetail));
        res.processAsync();
        res.finish();
      } else {
        res.setStatusLine(null, 200, "OK");
        res.write(JSON.stringify(room));
        res.processAsync();
        res.finish();
      }
    });
  });

  mockPushHandler.registrationPushURL = kEndPointUrl;

  yield MozLoopService.promiseRegisteredWithServers();
});

// Test if getting rooms saves unknown keys correctly.
add_task(function* test_get_rooms_upgrades_to_encryption() {
  let rooms = yield LoopRooms.promise("getAll");

  // Check that we've saved the encryption keys correctly.
  Assert.equal(LoopRoomsInternal.encryptionQueue.queue.length, 2, "Should have two rooms queued");
  Assert.equal(gTimerArgs.length, 1, "Should have started a timer");

  // Now pretend the timer has fired.
  yield gTimerArgs[0].callback();

  Assert.equal(gRoomPatches.length, 1, "Should have patched one room");
  Assert.equal(gTimerArgs.length, 2, "Should have started a second timer");

  yield gTimerArgs[1].callback();

  Assert.equal(gRoomPatches.length, 2, "Should have patches a second room");
  Assert.equal(gTimerArgs.length, 2, "Should not have queued another timer");

  // Now check that we've got the right data stored in the rooms.
  rooms = yield LoopRooms.promise("getAll");

  Assert.equal(rooms.length, 2, "Should have two rooms");

  // We have to decrypt the info, no other way.
  for (let room of rooms) {
    let roomData = yield loopCrypto.decryptBytes(room.roomKey, room.context.value);

    Assert.deepEqual(JSON.parse(roomData),
      { roomName: kRoomsResponses.get(room.roomToken).roomName },
      "Should have encrypted the data correctly");
  }
});

function run_test() {
  setupFakeLoopServer();

  Services.prefs.setCharPref("loop.key.fxa", kFxAKey);
  Services.prefs.setBoolPref(kContextEnabledPref, true);

  // Pretend we're signed into FxA.
  MozLoopServiceInternal.fxAOAuthTokenData = { token_type: "bearer" };
  MozLoopServiceInternal.fxAOAuthProfile = { email: "fake@invalid.com" };

  do_register_cleanup(function() {
    Services.prefs.clearUserPref(kContextEnabledPref);
    Services.prefs.clearUserPref("loop.key.fxa");

    MozLoopServiceInternal.fxAOAuthTokenData = null;
    MozLoopServiceInternal.fxAOAuthProfile = null;
  });

  run_next_test();
}
