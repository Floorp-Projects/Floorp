/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PushDB, PushService } = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(async function test_unregister_empty_scope() {
  let handshakeDone;
  let handshakePromise = new Promise(resolve => (handshakeDone = resolve));
  PushService.init({
    serverURI: "wss://push.example.org/",
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(
            JSON.stringify({
              messageType: "hello",
              status: 200,
              uaid: "5619557c-86fe-4711-8078-d1fd6987aef7",
            })
          );
          handshakeDone();
        },
      });
    },
  });
  await handshakePromise;

  await Assert.rejects(
    PushService.unregister({
      scope: "",
      originAttributes: ChromeUtils.originAttributesToSuffix({
        inIsolatedMozBrowser: false,
      }),
    }),
    /Invalid page record/,
    "Expected error for empty endpoint"
  );
});
