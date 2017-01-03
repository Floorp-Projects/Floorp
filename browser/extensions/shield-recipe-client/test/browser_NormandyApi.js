"use strict";

const {utils: Cu} = Components;
Cu.import("resource://shield-recipe-client/lib/NormandyApi.jsm", this);

add_task(function* () {
  // Point the add-on to the test server.
  yield SpecialPowers.pushPrefEnv({
    set: [
      [
        "extensions.shield-recipe-client.api_url",
        "http://mochi.test:8888/browser/browser/extensions/shield-recipe-client/test",
      ]
    ]
  })

  // Test that NormandyApi can fetch from the test server.
  const response = yield NormandyApi.get("test_server.sjs");
  const data = yield response.json();
  Assert.deepEqual(data, {test: "data"}, "NormandyApi returned incorrect server data.");
});
