/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

var userAgentID = '9a2f9efe-2ebb-4bcb-a5d9-9e2b73d30afe';
var channelID = '264c2ba0-f6db-4e84-acdb-bd225b62d9e3';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  run_next_test();
}

add_task(function* test_register_no_id() {
  let registers = 0;
  let helloDone;
  let helloPromise = new Promise(resolve => helloDone = after(2, resolve));

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
          registers++;
          equal(request.channelID, channelID, 'Register: wrong channel ID');
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200
          }));
        }
      });
    }
  });

  yield rejects(
    PushService.register({
      scope: 'https://example.com/incomplete',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    'Expected error for incomplete register response'
  );

  yield helloPromise;
  equal(registers, 1, 'Wrong register count');
});
