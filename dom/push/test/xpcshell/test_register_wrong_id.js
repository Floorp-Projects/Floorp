/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '84afc774-6995-40d1-9c90-8c34ddcd0cb4';
const clientChannelID = '4b42a681c99e4dfbbb166a7e01a09b8b';
const serverChannelID = '3f5aeb89c6e8405a9569619522783436';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  disableServiceWorkerEvents(
    'https://example.com/mismatched'
  );
  run_next_test();
}

add_task(function* test_register_wrong_id() {
  // Should reconnect after the register request times out.
  let registers = 0;
  let helloDone;
  let helloPromise = new Promise(resolve => helloDone = after(2, resolve));

  PushServiceWebSocket._generateID = () => clientChannelID;
  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
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
          equal(request.channelID, clientChannelID,
            'Register: wrong channel ID');
          registers++;
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200,
            // Reply with a different channel ID. Since the ID is used as a
            // nonce, the registration request will time out.
            channelID: serverChannelID
          }));
        }
      });
    }
  });

  yield rejects(
    PushService.register({
      scope: 'https://example.com/mismatched',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
    }),
    'Expected error for mismatched register reply'
  );

  yield waitForPromise(helloPromise, DEFAULT_TIMEOUT,
    'Reconnect after mismatched register reply timed out');
  equal(registers, 1, 'Wrong register count');
});
