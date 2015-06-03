/* jshint moz: true, esnext: true */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// Don't modify this, instead set dom.push.debug.
let gDebuggingEnabled = true;

function debug(s) {
  if (gDebuggingEnabled)
    dump("-*- PushService.jsm: " + s + "\n");
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

XPCOMUtils.defineLazyModuleGetter(this, "AlarmService",
                                  "resource://gre/modules/AlarmService.jsm");

this.EXPORTED_SYMBOLS = ["PushService"];

const prefs = new Preferences("dom.push.");
// Set debug first so that all debugging actually works.
gDebuggingEnabled = prefs.get("debug");

const kPUSHWSDB_DB_NAME = "pushapi";
const kPUSHWSDB_DB_VERSION = 1; // Change this if the IndexedDB format changes
const kPUSHWSDB_STORE_NAME = "pushapi";

const kCHILD_PROCESS_MESSAGES = ["Push:Register", "Push:Unregister",
                                 "Push:Registration"];

var upgradeSchemaWS = function(aTransaction, aDb, aOldVersion, aNewVersion, aDbInstance) {
  debug("upgradeSchemaWS()");

  let objectStore = aDb.createObjectStore(aDbInstance._dbStoreName,
                                          { keyPath: "channelID" });

  // index to fetch records based on endpoints. used by unregister
  objectStore.createIndex("pushEndpoint", "pushEndpoint", { unique: true });

  // index to fetch records per scope, so we can identify endpoints
  // associated with an app.
  objectStore.createIndex("scope", "scope", { unique: true });
};

/**
 * The implementation of the SimplePush system. This runs in the B2G parent
 * process and is started on boot. It uses WebSockets to communicate with the
 * server and PushDB (IndexedDB) for persistence.
 */
this.PushService = {
  _service: null,

  observe: function observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      /*
       * We need to call uninit() on shutdown to clean up things that modules aren't very good
       * at automatically cleaning up, so we don't get shutdown leaks on browser shutdown.
       */
      case "xpcom-shutdown":
        this.uninit();
        break;
      case "network-active-changed":         /* On B2G. */
      case "network:offline-status-changed": /* On desktop. */
        this._service.observeNetworkChange(aSubject, aTopic, aData);
        break;
      case "nsPref:changed":
        if (aData == "dom.push.serverURL") {
          debug("dom.push.serverURL changed! websocket. new value " +
                prefs.get("serverURL"));
          this._service.shutdownService();
        } else if (aData == "dom.push.connection.enabled") {
          this._service.observePushConnectionPref(prefs.get("connection.enabled"));
        } else if (aData == "dom.push.debug") {
          gDebuggingEnabled = prefs.get("debug");
          this._service.observeDebug(gDebuggingEnabled);
          this._db.observeDebug(gDebuggingEnabled);
        }
        break;
      case "timer-callback":
        this._service.observeTimer(aSubject, aTopic, aData);
        break;
      case "webapps-clear-data":
        debug("webapps-clear-data");

        let data = aSubject.QueryInterface(Ci.mozIApplicationClearPrivateDataParams);
        if (!data) {
          debug("webapps-clear-data: Failed to get information about application");
          return;
        }

        // TODO 1149274.  We should support site permissions as well as a way to go from manifest
        // url to 'all scopes registered for push in this app'
        let appsService = Cc["@mozilla.org/AppsService;1"]
                            .getService(Ci.nsIAppsService);
        let scope = appsService.getScopeByLocalId(data.appId);
        if (!scope) {
          debug("webapps-clear-data: No scope found for " + data.appId);
          return;
        }

        this._db.getByScope(scope)
          .then(record =>
            this._db.delete(records.channelID)
              .then(_ =>
                this._service.unregister(records),
                err => {
                  debug("webapps-clear-data: " + scope +
                        " Could not delete entry " + records.channelID);

                this._service.unregister(records)
                throw "Database error";
              })
          , _ => {
            debug("webapps-clear-data: Error in getByScope(" + scope + ")");
          });

        break;
    }
  },

  // utility function used to add/remove observers in init() and shutdown()
  getNetworkStateChangeEventName: function() {
    try {
      Cc["@mozilla.org/network/manager;1"].getService(Ci.nsINetworkManager);
      return "network-active-changed";
    } catch (e) {
      return "network:offline-status-changed";
    }
  },

  init: function(options = {}) {
    debug("init()");
    if (this._started) {
      return;
    }

    var globalMM = Cc["@mozilla.org/globalmessagemanager;1"]
               .getService(Ci.nsIFrameScriptLoader);

    globalMM.loadFrameScript("chrome://global/content/PushServiceChildPreload.js", true);

    let ppmm = Cc["@mozilla.org/parentprocessmessagemanager;1"]
                 .getService(Ci.nsIMessageBroadcaster);

    kCHILD_PROCESS_MESSAGES.forEach(function addMessage(msgName) {
        ppmm.addMessageListener(msgName, this);
    }.bind(this));

    this._alarmID = null;

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
    this._networkStateChangeEventName = this.getNetworkStateChangeEventName();
    Services.obs.addObserver(this, this._networkStateChangeEventName, false);

    // This is only used for testing. Different tests require connecting to
    // slightly different URLs.
    prefs.observe("serverURL", this);
    // Used to monitor if the user wishes to disable Push.
    prefs.observe("connection.enabled", this);
    // Debugging
    prefs.observe("debug", this);
    this._db = options.db;
    if (!this._db) {
      this._db = new PushDB(kPUSHWSDB_DB_NAME, kPUSHWSDB_DB_VERSION, kPUSHWSDB_STORE_NAME, upgradeSchemaWS);
    }
    this._service = PushServiceWebSocket;
    this._service.init(options, this);
    this._service.startListeningIfChannelsPresent();
    this._started = true;
  },

  uninit: function() {
    if (!this._started)
      return;

    debug("uninit()");

    prefs.ignore("debug", this);
    prefs.ignore("connection.enabled", this);
    prefs.ignore("serverURL", this);
    Services.obs.removeObserver(this, this._networkStateChangeEventName);
    Services.obs.removeObserver(this, "webapps-clear-data", false);
    Services.obs.removeObserver(this, "xpcom-shutdown", false);

    // At this point, profile-change-net-teardown has already fired, so the
    // WebSocket has been closed with NS_ERROR_ABORT (if it was up) and will
    // try to reconnect. Stop the timer.
    this.stopAlarm();

    if (this._db) {
      this._db.close();
      this._db = null;
    }

    this._service.uninit();

    this._started = false;
    debug("shutdown complete!");
  },

  getServerURI: function() {
    let serverURL = prefs.get("serverURL");
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
      return;
    }
    return uri;
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
      this._service.onAlarmFired.bind(this._service),
      function onSuccess(alarmID) {
        this._alarmID = alarmID;
        debug("Set alarm " + delay + " in the future " + this._alarmID);
        this._settingAlarm = false;

        if (this._waitingForAlarmSet) {
          this._waitingForAlarmSet = false;
          this.setAlarm(this._queuedAlarmDelay);
        }
      }.bind(this)
    )
  },

  stopAlarm: function() {
    if (this._alarmID !== null) {
      debug("Stopped existing alarm " + this._alarmID);
      AlarmService.remove(this._alarmID);
      this._alarmID = null;
    }
  },

  // Fires a push-register system message to all applications that have
  // registration.
  notifyAllAppsRegister: function() {
    debug("notifyAllAppsRegister()");
    // records are objects describing the registration as stored in IndexedDB.
    return this._db.getAllChannelIDs()
      .then(records => {
        let scopes = new Set();
        for (let record of records) {
          scopes.add(record.scope);
        }
        let globalMM = Cc['@mozilla.org/globalmessagemanager;1'].getService(Ci.nsIMessageListenerManager);
        for (let scope of scopes) {
          // Notify XPCOM observers.
          Services.obs.notifyObservers(
            null,
            "push-subscription-change",
            scope
          );

          let data = {
            originAttributes: {}, // TODO bug 1166350
            scope: scope
          };

          globalMM.broadcastAsyncMessage('pushsubscriptionchange', data);
        }
      });
  },

  notifyApp: function(aPushRecord) {
    if (!aPushRecord || !aPushRecord.scope) {
      debug("notifyApp() something is undefined.  Dropping notification: "
        + JSON.stringify(aPushRecord) );
      return;
    }

    debug("notifyApp() " + aPushRecord.scope);
    let scopeURI = Services.io.newURI(aPushRecord.scope, null, null);
    // Notify XPCOM observers.
    let notification = Cc["@mozilla.org/push/ObserverNotification;1"]
                         .createInstance(Ci.nsIPushObserverNotification);
    notification.pushEndpoint = aPushRecord.pushEndpoint;
    notification.version = aPushRecord.version;
    notification.data = "";
    notification.lastPush = aPushRecord.lastPush;
    notification.pushCount = aPushRecord.pushCount;

    Services.obs.notifyObservers(
      notification,
      "push-notification",
      aPushRecord.scope
    );

    // If permission has been revoked, trash the message.
    if(Services.perms.testExactPermission(scopeURI, "push") != Ci.nsIPermissionManager.ALLOW_ACTION) {
      debug("Does not have permission for push.")
      return;
    }

    // TODO data.
    let data = {
      payload: "Short as life is, we make it still shorter by the careless waste of time.",
      originAttributes: {}, // TODO bug 1166350
      scope: aPushRecord.scope
    };

    let globalMM = Cc['@mozilla.org/globalmessagemanager;1']
                 .getService(Ci.nsIMessageListenerManager);
    globalMM.broadcastAsyncMessage('push', data);
  },

  updatePushRecord: function(aPushRecord) {
    debug("updatePushRecord()");
    return this._db.put(aPushRecord);
  },

  getByChannelID: function(aChannelID) {
    return this._db.getByChannelID(aChannelID)
  },

  getAllChannelIDs: function() {
    return this._db.getAllChannelIDs()
  },

  dropRegistration: function() {
    return this._db.drop();
  },

    /**
   * Called on message from the child process. aPageRecord is an object sent by
   * navigator.push, identifying the sending page and other fields.
   */
  _registerWithServer: function(channelID, aPageRecord) {
    debug("registerWithServer()");

    return this._service.register(channelID)
      .then(
        this._onRegisterSuccess.bind(this, aPageRecord, channelID),
        this._onRegisterError.bind(this)
      );
  },

  _generateID: function() {
    let uuidGenerator = Cc["@mozilla.org/uuid-generator;1"]
                          .getService(Ci.nsIUUIDGenerator);
    // generateUUID() gives a UUID surrounded by {...}, slice them off.
    return uuidGenerator.generateUUID().toString().slice(1, -1);
  },

  _register: function(aPageRecord) {
    return this._db.getByScope(aPageRecord.scope)
      .then(pushRecord => {
        if (pushRecord == null) {
          let channelID = this._generateID();
          return this._registerWithServer(channelID, aPageRecord);
        }
        return pushRecord;
      },
      error => {
        debug("getByScope failed");
        throw "Database error";
      }
    );
  },

  /**
   * Exceptions thrown in _onRegisterSuccess are caught by the promise obtained
   * from _sendRequest, causing the promise to be rejected instead.
   */
  _onRegisterSuccess: function(aPageRecord, generatedChannelID, data) {
    debug("_onRegisterSuccess()");

    if (typeof data.channelID !== "string") {
      debug("Invalid channelID " + data.channelID);
      throw "Invalid channelID received";
    }
    else if (data.channelID != generatedChannelID) {
      debug("Server replied with different channelID " + data.channelID +
            " than what UA generated " + generatedChannelID);
      throw "Server sent 200 status code but different channelID";
    }

    try {
      Services.io.newURI(data.pushEndpoint, null, null);
    }
    catch (e) {
      debug("Invalid pushEndpoint " + data.pushEndpoint);
      throw "Invalid pushEndpoint " + data.pushEndpoint;
    }

    let record = {
      channelID: data.channelID,
      pushEndpoint: data.pushEndpoint,
      pageURL: aPageRecord.pageURL,
      scope: aPageRecord.scope,
      pushCount: 0,
      lastPush: 0,
      version: null
    };

    debug("scope in _onRegisterSuccess: " + aPageRecord.scope)

    return this.updatePushRecord(record)
      .then(
        function() {
          return record;
        },
        function(error) {
          // Unable to save.
          this._service.unregister(record);
          throw error;
        }.bind(this)
      );
  },

  /**
   * Exceptions thrown in _onRegisterError are caught by the promise obtained
   * from _sendRequest, causing the promise to be rejected instead.
   */
  _onRegisterError: function(reply) {
    debug("_onRegisterError()");
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

    let mm = aMessage.target.QueryInterface(Ci.nsIMessageSender);
    let json = aMessage.data;
    this[aMessage.name.slice("Push:".length).toLowerCase()](json, mm);
  },

  register: function(aPageRecord, aMessageManager) {
    debug("register(): " + JSON.stringify(aPageRecord));

    this._register(aPageRecord).then(
      function(aPageRecord, aMessageManager, pushRecord) {
        let message = {
          requestID: aPageRecord.requestID,
          pushEndpoint: pushRecord.pushEndpoint
        };
        aMessageManager.sendAsyncMessage("PushService:Register:OK", message);
      }.bind(this, aPageRecord, aMessageManager),
      function(error) {
        let message = {
          requestID: aPageRecord.requestID,
          error
        };
        aMessageManager.sendAsyncMessage("PushService:Register:KO", message);
      }
    );
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
  _unregister: function(aPageRecord) {
    debug("unregisterWithServer()");

    if (!aPageRecord.scope) {
      return Promise.reject("NotFoundError");
    }

    return this._db.getByScope(aPageRecord.scope)
      .then(record => {
        // If the endpoint didn't exist, let's just fail.
        if (record === undefined) {
          throw "NotFoundError";
        }

        this._db.delete(record.channelID)
          .then(_ =>
            // Let's be nice to the server and try to inform it, but we don't care
            // about the reply.
            this._service.unregister(record)
          );
      });
  },

  unregister: function(aPageRecord, aMessageManager) {
    debug("unregister() " + JSON.stringify(aPageRecord));

    this._unregister(aPageRecord).then(
      () => {
        aMessageManager.sendAsyncMessage("PushService:Unregister:OK", {
          requestID: aPageRecord.requestID,
          pushEndpoint: aPageRecord.pushEndpoint
        });
      },
      error => {
        aMessageManager.sendAsyncMessage("PushService:Unregister:KO", {
          requestID: aPageRecord.requestID,
          error
        });
      }
    );
  },

  _clearAll: function _clearAll() {
    return this._db.clearAll();
  },

  /**
   * Called on message from the child process
   */
  _registration: function(aPageRecord) {
    if (!aPageRecord.scope) {
      return Promise.reject("Database error");
    }

    return this._db.getByScope(aPageRecord.scope)
        .then(pushRecord => {
          let registration = null;
          if (pushRecord) {
            registration = {
              pushEndpoint: pushRecord.pushEndpoint,
              version: pushRecord.version,
              lastPush: pushRecord.lastPush,
              pushCount: pushRecord.pushCount
            };
          }
          return registration;
        }, _ => {
          throw "Database error";
        });
  },

  registration: function(aPageRecord, aMessageManager) {
    debug("registration()");

    return this._registration(aPageRecord).then(
      registration => {
        aMessageManager.sendAsyncMessage("PushService:Registration:OK", {
          requestID: aPageRecord.requestID,
          registration
        });
      },
      error => {
        aMessageManager.sendAsyncMessage("PushService:Registration:KO", {
          requestID: aPageRecord.requestID,
          error
        });
      }
    );
  }
};
