/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "PushDB",
  "resource://gre/modules/PushDB.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PushRecord",
  "resource://gre/modules/PushRecord.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "PushCrypto",
  "resource://gre/modules/PushCrypto.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "EventDispatcher",
  "resource://gre/modules/Messaging.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

XPCOMUtils.defineLazyGetter(this, "Log", () => {
  return ChromeUtils.import(
    "resource://gre/modules/AndroidLog.jsm",
    {}
  ).AndroidLog.bind("Push");
});

const EXPORTED_SYMBOLS = ["PushServiceAndroidGCM"];

XPCOMUtils.defineLazyGetter(this, "console", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    dump: Log.i,
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "PushServiceAndroidGCM",
  });
});

const kPUSHANDROIDGCMDB_DB_NAME = "pushAndroidGCM";
const kPUSHANDROIDGCMDB_DB_VERSION = 5; // Change this if the IndexedDB format changes
const kPUSHANDROIDGCMDB_STORE_NAME = "pushAndroidGCM";

const FXA_PUSH_SCOPE = "chrome://fxa-push";

const prefs = new Preferences("dom.push.");

/**
 * The implementation of WebPush push backed by Android's GCM
 * delivery.
 */
var PushServiceAndroidGCM = {
  _mainPushService: null,
  _serverURI: null,

  newPushDB() {
    return new PushDB(
      kPUSHANDROIDGCMDB_DB_NAME,
      kPUSHANDROIDGCMDB_DB_VERSION,
      kPUSHANDROIDGCMDB_STORE_NAME,
      "channelID",
      PushRecordAndroidGCM
    );
  },

  validServerURI(serverURI) {
    if (!serverURI) {
      return false;
    }

    if (serverURI.scheme == "https") {
      return true;
    }
    if (serverURI.scheme == "http") {
      // Allow insecure server URLs for development and testing.
      return !!prefs.get("testing.allowInsecureServerURL");
    }
    console.info(
      "Unsupported Android GCM dom.push.serverURL scheme",
      serverURI.scheme
    );
    return false;
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "nsPref:changed":
        if (data == "dom.push.debug") {
          // Reconfigure.
          let debug = !!prefs.get("debug");
          console.info(
            "Debug parameter changed; updating configuration with new debug",
            debug
          );
          this._configure(this._serverURI, debug);
        }
        break;
      case "PushServiceAndroidGCM:ReceivedPushMessage":
        this._onPushMessageReceived(data);
        break;
      default:
        break;
    }
  },

  _onPushMessageReceived(data) {
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

    let { headers, message } = this._messageAndHeaders(data);

    console.debug("Delivering message to main PushService:", message, headers);
    this._mainPushService.receivedPushMessage(
      data.channelID,
      "",
      headers,
      message,
      record => {
        // Always update the stored record.
        return record;
      }
    );
  },

  _messageAndHeaders(data) {
    // Default is no data (and no encryption).
    let message = null;
    let headers = null;

    if (data.message) {
      if (data.enc && (data.enckey || data.cryptokey)) {
        headers = {
          encryption_key: data.enckey,
          crypto_key: data.cryptokey,
          encryption: data.enc,
          encoding: data.con,
        };
      } else if (data.con == "aes128gcm") {
        headers = {
          encoding: data.con,
        };
      }
      // Ciphertext is (urlsafe) Base 64 encoded.
      message = ChromeUtils.base64URLDecode(data.message, {
        // The Push server may append padding.
        padding: "ignore",
      });
    }

    return { headers, message };
  },

  _configure(serverURL, debug) {
    return EventDispatcher.instance.sendRequestForResult({
      type: "PushServiceAndroidGCM:Configure",
      endpoint: serverURL.spec,
      debug,
    });
  },

  init(options, mainPushService, serverURL) {
    console.debug("init()");
    this._mainPushService = mainPushService;
    this._serverURI = serverURL;

    prefs.observe("debug", this);
    Services.obs.addObserver(this, "PushServiceAndroidGCM:ReceivedPushMessage");

    return this._configure(serverURL, !!prefs.get("debug")).then(() => {
      EventDispatcher.instance.sendRequestForResult({
        type: "PushServiceAndroidGCM:Initialized",
      });
    });
  },

  uninit() {
    console.debug("uninit()");
    EventDispatcher.instance.sendRequestForResult({
      type: "PushServiceAndroidGCM:Uninitialized",
    });

    this._mainPushService = null;
    Services.obs.removeObserver(
      this,
      "PushServiceAndroidGCM:ReceivedPushMessage"
    );
    prefs.ignore("debug", this);
  },

  onAlarmFired() {
    // No action required.
  },

  connect(records, broadcastListeners) {
    console.debug("connect:", records);
    // It's possible for the registration or subscriptions backing the
    // PushService to not be registered with the underlying AndroidPushService.
    // Expire those that are unrecognized.
    return EventDispatcher.instance
      .sendRequestForResult({
        type: "PushServiceAndroidGCM:DumpSubscriptions",
      })
      .then(subscriptions => {
        subscriptions = JSON.parse(subscriptions);
        console.debug("connect:", subscriptions);
        // subscriptions maps chid => subscription data.
        return Promise.all(
          records.map(record => {
            if (subscriptions.hasOwnProperty(record.keyID)) {
              console.debug("connect:", "hasOwnProperty", record.keyID);
              return Promise.resolve();
            }
            console.debug("connect:", "!hasOwnProperty", record.keyID);
            // Subscription is known to PushService.jsm but not to AndroidPushService.  Drop it.
            return this._mainPushService
              .dropRegistrationAndNotifyApp(record.keyID)
              .catch(error => {
                console.error(
                  "connect: Error dropping registration",
                  record.keyID,
                  error
                );
              });
          })
        );
      });
  },

  async sendSubscribeBroadcast(serviceId, version) {
    // Not implemented yet
  },

  isConnected() {
    return this._mainPushService != null;
  },

  disconnect() {
    console.debug("disconnect");
  },

  register(record) {
    console.debug("register:", record);
    let ctime = Date.now();
    let appServerKey = record.appServerKey
      ? ChromeUtils.base64URLEncode(record.appServerKey, {
          // The Push server requires padding.
          pad: true,
        })
      : null;
    let message = {
      type: "PushServiceAndroidGCM:SubscribeChannel",
      appServerKey,
    };
    if (record.scope == FXA_PUSH_SCOPE) {
      message.service = "fxa";
    }
    // Caller handles errors.
    return EventDispatcher.instance.sendRequestForResult(message).then(data => {
      data = JSON.parse(data);
      console.debug("Got data:", data);
      return PushCrypto.generateKeys().then(
        exportedKeys =>
          new PushRecordAndroidGCM({
            // Straight from autopush.
            channelID: data.channelID,
            pushEndpoint: data.endpoint,
            // Common to all PushRecord implementations.
            scope: record.scope,
            originAttributes: record.originAttributes,
            ctime,
            systemRecord: record.systemRecord,
            // Cryptography!
            p256dhPublicKey: exportedKeys[0],
            p256dhPrivateKey: exportedKeys[1],
            authenticationSecret: PushCrypto.generateAuthenticationSecret(),
            appServerKey: record.appServerKey,
          })
      );
    });
  },

  unregister(record) {
    console.debug("unregister: ", record);
    return EventDispatcher.instance.sendRequestForResult({
      type: "PushServiceAndroidGCM:UnsubscribeChannel",
      channelID: record.keyID,
    });
  },

  reportDeliveryError(messageID, reason) {
    console.warn(
      "reportDeliveryError: Ignoring message delivery error",
      messageID,
      reason
    );
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
