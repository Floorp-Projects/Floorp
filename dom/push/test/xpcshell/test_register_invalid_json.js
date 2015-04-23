/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = '8271186b-8073-43a3-adf6-225bd44a8b0a';
const channelID = '2d08571e-feab-48a0-9f05-8254c3c7e61f';

function run_test() {
  do_get_profile();
  setPrefs({
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  disableServiceWorkerEvents(
    'https://example.net/page/invalid-json'
  );
  run_next_test();
}

add_task(function* test_register_invalid_json() {
  let helloDefer = Promise.defer();
  let helloDone = after(2, helloDefer.resolve);
  let registers = 0;

  PushService._generateID = () => channelID;
  PushService.init({
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
          equal(request.channelID, channelID, 'Register: wrong channel ID');
          this.serverSendMsg(');alert(1);(');
          registers++;
        }
      });
    }
  });

  yield rejects(
    PushNotificationService.register('https://example.net/page/invalid-json'),
    function(error) {
      return error == 'TimeoutError';
    },
    'Wrong error for invalid JSON response'
  );

  yield waitForPromise(helloDefer.promise, DEFAULT_TIMEOUT,
    'Reconnect after invalid JSON response timed out');
  equal(registers, 1, 'Wrong register count');
});
