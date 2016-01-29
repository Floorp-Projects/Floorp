/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://testing-common/PromiseTestUtils.jsm");

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
// Instances of the rejection "record is undefined" may or may not appear.
PromiseTestUtils.thisTestLeaksUncaughtRejectionsAndShouldBeFixed();

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var prefs;
var tlsProfile;

var serverPort = -1;

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  serverPort = env.get("MOZHTTP2_PORT");
  do_check_neq(serverPort, null);

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

  run_next_test();
}

add_task(function* test_pushUnsubscriptionSuccess() {
  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  var serverURL = "https://localhost:" + serverPort;

  yield db.put({
    subscriptionUri: serverURL + '/subscriptionUnsubscriptionSuccess',
    pushEndpoint: serverURL + '/pushEndpointUnsubscriptionSuccess',
    pushReceiptEndpoint: serverURL + '/receiptPushEndpointUnsubscriptionSuccess',
    scope: 'https://example.com/page/unregister-success',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
    quota: Infinity,
  });

  PushService.init({
    serverURI: serverURL,
    db
  });

  yield PushService.unregister({
    scope: 'https://example.com/page/unregister-success',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
  });
  let record = yield db.getByKeyID(serverURL + '/subscriptionUnsubscriptionSuccess');
  ok(!record, 'Unregister did not remove record');

});

add_task(function* test_complete() {
  prefs.setBoolPref("network.http.spdy.enforce-tls-profile", tlsProfile);
});
