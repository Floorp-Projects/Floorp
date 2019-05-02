/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

function run_test() {
  do_get_profile();
  run_next_test();
}

add_task(async function test_registrations_error() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => { return db.drop().then(_ => db.close()); });

  PushService.init({
    serverURI: "https://push.example.org/",
    db: makeStub(db, {
      getByIdentifiers() {
        return Promise.reject("Database error");
      },
    }),
  });

  await Assert.rejects(
    PushService.registration({
      scope: "https://example.net/1",
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { inIsolatedMozBrowser: false }),
    }),
    function(error) {
      return error == "Database error";
    },
    "Wrong message"
  );
});
