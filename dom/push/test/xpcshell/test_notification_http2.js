/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

Cu.import("resource://gre/modules/Services.jsm");

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var prefs;
var tlsProfile;

var serverPort = -1;

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  serverPort = env.get("MOZHTTP2-PORT");
  do_check_neq(serverPort, null);
  dump("using port " + serverPort + "\n");

  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  tlsProfile = prefs.getBoolPref("network.http.spdy.enforce-tls-profile");

  // Set to allow the cert presented by our H2 server
  var oldPref = prefs.getIntPref("network.http.speculative-parallel-limit");
  prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", false);

  addCertOverride("localhost", serverPort,
                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                  Ci.nsICertOverrideService.ERROR_TIME);

  prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);

  disableServiceWorkerEvents(
    'https://example.com/page/1',
    'https://example.com/page/2',
    'https://example.com/page/3'
  );

  run_next_test();
}

add_task(function* test_pushNotifications() {

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  var serverURL = "https://localhost:" + serverPort;

  let records = [{
    subscriptionUri: serverURL + '/pushNotifications/subscription1',
    pushEndpoint: serverURL + '/pushEndpoint1',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint1',
    scope: 'https://example.com/page/1'
  }, {
    subscriptionUri: serverURL + '/pushNotifications/subscription2',
    pushEndpoint: serverURL + '/pushEndpoint2',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint2',
    scope: 'https://example.com/page/2'
  }, {
    subscriptionUri: serverURL + '/pushNotifications/subscription3',
    pushEndpoint: serverURL + '/pushEndpoint3',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint3',
    scope: 'https://example.com/page/3'
  }];

  for (let record of records) {
    yield db.put(record);
  }

  let notifyPromise = Promise.all([
    promiseObserverNotification('push-notification'),
    promiseObserverNotification('push-notification'),
    promiseObserverNotification('push-notification')
  ]);

  PushService.init({
    serverURI: serverURL,
    db
  });

  yield waitForPromise(notifyPromise, DEFAULT_TIMEOUT,
    'Timed out waiting for notifications');
});

add_task(function* test_complete() {
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", tlsProfile);
});
