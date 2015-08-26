/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "KillSwitchMain" ];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise", "resource://gre/modules/Promise.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "settings",
                   "@mozilla.org/settingsService;1",
                   "nsISettingsService");

XPCOMUtils.defineLazyServiceGetter(this, "ppmm",
                   "@mozilla.org/parentprocessmessagemanager;1",
                   "nsIMessageBroadcaster");

XPCOMUtils.defineLazyGetter(this, "permMgr", function() {
  return Cc["@mozilla.org/permissionmanager;1"]
           .getService(Ci.nsIPermissionManager);
});

#ifdef MOZ_WIDGET_GONK
XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});
#else
this.libcutils = null;
#endif

const DEBUG = false;

const kEnableKillSwitch   = "KillSwitch:Enable";
const kEnableKillSwitchOK = "KillSwitch:Enable:OK";
const kEnableKillSwitchKO = "KillSwitch:Enable:KO";

const kDisableKillSwitch   = "KillSwitch:Disable";
const kDisableKillSwitchOK = "KillSwitch:Disable:OK";
const kDisableKillSwitchKO = "KillSwitch:Disable:KO";

const kMessages = [kEnableKillSwitch, kDisableKillSwitch];

const kXpcomShutdownObserverTopic = "xpcom-shutdown";

const kProperty = "persist.moz.killswitch";

const kUserValues =
  OS.Path.join(OS.Constants.Path.profileDir, "killswitch.json");

let inParent = Cc["@mozilla.org/xre/app-info;1"]
                 .getService(Ci.nsIXULRuntime)
                 .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT;

function debug(aStr) {
  dump("--*-- KillSwitchMain: " + aStr + "\n");
}

