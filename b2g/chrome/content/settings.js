/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict;"

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

#ifdef MOZ_WIDGET_GONK
XPCOMUtils.defineLazyGetter(this, "libcutils", function () {
  Cu.import("resource://gre/modules/systemlibs.js");
  return libcutils;
});
#endif

XPCOMUtils.defineLazyServiceGetter(this, "uuidgen",
                                   "@mozilla.org/uuid-generator;1",
                                   "nsIUUIDGenerator");

// Once Bug 731746 - Allow chrome JS object to implement nsIDOMEventTarget
// is resolved this helper could be removed.
var SettingsListener = {
  _callbacks: {},

  init: function sl_init() {
    if ('mozSettings' in navigator && navigator.mozSettings) {
      navigator.mozSettings.onsettingchange = this.onchange.bind(this);
    }
  },

  onchange: function sl_onchange(evt) {
    var callback = this._callbacks[evt.settingName];
    if (callback) {
      callback(evt.settingValue);
    }
  },

  observe: function sl_observe(name, defaultValue, callback) {
    var settings = window.navigator.mozSettings;
    if (!settings) {
      window.setTimeout(function() { callback(defaultValue); });
      return;
    }

    if (!callback || typeof callback !== 'function') {
      throw new Error('Callback is not a function');
    }

    var req = settings.createLock().get(name);
    req.addEventListener('success', (function onsuccess() {
      callback(typeof(req.result[name]) != 'undefined' ?
        req.result[name] : defaultValue);
    }));

    this._callbacks[name] = callback;
  }
};

SettingsListener.init();

// =================== Console ======================

SettingsListener.observe('debug.console.enabled', true, function(value) {
  Services.prefs.setBoolPref('consoleservice.enabled', value);
  Services.prefs.setBoolPref('layout.css.report_errors', value);
});

// =================== Languages ====================
SettingsListener.observe('language.current', 'en-US', function(value) {
  Services.prefs.setCharPref('general.useragent.locale', value);

  let prefName = 'intl.accept_languages';
  if (Services.prefs.prefHasUserValue(prefName)) {
    Services.prefs.clearUserPref(prefName);
  }

  let intl = '';
  try {
    intl = Services.prefs.getComplexValue(prefName,
                                          Ci.nsIPrefLocalizedString).data;
  } catch(e) {}

  // Bug 830782 - Homescreen is in English instead of selected locale after
  // the first run experience.
  // In order to ensure the current intl value is reflected on the child
  // process let's always write a user value, even if this one match the
  // current localized pref value.
  if (!((new RegExp('^' + value + '[^a-z-_] *[,;]?', 'i')).test(intl))) {
    value = value + ', ' + intl;
  } else {
    value = intl;
  }
  Services.prefs.setCharPref(prefName, value);

  if (shell.hasStarted() == false) {
    shell.start();
  }
});

// =================== RIL ====================
(function RILSettingsToPrefs() {
  let strPrefs = ['ril.mms.mmsc', 'ril.mms.mmsproxy'];
  strPrefs.forEach(function(key) {
    SettingsListener.observe(key, "", function(value) {
      Services.prefs.setCharPref(key, value);
    });
  });

  ['ril.mms.mmsport'].forEach(function(key) {
    SettingsListener.observe(key, null, function(value) {
      if (value != null) {
        Services.prefs.setIntPref(key, value);
      }
    });
  });

  SettingsListener.observe('ril.mms.retrieval_mode', 'manual',
    function(value) {
      Services.prefs.setCharPref('dom.mms.retrieval_mode', value);
  });

  SettingsListener.observe('ril.sms.strict7BitEncoding.enabled', false,
    function(value) {
      Services.prefs.setBoolPref('dom.sms.strict7BitEncoding', value);
  });

  SettingsListener.observe('ril.sms.requestStatusReport.enabled', false,
    function(value) {
      Services.prefs.setBoolPref('dom.sms.requestStatusReport', value);
  });

  SettingsListener.observe('ril.mms.requestStatusReport.enabled', false,
    function(value) {
      Services.prefs.setBoolPref('dom.mms.requestStatusReport', value);
  });

  SettingsListener.observe('ril.mms.requestReadReport.enabled', true,
    function(value) {
      Services.prefs.setBoolPref('dom.mms.requestReadReport', value);
  });

  SettingsListener.observe('ril.cellbroadcast.disabled', false,
    function(value) {
      Services.prefs.setBoolPref('ril.cellbroadcast.disabled', value);
  });

  SettingsListener.observe('ril.radio.disabled', false,
    function(value) {
      Services.prefs.setBoolPref('ril.radio.disabled', value);
  });

  SettingsListener.observe('wap.UAProf.url', '',
    function(value) {
      Services.prefs.setCharPref('wap.UAProf.url', value);
  });

  SettingsListener.observe('wap.UAProf.tagname', 'x-wap-profile',
    function(value) {
      Services.prefs.setCharPref('wap.UAProf.tagname', value);
  });

  // DSDS default service IDs
  ['mms', 'sms', 'telephony', 'voicemail'].forEach(function(key) {
    SettingsListener.observe('ril.' + key + '.defaultServiceId', 0,
                             function(value) {
      if (value != null) {
        Services.prefs.setIntPref('dom.' + key + '.defaultServiceId', value);
      }
    });
  });
})();

