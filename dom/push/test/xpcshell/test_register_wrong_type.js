/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = 'c293fdc5-a75e-4eb1-af88-a203991c0787';

function run_test() {
  do_get_profile();
  setPrefs({
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  disableServiceWorkerEvents(
    'https://example.com/mistyped'
  );
  run_next_test();
}

add_task(function* test_register_wrong_type() {
  let registers = 0;
  let helloDefer = Promise.defer();
  let helloDone = after(2, helloDefer.resolve);

  PushService._generateID = () => '1234';
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
          registers++;
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200,
            channelID: 1234,
            uaid: userAgentID,
            pushEndpoint: 'https://example.org/update/wrong-type'
          }));
        }
      });
    }
  });

  let promise =

  yield rejects(
    PushNotificationService.register('https://example.com/mistyped'),
    function(error) {
      return error == 'TimeoutError';
    },
    'Wrong error for non-string channel ID'
  );

  yield waitForPromise(helloDefer.promise, DEFAULT_TIMEOUT,
    'Reconnect after sending non-string channel ID timed out');
  equal(registers, 1, 'Wrong register count');
});
