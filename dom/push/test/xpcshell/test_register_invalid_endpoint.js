/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = 'c9a12e81-ea5e-40f9-8bf4-acee34621671';
const channelID = 'c0660af8-b532-4931-81f0-9fd27a12d6ab';

function run_test() {
  do_get_profile();
  setPrefs();
  disableServiceWorkerEvents(
    'https://example.net/page/invalid-endpoint'
  );
  run_next_test();
}

add_task(function* test_register_invalid_endpoint() {
  let db = new PushDB();
  let promiseDB = promisifyDatabase(db);
  do_register_cleanup(() => cleanupDatabase(db));

  PushService._generateID = () => channelID;
  PushService.init({
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
        },
        onRegister(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200,
            channelID,
            uaid: userAgentID,
            pushEndpoint: '!@#$%^&*'
          }));
        }
      });
    }
  });

  yield rejects(
    PushNotificationService.register(
      'https://example.net/page/invalid-endpoint'),
    function(error) {
      return error && error.contains('Invalid pushEndpoint');
    },
    'Wrong error for invalid endpoint'
  );

  let record = yield promiseDB.getByChannelID(channelID);
  ok(!record, 'Should not store records with invalid endpoints');
});
