/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PushDB, PushService, PushServiceWebSocket } = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs({
    requestTimeout: 1000,
    retryBaseInterval: 150,
  });
  run_next_test();
}

add_task(async function test_register_request_queue() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  let onHello;
  let helloPromise = new Promise(
    resolve =>
      (onHello = after(2, function onHelloReceived(request) {
        this.serverSendMsg(
          JSON.stringify({
            messageType: "hello",
            status: 200,
            uaid: "54b08a9e-59c6-4ed7-bb54-f4fd60d6f606",
          })
        );
        resolve();
      }))
  );

  PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello,
        onRegister() {
          ok(false, "Should cancel timed-out requests");
        },
      });
    },
  });

  let firstRegister = PushService.register({
    scope: "https://example.com/page/1",
    originAttributes: ChromeUtils.originAttributesToSuffix({
      inIsolatedMozBrowser: false,
    }),
  });
  let secondRegister = PushService.register({
    scope: "https://example.com/page/1",
    originAttributes: ChromeUtils.originAttributesToSuffix({
      inIsolatedMozBrowser: false,
    }),
  });

  await Promise.all([
    // eslint-disable-next-line mozilla/rejects-requires-await
    Assert.rejects(
      firstRegister,
      /Registration error/,
      "Should time out the first request"
    ),
    // eslint-disable-next-line mozilla/rejects-requires-await
    Assert.rejects(
      secondRegister,
      /Registration error/,
      "Should time out the second request"
    ),
  ]);

  await helloPromise;
});
