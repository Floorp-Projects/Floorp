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
  serverPort = env.get("MOZHTTP2_PORT");
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

  // /pushNotifications/subscription1 will send a message with no rs and padding
  // length 1.
  // /pushNotifications/subscription2 will send a message with no rs and padding
  // length 16.
  // /pushNotifications/subscription3 will send a message with rs equal 24 and
  // padding length 16.

  let db = PushServiceHttp2.newPushDB();
  do_register_cleanup(() => {
    return db.drop().then(_ => db.close());
  });

  var serverURL = "https://localhost:" + serverPort;

  let records = [{
    subscriptionUri: serverURL + '/pushNotifications/subscription1',
    pushEndpoint: serverURL + '/pushEndpoint1',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint1',
    scope: 'https://example.com/page/1',
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
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
    quota: Infinity,
  }, {
    subscriptionUri: serverURL + '/pushNotifications/subscription2',
    pushEndpoint: serverURL + '/pushEndpoint2',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint2',
    scope: 'https://example.com/page/2',
    p256dhPublicKey: 'BPnWyUo7yMnuMlyKtERuLfWE8a09dtdjHSW2lpC9_BqR5TZ1rK8Ldih6ljyxVwnBA-nygQHGRpEmu1jV5K8437E',
    p256dhPrivateKey: {
      crv: 'P-256',
      d: 'lFm4nPsUKYgNGBJb5nXXKxl8bspCSp0bAhCYxbveqT4',
      ext: true,
      key_ops: ["deriveBits"],
      kty: 'EC',
      x: '-dbJSjvIye4yXIq0RG4t9YTxrT1212MdJbaWkL38GpE',
      y: '5TZ1rK8Ldih6ljyxVwnBA-nygQHGRpEmu1jV5K8437E'
    },
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
    quota: Infinity,
  }, {
    subscriptionUri: serverURL + '/pushNotifications/subscription3',
    pushEndpoint: serverURL + '/pushEndpoint3',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint3',
    scope: 'https://example.com/page/3',
    p256dhPublicKey: 'BDhUHITSeVrWYybFnb7ylVTCDDLPdQWMpf8gXhcWwvaaJa6n3YH8TOcH8narDF6t8mKVvg2ioLW-8MH5O4dzGcI',
    p256dhPrivateKey: {
      crv: 'P-256',
      d: 'Q1_SE1NySTYzjbqgWwPgrYh7XRg3adqZLkQPsy319G8',
      ext: true,
      key_ops: ["deriveBits"],
      kty: 'EC',
      x: 'OFQchNJ5WtZjJsWdvvKVVMIMMs91BYyl_yBeFxbC9po',
      y: 'Ja6n3YH8TOcH8narDF6t8mKVvg2ioLW-8MH5O4dzGcI'
    },
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inBrowser: false }),
    quota: Infinity,
  }];

  for (let record of records) {
    yield db.put(record);
  }

  let notifyPromise = Promise.all([
    promiseObserverNotification('push-notification', function(subject, data) {
      var notification = subject.QueryInterface(Ci.nsIPushObserverNotification);
      if (notification && (data == "https://example.com/page/1")){
        equal(subject.data, "Some message", "decoded message is incorrect");
        return true;
      }
    }),
    promiseObserverNotification('push-notification', function(subject, data) {
      var notification = subject.QueryInterface(Ci.nsIPushObserverNotification);
      if (notification && (data == "https://example.com/page/2")){
        equal(subject.data, "Some message", "decoded message is incorrect");
        return true;
      }
    }),
    promiseObserverNotification('push-notification', function(subject, data) {
      var notification = subject.QueryInterface(Ci.nsIPushObserverNotification);
      if (notification && (data == "https://example.com/page/3")){
        equal(subject.data, "Some message", "decoded message is incorrect");
        return true;
      }
    })
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
