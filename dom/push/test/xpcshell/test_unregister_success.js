/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const channelID = 'db0a7021-ec2d-4bd3-8802-7a6966f10ed8';

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(function* test_unregister_success() {
  let db = new PushDB();
  let promiseDB = promisifyDatabase(db);
  do_register_cleanup(() => cleanupDatabase(db));
  yield promiseDB.put({
    channelID,
    pushEndpoint: 'https://example.org/update/unregister-success',
    scope: 'https://example.com/page/unregister-success',
    version: 1
  });

  let unregisterDefer = Promise.defer();
  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: 'fbe865a6-aeb8-446f-873c-aeebdb8d493c'
          }));
        },
        onUnregister(request) {
          equal(request.channelID, channelID, 'Should include the channel ID');
          this.serverSendMsg(JSON.stringify({
            messageType: 'unregister',
            status: 200,
            channelID
          }));
          unregisterDefer.resolve();
        }
      });
    }
  });

  yield PushNotificationService.unregister(
    'https://example.com/page/unregister-success');
  let record = yield promiseDB.getByChannelID(channelID);
  ok(!record, 'Unregister did not remove record');

  yield waitForPromise(unregisterDefer.promise, DEFAULT_TIMEOUT,
    'Timed out waiting for unregister');
});
