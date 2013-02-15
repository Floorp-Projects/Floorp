/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* static functions */
const DEBUG = false;

function debug(aStr) {
  if (DEBUG)
    dump("AlarmService: " + aStr + "\n");
}

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/AlarmDB.jsm");

this.EXPORTED_SYMBOLS = ["AlarmService"];

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageListenerManager");

XPCOMUtils.defineLazyGetter(this, "messenger", function() {
  return Cc["@mozilla.org/system-message-internal;1"].getService(Ci.nsISystemMessagesInternal);
});

XPCOMUtils.defineLazyGetter(this, "powerManagerService", function() {
  return Cc["@mozilla.org/power/powermanagerservice;1"].getService(Ci.nsIPowerManagerService);
});

let myGlobal = this;

this.AlarmService = {
  init: function init() {
    debug("init()");

    this._currentTimezoneOffset = (new Date()).getTimezoneOffset();

    let alarmHalService = this._alarmHalService = Cc["@mozilla.org/alarmHalService;1"].getService(Ci.nsIAlarmHalService);
    alarmHalService.setAlarmFiredCb(this._onAlarmFired.bind(this));
    alarmHalService.setTimezoneChangedCb(this._onTimezoneChanged.bind(this));

    // add the messages to be listened
    const messages = ["AlarmsManager:GetAll",
                      "AlarmsManager:Add",
                      "AlarmsManager:Remove"];
    messages.forEach(function addMessage(msgName) {
        ppmm.addMessageListener(msgName, this);
    }.bind(this));

    // set the indexeddb database
    let idbManager = Cc["@mozilla.org/dom/indexeddb/manager;1"].getService(Ci.nsIIndexedDatabaseManager);
    idbManager.initWindowless(myGlobal);
    this._db = new AlarmDB(myGlobal);
    this._db.init(myGlobal);

    // variable to save alarms waiting to be set
    this._alarmQueue = [];

    this._restoreAlarmsFromDb();
  },

  // getter/setter to access the current alarm set in system
  _alarm: null,
  get _currentAlarm() {
    return this._alarm;
  },
  set _currentAlarm(aAlarm) {
    this._alarm = aAlarm;
    if (!aAlarm)
      return;

    if (!this._alarmHalService.setAlarm(this._getAlarmTime(aAlarm) / 1000, 0))
      throw Components.results.NS_ERROR_FAILURE;
  },

  receiveMessage: function receiveMessage(aMessage) {
    debug("receiveMessage(): " + aMessage.name);
    let json = aMessage.json;

    // To prevent the hacked child process from sending commands to parent
    // to schedule alarms, we need to check its permission and manifest URL.
    if (["AlarmsManager:GetAll", "AlarmsManager:Add", "AlarmsManager:Remove"]
          .indexOf(aMessage.name) != -1) {
      if (!aMessage.target.assertPermission("alarms")) {
        debug("Got message from a child process with no 'alarms' permission.");
        return null;
      }
      if (!aMessage.target.assertContainApp(json.manifestURL)) {
        debug("Got message from a child process containing illegal manifest URL.");
        return null;
      }
    }

    let mm = aMessage.target.QueryInterface(Ci.nsIMessageSender);
    switch (aMessage.name) {
      case "AlarmsManager:GetAll":
        this._db.getAll(
          json.manifestURL,
          function getAllSuccessCb(aAlarms) {
            debug("Callback after getting alarms from database: " + JSON.stringify(aAlarms));
            this._sendAsyncMessage(mm, "GetAll", true, json.requestId, aAlarms);
          }.bind(this),
          function getAllErrorCb(aErrorMsg) {
            this._sendAsyncMessage(mm, "GetAll", false, json.requestId, aErrorMsg);
          }.bind(this)
        );
        break;

      case "AlarmsManager:Add":
        // prepare a record for the new alarm to be added
        let newAlarm = {
          date: json.date, 
          ignoreTimezone: json.ignoreTimezone, 
          timezoneOffset: this._currentTimezoneOffset, 
          data: json.data,
          pageURL: json.pageURL,
          manifestURL: json.manifestURL
        };

        let newAlarmTime = this._getAlarmTime(newAlarm);
        if (newAlarmTime <= Date.now()) {
          debug("Adding a alarm that has past time. Return DOMError.");
          this._debugCurrentAlarm();
          this._sendAsyncMessage(mm, "Add", false, json.requestId, "InvalidStateError");
          break;
        }

        this._db.add(
          newAlarm,
          function addSuccessCb(aNewId) {
            debug("Callback after adding alarm in database.");

            newAlarm['id'] = aNewId;

            // if there is no alarm being set in system, set the new alarm
            if (this._currentAlarm == null) {
              this._currentAlarm = newAlarm;
              this._debugCurrentAlarm();
              this._sendAsyncMessage(mm, "Add", true, json.requestId, aNewId);
              return;
            }

            // if the new alarm is earlier than the current alarm
            // swap them and push the previous alarm back to queue
            let alarmQueue = this._alarmQueue;
            let currentAlarmTime = this._getAlarmTime(this._currentAlarm);
            if (newAlarmTime < currentAlarmTime) {
              alarmQueue.unshift(this._currentAlarm);
              this._currentAlarm = newAlarm;
              this._debugCurrentAlarm();
              this._sendAsyncMessage(mm, "Add", true, json.requestId, aNewId);
              return;
            }

            //push the new alarm in the queue
            alarmQueue.push(newAlarm);
            alarmQueue.sort(this._sortAlarmByTimeStamps.bind(this));
            this._debugCurrentAlarm();
            this._sendAsyncMessage(mm, "Add", true, json.requestId, aNewId);
          }.bind(this),
          function addErrorCb(aErrorMsg) {
            this._sendAsyncMessage(mm, "Add", false, json.requestId, aErrorMsg);
          }.bind(this)
        );
        break;

      case "AlarmsManager:Remove":
        this._removeAlarmFromDb(
          json.id,
          json.manifestURL,
          function removeSuccessCb() {
            debug("Callback after removing alarm from database.");

            // if there is no alarm being set
            if (!this._currentAlarm) {
              this._debugCurrentAlarm();
              return;
            }

            // check if the alarm to be removed is in the queue
            // by ID and whether it belongs to the requesting app
            let alarmQueue = this._alarmQueue;
            if (this._currentAlarm.id != json.id || 
                this._currentAlarm.manifestURL != json.manifestURL) {
              for (let i = 0; i < alarmQueue.length; i++) {
                if (alarmQueue[i].id == json.id && 
                    alarmQueue[i].manifestURL == json.manifestURL) {
                  alarmQueue.splice(i, 1);
                  break;
                }
              }
              this._debugCurrentAlarm();
              return;
            }

            // the alarm to be removed is the current alarm
            // reset the next alarm from queue if any
            if (alarmQueue.length) {
              this._currentAlarm = alarmQueue.shift();
              this._debugCurrentAlarm();
              return;
            }

            // no alarm waiting to be set in the queue
            this._currentAlarm = null;
            this._debugCurrentAlarm();
          }.bind(this)
        );
        break;

      default:
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        break;
    }
  },

  _sendAsyncMessage: function _sendAsyncMessage(aMessageManager, aMessageName, aSuccess, aRequestId, aData) {
    debug("_sendAsyncMessage()");

    if (!aMessageManager) {
      debug("Invalid message manager: null");
      throw Components.results.NS_ERROR_FAILURE;
    }

    let json = null;
    switch (aMessageName)
    {
      case "Add":
        json = aSuccess ? 
          { requestId: aRequestId, id: aData } : 
          { requestId: aRequestId, errorMsg: aData };
        break;

      case "GetAll":
        json = aSuccess ? 
          { requestId: aRequestId, alarms: aData } : 
          { requestId: aRequestId, errorMsg: aData };
        break;

      default:
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
        break;
    }

    aMessageManager.sendAsyncMessage("AlarmsManager:" + aMessageName + ":Return:" + (aSuccess ? "OK" : "KO"), json);
  },

  _removeAlarmFromDb: function _removeAlarmFromDb(aId, aManifestURL, aRemoveSuccessCb) {
    debug("_removeAlarmFromDb()");

    // If the aRemoveSuccessCb is undefined or null, set a 
    // dummy callback for it which is needed for _db.remove()
    if (!aRemoveSuccessCb) {
      aRemoveSuccessCb = function removeSuccessCb() {
        debug("Remove alarm from DB successfully.");
      };
    }

    this._db.remove(
      aId,
      aManifestURL,
      aRemoveSuccessCb,
      function removeErrorCb(aErrorMsg) {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
      }
    );
  },

  _fireSystemMessage: function _fireSystemMessage(aAlarm) {
    debug("Fire system message: " + JSON.stringify(aAlarm));

    let manifestURI = Services.io.newURI(aAlarm.manifestURL, null, null);
    let pageURI = Services.io.newURI(aAlarm.pageURL, null, null);

    // We don't need to expose everything to the web content.
    let alarm = { "id":              aAlarm.id,
                  "date":            aAlarm.date,
                  "respectTimezone": aAlarm.ignoreTimezone ?
                                       "ignoreTimezone" : "honorTimezone", 
                  "data":            aAlarm.data };

    messenger.sendMessage("alarm", alarm, pageURI, manifestURI);
  },

  _onAlarmFired: function _onAlarmFired() {
    debug("_onAlarmFired()");

    if (this._currentAlarm) {
      this._fireSystemMessage(this._currentAlarm);
      this._removeAlarmFromDb(this._currentAlarm.id, null);
      this._currentAlarm = null;
    }

    // Reset the next alarm from the queue.
    let alarmQueue = this._alarmQueue;
    while (alarmQueue.length > 0) {
      let nextAlarm = alarmQueue.shift();
      let nextAlarmTime = this._getAlarmTime(nextAlarm);

      // If the next alarm has been expired, directly 
      // fire system message for it instead of setting it.
      if (nextAlarmTime <= Date.now()) {
        this._fireSystemMessage(nextAlarm);
        this._removeAlarmFromDb(nextAlarm.id, null);
      } else {
        this._currentAlarm = nextAlarm;
        break;
      }
    }
    this._debugCurrentAlarm();
  },

  _onTimezoneChanged: function _onTimezoneChanged(aTimezoneOffset) {
    debug("_onTimezoneChanged()");

    this._currentTimezoneOffset = aTimezoneOffset;
    this._restoreAlarmsFromDb();
  },

  _restoreAlarmsFromDb: function _restoreAlarmsFromDb() {
    debug("_restoreAlarmsFromDb()");

    this._db.getAll(
      null,
      function getAllSuccessCb(aAlarms) {
        debug("Callback after getting alarms from database: " + JSON.stringify(aAlarms));

        // clear any alarms set or queued in the cache
        let alarmQueue = this._alarmQueue;
        alarmQueue.length = 0;
        this._currentAlarm = null;

        // Only restore the alarm that's not yet expired; otherwise,
        // fire a system message for it and remove it from database.
        aAlarms.forEach(function addAlarm(aAlarm) {
          if (this._getAlarmTime(aAlarm) > Date.now()) {
            alarmQueue.push(aAlarm);
          } else {
            this._fireSystemMessage(aAlarm);
            this._removeAlarmFromDb(aAlarm.id, null);
          }
        }.bind(this));

        // set the next alarm from queue
        if (alarmQueue.length) {
          alarmQueue.sort(this._sortAlarmByTimeStamps.bind(this));
          this._currentAlarm = alarmQueue.shift();
        }

        this._debugCurrentAlarm();
      }.bind(this),
      function getAllErrorCb(aErrorMsg) {
        throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
      }
    );
  },

  _getAlarmTime: function _getAlarmTime(aAlarm) {
    let alarmTime = (new Date(aAlarm.date)).getTime();

    // For an alarm specified with "ignoreTimezone",
    // it must be fired respect to the user's timezone.
    // Supposing an alarm was set at 7:00pm at Tokyo,
    // it must be gone off at 7:00pm respect to Paris' 
    // local time when the user is located at Paris.
    // We can adjust the alarm UTC time by calculating
    // the difference of the orginal timezone and the 
    // current timezone.
    if (aAlarm.ignoreTimezone)
       alarmTime += (this._currentTimezoneOffset - aAlarm.timezoneOffset) * 60000;

    return alarmTime;
  },

  _sortAlarmByTimeStamps: function _sortAlarmByTimeStamps(aAlarm1, aAlarm2) {
    return this._getAlarmTime(aAlarm1) - this._getAlarmTime(aAlarm2);
  },

  _debugCurrentAlarm: function _debugCurrentAlarm() {
    debug("Current alarm: " + JSON.stringify(this._currentAlarm));
    debug("Alarm queue: " + JSON.stringify(this._alarmQueue));
  },
}

AlarmService.init();
