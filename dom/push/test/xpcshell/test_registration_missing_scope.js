/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

const {PushDB, PushService} = serviceExports;

function run_test() {
  do_get_profile();
  setPrefs();
  run_next_test();
}

add_task(function* test_registration_missing_scope() {
  PushService.init({
    serverURI: "wss://push.example.org/",
    makeWebSocket(uri) {
      return new MockWebSocket(uri);
    }
  });
  yield rejects(
    PushService.registration({ scope: '', originAttributes: '' }),
    'Record missing page and manifest URLs'
  );
});
