/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = 'bd744428-f125-436a-b6d0-dd0c9845837f';
const channelIDs = ['0ef2ad4a-6c49-41ad-af6e-95d2425276bf', '4818b54a-97c5-4277-ad5d-0bfe630e4e50'];
var channelIDCounter = 0;

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

add_task(function* test_webapps_cleardata() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  let unregisterDone;
  let unregisterPromise = new Promise(resolve => unregisterDone = resolve);

  PushService.init({
    serverURI: "wss://push.example.org",
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
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200,
            channelID: data.channelID,
            uaid: userAgentID,
            pushEndpoint: 'https://example.com/update/' + Math.random(),
          }));
        },
        onUnregister(data) {
          unregisterDone();
        },
      });
    }
  });

  let registers = yield Promise.all([
    PushNotificationService.register(
      'https://example.org/1',
      ChromeUtils.originAttributesToSuffix({ appId: 1, inBrowser: false })),
    PushNotificationService.register(
      'https://example.org/1',
      ChromeUtils.originAttributesToSuffix({ appId: 1, inBrowser: true })),
  ]);

  Services.obs.notifyObservers(
      { appId: 1, browserOnly: false,
        QueryInterface: XPCOMUtils.generateQI([Ci.mozIApplicationClearPrivateDataParams])},
      "webapps-clear-data", "");

  let waitAWhile = new Promise(function(res) {
    setTimeout(res, 2000);
  });
  yield waitAWhile;

  let registration;
  registration = yield PushNotificationService.registration(
    'https://example.org/1',
    ChromeUtils.originAttributesToSuffix({ appId: 1, inBrowser: false }));
  ok(!registration, 'Registration for { 1, false } should not exist.');

  registration = yield PushNotificationService.registration(
    'https://example.org/1',
    ChromeUtils.originAttributesToSuffix({ appId: 1, inBrowser: true }));
  ok(registration, 'Registration for { 1, true } should still exist.');

  yield waitForPromise(unregisterPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for unregister');
});

