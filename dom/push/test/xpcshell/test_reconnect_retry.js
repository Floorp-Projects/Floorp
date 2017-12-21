/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs({
    requestTimeout: 10000,
    retryBaseInterval: 150
  });
  run_next_test();
}

add_task(async function test_reconnect_retry() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {return db.drop().then(_ => db.close());});

  let registers = 0;
  let channelID;
  PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: '083e6c17-1063-4677-8638-ab705aebebc2'
          }));
        },
        onRegister(request) {
          registers++;
          if (registers == 1) {
            channelID = request.channelID;
            this.serverClose();
            return;
          }
          if (registers == 2) {
            equal(request.channelID, channelID,
              'Should retry registers after reconnect');
          }
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            channelID: request.channelID,
            pushEndpoint: 'https://example.org/push/' + request.channelID,
            status: 200,
          }));
        }
      });
    }
  });

  let registration = await PushService.register({
    scope: 'https://example.com/page/1',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
  });
  let retryEndpoint = 'https://example.org/push/' + channelID;
  equal(registration.endpoint, retryEndpoint, 'Wrong endpoint for retried request');

  registration = await PushService.register({
    scope: 'https://example.com/page/2',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
  });
  notEqual(registration.endpoint, retryEndpoint, 'Wrong endpoint for new request');

  equal(registers, 3, 'Wrong registration count');
});
