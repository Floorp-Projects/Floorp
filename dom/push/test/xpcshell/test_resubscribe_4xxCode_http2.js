/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

var httpServer = null;

ChromeUtils.defineLazyGetter(this, "serverPort", function () {
  return httpServer.identity.primaryPort;
});

var handlerDone;
var handlerPromise = new Promise(r => (handlerDone = after(3, r)));

function listen4xxCodeHandler(metadata, response) {
  ok(true, "Listener point error");
  handlerDone();
  response.setStatusLine(metadata.httpVersion, 410, "GONE");
}

function resubscribeHandler(metadata, response) {
  ok(true, "Ask for new subscription");
  handlerDone();
  response.setHeader(
    "Location",
    "http://localhost:" + serverPort + "/newSubscription"
  );
  response.setHeader(
    "Link",
    '</newPushEndpoint>; rel="urn:ietf:params:push", ' +
      '</newReceiptPushEndpoint>; rel="urn:ietf:params:push:receipt"'
  );
  response.setStatusLine(metadata.httpVersion, 201, "OK");
}

function listenSuccessHandler(metadata, response) {
  Assert.ok(true, "New listener point");
  httpServer.stop(handlerDone);
  response.setStatusLine(metadata.httpVersion, 204, "Try again");
}

httpServer = new HttpServer();
httpServer.registerPathHandler("/subscription4xxCode", listen4xxCodeHandler);
httpServer.registerPathHandler("/subscribe", resubscribeHandler);
httpServer.registerPathHandler("/newSubscription", listenSuccessHandler);
httpServer.start(-1);

function run_test() {
  do_get_profile();

  setPrefs({
    "testing.allowInsecureServerURL": true,
    "testing.notifyWorkers": false,
    "testing.notifyAllObservers": true,
  });

  run_next_test();
}

add_task(async function test1() {
  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
    return db.drop().then(_ => db.close());
  });

  var serverURL = "http://localhost:" + httpServer.identity.primaryPort;

  let records = [
    {
      subscriptionUri: serverURL + "/subscription4xxCode",
      pushEndpoint: serverURL + "/pushEndpoint",
      pushReceiptEndpoint: serverURL + "/pushReceiptEndpoint",
      scope: "https://example.com/page",
      originAttributes: "",
      quota: Infinity,
    },
  ];

  for (let record of records) {
    await db.put(record);
  }

  PushService.init({
    serverURI: serverURL + "/subscribe",
    db,
  });

  await handlerPromise;

  let record = await db.getByIdentifiers({
    scope: "https://example.com/page",
    originAttributes: "",
  });
  equal(
    record.keyID,
    serverURL + "/newSubscription",
    "Should update subscription URL"
  );
  equal(
    record.pushEndpoint,
    serverURL + "/newPushEndpoint",
    "Should update push endpoint"
  );
  equal(
    record.pushReceiptEndpoint,
    serverURL + "/newReceiptPushEndpoint",
    "Should update push receipt endpoint"
  );
});
