/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PushDB, PushService, PushServiceWebSocket } = serviceExports;

const userAgentID = "b2546987-4f63-49b1-99f7-739cd3c40e44";
const channelID = "35a820f7-d7dd-43b3-af21-d65352212ae3";

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 1000,
    retryBaseInterval: 150,
  });
  run_next_test();
}

add_task(async function test_register_rollback() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  let handshakes = 0;
  let registers = 0;
  let unregisterDone;
  let unregisterPromise = new Promise(resolve => (unregisterDone = resolve));
  PushServiceWebSocket._generateID = () => channelID;
  PushService.init({
    serverURI: "wss://push.example.org/",
    db: makeStub(db, {
      put(prev, record) {
        return Promise.reject("universe has imploded");
      },
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          handshakes++;
          if (registers > 0) {
            equal(request.uaid, userAgentID, "Handshake: wrong device ID");
          } else {
            ok(
              !request.uaid,
              "Should not send UAID in handshake without local subscriptions"
            );
          }
          this.serverSendMsg(
            JSON.stringify({
              messageType: "hello",
              status: 200,
              uaid: userAgentID,
            })
          );
        },
        onRegister(request) {
          equal(request.channelID, channelID, "Register: wrong channel ID");
          registers++;
          this.serverSendMsg(
            JSON.stringify({
              messageType: "register",
              status: 200,
              uaid: userAgentID,
              channelID,
              pushEndpoint: "https://example.com/update/rollback",
            })
          );
        },
        onUnregister(request) {
          equal(request.channelID, channelID, "Unregister: wrong channel ID");
          equal(request.code, 200, "Expected manual unregister reason");
          this.serverSendMsg(
            JSON.stringify({
              messageType: "unregister",
              status: 200,
              channelID,
            })
          );
          unregisterDone();
        },
      });
    },
  });

  // Should return a rejected promise if storage fails.
  await Assert.rejects(
    PushService.register({
      scope: "https://example.com/storage-error",
      originAttributes: ChromeUtils.originAttributesToSuffix({
        inIsolatedMozBrowser: false,
      }),
    }),
    /universe has imploded/,
    "Expected error for unregister database failure"
  );

  // Should send an out-of-band unregister request.
  await unregisterPromise;
  equal(handshakes, 1, "Wrong handshake count");
  equal(registers, 1, "Wrong register count");
});