//=================== DeviceInfo ====================
Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import('resource://gre/modules/ctypes.jsm');
(function DeviceInfoToSettings() {
  // MOZ_B2G_VERSION is set in b2g/confvars.sh, and is output as a #define value
  // from configure.in, defaults to 1.0.0 if this value is not exist.
#filter attemptSubstitution
  let os_version = '@MOZ_B2G_VERSION@';
  let os_name = '@MOZ_B2G_OS_NAME@';
#unfilter attemptSubstitution

  let appInfo = Cc["@mozilla.org/xre/app-info;1"]
                  .getService(Ci.nsIXULAppInfo);
  let update_channel = Services.prefs.getCharPref('app.update.channel');

  // Get the hardware info and firmware revision from device properties.
  let hardware_info = null;
  let firmware_revision = null;
  let product_model = null;
#ifdef MOZ_WIDGET_GONK
    hardware_info = libcutils.property_get('ro.hardware');
    firmware_revision = libcutils.property_get('ro.firmware_revision');
    product_model = libcutils.property_get('ro.product.model');
#endif

  let software = os_name + ' ' + os_version;
  let setting = {
    'deviceinfo.os': os_version,
    'deviceinfo.software': software,
    'deviceinfo.platform_version': appInfo.platformVersion,
    'deviceinfo.platform_build_id': appInfo.platformBuildID,
    'deviceinfo.update_channel': update_channel,
    'deviceinfo.hardware': hardware_info,
    'deviceinfo.firmware_revision': firmware_revision,
    'deviceinfo.product_model': product_model
  }
  window.navigator.mozSettings.createLock().set(setting);
})();

// =================== Debugger / ADB ====================

