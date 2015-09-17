/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set dom.push.debug.
var gDebuggingEnabled = false;

function debug(s) {
  if (gDebuggingEnabled) {
    dump("-*- PushService.jsm: " + s + "\n");
  }
}

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const {PushDB} = Cu.import("resource://gre/modules/PushDB.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Promise.jsm");

const {PushServiceWebSocket} = Cu.import("resource://gre/modules/PushServiceWebSocket.jsm");
const {PushServiceHttp2} = Cu.import("resource://gre/modules/PushServiceHttp2.jsm");
const {PushCrypto} = Cu.import("resource://gre/modules/PushCrypto.jsm");

// Currently supported protocols: WebSocket.
const CONNECTION_PROTOCOLS = [PushServiceWebSocket, PushServiceHttp2];

XPCOMUtils.defineLazyModuleGetter(this, "AlarmService",
                                  "resource://gre/modules/AlarmService.jsm");

this.EXPORTED_SYMBOLS = ["PushService"];

const prefs = new Preferences("dom.push.");
// Set debug first so that all debugging actually works.
gDebuggingEnabled = prefs.get("debug");

const kCHILD_PROCESS_MESSAGES = ["Push:Register", "Push:Unregister",
                                 "Push:Registration", "Push:RegisterEventNotificationListener",
                                 "child-process-shutdown"];

const PUSH_SERVICE_UNINIT = 0;
const PUSH_SERVICE_INIT = 1; // No serverURI
const PUSH_SERVICE_ACTIVATING = 2;//activating db
const PUSH_SERVICE_CONNECTION_DISABLE = 3;
const PUSH_SERVICE_ACTIVE_OFFLINE = 4;
const PUSH_SERVICE_RUNNING = 5;

// Telemetry failure to send push notification to Service Worker reasons.
// Key not found in local database.
const kDROP_NOTIFICATION_REASON_KEY_NOT_FOUND = 0;
// User cleared history.
const kDROP_NOTIFICATION_REASON_NO_HISTORY = 1;
// Version of message received not newer than previous one.
const kDROP_NOTIFICATION_REASON_NO_VERSION_INCREMENT = 2;
// Subscription has expired.
const kDROP_NOTIFICATION_REASON_EXPIRED = 3;

/**
 * State is change only in couple of functions:
 *   init - change state to PUSH_SERVICE_INIT if state was PUSH_SERVICE_UNINIT
 *   changeServerURL - change state to PUSH_SERVICE_ACTIVATING if serverURL
 *                     present or PUSH_SERVICE_INIT if not present.
 *   changeStateConnectionEnabledEvent - it is call on pref change or during
 *                                       the service activation and it can
 *                                       change state to
 *                                       PUSH_SERVICE_CONNECTION_DISABLE
 *   changeStateOfflineEvent - it is called when offline state changes or during
 *                             the service activation and it change state to
 *                             PUSH_SERVICE_ACTIVE_OFFLINE or
 *                             PUSH_SERVICE_RUNNING.
 *   uninit - change state to PUSH_SERVICE_UNINIT.
 **/

// This is for starting and stopping service.
const STARTING_SERVICE_EVENT = 0;
const CHANGING_SERVICE_EVENT = 1;
const STOPPING_SERVICE_EVENT = 2;
const UNINIT_EVENT = 3;

/**
 * The implementation of the push system. It uses WebSockets
 * (PushServiceWebSocket) to communicate with the server and PushDB (IndexedDB)
 * for persistence.
 */
