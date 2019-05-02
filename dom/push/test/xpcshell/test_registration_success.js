/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = "997ee7ba-36b1-4526-ae9e-2d3f38d6efe8";

function run_test() {
  do_get_profile();
  setPrefs({userAgentID});
  run_next_test();
}

add_task(async function test_registration_success() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => { return db.drop().then(_ => db.close()); });
  let records = [{
    channelID: "bf001fe0-2684-42f2-bc4d-a3e14b11dd5b",
    pushEndpoint: "https://example.com/update/same-manifest/1",
    scope: "https://example.net/a",
    originAttributes: "",
    version: 5,
    quota: Infinity,
  }];
  for (let record of records) {
    await db.put(record);
  }

  let handshakeDone;
  let handshakePromise = new Promise(resolve => handshakeDone = resolve);
  PushService.init({
    serverURI: "wss://push.example.org/",
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          equal(request.uaid, userAgentID, "Wrong device ID in handshake");
          this.serverSendMsg(JSON.stringify({
            messageType: "hello",
            status: 200,
            uaid: userAgentID,
          }));
          handshakeDone();
        },
      });
    },
  });

  await handshakePromise;

  let registration = await PushService.registration({
    scope: "https://example.net/a",
    originAttributes: "",
  });
  equal(
    registration.endpoint,
    "https://example.com/update/same-manifest/1",
    "Wrong push endpoint for scope"
  );
  equal(registration.version, 5, "Wrong version for scope");
});
