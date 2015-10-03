/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = 'ba31ac13-88d4-4984-8e6b-8731315a7cf8';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: userAgentID,
  });
  disableServiceWorkerEvents(
    'https://example.net/case'
  );
  run_next_test();
}

add_task(function* test_notification_version_string() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  yield db.put({
    channelID: '6ff97d56-d0c0-43bc-8f5b-61b855e1d93b',
    pushEndpoint: 'https://example.org/updates/1',
    scope: 'https://example.com/page/1',
    originAttributes: '',
    version: 2,
    quota: Infinity,
  });

  let notifyPromise = promiseObserverNotification('push-notification');

  let ackDone;
  let ackPromise = new Promise(resolve => ackDone = resolve);
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
            messageType: 'notification',
            updates: [{
              channelID: '6ff97d56-d0c0-43bc-8f5b-61b855e1d93b',
              version: '4'
            }]
          }));
        },
        onACK: ackDone
      });
    }
  });

  let {subject: notification, data: scope} = yield waitForPromise(
    notifyPromise,
    DEFAULT_TIMEOUT,
    'Timed out waiting for string notification'
  );
  let message = notification.QueryInterface(Ci.nsIPushObserverNotification);
  equal(scope, 'https://example.com/page/1', 'Wrong scope');
  equal(message.pushEndpoint, 'https://example.org/updates/1',
    'Wrong push endpoint');
  strictEqual(message.version, 4, 'Wrong version');

  yield waitForPromise(ackPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for string acknowledgement');

  let storeRecord = yield db.getByKeyID(
    '6ff97d56-d0c0-43bc-8f5b-61b855e1d93b');
  strictEqual(storeRecord.version, 4, 'Wrong record version');
  equal(storeRecord.quota, Infinity, 'Wrong quota');
});
