/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var serverURL;

var serverPort = -1;

function run_test() {
  serverPort = getTestServerPort();

  do_get_profile();

  serverURL = "https://localhost:" + serverPort;

  run_next_test();
}

// Connection will fail because of the certificates.
add_task(async function test_pushSubscriptionNoConnection() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionNoConnection/subscribe",
    db,
  });

  await Assert.rejects(
    PushService.register({
      scope: "https://example.net/page/invalid-response",
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { inIsolatedMozBrowser: false }),
    }),
    /Registration error/,
    "Expected error for not being able to establish connecion."
  );

  let record = await db.getAllKeyIDs();
  ok(record.length === 0, "Should not store records when connection couldn't be established.");
  PushService.uninit();
});

add_task(async function test_TLS() {
    // Set to allow the cert presented by our H2 server
  var oldPref = Services.prefs.getIntPref("network.http.speculative-parallel-limit");
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);

  trustHttp2CA();

  Services.prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);
});

add_task(async function test_pushSubscriptionMissingLocation() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionMissingLocation/subscribe",
    db,
  });

  await Assert.rejects(
    PushService.register({
      scope: "https://example.net/page/invalid-response",
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { inIsolatedMozBrowser: false }),
    }),
    /Registration error/,
    "Expected error for the missing location header."
  );

  let record = await db.getAllKeyIDs();
  ok(record.length === 0, "Should not store records when the location header is missing.");
  PushService.uninit();
});

add_task(async function test_pushSubscriptionMissingLink() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionMissingLink/subscribe",
    db,
  });

  await Assert.rejects(
    PushService.register({
      scope: "https://example.net/page/invalid-response",
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { inIsolatedMozBrowser: false }),
    }),
    /Registration error/,
    "Expected error for the missing link header."
  );

  let record = await db.getAllKeyIDs();
  ok(record.length === 0, "Should not store records when a link header is missing.");
  PushService.uninit();
});

add_task(async function test_pushSubscriptionMissingLink1() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionMissingLink1/subscribe",
    db,
  });

  await Assert.rejects(
    PushService.register({
      scope: "https://example.net/page/invalid-response",
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { inIsolatedMozBrowser: false }),
    }),
    /Registration error/,
    "Expected error for the missing push endpoint."
  );

  let record = await db.getAllKeyIDs();
  ok(record.length === 0, "Should not store records when the push endpoint is missing.");
  PushService.uninit();
});

add_task(async function test_pushSubscriptionLocationBogus() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionLocationBogus/subscribe",
    db,
  });

  await Assert.rejects(
    PushService.register({
      scope: "https://example.net/page/invalid-response",
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { inIsolatedMozBrowser: false }),
    }),
    /Registration error/,
    "Expected error for the bogus location"
  );

  let record = await db.getAllKeyIDs();
  ok(record.length === 0, "Should not store records when location header is bogus.");
  PushService.uninit();
});

add_task(async function test_pushSubscriptionNot2xxCode() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionNot201Code/subscribe",
    db,
  });

  await Assert.rejects(
    PushService.register({
      scope: "https://example.net/page/invalid-response",
      originAttributes: ChromeUtils.originAttributesToSuffix(
        { inIsolatedMozBrowser: false }),
    }),
    /Registration error/,
    "Expected error for not 201 responce code."
  );

  let record = await db.getAllKeyIDs();
  ok(record.length === 0, "Should not store records when respons code is not 201.");
});
