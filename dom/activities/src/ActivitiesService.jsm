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
Cu.import("resource://gre/modules/ActivitiesServiceFilter.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                                   "@mozilla.org/parentprocessmessagemanager;1",
                                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyServiceGetter(this, "NetUtil",
                                   "@mozilla.org/network/util;1",
                                   "nsINetUtil");

this.EXPORTED_SYMBOLS = [];

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
    this.initDBHelper(DB_NAME, DB_VERSION, [STORE_NAME]);
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

  // Add all the activities carried in the |aObjects| array.
  add: function actdb_add(aObjects, aSuccess, aError) {
    this.newTxn("readwrite", STORE_NAME, function (txn, store) {
      aObjects.forEach(function (aObject) {
        let object = {
          manifest: aObject.manifest,
          name: aObject.name,
          icon: aObject.icon || "",
          description: aObject.description
        };
        object.id = this.createId(object);
        debug("Going to add " + JSON.stringify(object));
        store.put(object);
      }, this);
    }.bind(this), aSuccess, aError);
  },

  // Remove all the activities carried in the |aObjects| array.
  remove: function actdb_remove(aObjects) {
    this.newTxn("readwrite", STORE_NAME, function (txn, store) {
      aObjects.forEach(function (aObject) {
        let object = {
          manifest: aObject.manifest,
          name: aObject.name
        };
        debug("Going to remove " + JSON.stringify(object));
        store.delete(this.createId(object));
      }, this);
    }.bind(this), function() {}, function() {});
  },

  find: function actdb_find(aObject, aSuccess, aError, aMatch) {
    debug("Looking for " + aObject.options.name);

    this.newTxn("readonly", STORE_NAME, function (txn, store) {
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

    // ActivityWrapper.js
    "Activity:Ready",

    // ActivityRequestHandler.js
    "Activity:PostResult",
    "Activity:PostError",

    "Activities:Register",
    "Activities:Unregister",
    "Activities:GetContentTypes",

    "child-process-shutdown"
  ],

  init: function activities_init() {
    this.messages.forEach(function(msgName) {
      ppmm.addMessageListener(msgName, this);
    }, this);

    Services.obs.addObserver(this, "xpcom-shutdown", false);

    this.db = new ActivitiesDb();
    this.db.init();
    this.callers = {};
  },

  observe: function activities_observe(aSubject, aTopic, aData) {
    this.messages.forEach(function(msgName) {
      ppmm.removeMessageListener(msgName, this);
    }, this);
    ppmm = null;

    if (this.db) {
      this.db.close();
      this.db = null;
    }

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
      if (aResults.options.length === 0) {
        Activities.callers[aMsg.id].mm.sendAsyncMessage("Activity:FireError", {
          "id": aMsg.id,
          "error": "NO_PROVIDER"
        });
        delete Activities.callers[aMsg.id];
        return;
      }

      function getActivityChoice(aChoice) {
        debug("Activity choice: " + aChoice);

        // The user has cancelled the choice, fire an error.
        if (aChoice === -1) {
          Activities.callers[aMsg.id].mm.sendAsyncMessage("Activity:FireError", {
            "id": aMsg.id,
            "error": "USER_ABORT"
          });
          delete Activities.callers[aMsg.id];
          return;
        }

        let sysmm = Cc["@mozilla.org/system-message-internal;1"]
                      .getService(Ci.nsISystemMessagesInternal);
        if (!sysmm) {
          // System message is not present, what should we do?
          delete Activities.callers[aMsg.id];
          return;
        }

        debug("Sending system message...");
        let result = aResults.options[aChoice];
        sysmm.sendMessage("activity", {
            "id": aMsg.id,
            "payload": aMsg.options,
            "target": result.description
          },
          Services.io.newURI(result.description.href, null, null),
          Services.io.newURI(result.manifest, null, null),
          {
            "manifestURL": Activities.callers[aMsg.id].manifestURL,
            "pageURL": Activities.callers[aMsg.id].pageURL
          });

        if (!result.description.returnValue) {
          Activities.callers[aMsg.id].mm.sendAsyncMessage("Activity:FireSuccess", {
            "id": aMsg.id,
            "result": null
          });
          // No need to notify observers, since we don't want the caller
          // to be raised on the foreground that quick.
          delete Activities.callers[aMsg.id];
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
      return ActivitiesServiceFilter.match(aMsg.options.data,
                                           aResult.description.filters);
    };

    this.db.find(aMsg, successCb, errorCb, matchFunc);
  },

  receiveMessage: function activities_receiveMessage(aMessage) {
    let mm = aMessage.target;
    let msg = aMessage.json;

    let caller;
    let obsData;

    if (aMessage.name == "Activity:PostResult" ||
        aMessage.name == "Activity:PostError" ||
        aMessage.name == "Activity:Ready") {
      caller = this.callers[msg.id];
      if (!caller) {
        debug("!! caller is null for msg.id=" + msg.id);
        return;
      }
      obsData = JSON.stringify({ manifestURL: caller.manifestURL,
                                 pageURL: caller.pageURL,
                                 success: aMessage.name == "Activity:PostResult" });
    }

    switch(aMessage.name) {
      case "Activity:Start":
        this.callers[msg.id] = { mm: mm,
                                 manifestURL: msg.manifestURL,
                                 pageURL: msg.pageURL };
        this.startActivity(msg);
        break;

      case "Activity:Ready":
        caller.childMM = mm;
        break;

      case "Activity:PostResult":
        caller.mm.sendAsyncMessage("Activity:FireSuccess", msg);
        delete this.callers[msg.id];
        break;
      case "Activity:PostError":
        caller.mm.sendAsyncMessage("Activity:FireError", msg);
        delete this.callers[msg.id];
        break;

      case "Activities:Register":
        let self = this;
        this.db.add(msg,
          function onSuccess(aEvent) {
            mm.sendAsyncMessage("Activities:Register:OK", null);
            let res = [];
            msg.forEach(function(aActivity) {
              self.updateContentTypeList(aActivity, res);
            });
            if (res.length) {
              ppmm.broadcastAsyncMessage("Activities:RegisterContentTypes",
                                         { contentTypes: res });
            }
          },
          function onError(aEvent) {
            msg.error = "REGISTER_ERROR";
            mm.sendAsyncMessage("Activities:Register:KO", msg);
          });
        break;
      case "Activities:Unregister":
        this.db.remove(msg);
        let res = [];
        msg.forEach(function(aActivity) {
          this.updateContentTypeList(aActivity, res);
        }, this);
        if (res.length) {
          ppmm.broadcastAsyncMessage("Activities:UnregisterContentTypes",
                                     { contentTypes: res });
        }
        break;
      case "Activities:GetContentTypes":
        this.sendContentTypes(mm);
        break;
      case "child-process-shutdown":
        for (let id in this.callers) {
          if (this.callers[id].childMM == mm) {
            this.callers[id].mm.sendAsyncMessage("Activity:FireError", {
              "id": id,
              "error": "USER_ABORT"
            });
            delete this.callers[id];
            break;
          }
        }
        break;
    }
  },

  updateContentTypeList: function updateContentTypeList(aActivity, aResult) {
    // Bail out if this is not a "view" activity.
    if (aActivity.name != "view") {
      return;
    }

    let types = aActivity.description.filters.type;
    if (typeof types == "string") {
      types = [types];
    }

    // Check that this is a real content type and sanitize it.
    types.forEach(function(aContentType) {
      let hadCharset = { };
      let charset = { };
      let contentType =
        NetUtil.parseContentType(aContentType, charset, hadCharset);
      if (contentType) {
        aResult.push(contentType);
      }
    });
  },

  sendContentTypes: function sendContentTypes(aMm) {
    let res = [];
    let self = this;
    this.db.find({ options: { name: "view" } },
      function() { // Success callback.
        if (res.length) {
          aMm.sendAsyncMessage("Activities:RegisterContentTypes",
                               { contentTypes: res });
        }
      },
      null, // Error callback.
      function(aActivity) { // Matching callback.
        self.updateContentTypeList(aActivity, res)
        return false;
      }
    );
  }
}

Activities.init();
