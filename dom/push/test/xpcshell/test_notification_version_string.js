/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = "ba31ac13-88d4-4984-8e6b-8731315a7cf8";

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
  });
  run_next_test();
}

add_task(async function test_notification_version_string() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => { return db.drop().then(_ => db.close()); });
  await db.put({
    channelID: "6ff97d56-d0c0-43bc-8f5b-61b855e1d93b",
    pushEndpoint: "https://example.org/updates/1",
    scope: "https://example.com/page/1",
    originAttributes: "",
    version: 2,
    quota: Infinity,
    systemRecord: true,
  });

  let notifyPromise = promiseObserverNotification(PushServiceComponent.pushTopic);

  let ackDone;
  let ackPromise = new Promise(resolve => ackDone = resolve);
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
          }));
          this.serverSendMsg(JSON.stringify({
            messageType: "notification",
            updates: [{
              channelID: "6ff97d56-d0c0-43bc-8f5b-61b855e1d93b",
              version: "4",
            }],
          }));
        },
        onACK: ackDone,
      });
    },
  });

  let {subject: message} = await notifyPromise;
  equal(message.QueryInterface(Ci.nsIPushMessage).data, null,
    "Unexpected data for Simple Push message");

  await ackPromise;

  let storeRecord = await db.getByKeyID(
    "6ff97d56-d0c0-43bc-8f5b-61b855e1d93b");
  strictEqual(storeRecord.version, 4, "Wrong record version");
  equal(storeRecord.quota, Infinity, "Wrong quota");
});
