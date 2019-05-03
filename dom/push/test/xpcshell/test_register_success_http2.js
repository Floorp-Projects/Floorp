/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var serverURL;
var serverPort = -1;
var pushEnabled;
var pushConnectionEnabled;
var db;

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

  serverURL = "https://localhost:" + serverPort;

  run_next_test();
}

add_task(async function test_setup() {
  db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });
});

add_task(async function test_pushSubscriptionSuccess() {
  PushService.init({
    serverURI: serverURL + "/pushSubscriptionSuccess/subscribe",
    db,
  });

  let newRecord = await PushService.register({
    scope: "https://example.org/1",
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { inIsolatedMozBrowser: false }),
  });

  var subscriptionUri = serverURL + "/pushSubscriptionSuccesss";
  var pushEndpoint = serverURL + "/pushEndpointSuccess";
  var pushReceiptEndpoint = serverURL + "/receiptPushEndpointSuccess";
  equal(newRecord.endpoint, pushEndpoint,
    "Wrong push endpoint in registration record");

  equal(newRecord.pushReceiptEndpoint, pushReceiptEndpoint,
    "Wrong push endpoint receipt in registration record");

  let record = await db.getByKeyID(subscriptionUri);
  equal(record.subscriptionUri, subscriptionUri,
    "Wrong subscription ID in database record");
  equal(record.pushEndpoint, pushEndpoint,
    "Wrong push endpoint in database record");
  equal(record.pushReceiptEndpoint, pushReceiptEndpoint,
    "Wrong push endpoint receipt in database record");
  equal(record.scope, "https://example.org/1",
    "Wrong scope in database record");

  PushService.uninit();
});

add_task(async function test_pushSubscriptionMissingLink2() {
  PushService.init({
    serverURI: serverURL + "/pushSubscriptionMissingLink2/subscribe",
    db,
  });

  let newRecord = await PushService.register({
    scope: "https://example.org/no_receiptEndpoint",
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { inIsolatedMozBrowser: false }),
  });

  var subscriptionUri = serverURL + "/subscriptionMissingLink2";
  var pushEndpoint = serverURL + "/pushEndpointMissingLink2";
  var pushReceiptEndpoint = "";
  equal(newRecord.endpoint, pushEndpoint,
    "Wrong push endpoint in registration record");

  equal(newRecord.pushReceiptEndpoint, pushReceiptEndpoint,
    "Wrong push endpoint receipt in registration record");

  let record = await db.getByKeyID(subscriptionUri);
  equal(record.subscriptionUri, subscriptionUri,
    "Wrong subscription ID in database record");
  equal(record.pushEndpoint, pushEndpoint,
    "Wrong push endpoint in database record");
  equal(record.pushReceiptEndpoint, pushReceiptEndpoint,
    "Wrong push endpoint receipt in database record");
  equal(record.scope, "https://example.org/no_receiptEndpoint",
    "Wrong scope in database record");
});

add_task(async function test_complete() {
  Services.prefs.setBoolPref("dom.push.enabled", pushEnabled);
  Services.prefs.setBoolPref("dom.push.connection.enabled", pushConnectionEnabled);
});
