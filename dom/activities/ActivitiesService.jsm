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
Cu.import("resource://gre/modules/AppsUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "DOMApplicationRegistry",
  "resource://gre/modules/Webapps.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ActivitiesServiceFilter",
  "resource://gre/modules/ActivitiesServiceFilter.jsm");

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
const DB_VERSION = 2;
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

    let self = this;

    /**
     * WARNING!! Before upgrading the Activities DB take into account that an
     * OTA unregisters all the activities and reinstalls them during the first
     * run process. Check Bug 1193503.
     */

    function upgrade(currentVersion) {
      let next = upgrade.bind(self, currentVersion + 1);
      switch (currentVersion) {
        case 0:
          self.createSchema(aDb, next);
          break;
      }
    }

    upgrade(aOldVersion);
  },

  createSchema: function(aDb, aNext) {
    let objectStore = aDb.createObjectStore(STORE_NAME, { keyPath: "id" });

    // indexes
    objectStore.createIndex("name", "name", { unique: false });
    objectStore.createIndex("manifest", "manifest", { unique: false });

    debug("Created object stores and indexes");

    aNext();
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
    ["manifest", "name", "description"].forEach(function(aProp) {
      if (!aObject[aProp]) {
        return;
      }

      let property = aObject[aProp];
      if (aProp == "description") {
        property = JSON.stringify(aObject[aProp]);
      }

      let data = converter.convertToByteArray(property, {});
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
    this.newTxn("readwrite", STORE_NAME, (txn, store) => {
      aObjects.forEach((aObject) => {
        let object = {
          manifest: aObject.manifest,
          name: aObject.name,
          description: aObject.description
        };
        debug("Going to remove " + JSON.stringify(object));
        store.delete(this.createId(object));
      });
    }, function() {}, function() {});
  },

  // Remove all activities associated with the given |aManifest| URL.
  removeAll: function actdb_removeAll(aManifest) {
    this.newTxn("readwrite", STORE_NAME, function (txn, store) {
      let index = store.index("manifest");
      let request = index.mozGetAll(aManifest);
      request.onsuccess = function manifestActivities(aEvent) {
        aEvent.target.result.forEach(function(result) {
          debug('Removing activity: ' + JSON.stringify(result));
          store.delete(result.id);
        });
      };
    });
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

var Activities = {
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
    "Activities:UnregisterAll",
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

    let self = this;
    let successCb = function successCb(aResults) {
      debug(JSON.stringify(aResults));

      function getActivityChoice(aResultType, aResult) {
        switch(aResultType) {
          case Ci.nsIActivityUIGlueCallback.NATIVE_ACTIVITY: {
            self.callers[aMsg.id].mm.sendAsyncMessage("Activity:FireSuccess", {
              "id": aMsg.id,
              "result": aResult
            });
            break;
          }
          case Ci.nsIActivityUIGlueCallback.WEBAPPS_ACTIVITY: {
            debug("Activity choice: " + aResult);

            // We have no matching activity registered, let's fire an error.
            // Don't do this check until we have passed to UIGlue so the glue
            // can choose to launch its own activity if needed.
            if (aResults.options.length === 0) {
                self.trySendAndCleanup(aMsg.id, "Activity:FireError", {
                  "id": aMsg.id,
                  "error": "NO_PROVIDER"
                });
              return;
            }

            // The user has cancelled the choice, fire an error.
            if (aResult === -1) {
              self.trySendAndCleanup(aMsg.id, "Activity:FireError", {
                "id": aMsg.id,
                "error": "ActivityCanceled"
              });
              return;
            }

            let sysmm = Cc["@mozilla.org/system-message-internal;1"]
                          .getService(Ci.nsISystemMessagesInternal);
            if (!sysmm) {
              // System message is not present, what should we do?
              self.removeCaller(aMsg.id);
              return;
            }

            debug("Sending system message...");
            let result = aResults.options[aResult];
            sysmm.sendMessage("activity", {
                "id": aMsg.id,
                "payload": aMsg.options,
                "target": result.description
              },
              Services.io.newURI(result.description.href, null, null),
              Services.io.newURI(result.manifest, null, null),
              {
                "manifestURL": self.callers[aMsg.id].manifestURL,
                "pageURL": self.callers[aMsg.id].pageURL
              });

            if (!result.description.returnValue) {
              // No need to notify observers, since we don't want the caller
              // to be raised on the foreground that quick.
              self.trySendAndCleanup(aMsg.id, "Activity:FireSuccess", {
                "id": aMsg.id,
                "result": null
              });
            }
            break;
          }
        }
      };

      let caller = Activities.callers[aMsg.id];
      if (aMsg.getFilterResults === true &&
          caller.mm.assertAppHasStatus(Ci.nsIPrincipal.APP_STATUS_CERTIFIED)) {
        // Certified apps can ask to just get the picker data.

        // We want to return the manifest url, icon url and app name.
        // The app name needs to be picked up from the localized manifest.
        let reg = DOMApplicationRegistry;
        let ids = aResults.options.map((aItem) => {
          return { id: reg._appIdForManifestURL(aItem.manifest) }
        });

        reg._readManifests(ids).then((aManifests) => {
          let results = [];
          aManifests.forEach((aManifest, i) => {
            let manifestURL = aResults.options[i].manifest;
            // Not passing the origin is fine here since we only need
            // helper.name which doesn't rely on url resolution.
            let helper =
              new ManifestHelper(aManifest.manifest, manifestURL, manifestURL);
            results.push({
              manifestURL: manifestURL,
              iconURL: aResults.options[i].icon,
              appName: helper.name
            });
          });

          // Now fire success with the array of choices.
          caller.mm.sendAsyncMessage("Activity:FireSuccess",
            {
              "id": aMsg.id,
              "result": results
            });
          self.removeCaller(aMsg.id);
        });
      } else {
        let glue = Cc["@mozilla.org/dom/activities/ui-glue;1"]
                     .createInstance(Ci.nsIActivityUIGlue);
        glue.chooseActivity(aMsg.options, aResults.options, getActivityChoice);
      }
    };

    let errorCb = function errorCb(aError) {
      // Something unexpected happened. Should we send an error back?
      debug("Error in startActivity: " + aError + "\n");
    };

    let matchFunc = function matchFunc(aResult) {
      // If the activity is in the developer mode activity list, only let the
      // system app be a provider.
      let isSystemApp = false;
      let isDevModeActivity = false;
      try {
        isSystemApp =
          aResult.manifest == Services.prefs.getCharPref("b2g.system_manifest_url");
        isDevModeActivity =
          Services.prefs.getCharPref("dom.activities.developer_mode_only")
                        .split(",").indexOf(aMsg.options.name) !== -1;
      } catch(e)  {}

      if (isDevModeActivity && !isSystemApp) {
        return false;
      }

      return ActivitiesServiceFilter.match(aMsg.options.data,
                                           aResult.description.filters);
    };

    this.db.find(aMsg, successCb, errorCb, matchFunc);
  },

  trySendAndCleanup: function activities_trySendAndCleanup(aId, aName, aPayload) {
    try {
      this.callers[aId].mm.sendAsyncMessage(aName, aPayload);
    } finally {
      this.removeCaller(aId);
    }
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
        Services.obs.notifyObservers(null, "activity-opened", msg.childID);
        this.callers[msg.id] = { mm: mm,
                                 manifestURL: msg.manifestURL,
                                 childID: msg.childID,
                                 pageURL: msg.pageURL };
        this.startActivity(msg);
        break;

      case "Activity:Ready":
        caller.childMM = mm;
        break;

      case "Activity:PostResult":
        this.trySendAndCleanup(msg.id, "Activity:FireSuccess", msg);
        break;
      case "Activity:PostError":
        this.trySendAndCleanup(msg.id, "Activity:FireError", msg);
        break;

      case "Activities:Register":
        this.db.add(msg,
          function onSuccess(aEvent) {
            debug("Activities:Register:OK");
            Services.obs.notifyObservers(null, "new-activity-registered-success", null);
            mm.sendAsyncMessage("Activities:Register:OK", null);
          },
          function onError(aEvent) {
            msg.error = "REGISTER_ERROR";
            debug("Activities:Register:KO");
            Services.obs.notifyObservers(null, "new-activity-registered-failure", null);
            mm.sendAsyncMessage("Activities:Register:KO", msg);
          });
        break;
      case "Activities:Unregister":
        this.db.remove(msg);
        break;
      case "Activities:UnregisterAll":
        this.db.removeAll(msg);
        break;
      case "child-process-shutdown":
        for (let id in this.callers) {
          if (this.callers[id].childMM == mm) {
            this.trySendAndCleanup(id, "Activity:FireError", {
              "id": id,
              "error": "ActivityCanceled"
            });
            break;
          }
        }
        break;
    }
  },

  removeCaller: function activities_removeCaller(id) {
    Services.obs.notifyObservers(null, "activity-closed",
                                 this.callers[id].childID);
    delete this.callers[id];
  }

}

Activities.init();
