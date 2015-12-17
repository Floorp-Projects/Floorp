/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const channelID = 'db0a7021-ec2d-4bd3-8802-7a6966f10ed8';

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(function* test_unregister_success() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  yield db.put({
    channelID,
    pushEndpoint: 'https://example.org/update/unregister-success',
    scope: 'https://example.com/page/unregister-success',
    originAttributes: '',
    version: 1,
    quota: Infinity,
  });

  let unregisterDone;
  let unregisterPromise = new Promise(resolve => unregisterDone = resolve);
  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: 'fbe865a6-aeb8-446f-873c-aeebdb8d493c'
          }));
        },
        onUnregister(request) {
          equal(request.channelID, channelID, 'Should include the channel ID');
          this.serverSendMsg(JSON.stringify({
            messageType: 'unregister',
            status: 200,
            channelID
          }));
          unregisterDone();
        }
      });
    }
  });

  yield PushService.unregister({
    scope: 'https://example.com/page/unregister-success',
    originAttributes: '',
  });
  let record = yield db.getByKeyID(channelID);
  ok(!record, 'Unregister did not remove record');

  yield waitForPromise(unregisterPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for unregister');
});
