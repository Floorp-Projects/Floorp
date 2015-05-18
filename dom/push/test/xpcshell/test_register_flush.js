/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = '9ce1e6d3-7bdb-4fe9-90a5-def1d64716f1';
const channelID = 'c26892c5-6e08-4c16-9f0c-0044697b4d85';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  disableServiceWorkerEvents(
    'https://example.com/page/1',
    'https://example.com/page/2'
  );
  run_next_test();
}

add_task(function* test_register_flush() {
  let db = new PushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  let record = {
    channelID: '9bcc7efb-86c7-4457-93ea-e24e6eb59b74',
    pushEndpoint: 'https://example.org/update/1',
    scope: 'https://example.com/page/1',
    version: 2
  };
  yield db.put(record);

  let notifyPromise = promiseObserverNotification('push-notification');

  let ackDefer = Promise.defer();
  let ackDone = after(2, ackDefer.resolve);
  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID
          }));
        },
        onRegister(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'notification',
            updates: [{
              channelID: request.channelID,
              version: 2
            }, {
              channelID: '9bcc7efb-86c7-4457-93ea-e24e6eb59b74',
              version: 3
            }]
          }));
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200,
            channelID: request.channelID,
            uaid: userAgentID,
            pushEndpoint: 'https://example.org/update/2'
          }));
        },
        onACK: ackDone
      });
    }
  });

  let newRecord = yield PushNotificationService.register(
    'https://example.com/page/2'
  );
  equal(newRecord.pushEndpoint, 'https://example.org/update/2',
    'Wrong push endpoint in record');
  equal(newRecord.scope, 'https://example.com/page/2',
    'Wrong scope in record');

  let {data: scope} = yield waitForPromise(notifyPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for notification');
  equal(scope, 'https://example.com/page/1', 'Wrong notification scope');

  yield waitForPromise(ackDefer.promise, DEFAULT_TIMEOUT,
     'Timed out waiting for acknowledgements');

  let prevRecord = yield db.getByChannelID(
    '9bcc7efb-86c7-4457-93ea-e24e6eb59b74');
  equal(prevRecord.pushEndpoint, 'https://example.org/update/1',
    'Wrong existing push endpoint');
  strictEqual(prevRecord.version, 3,
    'Should record version updates sent before register responses');

  let registeredRecord = yield db.getByChannelID(newRecord.channelID);
  equal(registeredRecord.pushEndpoint, 'https://example.org/update/2',
    'Wrong new push endpoint');
  ok(!registeredRecord.version, 'Should not record premature updates');
});
