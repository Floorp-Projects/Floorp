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

function resubscribeHandler(metadata, response) {
  ok(true, "Ask for new subscription");
  do_test_finished();
  response.setHeader("Location",
                  'http://localhost:' + serverPort + '/newSubscription')
  response.setHeader("Link",
                  '</newPushEndpoint>; rel="urn:ietf:params:push", ' +
                  '</newReceiptPushEndpoint>; rel="urn:ietf:params:push:receipt"');
  response.setStatusLine(metadata.httpVersion, 201, "OK");
}

function listenSuccessHandler(metadata, response) {
  do_check_true(true, "New listener point");
  httpServer.stop(do_test_finished);
  response.setStatusLine(metadata.httpVersion, 204, "Try again");
}


httpServer = new HttpServer();
httpServer.registerPathHandler("/subscribe", resubscribeHandler);
httpServer.registerPathHandler("/newSubscription", listenSuccessHandler);
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
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  do_test_pending();
  do_test_pending();

  var serverURL = "http://localhost:" + httpServer.identity.primaryPort;

  let records = [{
    subscriptionUri: 'http://localhost/subscriptionNotExist',
    pushEndpoint: serverURL + '/pushEndpoint',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint',
    scope: 'https://example.com/page',
    p256dhPublicKey: 'BPCd4gNQkjwRah61LpdALdzZKLLnU5UAwDztQ5_h0QsT26jk0IFbqcK6-JxhHAm-rsHEwy0CyVJjtnfOcqc1tgA',
    p256dhPrivateKey: {
      crv: 'P-256',
      d: '1jUPhzVsRkzV0vIzwL4ZEsOlKdNOWm7TmaTfzitJkgM',
      ext: true,
      key_ops: ["deriveBits"],
      kty: "EC",
      x: '8J3iA1CSPBFqHrUul0At3NkosudTlQDAPO1Dn-HRCxM',
      y: '26jk0IFbqcK6-JxhHAm-rsHEwy0CyVJjtnfOcqc1tgA'
    },
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

});
