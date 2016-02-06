/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

const userAgentID = '05f7b940-51b6-4b6f-8032-b83ebb577ded';

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: userAgentID,
    pingInterval: 10000,
    retryBaseInterval: 25,
  });
  run_next_test();
}

add_task(function* test_ws_retry() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  yield db.put({
    channelID: '61770ba9-2d57-4134-b949-d40404630d5b',
    pushEndpoint: 'https://example.org/push/1',
    scope: 'https://example.net/push/1',
    version: 1,
    originAttributes: '',
    quota: Infinity,
  });

  let alarmDelays = [];
  let setAlarm = PushService.setAlarm;
  PushService.setAlarm = function(delay) {
    alarmDelays.push(delay);
    setAlarm.apply(this, arguments);
  };

  let handshakeDone;
  let handshakePromise = new Promise(resolve => handshakeDone = resolve);
  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    makeWebSocket(uri) {
      return new MockWebSocket(uri, {
        onHello(request) {
          if (alarmDelays.length == 10) {
            PushService.setAlarm = setAlarm;
            this.serverSendMsg(JSON.stringify({
              messageType: 'hello',
              status: 200,
              uaid: userAgentID,
            }));
            handshakeDone();
            return;
          }
          this.serverInterrupt();
        },
      });
    },
  });

  yield waitForPromise(
    handshakePromise,
    45000,
    'Timed out waiting for successful handshake'
  );
  deepEqual(alarmDelays, [25, 50, 100, 200, 400, 800, 1600, 3200, 6400, 10000],
    'Wrong reconnect alarm delays');
});
