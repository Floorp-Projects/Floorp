/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var pushEnabled;
var pushConnectionEnabled;

var serverPort = -1;

function run_test() {
  serverPort = getTestServerPort();

  do_get_profile();
  pushEnabled = Services.prefs.getBoolPref("dom.push.enabled");
  pushConnectionEnabled = Services.prefs.getBoolPref("dom.push.connection.enabled");

  // Set to allow the cert presented by our H2 server
  var oldPref = Services.prefs.getIntPref("network.http.speculative-parallel-limit");
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  Services.prefs.setBoolPref("dom.push.enabled", true);
  Services.prefs.setBoolPref("dom.push.connection.enabled", true);

  trustHttp2CA();

  Services.prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);

  run_next_test();
}

add_task(async function test_pushUnsubscriptionSuccess() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  var serverURL = "https://localhost:" + serverPort;

  await db.put({
    subscriptionUri: serverURL + "/subscriptionUnsubscriptionSuccess",
    pushEndpoint: serverURL + "/pushEndpointUnsubscriptionSuccess",
    pushReceiptEndpoint: serverURL + "/receiptPushEndpointUnsubscriptionSuccess",
    scope: "https://example.com/page/unregister-success",
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { inIsolatedMozBrowser: false }),
    quota: Infinity,
  });

  PushService.init({
    serverURI: serverURL,
    db,
  });

  await PushService.unregister({
    scope: "https://example.com/page/unregister-success",
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { inIsolatedMozBrowser: false }),
  });
  let record = await db.getByKeyID(serverURL + "/subscriptionUnsubscriptionSuccess");
  ok(!record, "Unregister did not remove record");
});

add_task(async function test_complete() {
  Services.prefs.setBoolPref("dom.push.enabled", pushEnabled);
  Services.prefs.setBoolPref("dom.push.connection.enabled", pushConnectionEnabled);
});
