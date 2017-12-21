/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '7f0af1bb-7e1f-4fb8-8e4a-e8de434abde3';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID,
    requestTimeout: 150,
    retryBaseInterval: 150
  });
  run_next_test();
}

add_task(async function test_unregister_invalid_json() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {return db.drop().then(_ => db.close());});
  let records = [{
    channelID: '87902e90-c57e-4d18-8354-013f4a556559',
    pushEndpoint: 'https://example.org/update/1',
    scope: 'https://example.edu/page/1',
    originAttributes: '',
    version: 1,
    quota: Infinity,
  }, {
    channelID: '057caa8f-9b99-47ff-891c-adad18ce603e',
    pushEndpoint: 'https://example.com/update/2',
    scope: 'https://example.net/page/1',
    originAttributes: '',
    version: 1,
    quota: Infinity,
  }];
  for (let record of records) {
    await db.put(record);
  }

  let unregisterDone;
  let unregisterPromise = new Promise(resolve => unregisterDone = after(2, resolve));
  PushService.init({
    serverURI: "wss://push.example.org/",
    db,
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          this.serverSendMsg(JSON.stringify({
            messageType: 'hello',
            status: 200,
            uaid: userAgentID,
            use_webpush: true,
          }));
        },
        onUnregister(request) {
          this.serverSendMsg(');alert(1);(');
          unregisterDone();
        }
      });
    }
  });

  await rejects(
    PushService.unregister({
      scope: 'https://example.edu/page/1',
      originAttributes: '',
    }),
    'Expected error for first invalid JSON response'
  );

  let record = await db.getByKeyID(
    '87902e90-c57e-4d18-8354-013f4a556559');
  ok(!record, 'Failed to delete unregistered record');

  await rejects(
    PushService.unregister({
      scope: 'https://example.net/page/1',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    'Expected error for second invalid JSON response'
  );

  record = await db.getByKeyID(
    '057caa8f-9b99-47ff-891c-adad18ce603e');
  ok(!record,
    'Failed to delete unregistered record after receiving invalid JSON');

  await unregisterPromise;
});