this.PushService = {
  _service: null,
  _state: PUSH_SERVICE_UNINIT,
  _db: null,
  _options: null,
  _alarmID: null,

  _childListeners: [],

  // When serverURI changes (this is used for testing), db is cleaned up and a
  // a new db is started. This events must be sequential.
  _serverURIProcessQueue: null,
  _serverURIProcessEnqueue: function(op) {
    if (!this._serverURIProcessQueue) {
      this._serverURIProcessQueue = Promise.resolve();
    }

    this._serverURIProcessQueue = this._serverURIProcessQueue
                                    .then(op)
                                    .catch(_ => {});
  },

  // Pending request. If a worker try to register for the same scope again, do
  // not send a new registration request. Therefore we need queue of pending
  // register requests. This is the list of scopes which pending registration.
  _pendingRegisterRequest: {},
  _notifyActivated: null,
  _activated: null,
  _checkActivated: function() {
    if (this._state < PUSH_SERVICE_ACTIVATING) {
      return Promise.reject({state: 0, error: "Service not active"});
    } else if (this._state > PUSH_SERVICE_ACTIVATING) {
      return Promise.resolve();
    } else {
      return (this._activated) ? this._activated :
                                 this._activated = new Promise((res, rej) =>
                                   this._notifyActivated = {resolve: res,
                                                            reject: rej});
    }
  },

  _makePendingKey: function(aPageRecord) {
    return aPageRecord.scope + "|" + aPageRecord.originAttributes;
  },

  _lookupOrPutPendingRequest: function(aPageRecord) {
    let key = this._makePendingKey(aPageRecord);
    if (this._pendingRegisterRequest[key]) {
      return this._pendingRegisterRequest[key];
    }

    return this._pendingRegisterRequest[key] = this._registerWithServer(aPageRecord);
  },

  _deletePendingRequest: function(aPageRecord) {
    let key = this._makePendingKey(aPageRecord);
    if (this._pendingRegisterRequest[key]) {
      delete this._pendingRegisterRequest[key];
    }
  },

  _setState: function(aNewState) {
    debug("new state: " + aNewState + " old state: " + this._state);

    if (this._state == aNewState) {
      return;
    }

    if (this._state == PUSH_SERVICE_ACTIVATING) {
      // It is not important what is the new state as soon as we leave
      // PUSH_SERVICE_ACTIVATING
      this._state = aNewState;
      if (this._notifyActivated) {
        if (aNewState < PUSH_SERVICE_ACTIVATING) {
          this._notifyActivated.reject({state: 0, error: "Service not active"});
        } else {
          this._notifyActivated.resolve();
        }
      }
      this._notifyActivated = null;
      this._activated = null;
    }
    this._state = aNewState;
  },

  _changeStateOfflineEvent: function(offline, calledFromConnEnabledEvent) {
    debug("changeStateOfflineEvent: " + offline);

    if (this._state < PUSH_SERVICE_ACTIVE_OFFLINE &&
        this._state != PUSH_SERVICE_ACTIVATING &&
        !calledFromConnEnabledEvent) {
      return;
    }

    if (offline) {
      if (this._state == PUSH_SERVICE_RUNNING) {
        this._service.disconnect();
      }
      this._setState(PUSH_SERVICE_ACTIVE_OFFLINE);
    } else {
      if (this._state == PUSH_SERVICE_RUNNING) {
        // PushService was not in the offline state, but got notification to
        // go online (a offline notification has not been sent).
        // Disconnect first.
        this._service.disconnect();
      }
      this._db.getAllUnexpired()
        .then(records => {
          if (records.length > 0) {
            // if there are request waiting
            this._service.connect(records);
          }
        });
      this._setState(PUSH_SERVICE_RUNNING);
    }
  },

  _changeStateConnectionEnabledEvent: function(enabled) {
    debug("changeStateConnectionEnabledEvent: " + enabled);

    if (this._state < PUSH_SERVICE_CONNECTION_DISABLE &&
        this._state != PUSH_SERVICE_ACTIVATING) {
      return;
    }

    if (enabled) {
      this._changeStateOfflineEvent(Services.io.offline, true);
    } else {
      if (this._state == PUSH_SERVICE_RUNNING) {
        this._service.disconnect();
      }
      this._setState(PUSH_SERVICE_CONNECTION_DISABLE);
    }
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      /*
       * We need to call uninit() on shutdown to clean up things that modules
       * aren't very good at automatically cleaning up, so we don't get shutdown
       * leaks on browser shutdown.
       */
      case "xpcom-shutdown":
        this.uninit();
        break;
      case "network-active-changed":         /* On B2G. */
      case "network:offline-status-changed": /* On desktop. */
        this._changeStateOfflineEvent(aData === "offline", false);
        break;

      case "nsPref:changed":
        if (aData == "dom.push.serverURL") {
          debug("dom.push.serverURL changed! websocket. new value " +
                prefs.get("serverURL"));
          this._serverURIProcessEnqueue(_ =>
            this._changeServerURL(prefs.get("serverURL"),
                                  CHANGING_SERVICE_EVENT)
          );

        } else if (aData == "dom.push.connection.enabled") {
          this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"));

        } else if (aData == "dom.push.debug") {
          gDebuggingEnabled = prefs.get("debug");
        }
        break;

      case "idle-daily":
        this._dropExpiredRegistrations();
        break;

      case "webapps-clear-data":
        debug("webapps-clear-data");

        let data = aSubject
                     .QueryInterface(Ci.mozIApplicationClearPrivateDataParams);
        if (!data) {
          debug("webapps-clear-data: Failed to get information about " +
                "application");
          return;
        }

        var originAttributes =
          ChromeUtils.originAttributesToSuffix({ appId: data.appId,
                                                 inBrowser: data.browserOnly });
        this._db.getAllByOriginAttributes(originAttributes)
          .then(records => {
            records.forEach(record => {
              this._db.delete(record.keyID)
                .then(_ => {
                  // courtesy, but don't establish a connection
                  // just for it
                  if (this._ws) {
                    debug("Had a connection, so telling the server");
                    this._sendUnregister({channelID: record.channelID})
                        .catch(function(e) {
                          debug("Unregister errored " + e);
                        });
                  }
                }, err => {
                  debug("webapps-clear-data: " + record.scope +
                        " Could not delete entry " + record.channelID);

                  // courtesy, but don't establish a connection
                  // just for it
                  if (this._ws) {
                    debug("Had a connection, so telling the server");
                    this._sendUnregister({channelID: record.channelID})
                        .catch(function(e) {
                          debug("Unregister errored " + e);
                        });
                  }
                  throw "Database error";
                });
            });
          }, _ => {
            debug("webapps-clear-data: Error in getAllByOriginAttributes(" + originAttributes + ")");
          });

        break;
    }
  },

  // utility function used to add/remove observers in startObservers() and
  // stopObservers()
  getNetworkStateChangeEventName: function() {
    try {
      Cc["@mozilla.org/network/manager;1"].getService(Ci.nsINetworkManager);
      return "network-active-changed";
    } catch (e) {
      return "network:offline-status-changed";
    }
  },

  _findService: function(serverURI) {
    var uri;
    var service;
    if (serverURI) {
      for (let connProtocol of CONNECTION_PROTOCOLS) {
        uri = connProtocol.checkServerURI(serverURI);
        if (uri) {
          service = connProtocol;
          break;
        }
      }
    }
    return [service, uri];
  },

  _changeServerURL: function(serverURI, event) {
    debug("changeServerURL");

    switch(event) {
      case UNINIT_EVENT:
        return this._stopService(event);

      case STARTING_SERVICE_EVENT:
      {
        let [service, uri] = this._findService(serverURI);
        if (!service) {
          this._setState(PUSH_SERVICE_INIT);
          return Promise.resolve();
        }
        return this._startService(service, uri, event)
          .then(_ =>
            this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"))
          );
      }
      case CHANGING_SERVICE_EVENT:
        let [service, uri] = this._findService(serverURI);
        if (service) {
          if (this._state == PUSH_SERVICE_INIT) {
            this._setState(PUSH_SERVICE_ACTIVATING);
            // The service has not been running - start it.
            return this._startService(service, uri, STARTING_SERVICE_EVENT)
              .then(_ =>
                this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"))
              );

          } else {
            this._setState(PUSH_SERVICE_ACTIVATING);
            // If we already had running service - stop service, start the new
            // one and check connection.enabled and offline state(offline state
            // check is called in changeStateConnectionEnabledEvent function)
            return this._stopService(CHANGING_SERVICE_EVENT)
              .then(_ =>
                 this._startService(service, uri, CHANGING_SERVICE_EVENT)
              )
              .then(_ =>
                this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"))
              );

          }
        } else {
          if (this._state == PUSH_SERVICE_INIT) {
            return Promise.resolve();

          } else {
            // The new serverUri is empty or misconfigured - stop service.
            this._setState(PUSH_SERVICE_INIT);
            return this._stopService(STOPPING_SERVICE_EVENT);
          }
        }
    }
  },

  /**
   * PushService initialization is divided into 4 parts:
   * init() - start listening for xpcom-shutdown and serverURL changes.
   *          state is change to PUSH_SERVICE_INIT
   * startService() - if serverURL is present this function is called. It starts
   *                  listening for broadcasted messages, starts db and
   *                  PushService connection (WebSocket).
   *                  state is change to PUSH_SERVICE_ACTIVATING.
   * startObservers() - start other observers.
   * changeStateConnectionEnabledEvent  - checks prefs and offline state.
   *                                      It changes state to:
   *                                        PUSH_SERVICE_RUNNING,
   *                                        PUSH_SERVICE_ACTIVE_OFFLINE or
   *                                        PUSH_SERVICE_CONNECTION_DISABLE.
   */
  init: function(options = {}) {
    debug("init()");

    if (this._state > PUSH_SERVICE_UNINIT) {
      return;
    }

    this._setState(PUSH_SERVICE_ACTIVATING);

    // Debugging
    prefs.observe("debug", this);

    Services.obs.addObserver(this, "xpcom-shutdown", false);

    if (options.serverURI) {
      // this is use for xpcshell test.

      var uri;
      var service;
      if (!options.service) {
        for (let connProtocol of CONNECTION_PROTOCOLS) {
          uri = connProtocol.checkServerURI(options.serverURI);
          if (uri) {
            service = connProtocol;
            break;
          }
        }
      } else {
        try {
          uri  = Services.io.newURI(options.serverURI, null, null);
          service = options.service;
        } catch(e) {}
      }
      if (!service) {
        this._setState(PUSH_SERVICE_INIT);
        return;
      }

      // Start service.
      this._startService(service, uri, false, options).then(_ => {
        // Before completing the activation check prefs. This will first check
        // connection.enabled pref and then check offline state.
        this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"));
      });

    } else {
      // This is only used for testing. Different tests require connecting to
      // slightly different URLs.
      prefs.observe("serverURL", this);

      this._serverURIProcessEnqueue(_ =>
        this._changeServerURL(prefs.get("serverURL"), STARTING_SERVICE_EVENT));
    }
  },

  _startObservers: function() {
    debug("startObservers");

    if (this._state != PUSH_SERVICE_ACTIVATING) {
      return;
    }

    Services.obs.addObserver(this, "webapps-clear-data", false);

    // On B2G the NetworkManager interface fires a network-active-changed
    // event.
    //
    // The "active network" is based on priority - i.e. Wi-Fi has higher
    // priority than data. The PushService should just use the preferred
    // network, and not care about all interface changes.
    // network-active-changed is not fired when the network goes offline, but
    // socket connections time out. The check for Services.io.offline in
    // PushServiceWebSocket._beginWSSetup() prevents unnecessary retries.  When
    // the network comes back online, network-active-changed is fired.
    //
    // On non-B2G platforms, the offline-status-changed event is used to know
    // when to (dis)connect. It may not fire if the underlying OS changes
    // networks; in such a case we rely on timeout.
    //
    // On B2G both events fire, one after the other, when the network goes
    // online, so we explicitly check for the presence of NetworkManager and
    // don't add an observer for offline-status-changed on B2G.
    this._networkStateChangeEventName = this.getNetworkStateChangeEventName();
    Services.obs.addObserver(this, this._networkStateChangeEventName, false);

    // Used to monitor if the user wishes to disable Push.
    prefs.observe("connection.enabled", this);

    // Used to prune expired registrations and notify dormant service workers.
    Services.obs.addObserver(this, "idle-daily", false);
  },

  _startService: function(service, serverURI, event, options = {}) {
    debug("startService");

    if (this._state != PUSH_SERVICE_ACTIVATING) {
      return;
    }

    if (event != CHANGING_SERVICE_EVENT) {
      // if only serverURL is changed we can keep listening for broadcast
      // messages and queue them.
      let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                   .getService(Ci.nsIMessageBroadcaster);

      kCHILD_PROCESS_MESSAGES.forEach(msgName =>
        ppmm.addMessageListener(msgName, this)
      );
    }

    this._service = service;

    this._db = options.db;
    if (!this._db) {
      this._db = this._service.newPushDB();
    }

    this._service.init(options, this, serverURI);
    this._startObservers();
    return Promise.resolve();
  },

  /**
   * PushService uninitialization is divided into 3 parts:
   * stopObservers() - stot observers started in startObservers.
   * stopService() - It stops listening for broadcasted messages, stops db and
   *                 PushService connection (WebSocket).
   *                 state is changed to PUSH_SERVICE_INIT.
   * uninit() - stop listening for xpcom-shutdown and serverURL changes.
   *            state is change to PUSH_SERVICE_UNINIT
   */
  _stopService: function(event) {
    debug("stopService");

    if (this._state < PUSH_SERVICE_ACTIVATING) {
      return;
    }

    this.stopAlarm();
    this._stopObservers();

    if (event != CHANGING_SERVICE_EVENT) {
      let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                   .getService(Ci.nsIMessageBroadcaster);

      kCHILD_PROCESS_MESSAGES.forEach(
        msgName => ppmm.removeMessageListener(msgName, this)
      );
    }

    this._service.disconnect();
    this._service.uninit();
    this._service = null;
    this.stopAlarm();

    if (!this._db) {
      return Promise.resolve();
    }
    if (event == UNINIT_EVENT) {
      // If it is uninitialized just close db.
      this._db.close();
      this._db = null;
      return Promise.resolve();
    }

    return this.dropRegistrations()
       .then(_ => {
         this._db.close();
         this._db = null;
       }, err => {
         this._db.close();
         this._db = null;
       });
  },

  _stopObservers: function() {
    debug("stopObservers()");

    if (this._state < PUSH_SERVICE_ACTIVATING) {
      return;
    }

    prefs.ignore("debug", this);
    prefs.ignore("connection.enabled", this);

    Services.obs.removeObserver(this, this._networkStateChangeEventName);
    Services.obs.removeObserver(this, "webapps-clear-data");
    Services.obs.removeObserver(this, "idle-daily");
  },

  uninit: function() {
    debug("uninit()");

    this._childListeners = [];

    if (this._state == PUSH_SERVICE_UNINIT) {
      return;
    }

    this._setState(PUSH_SERVICE_UNINIT);

    prefs.ignore("serverURL", this);
    Services.obs.removeObserver(this, "xpcom-shutdown");

    this._serverURIProcessEnqueue(_ =>
            this._changeServerURL("", UNINIT_EVENT));
    debug("shutdown complete!");
  },

  /** |delay| should be in milliseconds. */
  setAlarm: function(delay) {
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
    this.stopAlarm();

    this._settingAlarm = true;
    AlarmService.add(
      {
        date: new Date(Date.now() + delay),
        ignoreTimezone: true
      },
      () => {
        if (this._state > PUSH_SERVICE_ACTIVATING) {
          this._service.onAlarmFired();
        }
      }, (alarmID) => {
        this._alarmID = alarmID;
        debug("Set alarm " + delay + " in the future " + this._alarmID);
        this._settingAlarm = false;

        if (this._waitingForAlarmSet) {
          this._waitingForAlarmSet = false;
          this.setAlarm(this._queuedAlarmDelay);
        }
      }
    );
  },

  stopAlarm: function() {
    if (this._alarmID !== null) {
      debug("Stopped existing alarm " + this._alarmID);
      AlarmService.remove(this._alarmID);
      this._alarmID = null;
    }
  },

  dropRegistrations: function() {
    return this._notifyAllAppsRegister()
      .then(_ => this._db.drop());
  },

  _notifySubscriptionChangeObservers: function(record) {
    // Notify XPCOM observers.
    Services.obs.notifyObservers(
      null,
      "push-subscription-change",
      record.scope
    );

    let data = {
      originAttributes: record.originAttributes,
      scope: record.scope
    };

    Services.telemetry.getHistogramById("PUSH_API_NOTIFY_REGISTRATION_LOST").add();
    this._notifyListeners('pushsubscriptionchange', data);
  },

  _notifyListeners: function(name, data) {
    if (this._childListeners.length > 0) {
      // Try to send messages to all listeners, but remove any that fail since
      // the receiver is likely gone away.
      for (var i = this._childListeners.length - 1; i >= 0; --i) {
        try {
          this._childListeners[i].sendAsyncMessage(name, data);
        } catch(e) {
          this._childListeners.splice(i, 1);
        }
      }
    } else {
      let ppmm = Cc['@mozilla.org/parentprocessmessagemanager;1']
                   .getService(Ci.nsIMessageListenerManager);
      ppmm.broadcastAsyncMessage(name, data);
    }
  },

  // Fires a push-register system message to all applications that have
  // registration.
  _notifyAllAppsRegister: function() {
    debug("notifyAllAppsRegister()");
    // records are objects describing the registration as stored in IndexedDB.
    return this._db.getAllUnexpired().then(records => {
      records.forEach(record => {
        this._notifySubscriptionChangeObservers(record);
      });
    });
  },

  dropRegistrationAndNotifyApp: function(aKeyId) {
    return this._db.getByKeyID(aKeyId).then(record => {
      this._notifySubscriptionChangeObservers(record);
      return this._db.delete(aKeyId);
    });
  },

  updateRegistrationAndNotifyApp: function(aOldKey, aRecord) {
    return this._db.delete(aOldKey)
      .then(_ => this._db.put(aRecord))
      .then(record => this._notifySubscriptionChangeObservers(record));
  },

  ensureP256dhKey: function(record) {
    if (record.p256dhPublicKey && record.p256dhPrivateKey) {
      return Promise.resolve(record);
    }
    // We do not have a encryption key. so we need to generate it. This
    // is only going to happen on db upgrade from version 4 to higher.
    return PushCrypto.generateKeys()
      .then(exportedKeys => {
        return this.updateRecordAndNotifyApp(record.keyID, record => {
          record.p256dhPublicKey = exportedKeys[0];
          record.p256dhPrivateKey = exportedKeys[1];
          return record;
        });
      }, error => {
        return this.dropRegistrationAndNotifyApp(record.keyID).then(
          () => Promise.reject(error));
      });
  },

  updateRecordAndNotifyApp: function(aKeyID, aUpdateFunc) {
    return this._db.update(aKeyID, aUpdateFunc)
      .then(record => {
        this._notifySubscriptionChangeObservers(record);
        return record;
      });
  },

  _recordDidNotNotify: function(reason) {
    Services.telemetry.
      getHistogramById("PUSH_API_NOTIFICATION_RECEIVED_BUT_DID_NOT_NOTIFY").
      add(reason);
  },

  /**
   * Dispatches an incoming message to a service worker, recalculating the
   * quota for the associated push registration. If the quota is exceeded,
   * the registration and message will be dropped, and the worker will not
   * be notified.
   *
   * @param {String} keyID The push registration ID.
   * @param {String} message The message contents.
   * @param {Object} cryptoParams The message encryption settings.
   * @param {Function} updateFunc A function that receives the existing
   *  registration record as its argument, and returns a new record. If the
   *  function returns `null` or `undefined`, the record will not be updated.
   *  `PushServiceWebSocket` uses this to drop incoming updates with older
   *  versions.
   */
  receivedPushMessage: function(keyID, message, cryptoParams, updateFunc) {
    debug("receivedPushMessage()");
    Services.telemetry.getHistogramById("PUSH_API_NOTIFICATION_RECEIVED").add();

    let shouldNotify = false;
    return this.getByKeyID(keyID).then(record => {
      if (!record) {
        this._recordDidNotNotify(kDROP_NOTIFICATION_REASON_KEY_NOT_FOUND);
        throw new Error("No record for key ID " + keyID);
      }
      return record.getLastVisit();
    }).then(lastVisit => {
      // As a special case, don't notify the service worker if the user
      // cleared their history.
      shouldNotify = isFinite(lastVisit);
      if (!shouldNotify) {
          this._recordDidNotNotify(kDROP_NOTIFICATION_REASON_NO_HISTORY);
      }
      return this._db.update(keyID, record => {
        let newRecord = updateFunc(record);
        if (!newRecord) {
          this._recordDidNotNotify(kDROP_NOTIFICATION_REASON_NO_VERSION_INCREMENT);
          return null;
        }
        // Because `unregister` is advisory only, we can still receive messages
        // for stale Simple Push registrations from the server. To work around
        // this, we check if the record has expired before *and* after updating
        // the quota.
        if (newRecord.isExpired()) {
          debug("receivedPushMessage: Ignoring update for expired key ID " + keyID);
          return null;
        }
        newRecord.receivedPush(lastVisit);
        return newRecord;
      });
    }).then(record => {
      var notified = false;
      if (!record) {
        return notified;
      }
      let decodedPromise;
      if (cryptoParams) {
        decodedPromise = PushCrypto.decodeMsg(
          message,
          record.p256dhPrivateKey,
          cryptoParams.dh,
          cryptoParams.salt,
          cryptoParams.rs
        );
      } else {
        decodedPromise = Promise.resolve(null);
      }
      return decodedPromise.then(message => {
        if (shouldNotify) {
          notified = this._notifyApp(record, message);
        }
        if (record.isExpired()) {
          this._recordDidNotNotify(kDROP_NOTIFICATION_REASON_EXPIRED);
          // Drop the registration in the background. If the user returns to the
          // site, the service worker will be notified on the next `idle-daily`
          // event.
          this._sendUnregister(record).catch(error => {
            debug("receivedPushMessage: Unregister error: " + error);
          });
        }
        return notified;
      });
    }).catch(error => {
      debug("receivedPushMessage: Error notifying app: " + error);
    });
  },

  _notifyApp: function(aPushRecord, message) {
    if (!aPushRecord || !aPushRecord.scope ||
        aPushRecord.originAttributes === undefined) {
      debug("notifyApp() something is undefined.  Dropping notification: " +
        JSON.stringify(aPushRecord) );
      return false;
    }

    debug("notifyApp() " + aPushRecord.scope);
    // Notify XPCOM observers.
    let notification = Cc["@mozilla.org/push/ObserverNotification;1"]
                         .createInstance(Ci.nsIPushObserverNotification);
    notification.pushEndpoint = aPushRecord.pushEndpoint;
    notification.version = aPushRecord.version;

    let payload = ArrayBuffer.isView(message) ?
                  new Uint8Array(message.buffer) : message;
    if (payload) {
      notification.data = "";
      for (let i = 0; i < payload.length; i++) {
        notification.data += String.fromCharCode(payload[i]);
      }
    }

    notification.lastPush = aPushRecord.lastPush;
    notification.pushCount = aPushRecord.pushCount;

    Services.obs.notifyObservers(
      notification,
      "push-notification",
      aPushRecord.scope
    );

    // If permission has been revoked, trash the message.
    if (!aPushRecord.hasPermission()) {
      debug("Does not have permission for push.");
      return false;
    }

    let data = {
      payload: payload,
      originAttributes: aPushRecord.originAttributes,
      scope: aPushRecord.scope
    };

    Services.telemetry.getHistogramById("PUSH_API_NOTIFY").add();
    this._notifyListeners('push', data);
    return true;
  },

  getByKeyID: function(aKeyID) {
    return this._db.getByKeyID(aKeyID);
  },

  getAllUnexpired: function() {
    return this._db.getAllUnexpired();
  },

  _sendRequest: function(action, aRecord) {
    if (this._state == PUSH_SERVICE_CONNECTION_DISABLE) {
      return Promise.reject({state: 0, error: "Service not active"});
    } else if (this._state == PUSH_SERVICE_ACTIVE_OFFLINE) {
      return Promise.reject({state: 0, error: "NetworkError"});
    }
    return this._service.request(action, aRecord);
  },

  /**
   * Called on message from the child process. aPageRecord is an object sent by
   * navigator.push, identifying the sending page and other fields.
   */
  _registerWithServer: function(aPageRecord) {
    debug("registerWithServer()" + JSON.stringify(aPageRecord));

    Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_ATTEMPT").add();
    return this._sendRequest("register", aPageRecord)
      .then(record => this._onRegisterSuccess(record),
            err => this._onRegisterError(err))
      .then(record => {
        this._deletePendingRequest(aPageRecord);
        return record;
      }, err => {
        this._deletePendingRequest(aPageRecord);
        throw err;
     });
  },

  _register: function(aPageRecord) {
    debug("_register()");
    if (!aPageRecord.scope || aPageRecord.originAttributes === undefined) {
      return Promise.reject({state: 0, error: "NotFoundError"});
    }

    return this._checkActivated()
      .then(_ => this._db.getByIdentifiers(aPageRecord))
      .then(record => {
        if (!record) {
          return this._lookupOrPutPendingRequest(aPageRecord);
        }
        if (record.isExpired()) {
          return record.getLastVisit().then(lastVisit => {
            if (lastVisit > record.lastPush) {
              // If the user revisited the site, drop the expired push
              // registration and re-register.
              return this._db.delete(record.keyID).then(_ => {
                return this._lookupOrPutPendingRequest(aPageRecord);
              });
            }
            throw {state: 0, error: "NotFoundError"};
          });
        }
        return record;
      }, error => {
        debug("getByIdentifiers failed");
        throw error;
      });
  },

  _sendUnregister: function(aRecord) {
    Services.telemetry.getHistogramById("PUSH_API_UNSUBSCRIBE_ATTEMPT").add();
    return this._sendRequest("unregister", aRecord).then(function(v) {
      Services.telemetry.getHistogramById("PUSH_API_UNSUBSCRIBE_SUCCEEDED").add();
      return v;
    }).catch(function(e) {
      Services.telemetry.getHistogramById("PUSH_API_UNSUBSCRIBE_FAILED").add();
      return Promise.reject(e);
    });
  },

  /**
   * Exceptions thrown in _onRegisterSuccess are caught by the promise obtained
   * from _service.request, causing the promise to be rejected instead.
   */
  _onRegisterSuccess: function(aRecord) {
    debug("_onRegisterSuccess()");

    return this._db.put(aRecord)
      .then(record => {
        Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_SUCCEEDED").add();
        return record;
      })
      .catch(error => {
        Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_FAILED").add()
        // Unable to save. Destroy the subscription in the background.
        this._sendUnregister(aRecord).catch(err => {
          debug("_onRegisterSuccess: Error unregistering stale subscription" +
            err);
        });
        throw error;
      });
  },

  /**
   * Exceptions thrown in _onRegisterError are caught by the promise obtained
   * from _service.request, causing the promise to be rejected instead.
   */
  _onRegisterError: function(reply) {
    debug("_onRegisterError()");
    Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_FAILED").add()
    if (!reply.error) {
      debug("Called without valid error message!");
      throw "Registration error";
    }
    throw reply.error;
  },

  receiveMessage: function(aMessage) {
    debug("receiveMessage(): " + aMessage.name);

    if (kCHILD_PROCESS_MESSAGES.indexOf(aMessage.name) == -1) {
      debug("Invalid message from child " + aMessage.name);
      return;
    }

    if (aMessage.name === "Push:RegisterEventNotificationListener") {
      debug("Adding child listener");
      this._childListeners.push(aMessage.target);
      return;
    }

    if (aMessage.name === "child-process-shutdown") {
      debug("Possibly removing child listener");
      for (var i = this._childListeners.length - 1; i >= 0; --i) {
        if (this._childListeners[i] == aMessage.target) {
          debug("Removed child listener");
          this._childListeners.splice(i, 1);
        }
      }
      return;
    }

    if (!aMessage.target.assertPermission("push")) {
      debug("Got message from a child process that does not have 'push' permission.");
      return null;
    }

    let mm = aMessage.target.QueryInterface(Ci.nsIMessageSender);
    let pageRecord = aMessage.data;

    let principal = aMessage.principal;
    if (!principal) {
      debug("No principal passed!");
      let message = {
        requestID: pageRecord.requestID,
        error: "SecurityError"
      };
      mm.sendAsyncMessage("PushService:Register:KO", message);
      return;
    }

    pageRecord.originAttributes =
      ChromeUtils.originAttributesToSuffix(principal.originAttributes);

    if (!pageRecord.scope || pageRecord.originAttributes === undefined) {
      debug("Incorrect identifier values set! " + JSON.stringify(pageRecord));
      let message = {
        requestID: pageRecord.requestID,
        error: "SecurityError"
      };
      mm.sendAsyncMessage("PushService:Register:KO", message);
      return;
    }

    this[aMessage.name.slice("Push:".length).toLowerCase()](pageRecord, mm);
  },

  register: function(aPageRecord, aMessageManager) {
    debug("register(): " + JSON.stringify(aPageRecord));

    this._register(aPageRecord)
      .then(record => {
        let message = record.toRegister();
        message.requestID = aPageRecord.requestID;
        aMessageManager.sendAsyncMessage("PushService:Register:OK", message);
      }, error => {
        let message = {
          requestID: aPageRecord.requestID,
          error
        };
        aMessageManager.sendAsyncMessage("PushService:Register:KO", message);
      });
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
   * and the server GC would just clean up the channelID/subscription
   * eventually.  Since the appserver doesn't ping it, no data is lost.
   *
   * If rather we were to unregister at the server and update the database only
   * on success: If the server receives the unregister, and deletes the
   * channelID/subscription, but the response is lost because of network
   * failure, the application is never informed. In addition the application may
   * retry the unregister when it fails due to timeout (websocket) or any other
   * reason at which point the server will say it does not know of this
   * unregistration.  We'll have to make the registration/unregistration phases
   * have retries and attempts to resend messages from the server, and have the
   * client acknowledge. On a server, data is cheap, reliable notification is
   * not.
   */
  _unregister: function(aPageRecord) {
    debug("_unregister()");
    if (!aPageRecord.scope || aPageRecord.originAttributes === undefined) {
      return Promise.reject({state: 0, error: "NotFoundError"});
    }

    return this._checkActivated()
      .then(_ => this._db.getByIdentifiers(aPageRecord))
      .then(record => {
        if (record === undefined) {
          return false;
        }

        return Promise.all([
          this._sendUnregister(record),
          this._db.delete(record.keyID),
        ]).then(() => true);
      });
  },

  unregister: function(aPageRecord, aMessageManager) {
    debug("unregister() " + JSON.stringify(aPageRecord));

    this._unregister(aPageRecord)
      .then(result => {
          aMessageManager.sendAsyncMessage("PushService:Unregister:OK", {
            requestID: aPageRecord.requestID,
            result: result,
          })
        }, error => {
          debug("unregister(): Actual error " + error);
          aMessageManager.sendAsyncMessage("PushService:Unregister:KO", {
            requestID: aPageRecord.requestID,
          })
        }
      );
  },

  _clearAll: function _clearAll() {
    return this._checkActivated()
      .then(_ => this._db.clearAll())
      .catch(_ => Promise.resolve());
  },

  _clearForDomain: function(domain) {
    /**
     * Copied from ForgetAboutSite.jsm.
     *
     * Returns true if the string passed in is part of the root domain of the
     * current string.  For example, if this is "www.mozilla.org", and we pass in
     * "mozilla.org", this will return true.  It would return false the other way
     * around.
     */
    function hasRootDomain(str, aDomain)
    {
      let index = str.indexOf(aDomain);
      // If aDomain is not found, we know we do not have it as a root domain.
      if (index == -1)
        return false;

      // If the strings are the same, we obviously have a match.
      if (str == aDomain)
        return true;

      // Otherwise, we have aDomain as our root domain iff the index of aDomain is
      // aDomain.length subtracted from our length and (since we do not have an
      // exact match) the character before the index is a dot or slash.
      let prevChar = str[index - 1];
      return (index == (str.length - aDomain.length)) &&
             (prevChar == "." || prevChar == "/");
    }

    let clear = (db, domain) => {
      db.clearIf(record => {
        return hasRootDomain(record.uri.prePath, domain);
      });
    }

    return this._checkActivated()
      .then(_ => clear(this._db, domain))
      .catch(e => {
        debug("Error forgetting about domain! " + e);
        return Promise.resolve();
      });
  },

  /**
   * Called on message from the child process
   */
  _registration: function(aPageRecord) {
    debug("_registration()");
    if (!aPageRecord.scope || aPageRecord.originAttributes === undefined) {
      return Promise.reject({state: 0, error: "NotFoundError"});
    }

    return this._checkActivated()
      .then(_ => this._db.getByIdentifiers(aPageRecord))
      .then(record => {
        if (!record) {
          return null;
        }
        if (record.isExpired()) {
          return record.getLastVisit().then(lastVisit => {
            if (lastVisit > record.lastPush) {
              return this._db.delete(record.keyID).then(_ => null);
            }
            throw {state: 0, error: "NotFoundError"};
          });
        }
        return record.toRegistration();
      });
  },

  registration: function(aPageRecord, aMessageManager) {
    debug("registration()");

    return this._registration(aPageRecord)
      .then(registration =>
        aMessageManager.sendAsyncMessage("PushService:Registration:OK", {
          requestID: aPageRecord.requestID,
          registration
        }), error =>
        aMessageManager.sendAsyncMessage("PushService:Registration:KO", {
          requestID: aPageRecord.requestID,
          error
        })
      );
  },

  _dropExpiredRegistrations: function() {
    debug("dropExpiredRegistrations()");

    this._db.getAllExpired().then(records => {
      return Promise.all(records.map(record => {
        return record.getLastVisit().then(lastVisit => {
          if (lastVisit > record.lastPush) {
            // If the user revisited the site, drop the expired push
            // registration and notify the associated service worker.
            return this._db.delete(record.keyID).then(() => {
              this._notifySubscriptionChangeObservers(record);
            });
          }
        }).catch(error => {
          debug("dropExpiredRegistrations: Error dropping registration " +
            record.keyID + ": " + error);
        });
      }));
    }).catch(error => {
      debug("dropExpiredRegistrations: Error dropping registrations: " +
        error);
    });
  },
};
