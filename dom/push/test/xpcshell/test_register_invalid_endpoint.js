/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = 'c9a12e81-ea5e-40f9-8bf4-acee34621671';
const channelID = 'c0660af8-b532-4931-81f0-9fd27a12d6ab';

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(async function test_register_invalid_endpoint() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {return db.drop().then(_ => db.close());});

  PushServiceWebSocket._generateID = () => channelID;
  PushService.init({
    serverURI: "wss://push.example.org/",
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

  await rejects(
    PushService.register({
      scope: 'https://example.net/page/invalid-endpoint',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    'Expected error for invalid endpoint'
  );

  let record = await db.getByKeyID(channelID);
  ok(!record, 'Should not store records with invalid endpoints');
});
