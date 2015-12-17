/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

function run_test() {
  do_get_profile();
  run_next_test();
}

add_task(function* test_registrations_error() {
  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushService.init({
    serverURI: "https://push.example.org/",
    networkInfo: new MockDesktopNetworkInfo(),
    db: makeStub(db, {
      getByIdentifiers() {
        return Promise.reject('Database error');
      }
    }),
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
