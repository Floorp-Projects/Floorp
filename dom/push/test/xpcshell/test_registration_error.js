/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceWebSocket} = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: '6faed1f0-1439-4aac-a978-db21c81cd5eb'
  });
  run_next_test();
}

add_task(async function test_registrations_error() {
  let db = PushServiceWebSocket.newPushDB();
  registerCleanupFunction(() => {return db.drop().then(_ => db.close());});

  PushService.init({
    serverURI: "wss://push.example.org/",
    db: makeStub(db, {
      getByIdentifiers(prev, scope) {
        return Promise.reject('Database error');
      }
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri);
    }
  });

  await rejects(
    PushService.registration({
      scope: 'https://example.net/1',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    }),
    function(error) {
      return error == 'Database error';
    },
    'Wrong message'
  );
});
