/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict"

const Cu = Components.utils; 
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/IndexedDBHelper.jsm");

XPCOMUtils.defineLazyGetter(this, "ppmm", function() {
  return Cc["@mozilla.org/parentprocessmessagemanager;1"]
           .getService(Ci.nsIFrameMessageManager);
});

const EXPORTED_SYMBOLS = [];

let idbGlobal = this;

function debug(aMsg) { 
  //dump("-- ActivitiesService.jsm " + Date.now() + " " + aMsg + "\n");
}

const DB_NAME    = "activities";
const DB_VERSION = 1;
const STORE_NAME = "activities";

function ActivitiesDb() {

}

ActivitiesDb.prototype = {
  __proto__: IndexedDBHelper.prototype,

  init: function actdb_init() {
    let idbManager = Cc["@mozilla.org/dom/indexeddb/manager;1"]
                       .getService(Ci.nsIIndexedDatabaseManager);
    idbManager.initWindowless(idbGlobal);
    this.initDBHelper(DB_NAME, DB_VERSION, STORE_NAME, idbGlobal);
  },

  /**
   * Create the initial database schema.
   *
   * The schema of records stored is as follows:
   *
   * {
   *  id:                  String
   *  manifest:            String
   *  name:                String
   *  title:               String
   *  icon:                String
   *  description:         jsval
   * }
   */
  upgradeSchema: function actdb_upgradeSchema(aTransaction, aDb, aOldVersion, aNewVersion) {
    debug("Upgrade schema " + aOldVersion + " -> " + aNewVersion);
    let objectStore = aDb.createObjectStore(STORE_NAME, { keyPath: "id" });

    // indexes
    objectStore.createIndex("name", "name", { unique: false });
    objectStore.createIndex("manifest", "manifest", { unique: false });

    debug("Created object stores and indexes");
  },

  // unique ids made of (uri, action)
  createId: function actdb_createId(aObject) {
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                      .createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    let hasher = Cc["@mozilla.org/security/hash;1"]
                   .createInstance(Ci.nsICryptoHash);
    hasher.init(hasher.SHA1);

    // add uri and action to the hash
    ["manifest", "name"].forEach(function(aProp) {
      let data = converter.convertToByteArray(aObject[aProp], {});
      hasher.update(data, data.length);
    });
    
    return hasher.finish(true);
  },

  add: function actdb_add(aObject, aSuccess, aError) {
    this.newTxn("readwrite", function (txn, store) {
      let object = {
        manifest: aObject.manifest,
        name: aObject.name,
        title: aObject.title || "",
        icon: aObject.icon || "",
        description: aObject.description
      };
      object.id = this.createId(object);
      debug("Going to add " + JSON.stringify(object));
      
      store.put(object);
    }.bind(this), aSuccess, aError);
  },

  // we want to remove all activities for (manifest, name)
  remove: function actdb_remove(aObject) {
    this.newTxn("readwrite", function (txn, store) {
      let object = {
        manifest: aObject.manifest,
        name: aObject.name
      };
      debug("Going to remove " + JSON.stringify(object));
      store.delete(this.createId(object));
    }.bind(this), function() {}, function() {});
  },

  find: function actdb_find(aObject, aSuccess, aError, aMatch) {
    debug("Looking for " + aObject.options.name);

    this.newTxn("readonly", function (txn, store) {
      let index = store.index("name"); 
      let request = index.mozGetAll(aObject.options.name);
      request.onsuccess = function findSuccess(aEvent) {
        debug("Request successful. Record count: " + aEvent.target.result.length);
        if (!txn.result) {
          txn.result = {
            name: aObject.options.name,
            options: []
          };
        }

        aEvent.target.result.forEach(function(result) {
          if (!aMatch(result))
            return;

          txn.result.options.push({
            manifest: result.manifest,
            title: result.title,
            icon: result.icon,
            description: result.description
          });
        });
      }
    }.bind(this), aSuccess, aError);
  }
}

