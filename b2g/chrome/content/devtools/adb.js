/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This file is only loaded on Gonk to manage ADB state

Components.utils.import("resource://gre/modules/FileUtils.jsm");

const DEBUG = false;
var debug = function(str) {
  dump("AdbController: " + str + "\n");
}

let AdbController = {
  locked: undefined,
  remoteDebuggerEnabled: undefined,
  lockEnabled: undefined,
  disableAdbTimer: null,
  disableAdbTimeoutHours: 12,
  umsActive: false,

  setLockscreenEnabled: function(value) {
    this.lockEnabled = value;
    DEBUG && debug("setLockscreenEnabled = " + this.lockEnabled);
    this.updateState();
  },

  setLockscreenState: function(value) {
    this.locked = value;
    DEBUG && debug("setLockscreenState = " + this.locked);
    this.updateState();
  },

  setRemoteDebuggerState: function(value) {
    this.remoteDebuggerEnabled = value;
    DEBUG && debug("setRemoteDebuggerState = " + this.remoteDebuggerEnabled);
    this.updateState();
  },

  startDisableAdbTimer: function() {
    if (this.disableAdbTimer) {
      this.disableAdbTimer.cancel();
    } else {
      this.disableAdbTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      try {
        this.disableAdbTimeoutHours =
          Services.prefs.getIntPref("b2g.adb.timeout-hours");
      } catch (e) {
        // This happens if the pref doesn't exist, in which case
        // disableAdbTimeoutHours will still be set to the default.
      }
    }
    if (this.disableAdbTimeoutHours <= 0) {
      DEBUG && debug("Timer to disable ADB not started due to zero timeout");
      return;
    }

    DEBUG && debug("Starting timer to disable ADB in " +
                   this.disableAdbTimeoutHours + " hours");
    let timeoutMilliseconds = this.disableAdbTimeoutHours * 60 * 60 * 1000;
    this.disableAdbTimer.initWithCallback(this, timeoutMilliseconds,
                                          Ci.nsITimer.TYPE_ONE_SHOT);
  },

  stopDisableAdbTimer: function() {
    DEBUG && debug("Stopping timer to disable ADB");
    if (this.disableAdbTimer) {
      this.disableAdbTimer.cancel();
      this.disableAdbTimer = null;
    }
  },

  notify: function(aTimer) {
    if (aTimer == this.disableAdbTimer) {
      this.disableAdbTimer = null;
      // The following dump will be the last thing that shows up in logcat,
      // and will at least give the user a clue about why logcat was
      // disconnected, if the user happens to be using logcat.
      debug("ADB timer expired - disabling ADB\n");
      navigator.mozSettings.createLock().set(
        {'debugger.remote-mode': 'disabled'});
    }
  },

  updateState: function() {
    this.umsActive = false;
    this.storages = navigator.getDeviceStorages('sdcard');
    this.updateStorageState(0);
  },

  updateStorageState: function(storageIndex) {
    if (storageIndex >= this.storages.length) {
      // We've iterated through all of the storage objects, now we can
      // really do updateStateInternal.
      this.updateStateInternal();
      return;
    }
    let storage = this.storages[storageIndex];
    DEBUG && debug("Checking availability of storage: '" + storage.storageName + "'");

    let req = storage.available();
    req.onsuccess = function(e) {
      DEBUG && debug("Storage: '" + storage.storageName + "' is '" + e.target.result + "'");
      if (e.target.result == 'shared') {
        // We've found a storage area that's being shared with the PC.
        // We can stop looking now.
        this.umsActive = true;
        this.updateStateInternal();
        return;
      }
      this.updateStorageState(storageIndex + 1);
    }.bind(this);
    req.onerror = function(e) {

      Cu.reportError("AdbController: error querying storage availability for '" +
                     this.storages[storageIndex].storageName + "' (ignoring)\n");
      this.updateStorageState(storageIndex + 1);
    }.bind(this);
  },

  updateStateInternal: function() {
    DEBUG && debug("updateStateInternal: called");

    if (this.remoteDebuggerEnabled === undefined ||
        this.lockEnabled === undefined ||
        this.locked === undefined) {
      // Part of initializing the settings database will cause the observers
      // to trigger. We want to wait until both have been initialized before
      // we start changing ther adb state. Without this then we can wind up
      // toggling adb off and back on again (or on and back off again).
      //
      // For completeness, one scenario which toggles adb is using the unagi.
      // The unagi has adb enabled by default (prior to b2g starting). If you
      // have the phone lock disabled and remote debugging enabled, then we'll
      // receive an unlock event and an rde event. However at the time we
      // receive the unlock event we haven't yet received the rde event, so
      // we turn adb off momentarily, which disconnects a logcat that might
      // be running. Changing the defaults (in AdbController) just moves the
      // problem to a different phone, which has adb disabled by default and
      // we wind up turning on adb for a short period when we shouldn't.
      //
      // By waiting until both values are properly initialized, we avoid
      // turning adb on or off accidentally.
      DEBUG && debug("updateState: Waiting for all vars to be initialized");
      return;
    }

    // Check if we have a remote debugging session going on. If so, we won't
    // disable adb even if the screen is locked.
    let isDebugging = USBRemoteDebugger.isDebugging;
    DEBUG && debug("isDebugging=" + isDebugging);

    // If USB Mass Storage, USB tethering, or a debug session is active,
    // then we don't want to disable adb in an automatic fashion (i.e.
    // when the screen locks or due to timeout).
    let sysUsbConfig = libcutils.property_get("sys.usb.config").split(",");
    let usbFuncActive = this.umsActive || isDebugging;
    usbFuncActive |= (sysUsbConfig.indexOf("rndis") >= 0);
    usbFuncActive |= (sysUsbConfig.indexOf("mtp") >= 0);

    let enableAdb = this.remoteDebuggerEnabled &&
      (!(this.lockEnabled && this.locked) || usbFuncActive);

    let useDisableAdbTimer = true;
    try {
      if (Services.prefs.getBoolPref("marionette.defaultPrefs.enabled")) {
        // Marionette is enabled. Marionette requires that adb be on (and also
        // requires that remote debugging be off). The fact that marionette
        // is enabled also implies that we're doing a non-production build, so
        // we want adb enabled all of the time.
        enableAdb = true;
        useDisableAdbTimer = false;
      }
    } catch (e) {
      // This means that the pref doesn't exist. Which is fine. We just leave
      // enableAdb alone.
    }

    // Check wakelock to prevent adb from disconnecting when phone is locked
    let lockFile = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
    lockFile.initWithPath('/sys/power/wake_lock');
    if(lockFile.exists()) {
      let foStream = Cc["@mozilla.org/network/file-input-stream;1"]
            .createInstance(Ci.nsIFileInputStream);
      let coStream = Cc["@mozilla.org/intl/converter-input-stream;1"]
            .createInstance(Ci.nsIConverterInputStream);
      let str = {};
      foStream.init(lockFile, FileUtils.MODE_RDONLY, 0, 0);
      coStream.init(foStream, "UTF-8", 0, 0);
      coStream.readString(-1, str);
      coStream.close();
      foStream.close();
      let wakeLockContents = str.value.replace(/\n/, "");
      let wakeLockList = wakeLockContents.split(" ");
      if (wakeLockList.indexOf("adb") >= 0) {
        enableAdb = true;
        useDisableAdbTimer = false;
        DEBUG && debug("Keeping ADB enabled as ADB wakelock is present.");
      } else {
        DEBUG && debug("ADB wakelock not found.");
      }
    } else {
      DEBUG && debug("Wake_lock file not found.");
    }

    DEBUG && debug("updateState: enableAdb = " + enableAdb +
                   " remoteDebuggerEnabled = " + this.remoteDebuggerEnabled +
                   " lockEnabled = " + this.lockEnabled +
                   " locked = " + this.locked +
                   " usbFuncActive = " + usbFuncActive);

    // Configure adb.
    let currentConfig = libcutils.property_get("persist.sys.usb.config");
    let configFuncs = currentConfig.split(",");
    if (currentConfig == "" || currentConfig == "none") {
      // We want to treat none like the empty string.
      // "".split(",") yields [""] and not []
      configFuncs = [];
    }
    let adbIndex = configFuncs.indexOf("adb");

    if (enableAdb) {
      // Add adb to the list of functions, if not already present
      if (adbIndex < 0) {
        configFuncs.push("adb");
      }
    } else {
      // Remove adb from the list of functions, if present
      if (adbIndex >= 0) {
        configFuncs.splice(adbIndex, 1);
      }
    }
    let newConfig = configFuncs.join(",");
    if (newConfig == "") {
      // Convert the empty string back into none, since that's what init.rc
      // needs.
      newConfig = "none";
    }
    if (newConfig != currentConfig) {
      DEBUG && debug("updateState: currentConfig = " + currentConfig);
      DEBUG && debug("updateState:     newConfig = " + newConfig);
      try {
        libcutils.property_set("persist.sys.usb.config", newConfig);
      } catch(e) {
        Cu.reportError("Error configuring adb: " + e);
      }
    }
    if (useDisableAdbTimer) {
      if (enableAdb && !usbFuncActive) {
        this.startDisableAdbTimer();
      } else {
        this.stopDisableAdbTimer();
      }
    }
  }
};

SettingsListener.observe("lockscreen.locked", false,
                         AdbController.setLockscreenState.bind(AdbController));
SettingsListener.observe("lockscreen.enabled", false,
                         AdbController.setLockscreenEnabled.bind(AdbController));
