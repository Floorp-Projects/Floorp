/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Preferences.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/Timer.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const {PushDB} = ChromeUtils.import("resource://gre/modules/PushDB.jsm");
const {PushRecord} = ChromeUtils.import("resource://gre/modules/PushRecord.jsm");
const {PushCrypto} = ChromeUtils.import("resource://gre/modules/PushCrypto.jsm");

const kPUSHWSDB_DB_NAME = "pushapi";
const kPUSHWSDB_DB_VERSION = 5; // Change this if the IndexedDB format changes
const kPUSHWSDB_STORE_NAME = "pushapi";

// WebSocket close code sent by the server to indicate that the client should
// not automatically reconnect.
const kBACKOFF_WS_STATUS_CODE = 4774;

// Maps ack statuses, unsubscribe reasons, and delivery error reasons to codes
// included in request payloads.
const kACK_STATUS_TO_CODE = {
  [Ci.nsIPushErrorReporter.ACK_DELIVERED]: 100,
  [Ci.nsIPushErrorReporter.ACK_DECRYPTION_ERROR]: 101,
  [Ci.nsIPushErrorReporter.ACK_NOT_DELIVERED]: 102,
};

const kUNREGISTER_REASON_TO_CODE = {
  [Ci.nsIPushErrorReporter.UNSUBSCRIBE_MANUAL]: 200,
  [Ci.nsIPushErrorReporter.UNSUBSCRIBE_QUOTA_EXCEEDED]: 201,
  [Ci.nsIPushErrorReporter.UNSUBSCRIBE_PERMISSION_REVOKED]: 202,
};

const kDELIVERY_REASON_TO_CODE = {
  [Ci.nsIPushErrorReporter.DELIVERY_UNCAUGHT_EXCEPTION]: 301,
  [Ci.nsIPushErrorReporter.DELIVERY_UNHANDLED_REJECTION]: 302,
  [Ci.nsIPushErrorReporter.DELIVERY_INTERNAL_ERROR]: 303,
};

const prefs = new Preferences("dom.push.");

var EXPORTED_SYMBOLS = ["PushServiceWebSocket"];

XPCOMUtils.defineLazyGetter(this, "console", () => {
  let {ConsoleAPI} = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "PushServiceWebSocket",
  });
});

/**
 * A proxy between the PushService and the WebSocket. The listener is used so
 * that the PushService can silence messages from the WebSocket by setting
 * PushWebSocketListener._pushService to null. This is required because
 * a WebSocket can continue to send messages or errors after it has been
 * closed but the PushService may not be interested in these. It's easier to
 * stop listening than to have checks at specific points.
 */
var PushWebSocketListener = function(pushService) {
  this._pushService = pushService;
};

PushWebSocketListener.prototype = {
  onStart: function(context) {
    if (!this._pushService) {
        return;
    }
    this._pushService._wsOnStart(context);
  },

  onStop: function(context, statusCode) {
    if (!this._pushService) {
        return;
    }
    this._pushService._wsOnStop(context, statusCode);
  },

  onAcknowledge: function(context, size) {
    // EMPTY
  },

  onBinaryMessageAvailable: function(context, message) {
    // EMPTY
  },

  onMessageAvailable: function(context, message) {
    if (!this._pushService) {
        return;
    }
    this._pushService._wsOnMessageAvailable(context, message);
  },

  onServerClose: function(context, aStatusCode, aReason) {
    if (!this._pushService) {
        return;
    }
    this._pushService._wsOnServerClose(context, aStatusCode, aReason);
  }
};

// websocket states
// websocket is off
const STATE_SHUT_DOWN = 0;
// Websocket has been opened on client side, waiting for successful open.
// (_wsOnStart)
const STATE_WAITING_FOR_WS_START = 1;
// Websocket opened, hello sent, waiting for server reply (_handleHelloReply).
const STATE_WAITING_FOR_HELLO = 2;
// Websocket operational, handshake completed, begin protocol messaging.
const STATE_READY = 3;

