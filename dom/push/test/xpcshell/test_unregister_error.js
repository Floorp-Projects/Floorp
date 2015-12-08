/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const channelID = '00c7fa13-7b71-447d-bd27-a91abc09d1b2';

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(function* test_unregister_error() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  yield db.put({
    channelID: channelID,
    pushEndpoint: 'https://example.org/update/failure',
    scope: 'https://example.net/page/failure',
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
            uaid: '083e6c17-1063-4677-8638-ab705aebebc2'
          }));
        },
        onUnregister(request) {
          // The server is notified out-of-band. Since channels may be pruned,
          // any failures are swallowed.
          equal(request.channelID, channelID, 'Unregister: wrong channel ID');
          this.serverSendMsg(JSON.stringify({
            messageType: 'unregister',
            status: 500,
            error: 'omg, everything is exploding',
            channelID
          }));
          unregisterDone();
        }
      });
    }
  });

  yield PushService.unregister({
    scope: 'https://example.net/page/failure',
    originAttributes: '',
  });

  let result = yield db.getByKeyID(channelID);
  ok(!result, 'Deleted push record exists');

  // Make sure we send a request to the server.
  yield waitForPromise(unregisterPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for unregister');
});
