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

var serverPort = -1;

function run_test() {
  var env = Cc["@mozilla.org/process/environment;1"].getService(Ci.nsIEnvironment);
  serverPort = env.get("MOZHTTP2_PORT");
  do_check_neq(serverPort, null);

  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  run_next_test();
}

add_task(function* test_pushNotifications() {

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  var serverURL = "https://localhost:" + serverPort;

  let records = [{
    subscriptionUri: serverURL + '/subscriptionA',
    pushEndpoint: serverURL + '/pushEndpointA',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpointA',
    scope: 'https://example.net/a',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    quota: Infinity,
  }, {
    subscriptionUri: serverURL + '/subscriptionB',
    pushEndpoint: serverURL + '/pushEndpointB',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpointB',
    scope: 'https://example.net/b',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    quota: Infinity,
  }, {
    subscriptionUri: serverURL + '/subscriptionC',
    pushEndpoint: serverURL + '/pushEndpointC',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpointC',
    scope: 'https://example.net/c',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    quota: Infinity,
  }];

  for (let record of records) {
    yield db.put(record);
  }

  PushService.init({
    serverURI: serverURL,
    db
  });

  let registration = yield PushService.registration({
    scope: 'https://example.net/a',
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
  });
  equal(
    registration.endpoint,
    serverURL + '/pushEndpointA',
    'Wrong push endpoint for scope'
  );
});
