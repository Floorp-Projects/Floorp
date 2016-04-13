/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const {PushDB} = Cu.import("resource://gre/modules/PushDB.jsm");
const {PushRecord} = Cu.import("resource://gre/modules/PushRecord.jsm");
Cu.import("resource://gre/modules/Messaging.jsm"); /*global: Services */
Cu.import("resource://gre/modules/Services.jsm"); /*global: Services */
Cu.import("resource://gre/modules/Preferences.jsm"); /*global: Preferences */
Cu.import("resource://gre/modules/Promise.jsm"); /*global: Promise */
Cu.import("resource://gre/modules/XPCOMUtils.jsm"); /*global: XPCOMUtils */

const Log = Cu.import("resource://gre/modules/AndroidLog.jsm", {}).AndroidLog.bind("Push");

const {
  PushCrypto,
  base64UrlDecode,
  concatArray,
  getCryptoParams,
} = Cu.import("resource://gre/modules/PushCrypto.jsm");

this.EXPORTED_SYMBOLS = ["PushServiceAndroidGCM"];

XPCOMUtils.defineLazyGetter(this, "console", () => {
  let {ConsoleAPI} = Cu.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    dump: Log.i,
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "PushServiceAndroidGCM",
  });
});

const kPUSHANDROIDGCMDB_DB_NAME = "pushAndroidGCM";
const kPUSHANDROIDGCMDB_DB_VERSION = 5; // Change this if the IndexedDB format changes
const kPUSHANDROIDGCMDB_STORE_NAME = "pushAndroidGCM";

const prefs = new Preferences("dom.push.");

/**
 * The implementation of WebPush push backed by Android's GCM
 * delivery.
 */
this.PushServiceAndroidGCM = {
  _mainPushService: null,
  _serverURI: null,

  newPushDB: function() {
    return new PushDB(kPUSHANDROIDGCMDB_DB_NAME,
                      kPUSHANDROIDGCMDB_DB_VERSION,
                      kPUSHANDROIDGCMDB_STORE_NAME,
                      "channelID",
                      PushRecordAndroidGCM);
  },

  serviceType: function() {
    return "AndroidGCM";
  },

  validServerURI: function(serverURI) {
    if (!serverURI) {
      return false;
    }

    if (serverURI.scheme == "https") {
      return true;
    }
    if (prefs.get("debug") && serverURI.scheme == "http") {
      // Accept HTTP endpoints when debugging.
      return true;
    }
    console.info("Unsupported Android GCM dom.push.serverURL scheme", serverURI.scheme);
    return false;
  },

  observe: function(subject, topic, data) {
    if (topic == "nsPref:changed") {
      if (data == "dom.push.debug") {
        // Reconfigure.
        let debug = !!prefs.get("debug");
        console.info("Debug parameter changed; updating configuration with new debug", debug);
        this._configure(this._serverURI, debug);
      }
      return;
    }

    if (topic == "PushServiceAndroidGCM:ReceivedPushMessage") {
      // TODO: Use Messaging.jsm for this.
      if (this._mainPushService == null) {
        // Shouldn't ever happen, but let's be careful.
        console.error("No main PushService!  Dropping message.");
        return;
      }
      if (!data) {
        console.error("No data from Java!  Dropping message.");
        return;
      }
      data = JSON.parse(data);
      console.debug("ReceivedPushMessage with data", data);

      // Default is no data (and no encryption).
      let message = null;
      let cryptoParams = null;

      if (data.message && data.enc && (data.enckey || data.cryptokey)) {
        let headers = {
          encryption_key: data.enckey,
          crypto_key: data.cryptokey,
          encryption: data.enc,
          encoding: data.con,
        };
        cryptoParams = getCryptoParams(headers);
        // Ciphertext is (urlsafe) Base 64 encoded.
        message = base64UrlDecode(data.message);
      }

      console.debug("Delivering message to main PushService:", message, cryptoParams);
      this._mainPushService.receivedPushMessage(
        data.channelID, message, cryptoParams, (record) => {
          // Always update the stored record.
          return record;
        });
      return;
    }
  },

  _configure: function(serverURL, debug) {
    return Messaging.sendRequestForResult({
      type: "PushServiceAndroidGCM:Configure",
      endpoint: serverURL.spec,
      debug: debug,
    });
  },

  init: function(options, mainPushService, serverURL) {
    console.debug("init()");
    this._mainPushService = mainPushService;
    this._serverURI = serverURL;

    prefs.observe("debug", this);
    Services.obs.addObserver(this, "PushServiceAndroidGCM:ReceivedPushMessage", false);

    return this._configure(serverURL, !!prefs.get("debug")).then(() => {
      Messaging.sendRequestForResult({
        type: "PushServiceAndroidGCM:Initialized"
      });
    });
  },

  uninit: function() {
    console.debug("uninit()");
    Messaging.sendRequestForResult({
      type: "PushServiceAndroidGCM:Uninitialized"
    });

    this._mainPushService = null;
    Services.obs.removeObserver(this, "PushServiceAndroidGCM:ReceivedPushMessage");
    prefs.ignore("debug", this);
  },

  onAlarmFired: function() {
    // No action required.
  },

  connect: function(records) {
    console.debug("connect:", records);
    // It's possible for the registration or subscriptions backing the
    // PushService to not be registered with the underlying AndroidPushService.
    // Expire those that are unrecognized.
    return Messaging.sendRequestForResult({
      type: "PushServiceAndroidGCM:DumpSubscriptions",
    })
    .then(subscriptions => {
      console.debug("connect:", subscriptions);
      // subscriptions maps chid => subscription data.
      return Promise.all(records.map(record => {
        if (subscriptions.hasOwnProperty(record.keyID)) {
          console.debug("connect:", "hasOwnProperty", record.keyID);
          return Promise.resolve();
        }
        console.debug("connect:", "!hasOwnProperty", record.keyID);
        // Subscription is known to PushService.jsm but not to AndroidPushService.  Drop it.
        return this._mainPushService.dropRegistrationAndNotifyApp(record.keyID)
          .catch(error => {
            console.error("connect: Error dropping registration", record.keyID, error);
          });
      }));
    });
  },

  isConnected: function() {
    return this._mainPushService != null;
  },

  disconnect: function() {
    console.debug("disconnect");
  },

  register: function(record) {
    console.debug("register:", record);
    let ctime = Date.now();
    // Caller handles errors.
    return Messaging.sendRequestForResult({
      type: "PushServiceAndroidGCM:SubscribeChannel",
    }).then(data => {
      console.debug("Got data:", data);
      return PushCrypto.generateKeys()
        .then(exportedKeys =>
          new PushRecordAndroidGCM({
            // Straight from autopush.
            channelID: data.channelID,
            pushEndpoint: data.endpoint,
            // Common to all PushRecord implementations.
            scope: record.scope,
            originAttributes: record.originAttributes,
            ctime: ctime,
            // Cryptography!
            p256dhPublicKey: exportedKeys[0],
            p256dhPrivateKey: exportedKeys[1],
            authenticationSecret: PushCrypto.generateAuthenticationSecret(),
          })
      );
    });
  },

  unregister: function(record) {
    console.debug("unregister: ", record);
    return Messaging.sendRequestForResult({
      type: "PushServiceAndroidGCM:UnsubscribeChannel",
      channelID: record.keyID,
    });
  },
};

function PushRecordAndroidGCM(record) {
  PushRecord.call(this, record);
  this.channelID = record.channelID;
}

PushRecordAndroidGCM.prototype = Object.create(PushRecord.prototype, {
  keyID: {
    get() {
      return this.channelID;
    },
  },
});
