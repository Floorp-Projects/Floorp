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

var retries = 0

function subscribe5xxCodeHandler(metadata, response) {
  if (retries == 0) {
    ok(true, "Subscribe 5xx code");
    do_test_finished();
    response.setHeader("Retry-After", '1');
    response.setStatusLine(metadata.httpVersion, 500, "Retry");
  } else {
    ok(true, "Subscribed");
    do_test_finished();
    response.setHeader("Location",
                       'http://localhost:' + serverPort + '/subscription')
    response.setHeader("Link",
                       '</pushEndpoint>; rel="urn:ietf:params:push", ' +
                       '</receiptPushEndpoint>; rel="urn:ietf:params:push:receipt"');
    response.setStatusLine(metadata.httpVersion, 201, "OK");
  }
  retries++;
}

function listenSuccessHandler(metadata, response) {
  do_check_true(true, "New listener point");
  ok(retries == 2, "Should try 2 times.");
  do_test_finished();
  response.setHeader("Retry-After", '10');
  response.setStatusLine(metadata.httpVersion, 500, "Retry");
}


httpServer = new HttpServer();
httpServer.registerPathHandler("/subscribe5xxCode", subscribe5xxCodeHandler);
httpServer.registerPathHandler("/subscription", listenSuccessHandler);
httpServer.start(-1);

function run_test() {

  do_get_profile();
  setPrefs({
    'http2.retryInterval': 1000,
    'http2.maxRetries': 2
  });
  disableServiceWorkerEvents(
    'https://example.com/retry5xxCode'
  );

  run_next_test();
}

add_task(function* test1() {

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  do_test_pending();
  do_test_pending();
  do_test_pending();
  do_test_pending();

  var serverURL = "http://localhost:" + httpServer.identity.primaryPort;

  PushService.init({
    serverURI: serverURL + "/subscribe5xxCode",
    db
  });

  let newRecord = yield PushNotificationService.register(
    'https://example.com/retry5xxCode',
    ChromeUtils.originAttributesToSuffix({ appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false })
  );

  var subscriptionUri = serverURL + '/subscription';
  var pushEndpoint = serverURL + '/pushEndpoint';
  var pushReceiptEndpoint = serverURL + '/receiptPushEndpoint';
  equal(newRecord.pushEndpoint, pushEndpoint,
    'Wrong push endpoint in registration record');

  equal(newRecord.pushReceiptEndpoint, pushReceiptEndpoint,
    'Wrong push endpoint receipt in registration record');

  let record = yield db.getByKeyID(subscriptionUri);
  equal(record.subscriptionUri, subscriptionUri,
    'Wrong subscription ID in database record');
  equal(record.pushEndpoint, pushEndpoint,
    'Wrong push endpoint in database record');
  equal(record.pushReceiptEndpoint, pushReceiptEndpoint,
    'Wrong push endpoint receipt in database record');
  equal(record.scope, 'https://example.com/retry5xxCode',
    'Wrong scope in database record');

  httpServer.stop(do_test_finished);
});