let Activities = {
  messages: [
    // ActivityProxy.js
    "Activity:Start",

    // ActivityRequestHandler.js
    "Activity:PostResult",
    "Activity:PostError",

    "Activities:Register",
    "Activities:Unregister",
  ],

  init: function activities_init() {
    this.messages.forEach(function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }, this);

    Services.obs.addObserver(this, "xpcom-shutdown", false);

    this.db = new ActivitiesDb();
    this.db.init();
  },

  observe: function activities_observe(aSubject, aTopic, aData) {
    this.messages.forEach(function(msgName) {
      ppmm.removeMessageListener(msgName, this);
    }, this);
    ppmm = null;

    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  /**
    * Starts an activity by doing:
    * - finds a list of matching activities.
    * - calls the UI glue to get the user choice.
    * - fire an system message of type "activity" to this app, sending the 
    *   activity data as a payload.
    */
  startActivity: function activities_startActivity(aMsg) {
    debug("StartActivity: " + JSON.stringify(aMsg));

    let successCb = function successCb(aResults) {
      debug(JSON.stringify(aResults));

      // We have no matching activity registered, let's fire an error.
      if (aResults.length === 0) {
        ppmm.sendAsyncMessage("Activity:FireError", {
          "id": aMsg.id, 
          "error": "NO_PROVIDER"
        });
        return;
      }

      function getActivityChoice(aChoice) {
        debug("Activity choice: " + aChoice);

        // The user has cancelled the choice, fire an error.
        if (aChoice === -1) {
          ppmm.sendAsyncMessage("Activity:FireError", {
            "id": aMsg.id, 
            "error": "USER_ABORT"
          });
          return;
        }

        let sysmm = Cc["@mozilla.org/system-message-internal;1"]
                      .getService(Ci.nsISystemMessagesInternal);
        if (!sysmm) {
          // System message is not present, what should we do?
          return;
        }

        debug("Sending system message...");
        let result = aResults.options[aChoice];
        sysmm.sendMessage("activity", {
          "id": aMsg.id,
          "payload": aMsg.options
        }, Services.io.newURI(result.manifest, null, null));

        if (!result.description.returnValue) {
          ppmm.sendAsyncMessage("Activity:FireSuccess", {
            "id": aMsg.id,
            "result": null
          });
        }
      };

      let glue = Cc["@mozilla.org/dom/activities/ui-glue;1"]
                   .createInstance(Ci.nsIActivityUIGlue);
      glue.chooseActivity(aResults.name, aResults.options, getActivityChoice);
    };

    let errorCb = function errorCb(aError) {
      // Something unexpected happened. Should we send an error back?
      debug("Error in startActivity: " + aError + "\n");
    };

    let matchFunc = function matchFunc(aResult) {
      // Bug 773383: arrays of strings / regexp.
      for (let prop in aResult.description.filters) {
        if (aMsg.options.data[prop] !== aResult.description.filters[prop]) {
          return false;
        }
      }
      return true;
    };

    this.db.find(aMsg, successCb, errorCb, matchFunc);
  },

  receiveMessage: function activities_receiveMessage(aMessage) {
    let mm = aMessage.target.QueryInterface(Ci.nsIFrameMessageManager);
    let msg = aMessage.json;
    switch(aMessage.name) {
      case "Activity:Start":
        this.startActivity(msg);
        break;

      case "Activity:PostResult":
        ppmm.sendAsyncMessage("Activity:FireSuccess", msg);
        break;
      case "Activity:PostError":
        ppmm.sendAsyncMessage("Activity:FireError", msg);
        break;

      case "Activities:Register":
        this.db.add(msg, function onSuccess(aEvent) {
          mm.sendAsyncMessage("Activities:Register:OK", msg);
        },
        function onError(aEvent) {
          msg.error = "REGISTER_ERROR";
          mm.sendAsyncMessage("Activities:Register:KO", msg);
        });
        break;
      case "Activities:Unregister":
        this.db.remove(msg);
        break;
    }
  }
}

Activities.init();
