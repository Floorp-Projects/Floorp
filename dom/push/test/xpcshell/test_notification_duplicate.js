/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '1500e7d9-8cbe-4ee6-98da-7fa5d6a39852';

function run_test() {
  do_get_profile();
  setPrefs({
    maxRecentMessageIDsPerSubscription: 4,
    userAgentID: userAgentID,
  });
  run_next_test();
}

// Should acknowledge duplicate notifications, but not notify apps.
add_task(async function test_notification_duplicate() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {return db.drop().then(_ => db.close());});
  let records = [{
    channelID: 'has-recents',
    pushEndpoint: 'https://example.org/update/1',
    scope: 'https://example.com/1',
    originAttributes: "",
    recentMessageIDs: ['dupe'],
    quota: Infinity,
    systemRecord: true,
  }, {
    channelID: 'no-recents',
    pushEndpoint: 'https://example.org/update/2',
    scope: 'https://example.com/2',
    originAttributes: "",
    quota: Infinity,
    systemRecord: true,
  }, {
    channelID: 'dropped-recents',
    pushEndpoint: 'https://example.org/update/3',
    scope: 'https://example.com/3',
    originAttributes: '',
    recentMessageIDs: ['newest', 'newer', 'older', 'oldest'],
    quota: Infinity,
    systemRecord: true,
  }];
  for (let record of records) {
    await db.put(record);
  }

  let testData = [{
    channelID: 'has-recents',
    updates: 1,
    acks: [{
      version: 'dupe',
      code: 102,
    }, {
      version: 'not-dupe',
      code: 100,
    }],
    recents: ['not-dupe', 'dupe'],
  }, {
    channelID: 'no-recents',
    updates: 1,
    acks: [{
      version: 'not-dupe',
      code: 100,
    }],
    recents: ['not-dupe'],
  }, {
    channelID: 'dropped-recents',
    acks: [{
      version: 'overflow',
      code: 100,
    }, {
      version: 'oldest',
      code: 100,
    }],
    updates: 2,
    recents: ['oldest', 'overflow', 'newest', 'newer'],
  }];

  let expectedUpdates = testData.reduce((sum, {updates}) => sum + updates, 0);
  let notifiedScopes = [];
  let notifyPromise = promiseObserverNotification(PushServiceComponent.pushTopic, (subject, data) => {
    notifiedScopes.push(data);
    return notifiedScopes.length == expectedUpdates;
  });

  let expectedAcks = testData.reduce((sum, {acks}) => sum + acks.length, 0);
  let ackDone;
  let ackPromise = new Promise(resolve => ackDone = after(expectedAcks, resolve));
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
          for (let {channelID, acks} of testData) {
            for (let {version} of acks) {
              this.serverSendMsg(JSON.stringify({
                messageType: 'notification',
                channelID: channelID,
                version: version,
              }))
            }
          }
        },
        onACK(request) {
          let [ack] = request.updates;
          let expectedData = testData.find(test =>
            test.channelID == ack.channelID);
          ok(expectedData, `Unexpected channel ${ack.channelID}`);
          let expectedAck = expectedData.acks.find(expectedAck =>
            expectedAck.version == ack.version);
          ok(expectedAck, `Unexpected ack for message ${
            ack.version} on ${ack.channelID}`);
          equal(expectedAck.code, ack.code, `Wrong ack status for message ${
            ack.version} on ${ack.channelID}`);
          ackDone();
        },
      });
    }
  });

  await notifyPromise;
  await ackPromise;

  for (let {channelID, recents} of testData) {
    let record = await db.getByKeyID(channelID);
    deepEqual(record.recentMessageIDs, recents,
      `Wrong recent message IDs for ${channelID}`);
  }
});
