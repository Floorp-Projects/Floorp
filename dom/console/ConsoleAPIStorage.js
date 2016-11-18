/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var Cu = Components.utils;
var Ci = Components.interfaces;
var Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// This constant tells how many messages to process in a single timer execution.
const MESSAGES_IN_INTERVAL = 1500

const STORAGE_MAX_EVENTS = 1000;

var _consoleStorage = new Map();

const CONSOLEAPISTORAGE_CID = Components.ID('{96cf7855-dfa9-4c6d-8276-f9705b4890f2}');

/**
 * The ConsoleAPIStorage is meant to cache window.console API calls for later
 * reuse by other components when needed. For example, the Web Console code can
 * display the cached messages when it opens for the active tab.
 *
 * ConsoleAPI messages are stored as they come from the ConsoleAPI code, with
 * all their properties. They are kept around until the inner window object that
 * created the messages is destroyed. Messages are indexed by the inner window
 * ID.
 *
 * Usage:
 *    Cu.import("resource://gre/modules/ConsoleAPIStorage.jsm");
 *
 *    // Get the cached events array for the window you want (use the inner
 *    // window ID).
 *    let events = ConsoleAPIStorage.getEvents(innerWindowID);
 *    events.forEach(function(event) { ... });
 *
 *    // Clear the events for the given inner window ID.
 *    ConsoleAPIStorage.clearEvents(innerWindowID);
 */
function ConsoleAPIStorageService() {
  this.init();
}

ConsoleAPIStorageService.prototype = {
  classID : CONSOLEAPISTORAGE_CID,
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIConsoleAPIStorage,
                                         Ci.nsIObserver]),
  classInfo: XPCOMUtils.generateCI({
    classID: CONSOLEAPISTORAGE_CID,
    contractID: '@mozilla.org/consoleAPI-storage;1',
    interfaces: [Ci.nsIConsoleAPIStorage, Ci.nsIObserver],
    flags: Ci.nsIClassInfo.SINGLETON
  }),

  observe: function CS_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "xpcom-shutdown") {
      Services.obs.removeObserver(this, "xpcom-shutdown");
      Services.obs.removeObserver(this, "inner-window-destroyed");
      Services.obs.removeObserver(this, "memory-pressure");
    }
    else if (aTopic == "inner-window-destroyed") {
      let innerWindowID = aSubject.QueryInterface(Ci.nsISupportsPRUint64).data;
      this.clearEvents(innerWindowID + "");
    }
    else if (aTopic == "memory-pressure") {
      this.clearEvents();
    }
  },

  /** @private */
  init: function CS_init()
  {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "inner-window-destroyed", false);
    Services.obs.addObserver(this, "memory-pressure", false);
  },

  /**
   * Get the events array by inner window ID or all events from all windows.
   *
   * @param string [aId]
   *        Optional, the inner window ID for which you want to get the array of
   *        cached events.
   * @returns array
   *          The array of cached events for the given window. If no |aId| is
   *          given this function returns all of the cached events, from any
   *          window.
   */
  getEvents: function CS_getEvents(aId)
  {
    if (aId != null) {
      return (_consoleStorage.get(aId) || []).slice(0);
    }

    let result = [];

    for (let [id, events] of _consoleStorage) {
      result.push.apply(result, events);
    }

    return result.sort(function(a, b) {
      return a.timeStamp - b.timeStamp;
    });
  },

  /**
   * Record an event associated with the given window ID.
   *
   * @param string aId
   *        The ID of the inner window for which the event occurred or "jsm" for
   *        messages logged from JavaScript modules..
   * @param string aOuterId
   *        This ID is used as 3rd parameters for the console-api-log-event
   *        notification.
   * @param object aEvent
   *        A JavaScript object you want to store.
   */
  recordEvent: function CS_recordEvent(aId, aOuterId, aEvent)
  {
    if (!_consoleStorage.has(aId)) {
      _consoleStorage.set(aId, []);
    }

    let storage = _consoleStorage.get(aId);

    // Clone originAttributes to prevent "TypeError: can't access dead object"
    // exceptions when cached console messages are retrieved/filtered
    // by the devtools webconsole actor.
    aEvent.originAttributes = Cu.cloneInto(aEvent.originAttributes, {});

    storage.push(aEvent);

    // truncate
    if (storage.length > STORAGE_MAX_EVENTS) {
      storage.shift();
    }

    Services.obs.notifyObservers(aEvent, "console-api-log-event", aOuterId);
    Services.obs.notifyObservers(aEvent, "console-storage-cache-event", aId);
  },

  /**
   * Clear storage data for the given window.
   *
   * @param string [aId]
   *        Optional, the inner window ID for which you want to clear the
   *        messages. If this is not specified all of the cached messages are
   *        cleared, from all window objects.
   */
  clearEvents: function CS_clearEvents(aId)
  {
    if (aId != null) {
      _consoleStorage.delete(aId);
    }
    else {
      _consoleStorage.clear();
      Services.obs.notifyObservers(null, "console-storage-reset", null);
    }
  },
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ConsoleAPIStorageService]);
