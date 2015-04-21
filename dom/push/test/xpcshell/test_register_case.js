/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = '1760b1f5-c3ba-40e3-9344-adef7c18ab12';

function run_test() {
  do_get_profile();
  setPrefs();
  disableServiceWorkerEvents(
    'https://example.net/case'
  );
  run_next_test();
}

add_task(function* test_register_case() {
  let db = new PushDB();
  let promiseDB = promisifyDatabase(db);
  do_register_cleanup(() => cleanupDatabase(db));

  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'HELLO',
            uaid: userAgentID,
            status: 200
          }));
        },
        onRegister(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'ReGiStEr',
            uaid: userAgentID,
            channelID: request.channelID,
            status: 200,
            pushEndpoint: 'https://example.com/update/case'
          }));
        }
      });
    }
  });

  let newRecord = yield waitForPromise(
    PushNotificationService.register('https://example.net/case'),
    DEFAULT_TIMEOUT,
    'Mixed-case register response timed out'
  );
  equal(newRecord.pushEndpoint, 'https://example.com/update/case',
    'Wrong push endpoint in registration record');
  equal(newRecord.scope, 'https://example.net/case',
    'Wrong scope in registration record');

  let record = yield promiseDB.getByChannelID(newRecord.channelID);
  equal(record.pushEndpoint, 'https://example.com/update/case',
    'Wrong push endpoint in database record');
  equal(record.scope, 'https://example.net/case',
    'Wrong scope in database record');
});
