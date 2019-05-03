/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var httpServer = null;

XPCOMUtils.defineLazyGetter(this, "serverPort", function() {
  return httpServer.identity.primaryPort;
});

var retries = 0;

function subscribe5xxCodeHandler(metadata, response) {
  if (retries == 0) {
    ok(true, "Subscribe 5xx code");
    do_test_finished();
    response.setHeader("Retry-After", "1");
    response.setStatusLine(metadata.httpVersion, 500, "Retry");
  } else {
    ok(true, "Subscribed");
    do_test_finished();
    response.setHeader("Location",
                       "http://localhost:" + serverPort + "/subscription");
    response.setHeader("Link",
                       '</pushEndpoint>; rel="urn:ietf:params:push", ' +
                       '</receiptPushEndpoint>; rel="urn:ietf:params:push:receipt"');
    response.setStatusLine(metadata.httpVersion, 201, "OK");
  }
  retries++;
}

function listenSuccessHandler(metadata, response) {
  Assert.ok(true, "New listener point");
  ok(retries == 2, "Should try 2 times.");
  do_test_finished();
  response.setHeader("Retry-After", "10");
  response.setStatusLine(metadata.httpVersion, 500, "Retry");
}


httpServer = new HttpServer();
httpServer.registerPathHandler("/subscribe5xxCode", subscribe5xxCodeHandler);
httpServer.registerPathHandler("/subscription", listenSuccessHandler);
httpServer.start(-1);

function run_test() {
  do_get_profile();
  setPrefs({
    "testing.allowInsecureServerURL": true,
    "http2.retryInterval": 1000,
    "http2.maxRetries": 2,
  });

  run_next_test();
}

add_task(async function test1() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();

  var serverURL = "http://localhost:" + httpServer.identity.primaryPort;

  PushService.init({
    serverURI: serverURL + "/subscribe5xxCode",
    db,
  });

  let originAttributes = ChromeUtils.originAttributesToSuffix({
    inIsolatedMozBrowser: false,
  });
  let newRecord = await PushService.register({
    scope: "https://example.com/retry5xxCode",
    originAttributes,
  });

  var subscriptionUri = serverURL + "/subscription";
  var pushEndpoint = serverURL + "/pushEndpoint";
  var pushReceiptEndpoint = serverURL + "/receiptPushEndpoint";
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
  equal(record.scope, "https://example.com/retry5xxCode",
    "Wrong scope in database record");

  httpServer.stop(do_test_finished);
});
