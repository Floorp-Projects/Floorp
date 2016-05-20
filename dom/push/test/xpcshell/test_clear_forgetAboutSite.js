'use strict';

const {PushService, PushServiceWebSocket} = serviceExports;
const {ForgetAboutSite} = Cu.import(
  'resource://gre/modules/ForgetAboutSite.jsm', {});

var db;
var unregisterDefers = {};
var userAgentID = '4fe01c2d-72ac-4c13-93d2-bb072caf461d';

function promiseUnregister(keyID) {
  return new Promise(r => unregisterDefers[keyID] = r);
}

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: userAgentID,
  });
  run_next_test();
}

add_task(function* setup() {
  db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(_ => db.drop().then(_ => db.close()));

  // Active and expired subscriptions for a subdomain. The active subscription
  // should be expired, then removed; the expired subscription should be
  // removed immediately.
  yield putTestRecord(db, 'active-sub', 'https://sub.example.com/sub-page', 4);
  yield putTestRecord(db, 'expired-sub', 'https://sub.example.com/yet-another-page', 0);

  // Active subscriptions for another subdomain. Should be unsubscribed and
  // dropped.
  yield putTestRecord(db, 'active-1', 'https://sub2.example.com/some-page', 8);
  yield putTestRecord(db, 'active-2', 'https://sub3.example.com/another-page', 16);

  // A privileged subscription with a real URL that should not be affected
  // because its quota is set to `Infinity`.
  yield putTestRecord(db, 'privileged', 'https://sub.example.com/real-url', Infinity);

  let handshakeDone;
  let handshakePromise = new Promise(r => handshakeDone = r);
  PushService.init({
    serverURI: 'wss://push.example.org/',
    db: db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            uaid: userAgentID,
            status: 200,
            use_webpush: true,
          }));
          handshakeDone();
        },
        onUnregister(request) {
          let resolve = unregisterDefers[request.channelID];
          equal(typeof resolve, 'function',
            'Dropped unexpected channel ID ' + request.channelID);
          delete unregisterDefers[request.channelID];
          equal(request.code, 200,
            'Expected manual unregister reason');
          resolve();
        },
      });
    },
  });
  // For cleared subscriptions, we only send unregister requests in the
  // background and if we're connected.
  yield handshakePromise;
});

add_task(function* test_forgetAboutSubdomain() {
  let modifiedScopes = [];
  let promiseForgetSubs = Promise.all([
    // Active subscriptions should be dropped.
    promiseUnregister('active-sub'),
    promiseObserverNotification(
      PushServiceComponent.subscriptionModifiedTopic, (subject, data) => {
        modifiedScopes.push(data);
        return modifiedScopes.length == 1;
      }
    ),
  ]);
  yield ForgetAboutSite.removeDataFromDomain('sub.example.com');
  yield promiseForgetSubs;

  deepEqual(modifiedScopes.sort(compareAscending), [
    'https://sub.example.com/sub-page',
  ], 'Should fire modified notifications for active subscriptions');

  let remainingIDs = yield getAllKeyIDs(db);
  deepEqual(remainingIDs, ['active-1', 'active-2', 'privileged'],
    'Should only forget subscriptions for subdomain');
});

add_task(function* test_forgetAboutRootDomain() {
  let modifiedScopes = [];
  let promiseForgetSubs = Promise.all([
    promiseUnregister('active-1'),
    promiseUnregister('active-2'),
    promiseObserverNotification(
      PushServiceComponent.subscriptionModifiedTopic, (subject, data) => {
        modifiedScopes.push(data);
        return modifiedScopes.length == 2;
      }
    ),
  ]);

  yield ForgetAboutSite.removeDataFromDomain('example.com');
  yield promiseForgetSubs;

  deepEqual(modifiedScopes.sort(compareAscending), [
    'https://sub2.example.com/some-page',
    'https://sub3.example.com/another-page',
  ], 'Should fire modified notifications for entire domain');

  let remainingIDs = yield getAllKeyIDs(db);
  deepEqual(remainingIDs, ['privileged'],
    'Should ignore privileged records with a real URL');
});
