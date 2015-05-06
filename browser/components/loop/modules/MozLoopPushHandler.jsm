/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Timer.jsm");

const { MozLoopService } = Cu.import("resource:///modules/loop/MozLoopService.jsm", {});
const consoleLog = MozLoopService.log;

this.EXPORTED_SYMBOLS = ["MozLoopPushHandler"];

const CONNECTION_STATE_CLOSED = 0;
const CONNECTION_STATE_CONNECTING = 1;
const CONNECTION_STATE_OPEN = 2;

const SERVICE_STATE_OFFLINE = 0;
const SERVICE_STATE_PENDING = 1;
const SERVICE_STATE_ACTIVE = 2;

function PushSocket(webSocket = null) {
  this._websocket = webSocket;
}

PushSocket.prototype = {

  /**
   * Open push-notification websocket.
   *
   * @param {String} pushUri
   * @param {Function} onMsg(aMsg) callback receives any incoming messages
   *                   aMsg is constructed from the json payload; both
   *                   text and binary message reception are mapped to this
   *                   callback.
   * @param {Function} onStart called when the socket is connected
   * @param {Function} onClose(aCode, aReason) called when the socket closes;
   *                   both near and far side close events map to this
   *                   callback.
   *                   aCode is any status code returned on close
   *                   aReason is any string returned on close
   */

  connect: function(pushUri, onMsg, onStart, onClose) {
    if (!pushUri || !onMsg || !onStart || !onClose) {
      throw new Error("PushSocket: missing required parameter(s):"
                      (pushUri ? "" : " pushUri") +
                      (onMsg ? "" : " onMsg") +
                      (onStart ? "" : " onStart") +
                      (onClose ? "" : " onClose"));
    }

    this._onMsg = onMsg;
    this._onStart = onStart;
    this._onClose = onClose;

    if (!this._websocket) {
      this._websocket = Cc["@mozilla.org/network/protocol;1?name=wss"]
                          .createInstance(Ci.nsIWebSocketChannel);
      this._websocket.initLoadInfo(null, // aLoadingNode
                                   Services.scriptSecurityManager.getSystemPrincipal(),
                                   null, // aTriggeringPrincipal
                                   Ci.nsILoadInfo.SEC_NORMAL,
                                   Ci.nsIContentPolicy.TYPE_WEBSOCKET);
    }

    let uri = Services.io.newURI(pushUri, null, null);
    this._websocket.protocol = "push-notification";
    this._websocket.asyncOpen(uri, pushUri, this, null);
  },

  /**
   * nsIWebSocketListener method, handles the start of the websocket stream.
   *
   * @param {nsISupports} aContext Not used
   */
  onStart: function() {
    this._socketOpen = true;
    this._onStart();
  },

  /**
   * nsIWebSocketListener method, called when the websocket is closed locally.
   *
   * @param {nsISupports} aContext Not used
   * @param {nsresult} aStatusCode
   */
  onStop: function(aContext, aStatusCode) {
    this._socketOpen = false;
    this._onClose(aStatusCode, "websocket onStop");
  },

  /**
   * nsIWebSocketListener method, called when the websocket is closed
   * by the far end.
   *
   * @param {nsISupports} aContext Not used
   * @param {integer} aCode the websocket closing handshake close code
   * @param {String} aReason the websocket closing handshake close reason
   */
  onServerClose: function(aContext, aCode, aReason) {
    this._socketOpen = false;
    this._onClose(aCode, aReason);
  },

  /**
   * nsIWebSocketListener method, called when the websocket receives
   * a text message (normally json encoded).
   *
   * @param {nsISupports} aContext Not used
   * @param {String} aMsg The message data
   */
  onMessageAvailable: function(aContext, aMsg) {
    consoleLog.log("PushSocket: Message received: ", aMsg);
    if (!this._socketOpen) {
      consoleLog.error("Message received in Winsocket closed state");
      return;
    }

    try {
      this._onMsg(JSON.parse(aMsg));
    }
    catch (error) {
      consoleLog.error("PushSocket: error parsing message payload - ", error);
    }
  },

  /**
   * nsIWebSocketListener method, called when the websocket receives a binary message.
   * This class assumes that it is connected to a SimplePushServer and therefore treats
   * the message payload as json encoded.
   *
   * @param {nsISupports} aContext Not used
   * @param {String} aMsg The message data
   */
  onBinaryMessageAvailable: function(aContext, aMsg) {
    consoleLog.log("PushSocket: Binary message received: ", aMsg);
    if (!this._socketOpen) {
      consoleLog.error("PushSocket: message receive in Winsocket closed state");
      return;
    }

    try {
      this._onMsg(JSON.parse(aMsg));
    }
    catch (error) {
      consoleLog.error("PushSocket: error parsing message payload - ", error);
    }
  },

  /**
   * Create a JSON encoded message payload and send via websocket.
   *
   * @param {Object} aMsg Message to send.
   *
   * @returns {Boolean} true if message has been sent, false otherwise
   */
  send: function(aMsg) {
    if (!this._socketOpen) {
      consoleLog.error("PushSocket: attempt to send before websocket is open");
      return false;
    }

    let msg;
    try {
      msg = JSON.stringify(aMsg);
    }
    catch (error) {
      consoleLog.error("PushSocket: JSON generation error - ", error);
      return false;
    }

    try {
      this._websocket.sendMsg(msg);
      consoleLog.log("PushSocket: Message sent: ", msg);
    }
    // guard against the case that the websocket has closed before this call.
    catch (e) {
      consoleLog.warn("PushSocket: websocket send error", e);
      return false;
    }

    return true;
  },

  /**
   * Close the websocket.
   */
  close: function() {
    if (!this._socketOpen) {
      return;
    }

    this._socketOpen = false;
    consoleLog.info("PushSocket: websocket closing");

    // Do not pass through any callbacks after this point.
    this._onStart = function() {};
    this._onMsg = this._onStart;
    this._onClose = this._onStart;

    try {
      this._websocket.close(this._websocket.CLOSE_NORMAL);
    }
    catch (e) {}
  }
};


