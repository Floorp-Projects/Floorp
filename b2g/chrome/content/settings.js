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

// =================== Audio ====================
SettingsListener.observe('audio.volume.master', 1.0, function(value) {
  let audioManager = Services.audioManager;
  if (!audioManager)
    return;

  audioManager.masterVolume = Math.max(0.0, Math.min(value, 1.0));
});

let audioChannelSettings = [];

if ("nsIAudioManager" in Ci) {
  const nsIAudioManager = Ci.nsIAudioManager;
  audioChannelSettings = [
    // settings name, max value, apply to stream types
    ['audio.volume.content', 15, [nsIAudioManager.STREAM_TYPE_SYSTEM, nsIAudioManager.STREAM_TYPE_MUSIC]],
    ['audio.volume.notification', 15, [nsIAudioManager.STREAM_TYPE_RING, nsIAudioManager.STREAM_TYPE_NOTIFICATION]],
    ['audio.volume.alarm', 15, [nsIAudioManager.STREAM_TYPE_ALARM]],
    ['audio.volume.telephony', 5, [nsIAudioManager.STREAM_TYPE_VOICE_CALL]],
    ['audio.volume.bt_sco', 15, [nsIAudioManager.STREAM_TYPE_BLUETOOTH_SCO]],
  ];
}

for each (let [setting, maxValue, streamTypes] in audioChannelSettings) {
  (function AudioStreamSettings(setting, maxValue, streamTypes) {
    SettingsListener.observe(setting, maxValue, function(value) {
      let audioManager = Services.audioManager;
      if (!audioManager)
        return;

      for each(let streamType in streamTypes) {
        audioManager.setStreamVolumeIndex(streamType, Math.min(value, maxValue));
      }
    });
  })(setting, maxValue, streamTypes);
}

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

  SettingsListener.observe('ril.sms.requestStatusReport.enabled', true,
    function(value) {
      Services.prefs.setBoolPref('dom.sms.requestStatusReport', value);
  });
})();

//=================== DeviceInfo ====================
Components.utils.import('resource://gre/modules/XPCOMUtils.jsm');
Components.utils.import('resource://gre/modules/ctypes.jsm');
(function DeviceInfoToSettings() {
  XPCOMUtils.defineLazyServiceGetter(this, 'gSettingsService',
                                     '@mozilla.org/settingsService;1',
                                     'nsISettingsService');
  let lock = gSettingsService.createLock();
  // MOZ_B2G_VERSION is set in b2g/confvars.sh, and is output as a #define value
  // from configure.in, defaults to 1.0.0 if this value is not exist.
#filter attemptSubstitution
  let os_version = '@MOZ_B2G_VERSION@';
  let os_name = '@MOZ_B2G_OS_NAME@';
#unfilter attemptSubstitution
  lock.set('deviceinfo.os', os_version, null, null);
  lock.set('deviceinfo.software', os_name + ' ' + os_version, null, null);

  let appInfo = Cc["@mozilla.org/xre/app-info;1"]
                  .getService(Ci.nsIXULAppInfo);
  lock.set('deviceinfo.platform_version', appInfo.platformVersion, null, null);
  lock.set('deviceinfo.platform_build_id', appInfo.platformBuildID, null, null);

  let update_channel = Services.prefs.getCharPref('app.update.channel');
  lock.set('deviceinfo.update_channel', update_channel, null, null);

  // Get the hardware info and firmware revision from device properties.
  let hardware_info = null;
  let firmware_revision = null;
#ifdef MOZ_WIDGET_GONK
    hardware_info = libcutils.property_get('ro.hardware');
    firmware_revision = libcutils.property_get('ro.firmware_revision');
#endif
  lock.set('deviceinfo.hardware', hardware_info, null, null);
  lock.set('deviceinfo.firmware_revision', firmware_revision, null, null);
})();

// =================== Debugger ====================
SettingsListener.observe('devtools.debugger.remote-enabled', false, function(value) {
  Services.prefs.setBoolPref('devtools.debugger.remote-enabled', value);
  // This preference is consulted during startup
  Services.prefs.savePrefFile(null);
  value ? RemoteDebugger.start() : RemoteDebugger.stop();

#ifdef MOZ_WIDGET_GONK
  let enableAdb = value;

  try {
    if (Services.prefs.getBoolPref('marionette.defaultPrefs.enabled')) {
      // Marionette is enabled. Force adb on, since marionette requires remote
      // debugging to be disabled (we don't want adb to track the remote debugger
      // setting).

      enableAdb = true;
    }
  } catch (e) {
    // This means that the pref doesn't exist. Which is fine. We just leave
    // enableAdb alone.
  }

  // Configure adb.
  try {
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
        configFuncs.splice(adbIndex,1);
      }
    }
    let newConfig = configFuncs.join(",");
    if (newConfig != currentConfig) {
      libcutils.property_set("persist.sys.usb.config", newConfig);
    }
  } catch(e) {
    dump("Error configuring adb: " + e);
  }
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

// =================== Crash Reporting ====================
SettingsListener.observe('app.reportCrashes', 'ask', function(value) {
  if (value == 'always') {
    Services.prefs.setBoolPref('app.reportCrashes', true);
  } else if (value == 'never') {
    Services.prefs.setBoolPref('app.reportCrashes', false);
  } else {
    Services.prefs.clearUserPref('app.reportCrashes');
  }
});

// ================ Updates ================
SettingsListener.observe('app.update.interval', 86400, function(value) {
  Services.prefs.setIntPref('app.update.interval', value);
});
