/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '1760b1f5-c3ba-40e3-9344-adef7c18ab12';

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(async function test_register_case() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {return db.drop().then(_ => db.close());});

  PushService.init({
    serverURI: "wss://push.example.org/",
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

  let newRecord = await PushService.register({
    scope: 'https://example.net/case',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
  });
  equal(newRecord.endpoint, 'https://example.com/update/case',
    'Wrong push endpoint in registration record');

  let record = await db.getByPushEndpoint('https://example.com/update/case');
  equal(record.scope, 'https://example.net/case',
    'Wrong scope in database record');
});
