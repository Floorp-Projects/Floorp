/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '2c43af06-ab6e-476a-adc4-16cbda54fb89';

let db;

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
  });

  db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {return db.drop().then(_ => db.close());});

  run_next_test();
}

let unregisterDefers = {};

function promiseUnregister(keyID) {
  return new Promise(r => unregisterDefers[keyID] = r);
}

function makePushPermission(url, capability) {
  return {
    QueryInterface: ChromeUtils.generateQI([Ci.nsIPermission]),
    capability: Ci.nsIPermissionManager[capability],
    expireTime: 0,
    expireType: Ci.nsIPermissionManager.EXPIRE_NEVER,
    principal: Services.scriptSecurityManager.createCodebasePrincipal(
      Services.io.newURI(url), {}
    ),
    type: 'desktop-notification',
  };
}

function promiseObserverNotifications(topic, count) {
  let notifiedScopes = [];
  let subChangePromise = promiseObserverNotification(topic, (subject, data) => {
    notifiedScopes.push(data);
    return notifiedScopes.length == count;
  });
  return subChangePromise.then(_ => notifiedScopes.sort());
}

function promiseSubscriptionChanges(count) {
  return promiseObserverNotifications(
    PushServiceComponent.subscriptionChangeTopic, count);
}

function promiseSubscriptionModifications(count) {
  return promiseObserverNotifications(
    PushServiceComponent.subscriptionModifiedTopic, count);
}

function allExpired(...keyIDs) {
  return Promise.all(keyIDs.map(
    keyID => db.getByKeyID(keyID)
  )).then(records =>
    records.every(record => record.isExpired())
  );
}

add_task(async function setUp() {
  // Active registration; quota should be reset to 16. Since the quota isn't
  // exposed to content, we shouldn't receive a subscription change event.
  await putTestRecord(db, 'active-allow', 'https://example.info/page/1', 8);

  // Expired registration; should be dropped.
  await putTestRecord(db, 'expired-allow', 'https://example.info/page/2', 0);

  // Active registration; should be expired when we change the permission
  // to "deny".
  await putTestRecord(db, 'active-deny-changed', 'https://example.xyz/page/1', 16);

  // Two active registrations for a visited site. These will expire when we
  // add a "deny" permission.
  await putTestRecord(db, 'active-deny-added-1', 'https://example.net/ham', 16);
  await putTestRecord(db, 'active-deny-added-2', 'https://example.net/green', 8);

  // An already-expired registration for a visited site. We shouldn't send an
  // `unregister` request for this one, but still receive an observer
  // notification when we restore permissions.
  await putTestRecord(db, 'expired-deny-added', 'https://example.net/eggs', 0);

  // A registration that should not be affected by permission list changes
  // because its quota is set to `Infinity`.
  await putTestRecord(db, 'never-expires', 'app://chrome/only', Infinity);

  // A registration that should be dropped when we clear the permission
  // list.
  await putTestRecord(db, 'drop-on-clear', 'https://example.edu/lonely', 16);

  let handshakeDone;
  let handshakePromise = new Promise(resolve => handshakeDone = resolve);
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
          handshakeDone();
        },
        onUnregister(request) {
          let resolve = unregisterDefers[request.channelID];
          equal(typeof resolve, 'function',
            'Dropped unexpected channel ID ' + request.channelID);
          delete unregisterDefers[request.channelID];
          equal(request.code, 202,
            'Expected permission revoked unregister reason');
          resolve();
          this.serverSendMsg(JSON.stringify({
            messageType: 'unregister',
            status: 200,
            channelID: request.channelID,
          }));
        },
        onACK(request) {},
      });
    }
  });
  await handshakePromise;
});

add_task(async function test_permissions_allow_added() {
  let subChangePromise = promiseSubscriptionChanges(1);

  await PushService._onPermissionChange(
    makePushPermission('https://example.info', 'ALLOW_ACTION'),
    'added'
  );
  let notifiedScopes = await subChangePromise;

  deepEqual(notifiedScopes, [
    'https://example.info/page/2',
  ], 'Wrong scopes after adding allow');

  let record = await db.getByKeyID('active-allow');
  equal(record.quota, 16,
    'Should reset quota for active records after adding allow');

  record = await db.getByKeyID('expired-allow');
  ok(!record, 'Should drop expired records after adding allow');
});

