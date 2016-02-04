/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* exported run_test */

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Promise.jsm");

timerHandlers.startTimer = callback => callback();

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

// LoopRooms emits various events. Test if they work as expected here.
var gQueuedResponses = [];

add_task(function* setup_server() {
  loopServer.registerPathHandler("/registration", (req, res) => {
    res.setStatusLine(null, 200, "OK");
    res.processAsync();
    res.finish();
  });

  loopServer.registerPathHandler("/rooms", (req, res) => {
    res.processAsync();

    gQueuedResponses.push({
      req: req,
      res: res
    });
  });

  mockPushHandler.registrationPushURL = kEndPointUrl;

  yield MozLoopService.promiseRegisteredWithServers();
});

function handleQueuedResponses() {
  gQueuedResponses.forEach((xhr) => {
    xhr.res.setStatusLine(null, 200, "OK");
    xhr.res.write(JSON.stringify([...kRoomsResponses.values()]));
    xhr.res.finish();
  });
  gQueuedResponses = [];
}

function rejectQueuedResponses() {
  gQueuedResponses.forEach((xhr) => {
    xhr.res.setStatusLine(null, 404, "Not found");
    xhr.res.finish();
  });
  gQueuedResponses = [];
}

add_task(function* test_getAllRooms_failure() {
  let firstGetAllPromise = LoopRooms.promise("getAll");

  // Check the first one hasn't been resolved yet.
  yield waitForCondition(() => gQueuedResponses.length == 1);

  rejectQueuedResponses();

  yield Assert.rejects(firstGetAllPromise, "Not found", "should throw failure");
});

// Test fetching getAll with multiple requests.
add_task(function* test_getAllRooms_multiple() {
  let resolvedFirstCall = false;
  let resolvedSecondCall = false;

  let firstGetAllPromise = LoopRooms.promise("getAll").then(() => {
    resolvedFirstCall = true;
  });

  // Check the first one hasn't been resolved yet.
  yield waitForCondition(() => gQueuedResponses.length == 1);
  Assert.equal(resolvedFirstCall, false);

  // Queue a second one.
  let secondGetAllPromise = LoopRooms.promise("getAll").then(() => {
    resolvedSecondCall = true;
  });

  // Check the second one hasn't been resolved yet.
  yield waitForCondition(() => gQueuedResponses.length == 1);
  Assert.equal(resolvedFirstCall, false);
  Assert.equal(resolvedSecondCall, false);

  // Now resolve the requests, and check the calls returned.
  handleQueuedResponses();

  // Ensure the promises have resolved.
  yield firstGetAllPromise;
  yield secondGetAllPromise;

  yield waitForCondition(() => resolvedFirstCall == true);
  yield waitForCondition(() => resolvedSecondCall == true);
});

function run_test() {
  setupFakeLoopServer();

  Services.prefs.setCharPref("loop.key", kKey);

  do_register_cleanup(function() {
    Services.prefs.clearUserPref("loop.key");
    Services.prefs.clearUserPref("loop.key.fxa");

    MozLoopServiceInternal.fxAOAuthTokenData = null;
    MozLoopServiceInternal.fxAOAuthProfile = null;
  });

  run_next_test();
}