/**
 * Create a RetryManager object. Class to handle retrying a UserAgent
 * to PushServer request following a retry back-off scheme managed by
 * this class. The current delay mechanism is to double the delay
 * each time an operation to be retried until a maximum is met.
 *
 * @param {Integer} startDelay The initial delay interval in milliseconds.
 * @param {Integer} maxDelay Maximum time delay value in milliseconds.
 */
function RetryManager (startDelay, maxDelay) {
  if (!startDelay || !maxDelay) {
    throw new Error("RetryManager: missing required parameters(s)" +
                     (startDelay ? "" : " startDelay") +
                     (maxDelay ? "" : " maxDelay"));
  }

  this._startDelay = startDelay;
  // The maximum delay cannot be less than the starting delay.
  this._maxDelay = maxDelay > startDelay ? maxDelay : startDelay;
}

RetryManager.prototype = {
  /**
   * Method to handle retrying a UserAgent to PushServer request.
   *
   * @param {Function} delayedOp Function to call after current delay is satisfied
   */
  retry: function(delayedOp) {
    if (!this._timeoutID) {
      this._retryDelay = this._startDelay;
    } else {
      clearTimeout(this._timeoutID);
      let nextDelay = this._retryDelay * 2;
      this._retryDelay = nextDelay > this._maxDelay ? this._maxDelay : nextDelay;
    }

    this._timeoutID = setTimeout(delayedOp, this._retryDelay);
    consoleLog.log("PushHandler: retry delay set for ", this._retryDelay);
  },

  /**
   * Method used to reset the delay back-off logic and clear any currently
   * running delay timeout.
   */
  reset: function() {
    if (this._timeoutID) {
      clearTimeout(this._timeoutID);
      this._timeoutID = null;
    }
  }
};

