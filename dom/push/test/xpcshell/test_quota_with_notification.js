/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

Cu.import("resource://gre/modules/Task.jsm");

const userAgentID = 'aaabf1f8-2f68-44f1-a920-b88e9e7d7559';
const nsIPushQuotaManager = Components.interfaces.nsIPushQuotaManager;

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    'testing.ignorePermission': true,
  });
  run_next_test();
}

add_task(function* test_expiration_origin_threshold() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {
    PushService.notificationForOriginClosed("https://example.com");
    return db.drop().then(_ => db.close());
  });

  // Simulate a notification being shown for the origin,
  // this should relax the quota and allow as many push messages
  // as we want.
  PushService.notificationForOriginShown("https://example.com");

  yield db.put({
    channelID: 'f56645a9-1f32-4655-92ad-ddc37f6d54fb',
    pushEndpoint: 'https://example.org/push/1',
    scope: 'https://example.com/quota',
    pushCount: 0,
    lastPush: 0,
    version: null,
    originAttributes: '',
    quota: 16,
  });

  // A visit one day ago should provide a quota of 8 messages.
  yield addVisit({
    uri: 'https://example.com/login',
    title: 'Sign in to see your auctions',
    visits: [{
      visitDate: (Date.now() - 1 * 24 * 60 * 60 * 1000) * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  });

  let numMessages = 10;

  let updates = 0;
  let notifyPromise = promiseObserverNotification('push-notification', (subject, data) => {
    updates++;
    return updates == numMessages;
  });

  let updateQuotaPromise = new Promise((resolve, reject) => {
    let quotaUpdateCount = 0;
    PushService._updateQuotaTestCallback = function() {
      quotaUpdateCount++;
      if (quotaUpdateCount == 10) {
        resolve();
      }
    };
  });

  PushService.init({
    serverURI: 'wss://push.example.org/',
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID,
          }));

          // If the origin has visible notifications, the
          // message should not affect quota.
          for (let version = 1; version <= 10; version++) {
            this.serverSendMsg(JSON.stringify({
              messageType: 'notification',
              updates: [{
                channelID: 'f56645a9-1f32-4655-92ad-ddc37f6d54fb',
                version,
              }],
            }));
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

  yield waitForPromise(notifyPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for notifications');

  yield waitForPromise(updateQuotaPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for quota to be updated');

  let expiredRecord = yield db.getByKeyID('f56645a9-1f32-4655-92ad-ddc37f6d54fb');
  notStrictEqual(expiredRecord.quota, 0, 'Expired record not updated');
});
