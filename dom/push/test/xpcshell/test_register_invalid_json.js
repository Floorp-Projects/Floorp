/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '8271186b-8073-43a3-adf6-225bd44a8b0a';
const channelID = '2d08571e-feab-48a0-9f05-8254c3c7e61f';

function run_test() {
  do_get_profile();
  setPrefs({
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  run_next_test();
}

add_task(async function test_register_invalid_json() {
  let helloDone;
  let helloPromise = new Promise(resolve => helloDone = after(2, resolve));
  let registers = 0;

  PushServiceWebSocket._generateID = () => channelID;
  PushService.init({
    serverURI: "wss://push.example.org/",
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID
          }));
          helloDone();
        },
        onRegister(request) {
          equal(request.channelID, channelID, 'Register: wrong channel ID');
          this.serverSendMsg(');alert(1);(');
          registers++;
        }
      });
    }
  });

  await rejects(
    PushService.register({
      scope: 'https://example.net/page/invalid-json',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    /Registration error/,
    'Expected error for invalid JSON response'
  );

  await helloPromise;
  equal(registers, 1, 'Wrong register count');
});
