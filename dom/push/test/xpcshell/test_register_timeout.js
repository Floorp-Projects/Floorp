/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = 'a4be0df9-b16d-4b5f-8f58-0f93b6f1e23d';
const channelID = 'e1944e0b-48df-45e7-bdc0-d1fbaa7986d3';

function run_test() {
  do_get_profile();
  setPrefs({
    requestTimeout: 1000,
    retryBaseInterval: 150
  });
  run_next_test();
}

add_task(async function test_register_timeout() {
  let handshakes = 0;
  let timeoutDone;
  let timeoutPromise = new Promise(resolve => timeoutDone = resolve);
  let registers = 0;

  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushServiceWebSocket._generateID = () => channelID;
  PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          if (handshakes === 0) {
            equal(request.uaid, null, 'Should not include device ID');
          } else if (handshakes === 1) {
            // Should use the previously-issued device ID when reconnecting,
            // but should not include the timed-out channel ID.
            equal(request.uaid, userAgentID,
              'Should include device ID on reconnect');
          } else {
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
            timeoutDone();
          }, 2000);
          registers++;
        }
      });
    }
  });

  await rejects(
    PushService.register({
      scope: 'https://example.net/page/timeout',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    'Expected error for request timeout'
  );

  let record = await db.getByKeyID(channelID);
  ok(!record, 'Should not store records for timed-out responses');

  await timeoutPromise;
  equal(registers, 1, 'Should not handle timed-out register requests');
});
