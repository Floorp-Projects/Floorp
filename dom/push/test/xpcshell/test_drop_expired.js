/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '2c43af06-ab6e-476a-adc4-16cbda54fb89';

var db;
var quotaURI;
var permURI;

function visitURI(uri, timestamp) {
  return PlacesTestUtils.addVisits({
    uri: uri,
    title: uri.spec,
    visitDate: timestamp * 1000,
    transition: Ci.nsINavHistoryService.TRANSITION_LINK
  });
}

var putRecord = Task.async(function* ({scope, perm, quota, lastPush, lastVisit}) {
  let uri = Services.io.newURI(scope, null, null);

  Services.perms.add(uri, 'desktop-notification',
    Ci.nsIPermissionManager[perm]);
  do_register_cleanup(() => {
    Services.perms.remove(uri, 'desktop-notification');
  });

  yield visitURI(uri, lastVisit);

  yield db.put({
    channelID: uri.path,
    pushEndpoint: 'https://example.org/push' + uri.path,
    scope: uri.spec,
    pushCount: 0,
    lastPush: lastPush,
    version: null,
    originAttributes: '',
    quota: quota,
  });

  return uri;
});

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: userAgentID,
  });

  db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  run_next_test();
}

add_task(function* setUp() {
  // An expired registration that should be evicted on startup. Permission is
  // granted for this origin, and the last visit is more recent than the last
  // push message.
  yield putRecord({
    scope: 'https://example.com/expired-quota-restored',
    perm: 'ALLOW_ACTION',
    quota: 0,
    lastPush: Date.now() - 10,
    lastVisit: Date.now(),
  });

  // An expired registration that we should evict when the origin is visited
  // again.
  quotaURI = yield putRecord({
    scope: 'https://example.xyz/expired-quota-exceeded',
    perm: 'ALLOW_ACTION',
    quota: 0,
    lastPush: Date.now() - 10,
    lastVisit: Date.now() - 20,
  });

  // An expired registration that we should evict when permission is granted
  // again.
  permURI = yield putRecord({
    scope: 'https://example.info/expired-perm-revoked',
    perm: 'DENY_ACTION',
    quota: 0,
    lastPush: Date.now() - 10,
    lastVisit: Date.now(),
  });

  // An active registration that we should leave alone.
  yield putRecord({
    scope: 'https://example.ninja/active',
    perm: 'ALLOW_ACTION',
    quota: 16,
    lastPush: Date.now() - 10,
    lastVisit: Date.now() - 20,
  });

  let subChangePromise = promiseObserverNotification(
    PushServiceComponent.subscriptionChangeTopic,
    (subject, data) => data == 'https://example.com/expired-quota-restored'
  );

  PushService.init({
    serverURI: 'wss://push.example.org/',
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID,
          }));
        },
        onUnregister(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'unregister',
            channelID: request.channelID,
            status: 200,
          }));
        },
      });
    },
  });

  yield subChangePromise;
});

add_task(function* test_site_visited() {
  let subChangePromise = promiseObserverNotification(
    PushServiceComponent.subscriptionChangeTopic,
    (subject, data) => data == 'https://example.xyz/expired-quota-exceeded'
  );

  yield visitURI(quotaURI, Date.now());
  PushService.observe(null, 'idle-daily', '');

  yield subChangePromise;
});

add_task(function* test_perm_restored() {
  let subChangePromise = promiseObserverNotification(
    PushServiceComponent.subscriptionChangeTopic,
    (subject, data) => data == 'https://example.info/expired-perm-revoked'
  );

  Services.perms.add(permURI, 'desktop-notification',
    Ci.nsIPermissionManager.ALLOW_ACTION);

  yield subChangePromise;
});
