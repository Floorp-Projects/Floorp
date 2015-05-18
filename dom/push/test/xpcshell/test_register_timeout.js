/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

const userAgentID = 'a4be0df9-b16d-4b5f-8f58-0f93b6f1e23d';
const channelID = 'e1944e0b-48df-45e7-bdc0-d1fbaa7986d3';

function run_test() {
  do_get_profile();
  setPrefs({
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  disableServiceWorkerEvents(
    'https://example.net/page/timeout'
  );
  run_next_test();
}

add_task(function* test_register_timeout() {
  let handshakes = 0;
  let timeoutDefer = Promise.defer();
  let registers = 0;

  let db = new PushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushService._generateID = () => channelID;
  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          switch (handshakes) {
          case 0:
            equal(request.uaid, null, 'Should not include device ID');
            deepEqual(request.channelIDs, [],
              'Should include empty channel list');
            break;

          case 1:
            // Should use the previously-issued device ID when reconnecting,
            // but should not include the timed-out channel ID.
            equal(request.uaid, userAgentID,
              'Should include device ID on reconnect');
            deepEqual(request.channelIDs, [],
              'Should not include failed channel ID');
            break;

          default:
            ok(false, 'Unexpected reconnect attempt ' + handshakes);
          }
          handshakes++;
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID,
          }));
        },
        onRegister(request) {
          equal(request.channelID, channelID,
            'Wrong channel ID in register request');
          setTimeout(() => {
            // Should ignore replies for timed-out requests.
            this.serverSendMsg(JSON.stringify({
              messageType: 'register',
              status: 200,
              channelID: channelID,
              uaid: userAgentID,
              pushEndpoint: 'https://example.com/update/timeout',
            }));
            timeoutDefer.resolve();
          }, 2000);
          registers++;
        }
      });
    }
  });

  yield rejects(
    PushNotificationService.register('https://example.net/page/timeout'),
    function(error) {
      return error == 'TimeoutError';
    },
    'Wrong error for request timeout'
  );

  let record = yield db.getByChannelID(channelID);
  ok(!record, 'Should not store records for timed-out responses');

  yield waitForPromise(
    timeoutDefer.promise,
    DEFAULT_TIMEOUT,
    'Reconnect timed out'
  );
  equal(registers, 1, 'Should not handle timed-out register requests');
});
