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

function listenHandler(metadata, response) {
  do_check_true(true, "Start listening");
  httpServer.stop(do_test_finished);
  response.setHeader("Retry-After", "10");
  response.setStatusLine(metadata.httpVersion, 500, "Retry");
}

httpServer = new HttpServer();
httpServer.registerPathHandler("/subscriptionNoKey", listenHandler);
httpServer.start(-1);

function run_test() {

  do_get_profile();
  setPrefs({
    'http2.retryInterval': 1000,
    'http2.maxRetries': 2
  });
  disableServiceWorkerEvents(
    'https://example.com/page'
  );

  run_next_test();
}

add_task(function* test1() {

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(_ => {
    return db.drop().then(_ => db.close());
  });

  do_test_pending();

  var serverURL = "http://localhost:" + httpServer.identity.primaryPort;

  let record = {
    subscriptionUri: serverURL + '/subscriptionNoKey',
    pushEndpoint: serverURL + '/pushEndpoint',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint',
    scope: 'https://example.com/page',
    originAttributes: '',
    quota: Infinity,
  };

  yield db.put(record);

  let notifyPromise = promiseObserverNotification('push-subscription-change',
                                                  _ => true);

  PushService.init({
    serverURI: serverURL + "/subscribe",
    service: PushServiceHttp2,
    db
  });

  yield waitForPromise(notifyPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for notifications');

  let aRecord = yield db.getByKeyID(serverURL + '/subscriptionNoKey');
  ok(aRecord, 'The record should still be there');
  ok(aRecord.p256dhPublicKey, 'There should be a public key');
  ok(aRecord.p256dhPrivateKey, 'There should be a private key');
});