/**
 * Create a PingMonitor object. An object instance will periodically execute
 * a ping send function and if not reset, will then execute an error function.
 *
 * @param {Function} pingFunc Function that is called after a ping interval
 *                   has expired without being restart.
 * @param {Function} onTimeout Function that is called after a ping timeout
 *                   interval has expired without restart being called.
 * @param {Integer} interval Timeout value in milliseconds between successive
 *                  pings or between the last restart call and a ping.
 *                  When this interval expires, pingFunc is called and the
 *                  timeout interval is started.
 * @param {Integer} timeout Timeout value in milliseconds between a call to
 *                  pingFunc and a call to onTimeout unless restart is called.
 *                  Restart will begin the ping timeout interval again.
 */
function PingMonitor(pingFunc, onTimeout, interval, timeout) {
  if (!pingFunc || !onTimeout || !interval || !timeout) {
    throw new Error("PingMonitor: missing required parameters");
  }
  this._onTimeout = onTimeout;
  this._pingFunc = pingFunc;
  this._pingInterval = interval;
  this._pingTimeout = timeout;
}

PingMonitor.prototype = {
  /**
   * Function to restart the ping timeout and cancel any current timeout operation.
   */
  restart: function () {
    consoleLog.info("PushHandler: ping timeout restart");
    this.stop();
    this._pingTimerID = setTimeout(() => { this._pingSend(); }, this._pingInterval);
  },

  /**
   * Function to stop the PingMonitor.
   */
  stop: function() {
    if (this._pingTimerID){
      clearTimeout(this._pingTimerID);
      this._pingTimerID = undefined;
    }
  },

  _pingSend: function () {
    consoleLog.info("PushHandler: ping sent");
    this._pingTimerID = setTimeout(this._onTimeout, this._pingTimeout);
    this._pingFunc();
  }
};


/**
 * We don't have push notifications on desktop currently, so this is a
 * workaround to get them going for us.
 */
