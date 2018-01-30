/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

'use strict';

Cu.import("resource://gre/modules/Services.jsm");

const {PushDB, PushService, PushServiceHttp2} = serviceExports;

var prefs;

var serverPort = -1;

function run_test() {
  serverPort = getTestServerPort();

  do_get_profile();
  setPrefs({
    'testing.allowInsecureServerURL': true,
  });
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  // Set to allow the cert presented by our H2 server
  var oldPref = prefs.getIntPref("network.http.speculative-parallel-limit");
  prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  prefs.setBoolPref("dom.push.enabled", true);
  prefs.setBoolPref("dom.push.connection.enabled", true);

  addCertOverride("localhost", serverPort,
                  Ci.nsICertOverrideService.ERROR_UNTRUSTED |
                  Ci.nsICertOverrideService.ERROR_MISMATCH |
                  Ci.nsICertOverrideService.ERROR_TIME);

  prefs.setIntPref("network.http.speculative-parallel-limit", oldPref);

  run_next_test();
}

add_task(async function test_pushNotifications() {

  // /pushNotifications/subscription1 will send a message with no rs and padding
  // length 1.
  // /pushNotifications/subscription2 will send a message with no rs and padding
  // length 16.
  // /pushNotifications/subscription3 will send a message with rs equal 24 and
  // padding length 16.
  // /pushNotifications/subscription4 will send a message with no rs and padding
  // length 256.

  let db = PushServiceHttp2.newPushDB();
  registerCleanupFunction(() => {
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
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    quota: Infinity,
    systemRecord: true,
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
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    quota: Infinity,
    systemRecord: true,
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
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    quota: Infinity,
    systemRecord: true,
  }, {
    subscriptionUri: serverURL + '/pushNotifications/subscription4',
    pushEndpoint: serverURL + '/pushEndpoint4',
    pushReceiptEndpoint: serverURL + '/pushReceiptEndpoint4',
    scope: 'https://example.com/page/4',
    p256dhPublicKey: ChromeUtils.base64URLDecode('BEcvDzkWCrUtjU_wygL98sbQCQrW1lY9irtgGnlCc4B0JJXLCHB9MTM73qD6GZYfL0YOvKo8XLOflh-J4dMGklU', {
      padding: "reject",
    }),
    p256dhPrivateKey: {
      crv: 'P-256',
      d: 'fWi7tZaX0Pk6WnLrjQ3kiRq_g5XStL5pdH4pllNCqXw',
      ext: true,
      key_ops: ["deriveBits"],
      kty: 'EC',
      x: 'Ry8PORYKtS2NT_DKAv3yxtAJCtbWVj2Ku2AaeUJzgHQ',
      y: 'JJXLCHB9MTM73qD6GZYfL0YOvKo8XLOflh-J4dMGklU'
    },
    authenticationSecret: ChromeUtils.base64URLDecode('cwDVC1iwAn8E37mkR3tMSg', {
      padding: "reject",
    }),
    originAttributes: ChromeUtils.originAttributesToSuffix(
      { appId: Ci.nsIScriptSecurityManager.NO_APP_ID, inIsolatedMozBrowser: false }),
    quota: Infinity,
    systemRecord: true,
  }];

  for (let record of records) {
    await db.put(record);
  }

  let notifyPromise = Promise.all([
    promiseObserverNotification(PushServiceComponent.pushTopic, function(subject, data) {
      var message = subject.QueryInterface(Ci.nsIPushMessage).data;
      if (message && (data == "https://example.com/page/1")){
        equal(message.text(), "Some message", "decoded message is incorrect");
        return true;
      }
    }),
    promiseObserverNotification(PushServiceComponent.pushTopic, function(subject, data) {
      var message = subject.QueryInterface(Ci.nsIPushMessage).data;
      if (message && (data == "https://example.com/page/2")){
        equal(message.text(), "Some message", "decoded message is incorrect");
        return true;
      }
    }),
    promiseObserverNotification(PushServiceComponent.pushTopic, function(subject, data) {
      var message = subject.QueryInterface(Ci.nsIPushMessage).data;
      if (message && (data == "https://example.com/page/3")){
        equal(message.text(), "Some message", "decoded message is incorrect");
        return true;
      }
    }),
    promiseObserverNotification(PushServiceComponent.pushTopic, function(subject, data) {
      var message = subject.QueryInterface(Ci.nsIPushMessage).data;
      if (message && (data == "https://example.com/page/4")){
        equal(message.text(), "Yet another message", "decoded message is incorrect");
        return true;
      }
    }),
  ]);

  PushService.init({
    serverURI: serverURL,
    db
  });

  await notifyPromise;
});