add_task(async function test_permissions_allow_deleted() {
  let subModifiedPromise = promiseSubscriptionModifications(1);

  let unregisterPromise = promiseUnregister('active-allow');

  await PushService._onPermissionChange(
    makePushPermission('https://example.info', 'ALLOW_ACTION'),
    'deleted'
  );

  await unregisterPromise;

  let notifiedScopes = await subModifiedPromise;
  deepEqual(notifiedScopes, [
    'https://example.info/page/1',
  ], 'Wrong scopes modified after deleting allow');

  let record = await db.getByKeyID('active-allow');
  ok(record.isExpired(),
    'Should expire active record after deleting allow');
});

add_task(async function test_permissions_deny_added() {
  let subModifiedPromise = promiseSubscriptionModifications(2);

  let unregisterPromise = Promise.all([
    promiseUnregister('active-deny-added-1'),
    promiseUnregister('active-deny-added-2'),
  ]);

  await PushService._onPermissionChange(
    makePushPermission('https://example.net', 'DENY_ACTION'),
    'added'
  );
  await unregisterPromise;

  let notifiedScopes = await subModifiedPromise;
  deepEqual(notifiedScopes, [
    'https://example.net/green',
    'https://example.net/ham',
  ], 'Wrong scopes modified after adding deny');

  let isExpired = await allExpired(
    'active-deny-added-1',
    'expired-deny-added'
  );
  ok(isExpired, 'Should expire all registrations after adding deny');
});

add_task(async function test_permissions_deny_deleted() {
  await PushService._onPermissionChange(
    makePushPermission('https://example.net', 'DENY_ACTION'),
    'deleted'
  );

  let isExpired = await allExpired(
    'active-deny-added-1',
    'expired-deny-added'
  );
  ok(isExpired, 'Should retain expired registrations after deleting deny');
});

add_task(async function test_permissions_allow_changed() {
  let subChangePromise = promiseSubscriptionChanges(3);

  await PushService._onPermissionChange(
    makePushPermission('https://example.net', 'ALLOW_ACTION'),
    'changed'
  );

  let notifiedScopes = await subChangePromise;

  deepEqual(notifiedScopes, [
    'https://example.net/eggs',
    'https://example.net/green',
    'https://example.net/ham'
  ], 'Wrong scopes after changing to allow');

  let droppedRecords = await Promise.all([
    db.getByKeyID('active-deny-added-1'),
    db.getByKeyID('active-deny-added-2'),
    db.getByKeyID('expired-deny-added'),
  ]);
  ok(!droppedRecords.some(Boolean),
    'Should drop all expired registrations after changing to allow');
});

add_task(async function test_permissions_deny_changed() {
  let subModifiedPromise = promiseSubscriptionModifications(1);

  let unregisterPromise = promiseUnregister('active-deny-changed');

  await PushService._onPermissionChange(
    makePushPermission('https://example.xyz', 'DENY_ACTION'),
    'changed'
  );

  await unregisterPromise;

  let notifiedScopes = await subModifiedPromise;
  deepEqual(notifiedScopes, [
    'https://example.xyz/page/1',
  ], 'Wrong scopes modified after changing to deny');

  let record = await db.getByKeyID('active-deny-changed');
  ok(record.isExpired(),
    'Should expire active record after changing to deny');
});

add_task(async function test_permissions_clear() {
  let subModifiedPromise = promiseSubscriptionModifications(3);

  deepEqual(await getAllKeyIDs(db), [
    'active-allow',
    'active-deny-changed',
    'drop-on-clear',
    'never-expires',
  ], 'Wrong records in database before clearing');

  let unregisterPromise = Promise.all([
    promiseUnregister('active-allow'),
    promiseUnregister('active-deny-changed'),
    promiseUnregister('drop-on-clear'),
  ]);

  await PushService._onPermissionChange(null, 'cleared');

  await unregisterPromise;

  let notifiedScopes = await subModifiedPromise;
  deepEqual(notifiedScopes, [
    'https://example.edu/lonely',
    'https://example.info/page/1',
    'https://example.xyz/page/1',
  ], 'Wrong scopes modified after clearing registrations');

  deepEqual(await getAllKeyIDs(db), [
    'never-expires',
  ], 'Unrestricted registrations should not be dropped');
});
