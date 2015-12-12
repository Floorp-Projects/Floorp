/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket, PushCrypto} = serviceExports;

const userAgentID = '4dffd396-6582-471d-8c0c-84f394e9f7db';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
  });
  disableServiceWorkerEvents(
    'https://example.com/page/1',
    'https://example.com/page/2',
    'https://example.com/page/3'
  );
  run_next_test();
}

add_task(function* test_with_data_enabled() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  let [publicKey, privateKey] = yield PushCrypto.generateKeys();
  let records = [{
    channelID: 'eb18f12a-cc42-4f14-accb-3bfc1227f1aa',
    pushEndpoint: 'https://example.org/push/no-key/1',
    scope: 'https://example.com/page/1',
    originAttributes: '',
    quota: Infinity,
  }, {
    channelID: '0d8886b9-8da1-4778-8f5d-1cf93a877ed6',
    pushEndpoint: 'https://example.org/push/key',
    scope: 'https://example.com/page/2',
    originAttributes: '',
    p256dhPublicKey: publicKey,
    p256dhPrivateKey: privateKey,
    quota: Infinity,
  }];
  for (let record of records) {
    yield db.put(record);
  }

  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          ok(request.use_webpush,
            'Should use Web Push if data delivery is enabled');
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: request.uaid,
            use_webpush: true,
          }));
        },
        onRegister(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'register',
            status: 200,
            uaid: userAgentID,
            channelID: request.channelID,
            pushEndpoint: 'https://example.org/push/new',
          }));
        }
      });
    },
  });

  let newRecord = yield PushNotificationService.register(
    'https://example.com/page/3',
    ChromeUtils.originAttributesToSuffix({ appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false })
  );
  ok(newRecord.p256dhKey, 'Should generate public keys for new records');

  let record = yield db.getByKeyID('eb18f12a-cc42-4f14-accb-3bfc1227f1aa');
  ok(record.p256dhPublicKey, 'Should add public key to partial record');
  ok(record.p256dhPrivateKey, 'Should add private key to partial record');

  record = yield db.getByKeyID('0d8886b9-8da1-4778-8f5d-1cf93a877ed6');
  deepEqual(record.p256dhPublicKey, publicKey,
    'Should leave existing public key');
  deepEqual(record.p256dhPrivateKey, privateKey,
    'Should leave existing private key');
});
