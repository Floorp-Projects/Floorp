/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = "fbe865a6-aeb8-446f-873c-aeebdb8d493c";
const channelID = "db0a7021-ec2d-4bd3-8802-7a6966f10ed8";

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
  });
  run_next_test();
}

add_task(async function test_unregister_success() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => { return db.drop().then(_ => db.close()); });
  await db.put({
    channelID,
    pushEndpoint: "https://example.org/update/unregister-success",
    scope: "https://example.com/page/unregister-success",
    originAttributes: "",
    version: 1,
    quota: Infinity,
  });

  let unregisterDone;
  let unregisterPromise = new Promise(resolve => unregisterDone = resolve);
  PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: "hello",
            status: 200,
            uaid: userAgentID,
            use_webpush: true,
          }));
        },
        onUnregister(request) {
          equal(request.channelID, channelID, "Should include the channel ID");
          equal(request.code, 200, "Expected manual unregister reason");
          this.serverSendMsg(JSON.stringify({
            messageType: "unregister",
            status: 200,
            channelID,
          }));
          unregisterDone();
        },
      });
    },
  });

  let subModifiedPromise = promiseObserverNotification(
    PushServiceComponent.subscriptionModifiedTopic);

  await PushService.unregister({
    scope: "https://example.com/page/unregister-success",
    originAttributes: "",
  });

  let {data: subModifiedScope} = await subModifiedPromise;
  equal(subModifiedScope, "https://example.com/page/unregister-success",
    "Should fire a subscription modified event after unsubscribing");

  let record = await db.getByKeyID(channelID);
  ok(!record, "Unregister did not remove record");

  await unregisterPromise;
});
