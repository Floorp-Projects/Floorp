/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PushDB, PushService, PushServiceWebSocket } = serviceExports;

const userAgentID = "a4be0df9-b16d-4b5f-8f58-0f93b6f1e23d";
const channelID = "e1944e0b-48df-45e7-bdc0-d1fbaa7986d3";

function run_test() {
  do_get_profile();
  setPrefs({
    requestTimeout: 1000,
    retryBaseInterval: 150,
  });
  run_next_test();
}

add_task(async function test_register_timeout() {
  let handshakes = 0;
  let timeoutDone;
  let timeoutPromise = new Promise(resolve => (timeoutDone = resolve));
  let registers = 0;

  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  PushServiceWebSocket._generateID = () => channelID;
  PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          if (registers > 0) {
            equal(
              request.uaid,
              userAgentID,
              "Should include device ID on reconnect with subscriptions"
            );
          } else {
            ok(
              !request.uaid,
              "Should not send UAID in handshake without local subscriptions"
            );
          }
          if (handshakes > 1) {
            ok(false, "Unexpected reconnect attempt " + handshakes);
          }
          handshakes++;
          this.serverSendMsg(
            JSON.stringify({
              messageType: "hello",
              status: 200,
              uaid: userAgentID,
            })
          );
        },
        onRegister(request) {
          equal(
            request.channelID,
            channelID,
            "Wrong channel ID in register request"
          );
          // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
          setTimeout(() => {
            // Should ignore replies for timed-out requests.
            this.serverSendMsg(
              JSON.stringify({
                messageType: "register",
                status: 200,
                channelID,
                uaid: userAgentID,
                pushEndpoint: "https://example.com/update/timeout",
              })
            );
            registers++;
            timeoutDone();
          }, 2000);
        },
      });
    },
  });

  await Assert.rejects(
    PushService.register({
      scope: "https://example.net/page/timeout",
      originAttributes: ChromeUtils.originAttributesToSuffix({
        inIsolatedMozBrowser: false,
      }),
    }),
    /Registration error/,
    "Expected error for request timeout"
  );

  let record = await db.getByKeyID(channelID);
  ok(!record, "Should not store records for timed-out responses");

  await timeoutPromise;
  equal(registers, 1, "Should not handle timed-out register requests");
});
