/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = 'bd744428-f125-436a-b6d0-dd0c9845837f';

let clearForPattern = Task.async(function* (testRecords, pattern) {
  let patternString = JSON.stringify(pattern);
  yield PushService._clearOriginData(patternString);

  for (let length = testRecords.length; length--;) {
    let test = testRecords[length];
    let originSuffix = ChromeUtils.originAttributesToSuffix(
      test.originAttributes);

    let registration = yield PushService.registration({
      scope: test.scope,
      originAttributes: originSuffix,
    });

    let url = test.scope + originSuffix;

    if (ObjectUtils.deepEqual(test.clearIf, pattern)) {
      ok(!registration, 'Should clear registration ' + url +
        ' for pattern ' + patternString);
      testRecords.splice(length, 1);
    } else {
      ok(registration, 'Should not clear registration ' + url +
        ' for pattern ' + patternString);
    }
  }
});

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

  let testRecords = [{
    scope: 'https://example.org/1',
    originAttributes: { appId: 1 },
    clearIf: { appId: 1, inBrowser: false },
  }, {
    scope: 'https://example.org/1',
    originAttributes: { appId: 1, inBrowser: true },
    clearIf: { appId: 1 },
  }, {
    scope: 'https://example.org/1',
    originAttributes: { appId: 2, inBrowser: true },
    clearIf: { appId: 2, inBrowser: true },
  }, {
    scope: 'https://example.org/2',
    originAttributes: { appId: 1 },
    clearIf: { appId: 1, inBrowser: false },
  }, {
    scope: 'https://example.org/2',
    originAttributes: { appId: 2, inBrowser: true },
    clearIf: { appId: 2, inBrowser: true },
  }, {
    scope: 'https://example.org/3',
    originAttributes: { appId: 3, inBrowser: true },
    clearIf: { inBrowser: true },
  }, {
    scope: 'https://example.org/3',
    originAttributes: { appId: 4, inBrowser: true },
    clearIf: { inBrowser: true },
  }];

  let unregisterDone;
  let unregisterPromise = new Promise(resolve =>
    unregisterDone = after(testRecords.length, resolve));

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

  yield Promise.all(testRecords.map(test =>
    PushService.register({
      scope: test.scope,
      originAttributes: ChromeUtils.originAttributesToSuffix(
        test.originAttributes),
    })
  ));

  // Removes records for all scopes with the same app ID. Excludes records
  // where `inBrowser` is true.
  yield clearForPattern(testRecords, { appId: 1, inBrowser: false });

  // Removes the remaining record for app ID 1, where `inBrowser` is true.
  yield clearForPattern(testRecords, { appId: 1 });

  // Removes all records for all scopes with the same app ID, where
  // `inBrowser` is true.
  yield clearForPattern(testRecords, { appId: 2, inBrowser: true });

  // Removes all records where `inBrowser` is true.
  yield clearForPattern(testRecords, { inBrowser: true });

  equal(testRecords.length, 0, 'Should remove all test records');
  yield waitForPromise(unregisterPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for unregister');
});
