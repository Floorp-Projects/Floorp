/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Timer.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const {PushCrypto} = Cu.import("resource://gre/modules/PushCrypto.jsm");
const {PushDB} = Cu.import("resource://gre/modules/PushDB.jsm");

const CONNECTION_PROTOCOLS = (function() {
  if ('android' != AppConstants.MOZ_WIDGET_TOOLKIT) {
    const {PushServiceWebSocket} = Cu.import("resource://gre/modules/PushServiceWebSocket.jsm");
    const {PushServiceHttp2} = Cu.import("resource://gre/modules/PushServiceHttp2.jsm");
    return [PushServiceWebSocket, PushServiceHttp2];
  } else {
    const {PushServiceAndroidGCM} = Cu.import("resource://gre/modules/PushServiceAndroidGCM.jsm");
    return [PushServiceAndroidGCM];
  }
})();

XPCOMUtils.defineLazyServiceGetter(this, "gPushNotifier",
                                   "@mozilla.org/push/Notifier;1",
                                   "nsIPushNotifier");

XPCOMUtils.defineLazyGetter(this, "gDOMBundle", () =>
  Services.strings.createBundle("chrome://global/locale/dom/dom.properties"));

this.EXPORTED_SYMBOLS = ["PushService"];

XPCOMUtils.defineLazyGetter(this, "console", () => {
  let {ConsoleAPI} = Cu.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "PushService",
  });
});

const prefs = new Preferences("dom.push.");

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
 * Annotates an error with an XPCOM result code. We use this helper
 * instead of `Components.Exception` because the latter can assert in
 * `nsXPCComponents_Exception::HasInstance` when inspected at shutdown.
 */
