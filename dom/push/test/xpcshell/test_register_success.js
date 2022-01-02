/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PushDB, PushService, PushServiceWebSocket } = serviceExports;

const userAgentID = "bd744428-f125-436a-b6d0-dd0c9845837f";
const channelID = "0ef2ad4a-6c49-41ad-af6e-95d2425276bf";

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 1000,
    retryBaseInterval: 150,
  });
  run_next_test();
}

add_task(async function test_register_success() {
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
        onHello(data) {
          equal(data.messageType, "hello", "Handshake: wrong message type");
          ok(
            !data.uaid,
            "Should not send UAID in handshake without local subscriptions"
          );
          this.serverSendMsg(
            JSON.stringify({
              messageType: "hello",
              status: 200,
              uaid: userAgentID,
            })
          );
        },
        onRegister(data) {
          equal(data.messageType, "register", "Register: wrong message type");
          equal(data.channelID, channelID, "Register: wrong channel ID");
          this.serverSendMsg(
            JSON.stringify({
              messageType: "register",
              status: 200,
              channelID,
              uaid: userAgentID,
              pushEndpoint: "https://example.com/update/1",
            })
          );
        },
      });
    },
  });

  let subModifiedPromise = promiseObserverNotification(
    PushServiceComponent.subscriptionModifiedTopic
  );

  let newRecord = await PushService.register({
    scope: "https://example.org/1",
    originAttributes: ChromeUtils.originAttributesToSuffix({
      inIsolatedMozBrowser: false,
    }),
  });
  equal(
    newRecord.endpoint,
    "https://example.com/update/1",
    "Wrong push endpoint in registration record"
  );

  let { data: subModifiedScope } = await subModifiedPromise;
  equal(
    subModifiedScope,
    "https://example.org/1",
    "Should fire a subscription modified event after subscribing"
  );

  let record = await db.getByKeyID(channelID);
  equal(record.channelID, channelID, "Wrong channel ID in database record");
  equal(
    record.pushEndpoint,
    "https://example.com/update/1",
    "Wrong push endpoint in database record"
  );
  equal(record.quota, 16, "Wrong quota in database record");
});
