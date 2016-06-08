/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set services.push.debug.
var gDebuggingEnabled = false;

function debug(s) {
  if (gDebuggingEnabled)
    dump("-*- PushService.jsm: " + s + "\n");
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.importGlobalProperties(["indexedDB"]);

XPCOMUtils.defineLazyServiceGetter(this, "gDNSService",
                                   "@mozilla.org/network/dns-service;1",
                                   "nsIDNSService");

XPCOMUtils.defineLazyModuleGetter(this, "AlarmService",
                                  "resource://gre/modules/AlarmService.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gPowerManagerService",
                                   "@mozilla.org/power/powermanagerservice;1",
                                   "nsIPowerManagerService");

var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(Ci.nsIThreadManager);

this.EXPORTED_SYMBOLS = ["PushService"];

const prefs = new Preferences("services.push.");
// Set debug first so that all debugging actually works.
gDebuggingEnabled = prefs.get("debug");

const kPUSHDB_DB_NAME = "push";
const kPUSHDB_DB_VERSION = 1; // Change this if the IndexedDB format changes
const kPUSHDB_STORE_NAME = "push";

const kUDP_WAKEUP_WS_STATUS_CODE = 4774;  // WebSocket Close status code sent
                                          // by server to signal that it can
                                          // wake client up using UDP.

const kCHILD_PROCESS_MESSAGES = ["Push:Register", "Push:Unregister",
                                 "Push:Registrations"];

// This is a singleton
this.PushDB = function PushDB() {
  debug("PushDB()");

  // set the indexeddb database
  this.initDBHelper(kPUSHDB_DB_NAME, kPUSHDB_DB_VERSION,
                    [kPUSHDB_STORE_NAME]);
};

this.PushDB.prototype = {
  __proto__: IndexedDBHelper.prototype,

  upgradeSchema: function(aTransaction, aDb, aOldVersion, aNewVersion) {
    debug("PushDB.upgradeSchema()");

    let objectStore = aDb.createObjectStore(kPUSHDB_STORE_NAME,
                                            { keyPath: "channelID" });

    // index to fetch records based on endpoints. used by unregister
    objectStore.createIndex("pushEndpoint", "pushEndpoint", { unique: true });

    // index to fetch records per manifest, so we can identify endpoints
    // associated with an app. Since an app can have multiple endpoints
    // uniqueness cannot be enforced
    objectStore.createIndex("manifestURL", "manifestURL", { unique: false });
  },

  /*
   * @param aChannelRecord
   *        The record to be added.
   * @param aSuccessCb
   *        Callback function to invoke with result ID.
   * @param aErrorCb [optional]
   *        Callback function to invoke when there was an error.
   */
  put: function(aChannelRecord, aSuccessCb, aErrorCb) {
    debug("put()");

    this.newTxn(
      "readwrite",
      kPUSHDB_STORE_NAME,
      function txnCb(aTxn, aStore) {
        debug("Going to put " + aChannelRecord.channelID);
        aStore.put(aChannelRecord).onsuccess = function setTxnResult(aEvent) {
          debug("Request successful. Updated record ID: " +
                aEvent.target.result);
        };
      },
      aSuccessCb,
      aErrorCb
    );
  },

  /*
   * @param aChannelID
   *        The ID of record to be deleted.
   * @param aSuccessCb
   *        Callback function to invoke with result.
   * @param aErrorCb [optional]
   *        Callback function to invoke when there was an error.
   */
  delete: function(aChannelID, aSuccessCb, aErrorCb) {
    debug("delete()");

    this.newTxn(
      "readwrite",
      kPUSHDB_STORE_NAME,
      function txnCb(aTxn, aStore) {
        debug("Going to delete " + aChannelID);
        aStore.delete(aChannelID);
      },
      aSuccessCb,
      aErrorCb
    );
  },

  getByPushEndpoint: function(aPushEndpoint, aSuccessCb, aErrorCb) {
    debug("getByPushEndpoint()");

    this.newTxn(
      "readonly",
      kPUSHDB_STORE_NAME,
      function txnCb(aTxn, aStore) {
        aTxn.result = undefined;

        let index = aStore.index("pushEndpoint");
        index.get(aPushEndpoint).onsuccess = function setTxnResult(aEvent) {
          aTxn.result = aEvent.target.result;
          debug("Fetch successful " + aEvent.target.result);
        }
      },
      aSuccessCb,
      aErrorCb
    );
  },

  getByChannelID: function(aChannelID, aSuccessCb, aErrorCb) {
    debug("getByChannelID()");

    this.newTxn(
      "readonly",
      kPUSHDB_STORE_NAME,
      function txnCb(aTxn, aStore) {
        aTxn.result = undefined;

        aStore.get(aChannelID).onsuccess = function setTxnResult(aEvent) {
          aTxn.result = aEvent.target.result;
          debug("Fetch successful " + aEvent.target.result);
        }
      },
      aSuccessCb,
      aErrorCb
    );
  },

  getAllByManifestURL: function(aManifestURL, aSuccessCb, aErrorCb) {
    debug("getAllByManifestURL()");
    if (!aManifestURL) {
      if (typeof aErrorCb == "function") {
        aErrorCb("PushDB.getAllByManifestURL: Got undefined aManifestURL");
      }
      return;
    }

    let self = this;
    this.newTxn(
      "readonly",
      kPUSHDB_STORE_NAME,
      function txnCb(aTxn, aStore) {
        let index = aStore.index("manifestURL");
        let range = IDBKeyRange.only(aManifestURL);
        aTxn.result = [];
        index.openCursor(range).onsuccess = function(event) {
          let cursor = event.target.result;
          if (cursor) {
            debug(cursor.value.manifestURL + " " + cursor.value.channelID);
            aTxn.result.push(cursor.value);
            cursor.continue();
          }
        }
      },
      aSuccessCb,
      aErrorCb
    );
  },

  getAllChannelIDs: function(aSuccessCb, aErrorCb) {
    debug("getAllChannelIDs()");

    this.newTxn(
      "readonly",
      kPUSHDB_STORE_NAME,
      function txnCb(aTxn, aStore) {
        aStore.mozGetAll().onsuccess = function(event) {
          aTxn.result = event.target.result;
        }
      },
      aSuccessCb,
      aErrorCb
    );
  },

  drop: function(aSuccessCb, aErrorCb) {
    debug("drop()");
    this.newTxn(
      "readwrite",
      kPUSHDB_STORE_NAME,
      function txnCb(aTxn, aStore) {
        aStore.clear();
      },
      aSuccessCb(),
      aErrorCb()
    );
  }
};

/**
 * A proxy between the PushService and the WebSocket. The listener is used so
 * that the PushService can silence messages from the WebSocket by setting
 * PushWebSocketListener._pushService to null. This is required because
 * a WebSocket can continue to send messages or errors after it has been
 * closed but the PushService may not be interested in these. It's easier to
 * stop listening than to have checks at specific points.
 */
this.PushWebSocketListener = function(pushService) {
  this._pushService = pushService;
}

this.PushWebSocketListener.prototype = {
  onStart: function(context) {
    if (!this._pushService)
        return;
    this._pushService._wsOnStart(context);
  },

  onStop: function(context, statusCode) {
    if (!this._pushService)
        return;
    this._pushService._wsOnStop(context, statusCode);
  },

  onAcknowledge: function(context, size) {
    // EMPTY
  },

  onBinaryMessageAvailable: function(context, message) {
    // EMPTY
  },

  onMessageAvailable: function(context, message) {
    if (!this._pushService)
        return;
    this._pushService._wsOnMessageAvailable(context, message);
  },

  onServerClose: function(context, aStatusCode, aReason) {
    if (!this._pushService)
        return;
    this._pushService._wsOnServerClose(context, aStatusCode, aReason);
  }
}

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

/**
 * The implementation of the SimplePush system. This runs in the B2G parent
 * process and is started on boot. It uses WebSockets to communicate with the
 * server and PushDB (IndexedDB) for persistence.
 */
this.PushService = {
  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      /*
       * We need to call uninit() on shutdown to clean up things that modules aren't very good
       * at automatically cleaning up, so we don't get shutdown leaks on browser shutdown.
       */
      case "xpcom-shutdown":
        this.uninit();
      case "network-active-changed":         /* On B2G. */
      case "network:offline-status-changed": /* On desktop. */
        // In case of network-active-changed, always disconnect existing
        // connections. In case of offline-status changing from offline to
        // online, it is likely that these statements will be no-ops.
        if (this._udpServer) {
          this._udpServer.close();
          // Set to null since this is checked in _listenForUDPWakeup()
          this._udpServer = null;
        }

        this._shutdownWS();

        // Try to connect if network-active-changed or the offline-status
        // changed to online.
        if (aTopic === "network-active-changed" || aData === "online") {
          this._startListeningIfChannelsPresent();
        }
        break;
      case "nsPref:changed":
        if (aData == "services.push.serverURL") {
          debug("services.push.serverURL changed! websocket. new value " +
                prefs.get("serverURL"));
          this._shutdownWS();
        } else if (aData == "services.push.connection.enabled") {
          if (prefs.get("connection.enabled")) {
            this._startListeningIfChannelsPresent();
          } else {
            this._shutdownWS();
          }
        } else if (aData == "services.push.debug") {
          gDebuggingEnabled = prefs.get("debug");
        }
        break;
      case "timer-callback":
        if (aSubject == this._requestTimeoutTimer) {
          if (Object.keys(this._pendingRequests).length == 0) {
            this._requestTimeoutTimer.cancel();
          }

          // Set to true if at least one request timed out.
          let requestTimedOut = false;
          for (let channelID in this._pendingRequests) {
            let duration = Date.now() - this._pendingRequests[channelID].ctime;

            // If any of the registration requests time out, all the ones after it
            // also made to fail, since we are going to be disconnecting the socket.
            if (requestTimedOut || duration > this._requestTimeout) {
              debug("Request timeout: Removing " + channelID);
              requestTimedOut = true;
              this._pendingRequests[channelID]
                .deferred.reject({status: 0, error: "TimeoutError"});

              delete this._pendingRequests[channelID];
              for (let i = this._requestQueue.length - 1; i >= 0; --i)
                if (this._requestQueue[i][1].channelID == channelID)
                  this._requestQueue.splice(i, 1);
            }
          }

          // The most likely reason for a registration request timing out is
          // that the socket has disconnected. Best to reconnect.
          if (requestTimedOut) {
            this._shutdownWS();
            this._reconnectAfterBackoff();
          }
        }
        break;
      case "webapps-clear-data":
        debug("webapps-clear-data");

        let data = aSubject.QueryInterface(Ci.mozIApplicationClearPrivateDataParams);
        if (!data) {
          debug("webapps-clear-data: Failed to get information about application");
          return;
        }

        // Only remove push registrations for apps.
        if (data.browserOnly) {
          return;
        }

        let appsService = Cc["@mozilla.org/AppsService;1"]
                            .getService(Ci.nsIAppsService);
        let manifestURL = appsService.getManifestURLByLocalId(data.appId);
        if (!manifestURL) {
          debug("webapps-clear-data: No manifest URL found for " + data.appId);
          return;
        }

        this._db.getAllByManifestURL(manifestURL, function(records) {
          debug("Got " + records.length);
          for (let i = 0; i < records.length; i++) {
            this._db.delete(records[i].channelID, null, function() {
              debug("webapps-clear-data: " + manifestURL +
                    " Could not delete entry " + records[i].channelID);
            });
            // courtesy, but don't establish a connection
            // just for it
            if (this._ws) {
              debug("Had a connection, so telling the server");
              this._send("unregister", {channelID: records[i].channelID});
            }
          }
        }.bind(this), function() {
          debug("webapps-clear-data: Error in getAllByManifestURL(" + manifestURL + ")");
        });

        break;
    }
  },

  get _UAID() {
    return prefs.get("userAgentID");
  },

  set _UAID(newID) {
    if (typeof(newID) !== "string") {
      debug("Got invalid, non-string UAID " + newID +
            ". Not updating userAgentID");
      return;
    }
    debug("New _UAID: " + newID);
    prefs.set("userAgentID", newID);
  },

  // keeps requests buffered if the websocket disconnects or is not connected
  _requestQueue: [],
  _ws: null,
  _pendingRequests: {},
  _currentState: STATE_SHUT_DOWN,
  _requestTimeout: 0,
  _requestTimeoutTimer: null,
  _retryFailCount: 0,

  /**
   * According to the WS spec, servers should immediately close the underlying
   * TCP connection after they close a WebSocket. This causes wsOnStop to be
   * called with error NS_BASE_STREAM_CLOSED. Since the client has to keep the
   * WebSocket up, it should try to reconnect. But if the server closes the
   * WebSocket because it will wake up the client via UDP, then the client
   * shouldn't re-establish the connection. If the server says that it will
   * wake up the client over UDP, this is set to true in wsOnServerClose. It is
   * checked in wsOnStop.
   */
  _willBeWokenUpByUDP: false,

  /**
   * Holds if the adaptive ping is enabled. This is read on init().
   * If adaptive ping is enabled, a new ping is calculed each time we receive
   * a pong message, trying to maximize network resources while minimizing
   * cellular signalling storms.
   */
  _adaptiveEnabled: false,

  /**
   * This saves a flag about if we need to recalculate a new ping, based on:
   *   1) the gap between the maximum working ping and the first ping that
   *      gives an error (timeout) OR
   *   2) we have reached the pref of the maximum value we allow for a ping
   *      (services.push.adaptive.upperLimit)
   */
  _recalculatePing: true,

  /**
   * This map holds a (pingInterval, triedTimes) of each pingInterval tried.
   * It is used to check if the pingInterval has been tested enough to know that
   * is incorrect and is above the limit the network allow us to keep the
   * connection open.
   */
  _pingIntervalRetryTimes: {},

  /**
   * Holds the lastGoodPingInterval for our current connection.
   */
  _lastGoodPingInterval: 0,

  /**
   * Maximum ping interval that we can reach.
   */
  _upperLimit: 0,

  /**
   * Sends a message to the Push Server through an open websocket.
   * typeof(msg) shall be an object
   */
  _wsSendMessage: function(msg) {
    if (!this._ws) {
      debug("No WebSocket initialized. Cannot send a message.");
      return;
    }
    msg = JSON.stringify(msg);
    debug("Sending message: " + msg);
    this._ws.sendMsg(msg);
  },

  init: function() {
    debug("init()");
    if (!prefs.get("enabled"))
        return null;

    this._db = new PushDB();

    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                 .getService(Ci.nsIMessageBroadcaster);

    kCHILD_PROCESS_MESSAGES.forEach(function addMessage(msgName) {
        ppmm.addMessageListener(msgName, this);
    }.bind(this));

    this._alarmID = null;

    this._requestTimeout = prefs.get("requestTimeout");
    this._adaptiveEnabled = prefs.get('adaptive.enabled');
    this._upperLimit = prefs.get('adaptive.upperLimit');

    this._startListeningIfChannelsPresent();

    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "webapps-clear-data", false);

    // On B2G the NetworkManager interface fires a network-active-changed
    // event.
    //
    // The "active network" is based on priority - i.e. Wi-Fi has higher
    // priority than data. The PushService should just use the preferred
    // network, and not care about all interface changes.
    // network-active-changed is not fired when the network goes offline, but
    // socket connections time out. The check for Services.io.offline in
    // _beginWSSetup() prevents unnecessary retries.  When the network comes
    // back online, network-active-changed is fired.
    //
    // On non-B2G platforms, the offline-status-changed event is used to know
    // when to (dis)connect. It may not fire if the underlying OS changes
    // networks; in such a case we rely on timeout.
    //
    // On B2G both events fire, one after the other, when the network goes
    // online, so we explicitly check for the presence of NetworkManager and
    // don't add an observer for offline-status-changed on B2G.
    Services.obs.addObserver(this, this._getNetworkStateChangeEventName(), false);

    // This is only used for testing. Different tests require connecting to
    // slightly different URLs.
    prefs.observe("serverURL", this);
    // Used to monitor if the user wishes to disable Push.
    prefs.observe("connection.enabled", this);
    // Debugging
    prefs.observe("debug", this);

    this._started = true;
  },

  _shutdownWS: function() {
    debug("shutdownWS()");
    this._currentState = STATE_SHUT_DOWN;
    this._willBeWokenUpByUDP = false;

    if (this._wsListener)
      this._wsListener._pushService = null;
    try {
        this._ws.close(0, null);
    } catch (e) {}
    this._ws = null;

    this._waitingForPong = false;
    this._stopAlarm();
  },

  uninit: function() {
    if (!this._started)
      return;

    debug("uninit()");

    prefs.ignore("debug", this);
    prefs.ignore("connection.enabled", this);
    prefs.ignore("serverURL", this);
    Services.obs.removeObserver(this, this._getNetworkStateChangeEventName());
    Services.obs.removeObserver(this, "webapps-clear-data", false);
    Services.obs.removeObserver(this, "xpcom-shutdown", false);

    if (this._db) {
      this._db.close();
      this._db = null;
    }

    if (this._udpServer) {
      this._udpServer.close();
      this._udpServer = null;
    }

    // All pending requests (ideally none) are dropped at this point. We
    // shouldn't have any applications performing registration/unregistration
    // or receiving notifications.
    this._shutdownWS();

    // At this point, profile-change-net-teardown has already fired, so the
    // WebSocket has been closed with NS_ERROR_ABORT (if it was up) and will
    // try to reconnect. Stop the timer.
    this._stopAlarm();

    if (this._requestTimeoutTimer) {
      this._requestTimeoutTimer.cancel();
    }

    debug("shutdown complete!");
  },

  /**
   * How retries work:  The goal is to ensure websocket is always up on
   * networks not supporting UDP. So the websocket should only be shutdown if
   * onServerClose indicates UDP wakeup.  If WS is closed due to socket error,
   * _reconnectAfterBackoff() is called.  The retry alarm is started and when
   * it times out, beginWSSetup() is called again.
   *
   * On a successful connection, the alarm is cancelled in
   * wsOnMessageAvailable() when the ping alarm is started.
   *
   * If we are in the middle of a timeout (i.e. waiting), but
   * a register/unregister is called, we don't want to wait around anymore.
   * _sendRequest will automatically call beginWSSetup(), which will cancel the
   * timer. In addition since the state will have changed, even if a pending
   * timer event comes in (because the timer fired the event before it was
   * cancelled), so the connection won't be reset.
   */
  _reconnectAfterBackoff: function() {
    debug("reconnectAfterBackoff()");
    //Calculate new ping interval
    this._calculateAdaptivePing(true /* wsWentDown */);

    // Calculate new timeout, but cap it to pingInterval.
    let retryTimeout = prefs.get("retryBaseInterval") *
                       Math.pow(2, this._retryFailCount);
    retryTimeout = Math.min(retryTimeout, prefs.get("pingInterval"));

    this._retryFailCount++;

    debug("Retry in " + retryTimeout + " Try number " + this._retryFailCount);
    this._setAlarm(retryTimeout);
  },

  /**
   * We need to calculate a new ping based on:
   *  1) Latest good ping
   *  2) A safe gap between 1) and the calculated new ping (which is
   *  by default, 1 minute)
   *
   * This is for 3G networks, whose connections keepalives differ broadly,
   * for example:
   *  1) Movistar Spain: 29 minutes
   *  2) VIVO Brazil: 5 minutes
   *  3) Movistar Colombia: XXX minutes
   *
   * So a fixed ping is not good for us for two reasons:
   *  1) We might lose the connection, so we need to reconnect again (wasting
   *  resources)
   *  2) We use a lot of network signaling just for pinging.
   *
   * This algorithm tries to search the best value between a disconnection and a
   * valid ping, to ensure better battery life and network resources usage.
   *
   * The value is saved in services.push.pingInterval
   * @param wsWentDown [Boolean] if the WebSocket was closed or it is still alive
   *
   */
  _calculateAdaptivePing: function(wsWentDown) {
    debug('_calculateAdaptivePing()');
    if (!this._adaptiveEnabled) {
      debug('Adaptive ping is disabled');
      return;
    }

    if (this._retryFailCount > 0) {
      debug('Push has failed to connect to the Push Server ' +
        this._retryFailCount + ' times. ' +
        'Do not calculate a new pingInterval now');
      return;
    }

    if (!this._recalculatePing && !wsWentDown) {
      debug('We do not need to recalculate the ping now, based on previous data');
      return;
    }

    // Save actual state of the network
    let ns = this._getNetworkInformation();

    if (ns.ip) {
      // mobile
      debug('mobile');
      let oldNetwork = prefs.get('adaptive.mobile');
      let newNetwork = 'mobile-' + ns.mcc + '-' + ns.mnc;

      // Mobile networks differ, reset all intervals and pings
      if (oldNetwork !== newNetwork) {
        // Network differ, reset all values
        debug('Mobile networks differ. Old network is ' + oldNetwork +
              ' and new is ' + newNetwork);
        prefs.set('adaptive.mobile', newNetwork);
        //We reset the upper bound member
        this._recalculatePing = true;
        this._pingIntervalRetryTimes = {};

        // Put default values
        let defaultPing = prefs.get('pingInterval.default');
        prefs.set('pingInterval', defaultPing);
        this._lastGoodPingInterval = defaultPing;

      } else {
        // Mobile network is the same, let's just update things
        prefs.set('pingInterval', prefs.get('pingInterval.mobile'));
        this._lastGoodPingInterval = prefs.get('adaptive.lastGoodPingInterval.mobile');
      }

    } else {
      // wifi
      debug('wifi');
      prefs.set('pingInterval', prefs.get('pingInterval.wifi'));
      this._lastGoodPingInterval = prefs.get('adaptive.lastGoodPingInterval.wifi');
    }

    let nextPingInterval;
    let lastTriedPingInterval = prefs.get('pingInterval');

    if (wsWentDown) {
      debug('The WebSocket was disconnected, calculating next ping');

      // If we have not tried this pingInterval yet, initialize
      this._pingIntervalRetryTimes[lastTriedPingInterval] =
           (this._pingIntervalRetryTimes[lastTriedPingInterval] || 0) + 1;

       // Try the pingInterval at least 3 times, just to be sure that the
       // calculated interval is not valid.
       if (this._pingIntervalRetryTimes[lastTriedPingInterval] < 2) {
         debug('pingInterval= ' + lastTriedPingInterval + ' tried only ' +
           this._pingIntervalRetryTimes[lastTriedPingInterval] + ' times');
         return;
       }

       // Latest ping was invalid, we need to lower the limit to limit / 2
       nextPingInterval = Math.floor(lastTriedPingInterval / 2);

      // If the new ping interval is close to the last good one, we are near
      // optimum, so stop calculating.
      if (nextPingInterval - this._lastGoodPingInterval < prefs.get('adaptive.gap')) {
        debug('We have reached the gap, we have finished the calculation');
        debug('nextPingInterval=' + nextPingInterval);
        debug('lastGoodPing=' + this._lastGoodPingInterval);
        nextPingInterval = this._lastGoodPingInterval;
        this._recalculatePing = false;
      } else {
        debug('We need to calculate next time');
        this._recalculatePing = true;
      }

    } else {
      debug('The WebSocket is still up');
      this._lastGoodPingInterval = lastTriedPingInterval;
      nextPingInterval = Math.floor(lastTriedPingInterval * 1.5);
    }

    // Check if we have reached the upper limit
    if (this._upperLimit < nextPingInterval) {
      debug('Next ping will be bigger than the configured upper limit, capping interval');
      this._recalculatePing = false;
      this._lastGoodPingInterval = lastTriedPingInterval;
      nextPingInterval = lastTriedPingInterval;
    }

    debug('Setting the pingInterval to ' + nextPingInterval);
    prefs.set('pingInterval', nextPingInterval);

    //Save values for our current network
    if (ns.ip) {
      prefs.set('pingInterval.mobile', nextPingInterval);
      prefs.set('adaptive.lastGoodPingInterval.mobile', this._lastGoodPingInterval);
    } else {
      prefs.set('pingInterval.wifi', nextPingInterval);
      prefs.set('adaptive.lastGoodPingInterval.wifi', this._lastGoodPingInterval);
    }
  },

  _beginWSSetup: function() {
    debug("beginWSSetup()");
    if (this._currentState != STATE_SHUT_DOWN) {
      debug("_beginWSSetup: Not in shutdown state! Current state " +
            this._currentState);
      return;
    }

    if (!prefs.get("connection.enabled")) {
      debug("_beginWSSetup: connection.enabled is not set to true. Aborting.");
      return;
    }

    // Stop any pending reconnects scheduled for the near future.
    this._stopAlarm();

    if (Services.io.offline) {
      debug("Network is offline.");
      return;
    }

    let serverURL = prefs.get("serverURL");
    if (!serverURL) {
      debug("No services.push.serverURL found!");
      return;
    }

    let uri;
    try {
      uri = Services.io.newURI(serverURL, null, null);
    } catch(e) {
      debug("Error creating valid URI from services.push.serverURL (" +
            serverURL + ")");
      return;
    }

    if (uri.scheme === "wss") {
      this._ws = Cc["@mozilla.org/network/protocol;1?name=wss"]
                   .createInstance(Ci.nsIWebSocketChannel);

      this._ws.initLoadInfo(null, // aLoadingNode
                            Services.scriptSecurityManager.getSystemPrincipal(),
                            null, // aTriggeringPrincipal
                            Ci.nsILoadInfo.SEC_NORMAL,
                            Ci.nsIContentPolicy.TYPE_WEBSOCKET);
    }
    else if (uri.scheme === "ws") {
      debug("Push over an insecure connection (ws://) is not allowed!");
      return;
    }
    else {
      debug("Unsupported websocket scheme " + uri.scheme);
      return;
    }

    debug("serverURL: " + uri.spec);
    this._wsListener = new PushWebSocketListener(this);
    this._ws.protocol = "push-notification";

    try {
      // Grab a wakelock before we open the socket to ensure we don't go to sleep
      // before connection the is opened.
      this._ws.asyncOpen(uri, serverURL, 0, this._wsListener, null);
      this._acquireWakeLock();
      this._currentState = STATE_WAITING_FOR_WS_START;
    } catch(e) {
      debug("Error opening websocket. asyncOpen failed!");
      this._shutdownWS();
      this._reconnectAfterBackoff();
    }
  },

  _startListeningIfChannelsPresent: function() {
    // Check to see if we need to do anything.
    this._db.getAllChannelIDs(function(channelIDs) {
      if (channelIDs.length > 0) {
        this._beginWSSetup();
      }
    }.bind(this));
  },

  /** |delay| should be in milliseconds. */
  _setAlarm: function(delay) {
    // Bug 909270: Since calls to AlarmService.add() are async, calls must be
    // 'queued' to ensure only one alarm is ever active.
    if (this._settingAlarm) {
        // onSuccess will handle the set. Overwriting the variable enforces the
        // last-writer-wins semantics.
        this._queuedAlarmDelay = delay;
        this._waitingForAlarmSet = true;
        return;
    }

    // Stop any existing alarm.
    this._stopAlarm();

    this._settingAlarm = true;
    AlarmService.add(
      {
        date: new Date(Date.now() + delay),
        ignoreTimezone: true
      },
      this._onAlarmFired.bind(this),
      function onSuccess(alarmID) {
        this._alarmID = alarmID;
        debug("Set alarm " + delay + " in the future " + this._alarmID);
        this._settingAlarm = false;

        if (this._waitingForAlarmSet) {
          this._waitingForAlarmSet = false;
          this._setAlarm(this._queuedAlarmDelay);
        }
      }.bind(this)
    )
  },

  _stopAlarm: function() {
    if (this._alarmID !== null) {
      debug("Stopped existing alarm " + this._alarmID);
      AlarmService.remove(this._alarmID);
      this._alarmID = null;
    }
  },

  /**
   * There is only one alarm active at any time. This alarm has 3 intervals
   * corresponding to 3 tasks.
   *
   * 1) Reconnect on ping timeout.
   *    If we haven't received any messages from the server by the time this
   *    alarm fires, the connection is closed and PushService tries to
   *    reconnect, repurposing the alarm for (3).
   *
   * 2) Send a ping.
   *    The protocol sends a ping ({}) on the wire every pingInterval ms. Once
   *    it sends the ping, the alarm goes to task (1) which is waiting for
   *    a pong. If data is received after the ping is sent,
   *    _wsOnMessageAvailable() will reset the ping alarm (which cancels
   *    waiting for the pong). So as long as the connection is fine, pong alarm
   *    never fires.
   *
   * 3) Reconnect after backoff.
   *    The alarm is set by _reconnectAfterBackoff() and increases in duration
   *    every time we try and fail to connect.  When it triggers, websocket
   *    setup begins again. On successful socket setup, the socket starts
   *    receiving messages. The alarm now goes to (2) where it monitors the
   *    WebSocket by sending a ping.  Since incoming data is a sign of the
   *    connection being up, the ping alarm is reset every time data is
   *    received.
   */
  _onAlarmFired: function() {
    // Conditions are arranged in decreasing specificity.
    // i.e. when _waitingForPong is true, other conditions are also true.
    if (this._waitingForPong) {
      debug("Did not receive pong in time. Reconnecting WebSocket.");
      this._shutdownWS();
      this._reconnectAfterBackoff();
    }
    else if (this._currentState == STATE_READY) {
      // Send a ping.
      // Bypass the queue; we don't want this to be kept pending.
      // Watch out for exception in case the socket has disconnected.
      // When this happens, we pretend the ping was sent and don't specially
      // handle the exception, as the lack of a pong will lead to the socket
      // being reset.
      try {
        this._wsSendMessage({});
      } catch (e) {
      }

      this._waitingForPong = true;
      this._setAlarm(prefs.get("requestTimeout"));
    }
    else if (this._alarmID !== null) {
      debug("reconnect alarm fired.");
      // Reconnect after back-off.
      // The check for a non-null _alarmID prevents a situation where the alarm
      // fires, but _shutdownWS() is called from another code-path (e.g.
      // network state change) and we don't want to reconnect.
      //
      // It also handles the case where _beginWSSetup() is called from another
      // code-path.
      //
      // alarmID will be non-null only when no shutdown/connect is
      // called between _reconnectAfterBackoff() setting the alarm and the
      // alarm firing.

      // Websocket is shut down. Backoff interval expired, try to connect.
      this._beginWSSetup();
    }
  },

  _acquireWakeLock: function() {
    if (!this._socketWakeLock) {
      debug("Acquiring Socket Wakelock");
      this._socketWakeLock = gPowerManagerService.newWakeLock("cpu");
    }
    if (!this._socketWakeLockTimer) {
      debug("Creating Socket WakeLock Timer");
      this._socketWakeLockTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }

    debug("Setting Socket WakeLock Timer");
    this._socketWakeLockTimer
      .initWithCallback(this._releaseWakeLock.bind(this),
                        // Allow the same time for socket setup as we do for
                        // requests after the setup. Fudge it a bit since
                        // timers can be a little off and we don't want to go
                        // to sleep just as the socket connected.
                        this._requestTimeout + 1000,
                        Ci.nsITimer.ONE_SHOT);
  },

  _releaseWakeLock: function() {
    debug("Releasing Socket WakeLock");
    if (this._socketWakeLockTimer) {
      this._socketWakeLockTimer.cancel();
    }
    if (this._socketWakeLock) {
      this._socketWakeLock.unlock();
      this._socketWakeLock = null;
    }
  },

  /**
   * Protocol handler invoked by server message.
   */
  _handleHelloReply: function(reply) {
    debug("handleHelloReply()");
    if (this._currentState != STATE_WAITING_FOR_HELLO) {
      debug("Unexpected state " + this._currentState +
            "(expected STATE_WAITING_FOR_HELLO)");
      this._shutdownWS();
      return;
    }

    if (typeof reply.uaid !== "string") {
      debug("No UAID received or non string UAID received");
      this._shutdownWS();
      return;
    }

    if (reply.uaid === "") {
      debug("Empty UAID received!");
      this._shutdownWS();
      return;
    }

    // To avoid sticking extra large values sent by an evil server into prefs.
    if (reply.uaid.length > 128) {
      debug("UAID received from server was too long: " +
            reply.uaid);
      this._shutdownWS();
      return;
    }

    function finishHandshake() {
      this._UAID = reply.uaid;
      this._currentState = STATE_READY;
      this._processNextRequestInQueue();
    }

    // By this point we've got a UAID from the server that we are ready to
    // accept.
    //
    // If we already had a valid UAID before, we have to ask apps to
    // re-register.
    if (this._UAID && this._UAID != reply.uaid) {
      debug("got new UAID: all re-register");

      this._notifyAllAppsRegister()
          .then(this._dropRegistrations.bind(this))
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
    debug("handleRegisterReply()");
    if (typeof reply.channelID !== "string" ||
        typeof this._pendingRequests[reply.channelID] !== "object")
      return;

    let tmp = this._pendingRequests[reply.channelID];
    delete this._pendingRequests[reply.channelID];
    if (Object.keys(this._pendingRequests).length == 0 &&
        this._requestTimeoutTimer)
      this._requestTimeoutTimer.cancel();

    if (reply.status == 200) {
      tmp.deferred.resolve(reply);
    } else {
      tmp.deferred.reject(reply);
    }
  },

  /**
   * Protocol handler invoked by server message.
   */
  _handleNotificationReply: function(reply) {
    debug("handleNotificationReply()");
    if (typeof reply.updates !== 'object') {
      debug("No 'updates' field in response. Type = " + typeof reply.updates);
      return;
    }

    debug("Reply updates: " + reply.updates.length);
    for (let i = 0; i < reply.updates.length; i++) {
      let update = reply.updates[i];
      debug("Update: " + update.channelID + ": " + update.version);
      if (typeof update.channelID !== "string") {
        debug("Invalid update literal at index " + i);
        continue;
      }

      if (update.version === undefined) {
        debug("update.version does not exist");
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
        this._sendAck(update.channelID, version);
      }
    }
  },

  // FIXME(nsm): batch acks for efficiency reasons.
  _sendAck: function(channelID, version) {
    debug("sendAck()");
    this._send('ack', {
      updates: [{channelID: channelID, version: version}]
    });
  },

  /*
   * Must be used only by request/response style calls over the websocket.
   */
  _sendRequest: function(action, data) {
    debug("sendRequest() " + action);
    if (typeof data.channelID !== "string") {
      debug("Received non-string channelID");
      return Promise.reject("Received non-string channelID");
    }

    let deferred = Promise.defer();

    if (Object.keys(this._pendingRequests).length == 0) {
      // start the timer since we now have at least one request
      if (!this._requestTimeoutTimer)
        this._requestTimeoutTimer = Cc["@mozilla.org/timer;1"]
                                      .createInstance(Ci.nsITimer);
      this._requestTimeoutTimer.init(this,
                                     this._requestTimeout,
                                     Ci.nsITimer.TYPE_REPEATING_SLACK);
    }

    this._pendingRequests[data.channelID] = { deferred: deferred,
                                              ctime: Date.now() };

    this._send(action, data);
    return deferred.promise;
  },

  _send: function(action, data) {
    debug("send()");
    this._requestQueue.push([action, data]);
    debug("Queued " + action);
    this._processNextRequestInQueue();
  },

  _processNextRequestInQueue: function() {
    debug("_processNextRequestInQueue()");

    if (this._requestQueue.length == 0) {
      debug("Request queue empty");
      return;
    }

    if (this._currentState != STATE_READY) {
      if (!this._ws) {
        // This will end up calling processNextRequestInQueue().
        this._beginWSSetup();
      }
      else {
        // We have a socket open so we are just waiting for hello to finish.
        // That will call processNextRequestInQueue().
      }
      return;
    }

    let [action, data] = this._requestQueue.shift();
    data.messageType = action;
    if (!this._ws) {
      // If our websocket is not ready and our state is STATE_READY we may as
      // well give up all assumptions about the world and start from scratch
      // again.  Discard the message itself, let the timeout notify error to
      // the app.
      debug("This should never happen!");
      this._shutdownWS();
    }

    this._wsSendMessage(data);
    // Process the next one as soon as possible.
    setTimeout(this._processNextRequestInQueue.bind(this), 0);
  },

  _receivedUpdate: function(aChannelID, aLatestVersion) {
    debug("Updating: " + aChannelID + " -> " + aLatestVersion);

    let compareRecordVersionAndNotify = function(aPushRecord) {
      debug("compareRecordVersionAndNotify()");
      if (!aPushRecord) {
        debug("No record for channel ID " + aChannelID);
        return;
      }

      if (aPushRecord.version == null ||
          aPushRecord.version < aLatestVersion) {
        debug("Version changed, notifying app and updating DB");
        aPushRecord.version = aLatestVersion;
        this._notifyApp(aPushRecord);
        this._updatePushRecord(aPushRecord)
          .then(
            null,
            function(e) {
              debug("Error updating push record");
            }
          );
      }
      else {
        debug("No significant version change: " + aLatestVersion);
      }
    }

    let recoverNoSuchChannelID = function(aChannelIDFromServer) {
      debug("Could not get channelID " + aChannelIDFromServer + " from DB");
    }

    this._db.getByChannelID(aChannelID,
                            compareRecordVersionAndNotify.bind(this),
                            recoverNoSuchChannelID.bind(this));
  },

  // Fires a push-register system message to all applications that have
  // registrations.
  _notifyAllAppsRegister: function() {
    debug("notifyAllAppsRegister()");
    let deferred = Promise.defer();

    // records are objects describing the registrations as stored in IndexedDB.
    function wakeupRegisteredApps(records) {
      // Pages to be notified.
      // wakeupTable[manifestURL] -> [ pageURL ]
      let wakeupTable = {};
      for (let i = 0; i < records.length; i++) {
        let record = records[i];
        if (!(record.manifestURL in wakeupTable))
          wakeupTable[record.manifestURL] = [];

        wakeupTable[record.manifestURL].push(record.pageURL);
      }

      let messenger = Cc["@mozilla.org/system-message-internal;1"]
                        .getService(Ci.nsISystemMessagesInternal);

      for (let manifestURL in wakeupTable) {
        wakeupTable[manifestURL].forEach(function(pageURL) {
          messenger.sendMessage('push-register', {},
                                Services.io.newURI(pageURL, null, null),
                                Services.io.newURI(manifestURL, null, null));
        });
      }

      deferred.resolve();
    }

    this._db.getAllChannelIDs(wakeupRegisteredApps, deferred.reject);

    return deferred.promise;
  },

  _notifyApp: function(aPushRecord) {
    if (!aPushRecord || !aPushRecord.pageURL || !aPushRecord.manifestURL) {
      debug("notifyApp() something is undefined.  Dropping notification");
      return;
    }

    debug("notifyApp() " + aPushRecord.pageURL +
          "  " + aPushRecord.manifestURL);
    let pageURI = Services.io.newURI(aPushRecord.pageURL, null, null);
    let manifestURI = Services.io.newURI(aPushRecord.manifestURL, null, null);
    let message = {
      pushEndpoint: aPushRecord.pushEndpoint,
      version: aPushRecord.version
    };
    let messenger = Cc["@mozilla.org/system-message-internal;1"]
                      .getService(Ci.nsISystemMessagesInternal);
    messenger.sendMessage('push', message, pageURI, manifestURI);
  },

  _updatePushRecord: function(aPushRecord) {
    debug("updatePushRecord()");
    let deferred = Promise.defer();
    this._db.put(aPushRecord, deferred.resolve, deferred.reject);
    return deferred.promise;
  },

  _dropRegistrations: function() {
    let deferred = Promise.defer();
    this._db.drop(deferred.resolve, deferred.reject);
    return deferred.promise;
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage(): " + aMessage.name);

    if (kCHILD_PROCESS_MESSAGES.indexOf(aMessage.name) == -1) {
      debug("Invalid message from child " + aMessage.name);
      return;
    }

    let mm = aMessage.target.QueryInterface(Ci.nsIMessageSender);
    let json = aMessage.data;
    this[aMessage.name.slice("Push:".length).toLowerCase()](json, mm);
  },

  /**
   * Called on message from the child process. aPageRecord is an object sent by
   * navigator.push, identifying the sending page and other fields.
   */
  register: function(aPageRecord, aMessageManager) {
    debug("register()");

    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                          .getService(Ci.nsIUUIDGenerator);
    // generateUUID() gives a UUID surrounded by {...}, slice them off.
    let channelID = uuidGenerator.generateUUID().toString().slice(1, -1);

    this._sendRequest("register", {channelID: channelID})
      .then(
        this._onRegisterSuccess.bind(this, aPageRecord, channelID),
        this._onRegisterError.bind(this, aPageRecord, aMessageManager)
      )
      .then(
        function(message) {
          aMessageManager.sendAsyncMessage("PushService:Register:OK", message);
        },
        function(message) {
          aMessageManager.sendAsyncMessage("PushService:Register:KO", message);
      });
  },

  /**
   * Exceptions thrown in _onRegisterSuccess are caught by the promise obtained
   * from _sendRequest, causing the promise to be rejected instead.
   */
  _onRegisterSuccess: function(aPageRecord, generatedChannelID, data) {
    debug("_onRegisterSuccess()");
    let deferred = Promise.defer();
    let message = { requestID: aPageRecord.requestID };

    if (typeof data.channelID !== "string") {
      debug("Invalid channelID " + message);
      message["error"] = "Invalid channelID received";
      throw message;
    }
    else if (data.channelID != generatedChannelID) {
      debug("Server replied with different channelID " + data.channelID +
            " than what UA generated " + generatedChannelID);
      message["error"] = "Server sent 200 status code but different channelID";
      throw message;
    }

    try {
      Services.io.newURI(data.pushEndpoint, null, null);
    }
    catch (e) {
      debug("Invalid pushEndpoint " + data.pushEndpoint);
      message["error"] = "Invalid pushEndpoint " + data.pushEndpoint;
      throw message;
    }

    let record = {
      channelID: data.channelID,
      pushEndpoint: data.pushEndpoint,
      pageURL: aPageRecord.pageURL,
      manifestURL: aPageRecord.manifestURL,
      version: null
    };

    this._updatePushRecord(record)
      .then(
        function() {
          message["pushEndpoint"] = data.pushEndpoint;
          deferred.resolve(message);
        },
        function(error) {
          // Unable to save.
          this._send("unregister", {channelID: record.channelID});
          message["error"] = error;
          deferred.reject(message);
        }
      );

    return deferred.promise;
  },

  /**
   * Exceptions thrown in _onRegisterError are caught by the promise obtained
   * from _sendRequest, causing the promise to be rejected instead.
   */
  _onRegisterError: function(aPageRecord, aMessageManager, reply) {
    debug("_onRegisterError()");
    if (!reply.error) {
      debug("Called without valid error message!");
    }
    throw { requestID: aPageRecord.requestID, error: reply.error };
  },

  /**
   * Called on message from the child process.
   *
   * Why is the record being deleted from the local database before the server
   * is told?
   *
   * Unregistration is for the benefit of the app and the AppServer
   * so that the AppServer does not keep pinging a channel the UserAgent isn't
   * watching The important part of the transaction in this case is left to the
   * app, to tell its server of the unregistration.  Even if the request to the
   * PushServer were to fail, it would not affect correctness of the protocol,
   * and the server GC would just clean up the channelID eventually.  Since the
   * appserver doesn't ping it, no data is lost.
   *
   * If rather we were to unregister at the server and update the database only
   * on success: If the server receives the unregister, and deletes the
   * channelID, but the response is lost because of network failure, the
   * application is never informed. In addition the application may retry the
   * unregister when it fails due to timeout at which point the server will say
   * it does not know of this unregistration.  We'll have to make the
   * registration/unregistration phases have retries and attempts to resend
   * messages from the server, and have the client acknowledge. On a server,
   * data is cheap, reliable notification is not.
   */
  unregister: function(aPageRecord, aMessageManager) {
    debug("unregister()");

    let fail = function(error) {
      debug("unregister() fail() error " + error);
      let message = {requestID: aPageRecord.requestID, error: error};
      aMessageManager.sendAsyncMessage("PushService:Unregister:KO", message);
    }

    this._db.getByPushEndpoint(aPageRecord.pushEndpoint, function(record) {
      // If the endpoint didn't exist, let's just fail.
      if (record === undefined) {
        fail("NotFoundError");
        return;
      }

      // Non-owner tried to unregister, say success, but don't do anything.
      if (record.manifestURL !== aPageRecord.manifestURL) {
        aMessageManager.sendAsyncMessage("PushService:Unregister:OK", {
          requestID: aPageRecord.requestID,
          pushEndpoint: aPageRecord.pushEndpoint
        });
        return;
      }

      this._db.delete(record.channelID, function() {
        // Let's be nice to the server and try to inform it, but we don't care
        // about the reply.
        this._send("unregister", {channelID: record.channelID});
        aMessageManager.sendAsyncMessage("PushService:Unregister:OK", {
          requestID: aPageRecord.requestID,
          pushEndpoint: aPageRecord.pushEndpoint
        });
      }.bind(this), fail);
    }.bind(this), fail);
  },

  /**
   * Called on message from the child process
   */
  registrations: function(aPageRecord, aMessageManager) {
    debug("registrations()");

    if (aPageRecord.manifestURL) {
      this._db.getAllByManifestURL(aPageRecord.manifestURL,
        this._onRegistrationsSuccess.bind(this, aPageRecord, aMessageManager),
        this._onRegistrationsError.bind(this, aPageRecord, aMessageManager));
    }
    else {
      this._onRegistrationsError(aPageRecord, aMessageManager);
    }
  },

  _onRegistrationsSuccess: function(aPageRecord,
                                    aMessageManager,
                                    pushRecords) {
    let registrations = [];
    pushRecords.forEach(function(pushRecord) {
      registrations.push({
          __exposedProps__: { pushEndpoint: 'r', version: 'r' },
          pushEndpoint: pushRecord.pushEndpoint,
          version: pushRecord.version
      });
    });
    aMessageManager.sendAsyncMessage("PushService:Registrations:OK", {
      requestID: aPageRecord.requestID,
      registrations: registrations
    });
  },

  _onRegistrationsError: function(aPageRecord, aMessageManager) {
    aMessageManager.sendAsyncMessage("PushService:Registrations:KO", {
      requestID: aPageRecord.requestID,
      error: "Database error"
    });
  },

  // begin Push protocol handshake
  _wsOnStart: function(context) {
    debug("wsOnStart()");
    this._releaseWakeLock();

    if (this._currentState != STATE_WAITING_FOR_WS_START) {
      debug("NOT in STATE_WAITING_FOR_WS_START. Current state " +
            this._currentState + ". Skipping");
      return;
    }

    // Since we've had a successful connection reset the retry fail count.
    this._retryFailCount = 0;

    let data = {
      messageType: "hello",
    }

    if (this._UAID)
      data["uaid"] = this._UAID;

    this._getNetworkState((networkState) => {
      if (networkState.ip) {
        // Opening an available UDP port.
        this._listenForUDPWakeup();

        // Host-port is apparently a thing.
        data["wakeup_hostport"] = {
          ip: networkState.ip,
          port: this._udpServer && this._udpServer.port
        };

        data["mobilenetwork"] = {
          mcc: networkState.mcc,
          mnc: networkState.mnc,
          netid: networkState.netid
        };
      }

      this._wsSendMessage(data);
      this._currentState = STATE_WAITING_FOR_HELLO;
    });
  },

  /**
   * This statusCode is not the websocket protocol status code, but the TCP
   * connection close status code.
   *
   * If we do not explicitly call ws.close() then statusCode is always
   * NS_BASE_STREAM_CLOSED, even on a successful close.
   */
  _wsOnStop: function(context, statusCode) {
    debug("wsOnStop()");
    this._releaseWakeLock();

    if (statusCode != Cr.NS_OK &&
        !(statusCode == Cr.NS_BASE_STREAM_CLOSED && this._willBeWokenUpByUDP)) {
      debug("Socket error " + statusCode);
      this._reconnectAfterBackoff();
    }

    // Bug 896919. We always shutdown the WebSocket, even if we need to
    // reconnect. This works because _reconnectAfterBackoff() is "async"
    // (there is a minimum delay of the pref retryBaseInterval, which by default
    // is 5000ms), so that function will open the WebSocket again.
    this._shutdownWS();
  },

  _wsOnMessageAvailable: function(context, message) {
    debug("wsOnMessageAvailable() " + message);

    this._waitingForPong = false;

    let reply = undefined;
    try {
      reply = JSON.parse(message);
    } catch(e) {
      debug("Parsing JSON failed. text : " + message);
      return;
    }

    // If we are not waiting for a hello message, reset the retry fail count
    if (this._currentState != STATE_WAITING_FOR_HELLO) {
      debug('Reseting _retryFailCount and _pingIntervalRetryTimes');
      this._retryFailCount = 0;
      this._pingIntervalRetryTimes = {};
    }

    let doNotHandle = false;
    if ((message === '{}') ||
        (reply.messageType === undefined) ||
        (reply.messageType === "ping") ||
        (typeof reply.messageType != "string")) {
      debug('Pong received');
      this._calculateAdaptivePing(false);
      doNotHandle = true;
    }

    // Reset the ping timer.  Note: This path is executed at every step of the
    // handshake, so this alarm does not need to be set explicitly at startup.
    this._setAlarm(prefs.get("pingInterval"));

    // If it is a ping, do not handle the message.
    if (doNotHandle) {
      return;
    }

    // A whitelist of protocol handlers. Add to these if new messages are added
    // in the protocol.
    let handlers = ["Hello", "Register", "Notification"];

    // Build up the handler name to call from messageType.
    // e.g. messageType == "register" -> _handleRegisterReply.
    let handlerName = reply.messageType[0].toUpperCase() +
                      reply.messageType.slice(1).toLowerCase();

    if (handlers.indexOf(handlerName) == -1) {
      debug("No whitelisted handler " + handlerName + ". messageType: " +
            reply.messageType);
      return;
    }

    let handler = "_handle" + handlerName + "Reply";

    if (typeof this[handler] !== "function") {
      debug("Handler whitelisted but not implemented! " + handler);
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
   * If the server said it'll use UDP for wakeup, we set _willBeWokenUpByUDP
   * and stop reconnecting in _wsOnStop().
   */
  _wsOnServerClose: function(context, aStatusCode, aReason) {
    debug("wsOnServerClose() " + aStatusCode + " " + aReason);

    // Switch over to UDP.
    if (aStatusCode == kUDP_WAKEUP_WS_STATUS_CODE) {
      debug("Server closed with promise to wake up");
      this._willBeWokenUpByUDP = true;
      // TODO: there should be no pending requests
    }
  },

  /**
   * This method should be called only if the device is on a mobile network!
   */
  _listenForUDPWakeup: function() {
    debug("listenForUDPWakeup()");

    if (this._udpServer) {
      debug("UDP Server already running");
      return;
    }

    if (!prefs.get("udp.wakeupEnabled")) {
      debug("UDP support disabled");
      return;
    }

    this._udpServer = Cc["@mozilla.org/network/udp-socket;1"]
                        .createInstance(Ci.nsIUDPSocket);
    this._udpServer.init(-1, false, Services.scriptSecurityManager.getSystemPrincipal());
    this._udpServer.asyncListen(this);
    debug("listenForUDPWakeup listening on " + this._udpServer.port);

    return this._udpServer.port;
  },

  /**
   * Called by UDP Server Socket. As soon as a ping is recieved via UDP,
   * reconnect the WebSocket and get the actual data.
   */
  onPacketReceived: function(aServ, aMessage) {
    debug("Recv UDP datagram on port: " + this._udpServer.port);
    this._beginWSSetup();
  },

  /**
   * Called by UDP Server Socket if the socket was closed for some reason.
   *
   * If this happens, we reconnect the WebSocket to not miss out on
   * notifications.
   */
  onStopListening: function(aServ, aStatus) {
    debug("UDP Server socket was shutdown. Status: " + aStatus);
    this._udpServer = undefined;
    this._beginWSSetup();
  },

  /**
   * Returns information about MCC-MNC and the IP of the current connection.
   */
  _getNetworkInformation: function() {
    debug("getNetworkInformation()");

    try {
      if (!prefs.get("udp.wakeupEnabled")) {
        debug("UDP support disabled, we do not send any carrier info");
        throw new Error("UDP disabled");
      }

      let nm = Cc["@mozilla.org/network/manager;1"].getService(Ci.nsINetworkManager);
      if (nm.activeNetworkInfo &&
          nm.activeNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE) {
        let iccService = Cc["@mozilla.org/icc/iccservice;1"].getService(Ci.nsIIccService);
        // TODO: Bug 927721 - PushService for multi-sim
        // In Multi-sim, there is more than one client in iccService. Each
        // client represents a icc handle. To maintain backward compatibility
        // with single sim, we always use client 0 for now. Adding support
        // for multiple sim will be addressed in bug 927721, if needed.
        let clientId = 0;
        let icc = iccService.getIccByServiceId(clientId);
        let iccInfo = icc && icc.iccInfo;
        if (iccInfo) {
          debug("Running on mobile data");

          let ips = {};
          let prefixLengths = {};
          nm.activeNetworkInfo.getAddresses(ips, prefixLengths);

          return {
            mcc: iccInfo.mcc,
            mnc: iccInfo.mnc,
            ip:  ips.value[0]
          }
        }
      }
    } catch (e) {
      debug("Error recovering mobile network information: " + e);
    }

    debug("Running on wifi");
    return {
      mcc: 0,
      mnc: 0,
      ip: undefined
    };
  },

  /**
   * Get mobile network information to decide if the client is capable of being
   * woken up by UDP (which currently just means having an mcc and mnc along
   * with an IP, and optionally a netid).
   */
  _getNetworkState: function(callback) {
    debug("getNetworkState()");

    if (typeof callback !== 'function') {
      throw new Error("No callback method. Aborting push agent !");
    }

    var networkInfo = this._getNetworkInformation();

    if (networkInfo.ip) {
      this._getMobileNetworkId(networkInfo, function(netid) {
        debug("Recovered netID = " + netid);
        callback({
          mcc: networkInfo.mcc,
          mnc: networkInfo.mnc,
          ip:  networkInfo.ip,
          netid: netid
        });
      });
    } else {
      callback(networkInfo);
    }
  },

  // utility function used to add/remove observers in init() and shutdown()
  _getNetworkStateChangeEventName: function() {
    try {
      Cc["@mozilla.org/network/manager;1"].getService(Ci.nsINetworkManager);
      return "network-active-changed";
    } catch (e) {
      return "network:offline-status-changed";
    }
  },

  /*
   * Get the mobile network ID (netid)
   *
   * @param networkInfo
   *        Network information object { mcc, mnc, ip, port }
   * @param callback
   *        Callback function to invoke with the netid or null if not found
   */
  _getMobileNetworkId: function(networkInfo, callback) {
    if (typeof callback !== 'function') {
      return;
    }

    function queryDNSForDomain(domain) {
      debug("[_getMobileNetworkId:queryDNSForDomain] Querying DNS for " +
        domain);
      let netIDDNSListener = {
        onLookupComplete: function(aRequest, aRecord, aStatus) {
          if (aRecord) {
            let netid = aRecord.getNextAddrAsString();
            debug("[_getMobileNetworkId:queryDNSForDomain] NetID found: " +
              netid);
            callback(netid);
          } else {
            debug("[_getMobileNetworkId:queryDNSForDomain] NetID not found");
            callback(null);
          }
        }
      };
      gDNSService.asyncResolve(domain, 0, netIDDNSListener,
        threadManager.currentThread);
      return [];
    }

    debug("[_getMobileNetworkId:queryDNSForDomain] Getting mobile network ID");

    let netidAddress = "wakeup.mnc" + ("00" + networkInfo.mnc).slice(-3) +
      ".mcc" + ("00" + networkInfo.mcc).slice(-3) + ".3gppnetwork.org";
    queryDNSForDomain(netidAddress, callback);
  }
}
