/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

var db;
var unregisterDefers = {};
var userAgentID = '4ce480ef-55b2-4f83-924c-dcd35ab978b4';

function promiseUnregister(keyID, code) {
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

  // Active subscriptions; should be expired then dropped.
  yield putTestRecord(db, 'active-1', 'https://example.info/some-page', 8);
  yield putTestRecord(db, 'active-2', 'https://example.com/another-page', 16);

  // Expired subscription; should be dropped.
  yield putTestRecord(db, 'expired', 'https://example.net/yet-another-page', 0);

  // A privileged subscription that should not be affected by sanitizing data
  // because its quota is set to `Infinity`.
  yield putTestRecord(db, 'privileged', 'app://chrome/only', Infinity);

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
  yield handshakePromise;
});

add_task(function* test_sanitize() {
  let modifiedScopes = [];
  let changeScopes = [];

  let promiseCleared = Promise.all([
    // Active subscriptions should be unregistered.
    promiseUnregister('active-1'),
    promiseUnregister('active-2'),
    promiseObserverNotification(
      PushServiceComponent.subscriptionModifiedTopic, (subject, data) => {
        modifiedScopes.push(data);
        return modifiedScopes.length == 3;
    }),

    // Privileged should be recreated.
    promiseUnregister('privileged'),
    promiseObserverNotification(
      PushServiceComponent.subscriptionChangeTopic, (subject, data) => {
        changeScopes.push(data);
        return changeScopes.length == 1;
    }),
  ]);

  yield PushService.clear({
    domain: '*',
  });

  yield promiseCleared;

  deepEqual(modifiedScopes.sort(compareAscending), [
    'app://chrome/only',
    'https://example.com/another-page',
    'https://example.info/some-page',
  ], 'Should modify active subscription scopes');

  deepEqual(changeScopes, ['app://chrome/only'],
    'Should fire change notification for privileged scope');

  let remainingIDs = yield getAllKeyIDs(db);
  deepEqual(remainingIDs, [], 'Should drop all subscriptions');
});
