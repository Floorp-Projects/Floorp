/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = '52b2b04c-b6cc-42c6-abdf-bef9cbdbea00';
const channelID = 'cafed00d';

function run_test() {
  do_get_profile();
  setPrefs();
  disableServiceWorkerEvents(
    'https://example.com/invalid-channel'
  );
  run_next_test();
}

add_task(function* test_register_invalid_channel() {
  let db = new PushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushService._generateID = () => channelID;
  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            uaid: userAgentID,
            status: 200
          }));
        },
        onRegister(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 403,
            channelID,
            error: 'Invalid channel ID'
          }));
        }
      });
    }
  });

  yield rejects(
    PushNotificationService.register('https://example.com/invalid-channel'),
    function(error) {
      return error == 'Invalid channel ID';
    },
    'Wrong error for invalid channel ID'
  );

  let record = yield db.getByChannelID(channelID);
  ok(!record, 'Should not store records for error responses');
});
