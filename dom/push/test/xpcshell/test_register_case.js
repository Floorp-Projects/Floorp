/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

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
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushService.init({
    serverURI: "wss://push.example.org/",
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
    PushService.register({
      scope: 'https://example.net/case',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
    }),
    DEFAULT_TIMEOUT,
    'Mixed-case register response timed out'
  );
  equal(newRecord.endpoint, 'https://example.com/update/case',
    'Wrong push endpoint in registration record');

  let record = yield db.getByPushEndpoint('https://example.com/update/case');
  equal(record.scope, 'https://example.net/case',
    'Wrong scope in database record');
});
