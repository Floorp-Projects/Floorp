/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PushDB, PushService, PushServiceWebSocket } = serviceExports;

const userAgentID = "aaabf1f8-2f68-44f1-a920-b88e9e7d7559";
const nsIPushQuotaManager = Ci.nsIPushQuotaManager;

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    "testing.ignorePermission": true,
  });
  run_next_test();
}

add_task(async function test_expiration_origin_threshold() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {
    PushService.notificationForOriginClosed("https://example.com");
    return db.drop().then(_ => db.close());
  });

  // Simulate a notification being shown for the origin,
  // this should relax the quota and allow as many push messages
  // as we want.
  PushService.notificationForOriginShown("https://example.com");

  await db.put({
    channelID: "f56645a9-1f32-4655-92ad-ddc37f6d54fb",
    pushEndpoint: "https://example.org/push/1",
    scope: "https://example.com/quota",
    pushCount: 0,
    lastPush: 0,
    version: null,
    originAttributes: "",
    quota: 16,
  });

  // A visit one day ago should provide a quota of 8 messages.
  await PlacesTestUtils.addVisits({
    uri: "https://example.com/login",
    title: "Sign in to see your auctions",
    visitDate: (Date.now() - MS_IN_ONE_DAY) * 1000,
    transition: Ci.nsINavHistoryService.TRANSITION_LINK,
  });

  let numMessages = 10;

  let updates = 0;
  let notifyPromise = promiseObserverNotification(
    PushServiceComponent.pushTopic,
    (subject, data) => {
      updates++;
      return updates == numMessages;
    }
  );

  let modifications = 0;
  let modifiedPromise = promiseObserverNotification(
    PushServiceComponent.subscriptionModifiedTopic,
    (subject, data) => {
      // Each subscription should be modified twice: once to update the message
      // count and last push time, and the second time to update the quota.
      modifications++;
      return modifications == numMessages * 2;
    }
  );

  let updateQuotaPromise = new Promise((resolve, reject) => {
    let quotaUpdateCount = 0;
    PushService._updateQuotaTestCallback = function() {
      quotaUpdateCount++;
      if (quotaUpdateCount == numMessages) {
        resolve();
      }
    };
  });

  PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(
            JSON.stringify({
              messageType: "hello",
              status: 200,
              uaid: userAgentID,
            })
          );

          // If the origin has visible notifications, the
          // message should not affect quota.
          for (let version = 1; version <= 10; version++) {
            this.serverSendMsg(
              JSON.stringify({
                messageType: "notification",
                updates: [
                  {
                    channelID: "f56645a9-1f32-4655-92ad-ddc37f6d54fb",
                    version,
                  },
                ],
              })
            );
          }
        },
        onUnregister(request) {
          ok(false, "Channel should not be unregistered.");
        },
        // We expect to receive acks, but don't care about their
        // contents.
        onACK(request) {},
      });
    },
  });

  await notifyPromise;

  await updateQuotaPromise;
  await modifiedPromise;

  let expiredRecord = await db.getByKeyID(
    "f56645a9-1f32-4655-92ad-ddc37f6d54fb"
  );
  notStrictEqual(expiredRecord.quota, 0, "Expired record not updated");
});
