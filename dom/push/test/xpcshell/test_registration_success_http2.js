/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var serverPort = -1;

function run_test() {
  serverPort = getTestServerPort();

  do_get_profile();

  run_next_test();
}

add_task(async function test_pushNotifications() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  var serverURL = "https://localhost:" + serverPort;

  let records = [{
    subscriptionUri: serverURL + "/subscriptionA",
    pushEndpoint: serverURL + "/pushEndpointA",
    pushReceiptEndpoint: serverURL + "/pushReceiptEndpointA",
    scope: "https://example.net/a",
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { inIsolatedMozBrowser: false }),
    quota: Infinity,
  }, {
    subscriptionUri: serverURL + "/subscriptionB",
    pushEndpoint: serverURL + "/pushEndpointB",
    pushReceiptEndpoint: serverURL + "/pushReceiptEndpointB",
    scope: "https://example.net/b",
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { inIsolatedMozBrowser: false }),
    quota: Infinity,
  }, {
    subscriptionUri: serverURL + "/subscriptionC",
    pushEndpoint: serverURL + "/pushEndpointC",
    pushReceiptEndpoint: serverURL + "/pushReceiptEndpointC",
    scope: "https://example.net/c",
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { inIsolatedMozBrowser: false }),
    quota: Infinity,
  }];

  for (let record of records) {
    await db.put(record);
  }

  PushService.init({
    serverURI: serverURL,
    db,
  });

  let registration = await PushService.registration({
    scope: "https://example.net/a",
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { inIsolatedMozBrowser: false }),
  });
  equal(
    registration.endpoint,
    serverURL + "/pushEndpointA",
    "Wrong push endpoint for scope"
  );
});
