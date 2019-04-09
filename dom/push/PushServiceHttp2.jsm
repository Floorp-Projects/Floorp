/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {PushDB} = ChromeUtils.import("resource://gre/modules/PushDB.jsm");
const {PushRecord} = ChromeUtils.import("resource://gre/modules/PushRecord.jsm");
const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");
const {NetUtil} = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
const {clearTimeout, setTimeout} = ChromeUtils.import("resource://gre/modules/Timer.jsm");

const {
  PushCrypto,
  concatArray,
} = ChromeUtils.import("resource://gre/modules/PushCrypto.jsm");

var EXPORTED_SYMBOLS = ["PushServiceHttp2"];

XPCOMUtils.defineLazyGetter(this, "console", () => {
  let {ConsoleAPI} = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "PushServiceHttp2",
  });
});

const prefs = Services.prefs.getBranch("dom.push.");

const kPUSHHTTP2DB_DB_NAME = "pushHttp2";
const kPUSHHTTP2DB_DB_VERSION = 5; // Change this if the IndexedDB format changes
const kPUSHHTTP2DB_STORE_NAME = "pushHttp2";

/**
 * A proxy between the PushService and connections listening for incoming push
 * messages. The PushService can silence messages from the connections by
 * setting PushSubscriptionListener._pushService to null. This is required
 * because it can happen that there is an outstanding push message that will
 * be send on OnStopRequest but the PushService may not be interested in these.
 * It's easier to stop listening than to have checks at specific points.
 */
var PushSubscriptionListener = function(pushService, uri) {
  console.debug("PushSubscriptionListener()");
  this._pushService = pushService;
  this.uri = uri;
};

PushSubscriptionListener.prototype = {

  QueryInterface: ChromeUtils.generateQI(["nsIHttpPushListener",
                                         "nsIStreamListener"]),

  getInterface(aIID) {
    return this.QueryInterface(aIID);
  },

  onStartRequest(aRequest) {
    console.debug("PushSubscriptionListener: onStartRequest()");
    // We do not do anything here.
  },

  onDataAvailable(aRequest, aStream, aOffset, aCount) {
    console.debug("PushSubscriptionListener: onDataAvailable()");
    // Nobody should send data, but just to be sure, otherwise necko will
    // complain.
    if (aCount === 0) {
      return;
    }

    let inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(Ci.nsIScriptableInputStream);

    inputStream.init(aStream);
    inputStream.read(aCount);
  },

  onStopRequest(aRequest, aStatusCode) {
    console.debug("PushSubscriptionListener: onStopRequest()");
    if (!this._pushService) {
        return;
    }

    this._pushService.connOnStop(aRequest,
                                 Components.isSuccessCode(aStatusCode),
                                 this.uri);
  },

  onPush(associatedChannel, pushChannel) {
    console.debug("PushSubscriptionListener: onPush()");
    var pushChannelListener = new PushChannelListener(this);
    pushChannel.asyncOpen(pushChannelListener);
  },

  disconnect() {
    this._pushService = null;
  },
};

/**
 * The listener for pushed messages. The message data is collected in
 * OnDataAvailable and send to the app in OnStopRequest.
 */
var PushChannelListener = function(pushSubscriptionListener) {
  console.debug("PushChannelListener()");
  this._mainListener = pushSubscriptionListener;
  this._message = [];
  this._ackUri = null;
};

PushChannelListener.prototype = {

  onStartRequest(aRequest) {
    this._ackUri = aRequest.URI.spec;
  },

  onDataAvailable(aRequest, aStream, aOffset, aCount) {
    console.debug("PushChannelListener: onDataAvailable()");

    if (aCount === 0) {
      return;
    }

    let inputStream = Cc["@mozilla.org/binaryinputstream;1"]
                        .createInstance(Ci.nsIBinaryInputStream);

    inputStream.setInputStream(aStream);
    let chunk = new ArrayBuffer(aCount);
    inputStream.readArrayBuffer(aCount, chunk);
    this._message.push(chunk);
  },

  onStopRequest(aRequest, aStatusCode) {
    console.debug("PushChannelListener: onStopRequest()", "status code",
      aStatusCode);
    if (Components.isSuccessCode(aStatusCode) &&
        this._mainListener &&
        this._mainListener._pushService) {
      let headers = {
        encryption_key: getHeaderField(aRequest, "Encryption-Key"),
        crypto_key: getHeaderField(aRequest, "Crypto-Key"),
        encryption: getHeaderField(aRequest, "Encryption"),
        encoding: getHeaderField(aRequest, "Content-Encoding"),
      };
      let msg = concatArray(this._message);

      this._mainListener._pushService._pushChannelOnStop(this._mainListener.uri,
                                                         this._ackUri,
                                                         headers,
                                                         msg);
    }
  },
};