this.KillSwitchMain = {
  _ksState: null,
  _libcutils: null,

  _enabledValues: {
    // List of settings to set to a specific value
    settings: {
      "debugger.remote-mode": "disabled",
      "developer.menu.enabled": false,
      "devtools.unrestricted": false,
      "lockscreen.enabled": true,
      "lockscreen.locked": true,
      "lockscreen.lock-immediately": true,
      "tethering.usb.enabled": false,
      "tethering.wifi.enabled": false,
      "ums.enabled": false
    },

    // List of preferences to set to a specific value
    prefs: {
      "b2g.killswitch.test": true
    },

    // List of Android properties to set to a specific value
    properties: {
      "persist.sys.usb.config": "none" // will change sys.usb.config and sys.usb.state
    },

    // List of Android services to control
    services: {
      "adbd": "stop"
    }
  },

  init: function() {
    DEBUG && debug("init");
    if (libcutils) {
      this._libcutils = libcutils;
    }

    kMessages.forEach(m => {
      ppmm.addMessageListener(m, this);
    });

    Services.obs.addObserver(this, kXpcomShutdownObserverTopic, false);

    this.readStateProperty();
  },

  uninit: function() {
    kMessages.forEach(m => {
      ppmm.removeMessageListener(m, this);
    });

    Services.obs.removeObserver(this, kXpcomShutdownObserverTopic);
  },

  checkLibcUtils: function() {
    DEBUG && debug("checkLibcUtils");
    if (!this._libcutils) {
      debug("No proper libcutils binding, aborting.");
      throw Cr.NS_ERROR_NO_INTERFACE;
    }

    return true;
  },

  readStateProperty: function() {
    DEBUG && debug("readStateProperty");
    try {
      this.checkLibcUtils();
    } catch (ex) {
      return;
    }

    this._ksState =
      this._libcutils.property_get(kProperty, "false") === "true";
  },

  writeStateProperty: function() {
    DEBUG && debug("writeStateProperty");
    try {
      this.checkLibcUtils();
    } catch (ex) {
      return;
    }

    this._libcutils.property_set(kProperty, this._ksState.toString());
  },

  getPref(name, value) {
    let rv = undefined;

    try {
      switch (typeof value) {
        case "boolean":
          rv = Services.prefs.getBoolPref(name, value);
          break;

        case "number":
          rv = Services.prefs.getIntPref(name, value);
          break;

        case "string":
          rv = Services.prefs.getCharPref(name, value);
          break;

        default:
          debug("Unexpected pref type " + value);
          break;
      }
    } catch (ex) {
    }

    return rv;
  },

  setPref(name, value) {
    switch (typeof value) {
      case "boolean":
        Services.prefs.setBoolPref(name, value);
        break;

      case "number":
        Services.prefs.setIntPref(name, value);
        break;

      case "string":
        Services.prefs.setCharPref(name, value);
        break;

      default:
        debug("Unexpected pref type " + value);
        break;
    }
  },

  doEnable: function() {
    return new Promise((resolve, reject) => {
      // Make sure that the API cannot do a new |enable()| call once the
      // feature has been enabled, otherwise we will overwrite the user values.
      if (this._ksState) {
        reject(true);
        return;
      }

      this.saveUserValues().then(() => {
        DEBUG && debug("Toggling settings: " +
                       JSON.stringify(this._enabledValues.settings));

        let lock = settings.createLock();
        for (let key of Object.keys(this._enabledValues.settings)) {
          lock.set(key, this._enabledValues.settings[key], this);
        }

        DEBUG && debug("Toggling prefs: " +
                       JSON.stringify(this._enabledValues.prefs));

        for (let key of Object.keys(this._enabledValues.prefs)) {
          this.setPref(key, this._enabledValues.prefs[key]);
        }

        DEBUG && debug("Toggling properties: " +
                       JSON.stringify(this._enabledValues.properties));

        for (let key of Object.keys(this._enabledValues.properties)) {
          this._libcutils.property_set(key, this._enabledValues.properties[key]);
        }

        DEBUG && debug("Toggling services: " +
                       JSON.stringify(this._enabledValues.services));

        for (let key of Object.keys(this._enabledValues.services)) {
          let value = this._enabledValues.services[key];
          if (value !== "start" && value !== "stop") {
            debug("Unexpected service " + key + " value:" + value);
          }

          this._libcutils.property_set("ctl." + value, key);
        }

        this._ksState = true;
        this.writeStateProperty();

        resolve(true);
      }).catch(err => {
        DEBUG && debug("doEnable: " + err);

        reject(false);
      });
    });
  },

  saveUserValues: function() {
    return new Promise((resolve, reject) => {
      try {
        this.checkLibcUtils();
      } catch (ex) {
        reject("nolibcutils");
      }

      let _userValues = {
        settings: { },
        prefs: { },
        properties: { }
      };

      // Those will be sync calls
      for (let key of Object.keys(this._enabledValues.prefs)) {
        _userValues.prefs[key] =
           this.getPref(key, this._enabledValues.prefs[key]);
      }

      for (let key of Object.keys(this._enabledValues.properties)) {
        _userValues.properties[key] = this._libcutils.property_get(key);
      }

      let self = this;
      let getCallback = {
        handleAbort: function(m) {
          DEBUG && debug("getCallback: handleAbort: m=" + m);
          reject(m);
        },

        handleError: function(m) {
          DEBUG && debug("getCallback: handleError: m=" + m);
          reject(m);
        },

        handle: function(n, v) {
          DEBUG && debug("getCallback: handle: n=" + n + " ; v=" + v);

          if (self._pendingSettingsGet) {
            // We have received a settings callback value for saving user data
            let pending = self._pendingSettingsGet.indexOf(n);
            if (pending !== -1) {
              _userValues.settings[n] = v;
              self._pendingSettingsGet.splice(pending, 1);
            }

            if (self._pendingSettingsGet.length === 0) {
              delete self._pendingSettingsGet;
              let payload = JSON.stringify(_userValues);
              DEBUG && debug("Dumping to " + kUserValues + ": " + payload);
              OS.File.writeAtomic(kUserValues, payload).then(
                function writeOk() {
                  resolve(true);
                },
                function writeNok(err) {
                  reject("write error");
                }
              );
            }
          }
        }
      };

      // For settings we have to wait all the callbacks to come back before
      // we can resolve or reject
      this._pendingSettingsGet = [];
      let lock = settings.createLock();
      for (let key of Object.keys(this._enabledValues.settings)) {
        this._pendingSettingsGet.push(key);
        lock.get(key, getCallback);
      }
    });
  },

  doDisable: function() {
    return new Promise((resolve, reject) => {
      this.restoreUserValues().then(() => {
        this._ksState = false;
        this.writeStateProperty();

        resolve(true);
      }).catch(err => {
        DEBUG && debug("doDisable: " + err);

        reject(false);
      });
    });
  },

  restoreUserValues: function() {
    return new Promise((resolve, reject) => {
      try {
        this.checkLibcUtils();
      } catch (ex) {
        reject("nolibcutils");
      }

      OS.File.read(kUserValues, { encoding: "utf-8" }).then(content => {
        let values = JSON.parse(content);

        for (let key of Object.keys(values.prefs)) {
          this.setPref(key, values.prefs[key]);
        }

        for (let key of Object.keys(values.properties)) {
          this._libcutils.property_set(key, values.properties[key]);
        }

        let self = this;
        let saveCallback = {
          handleAbort: function(m) {
            DEBUG && debug("saveCallback: handleAbort: m=" + m);
            reject(m);
          },

          handleError: function(m) {
            DEBUG && debug("saveCallback: handleError: m=" + m);
            reject(m);
          },

          handle: function(n, v) {
            DEBUG && debug("saveCallback: handle: n=" + n + " ; v=" + v);

            if (self._pendingSettingsSet) {
              // We have received a settings callback value for setting user data
              let pending = self._pendingSettingsSet.indexOf(n);
              if (pending !== -1) {
                self._pendingSettingsSet.splice(pending, 1);
              }

              if (self._pendingSettingsSet.length === 0) {
                delete self._pendingSettingsSet;
                DEBUG && debug("Restored from " + kUserValues + ": " + JSON.stringify(values));
                resolve(values);
              }
            }
          }
        };

        // For settings we have to wait all the callbacks to come back before
        // we can resolve or reject
        this._pendingSettingsSet = [];
        let lock = settings.createLock();
        for (let key of Object.keys(values.settings)) {
          this._pendingSettingsSet.push(key);
          lock.set(key, values.settings[key], saveCallback);
        }
      }).catch(err => {
        reject(err);
      });
    });
  },

  // Settings Callbacks
  handle: function(aName, aValue) {
    DEBUG && debug("handle: aName=" + aName + " ; aValue=" + aValue);
    // We don't have to do anything for now.
  },

  handleAbort: function(aMessage) {
    debug("handleAbort: " + JSON.stringify(aMessage));
    throw Cr.NS_ERROR_ABORT;
  },

  handleError: function(aMessage) {
    debug("handleError: " + JSON.stringify(aMessage));
    throw Cr.NS_ERROR_FAILURE;
  },

  // addObserver
  observe: function(aSubject, aTopic, aData) {
    switch (aTopic) {
      case kXpcomShutdownObserverTopic:
        this.uninit();
        break;

      default:
        DEBUG && debug("Wrong observer topic: " + aTopic);
        break;
    }
  },

  // addMessageListener
  receiveMessage: function(aMessage) {
    let hasPermission = aMessage.target.assertPermission("killswitch");
    DEBUG && debug("hasPermission: " + hasPermission);

    if (!hasPermission) {
      debug("Message " + aMessage.name + " from a process with no killswitch perm.");
      aMessage.target.killChild();
      throw Cr.NS_ERROR_NOT_AVAILABLE;
      return;
    }

    function returnMessage(name, data) {
      if (aMessage.target) {
        data.requestID = aMessage.data.requestID;
        try {
          aMessage.target.sendAsyncMessage(name, data);
        } catch (e) {
          if (DEBUG) debug("Return message failed, " + name + ": " + e);
        }
      }
    }

    switch (aMessage.name) {
      case kEnableKillSwitch:
        this.doEnable().then(
          ()  => {
            returnMessage(kEnableKillSwitchOK, {});
          },
          err => {
            debug("doEnable failed: " + err);
            returnMessage(kEnableKillSwitchKO, {});
          }
        );
        break;
      case kDisableKillSwitch:
        this.doDisable().then(
          ()  => {
            returnMessage(kDisableKillSwitchOK, {});
          },
          err => {
            debug("doDisable failed: " + err);
            returnMessage(kDisableKillSwitchKO, {});
          }
        );
        break;

      default:
        debug("Unsupported message: " + aMessage.name);
        aMessage.target && aMessage.target.killChild();
        throw Cr.NS_ERROR_ILLEGAL_VALUE;
        return;
        break;
    }
  }
};

// This code should ALWAYS be living only on the parent side.
if (!inParent) {
  debug("KillSwitchMain should only be living on parent side.");
  throw Cr.NS_ERROR_ABORT;
} else {
  this.KillSwitchMain.init();
}