function errorWithResult(message, result = Cr.NS_ERROR_FAILURE) {
  let error = new Error(message);
  error.result = result;
  return error;
}

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
  _visibleNotifications: new Map(),

  // Callback that is called after attempting to
  // reduce the quota for a record. Used for testing purposes.
  _updateQuotaTestCallback: null,

  // Set of timeout ID of tasks to reduce quota.
  _updateQuotaTimeouts: new Set(),

  // When serverURI changes (this is used for testing), db is cleaned up and a
  // a new db is started. This events must be sequential.
  _stateChangeProcessQueue: null,
  _stateChangeProcessEnqueue: function(op) {
    if (!this._stateChangeProcessQueue) {
      this._stateChangeProcessQueue = Promise.resolve();
    }

    this._stateChangeProcessQueue = this._stateChangeProcessQueue
      .then(op)
      .catch(error => {
        console.error(
          "stateChangeProcessEnqueue: Error transitioning state", error);
        return this._shutdownService();
      })
      .catch(error => {
        console.error(
          "stateChangeProcessEnqueue: Error shutting down service", error);
      });
    return this._stateChangeProcessQueue;
  },

  // Pending request. If a worker try to register for the same scope again, do
  // not send a new registration request. Therefore we need queue of pending
  // register requests. This is the list of scopes which pending registration.
  _pendingRegisterRequest: {},
  _notifyActivated: null,
  _activated: null,
  _checkActivated: function() {
    if (this._state < PUSH_SERVICE_ACTIVATING) {
      return Promise.reject(new Error("Push service not active"));
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
    console.debug("setState()", "new state", aNewState, "old state", this._state);

    if (this._state == aNewState) {
      return;
    }

    if (this._state == PUSH_SERVICE_ACTIVATING) {
      // It is not important what is the new state as soon as we leave
      // PUSH_SERVICE_ACTIVATING
      if (this._notifyActivated) {
        if (aNewState < PUSH_SERVICE_ACTIVATING) {
          this._notifyActivated.reject(new Error("Push service not active"));
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
    console.debug("changeStateOfflineEvent()", offline);

    if (this._state < PUSH_SERVICE_ACTIVE_OFFLINE &&
        this._state != PUSH_SERVICE_ACTIVATING &&
        !calledFromConnEnabledEvent) {
      return Promise.resolve();
    }

    if (offline) {
      if (this._state == PUSH_SERVICE_RUNNING) {
        this._service.disconnect();
      }
      this._setState(PUSH_SERVICE_ACTIVE_OFFLINE);
      return Promise.resolve();
    }

    if (this._state == PUSH_SERVICE_RUNNING) {
      // PushService was not in the offline state, but got notification to
      // go online (a offline notification has not been sent).
      // Disconnect first.
      this._service.disconnect();
    }
    return this.getAllUnexpired().then(records => {
      this._setState(PUSH_SERVICE_RUNNING);
      if (records.length > 0) {
        // if there are request waiting
        this._service.connect(records);
      }
    });
  },

  _changeStateConnectionEnabledEvent: function(enabled) {
    console.debug("changeStateConnectionEnabledEvent()", enabled);

    if (this._state < PUSH_SERVICE_CONNECTION_DISABLE &&
        this._state != PUSH_SERVICE_ACTIVATING) {
      return Promise.resolve();
    }

    if (enabled) {
      return this._changeStateOfflineEvent(Services.io.offline, true);
    }

    if (this._state == PUSH_SERVICE_RUNNING) {
      this._service.disconnect();
    }
    this._setState(PUSH_SERVICE_CONNECTION_DISABLE);
    return Promise.resolve();
  },

  // Used for testing.
  changeTestServer(url, options = {}) {
    console.debug("changeTestServer()");

    return this._stateChangeProcessEnqueue(_ => {
      if (this._state < PUSH_SERVICE_ACTIVATING) {
        console.debug("changeTestServer: PushService not activated?");
        return Promise.resolve();
      }

      return this._changeServerURL(url, CHANGING_SERVICE_EVENT, options);
    });
  },

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      /*
       * We need to call uninit() on shutdown to clean up things that modules
       * aren't very good at automatically cleaning up, so we don't get shutdown
       * leaks on browser shutdown.
       */
      case "quit-application":
        this.uninit();
        break;
      case "network-active-changed":         /* On B2G. */
      case "network:offline-status-changed": /* On desktop. */
        this._stateChangeProcessEnqueue(_ =>
          this._changeStateOfflineEvent(aData === "offline", false)
        );
        break;

      case "nsPref:changed":
        if (aData == "dom.push.serverURL") {
          console.debug("observe: dom.push.serverURL changed for websocket",
                prefs.get("serverURL"));
          this._stateChangeProcessEnqueue(_ =>
            this._changeServerURL(prefs.get("serverURL"),
                                  CHANGING_SERVICE_EVENT)
          );

        } else if (aData == "dom.push.connection.enabled") {
          this._stateChangeProcessEnqueue(_ =>
            this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"))
          );
        }
        break;

      case "idle-daily":
        this._dropExpiredRegistrations().catch(error => {
          console.error("Failed to drop expired registrations on idle", error);
        });
        break;

      case "perm-changed":
        this._onPermissionChange(aSubject, aData).catch(error => {
          console.error("onPermissionChange: Error updating registrations:",
            error);
        })
        break;

      case "clear-origin-data":
        this._clearOriginData(aData).catch(error => {
          console.error("clearOriginData: Error clearing origin data:", error);
        });
        break;
    }
  },

  _clearOriginData: function(data) {
    console.log("clearOriginData()");

    if (!data) {
      return Promise.resolve();
    }

    let pattern = JSON.parse(data);
    return this._dropRegistrationsIf(record =>
      record.matchesOriginAttributes(pattern));
  },

  /**
   * Sends an unregister request to the server in the background. If the
   * service is not connected, this function is a no-op.
   *
   * @param {PushRecord} record The record to unregister.
   * @param {Number} reason An `nsIPushErrorReporter` unsubscribe reason,
   *  indicating why this record was removed.
   */
  _backgroundUnregister(record, reason) {
    console.debug("backgroundUnregister()");

    if (!this._service.isConnected() || !record) {
      return;
    }

    console.debug("backgroundUnregister: Notifying server", record);
    this._sendUnregister(record, reason).then(() => {
      gPushNotifier.notifySubscriptionModified(record.scope, record.principal);
    }).catch(e => {
      console.error("backgroundUnregister: Error notifying server", e);
    });
  },

  // utility function used to add/remove observers in startObservers() and
  // stopObservers()
  getNetworkStateChangeEventName: function() {
    try {
      let networkManager = Cc["@mozilla.org/network/manager;1"];
      if (networkManager) {
        networkManager.getService(Ci.nsINetworkManager);
        return "network-active-changed";
      }
    } catch (e) {}
    return "network:offline-status-changed";
  },

  _findService: function(serverURL) {
    console.debug("findService()");

    let uri;
    let service;

    if (!serverURL) {
      console.warn("findService: No dom.push.serverURL found");
      return [];
    }

    try {
      uri = Services.io.newURI(serverURL, null, null);
    } catch (e) {
      console.warn("findService: Error creating valid URI from",
        "dom.push.serverURL", serverURL);
      return [];
    }

    for (let connProtocol of CONNECTION_PROTOCOLS) {
      if (connProtocol.validServerURI(uri)) {
        service = connProtocol;
        break;
      }
    }
    return [service, uri];
  },

  _changeServerURL: function(serverURI, event, options = {}) {
    console.debug("changeServerURL()");

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
        return this._startService(service, uri, options)
          .then(_ => this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"))
          );
      }
      case CHANGING_SERVICE_EVENT:
        let [service, uri] = this._findService(serverURI);
        if (service) {
          if (this._state == PUSH_SERVICE_INIT) {
            this._setState(PUSH_SERVICE_ACTIVATING);
            // The service has not been running - start it.
            return this._startService(service, uri, options)
              .then(_ => this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"))
              );

          } else {
            this._setState(PUSH_SERVICE_ACTIVATING);
            // If we already had running service - stop service, start the new
            // one and check connection.enabled and offline state(offline state
            // check is called in changeStateConnectionEnabledEvent function)
            return this._stopService(CHANGING_SERVICE_EVENT)
              .then(_ =>
                 this._startService(service, uri, options)
              )
              .then(_ => this._changeStateConnectionEnabledEvent(prefs.get("connection.enabled"))
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
      default:
        console.error("Unexpected event in _changeServerURL", event);
        return Promise.reject(new Error(`Unexpected event ${event}`));
    }
  },

  /**
   * PushService initialization is divided into 4 parts:
   * init() - start listening for quit-application and serverURL changes.
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
    console.debug("init()");

    if (this._state > PUSH_SERVICE_UNINIT) {
      return;
    }

    this._setState(PUSH_SERVICE_ACTIVATING);

    Services.obs.addObserver(this, "quit-application", false);

    if (options.serverURI) {
      // this is use for xpcshell test.

      this._stateChangeProcessEnqueue(_ =>
        this._changeServerURL(options.serverURI, STARTING_SERVICE_EVENT, options));

    } else {
      // This is only used for testing. Different tests require connecting to
      // slightly different URLs.
      prefs.observe("serverURL", this);

      this._stateChangeProcessEnqueue(_ =>
        this._changeServerURL(prefs.get("serverURL"), STARTING_SERVICE_EVENT));
    }
  },

  _startObservers: function() {
    console.debug("startObservers()");

    if (this._state != PUSH_SERVICE_ACTIVATING) {
      return;
    }

    Services.obs.addObserver(this, "clear-origin-data", false);

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

    // Prunes expired registrations and notifies dormant service workers.
    Services.obs.addObserver(this, "idle-daily", false);

    // Prunes registrations for sites for which the user revokes push
    // permissions.
    Services.obs.addObserver(this, "perm-changed", false);
  },

  _startService(service, serverURI, options) {
    console.debug("startService()");

    if (this._state != PUSH_SERVICE_ACTIVATING) {
      return Promise.reject();
    }

    this._service = service;

    this._db = options.db;
    if (!this._db) {
      this._db = this._service.newPushDB();
    }

    return this._service.init(options, this, serverURI)
      .then(() => {
        this._startObservers();
        return this._dropExpiredRegistrations();
      });
  },

  /**
   * PushService uninitialization is divided into 3 parts:
   * stopObservers() - stot observers started in startObservers.
   * stopService() - It stops listening for broadcasted messages, stops db and
   *                 PushService connection (WebSocket).
   *                 state is changed to PUSH_SERVICE_INIT.
   * uninit() - stop listening for quit-application and serverURL changes.
   *            state is change to PUSH_SERVICE_UNINIT
   */
  _stopService: function(event) {
    console.debug("stopService()");

    if (this._state < PUSH_SERVICE_ACTIVATING) {
      return Promise.resolve();
    }

    this._stopObservers();

    this._service.disconnect();
    this._service.uninit();
    this._service = null;

    this._updateQuotaTimeouts.forEach((timeoutID) => clearTimeout(timeoutID));
    this._updateQuotaTimeouts.clear();

    if (!this._db) {
      return Promise.resolve();
    }
    if (event == UNINIT_EVENT) {
      // If it is uninitialized just close db.
      this._db.close();
      this._db = null;
      return Promise.resolve();
    }

    return this.dropUnexpiredRegistrations()
       .then(_ => {
         this._db.close();
         this._db = null;
       }, err => {
         this._db.close();
         this._db = null;
       });
  },

  _stopObservers: function() {
    console.debug("stopObservers()");

    if (this._state < PUSH_SERVICE_ACTIVATING) {
      return;
    }

    prefs.ignore("connection.enabled", this);

    Services.obs.removeObserver(this, this._networkStateChangeEventName);
    Services.obs.removeObserver(this, "clear-origin-data");
    Services.obs.removeObserver(this, "idle-daily");
    Services.obs.removeObserver(this, "perm-changed");
  },

  _shutdownService() {
    let promiseChangeURL = this._changeServerURL("", UNINIT_EVENT);
    this._setState(PUSH_SERVICE_UNINIT);
    console.debug("shutdownService: shutdown complete!");
    return promiseChangeURL;
  },

  uninit: function() {
    console.debug("uninit()");

    if (this._state == PUSH_SERVICE_UNINIT) {
      return;
    }

    prefs.ignore("serverURL", this);
    Services.obs.removeObserver(this, "quit-application");

    this._stateChangeProcessEnqueue(_ => this._shutdownService());
  },

  /**
   * Drops all active registrations and notifies the associated service
   * workers. This function is called when the user switches Push servers,
   * or when the server invalidates all existing registrations.
   *
   * We ignore expired registrations because they're already handled in other
   * code paths. Registrations that expired after exceeding their quotas are
   * evicted at startup, or on the next `idle-daily` event. Registrations that
   * expired because the user revoked the notification permission are evicted
   * once the permission is reinstated.
   */
  dropUnexpiredRegistrations: function() {
    return this._db.clearIf(record => {
      if (record.isExpired()) {
        return false;
      }
      this._notifySubscriptionChangeObservers(record);
      return true;
    });
  },

  _notifySubscriptionChangeObservers: function(record) {
    if (!record) {
      return;
    }

    Services.telemetry.getHistogramById("PUSH_API_NOTIFY_REGISTRATION_LOST").add();
    gPushNotifier.notifySubscriptionChange(record.scope, record.principal);
  },

  /**
   * Drops a registration and notifies the associated service worker. If the
   * registration does not exist, this function is a no-op.
   *
   * @param {String} keyID The registration ID to remove.
   * @returns {Promise} Resolves once the worker has been notified.
   */
  dropRegistrationAndNotifyApp: function(aKeyID) {
    return this._db.delete(aKeyID)
      .then(record => this._notifySubscriptionChangeObservers(record));
  },

  /**
   * Replaces an existing registration and notifies the associated service
   * worker.
   *
   * @param {String} aOldKey The registration ID to replace.
   * @param {PushRecord} aNewRecord The new record.
   * @returns {Promise} Resolves once the worker has been notified.
   */
  updateRegistrationAndNotifyApp: function(aOldKey, aNewRecord) {
    return this.updateRecordAndNotifyApp(aOldKey, _ => aNewRecord);
  },
  /**
   * Updates a registration and notifies the associated service worker.
   *
   * @param {String} keyID The registration ID to update.
   * @param {Function} updateFunc Returns the updated record.
   * @returns {Promise} Resolves with the updated record once the worker
   *  has been notified.
   */
  updateRecordAndNotifyApp: function(aKeyID, aUpdateFunc) {
    return this._db.update(aKeyID, aUpdateFunc)
      .then(record => {
        this._notifySubscriptionChangeObservers(record);
        return record;
      });
  },

  ensureCrypto: function(record) {
    if (record.hasAuthenticationSecret() &&
        record.p256dhPublicKey &&
        record.p256dhPrivateKey) {
      return Promise.resolve(record);
    }

    let keygen = Promise.resolve([]);
    if (!record.p256dhPublicKey || !record.p256dhPrivateKey) {
      keygen = PushCrypto.generateKeys();
    }
    // We do not have a encryption key. so we need to generate it. This
    // is only going to happen on db upgrade from version 4 to higher.
    return keygen
      .then(([pubKey, privKey]) => {
        return this.updateRecordAndNotifyApp(record.keyID, record => {
          if (!record.p256dhPublicKey || !record.p256dhPrivateKey) {
            record.p256dhPublicKey = pubKey;
            record.p256dhPrivateKey = privKey;
          }
          if (!record.hasAuthenticationSecret()) {
            record.authenticationSecret = PushCrypto.generateAuthenticationSecret();
          }
          return record;
        });
      }, error => {
        return this.dropRegistrationAndNotifyApp(record.keyID).then(
          () => Promise.reject(error));
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
   * @param {String} messageID The message ID, used to report service worker
   *  delivery failures. For Web Push messages, this is the version. If empty,
   *  failures will not be reported.
   * @param {ArrayBuffer|Uint8Array} data The encrypted message data.
   * @param {Object} cryptoParams The message encryption settings.
   * @param {Function} updateFunc A function that receives the existing
   *  registration record as its argument, and returns a new record. If the
   *  function returns `null` or `undefined`, the record will not be updated.
   *  `PushServiceWebSocket` uses this to drop incoming updates with older
   *  versions.
   * @returns {Promise} Resolves with an `nsIPushErrorReporter` ack status
   *  code, indicating whether the message was delivered successfully.
   */
  receivedPushMessage(keyID, messageID, data, cryptoParams, updateFunc) {
    console.debug("receivedPushMessage()");
    Services.telemetry.getHistogramById("PUSH_API_NOTIFICATION_RECEIVED").add();

    return this._updateRecordAfterPush(keyID, updateFunc).then(record => {
      if (!record) {
        throw new Error("Ignoring update for key ID " + keyID);
      }
      if (record.quotaApplies()) {
        // Update quota after the delay, at which point
        // we check for visible notifications.
        let timeoutID = setTimeout(_ =>
          {
            this._updateQuota(keyID);
            if (!this._updateQuotaTimeouts.delete(timeoutID)) {
              console.debug("receivedPushMessage: quota update timeout missing?");
            }
          }, prefs.get("quotaUpdateDelay"));
        this._updateQuotaTimeouts.add(timeoutID);
      }
      return this._decryptAndNotifyApp(record, messageID, data, cryptoParams);
    }).catch(error => {
      console.error("receivedPushMessage: Error notifying app", error);
      return Ci.nsIPushErrorReporter.ACK_NOT_DELIVERED;
    });
  },

  /**
   * Updates a registration record after receiving a push message.
   *
   * @param {String} keyID The push registration ID.
   * @param {Function} updateFunc The function passed to `receivedPushMessage`.
   * @returns {Promise} Resolves with the updated record, or `null` if the
   *  record was not updated.
   */
  _updateRecordAfterPush(keyID, updateFunc) {
    return this.getByKeyID(keyID).then(record => {
      if (!record) {
        this._recordDidNotNotify(kDROP_NOTIFICATION_REASON_KEY_NOT_FOUND);
        throw new Error("No record for key ID " + keyID);
      }
      return record.getLastVisit().then(lastVisit => {
        // As a special case, don't notify the service worker if the user
        // cleared their history.
        if (!isFinite(lastVisit)) {
          this._recordDidNotNotify(kDROP_NOTIFICATION_REASON_NO_HISTORY);
          throw new Error("Ignoring message sent to unvisited origin");
        }
        return lastVisit;
      }).then(lastVisit => {
        // Update the record, resetting the quota if the user has visited the
        // site since the last push.
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
            return null;
          }
          newRecord.receivedPush(lastVisit);
          return newRecord;
        });
      });
    }).then(record => {
      if (record) {
        gPushNotifier.notifySubscriptionModified(record.scope,
                                                 record.principal);
      }
      return record;
    });
  },

  /**
   * Decrypts a message. Will resolve with null if cryptoParams is falsy.
   *
   * @param {PushRecord} record The receiving registration.
   * @param {ArrayBuffer|Uint8Array} data The encrypted message data.
   * @param {Object} cryptoParams The message encryption settings.
   * @returns {Promise} Resolves with the decrypted message.
   */
  _decryptMessage(data, record, cryptoParams) {
    if (!cryptoParams) {
      return Promise.resolve(null);
    }
    return PushCrypto.decodeMsg(
      data,
      record.p256dhPrivateKey,
      record.p256dhPublicKey,
      cryptoParams.dh,
      cryptoParams.salt,
      cryptoParams.rs,
      record.authenticationSecret,
      cryptoParams.padSize
    );
  },

  /**
   * Decrypts an incoming message and notifies the associated service worker.
   *
   * @param {PushRecord} record The receiving registration.
   * @param {String} messageID The message ID.
   * @param {ArrayBuffer|Uint8Array} data The encrypted message data.
   * @param {Object} cryptoParams The message encryption settings.
   * @returns {Promise} Resolves with an ack status code.
   */
  _decryptAndNotifyApp(record, messageID, data, cryptoParams) {
    return this._decryptMessage(data, record, cryptoParams)
      .then(
        message => this._notifyApp(record, messageID, message),
        error => {
          let message = gDOMBundle.formatStringFromName(
            "PushMessageDecryptionFailure", [record.scope, String(error)], 2);
          gPushNotifier.notifyError(record.scope, record.principal, message,
                                    Ci.nsIScriptError.errorFlag);
          return Ci.nsIPushErrorReporter.ACK_DECRYPTION_ERROR;
        });
  },

  _updateQuota: function(keyID) {
    console.debug("updateQuota()");

    this._db.update(keyID, record => {
      // Record may have expired from an earlier quota update.
      if (record.isExpired()) {
        console.debug(
          "updateQuota: Trying to update quota for expired record", record);
        return null;
      }
      // If there are visible notifications, don't apply the quota penalty
      // for the message.
      if (record.uri && !this._visibleNotifications.has(record.uri.prePath)) {
        record.reduceQuota();
      }
      return record;
    }).then(record => {
      if (record) {
        if (record.isExpired()) {
          this._recordDidNotNotify(kDROP_NOTIFICATION_REASON_EXPIRED);
          // Drop the registration in the background. If the user returns to the
          // site, the service worker will be notified on the next `idle-daily`
          // event.
          this._backgroundUnregister(record,
            Ci.nsIPushErrorReporter.UNSUBSCRIBE_QUOTA_EXCEEDED);
        } else {
          gPushNotifier.notifySubscriptionModified(record.scope,
                                                   record.principal);
        }
      }
      if (this._updateQuotaTestCallback) {
        // Callback so that test may be notified when the quota update is complete.
        this._updateQuotaTestCallback();
      }
    }).catch(error => {
      console.debug("updateQuota: Error while trying to update quota", error);
    });
  },

  notificationForOriginShown(origin) {
    console.debug("notificationForOriginShown()", origin);
    let count;
    if (this._visibleNotifications.has(origin)) {
      count = this._visibleNotifications.get(origin);
    } else {
      count = 0;
    }
    this._visibleNotifications.set(origin, count + 1);
  },

  notificationForOriginClosed(origin) {
    console.debug("notificationForOriginClosed()", origin);
    let count;
    if (this._visibleNotifications.has(origin)) {
      count = this._visibleNotifications.get(origin);
    } else {
      console.debug("notificationForOriginClosed: closing notification that has not been shown?");
      return;
    }
    if (count > 1) {
      this._visibleNotifications.set(origin, count - 1);
    } else {
      this._visibleNotifications.delete(origin);
    }
  },

  reportDeliveryError(messageID, reason) {
    console.debug("reportDeliveryError()", messageID, reason);
    if (this._state == PUSH_SERVICE_RUNNING &&
        this._service.isConnected()) {

      // Only report errors if we're initialized and connected.
      this._service.reportDeliveryError(messageID, reason);
    }
  },

  _notifyApp(aPushRecord, messageID, message) {
    if (!aPushRecord || !aPushRecord.scope ||
        aPushRecord.originAttributes === undefined) {
      console.error("notifyApp: Invalid record", aPushRecord);
      return Ci.nsIPushErrorReporter.ACK_NOT_DELIVERED;
    }

    console.debug("notifyApp()", aPushRecord.scope);

    // If permission has been revoked, trash the message.
    if (!aPushRecord.hasPermission()) {
      console.warn("notifyApp: Missing push permission", aPushRecord);
      return Ci.nsIPushErrorReporter.ACK_NOT_DELIVERED;
    }

    let payload = ArrayBuffer.isView(message) ?
                  new Uint8Array(message.buffer) : message;

    if (aPushRecord.quotaApplies()) {
      // Don't record telemetry for chrome push messages.
      Services.telemetry.getHistogramById("PUSH_API_NOTIFY").add();
    }

    if (payload) {
      gPushNotifier.notifyPushWithData(aPushRecord.scope,
                                       aPushRecord.principal,
                                       messageID, payload.length, payload);
    } else {
      gPushNotifier.notifyPush(aPushRecord.scope, aPushRecord.principal,
                               messageID);
    }

    return Ci.nsIPushErrorReporter.ACK_DELIVERED;
  },

  getByKeyID: function(aKeyID) {
    return this._db.getByKeyID(aKeyID);
  },

  getAllUnexpired: function() {
    return this._db.getAllUnexpired();
  },

  _sendRequest(action, ...params) {
    if (this._state == PUSH_SERVICE_CONNECTION_DISABLE) {
      return Promise.reject(new Error("Push service disabled"));
    }
    if (this._state == PUSH_SERVICE_ACTIVE_OFFLINE) {
      return Promise.reject(new Error("Push service offline"));
    }
    // Ensure the backend is ready. `getByPageRecord` already checks this, but
    // we need to check again here in case the service was restarted in the
    // meantime.
    return this._checkActivated().then(_ => {
      switch (action) {
        case "register":
          return this._service.register(...params);
        case "unregister":
          return this._service.unregister(...params);
      }
      return Promise.reject(new Error("Unknown request type: " + action));
    });
  },

  /**
   * Called on message from the child process. aPageRecord is an object sent by
   * navigator.push, identifying the sending page and other fields.
   */
  _registerWithServer: function(aPageRecord) {
    console.debug("registerWithServer()", aPageRecord);

    Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_ATTEMPT").add();
    return this._sendRequest("register", aPageRecord)
      .then(record => this._onRegisterSuccess(record),
            err => this._onRegisterError(err))
      .then(record => {
        this._deletePendingRequest(aPageRecord);
        gPushNotifier.notifySubscriptionModified(record.scope,
                                                 record.principal);
        return record.toSubscription();
      }, err => {
        this._deletePendingRequest(aPageRecord);
        throw err;
     });
  },

  _sendUnregister(aRecord, aReason) {
    Services.telemetry.getHistogramById("PUSH_API_UNSUBSCRIBE_ATTEMPT").add();
    return this._sendRequest("unregister", aRecord, aReason).then(function(v) {
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
    console.debug("_onRegisterSuccess()");

    return this._db.put(aRecord)
      .then(record => {
        Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_SUCCEEDED").add();
        return record;
      })
      .catch(error => {
        Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_FAILED").add()
        // Unable to save. Destroy the subscription in the background.
        this._backgroundUnregister(aRecord,
                                   Ci.nsIPushErrorReporter.UNSUBSCRIBE_MANUAL);
        throw error;
      });
  },

  /**
   * Exceptions thrown in _onRegisterError are caught by the promise obtained
   * from _service.request, causing the promise to be rejected instead.
   */
  _onRegisterError: function(reply) {
    console.debug("_onRegisterError()");
    Services.telemetry.getHistogramById("PUSH_API_SUBSCRIBE_FAILED").add()
    if (!reply.error) {
      console.warn("onRegisterError: Called without valid error message!",
        reply);
      throw new Error("Registration error");
    }
    throw reply.error;
  },

  notificationsCleared() {
    this._visibleNotifications.clear();
  },

  _getByPageRecord(pageRecord) {
    return this._checkActivated().then(_ =>
      this._db.getByIdentifiers(pageRecord)
    );
  },

  register: function(aPageRecord) {
    console.debug("register()", aPageRecord);

    let keyPromise;
    if (aPageRecord.appServerKey) {
      let keyView = new Uint8Array(aPageRecord.appServerKey);
      keyPromise = PushCrypto.validateAppServerKey(keyView)
        .catch(error => {
          // Normalize Web Crypto exceptions. `nsIPushService` will forward the
          // error result to the DOM API implementation in `PushManager.cpp` or
          // `Push.js`, which will convert it to the correct `DOMException`.
          throw errorWithResult("Invalid app server key",
                                Cr.NS_ERROR_DOM_PUSH_INVALID_KEY_ERR);
        });
    } else {
      keyPromise = Promise.resolve(null);
    }

    return Promise.all([
      keyPromise,
      this._getByPageRecord(aPageRecord),
    ]).then(([appServerKey, record]) => {
      aPageRecord.appServerKey = appServerKey;
      if (!record) {
        return this._lookupOrPutPendingRequest(aPageRecord);
      }
      if (!record.matchesAppServerKey(appServerKey)) {
        throw errorWithResult("Mismatched app server key",
                              Cr.NS_ERROR_DOM_PUSH_MISMATCHED_KEY_ERR);
      }
      if (record.isExpired()) {
        return record.quotaChanged().then(isChanged => {
          if (isChanged) {
            // If the user revisited the site, drop the expired push
            // registration and re-register.
            return this.dropRegistrationAndNotifyApp(record.keyID);
          }
          throw new Error("Push subscription expired");
        }).then(_ => this._lookupOrPutPendingRequest(aPageRecord));
      }
      return record.toSubscription();
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
  unregister: function(aPageRecord) {
    console.debug("unregister()", aPageRecord);

    return this._getByPageRecord(aPageRecord)
      .then(record => {
        if (record === undefined) {
          return false;
        }

        let reason = Ci.nsIPushErrorReporter.UNSUBSCRIBE_MANUAL;
        return Promise.all([
          this._sendUnregister(record, reason),
          this._db.delete(record.keyID).then(record => {
            if (record) {
              gPushNotifier.notifySubscriptionModified(record.scope,
                                                       record.principal);
            }
          }),
        ]).then(([success]) => success);
      });
  },

  clear: function(info) {
    return this._checkActivated()
      .then(_ => {
        return this._dropRegistrationsIf(record =>
          info.domain == "*" ||
          (record.uri && hasRootDomain(record.uri.prePath, info.domain))
        );
      })
      .catch(e => {
        console.warn("clear: Error dropping subscriptions for domain",
          info.domain, e);
        return Promise.resolve();
      });
  },

  registration: function(aPageRecord) {
    console.debug("registration()");

    return this._getByPageRecord(aPageRecord)
      .then(record => {
        if (!record) {
          return null;
        }
        if (record.isExpired()) {
          return record.quotaChanged().then(isChanged => {
            if (isChanged) {
              return this.dropRegistrationAndNotifyApp(record.keyID).then(_ => null);
            }
            return null;
          });
        }
        return record.toSubscription();
      });
  },

  _dropExpiredRegistrations: function() {
    console.debug("dropExpiredRegistrations()");

    return this._db.getAllExpired().then(records => {
      return Promise.all(records.map(record =>
        record.quotaChanged().then(isChanged => {
          if (isChanged) {
            // If the user revisited the site, drop the expired push
            // registration and notify the associated service worker.
            return this.dropRegistrationAndNotifyApp(record.keyID);
          }
        }).catch(error => {
          console.error("dropExpiredRegistrations: Error dropping registration",
            record.keyID, error);
        })
      ));
    });
  },

  _onPermissionChange: function(subject, data) {
    console.debug("onPermissionChange()");

    if (data == "cleared") {
      return this._clearPermissions();
    }

    let permission = subject.QueryInterface(Ci.nsIPermission);
    if (permission.type != "desktop-notification") {
      return Promise.resolve();
    }

    return this._updatePermission(permission, data);
  },

  _clearPermissions() {
    console.debug("clearPermissions()");

    return this._db.clearIf(record => {
      if (!record.quotaApplies()) {
        // Only drop registrations that are subject to quota.
        return false;
      }
      this._backgroundUnregister(record,
        Ci.nsIPushErrorReporter.UNSUBSCRIBE_PERMISSION_REVOKED);
      return true;
    });
  },

  _updatePermission: function(permission, type) {
    console.debug("updatePermission()");

    let isAllow = permission.capability ==
                  Ci.nsIPermissionManager.ALLOW_ACTION;
    let isChange = type == "added" || type == "changed";

    if (isAllow && isChange) {
      // Permission set to "allow". Drop all expired registrations for this
      // site, notify the associated service workers, and reset the quota
      // for active registrations.
      return this._forEachPrincipal(
        permission.principal,
        (record, cursor) => this._permissionAllowed(record, cursor)
      );
    } else if (isChange || (isAllow && type == "deleted")) {
      // Permission set to "block" or "always ask," or "allow" permission
      // removed. Expire all registrations for this site.
      return this._forEachPrincipal(
        permission.principal,
        (record, cursor) => this._permissionDenied(record, cursor)
      );
    }

    return Promise.resolve();
  },

  _forEachPrincipal: function(principal, callback) {
    return this._db.forEachOrigin(
      principal.URI.prePath,
      ChromeUtils.originAttributesToSuffix(principal.originAttributes),
      callback
    );
  },

  /**
   * The update function called for each registration record if the push
   * permission is revoked. We only expire the record so we can notify the
   * service worker as soon as the permission is reinstated. If we just
   * deleted the record, the worker wouldn't be notified until the next visit
   * to the site.
   *
   * @param {PushRecord} record The record to expire.
   * @param {IDBCursor} cursor The IndexedDB cursor.
   */
  _permissionDenied: function(record, cursor) {
    console.debug("permissionDenied()");

    if (!record.quotaApplies() || record.isExpired()) {
      // Ignore already-expired records.
      return;
    }
    // Drop the registration in the background.
    this._backgroundUnregister(record,
      Ci.nsIPushErrorReporter.UNSUBSCRIBE_PERMISSION_REVOKED);
    record.setQuota(0);
    cursor.update(record);
  },

  /**
   * The update function called for each registration record if the push
   * permission is granted. If the record has expired, it will be dropped;
   * otherwise, its quota will be reset to the default value.
   *
   * @param {PushRecord} record The record to update.
   * @param {IDBCursor} cursor The IndexedDB cursor.
   */
  _permissionAllowed(record, cursor) {
    console.debug("permissionAllowed()");

    if (!record.quotaApplies()) {
      return;
    }
    if (record.isExpired()) {
      // If the registration has expired, drop and notify the worker
      // unconditionally.
      this._notifySubscriptionChangeObservers(record);
      cursor.delete();
      return;
    }
    record.resetQuota();
    cursor.update(record);
  },

  /**
   * Drops all matching registrations from the database. Notifies the
   * associated service workers if permission is granted, and removes
   * unexpired registrations from the server.
   *
   * @param {Function} predicate A function called for each record.
   * @returns {Promise} Resolves once the registrations have been dropped.
   */
  _dropRegistrationsIf(predicate) {
    return this._db.clearIf(record => {
      if (!predicate(record)) {
        return false;
      }
      if (record.hasPermission()) {
        // "Clear Recent History" and the Forget button remove permissions
        // before clearing registrations, but it's possible for the worker to
        // resubscribe if the "dom.push.testing.ignorePermission" pref is set.
        this._notifySubscriptionChangeObservers(record);
      }
      if (!record.isExpired()) {
        // Only unregister active registrations, since we already told the
        // server about expired ones.
        this._backgroundUnregister(record,
                                   Ci.nsIPushErrorReporter.UNSUBSCRIBE_MANUAL);
      }
      return true;
    });
  },
};