function getHeaderField(aRequest, name) {
  try {
    return aRequest.getRequestHeader(name);
  } catch (e) {
    // getRequestHeader can throw.
    return null;
  }
}

var PushServiceDelete = function(resolve, reject) {
  this._resolve = resolve;
  this._reject = reject;
};

PushServiceDelete.prototype = {

  onStartRequest(aRequest) {},

  onDataAvailable(aRequest, aStream, aOffset, aCount) {
    // Nobody should send data, but just to be sure, otherwise necko will
    // complain.
    if (aCount === 0) {
      return;
    }

    let inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(Ci.nsIScriptableInputStream);

    inputStream.init(aStream);
    inputStream.read(aCount);
  },

  onStopRequest(aRequest, aStatusCode) {
    if (Components.isSuccessCode(aStatusCode)) {
       this._resolve();
    } else {
       this._reject(new Error("Error removing subscription: " + aStatusCode));
    }
  },
};

var SubscriptionListener = function(aSubInfo, aResolve, aReject,
                                    aServerURI, aPushServiceHttp2) {
  console.debug("SubscriptionListener()");
  this._subInfo = aSubInfo;
  this._resolve = aResolve;
  this._reject = aReject;
  this._serverURI = aServerURI;
  this._service = aPushServiceHttp2;
  this._ctime = Date.now();
  this._retryTimeoutID = null;
};

SubscriptionListener.prototype = {

  onStartRequest(aRequest) {},

  onDataAvailable(aRequest, aStream, aOffset, aCount) {},

  onStopRequest(aRequest, aStatus) {
    console.debug("SubscriptionListener: onStopRequest()");

    // Check if pushService is still active.
    if (!this._service.hasmainPushService()) {
      this._reject(new Error("Push service unavailable"));
      return;
    }

    if (!Components.isSuccessCode(aStatus)) {
      this._reject(new Error("Error listening for messages: " + aStatus));
      return;
    }

    var statusCode = aRequest.QueryInterface(Ci.nsIHttpChannel).responseStatus;

    if (Math.floor(statusCode / 100) == 5) {
      if (this._subInfo.retries < prefs.getIntPref("http2.maxRetries")) {
        this._subInfo.retries++;
        var retryAfter = retryAfterParser(aRequest);
        this._retryTimeoutID = setTimeout(_ => {
            this._reject(
              {
                retry: true,
                subInfo: this._subInfo,
              });
            this._service.removeListenerPendingRetry(this);
            this._retryTimeoutID = null;
          }, retryAfter);
        this._service.addListenerPendingRetry(this);
      } else {
        this._reject(new Error("Unexpected server response: " + statusCode));
      }
      return;
    } else if (statusCode != 201) {
      this._reject(new Error("Unexpected server response: " + statusCode));
      return;
    }

    var subscriptionUri;
    try {
      subscriptionUri = aRequest.getResponseHeader("location");
    } catch (err) {
      this._reject(new Error("Missing Location header"));
      return;
    }

    console.debug("onStopRequest: subscriptionUri", subscriptionUri);

    var linkList;
    try {
      linkList = aRequest.getResponseHeader("link");
    } catch (err) {
      this._reject(new Error("Missing Link header"));
      return;
    }

    var linkParserResult;
    try {
      linkParserResult = linkParser(linkList, this._serverURI);
    } catch (e) {
      this._reject(e);
      return;
    }

    if (!subscriptionUri) {
      this._reject(new Error("Invalid Location header"));
      return;
    }
    try {
      Services.io.newURI(subscriptionUri);
    } catch (e) {
      console.error("onStopRequest: Invalid subscription URI",
        subscriptionUri);
      this._reject(new Error("Invalid subscription endpoint: " +
        subscriptionUri));
      return;
    }

    let reply = new PushRecordHttp2({
      subscriptionUri,
      pushEndpoint: linkParserResult.pushEndpoint,
      pushReceiptEndpoint: linkParserResult.pushReceiptEndpoint,
      scope: this._subInfo.record.scope,
      originAttributes: this._subInfo.record.originAttributes,
      systemRecord: this._subInfo.record.systemRecord,
      appServerKey: this._subInfo.record.appServerKey,
      ctime: Date.now(),
    });

    this._resolve(reply);
  },

  abortRetry() {
    if (this._retryTimeoutID != null) {
      clearTimeout(this._retryTimeoutID);
      this._retryTimeoutID = null;
    } else {
      console.debug("SubscriptionListener.abortRetry: aborting non-existent retry?");
    }
  },
};

