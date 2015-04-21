/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs();
  disableServiceWorkerEvents(
    'https://example.com/1',
    'https://example.com/2'
  );
  run_next_test();
}

// Should acknowledge duplicate notifications, but not notify apps.
add_task(function* test_notification_duplicate() {
  let db = new PushDB();
  let promiseDB = promisifyDatabase(db);
  do_register_cleanup(() => cleanupDatabase(db));
  let records = [{
    channelID: '8d2d9400-3597-4c5a-8a38-c546b0043bcc',
    pushEndpoint: 'https://example.org/update/1',
    scope: 'https://example.com/1',
    version: 2
  }, {
    channelID: '27d1e393-03ef-4c72-a5e6-9e890dfccad0',
    pushEndpoint: 'https://example.org/update/2',
    scope: 'https://example.com/2',
    version: 2
  }];
  for (let record of records) {
    yield promiseDB.put(record);
  }

  let notifyPromise = promiseObserverNotification('push-notification');

  let acks = 0;
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
            uaid: '1500e7d9-8cbe-4ee6-98da-7fa5d6a39852'
          }));
          this.serverSendMsg(JSON.stringify({
            messageType: 'notification',
            updates: [{
              channelID: '8d2d9400-3597-4c5a-8a38-c546b0043bcc',
              version: 2
            }, {
              channelID: '27d1e393-03ef-4c72-a5e6-9e890dfccad0',
              version: 3
            }]
          }));
        },
        onACK: ackDone
      });
    }
  });

  yield waitForPromise(notifyPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for notifications');
  yield waitForPromise(ackDefer.promise, DEFAULT_TIMEOUT,
    'Timed out waiting for stale acknowledgement');

  let staleRecord = yield promiseDB.getByChannelID(
    '8d2d9400-3597-4c5a-8a38-c546b0043bcc');
  strictEqual(staleRecord.version, 2, 'Wrong stale record version');

  let updatedRecord = yield promiseDB.getByChannelID(
    '27d1e393-03ef-4c72-a5e6-9e890dfccad0');
  strictEqual(updatedRecord.version, 3, 'Wrong updated record version');
});
