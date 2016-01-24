/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '1ca1cf66-eeb4-4df7-87c1-d5c92906ab90';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: userAgentID,
  });
  run_next_test();
}

add_task(function* test_notification_incomplete() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  let records = [{
    channelID: '123',
    pushEndpoint: 'https://example.org/update/1',
    scope: 'https://example.com/page/1',
    version: 1,
    originAttributes: '',
    quota: Infinity,
  }, {
    channelID: '3ad1ed95-d37a-4d88-950f-22cbe2e240d7',
    pushEndpoint: 'https://example.org/update/2',
    scope: 'https://example.com/page/2',
    version: 1,
    originAttributes: '',
    quota: Infinity,
  }, {
    channelID: 'd239498b-1c85-4486-b99b-205866e82d1f',
    pushEndpoint: 'https://example.org/update/3',
    scope: 'https://example.com/page/3',
    version: 3,
    originAttributes: '',
    quota: Infinity,
  }, {
    channelID: 'a50de97d-b496-43ce-8b53-05522feb78db',
    pushEndpoint: 'https://example.org/update/4',
    scope: 'https://example.com/page/4',
    version: 10,
    originAttributes: '',
    quota: Infinity,
  }];
  for (let record of records) {
    yield db.put(record);
  }

  function observeMessage(subject, topic, data) {
    ok(false, 'Should not deliver malformed updates');
  }
  do_register_cleanup(() =>
    Services.obs.removeObserver(observeMessage, 'push-message'));
  Services.obs.addObserver(observeMessage, 'push-message', false);

  let notificationDone;
  let notificationPromise = new Promise(resolve => notificationDone = after(2, resolve));
  let prevHandler = PushServiceWebSocket._handleNotificationReply;
  PushServiceWebSocket._handleNotificationReply = function _handleNotificationReply() {
    notificationDone();
    return prevHandler.apply(this, arguments);
  };
  PushService.init({
    serverURI: "wss://push.example.org/",
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
          this.serverSendMsg(JSON.stringify({
            // Missing "updates" field; should ignore message.
            messageType: 'notification'
          }));
          this.serverSendMsg(JSON.stringify({
            messageType: 'notification',
            updates: [{
              // Wrong channel ID field type.
              channelID: 123,
              version: 3
            }, {
              // Missing version field.
              channelID: '3ad1ed95-d37a-4d88-950f-22cbe2e240d7'
            }, {
              // Wrong version field type.
              channelID: 'd239498b-1c85-4486-b99b-205866e82d1f',
              version: true
            }, {
              // Negative versions should be ignored.
              channelID: 'a50de97d-b496-43ce-8b53-05522feb78db',
              version: -5
            }]
          }));
        },
        onACK() {
          ok(false, 'Should not acknowledge malformed updates');
        }
      });
    }
  });

  yield waitForPromise(notificationPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for incomplete notifications');

  let storeRecords = yield db.getAllKeyIDs();
  storeRecords.sort(({pushEndpoint: a}, {pushEndpoint: b}) =>
    compareAscending(a, b));
  recordsAreEqual(records, storeRecords);
});

function recordIsEqual(a, b) {
  strictEqual(a.channelID, b.channelID, 'Wrong channel ID in record');
  strictEqual(a.pushEndpoint, b.pushEndpoint, 'Wrong push endpoint in record');
  strictEqual(a.scope, b.scope, 'Wrong scope in record');
  strictEqual(a.version, b.version, 'Wrong version in record');
}

function recordsAreEqual(a, b) {
  equal(a.length, b.length, 'Mismatched record count');
  for (let i = 0; i < a.length; i++) {
    recordIsEqual(a[i], b[i]);
  }
}
