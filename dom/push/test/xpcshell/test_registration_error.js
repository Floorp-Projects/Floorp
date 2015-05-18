/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs({
    userAgentID: '6faed1f0-1439-4aac-a978-db21c81cd5eb'
  });
  run_next_test();
}

add_task(function* test_registrations_error() {
  let db = new PushDB();
  do_register_cleanup(() => {return db.drop().then(_ => db.close());});

  PushService.init({
    networkInfo: new MockDesktopNetworkInfo(),
    db: makeStub(db, {
      getByScope(prev, scope) {
        return Promise.reject('oops');
      }
    }),
    makeWebSocket(uri) {
      return new MockWebSocket(uri);
    }
  });

  yield rejects(
    PushNotificationService.registration('https://example.net/1'),
    function(error) {
      return error == 'Database error';
    },
    'Wrong message'
  );
});
