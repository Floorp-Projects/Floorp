/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = 'bd744428-f125-436a-b6d0-dd0c9845837f';
const channelID = '0ef2ad4a-6c49-41ad-af6e-95d2425276bf';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  disableServiceWorkerEvents(
    'https://example.org/1'
  );
  run_next_test();
}

add_task(function* test_register_success() {
  let db = new PushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushService._generateID = () => channelID;
  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(data) {
          equal(data.messageType, 'hello', 'Handshake: wrong message type');
          equal(data.uaid, userAgentID, 'Handshake: wrong device ID');
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID
          }));
        },
        onRegister(data) {
          equal(data.messageType, 'register', 'Register: wrong message type');
          equal(data.channelID, channelID, 'Register: wrong channel ID');
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200,
            channelID: channelID,
            uaid: userAgentID,
            pushEndpoint: 'https://example.com/update/1',
          }));
        }
      });
    }
  });

  let newRecord = yield PushNotificationService.register(
    'https://example.org/1'
  );
  equal(newRecord.channelID, channelID,
    'Wrong channel ID in registration record');
  equal(newRecord.pushEndpoint, 'https://example.com/update/1',
    'Wrong push endpoint in registration record');
  equal(newRecord.scope, 'https://example.org/1',
    'Wrong scope in registration record');

  let record = yield db.getByChannelID(channelID);
  equal(record.channelID, channelID,
    'Wrong channel ID in database record');
  equal(record.pushEndpoint, 'https://example.com/update/1',
    'Wrong push endpoint in database record');
  equal(record.scope, 'https://example.org/1',
    'Wrong scope in database record');
});