var PushServiceWebSocket = {
  _mainPushService: null,
  _serverURI: null,

  newPushDB: function() {
    return new PushDB(kPUSHWSDB_DB_NAME,
                      kPUSHWSDB_DB_VERSION,
                      kPUSHWSDB_STORE_NAME,
                      "channelID",
                      PushRecordWebSocket);
  },

  disconnect: function() {
    this._shutdownWS();
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed" && aData == "dom.push.userAgentID") {
      this._onUAIDChanged();
    } else if (aTopic == "timer-callback") {
      this._onTimerFired(aSubject);
    }
  },

  /**
   * Handles a UAID change. Unlike reconnects, we cancel all pending requests
   * after disconnecting. Existing subscriptions stored in IndexedDB will be
   * dropped on reconnect.
   */
  _onUAIDChanged() {
    console.debug("onUAIDChanged()");

    this._shutdownWS();
    this._startBackoffTimer();
  },

  /** Handles a ping, backoff, or request timeout timer event. */
  _onTimerFired(timer) {
    console.debug("onTimerFired()");

    if (timer == this._pingTimer) {
      this._sendPing();
      return;
    }

    if (timer == this._backoffTimer) {
      console.debug("onTimerFired: Reconnecting after backoff");
      this._beginWSSetup();
      return;
    }

    if (timer == this._requestTimeoutTimer) {
      this._timeOutRequests();
      return;
    }
  },

  /**
   * Sends a ping to the server. Bypasses the request queue, but starts the
   * request timeout timer. If the socket is already closed, or the server
   * does not respond within the timeout, the client will reconnect.
   */
  _sendPing() {
    console.debug("sendPing()");

    this._startRequestTimeoutTimer();
    try {
      this._wsSendMessage({});
      this._lastPingTime = Date.now();
    } catch (e) {
      console.debug("sendPing: Error sending ping", e);
      this._reconnect();
    }
  },

  /** Times out any pending requests. */
  _timeOutRequests() {
    console.debug("timeOutRequests()");

    if (!this._hasPendingRequests()) {
      // Cancel the repeating timer and exit early if we aren't waiting for
      // pongs or requests.
      this._requestTimeoutTimer.cancel();
      return;
    }

    let now = Date.now();

    // Set to true if at least one request timed out, or we're still waiting
    // for a pong after the request timeout.
    let requestTimedOut = false;

    if (this._lastPingTime > 0 &&
        now - this._lastPingTime > this._requestTimeout) {

      console.debug("timeOutRequests: Did not receive pong in time");
      requestTimedOut = true;

    } else {
      for (let [key, request] of this._pendingRequests) {
        let duration = now - request.ctime;
        // If any of the registration requests time out, all the ones after it
        // also made to fail, since we are going to be disconnecting the
        // socket.
        requestTimedOut |= duration > this._requestTimeout;
        if (requestTimedOut) {
          request.reject(new Error("Request timed out: " + key));
          this._pendingRequests.delete(key);
        }
      }
    }

    // The most likely reason for a pong or registration request timing out is
    // that the socket has disconnected. Best to reconnect.
    if (requestTimedOut) {
      this._reconnect();
    }
  },

  validServerURI: function(serverURI) {
    if (serverURI.scheme == "ws") {
      return !!prefs.get("testing.allowInsecureServerURL");
    }
    return serverURI.scheme == "wss";
  },

  get _UAID() {
    return prefs.get("userAgentID");
  },

  set _UAID(newID) {
    if (typeof(newID) !== "string") {
      console.warn("Got invalid, non-string UAID", newID,
        "Not updating userAgentID");
      return;
    }
    console.debug("New _UAID", newID);
    prefs.set("userAgentID", newID);
  },

  _ws: null,
  _pendingRequests: new Map(),
  _currentState: STATE_SHUT_DOWN,
  _requestTimeout: 0,
  _requestTimeoutTimer: null,
  _retryFailCount: 0,

  /**
   * According to the WS spec, servers should immediately close the underlying
   * TCP connection after they close a WebSocket. This causes wsOnStop to be
   * called with error NS_BASE_STREAM_CLOSED. Since the client has to keep the
   * WebSocket up, it should try to reconnect. But if the server closes the
   * WebSocket because it wants the client to back off, then the client
   * shouldn't re-establish the connection. If the server sends the backoff
   * close code, this field will be set to true in wsOnServerClose. It is
   * checked in wsOnStop.
   */
  _skipReconnect: false,

  /** Indicates whether the server supports Web Push-style message delivery. */
  _dataEnabled: false,

  /**
   * The last time the client sent a ping to the server. If non-zero, keeps the
   * request timeout timer active. Reset to zero when the server responds with
   * a pong or pending messages.
   */
  _lastPingTime: 0,

  /**
   * A one-shot timer used to ping the server, to avoid timing out idle
   * connections. Reset to the ping interval on each incoming message.
   */
  _pingTimer: null,

  /** A one-shot timer fired after the reconnect backoff period. */
  _backoffTimer: null,

  /**
   * Sends a message to the Push Server through an open websocket.
   * typeof(msg) shall be an object
   */
  _wsSendMessage: function(msg) {
    if (!this._ws) {
      console.warn("wsSendMessage: No WebSocket initialized.",
        "Cannot send a message");
      return;
    }
    msg = JSON.stringify(msg);
    console.debug("wsSendMessage: Sending message", msg);
    this._ws.sendMsg(msg);
  },

  init: function(options, mainPushService, serverURI) {
    console.debug("init()");

    this._mainPushService = mainPushService;
    this._serverURI = serverURI;
    // Filled in at connect() time
    this._broadcastListeners = null;

    // Override the default WebSocket factory function. The returned object
    // must be null or satisfy the nsIWebSocketChannel interface. Used by
    // the tests to provide a mock WebSocket implementation.
    if (options.makeWebSocket) {
      this._makeWebSocket = options.makeWebSocket;
    }

    this._requestTimeout = prefs.get("requestTimeout");

    return Promise.resolve();
  },

  _reconnect: function () {
    console.debug("reconnect()");
    this._shutdownWS(false);
    this._startBackoffTimer();
  },

  _shutdownWS: function(shouldCancelPending = true) {
    console.debug("shutdownWS()");

    if (this._currentState == STATE_READY) {
      prefs.ignore("userAgentID", this);
    }

    this._currentState = STATE_SHUT_DOWN;
    this._skipReconnect = false;

    if (this._wsListener) {
      this._wsListener._pushService = null;
    }
    try {
        this._ws.close(0, null);
    } catch (e) {}
    this._ws = null;

    this._lastPingTime = 0;

    if (this._pingTimer) {
      this._pingTimer.cancel();
    }

    if (shouldCancelPending) {
      this._cancelPendingRequests();
    }

    if (this._notifyRequestQueue) {
      this._notifyRequestQueue();
      this._notifyRequestQueue = null;
    }
  },

  uninit: function() {
    // All pending requests (ideally none) are dropped at this point. We
    // shouldn't have any applications performing registration/unregistration
    // or receiving notifications.
    this._shutdownWS();

    if (this._backoffTimer) {
      this._backoffTimer.cancel();
    }
    if (this._requestTimeoutTimer) {
      this._requestTimeoutTimer.cancel();
    }

    this._mainPushService = null;

    this._dataEnabled = false;
  },

  /**
   * How retries work: If the WS is closed due to a socket error,
   * _startBackoffTimer() is called. The retry timer is started and when
   * it times out, beginWSSetup() is called again.
   *
   * If we are in the middle of a timeout (i.e. waiting), but
   * a register/unregister is called, we don't want to wait around anymore.
   * _sendRequest will automatically call beginWSSetup(), which will cancel the
   * timer. In addition since the state will have changed, even if a pending
   * timer event comes in (because the timer fired the event before it was
   * cancelled), so the connection won't be reset.
   */
  _startBackoffTimer() {
    console.debug("startBackoffTimer()");

    // Calculate new timeout, but cap it to pingInterval.
    let retryTimeout = prefs.get("retryBaseInterval") *
                       Math.pow(2, this._retryFailCount);
    retryTimeout = Math.min(retryTimeout, prefs.get("pingInterval"));

    this._retryFailCount++;

    console.debug("startBackoffTimer: Retry in", retryTimeout,
      "Try number", this._retryFailCount);

    if (!this._backoffTimer) {
      this._backoffTimer = Cc["@mozilla.org/timer;1"]
                               .createInstance(Ci.nsITimer);
    }
    this._backoffTimer.init(this, retryTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /** Indicates whether we're waiting for pongs or requests. */
  _hasPendingRequests() {
    return this._lastPingTime > 0 || this._pendingRequests.size > 0;
  },

  /**
   * Starts the request timeout timer unless we're already waiting for a pong
   * or register request.
   */
  _startRequestTimeoutTimer() {
    if (this._hasPendingRequests()) {
      return;
    }
    if (!this._requestTimeoutTimer) {
      this._requestTimeoutTimer = Cc["@mozilla.org/timer;1"]
                                    .createInstance(Ci.nsITimer);
    }
    this._requestTimeoutTimer.init(this,
                                   this._requestTimeout,
                                   Ci.nsITimer.TYPE_REPEATING_SLACK);
  },

  /** Starts or resets the ping timer. */
  _startPingTimer() {
    if (!this._pingTimer) {
      this._pingTimer = Cc["@mozilla.org/timer;1"]
                          .createInstance(Ci.nsITimer);
    }
    this._pingTimer.init(this, prefs.get("pingInterval"),
                         Ci.nsITimer.TYPE_ONE_SHOT);
  },

  _makeWebSocket: function(uri) {
    if (!prefs.get("connection.enabled")) {
      console.warn("makeWebSocket: connection.enabled is not set to true.",
        "Aborting.");
      return null;
    }
    if (Services.io.offline) {
      console.warn("makeWebSocket: Network is offline.");
      return null;
    }
    let contractId = uri.scheme == "ws" ?
                     "@mozilla.org/network/protocol;1?name=ws" :
                     "@mozilla.org/network/protocol;1?name=wss";
    let socket = Cc[contractId].createInstance(Ci.nsIWebSocketChannel);

    socket.initLoadInfo(null, // aLoadingNode
                        Services.scriptSecurityManager.getSystemPrincipal(),
                        null, // aTriggeringPrincipal
                        Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                        Ci.nsIContentPolicy.TYPE_WEBSOCKET);

    return socket;
  },

  _beginWSSetup: function() {
    console.debug("beginWSSetup()");
    if (this._currentState != STATE_SHUT_DOWN) {
      console.error("_beginWSSetup: Not in shutdown state! Current state",
        this._currentState);
      return;
    }

    // Stop any pending reconnects scheduled for the near future.
    if (this._backoffTimer) {
      this._backoffTimer.cancel();
    }

    let uri = this._serverURI;
    if (!uri) {
      return;
    }
    let socket = this._makeWebSocket(uri);
    if (!socket) {
      return;
    }
    this._ws = socket.QueryInterface(Ci.nsIWebSocketChannel);

    console.debug("beginWSSetup: Connecting to", uri.spec);
    this._wsListener = new PushWebSocketListener(this);
    this._ws.protocol = "push-notification";

    try {
      // Grab a wakelock before we open the socket to ensure we don't go to
      // sleep before connection the is opened.
      this._ws.asyncOpen(uri, uri.spec, 0, this._wsListener, null);
      this._currentState = STATE_WAITING_FOR_WS_START;
    } catch(e) {
      console.error("beginWSSetup: Error opening websocket.",
        "asyncOpen failed", e);
      this._reconnect();
    }
  },

  connect: function(records, broadcastListeners) {
    console.debug("connect()", broadcastListeners);
    this._broadcastListeners = broadcastListeners;
    this._beginWSSetup();
  },

  isConnected: function() {
    return !!this._ws;
  },

  /**
   * Protocol handler invoked by server message.
   */
  _handleHelloReply: function(reply) {
    console.debug("handleHelloReply()");
    if (this._currentState != STATE_WAITING_FOR_HELLO) {
      console.error("handleHelloReply: Unexpected state", this._currentState,
        "(expected STATE_WAITING_FOR_HELLO)");
      this._shutdownWS();
      return;
    }

    if (typeof reply.uaid !== "string") {
      console.error("handleHelloReply: Received invalid UAID", reply.uaid);
      this._shutdownWS();
      return;
    }

    if (reply.uaid === "") {
      console.error("handleHelloReply: Received empty UAID");
      this._shutdownWS();
      return;
    }

    // To avoid sticking extra large values sent by an evil server into prefs.
    if (reply.uaid.length > 128) {
      console.error("handleHelloReply: UAID received from server was too long",
        reply.uaid);
      this._shutdownWS();
      return;
    }

    let sendRequests = () => {
      if (this._notifyRequestQueue) {
        this._notifyRequestQueue();
        this._notifyRequestQueue = null;
      }
      this._sendPendingRequests();
    };

    function finishHandshake() {
      this._UAID = reply.uaid;
      this._currentState = STATE_READY;
      prefs.observe("userAgentID", this);

      // Handle broadcasts received in response to the "hello" message.
      if (reply.broadcasts) {
        // The reply isn't technically a broadcast message, but it has
        // the shape of a broadcast message (it has a broadcasts field).
        this._mainPushService.receivedBroadcastMessage(reply);
      }

      this._dataEnabled = !!reply.use_webpush;
      if (this._dataEnabled) {
        this._mainPushService.getAllUnexpired().then(records =>
          Promise.all(records.map(record =>
            this._mainPushService.ensureCrypto(record).catch(error => {
              console.error("finishHandshake: Error updating record",
                record.keyID, error);
            })
          ))
        ).then(sendRequests);
      } else {
        sendRequests();
      }
    }

    // By this point we've got a UAID from the server that we are ready to
    // accept.
    //
    // We unconditionally drop all existing registrations and notify service
    // workers if we receive a new UAID. This ensures we expunge all stale
    // registrations if the `userAgentID` pref is reset.
    if (this._UAID != reply.uaid) {
      console.debug("handleHelloReply: Received new UAID");

      this._mainPushService.dropUnexpiredRegistrations()
          .then(finishHandshake.bind(this));

      return;
    }

    // otherwise we are good to go
    finishHandshake.bind(this)();
  },

  /**
   * Protocol handler invoked by server message.
   */
  _handleRegisterReply: function(reply) {
    console.debug("handleRegisterReply()");

    let tmp = this._takeRequestForReply(reply);
    if (!tmp) {
      return;
    }

    if (reply.status == 200) {
      try {
        Services.io.newURI(reply.pushEndpoint);
      }
      catch (e) {
        tmp.reject(new Error("Invalid push endpoint: " + reply.pushEndpoint));
        return;
      }

      let record = new PushRecordWebSocket({
        channelID: reply.channelID,
        pushEndpoint: reply.pushEndpoint,
        scope: tmp.record.scope,
        originAttributes: tmp.record.originAttributes,
        version: null,
        systemRecord: tmp.record.systemRecord,
        appServerKey: tmp.record.appServerKey,
        ctime: Date.now(),
      });
      tmp.resolve(record);
    } else {
      console.error("handleRegisterReply: Unexpected server response", reply);
      tmp.reject(new Error("Wrong status code for register reply: " +
        reply.status));
    }
  },

  _handleUnregisterReply(reply) {
    console.debug("handleUnregisterReply()");

    let request = this._takeRequestForReply(reply);
    if (!request) {
      return;
    }

    let success = reply.status === 200;
    request.resolve(success);
  },

  _handleDataUpdate: function(update) {
    let promise;
    if (typeof update.channelID != "string") {
      console.warn("handleDataUpdate: Discarding update without channel ID",
        update);
      return;
    }
    function updateRecord(record) {
      // Ignore messages that we've already processed. This can happen if the
      // connection drops between notifying the service worker and acking the
      // the message. In that case, the server will re-send the message on
      // reconnect.
      if (record.hasRecentMessageID(update.version)) {
        console.warn("handleDataUpdate: Ignoring duplicate message",
          update.version);
        return null;
      }
      record.noteRecentMessageID(update.version);
      return record;
    }
    if (typeof update.data != "string") {
      promise = this._mainPushService.receivedPushMessage(
        update.channelID,
        update.version,
        null,
        null,
        updateRecord
      );
    } else {
      let message = ChromeUtils.base64URLDecode(update.data, {
        // The Push server may append padding.
        padding: "ignore",
      });
      promise = this._mainPushService.receivedPushMessage(
        update.channelID,
        update.version,
        update.headers,
        message,
        updateRecord
      );
    }
    promise.then(status => {
      this._sendAck(update.channelID, update.version, status);
    }, err => {
      console.error("handleDataUpdate: Error delivering message", update, err);
      this._sendAck(update.channelID, update.version,
        Ci.nsIPushErrorReporter.ACK_DECRYPTION_ERROR);
    }).catch(err => {
      console.error("handleDataUpdate: Error acknowledging message", update,
        err);
    });
  },

  /**
   * Protocol handler invoked by server message.
   */
  _handleNotificationReply: function(reply) {
    console.debug("handleNotificationReply()");
    if (this._dataEnabled) {
      this._handleDataUpdate(reply);
      return;
    }

    if (typeof reply.updates !== 'object') {
      console.warn("handleNotificationReply: Missing updates", reply.updates);
      return;
    }

    console.debug("handleNotificationReply: Got updates", reply.updates);
    for (let i = 0; i < reply.updates.length; i++) {
      let update = reply.updates[i];
      console.debug("handleNotificationReply: Handling update", update);
      if (typeof update.channelID !== "string") {
        console.debug("handleNotificationReply: Invalid update at index",
          i, update);
        continue;
      }

      if (update.version === undefined) {
        console.debug("handleNotificationReply: Missing version", update);
        continue;
      }

      let version = update.version;

      if (typeof version === "string") {
        version = parseInt(version, 10);
      }

      if (typeof version === "number" && version >= 0) {
        // FIXME(nsm): this relies on app update notification being infallible!
        // eventually fix this
        this._receivedUpdate(update.channelID, version);
      }
    }
  },

  _handleBroadcastReply: function(reply) {
    this._mainPushService.receivedBroadcastMessage(reply);
  },

  reportDeliveryError(messageID, reason) {
    console.debug("reportDeliveryError()");
    let code = kDELIVERY_REASON_TO_CODE[reason];
    if (!code) {
      throw new Error('Invalid delivery error reason');
    }
    let data = {messageType: 'nack',
                version: messageID,
                code: code};
    this._queueRequest(data);
  },

  _sendAck(channelID, version, status) {
    console.debug("sendAck()");
    let code = kACK_STATUS_TO_CODE[status];
    if (!code) {
      throw new Error('Invalid ack status');
    }
    let data = {messageType: 'ack',
                updates: [{channelID: channelID,
                           version: version,
                           code: code}]};
    this._queueRequest(data);
  },

  _generateID: function() {
    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                          .getService(Ci.nsIUUIDGenerator);
    // generateUUID() gives a UUID surrounded by {...}, slice them off.
    return uuidGenerator.generateUUID().toString().slice(1, -1);
  },

  register(record) {
    console.debug("register() ", record);

    let data = {channelID: this._generateID(),
                messageType: "register"};

    if (record.appServerKey) {
      data.key = ChromeUtils.base64URLEncode(record.appServerKey, {
        // The Push server requires padding.
        pad: true,
      });
    }

    return this._sendRequestForReply(record, data).then(record => {
      if (!this._dataEnabled) {
        return record;
      }
      return PushCrypto.generateKeys()
        .then(([publicKey, privateKey]) => {
          record.p256dhPublicKey = publicKey;
          record.p256dhPrivateKey = privateKey;
          record.authenticationSecret = PushCrypto.generateAuthenticationSecret();
          return record;
        });
    });
  },

  unregister(record, reason) {
    console.debug("unregister() ", record, reason);

    return Promise.resolve().then(_ => {
      let code = kUNREGISTER_REASON_TO_CODE[reason];
      if (!code) {
        throw new Error('Invalid unregister reason');
      }
      let data = {channelID: record.channelID,
                  messageType: "unregister",
                  code: code};

      return this._sendRequestForReply(record, data);
    });
  },

  _queueStart: Promise.resolve(),
  _notifyRequestQueue: null,
  _queue: null,
  _enqueue: function(op) {
    console.debug("enqueue()");
    if (!this._queue) {
      this._queue = this._queueStart;
    }
    this._queue = this._queue
                    .then(op)
                    .catch(_ => {});
  },

  /** Sends a request to the server. */
  _send(data) {
    if (this._currentState != STATE_READY) {
      console.warn("send: Unexpected state; ignoring message",
        this._currentState);
      return;
    }
    if (!this._requestHasReply(data)) {
      this._wsSendMessage(data);
      return;
    }
    // If we're expecting a reply, check that we haven't cancelled the request.
    let key = this._makePendingRequestKey(data);
    if (!this._pendingRequests.has(key)) {
      console.log("send: Request cancelled; ignoring message", key);
      return;
    }
    this._wsSendMessage(data);
  },

  /** Indicates whether a request has a corresponding reply from the server. */
  _requestHasReply(data) {
    return data.messageType == "register" || data.messageType == "unregister";
  },

  /**
   * Sends all pending requests that expect replies. Called after the connection
   * is established and the handshake is complete.
   */
  _sendPendingRequests() {
    this._enqueue(_ => {
      for (let request of this._pendingRequests.values()) {
        this._send(request.data);
      }
    });
  },

  /** Queues an outgoing request, establishing a connection if necessary. */
  _queueRequest(data) {
    console.debug("queueRequest()", data);

    if (this._currentState == STATE_READY) {
      // If we're ready, no need to queue; just send the request.
      this._send(data);
      return;
    }

    // Otherwise, we're still setting up. If we don't have a request queue,
    // make one now.
    if (!this._notifyRequestQueue) {
      let promise = new Promise((resolve, reject) => {
        this._notifyRequestQueue = resolve;
      });
      this._enqueue(_ => promise);
    }

    let isRequest = this._requestHasReply(data);
    if (!isRequest) {
      // Don't queue requests, since they're stored in `_pendingRequests`, and
      // `_sendPendingRequests` will send them after reconnecting. Without this
      // check, we'd send requests twice.
      this._enqueue(_ => this._send(data));
    }

    if (!this._ws) {
      // This will end up calling notifyRequestQueue().
      this._beginWSSetup();
      // If beginWSSetup does not succeed to make ws, notifyRequestQueue will
      // not be call.
      if (!this._ws && this._notifyRequestQueue) {
        this._notifyRequestQueue();
        this._notifyRequestQueue = null;
      }
    }
  },

  _receivedUpdate: function(aChannelID, aLatestVersion) {
    console.debug("receivedUpdate: Updating", aChannelID, "->", aLatestVersion);

    this._mainPushService.receivedPushMessage(aChannelID, "", null, null, record => {
      if (record.version === null ||
          record.version < aLatestVersion) {
        console.debug("receivedUpdate: Version changed for", aChannelID,
          aLatestVersion);
        record.version = aLatestVersion;
        return record;
      }
      console.debug("receivedUpdate: No significant version change for",
        aChannelID, aLatestVersion);
      return null;
    }).then(status => {
      this._sendAck(aChannelID, aLatestVersion, status);
    }).catch(err => {
      console.error("receivedUpdate: Error acknowledging message", aChannelID,
        aLatestVersion, err);
    });
  },

  // begin Push protocol handshake
  _wsOnStart: function(context) {
    console.debug("wsOnStart()");

    if (this._currentState != STATE_WAITING_FOR_WS_START) {
      console.error("wsOnStart: NOT in STATE_WAITING_FOR_WS_START. Current",
        "state", this._currentState, "Skipping");
      return;
    }

    let data = {
      messageType: "hello",
      broadcasts: this._broadcastListeners,
      use_webpush: true,
    };

    if (this._UAID) {
      data.uaid = this._UAID;
    }

    this._wsSendMessage(data);
    this._currentState = STATE_WAITING_FOR_HELLO;
  },

  /**
   * This statusCode is not the websocket protocol status code, but the TCP
   * connection close status code.
   *
   * If we do not explicitly call ws.close() then statusCode is always
   * NS_BASE_STREAM_CLOSED, even on a successful close.
   */
  _wsOnStop: function(context, statusCode) {
    console.debug("wsOnStop()");

    if (statusCode != Cr.NS_OK && !this._skipReconnect) {
      console.debug("wsOnStop: Reconnecting after socket error", statusCode);
      this._reconnect();
      return;
    }

    this._shutdownWS();
  },

  _wsOnMessageAvailable: function(context, message) {
    console.debug("wsOnMessageAvailable()", message);

    // Clearing the last ping time indicates we're no longer waiting for a pong.
    this._lastPingTime = 0;

    let reply;
    try {
      reply = JSON.parse(message);
    } catch(e) {
      console.warn("wsOnMessageAvailable: Invalid JSON", message, e);
      return;
    }

    // If we receive a message, we know the connection succeeded. Reset the
    // connection attempt and ping interval counters.
    this._retryFailCount = 0;

    let doNotHandle = false;
    if ((message === '{}') ||
        (reply.messageType === undefined) ||
        (reply.messageType === "ping") ||
        (typeof reply.messageType != "string")) {
      console.debug("wsOnMessageAvailable: Pong received");
      doNotHandle = true;
    }

    // Reset the ping timer.  Note: This path is executed at every step of the
    // handshake, so this timer does not need to be set explicitly at startup.
    this._startPingTimer();

    // If it is a ping, do not handle the message.
    if (doNotHandle) {
      return;
    }

    // A whitelist of protocol handlers. Add to these if new messages are added
    // in the protocol.
    let handlers = ["Hello", "Register", "Unregister", "Notification", "Broadcast"];

    // Build up the handler name to call from messageType.
    // e.g. messageType == "register" -> _handleRegisterReply.
    let handlerName = reply.messageType[0].toUpperCase() +
                      reply.messageType.slice(1).toLowerCase();

    if (!handlers.includes(handlerName)) {
      console.warn("wsOnMessageAvailable: No whitelisted handler", handlerName,
        "for message", reply.messageType);
      return;
    }

    let handler = "_handle" + handlerName + "Reply";

    if (typeof this[handler] !== "function") {
      console.warn("wsOnMessageAvailable: Handler", handler,
        "whitelisted but not implemented");
      return;
    }

    this[handler](reply);
  },

  /**
   * The websocket should never be closed. Since we don't call ws.close(),
   * _wsOnStop() receives error code NS_BASE_STREAM_CLOSED (see comment in that
   * function), which calls reconnect and re-establishes the WebSocket
   * connection.
   *
   * If the server requested that we back off, we won't reconnect until the
   * next network state change event, or until we need to send a new register
   * request.
   */
  _wsOnServerClose: function(context, aStatusCode, aReason) {
    console.debug("wsOnServerClose()", aStatusCode, aReason);

    if (aStatusCode == kBACKOFF_WS_STATUS_CODE) {
      console.debug("wsOnServerClose: Skipping automatic reconnect");
      this._skipReconnect = true;
    }
  },

  /**
   * Rejects all pending register requests with errors.
   */
  _cancelPendingRequests() {
    for (let request of this._pendingRequests.values()) {
      request.reject(new Error("Request aborted"));
    }
    this._pendingRequests.clear();
  },

  /** Creates a case-insensitive map key for a request that expects a reply. */
  _makePendingRequestKey(data) {
    return (data.messageType + "|" + data.channelID).toLowerCase();
  },

  /** Sends a request and waits for a reply from the server. */
  _sendRequestForReply(record, data) {
    return Promise.resolve().then(_ => {
      // start the timer since we now have at least one request
      this._startRequestTimeoutTimer();

      let key = this._makePendingRequestKey(data);
      if (!this._pendingRequests.has(key)) {
        let request = {
          data: data,
          record: record,
          ctime: Date.now(),
        };
        request.promise = new Promise((resolve, reject) => {
          request.resolve = resolve;
          request.reject = reject;
        });
        this._pendingRequests.set(key, request);
        this._queueRequest(data);
      }

      return this._pendingRequests.get(key).promise;
    });
  },

  /** Removes and returns a pending request for a server reply. */
  _takeRequestForReply(reply) {
    if (typeof reply.channelID !== "string") {
      return null;
    }
    let key = this._makePendingRequestKey(reply);
    let request = this._pendingRequests.get(key);
    if (!request) {
      return null;
    }
    this._pendingRequests.delete(key);
    if (!this._hasPendingRequests()) {
      this._requestTimeoutTimer.cancel();
    }
    return request;
  },

  sendSubscribeBroadcast(serviceId, version) {
    let data = {
      messageType: "broadcast_subscribe",
      broadcasts: {
        [serviceId]: version
      },
    };

    this._queueRequest(data);
  },
};

function PushRecordWebSocket(record) {
  PushRecord.call(this, record);
  this.channelID = record.channelID;
  this.version = record.version;
}

PushRecordWebSocket.prototype = Object.create(PushRecord.prototype, {
  keyID: {
    get() {
      return this.channelID;
    },
  },
});

PushRecordWebSocket.prototype.toSubscription = function() {
  let subscription = PushRecord.prototype.toSubscription.call(this);
  subscription.version = this.version;
  return subscription;
};