let MozLoopPushHandler = {
  // This is the uri of the push server.
  pushServerUri: undefined,
  // Records containing the registration and notification callbacks indexed by channelID.
  // Each channel will be registered with the PushServer.
  channels: new Map(),
  // This is the UserAgent UUID assigned by the PushServer
  uaID: undefined,
  // Each successfully registered channelID is used as a key to hold its pushEndpoint URL.
  registeredChannels: {},
  // Push protocol state variable
  serviceState: SERVICE_STATE_OFFLINE,
  // Websocket connection state variable
  connectionState: CONNECTION_STATE_CLOSED,
  // Contains channels that need to be registered with the PushServer
  _channelsToRegister: [],

  get _startRetryDelay_ms() {
    try {
      return Services.prefs.getIntPref("loop.retry_delay.start");
    }
    catch (e) {
      return 60000; // 1 minute
    }
  },

  get _maxRetryDelay_ms() {
    try {
      return Services.prefs.getIntPref("loop.retry_delay.limit");
    }
    catch (e) {
      return 300000; // 5 minutes
    }
  },

  get _pingInterval_ms() {
    try {
      return Services.prefs.getIntPref("loop.ping.interval");
    }
    catch (e) {
      return 18000000; // 30 minutes
    }
  },

  get _pingTimeout_ms() {
    try {
      return Services.prefs.getIntPref("loop.ping.timeout");
    }
    catch (e) {
      return 10000; // 10 seconds
    }
  },

   /**
    * Inializes the PushHandler and opens a socket with the PushServer.
    * It will automatically say hello and register any channels
    * that are found in the work queue at that point.
    *
    * @param {Object} options Set of configuration options. Currently,
    *                 the only option is mocketWebSocket which will be
    *                 used for testing.
    */
  initialize: function(options = {}) {
    consoleLog.info("PushHandler: initialize options = ", options);
    if (Services.io.offline) {
      consoleLog.warn("PushHandler: IO offline");
      return false;
    }

    if (this._initDone) {
      return true;
    }

    this._initDone = true;
    this._retryManager = new RetryManager(this._startRetryDelay_ms,
                                          this._maxRetryDelay_ms);
    // Send an empty json payload as a ping.
    // Close the websocket and re-open if a timeout occurs.
    this._pingMonitor = new PingMonitor(() => this._pushSocket.send({}),
                                        () => this._restartConnection(),
                                        this._pingInterval_ms,
                                        this._pingTimeout_ms);

    if ("mockWebSocket" in options) {
      this._mockWebSocket = options.mockWebSocket;
    }

    this._openSocket();
    return true;
  },

  /**
   * Reset and clear PushServer connection.
   * Returns MozLoopPushHandler to pre-initialized state.
   */
  shutdown: function() {
    consoleLog.info("PushHandler: shutdown");
    if (!this._initDone) {
      return;
    }

    this._initDone = false;
    this._retryManager.reset();
    this._pingMonitor.stop();

    // Un-register each active notification channel
    if (this.connectionState === CONNECTION_STATE_OPEN) {
      Object.keys(this.registeredChannels).forEach((id) => {
        let unRegMsg = {messageType: "unregister",
                        channelID: id};
        this._pushSocket.send(unRegMsg);
      });
      this.registeredChannels = {};
    }

    this.connectionState = CONNECTION_STATE_CLOSED;
    this.serviceState = SERVICE_STATE_OFFLINE;
    this._pushSocket.close();
    this._pushSocket = undefined;
    // NOTE: this PushSocket instance will not be released until at least
    // the websocket referencing it as an nsIWebSocketListener is released.
    this.channels.clear();
    this.uaID = undefined;
    this.pushUrl = undefined;
    this.pushServerUri = undefined;
  },

   /**
    * Assign a channel to be registered with the PushServer
    * This channel will be registered when a connection to the PushServer
    * has been established or re-registered after a connection has been lost
    * and re-established. Calling this more than once for the same channel
    * has no additional effect.
    *
    * onRegistered callback parameters:
    * - {String|null} err: Encountered error, if any
    * - {String} url: The push url obtained from the server
    * - {String} channelID The channelID on which the notification was sent.
    *
    * onNotification parameters:
    * - {String} version The version string received from the push server for
    *                    the notification.
    * - {String} channelID The channelID on which the notification was sent.
    *
    * @param {String} channelID Channel ID to use in registration.
    *
    * @param {Function} onRegistered Callback to be called once we are
    *                   registered.
    *                   NOTE: This function can be called multiple times if
    *                   the PushServer generates new pushURLs due to
    *                   re-registration due to network loss or PushServer
    *                   initiated re-assignment.
    * @param {Function} onNotification Callback to be called when a
    *                   push notification is received (may be called multiple
    *                   times).
    */
  register: function(channelID, onRegistered, onNotification) {
    if (!channelID || !onRegistered || !onNotification) {
      throw new Error("missing required parameter(s):" +
                      (channelID ? "" : " channelID") +
                      (onRegistered ? "" : " onRegistered") +
                      (onNotification ? "" : " onNotification"));
    }

    consoleLog.info("PushHandler: channel registration: ", channelID);
    if (this.channels.has(channelID)) {
      // If this channel has an active registration with the PushServer
      // call the onRegister callback with the URL.
      if (this.registeredChannels[channelID]) {
        onRegistered(null, this.registeredChannels[channelID], channelID);
      }
      // Update the channel record.
      this.channels.set(channelID, {onRegistered: onRegistered,
                        onNotification: onNotification});
      return;
    }

    this.channels.set(channelID, {onRegistered: onRegistered,
                                  onNotification: onNotification});
    this._channelsToRegister.push(channelID);
    this._registerChannels();
  },

  /**
   * Un-register a notification channel.
   *
   * @param {String} channelID Notification channel ID.
   */
  unregister: function(channelID) {
    consoleLog.info("MozLoopPushHandler: un-register channel ", channelID);
    if (!this.channels.has(channelID)) {
      return;
    }

    this.channels.delete(channelID);

    if (this.registeredChannels[channelID]) {
      delete this.registeredChannels[channelID];
      if (this.connectionState === CONNECTION_STATE_OPEN) {
        this._pushSocket.send({messageType: "unregister",
                               channelID: channelID});
      }
    }
  },

  /**
   * Handles the start of the websocket stream.
   * Sends a hello message to the server.
   *
   */
  _onStart: function() {
    consoleLog.info("PushHandler: websocket open, sending 'hello' to PushServer");
    this.connectionState = CONNECTION_STATE_OPEN;
    // If a uaID has already been assigned, assume this is a re-connect;
    // send the uaID and channelIDs in order to re-synch with the
    // PushServer. The PushServer does not need to accept the existing channelIDs
    // and may issue new channelIDs along with new pushURLs.
    this.serviceState = SERVICE_STATE_PENDING;
    let helloMsg = {
      messageType: "hello",
      uaid: this.uaID || "",
      channelIDs: this.uaID ? Object.keys(this.registeredChannels) : []
    };
    // The Simple PushServer spec does not allow a retry of the Hello handshake but requires that the socket
    // be closed and another socket openned in order to re-attempt the handshake.
    // Here, the retryManager is not set up to retry the sending another 'hello' message: the timeout will
    // trigger closing the websocket and starting the connection again from the start.
    this._retryManager.reset();
    this._retryManager.retry(() => this._restartConnection());
    this._pushSocket.send(helloMsg);
  },

  /**
   * Handles websocket close callbacks.
   *
   * This method will continually try to re-establish a connection
   * to the PushServer unless shutdown has been called.
   */
  _onClose: function(aCode, aReason) {
    this._pingMonitor.stop();

    switch (this.connectionState) {
    case CONNECTION_STATE_OPEN:
        this.connectionState = CONNECTION_STATE_CLOSED;
        consoleLog.info("PushHandler: websocket closed: begin reconnect - ", aCode);
        // The first retry is immediate
        this._retryManager.reset();
        this._openSocket();
        break;

      case CONNECTION_STATE_CONNECTING:
        // Wait before re-attempting to open the websocket.
        consoleLog.info("PushHandler: websocket closed: delay and retry - ", aCode);
        this._retryManager.retry(() => this._openSocket());
        break;
     }
   },

  /**
   * Listener method, called when the websocket receives a message.
   *
   * @param {Object} aMsg The message data
   */
  _onMsg: function(aMsg) {
    // If an error property exists in the message object ignore the other
    // properties.
    if (aMsg.error) {
      consoleLog.error("PushHandler: received error response msg: ", aMsg.error);
      return;
    }

    // The recommended response to a ping message when the push server has nothing
    // else to send is a blank JSON message body: {}
    if (!aMsg.messageType && this.serviceState === SERVICE_STATE_ACTIVE) {
      // Treat this as a ping response
      this._pingMonitor.restart();
      return;
    }

    switch(aMsg.messageType) {
      case "hello":
        this._onHello(aMsg);
        break;

      case "register":
        this._onRegister(aMsg);
        break;

      case "notification":
        this._onNotification(aMsg);
        break;

      default:
        consoleLog.warn("PushHandler: unknown message type = ", aMsg.messageType);
        if (this.serviceState === SERVICE_STATE_ACTIVE) {
          // Treat this as a ping response
          this._pingMonitor.restart();
        }
        break;
     }
   },

  /**
   * Handles hello message.
   *
   * This method will parse the hello response from the PushServer
   * and determine whether registration is necessary.
   *
   * @param {aMsg} hello message body
   */
  _onHello: function(aMsg) {
    if (this.serviceState !== SERVICE_STATE_PENDING) {
      consoleLog.error("PushHandler: extra 'hello' response received from PushServer");
      return;
    }

    // Clear any pending timeout that will restart the connection.
    this._retryManager.reset();
    this.serviceState = SERVICE_STATE_ACTIVE;
    consoleLog.info("PushHandler: 'hello' handshake complete");
    // Start the PushServer ping monitor
    this._pingMonitor.restart();
    // If a new uaID is received, then any previous channel registrations
    // are no longer valid and a Registration request is generated.
    if (this.uaID !== aMsg.uaid) {
      consoleLog.log("PushHandler: registering all channels");
      this.uaID = aMsg.uaid;
      // Re-register all channels.
      this._channelsToRegister = [...this.channels.keys()];
      this.registeredChannels = {};
    }
    // Allow queued registrations to start (or all if cleared above).
    this._registerChannels();
  },

  /**
   * Handles notification message.
   *
   * This method will parse the Array of updates and trigger
   * the callback of any registered channel.
   * This method will construct an ack message containing
   * a set of channel version update notifications.
   *
   * @param {aMsg} notification message body
   */
  _onNotification: function(aMsg) {
    if (this.serviceState !== SERVICE_STATE_ACTIVE ||
       this.registeredChannels.length === 0) {
      // Treat reception of a notification before handshake and registration
      // are complete as a fatal error.
      consoleLog.error("PushHandler: protocol error - notification received in wrong state");
      this._restartConnection();
      return;
    }

    this._pingMonitor.restart();
    if (Array.isArray(aMsg.updates) && aMsg.updates.length > 0) {
      let ackChannels = [];
      aMsg.updates.forEach(update => {
        if (update.channelID in this.registeredChannels) {
          consoleLog.log("PushHandler: notification: version = ", update.version,
                         ", channelID = ", update.channelID);
          this.channels.get(update.channelID)
            .onNotification(update.version, update.channelID);
          ackChannels.push(update);
        } else {
          consoleLog.error("PushHandler: notification received for unknown channelID: ",
                           update.channelID);
        }
      });

      consoleLog.log("PushHandler: PusherServer 'ack': ", ackChannels);
      this._pushSocket.send({messageType: "ack",
                             updates: ackChannels});
     }
   },

  /**
   * Handles the PushServer registration response.
   *
   * @param {Object} msg PushServer to UserAgent registration response (parsed from JSON).
   */
  _onRegister: function(msg) {
    if (this.serviceState !== SERVICE_STATE_ACTIVE ||
        msg.channelID != this._pendingChannelID) {
      // Treat reception of a register response outside of a completed handshake
      // or for a channelID not currently pending a response
      // as an indication that the connections should be reset.
      consoleLog.error("PushHandler: registration protocol error");
      this._restartConnection();
      return;
    }

    this._retryManager.reset();
    this._pingMonitor.restart();

    switch (msg.status) {
      case 200:
        consoleLog.info("PushHandler: channel registered: ", msg.channelID);
        this.registeredChannels[msg.channelID] = msg.pushEndpoint;
        this.channels.get(msg.channelID)
          .onRegistered(null, msg.pushEndpoint, msg.channelID);
        this._registerNext();
        break;

      case 500:
        consoleLog.info("PushHandler: eeceived a 500 retry response from the PushServer: ",
                        msg.channelID);
        // retry the registration request after a suitable delay
        this._retryManager.retry(() => this._sendRegistration(msg.channelID));
        break;

      case 409:
        consoleLog.error("PushHandler: received a 409 response from the PushServer: ",
                         msg.channelID);
        this.channels.get(this._pendingChannelID).onRegistered("409");
        // Remove this channel from the channel list.
        this.channels.delete(this._pendingChannelID);
        this._registerNext();
        break;

      default:
        consoleLog.error("PushHandler: received error ", msg.status,
                         " from the PushServer: ", msg.channelID);
        this.channels.get(this._pendingChannelID).onRegistered(msg.status);
        this.channels.delete(this._pendingChannelID);
        this._registerNext();
        break;
    }
  },

  /**
   * Attempts to open a websocket.
   *
   * A new websocket interface is used each time. If an onStop callback
   * was received, calling asyncOpen() on the same interface will
   * trigger an "already open socket" exception even though the channel
   * is logically closed.
   */
  _openSocket: function() {
    this.connectionState = CONNECTION_STATE_CONNECTING;
    // For tests, use the mock instance.
    this._pushSocket = new PushSocket(this._mockWebSocket);

    let performOpen = () => {
      consoleLog.info("PushHandler: attempt to open websocket to PushServer: ", this.pushServerUri);
      this._pushSocket.connect(this.pushServerUri,
                               (aMsg) => this._onMsg(aMsg),
                               () => this._onStart(),
                               (aCode, aReason) => this._onClose(aCode, aReason));
    };

    let pushServerURLFetchError = () => {
      consoleLog.warn("PushHandler: Could not retrieve push server URL from Loop server, will retry");
      this._pushSocket = undefined;
      this._retryManager.retry(() => this._openSocket());
      return;
    };

    try {
      this.pushServerUri = Services.prefs.getCharPref("loop.debug.pushserver");
    }
    catch (e) {}

    if (!this.pushServerUri) {
      // Get push server to use from the Loop server
      let pushUrlEndpoint = Services.prefs.getCharPref("loop.server") + "/push-server-config";
      let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
                  createInstance(Ci.nsIXMLHttpRequest);
      req.open("GET", pushUrlEndpoint);
      req.onload = () => {
        if (req.status >= 200 && req.status < 300) {
          let pushServerConfig;
          try {
            pushServerConfig = JSON.parse(req.responseText);
          } catch (e) {
            consoleLog.warn("PushHandler: Error parsing JSON response for push server URL");
            pushServerURLFetchError();
          }
          if (pushServerConfig.pushServerURI) {
            this._retryManager.reset();
            this.pushServerUri = pushServerConfig.pushServerURI;
            performOpen();
          } else {
            consoleLog.warn("PushHandler: push server URL config lacks pushServerURI parameter");
            pushServerURLFetchError();
          }
        } else {
          consoleLog.warn("PushHandler: push server URL retrieve error: " + req.status);
          pushServerURLFetchError();
        }
      };
      req.onerror = pushServerURLFetchError;
      req.send();
    } else {
      // this.pushServerUri already set -- just open the channel
      performOpen();
    }
  },

  /**
    * Closes websocket and begins re-establishing a connection with the PushServer
    */
  _restartConnection: function() {
    this._retryManager.reset();
    this._pingMonitor.stop();
    this.serviceState = SERVICE_STATE_OFFLINE;
    this._pendingChannelID = null;

    if (this.connectionState === CONNECTION_STATE_OPEN) {
      // Close the current PushSocket and start the operation to open a new one.
      this.connectionState = CONNECTION_STATE_CLOSED;
      this._pushSocket.close();
      consoleLog.warn("PushHandler: connection error: re-establishing connection to PushServer");
      this._openSocket();
    }
  },

  /**
   * Begins registering the channelIDs with the PushServer
   */
  _registerChannels: function() {
    // Hold off registration operation until handshake is complete.
    // If a registration cycle is in progress, do nothing.
    if (this.serviceState !== SERVICE_STATE_ACTIVE ||
       this._pendingChannelID) {
      return;
    }
    this._registerNext();
  },

  /**
   * Gets the next channel to register from the worklist and kicks of its registration
   */
  _registerNext: function() {
    this._pendingChannelID = this._channelsToRegister.pop();
    this._sendRegistration(this._pendingChannelID);
  },

  /**
   * Handles registering a service
   *
   * @param {string} channelID - identification token to use in registration for this channel.
   */
  _sendRegistration: function(channelID) {
    if (channelID) {
      this._pushSocket.send({messageType: "register",
                             channelID: channelID});
    }
  }
};