#ifdef MOZ_WIDGET_GONK
let AdbController = {
  DEBUG: false,
  locked: undefined,
  remoteDebuggerEnabled: undefined,
  lockEnabled: undefined,
  disableAdbTimer: null,
  disableAdbTimeoutHours: 12,
  umsActive: false,

  debug: function(str) {
    dump("AdbController: " + str + "\n");
  },

  setLockscreenEnabled: function(value) {
    this.lockEnabled = value;
    if (this.DEBUG) {
      this.debug("setLockscreenEnabled = " + this.lockEnabled);
    }
    this.updateState();
  },

  setLockscreenState: function(value) {
    this.locked = value;
    if (this.DEBUG) {
      this.debug("setLockscreenState = " + this.locked);
    }
    this.updateState();
  },

  setRemoteDebuggerState: function(value) {
    this.remoteDebuggerEnabled = value;
    if (this.DEBUG) {
      this.debug("setRemoteDebuggerState = " + this.remoteDebuggerEnabled);
    }
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
      if (this.DEBUG) {
        this.debug("Timer to disable ADB not started due to zero timeout");
      }
      return;
    }

    if (this.DEBUG) {
      this.debug("Starting timer to disable ADB in " +
                 this.disableAdbTimeoutHours + " hours");
    }
    let timeoutMilliseconds = this.disableAdbTimeoutHours * 60 * 60 * 1000;
    this.disableAdbTimer.initWithCallback(this, timeoutMilliseconds,
                                          Ci.nsITimer.TYPE_ONE_SHOT);
  },

  stopDisableAdbTimer: function() {
    if (this.DEBUG) {
      this.debug("Stopping timer to disable ADB");
    }
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
      dump("AdbController: ADB timer expired - disabling ADB\n");
      navigator.mozSettings.createLock().set(
        {'devtools.debugger.remote-enabled': false});
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
    if (this.DEBUG) {
      this.debug("Checking availability of storage: '" +
                 storage.storageName);
    }

    let req = storage.available();
    req.onsuccess = function(e) {
      if (this.DEBUG) {
        this.debug("Storage: '" + storage.storageName + "' is '" +
                   e.target.result);
      }
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
      dump("AdbController: error querying storage availability for '" +
           this.storages[storageIndex].storageName + "' (ignoring)\n");
      this.updateStorageState(storageIndex + 1);
    }.bind(this);
  },

  updateStateInternal: function() {
    if (this.DEBUG) {
      this.debug("updateStateInternal: called");
    }

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
      if (this.DEBUG) {
        this.debug("updateState: Waiting for all vars to be initialized");
      }
      return;
    }

    // Check if we have a remote debugging session going on. If so, we won't
    // disable adb even if the screen is locked.
    let isDebugging = RemoteDebugger.isDebugging;
    if (this.DEBUG) {
      this.debug("isDebugging=" + isDebugging);
    }

    // If USB Mass Storage, USB tethering, or a debug session is active,
    // then we don't want to disable adb in an automatic fashion (i.e.
    // when the screen locks or due to timeout).
    let sysUsbConfig = libcutils.property_get("sys.usb.config");
    let rndisActive = (sysUsbConfig.split(",").indexOf("rndis") >= 0);
    let usbFuncActive = rndisActive || this.umsActive || isDebugging;

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
    if (this.DEBUG) {
      this.debug("updateState: enableAdb = " + enableAdb +
                 " remoteDebuggerEnabled = " + this.remoteDebuggerEnabled +
                 " lockEnabled = " + this.lockEnabled +
                 " locked = " + this.locked +
                 " usbFuncActive = " + usbFuncActive);
    }

    // Configure adb.
    let currentConfig = libcutils.property_get("persist.sys.usb.config");
    let configFuncs = currentConfig.split(",");
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
    if (newConfig != currentConfig) {
      if (this.DEBUG) {
        this.debug("updateState: currentConfig = " + currentConfig);
        this.debug("updateState:     newConfig = " + newConfig);
      }
      try {
        libcutils.property_set("persist.sys.usb.config", newConfig);
      } catch(e) {
        dump("Error configuring adb: " + e);
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
#endif

// Keep the old setting to not break people that won't have updated
// gaia and gecko.
SettingsListener.observe('devtools.debugger.remote-enabled', false, function(value) {
  Services.prefs.setBoolPref('devtools.debugger.remote-enabled', value);
  // This preference is consulted during startup
  Services.prefs.savePrefFile(null);
  try {
    value ? RemoteDebugger.start() : RemoteDebugger.stop();
  } catch(e) {
    dump("Error while initializing devtools: " + e + "\n" + e.stack + "\n");
  }

#ifdef MOZ_WIDGET_GONK
  AdbController.setRemoteDebuggerState(value);
#endif
});

SettingsListener.observe('debugger.remote-mode', false, function(value) {
  if (['disabled', 'adb-only', 'adb-devtools'].indexOf(value) == -1) {
    dump('Illegal value for debugger.remote-mode: ' + value + '\n');
    return;
  }

  Services.prefs.setBoolPref('devtools.debugger.remote-enabled',
                             value == 'adb-devtools');
  // This preference is consulted during startup
  Services.prefs.savePrefFile(null);

  try {
    (value == 'adb-devtools') ? RemoteDebugger.start()
                              : RemoteDebugger.stop();
  } catch(e) {
    dump("Error while initializing devtools: " + e + "\n" + e.stack + "\n");
  }

#ifdef MOZ_WIDGET_GONK
  AdbController.setRemoteDebuggerState(value != 'disabled');
#endif
});


SettingsListener.observe('debug.log-animations.enabled', false, function(value) {
  Services.prefs.setBoolPref('layers.offmainthreadcomposition.log-animations', value);
});

// =================== Device Storage ====================
SettingsListener.observe('device.storage.writable.name', 'sdcard', function(value) {
  if (Services.prefs.getPrefType('device.storage.writable.name') != Ci.nsIPrefBranch.PREF_STRING) {
    // We clear the pref because it used to be erroneously written as a bool
    // and we need to clear it before we can change it to have the correct type.
    Services.prefs.clearUserPref('device.storage.writable.name');
  }
  Services.prefs.setCharPref('device.storage.writable.name', value);
});

// =================== Privacy ====================
SettingsListener.observe('privacy.donottrackheader.enabled', false, function(value) {
  Services.prefs.setBoolPref('privacy.donottrackheader.enabled', value);
});

SettingsListener.observe('privacy.donottrackheader.value', 1, function(value) {
  Services.prefs.setIntPref('privacy.donottrackheader.value', value);
  // If the user specifically disallows tracking, we set the value of
  // app.update.custom (update tracking ID) to an empty string.
  if (value == 1) {
    Services.prefs.setCharPref('app.update.custom', '');
    return;
  }
  // Otherwise, we assure that the update tracking ID exists.
  setUpdateTrackingId();
});

// =================== Crash Reporting ====================
SettingsListener.observe('app.reportCrashes', 'ask', function(value) {
  if (value == 'always') {
    Services.prefs.setBoolPref('app.reportCrashes', true);
  } else if (value == 'never') {
    Services.prefs.setBoolPref('app.reportCrashes', false);
  } else {
    Services.prefs.clearUserPref('app.reportCrashes');
  }
  // This preference is consulted during startup.
  Services.prefs.savePrefFile(null);
});

// ================ Updates ================
/**
 * For tracking purposes some partners require us to add an UUID to the
 * update URL. The update tracking ID will be an empty string if the
 * do-not-track feature specifically disallows tracking and it is reseted
 * to a different ID if the do-not-track value changes from disallow to allow.
 */
function setUpdateTrackingId() {
  try {
    let dntEnabled = Services.prefs.getBoolPref('privacy.donottrackheader.enabled');
    let dntValue =  Services.prefs.getIntPref('privacy.donottrackheader.value');
    // If the user specifically decides to disallow tracking (1), we just bail out.
    if (dntEnabled && (dntValue == 1)) {
      return;
    }

    let trackingId =
      Services.prefs.getPrefType('app.update.custom') ==
      Ci.nsIPrefBranch.PREF_STRING &&
      Services.prefs.getCharPref('app.update.custom');

    // If there is no previous registered tracking ID, we generate a new one.
    // This should only happen on first usage or after changing the
    // do-not-track value from disallow to allow.
    if (!trackingId) {
      trackingId = uuidgen.generateUUID().toString().replace(/[{}]/g, "");
      Services.prefs.setCharPref('app.update.custom', trackingId);
    }
  } catch(e) {
    dump('Error getting tracking ID ' + e + '\n');
  }
}
setUpdateTrackingId();

SettingsListener.observe('app.update.interval', 86400, function(value) {
  Services.prefs.setIntPref('app.update.interval', value);
});

// ================ Debug ================
// XXX could factor out into a settings->pref map.
SettingsListener.observe("debug.fps.enabled", false, function(value) {
  Services.prefs.setBoolPref("layers.acceleration.draw-fps", value);
});
SettingsListener.observe("debug.paint-flashing.enabled", false, function(value) {
  Services.prefs.setBoolPref("nglayout.debug.paint_flashing", value);
});
SettingsListener.observe("layers.draw-borders", false, function(value) {
  Services.prefs.setBoolPref("layers.draw-borders", value);
});
SettingsListener.observe("layers.composer2d.enabled", true, function(value) {
  Services.prefs.setBoolPref("layers.composer2d.enabled", value);
});

// ================ Accessibility ============
SettingsListener.observe("accessibility.screenreader", false, function(value) {
  if (value && !("AccessFu" in this)) {
    Cu.import('resource://gre/modules/accessibility/AccessFu.jsm');
    AccessFu.attach(window);
  }
});
