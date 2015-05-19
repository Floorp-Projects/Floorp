/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

let userAgentID = '5ab1d1df-7a3d-4024-a469-b9e1bb399fad';

function run_test() {
  do_get_profile();
  setPrefs({userAgentID});
  disableServiceWorkerEvents(
    'https://example.org/1',
    'https://example.org/2',
    'https://example.org/3'
  );
  run_next_test();
}

add_task(function* test_notification_ack() {
  let db = new PushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});
  let records = [{
    channelID: '21668e05-6da8-42c9-b8ab-9cc3f4d5630c',
    pushEndpoint: 'https://example.com/update/1',
    scope: 'https://example.org/1',
    version: 1
  }, {
    channelID: '9a5ff87f-47c9-4215-b2b8-0bdd38b4b305',
    pushEndpoint: 'https://example.com/update/2',
    scope: 'https://example.org/2',
    version: 2
  }, {
    channelID: '5477bfda-22db-45d4-9614-fee369630260',
    pushEndpoint: 'https://example.com/update/3',
    scope: 'https://example.org/3',
    version: 3
  }];
  for (let record of records) {
    yield db.put(record);
  }

  let notifyPromise = Promise.all([
    promiseObserverNotification('push-notification'),
    promiseObserverNotification('push-notification'),
    promiseObserverNotification('push-notification')
  ]);

  let acks = 0;
  let ackDefer = Promise.defer();
  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          equal(request.uaid, userAgentID,
            'Should send matching device IDs in handshake');
          deepEqual(request.channelIDs.sort(), [
            '21668e05-6da8-42c9-b8ab-9cc3f4d5630c',
            '5477bfda-22db-45d4-9614-fee369630260',
            '9a5ff87f-47c9-4215-b2b8-0bdd38b4b305'
          ], 'Should send matching channel IDs in handshake');
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
          ok(Array.isArray(updates),
            'Should send an array of acknowledged updates');
          equal(updates.length, 1,
            'Should send one acknowledged update per packet');
          switch (++acks) {
          case 1:
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
              version: 4
            }], updates, 'Wrong updates for acknowledgement 2');
            break;

          case 3:
            deepEqual([{
              channelID: '5477bfda-22db-45d4-9614-fee369630260',
              version: 6
            }], updates, 'Wrong updates for acknowledgement 3');
            ackDefer.resolve();
            break;

          default:
            ok(false, 'Unexpected acknowledgement ' + acks);
          }
        }
      });
    }
  });

  yield waitForPromise(notifyPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for notifications');
  yield waitForPromise(ackDefer.promise, DEFAULT_TIMEOUT,
    'Timed out waiting for multiple acknowledgements');
});
