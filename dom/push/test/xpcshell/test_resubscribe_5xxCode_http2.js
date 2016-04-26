/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/httpd.js");

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var httpServer = null;

XPCOMUtils.defineLazyGetter(this, "serverPort", function() {
  return httpServer.identity.primaryPort;
});

var retries = 0;
var handlerDone;
var handlerPromise = new Promise(r => handlerDone = after(5, r));

function listen5xxCodeHandler(metadata, response) {
  ok(true, "Listener 5xx code");
  handlerDone();
  retries++;
  response.setHeader("Retry-After", '1');
  response.setStatusLine(metadata.httpVersion, 500, "Retry");
}

function resubscribeHandler(metadata, response) {
  ok(true, "Ask for new subscription");
  ok(retries == 3, "Should retry 2 times.");
  handlerDone();
  response.setHeader("Location",
                  'http://localhost:' + serverPort + '/newSubscription')
  response.setHeader("Link",
                  '</newPushEndpoint>; rel="urn:ietf:params:push", ' +
                  '</newReceiptPushEndpoint>; rel="urn:ietf:params:push:receipt"');
  response.setStatusLine(metadata.httpVersion, 201, "OK");
}

function listenSuccessHandler(metadata, response) {
  do_check_true(true, "New listener point");
  httpServer.stop(handlerDone);
  response.setStatusLine(metadata.httpVersion, 204, "Try again");
}


httpServer = new HttpServer();
httpServer.registerPathHandler("/subscription5xxCode", listen5xxCodeHandler);
httpServer.registerPathHandler("/subscribe", resubscribeHandler);
httpServer.registerPathHandler("/newSubscription", listenSuccessHandler);
httpServer.start(-1);

function run_test() {

  do_get_profile();
  setPrefs({
    'testing.allowInsecureServerURL': true,
    'http2.retryInterval': 1000,
    'http2.maxRetries': 2
  });

  run_next_test();
}

add_task(function* test1() {

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  var serverURL = "http://localhost:" + httpServer.identity.primaryPort;

  let records = [{
    subscriptionUri: serverURL + '/subscription5xxCode',
    pushEndpoint: serverURL + '/pushEndpoint',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint',
    scope: 'https://example.com/page',
    originAttributes: '',
    quota: Infinity,
  }];

  for (let record of records) {
    yield db.put(record);
  }

  PushService.init({
    serverURI: serverURL + "/subscribe",
    db
  });

  yield handlerPromise;

  let record = yield db.getByIdentifiers({
    scope: 'https://example.com/page',
    originAttributes: '',
  });
  equal(record.keyID, serverURL + '/newSubscription',
    'Should update subscription URL');
  equal(record.pushEndpoint, serverURL + '/newPushEndpoint',
    'Should update push endpoint');
  equal(record.pushReceiptEndpoint, serverURL + '/newReceiptPushEndpoint',
    'Should update push receipt endpoint');

});