function retryAfterParser(aRequest) {
  let retryAfter = 0;
  try {
    let retryField = aRequest.getResponseHeader("retry-after");
    if (isNaN(retryField)) {
      retryAfter = Date.parse(retryField) - (new Date().getTime());
    } else {
      retryAfter = parseInt(retryField, 10) * 1000;
    }
    retryAfter = (retryAfter > 0) ? retryAfter : 0;
  } catch (e) {}

  return retryAfter;
}

function linkParser(linkHeader, serverURI) {
  let linkList = linkHeader.split(",");
  if ((linkList.length < 1)) {
    throw new Error("Invalid Link header");
  }

  let pushEndpoint;
  let pushReceiptEndpoint;

  linkList.forEach(link => {
    let linkElems = link.split(";");

    if (linkElems.length == 2) {
      if (linkElems[1].trim() === 'rel="urn:ietf:params:push"') {
        pushEndpoint = linkElems[0].substring(linkElems[0].indexOf("<") + 1,
                                              linkElems[0].indexOf(">"));
      } else if (linkElems[1].trim() === 'rel="urn:ietf:params:push:receipt"') {
        pushReceiptEndpoint = linkElems[0].substring(linkElems[0].indexOf("<") + 1,
                                                     linkElems[0].indexOf(">"));
      }
    }
  });

  console.debug("linkParser: pushEndpoint", pushEndpoint);
  console.debug("linkParser: pushReceiptEndpoint", pushReceiptEndpoint);
  // Missing pushReceiptEndpoint is allowed.
  if (!pushEndpoint) {
    throw new Error("Missing push endpoint");
  }

  const pushURI = Services.io.newURI(pushEndpoint, null, serverURI);
  let pushReceiptURI;
  if (pushReceiptEndpoint) {
    pushReceiptURI = Services.io.newURI(pushReceiptEndpoint, null,
                                        serverURI);
  }

  return {
    pushEndpoint: pushURI.spec,
    pushReceiptEndpoint: (pushReceiptURI) ? pushReceiptURI.spec : "",
  };
}

/**
 * The implementation of the WebPush.
 */
