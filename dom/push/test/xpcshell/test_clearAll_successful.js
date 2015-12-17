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
    version: 1,
    originAttributes: '',
    quota: Infinity,
  });

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
        }
      });
    }
  });

  yield PushService._clearAll();
  let record = yield db.getByKeyID(channelID);
  ok(!record, 'Unregister did not remove record');
});
