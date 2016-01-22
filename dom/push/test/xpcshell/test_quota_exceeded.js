/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

Cu.import("resource://gre/modules/Task.jsm");

const userAgentID = '7eb873f9-8d47-4218-804b-fff78dc04e88';

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
  do_register_cleanup(() => db.drop().then(_ => db.close()));

  yield db.put({
    channelID: 'eb33fc90-c883-4267-b5cb-613969e8e349',
    pushEndpoint: 'https://example.org/push/1',
    scope: 'https://example.com/auctions',
    pushCount: 0,
    lastPush: 0,
    version: null,
    originAttributes: '',
    quota: 16,
  });
  yield db.put({
    channelID: '46cc6f6a-c106-4ffa-bb7c-55c60bd50c41',
    pushEndpoint: 'https://example.org/push/2',
    scope: 'https://example.com/deals',
    pushCount: 0,
    lastPush: 0,
    version: null,
    originAttributes: '',
    quota: 16,
  });

  // The notification threshold is per-origin, even with multiple service
  // workers for different scopes.
  yield addVisit({
    uri: 'https://example.com/login',
    title: 'Sign in to see your auctions',
    visits: [{
      visitDate: (Date.now() - 7 * 24 * 60 * 60 * 1000) * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  });

  // We'll always use your most recent visit to an origin.
  yield addVisit({
    uri: 'https://example.com/auctions',
    title: 'Your auctions',
    visits: [{
      visitDate: (Date.now() - 2 * 24 * 60 * 60 * 1000) * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_LINK,
    }],
  });

  // ...But we won't count downloads or embeds.
  yield addVisit({
    uri: 'https://example.com/invoices/invoice.pdf',
    title: 'Invoice #123',
    visits: [{
      visitDate: (Date.now() - 1 * 24 * 60 * 60 * 1000) * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_EMBED,
    }, {
      visitDate: Date.now() * 1000,
      transitionType: Ci.nsINavHistoryService.TRANSITION_DOWNLOAD,
    }],
  });

  // We expect to receive 6 notifications: 5 on the `auctions` channel,
  // and 1 on the `deals` channel. They're from the same origin, but
  // different scopes, so each can send 5 notifications before we remove
  // their subscription.
  let updates = 0;
  let notifyPromise = promiseObserverNotification('push-notification', (subject, data) => {
    updates++;
    return updates == 6;
  });

  let unregisterDone;
  let unregisterPromise = new Promise(resolve => unregisterDone = resolve);

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
          // We last visited the site 2 days ago, so we can send 5
          // notifications without throttling. Sending a 6th should
          // drop the registration.
          for (let version = 1; version <= 6; version++) {
            this.serverSendMsg(JSON.stringify({
              messageType: 'notification',
              updates: [{
                channelID: 'eb33fc90-c883-4267-b5cb-613969e8e349',
                version,
              }],
            }));
          }
          // But the limits are per-channel, so we can send 5 more
          // notifications on a different channel.
          this.serverSendMsg(JSON.stringify({
            messageType: 'notification',
            updates: [{
              channelID: '46cc6f6a-c106-4ffa-bb7c-55c60bd50c41',
              version: 1,
            }],
          }));
        },
        onUnregister(request) {
          equal(request.channelID, 'eb33fc90-c883-4267-b5cb-613969e8e349', 'Unregistered wrong channel ID');
          unregisterDone();
        },
        // We expect to receive acks, but don't care about their
        // contents.
        onACK(request) {},
      });
    },
  });

  yield waitForPromise(unregisterPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for unregister request');

  yield waitForPromise(notifyPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for notifications');

  let expiredRecord = yield db.getByKeyID('eb33fc90-c883-4267-b5cb-613969e8e349');
  strictEqual(expiredRecord.quota, 0, 'Expired record not updated');
});