var PushServiceHttp2 = {
  _mainPushService: null,
  _serverURI: null,

  // Keep information about all connections, e.g. the channel, listener...
  _conns: {},
  _started: false,

  // Set of SubscriptionListeners that are pending a subscription retry attempt.
  _listenersPendingRetry: new Set(),

  newPushDB() {
    return new PushDB(kPUSHHTTP2DB_DB_NAME,
                      kPUSHHTTP2DB_DB_VERSION,
                      kPUSHHTTP2DB_STORE_NAME,
                      "subscriptionUri",
                      PushRecordHttp2);
  },

  hasmainPushService() {
    return this._mainPushService !== null;
  },

  validServerURI(serverURI) {
    if (serverURI.scheme == "http") {
      return !!prefs.getBoolPref("testing.allowInsecureServerURL", false);
    }
    return serverURI.scheme == "https";
  },

  async connect(broadcastListeners) {
    let subscriptions = await this._mainPushService.getAllUnexpired();
    this.startConnections(subscriptions);
  },

  async sendSubscribeBroadcast(serviceId, version) {
    // Not implemented yet
  },

  isConnected() {
    return this._mainPushService != null;
  },

  disconnect() {
    this._shutdownConnections(false);
  },

  _makeChannel(aUri) {
    var chan = NetUtil.newChannel({uri: aUri, loadUsingSystemPrincipal: true})
                      .QueryInterface(Ci.nsIHttpChannel);

    var loadGroup = Cc["@mozilla.org/network/load-group;1"]
                      .createInstance(Ci.nsILoadGroup);
    chan.loadGroup = loadGroup;
    return chan;
  },

  /**
   * Subscribe new resource.
   */
  register(aRecord) {
    console.debug("subscribeResource()");

    return this._subscribeResourceInternal({
      record: aRecord,
      retries: 0,
    })
    .then(result =>
      PushCrypto.generateKeys()
        .then(([publicKey, privateKey]) => {
          result.p256dhPublicKey = publicKey;
          result.p256dhPrivateKey = privateKey;
          result.authenticationSecret = PushCrypto.generateAuthenticationSecret();
          this._conns[result.subscriptionUri] = {
            channel: null,
            listener: null,
            countUnableToConnect: 0,
            lastStartListening: 0,
            retryTimerID: 0,
          };
          this._listenForMsgs(result.subscriptionUri);
          return result;
        })
    );
  },

  _subscribeResourceInternal(aSubInfo) {
    console.debug("subscribeResourceInternal()");

    return new Promise((resolve, reject) => {
      var listener = new SubscriptionListener(aSubInfo,
                                              resolve,
                                              reject,
                                              this._serverURI,
                                              this);

      var chan = this._makeChannel(this._serverURI.spec);
      chan.requestMethod = "POST";
      chan.asyncOpen(listener);
    })
    .catch(err => {
      if ("retry" in err) {
        return this._subscribeResourceInternal(err.subInfo);
      }
      throw err;
    });
  },

  _deleteResource(aUri) {
    return new Promise((resolve, reject) => {
      var chan = this._makeChannel(aUri);
      chan.requestMethod = "DELETE";
      chan.asyncOpen(new PushServiceDelete(resolve, reject));
    });
  },

  /**
   * Unsubscribe the resource with a subscription uri aSubscriptionUri.
   * We can't do anything about it if it fails, so we don't listen for response.
   */
  _unsubscribeResource(aSubscriptionUri) {
    console.debug("unsubscribeResource()");

    return this._deleteResource(aSubscriptionUri);
  },

  /**
   * Start listening for messages.
   */
  _listenForMsgs(aSubscriptionUri) {
    console.debug("listenForMsgs()", aSubscriptionUri);
    if (!this._conns[aSubscriptionUri]) {
      console.warn("listenForMsgs: We do not have this subscription",
        aSubscriptionUri);
      return;
    }

    var chan = this._makeChannel(aSubscriptionUri);
    var conn = {};
    conn.channel = chan;
    var listener = new PushSubscriptionListener(this, aSubscriptionUri);
    conn.listener = listener;

    chan.notificationCallbacks = listener;

    try {
      chan.asyncOpen(listener);
    } catch (e) {
      console.error("listenForMsgs: Error connecting to push server.",
        "asyncOpen failed", e);
      conn.listener.disconnect();
      chan.cancel(Cr.NS_ERROR_ABORT);
      this._retryAfterBackoff(aSubscriptionUri, -1);
      return;
    }

    this._conns[aSubscriptionUri].lastStartListening = Date.now();
    this._conns[aSubscriptionUri].channel = conn.channel;
    this._conns[aSubscriptionUri].listener = conn.listener;
  },

  _ackMsgRecv(aAckUri) {
    console.debug("ackMsgRecv()", aAckUri);
    return this._deleteResource(aAckUri);
  },

  init(aOptions, aMainPushService, aServerURL) {
    console.debug("init()");
    this._mainPushService = aMainPushService;
    this._serverURI = aServerURL;

    return Promise.resolve();
  },

  _retryAfterBackoff(aSubscriptionUri, retryAfter) {
    console.debug("retryAfterBackoff()");

    var resetRetryCount = prefs.getIntPref("http2.reset_retry_count_after_ms");
    // If it was running for some time, reset retry counter.
    if ((Date.now() - this._conns[aSubscriptionUri].lastStartListening) >
        resetRetryCount) {
      this._conns[aSubscriptionUri].countUnableToConnect = 0;
    }

    let maxRetries = prefs.getIntPref("http2.maxRetries");
    if (this._conns[aSubscriptionUri].countUnableToConnect >= maxRetries) {
      this._shutdownSubscription(aSubscriptionUri);
      this._resubscribe(aSubscriptionUri);
      return;
    }

    if (retryAfter !== -1) {
      // This is a 5xx response.
      this._conns[aSubscriptionUri].countUnableToConnect++;
      this._conns[aSubscriptionUri].retryTimerID =
        setTimeout(_ => this._listenForMsgs(aSubscriptionUri), retryAfter);
      return;
    }

    retryAfter = prefs.getIntPref("http2.retryInterval") *
      Math.pow(2, this._conns[aSubscriptionUri].countUnableToConnect);

    retryAfter = retryAfter * (0.8 + Math.random() * 0.4); // add +/-20%.

    this._conns[aSubscriptionUri].countUnableToConnect++;
    this._conns[aSubscriptionUri].retryTimerID =
      setTimeout(_ => this._listenForMsgs(aSubscriptionUri), retryAfter);

    console.debug("retryAfterBackoff: Retry in", retryAfter);
  },

  // Close connections.
  _shutdownConnections(deleteInfo) {
    console.debug("shutdownConnections()");

    for (let subscriptionUri in this._conns) {
      if (this._conns[subscriptionUri]) {
        if (this._conns[subscriptionUri].listener) {
          this._conns[subscriptionUri].listener._pushService = null;
        }

        if (this._conns[subscriptionUri].channel) {
          try {
            this._conns[subscriptionUri].channel.cancel(Cr.NS_ERROR_ABORT);
          } catch (e) {}
        }
        this._conns[subscriptionUri].listener = null;
        this._conns[subscriptionUri].channel = null;

        if (this._conns[subscriptionUri].retryTimerID > 0) {
          clearTimeout(this._conns[subscriptionUri].retryTimerID);
        }

        if (deleteInfo) {
          delete this._conns[subscriptionUri];
        }
      }
    }
  },

  // Start listening if subscriptions present.
  startConnections(aSubscriptions) {
    console.debug("startConnections()", aSubscriptions.length);

    for (let i = 0; i < aSubscriptions.length; i++) {
      let record = aSubscriptions[i];
      this._mainPushService.ensureCrypto(record).then(record => {
        this._startSingleConnection(record);
      }, error => {
        console.error("startConnections: Error updating record",
          record.keyID, error);
      });
    }
  },

  _startSingleConnection(record) {
    console.debug("_startSingleConnection()");
    if (typeof this._conns[record.subscriptionUri] != "object") {
      this._conns[record.subscriptionUri] = {channel: null,
                                             listener: null,
                                             countUnableToConnect: 0,
                                             retryTimerID: 0};
    }
    if (!this._conns[record.subscriptionUri].conn) {
      this._listenForMsgs(record.subscriptionUri);
    }
  },

  // Close connection and notify apps that subscription are gone.
  _shutdownSubscription(aSubscriptionUri) {
    console.debug("shutdownSubscriptions()");

    if (typeof this._conns[aSubscriptionUri] == "object") {
      if (this._conns[aSubscriptionUri].listener) {
        this._conns[aSubscriptionUri].listener._pushService = null;
      }

      if (this._conns[aSubscriptionUri].channel) {
        try {
          this._conns[aSubscriptionUri].channel.cancel(Cr.NS_ERROR_ABORT);
        } catch (e) {}
      }
      delete this._conns[aSubscriptionUri];
    }
  },

  uninit() {
    console.debug("uninit()");
    this._abortPendingSubscriptionRetries();
    this._shutdownConnections(true);
    this._mainPushService = null;
  },

  _abortPendingSubscriptionRetries() {
    this._listenersPendingRetry.forEach((listener) => listener.abortRetry());
    this._listenersPendingRetry.clear();
  },

  unregister(aRecord) {
    this._shutdownSubscription(aRecord.subscriptionUri);
    return this._unsubscribeResource(aRecord.subscriptionUri);
  },

  reportDeliveryError(messageID, reason) {
    console.warn("reportDeliveryError: Ignoring message delivery error",
      messageID, reason);
  },

  /** Push server has deleted subscription.
   *  Re-subscribe - if it succeeds send update db record and send
   *                 pushsubscriptionchange,
   *               - on error delete record and send pushsubscriptionchange
   *  TODO: maybe pushsubscriptionerror will be included.
   */
  _resubscribe(aSubscriptionUri) {
    this._mainPushService.getByKeyID(aSubscriptionUri)
      .then(record => this.register(record)
        .then(recordNew => {
          if (this._mainPushService) {
            this._mainPushService
                .updateRegistrationAndNotifyApp(aSubscriptionUri, recordNew)
                .catch(Cu.reportError);
          }
        }, error => {
          if (this._mainPushService) {
            this._mainPushService
                .dropRegistrationAndNotifyApp(aSubscriptionUri)
                .catch(Cu.reportError);
          }
        })
      );
  },

  connOnStop(aRequest, aSuccess, aSubscriptionUri) {
    console.debug("connOnStop() succeeded", aSuccess);

    var conn = this._conns[aSubscriptionUri];
    if (!conn) {
      // there is no connection description that means that we closed
      // connection, so do nothing. But we should have already deleted
      // the listener.
      return;
    }

    conn.channel = null;
    conn.listener = null;

    if (!aSuccess) {
      this._retryAfterBackoff(aSubscriptionUri, -1);
    } else if (Math.floor(aRequest.responseStatus / 100) == 5) {
      var retryAfter = retryAfterParser(aRequest);
      this._retryAfterBackoff(aSubscriptionUri, retryAfter);
    } else if (Math.floor(aRequest.responseStatus / 100) == 4) {
      this._shutdownSubscription(aSubscriptionUri);
      this._resubscribe(aSubscriptionUri);
    } else if (Math.floor(aRequest.responseStatus / 100) == 2) { // This should be 204
      setTimeout(_ => this._listenForMsgs(aSubscriptionUri), 0);
    } else {
      this._retryAfterBackoff(aSubscriptionUri, -1);
    }
  },

  addListenerPendingRetry(aListener) {
    this._listenersPendingRetry.add(aListener);
  },

  removeListenerPendingRetry(aListener) {
    if (!this._listenersPendingRetry.remove(aListener)) {
      console.debug("removeListenerPendingRetry: listener not in list?");
    }
  },

  _pushChannelOnStop(aUri, aAckUri, aHeaders, aMessage) {
    console.debug("pushChannelOnStop()");

    this._mainPushService.receivedPushMessage(
      aUri, "", aHeaders, aMessage, record => {
        // Always update the stored record.
        return record;
      }
    )
    .then(_ => this._ackMsgRecv(aAckUri))
    .catch(err => {
      console.error("pushChannelOnStop: Error receiving message",
        err);
    });
  },
};

function PushRecordHttp2(record) {
  PushRecord.call(this, record);
  this.subscriptionUri = record.subscriptionUri;
  this.pushReceiptEndpoint = record.pushReceiptEndpoint;
}

PushRecordHttp2.prototype = Object.create(PushRecord.prototype, {
  keyID: {
    get() {
      return this.subscriptionUri;
    },
  },
});

PushRecordHttp2.prototype.toSubscription = function() {
  let subscription = PushRecord.prototype.toSubscription.call(this);
  subscription.pushReceiptEndpoint = this.pushReceiptEndpoint;
  return subscription;
};
