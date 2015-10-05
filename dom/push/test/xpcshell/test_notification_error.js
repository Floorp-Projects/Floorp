/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '3c7462fc-270f-45be-a459-b9d631b0d093';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: userAgentID,
  });
  disableServiceWorkerEvents(
    'https://example.com/a',
    'https://example.com/b',
    'https://example.com/c'
  );
  run_next_test();
}

add_task(function* test_notification_error() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  let originAttributes = '';
  let records = [{
    channelID: 'f04f1e46-9139-4826-b2d1-9411b0821283',
    pushEndpoint: 'https://example.org/update/success-1',
    scope: 'https://example.com/a',
    originAttributes: originAttributes,
    version: 1,
    quota: Infinity,
  }, {
    channelID: '3c3930ba-44de-40dc-a7ca-8a133ec1a866',
    pushEndpoint: 'https://example.org/update/error',
    scope: 'https://example.com/b',
    originAttributes: originAttributes,
    version: 2,
    quota: Infinity,
  }, {
    channelID: 'b63f7bef-0a0d-4236-b41e-086a69dfd316',
    pushEndpoint: 'https://example.org/update/success-2',
    scope: 'https://example.com/c',
    originAttributes: originAttributes,
    version: 3,
    quota: Infinity,
  }];
  for (let record of records) {
    yield db.put(record);
  }

  let notifyPromise = Promise.all([
    promiseObserverNotification(
      'push-notification',
      (subject, data) => data == 'https://example.com/a'
    ),
    promiseObserverNotification(
      'push-notification',
      (subject, data) => data == 'https://example.com/c'
    )
  ]);

  let ackDone;
  let ackPromise = new Promise(resolve => ackDone = after(records.length, resolve));
  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    db: makeStub(db, {
      getByKeyID(prev, channelID) {
        if (channelID == '3c3930ba-44de-40dc-a7ca-8a133ec1a866') {
          return Promise.reject('splines not reticulated');
        }
        return prev.call(this, channelID);
      }
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          deepEqual(request.channelIDs.sort(), [
            '3c3930ba-44de-40dc-a7ca-8a133ec1a866',
            'b63f7bef-0a0d-4236-b41e-086a69dfd316',
            'f04f1e46-9139-4826-b2d1-9411b0821283'
          ], 'Wrong channel list');
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID,
          }));
          this.serverSendMsg(JSON.stringify({
            messageType: 'notification',
            updates: records.map(({channelID, version}) =>
              ({channelID, version: ++version}))
          }));
        },
        // Should acknowledge all received updates, even if updating
        // IndexedDB fails.
        onACK: ackDone
      });
    }
  });

  let [a, c] = yield waitForPromise(
    notifyPromise,
    DEFAULT_TIMEOUT,
    'Timed out waiting for notifications'
  );
  let aPush = a.subject.QueryInterface(Ci.nsIPushObserverNotification);
  equal(aPush.pushEndpoint, 'https://example.org/update/success-1',
    'Wrong endpoint for notification A');
  equal(aPush.version, 2, 'Wrong version for notification A');

  let cPush = c.subject.QueryInterface(Ci.nsIPushObserverNotification);
  equal(cPush.pushEndpoint, 'https://example.org/update/success-2',
    'Wrong endpoint for notification C');
  equal(cPush.version, 4, 'Wrong version for notification C');

  yield waitForPromise(ackPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for acknowledgements');

  let aRecord = yield db.getByIdentifiers({scope: 'https://example.com/a',
                                           originAttributes: originAttributes });
  equal(aRecord.channelID, 'f04f1e46-9139-4826-b2d1-9411b0821283',
    'Wrong channel ID for record A');
  strictEqual(aRecord.version, 2,
    'Should return the new version for record A');

  let bRecord = yield db.getByIdentifiers({scope: 'https://example.com/b',
                                           originAttributes: originAttributes });
  equal(bRecord.channelID, '3c3930ba-44de-40dc-a7ca-8a133ec1a866',
    'Wrong channel ID for record B');
  strictEqual(bRecord.version, 2,
    'Should return the previous version for record B');

  let cRecord = yield db.getByIdentifiers({scope: 'https://example.com/c',
                                           originAttributes: originAttributes });
  equal(cRecord.channelID, 'b63f7bef-0a0d-4236-b41e-086a69dfd316',
    'Wrong channel ID for record C');
  strictEqual(cRecord.version, 4,
    'Should return the new version for record C');
});
