/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { PushDB, PushService } = serviceExports;

const userAgentID = "a722e448-c481-4c48-aea0-fc411cb7c9ed";

function run_test() {
  do_get_profile();
  setPrefs({ userAgentID });
  run_next_test();
}

// Should not open a connection if the client has no registrations.
add_task(async function test_registration_none() {
  PushService.init({
    serverURI: "wss://push.example.org/",
    makeWebSocket(uri) {
      return new MockWebSocket(uri);
    },
  });

  let registration = await PushService.registration({
    scope: "https://example.net/1",
    originAttributes: ChromeUtils.originAttributesToSuffix({
      inIsolatedMozBrowser: false,
    }),
  });
  ok(!registration, "Should not open a connection without registration");
});
