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
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const {
  PushCrypto,
  concatArray,
  getEncryptionKeyParams,
  getEncryptionParams,
} = Cu.import("resource://gre/modules/PushCrypto.jsm");

this.EXPORTED_SYMBOLS = ["PushServiceHttp2"];

const prefs = new Preferences("dom.push.");

// Don't modify this, instead set dom.push.debug.
// Set debug first so that all debugging actually works.
var gDebuggingEnabled = prefs.get("debug");

function debug(s) {
  if (gDebuggingEnabled) {
    dump("-*- PushServiceHttp2.jsm: " + s + "\n");
  }
}

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
  debug("Creating a new pushSubscription listener.");
  this._pushService = pushService;
  this.uri = uri;
};

PushSubscriptionListener.prototype = {

  QueryInterface: function (aIID) {
    if (aIID.equals(Ci.nsIHttpPushListener) ||
        aIID.equals(Ci.nsIStreamListener)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  onStartRequest: function(aRequest, aContext) {
    debug("PushSubscriptionListener onStartRequest()");
    // We do not do anything here.
  },

  onDataAvailable: function(aRequest, aContext, aStream, aOffset, aCount) {
    debug("PushSubscriptionListener onDataAvailable()");
    // Nobody should send data, but just to be sure, otherwise necko will
    // complain.
    if (aCount === 0) {
      return;
    }

    let inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(Ci.nsIScriptableInputStream);

    inputStream.init(aStream);
    var data = inputStream.read(aCount);
  },

  onStopRequest: function(aRequest, aContext, aStatusCode) {
    debug("PushSubscriptionListener onStopRequest()");
    if (!this._pushService) {
        return;
    }

    this._pushService.connOnStop(aRequest,
                                 Components.isSuccessCode(aStatusCode),
                                 this.uri);
  },

  onPush: function(associatedChannel, pushChannel) {
    debug("PushSubscriptionListener onPush()");
    var pushChannelListener = new PushChannelListener(this);
    pushChannel.asyncOpen(pushChannelListener, pushChannel);
  },

  disconnect: function() {
    this._pushService = null;
  }
};

/**
 * The listener for pushed messages. The message data is collected in
 * OnDataAvailable and send to the app in OnStopRequest.
 */
var PushChannelListener = function(pushSubscriptionListener) {
  debug("Creating a new push channel listener.");
  this._mainListener = pushSubscriptionListener;
  this._message = [];
  this._ackUri = null;
};

PushChannelListener.prototype = {

  onStartRequest: function(aRequest, aContext) {
    this._ackUri = aRequest.URI.spec;
  },

  onDataAvailable: function(aRequest, aContext, aStream, aOffset, aCount) {
    debug("push channel listener onDataAvailable()");

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

  onStopRequest: function(aRequest, aContext, aStatusCode) {
    debug("push channel listener onStopRequest()  status code:" + aStatusCode);
    if (Components.isSuccessCode(aStatusCode) &&
        this._mainListener &&
        this._mainListener._pushService) {

      var keymap = encryptKeyFieldParser(aRequest);
      if (!keymap) {
        return;
      }
      var enc = encryptFieldParser(aRequest);
      if (!enc || !enc.keyid) {
        return;
      }
      var dh = keymap[enc.keyid];
      var salt = enc.salt;
      var rs = (enc.rs)? parseInt(enc.rs, 10) : 4096;
      if (!dh || !salt || isNaN(rs) || (rs <= 1)) {
        return;
      }

      var msg = concatArray(this._message);

      this._mainListener._pushService._pushChannelOnStop(this._mainListener.uri,
                                                         this._ackUri,
                                                         msg,
                                                         dh,
                                                         salt,
                                                         rs);
    }
  }
};

function encryptKeyFieldParser(aRequest) {
  try {
    var encryptKeyField = aRequest.getRequestHeader("Encryption-Key");
    return getEncryptionKeyParams(encryptKeyField);
  } catch(e) {
    // getRequestHeader can throw.
    return null;
  }
}

function encryptFieldParser(aRequest) {
  try {
    var encryptField = aRequest.getRequestHeader("Encryption");
    return getEncryptionParams(encryptField);
  } catch(e) {
    // getRequestHeader can throw.
    return null;
  }
}

var PushServiceDelete = function(resolve, reject) {
  this._resolve = resolve;
  this._reject = reject;
};

PushServiceDelete.prototype = {

  onStartRequest: function(aRequest, aContext) {},

  onDataAvailable: function(aRequest, aContext, aStream, aOffset, aCount) {
    // Nobody should send data, but just to be sure, otherwise necko will
    // complain.
    if (aCount === 0) {
      return;
    }

    let inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(Ci.nsIScriptableInputStream);

    inputStream.init(aStream);
    var data = inputStream.read(aCount);
  },

  onStopRequest: function(aRequest, aContext, aStatusCode) {

    if (Components.isSuccessCode(aStatusCode)) {
       this._resolve();
    } else {
       this._reject({status: 0, error: "NetworkError"});
    }
  }
};

var SubscriptionListener = function(aSubInfo, aResolve, aReject,
                                    aServerURI, aPushServiceHttp2) {
  debug("Creating a new subscription listener.");
  this._subInfo = aSubInfo;
  this._resolve = aResolve;
  this._reject = aReject;
  this._data = '';
  this._serverURI = aServerURI;
  this._service = aPushServiceHttp2;
  this._ctime = Date.now();
};

SubscriptionListener.prototype = {

  onStartRequest: function(aRequest, aContext) {},

  onDataAvailable: function(aRequest, aContext, aStream, aOffset, aCount) {
    debug("subscription listener onDataAvailable()");

    // We do not expect any data, but necko will complain if we do not consume
    // it.
    if (aCount === 0) {
      return;
    }

    let inputStream = Cc["@mozilla.org/scriptableinputstream;1"]
                        .createInstance(Ci.nsIScriptableInputStream);

    inputStream.init(aStream);
    this._data.concat(inputStream.read(aCount));
  },

  onStopRequest: function(aRequest, aContext, aStatus) {
    debug("subscription listener onStopRequest()");

    // Check if pushService is still active.
    if (!this._service.hasmainPushService()) {
      this._reject({error: "Service deactivated"});
      return;
    }

    if (!Components.isSuccessCode(aStatus)) {
      this._reject({error: "Error status" + aStatus});
      return;
    }

    var statusCode = aRequest.QueryInterface(Ci.nsIHttpChannel).responseStatus;

    if (Math.floor(statusCode / 100) == 5) {
      if (this._subInfo.retries < prefs.get("http2.maxRetries")) {
        this._subInfo.retries++;
        var retryAfter = retryAfterParser(aRequest);
        setTimeout(_ => this._reject(
          {
            retry: true,
            subInfo: this._subInfo
          }),
          retryAfter);
      } else {
        this._reject({error: "Error response code: " + statusCode });
      }
      return;
    } else if (statusCode != 201) {
      this._reject({error: "Error response code: " + statusCode });
      return;
    }

    var subscriptionUri;
    try {
      subscriptionUri = aRequest.getResponseHeader("location");
    } catch (err) {
      this._reject({error: "Return code 201, but the answer is bogus"});
      return;
    }

    debug("subscriptionUri: " + subscriptionUri);

    var linkList;
    try {
      linkList = aRequest.getResponseHeader("link");
    } catch (err) {
      this._reject({error: "Return code 201, but the answer is bogus"});
      return;
    }

    var linkParserResult = linkParser(linkList, this._serverURI);
    if (linkParserResult.error) {
      this._reject(linkParserResult);
      return;
    }

    if (!subscriptionUri) {
      this._reject({error: "Return code 201, but the answer is bogus," +
                           " missing subscriptionUri"});
      return;
    }
    try {
      let uriTry = Services.io.newURI(subscriptionUri, null, null);
    } catch (e) {
      debug("Invalid URI " + subscriptionUri);
      this._reject({error: "Return code 201, but URI is bogus. " +
                    subscriptionUri});
      return;
    }

    let reply = new PushRecordHttp2({
      subscriptionUri: subscriptionUri,
      pushEndpoint: linkParserResult.pushEndpoint,
      pushReceiptEndpoint: linkParserResult.pushReceiptEndpoint,
      scope: this._subInfo.record.scope,
      originAttributes: this._subInfo.record.originAttributes,
      quota: this._subInfo.record.maxQuota,
      ctime: Date.now(),
    });

    Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_HTTP2_TIME").add(Date.now() - this._ctime);
    this._resolve(reply);
  },
};

function retryAfterParser(aRequest) {
  var retryAfter = 0;
  try {
    var retryField = aRequest.getResponseHeader("retry-after");
    if (isNaN(retryField)) {
      retryAfter = Date.parse(retryField) - (new Date().getTime());
    } else {
      retryAfter = parseInt(retryField, 10) * 1000;
    }
    retryAfter = (retryAfter > 0) ? retryAfter : 0;
  } catch(e) {}

  return retryAfter;
}

function linkParser(linkHeader, serverURI) {

  var linkList = linkHeader.split(',');
  if ((linkList.length < 1)) {
    return {error: "Return code 201, but the answer is bogus"};
  }

  var pushEndpoint;
  var pushReceiptEndpoint;

  linkList.forEach(link => {
    var linkElems = link.split(';');

    if (linkElems.length == 2) {
      if (linkElems[1].trim() === 'rel="urn:ietf:params:push"') {
        pushEndpoint = linkElems[0].substring(linkElems[0].indexOf('<') + 1,
                                              linkElems[0].indexOf('>'));

      } else if (linkElems[1].trim() === 'rel="urn:ietf:params:push:receipt"') {
        pushReceiptEndpoint = linkElems[0].substring(linkElems[0].indexOf('<') + 1,
                                                     linkElems[0].indexOf('>'));
      }
    }
  });

  debug("pushEndpoint: " + pushEndpoint);
  debug("pushReceiptEndpoint: " + pushReceiptEndpoint);
  // Missing pushReceiptEndpoint is allowed.
  if (!pushEndpoint) {
    return {error: "Return code 201, but the answer is bogus, missing" +
                   " pushEndpoint"};
  }

  var uri;
  var resUri = [];
  try {
    [pushEndpoint, pushReceiptEndpoint].forEach(u => {
      if (u) {
        uri = u;
        resUri[u] = Services.io.newURI(uri, null, serverURI);
      }
    });
  } catch (e) {
    debug("Invalid URI " + uri);
    return {error: "Return code 201, but URI is bogus. " + uri};
  }

  return {
    pushEndpoint: resUri[pushEndpoint].spec,
    pushReceiptEndpoint: (pushReceiptEndpoint) ? resUri[pushReceiptEndpoint].spec
                                               : ""
  };
}

/**
 * The implementation of the WebPush.
 */
this.PushServiceHttp2 = {
  _mainPushService: null,
  _serverURI: null,

  // Keep information about all connections, e.g. the channel, listener...
  _conns: {},
  _started: false,

  newPushDB: function() {
    return new PushDB(kPUSHHTTP2DB_DB_NAME,
                      kPUSHHTTP2DB_DB_VERSION,
                      kPUSHHTTP2DB_STORE_NAME,
                      "subscriptionUri",
                      PushRecordHttp2);
  },

  hasmainPushService: function() {
    return this._mainPushService !== null;
  },

  checkServerURI: function(serverURL) {
    if (!serverURL) {
      debug("No dom.push.serverURL found!");
      return;
    }

    let uri;
    try {
      uri = Services.io.newURI(serverURL, null, null);
    } catch(e) {
      debug("Error creating valid URI from dom.push.serverURL (" +
            serverURL + ")");
      return null;
    }

    if (uri.scheme !== "https") {
      debug("Unsupported websocket scheme " + uri.scheme);
      return null;
    }
    return uri;
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed") {
      if (aData == "dom.push.debug") {
        gDebuggingEnabled = prefs.get("debug");
      }
    }
  },

  connect: function(subscriptions) {
    this.startConnections(subscriptions);
  },

  disconnect: function() {
    this._shutdownConnections(false);
  },

  _makeChannel: function(aUri) {

    var ios = Cc["@mozilla.org/network/io-service;1"]
                .getService(Ci.nsIIOService);

    var chan = ios.newChannel2(aUri,
                               null,
                               null,
                               null,      // aLoadingNode
                               Services.scriptSecurityManager.getSystemPrincipal(),
                               null,      // aTriggeringPrincipal
                               Ci.nsILoadInfo.SEC_NORMAL,
                               Ci.nsIContentPolicy.TYPE_OTHER)
                 .QueryInterface(Ci.nsIHttpChannel);

    var loadGroup = Cc["@mozilla.org/network/load-group;1"]
                      .createInstance(Ci.nsILoadGroup);
    chan.loadGroup = loadGroup;
    return chan;
  },

  /**
   * Subscribe new resource.
   */
  _subscribeResource: function(aRecord) {
    debug("subscribeResource()");

    return this._subscribeResourceInternal({
      record: aRecord,
      retries: 0
    })
    .then(result =>
      PushCrypto.generateKeys()
      .then(exportedKeys => {
        result.p256dhPublicKey = exportedKeys[0];
        result.p256dhPrivateKey = exportedKeys[1];
        this._conns[result.subscriptionUri] = {
          channel: null,
          listener: null,
          countUnableToConnect: 0,
          lastStartListening: 0,
          waitingForAlarm: false
        };
        this._listenForMsgs(result.subscriptionUri);
        return result;
      })
    );
  },

  _subscribeResourceInternal: function(aSubInfo) {
    debug("subscribeResourceInternal()");

    return new Promise((resolve, reject) => {
      var listener = new SubscriptionListener(aSubInfo,
                                              resolve,
                                              reject,
                                              this._serverURI,
                                              this);

      var chan = this._makeChannel(this._serverURI.spec);
      chan.requestMethod = "POST";
      try {
        chan.asyncOpen(listener, null);
      } catch(e) {
        reject({status: 0, error: "NetworkError"});
      }
    })
    .catch(err => {
      if ("retry" in err) {
        return this._subscribeResourceInternal(err.subInfo);
      } else {
        throw err;
      }
    })
  },

  _deleteResource: function(aUri) {

    return new Promise((resolve,reject) => {
      var chan = this._makeChannel(aUri);
      chan.requestMethod = "DELETE";
      try {
        chan.asyncOpen(new PushServiceDelete(resolve, reject), null);
      } catch(err) {
        reject({status: 0, error: "NetworkError"});
      }
    });
  },

  /**
   * Unsubscribe the resource with a subscription uri aSubscriptionUri.
   * We can't do anything about it if it fails, so we don't listen for response.
   */
  _unsubscribeResource: function(aSubscriptionUri) {
    debug("unsubscribeResource()");

    return this._deleteResource(aSubscriptionUri);
  },

  /**
   * Start listening for messages.
   */
  _listenForMsgs: function(aSubscriptionUri) {
    debug("listenForMsgs() " + aSubscriptionUri);
    if (!this._conns[aSubscriptionUri]) {
      debug("We do not have this subscription " + aSubscriptionUri);
      return;
    }

    var chan = this._makeChannel(aSubscriptionUri);
    var conn = {};
    conn.channel = chan;
    var listener = new PushSubscriptionListener(this, aSubscriptionUri);
    conn.listener = listener;

    chan.notificationCallbacks = listener;

    try {
      chan.asyncOpen(listener, chan);
    } catch (e) {
      debug("Error connecting to push server. asyncOpen failed!");
      conn.listener.disconnect();
      chan.cancel(Cr.NS_ERROR_ABORT);
      this._retryAfterBackoff(aSubscriptionUri, -1);
      return;
    }

    this._conns[aSubscriptionUri].lastStartListening = Date.now();
    this._conns[aSubscriptionUri].channel = conn.channel;
    this._conns[aSubscriptionUri].listener = conn.listener;

  },

  _ackMsgRecv: function(aAckUri) {
    debug("ackMsgRecv() " + aAckUri);
    // We can't do anything about it if it fails,
    // so we don't listen for response.
    this._deleteResource(aAckUri);
  },

  init: function(aOptions, aMainPushService, aServerURL) {
    debug("init()");
    this._mainPushService = aMainPushService;
    this._serverURI = aServerURL;
    gDebuggingEnabled = prefs.get("debug");
    prefs.observe("debug", this);
  },

  _retryAfterBackoff: function(aSubscriptionUri, retryAfter) {
    debug("retryAfterBackoff()");

    var resetRetryCount = prefs.get("http2.reset_retry_count_after_ms");
    // If it was running for some time, reset retry counter.
    if ((Date.now() - this._conns[aSubscriptionUri].lastStartListening) >
        resetRetryCount) {
      this._conns[aSubscriptionUri].countUnableToConnect = 0;
    }

    let maxRetries = prefs.get("http2.maxRetries");
    if (this._conns[aSubscriptionUri].countUnableToConnect >= maxRetries) {
      this._shutdownSubscription(aSubscriptionUri);
      this._resubscribe(aSubscriptionUri);
      return;
    }

    if (retryAfter !== -1) {
      // This is a 5xx response.
      // To respect RetryAfter header, setTimeout is used. setAlarm sets a
      // cumulative alarm so it will not always respect RetryAfter header.
      this._conns[aSubscriptionUri].countUnableToConnect++;
      setTimeout(_ => this._listenForMsgs(aSubscriptionUri), retryAfter);
      return;
    }

    // we set just one alarm because most probably all connection will go over
    // a single TCP connection.
    retryAfter = prefs.get("http2.retryInterval") *
      Math.pow(2, this._conns[aSubscriptionUri].countUnableToConnect);

    retryAfter = retryAfter * (0.8 + Math.random() * 0.4); // add +/-20%.

    this._conns[aSubscriptionUri].countUnableToConnect++;

    if (retryAfter === 0) {
      setTimeout(_ => this._listenForMsgs(aSubscriptionUri), 0);
    } else {
      this._conns[aSubscriptionUri].waitingForAlarm = true;
      this._mainPushService.setAlarm(retryAfter);
    }
      debug("Retry in " + retryAfter);
  },

  // Close connections.
  _shutdownConnections: function(deleteInfo) {
    debug("shutdownConnections()");

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
        this._conns[subscriptionUri].waitingForAlarm = false;
        if (deleteInfo) {
          delete this._conns[subscriptionUri];
        }
      }
    }
  },

  // Start listening if subscriptions present.
  startConnections: function(aSubscriptions) {
    debug("startConnections() " + aSubscriptions.length);

    for (let i = 0; i < aSubscriptions.length; i++) {
      let record = aSubscriptions[i];
      this._mainPushService.ensureP256dhKey(record).then(record => {
        this._startSingleConnection(record);
      }, error => {
        debug("startConnections: Error updating record " + record.keyID);
      });
    }
  },

  _startSingleConnection: function(record) {
    debug("_startSingleConnection()");
    if (typeof this._conns[record.subscriptionUri] != "object") {
      this._conns[record.subscriptionUri] = {channel: null,
                                             listener: null,
                                             countUnableToConnect: 0,
                                             waitingForAlarm: false};
    }
    if (!this._conns[record.subscriptionUri].conn) {
      this._conns[record.subscriptionUri].waitingForAlarm = false;
      this._listenForMsgs(record.subscriptionUri);
    }
  },

  // Start listening if subscriptions present.
  _startConnectionsWaitingForAlarm: function() {
    debug("startConnectionsWaitingForAlarm()");
    for (let subscriptionUri in this._conns) {
      if ((this._conns[subscriptionUri]) &&
          !this._conns[subscriptionUri].conn &&
          this._conns[subscriptionUri].waitingForAlarm) {
        this._conns[subscriptionUri].waitingForAlarm = false;
        this._listenForMsgs(subscriptionUri);
      }
    }
  },

  // Close connection and notify apps that subscription are gone.
  _shutdownSubscription: function(aSubscriptionUri) {
    debug("shutdownSubscriptions()");

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

  uninit: function() {
    debug("uninit()");
    this._shutdownConnections(true);
    this._mainPushService = null;
  },


  request: function(action, aRecord) {
    switch (action) {
      case "register":
        debug("register");
        return this._subscribeResource(aRecord);
     case "unregister":
        this._shutdownSubscription(aRecord.subscriptionUri);
        return this._unsubscribeResource(aRecord.subscriptionUri);
    }
  },

  /** Push server has deleted subscription.
   *  Re-subscribe - if it succeeds send update db record and send
   *                 pushsubscriptionchange,
   *               - on error delete record and send pushsubscriptionchange
   *  TODO: maybe pushsubscriptionerror will be included.
   */
  _resubscribe: function(aSubscriptionUri) {
    this._mainPushService.getByKeyID(aSubscriptionUri)
      .then(record => this._subscribeResource(record)
        .then(recordNew => {
          if (this._mainPushService) {
            this._mainPushService.updateRegistrationAndNotifyApp(aSubscriptionUri,
                                                                 recordNew);
          }
        }, error => {
          if (this._mainPushService) {
            this._mainPushService.dropRegistrationAndNotifyApp(aSubscriptionUri);
          }
        })
      );
  },

  connOnStop: function(aRequest, aSuccess,
                       aSubscriptionUri) {
    debug("connOnStop() succeeded: " + aSuccess);

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

  _pushChannelOnStop: function(aUri, aAckUri, aMessage, dh, salt, rs) {
    debug("pushChannelOnStop() ");

    let cryptoParams = {
      dh: dh,
      salt: salt,
      rs: rs,
    };
    this._mainPushService.receivedPushMessage(
      aUri, aMessage, cryptoParams, record => {
        // Always update the stored record.
        return record;
      }
    )
    .then(_ => this._ackMsgRecv(aAckUri))
    .catch(err => {
      debug("Error receiving message: " + err);
    });
  },

  onAlarmFired: function() {
    this._startConnectionsWaitingForAlarm();
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

PushRecordHttp2.prototype.toRegistration = function() {
  let registration = PushRecord.prototype.toRegistration.call(this);
  registration.pushReceiptEndpoint = this.pushReceiptEndpoint;
  return registration;
};

PushRecordHttp2.prototype.toRegister = function() {
  let register = PushRecord.prototype.toRegister.call(this);
  register.pushReceiptEndpoint = this.pushReceiptEndpoint;
  return register;
};
