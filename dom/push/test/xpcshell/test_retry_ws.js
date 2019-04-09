/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = "05f7b940-51b6-4b6f-8032-b83ebb577ded";

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    pingInterval: 2000,
    retryBaseInterval: 25,
  });
  run_next_test();
}

add_task(async function test_ws_retry() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => { return db.drop().then(_ => db.close()); });

  await db.put({
    channelID: "61770ba9-2d57-4134-b949-d40404630d5b",
    pushEndpoint: "https://example.org/push/1",
    scope: "https://example.net/push/1",
    version: 1,
    originAttributes: "",
    quota: Infinity,
  });

  // Use a mock timer to avoid waiting for the backoff interval.
  let reconnects = 0;
  PushServiceWebSocket._backoffTimer = {
    init(observer, delay, type) {
      reconnects++;
      ok(delay >= 5 && delay <= 2000, `Backoff delay ${
        delay} out of range for attempt ${reconnects}`);
      observer.observe(this, "timer-callback", null);
    },

    cancel() {},
  };

  let handshakeDone;
  let handshakePromise = new Promise(resolve => handshakeDone = resolve);
  PushService.init({
    serverURI: "wss://push.example.org/",
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          if (reconnects == 10) {
            this.serverSendMsg(JSON.stringify({
              messageType: "hello",
              status: 200,
              uaid: userAgentID,
            }));
            handshakeDone();
            return;
          }
          this.serverInterrupt();
        },
      });
    },
  });

  await handshakePromise;
});
