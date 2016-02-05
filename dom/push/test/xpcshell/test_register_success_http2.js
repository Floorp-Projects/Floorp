/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

Cu.import("resource://gre/modules/Services.jsm");

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var prefs;
var tlsProfile;
var serverURL;
var serverPort = -1;
var pushEnabled;
var pushConnectionEnabled;
var db;

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  serverPort = env.get("MOZHTTP2_PORT");
  do_check_neq(serverPort, null);

  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  tlsProfile = prefs.getBoolPref("network.http.spdy.enforce-tls-profile");
  pushEnabled = prefs.getBoolPref("dom.push.enabled");
  pushConnectionEnabled = prefs.getBoolPref("dom.push.connection.enabled");

  // Set to allow the cert presented by our H2 server
  var oldPref = prefs.getIntPref("network.http.speculative-parallel-limit");
  prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", false);
  prefs.setBoolPref("dom.push.enabled", true);
  prefs.setBoolPref("dom.push.connection.enabled", true);

  addCertOverride("localhost", serverPort,
                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                  Ci.nsICertOverrideService.ERROR_TIME);

  prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);

  serverURL = "https://localhost:" + serverPort;

  run_next_test();
}

add_task(function* test_setup() {

  db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

});

add_task(function* test_pushSubscriptionSuccess() {

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionSuccess/subscribe",
    db
  });

  let newRecord = yield PushService.register({
    scope: 'https://example.org/1',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
  });

  var subscriptionUri = serverURL + '/pushSubscriptionSuccesss';
  var pushEndpoint = serverURL + '/pushEndpointSuccess';
  var pushReceiptEndpoint = serverURL + '/receiptPushEndpointSuccess';
  equal(newRecord.endpoint, pushEndpoint,
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
  equal(record.scope, 'https://example.org/1',
    'Wrong scope in database record');

  PushService.uninit()
});

add_task(function* test_pushSubscriptionMissingLink2() {

  PushService.init({
    serverURI: serverURL + "/pushSubscriptionMissingLink2/subscribe",
    db
  });

  let newRecord = yield PushService.register({
    scope: 'https://example.org/no_receiptEndpoint',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
  });

  var subscriptionUri = serverURL + '/subscriptionMissingLink2';
  var pushEndpoint = serverURL + '/pushEndpointMissingLink2';
  var pushReceiptEndpoint = '';
  equal(newRecord.endpoint, pushEndpoint,
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
  equal(record.scope, 'https://example.org/no_receiptEndpoint',
    'Wrong scope in database record');
});

add_task(function* test_complete() {
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", tlsProfile);
  prefs.setBoolPref("dom.push.enabled", pushEnabled);
  prefs.setBoolPref("dom.push.connection.enabled", pushConnectionEnabled);
});
