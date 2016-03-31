/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

var userAgentID = '5ab1d1df-7a3d-4024-a469-b9e1bb399fad';

function run_test() {
  do_get_profile();
  setPrefs({userAgentID});
  run_next_test();
}

add_task(function* test_notification_ack() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  let records = [{
    channelID: '21668e05-6da8-42c9-b8ab-9cc3f4d5630c',
    pushEndpoint: 'https://example.com/update/1',
    scope: 'https://example.org/1',
    originAttributes: '',
    version: 1,
    quota: Infinity,
    systemRecord: true,
  }, {
    channelID: '9a5ff87f-47c9-4215-b2b8-0bdd38b4b305',
    pushEndpoint: 'https://example.com/update/2',
    scope: 'https://example.org/2',
    originAttributes: '',
    version: 2,
    quota: Infinity,
    systemRecord: true,
  }, {
    channelID: '5477bfda-22db-45d4-9614-fee369630260',
    pushEndpoint: 'https://example.com/update/3',
    scope: 'https://example.org/3',
    originAttributes: '',
    version: 3,
    quota: Infinity,
    systemRecord: true,
  }];
  for (let record of records) {
    yield db.put(record);
  }

  let notifyCount = 0;
  let notifyPromise = promiseObserverNotification(PushServiceComponent.pushTopic, () =>
    ++notifyCount == 3);

  let acks = 0;
  let ackDone;
  let ackPromise = new Promise(resolve => ackDone = resolve);
  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          equal(request.uaid, userAgentID,
            'Should send matching device IDs in handshake');
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            uaid: userAgentID,
            status: 200
          }));
          this.serverSendMsg(JSON.stringify({
            messageType: 'notification',
            updates: [{
              channelID: '21668e05-6da8-42c9-b8ab-9cc3f4d5630c',
              version: 2
            }]
          }));
        },
        onACK(request) {
          equal(request.messageType, 'ack', 'Should send acknowledgements');
          let updates = request.updates;
          switch (++acks) {
          case 1:
            deepEqual([{
              channelID: '21668e05-6da8-42c9-b8ab-9cc3f4d5630c',
              version: 2,
              code: 100,
            }], updates, 'Wrong updates for acknowledgement 1');
            this.serverSendMsg(JSON.stringify({
              messageType: 'notification',
              updates: [{
                channelID: '9a5ff87f-47c9-4215-b2b8-0bdd38b4b305',
                version: 4
              }, {
                channelID: '5477bfda-22db-45d4-9614-fee369630260',
                version: 6
              }]
            }));
            break;

          case 2:
            deepEqual([{
              channelID: '9a5ff87f-47c9-4215-b2b8-0bdd38b4b305',
              version: 4,
              code: 100,
            }], updates, 'Wrong updates for acknowledgement 2');
            break;

          case 3:
            deepEqual([{
              channelID: '5477bfda-22db-45d4-9614-fee369630260',
              version: 6,
              code: 100,
            }], updates, 'Wrong updates for acknowledgement 3');
            ackDone();
            break;

          default:
            ok(false, 'Unexpected acknowledgement ' + acks);
          }
        }
      });
    }
  });

  yield notifyPromise;
  yield ackPromise;
});
