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

add_task(function* test_registrations_error() {
  let db = PushServiceWebSocket.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushService.init({
    serverURI: "wss://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    db: makeStub(db, {
      getByIdentifiers(prev, scope) {
        return Promise.reject('Database error');
      }
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri);
    }
  });

  yield rejects(
    PushService.registration({
      scope: 'https://example.net/1',
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
    }),
    function(error) {
      return error == 'Database error';
    },
    'Wrong message'
  );
});
