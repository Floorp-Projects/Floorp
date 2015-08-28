/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

window.performance.mark('gecko-settings-loadstart');

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

// The load order is important here SettingsRequestManager _must_ be loaded
// prior to using SettingsListener otherwise there is a race in acquiring the
// lock and fulfilling it. If we ever move SettingsListener or this file down in
// the load order of shell.html things will likely break.
Cu.import('resource://gre/modules/SettingsRequestManager.jsm');
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

XPCOMUtils.defineLazyServiceGetter(this, "gPACGenerator",
                                   "@mozilla.org/pac-generator;1",
                                   "nsIPACGenerator");

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

SettingsListener.observe('homescreen.manifestURL', 'Sentinel Value' , function(value) {
  Services.prefs.setCharPref('dom.mozApps.homescreenURL', value);
});

// =================== Languages ====================
SettingsListener.observe('language.current', 'en-US', function(value) {
  Services.prefs.setCharPref('general.useragent.locale', value);

  let prefName = 'intl.accept_languages';
  let defaultBranch = Services.prefs.getDefaultBranch(null);

  let intl = '';
  try {
    intl = defaultBranch.getComplexValue(prefName,
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
    shell.bootstrap();
  }
});

// =================== RIL ====================
(function RILSettingsToPrefs() {
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

  // Get the hardware info and firmware revision from device properties.
  let hardware_info = null;
  let firmware_revision = null;
  let product_manufacturer = null;
  let product_model = null;
  let product_device = null;
  let build_number = null;
#ifdef MOZ_WIDGET_GONK
    hardware_info = libcutils.property_get('ro.hardware');
    firmware_revision = libcutils.property_get('ro.firmware_revision');
    product_manufacturer = libcutils.property_get('ro.product.manufacturer');
    product_model = libcutils.property_get('ro.product.model');
    product_device = libcutils.property_get('ro.product.device');
    build_number = libcutils.property_get('ro.build.version.incremental');
#endif

  // Populate deviceinfo settings,
  // copying any existing deviceinfo.os into deviceinfo.previous_os
  let lock = window.navigator.mozSettings.createLock();
  let req = lock.get('deviceinfo.os');
  req.onsuccess = req.onerror = () => {
    let previous_os = req.result && req.result['deviceinfo.os'] || '';
    let software = os_name + ' ' + os_version;
    let setting = {
      'deviceinfo.build_number': build_number,
      'deviceinfo.os': os_version,
      'deviceinfo.previous_os': previous_os,
      'deviceinfo.software': software,
      'deviceinfo.platform_version': appInfo.platformVersion,
      'deviceinfo.platform_build_id': appInfo.platformBuildID,
      'deviceinfo.hardware': hardware_info,
      'deviceinfo.firmware_revision': firmware_revision,
      'deviceinfo.product_manufacturer': product_manufacturer,
      'deviceinfo.product_model': product_model,
      'deviceinfo.product_device': product_device
    }
    lock.set(setting);
  }
})();

// =================== DevTools ====================

let developerHUD;
SettingsListener.observe('devtools.overlay', false, (value) => {
  if (value) {
    if (!developerHUD) {
      let scope = {};
      Services.scriptloader.loadSubScript('chrome://b2g/content/devtools/hud.js', scope);
      developerHUD = scope.developerHUD;
    }
    developerHUD.init();
  } else {
    if (developerHUD) {
      developerHUD.uninit();
    }
  }
});

#ifdef MOZ_WIDGET_GONK

let LogShake;
(function() {
  let scope = {};
  Cu.import('resource://gre/modules/LogShake.jsm', scope);
  LogShake = scope.LogShake;
  LogShake.init();
})();

SettingsListener.observe('devtools.logshake.enabled', false, value => {
  if (value) {
    LogShake.enableDeviceMotionListener();
  } else {
    LogShake.disableDeviceMotionListener();
  }
});

SettingsListener.observe('devtools.logshake.qa_enabled', false, value => {
  if (value) {
    LogShake.enableQAMode();
  } else {
    LogShake.disableQAMode();
  }
});
#endif

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

(function syncUpdatePrefs() {
  // The update service reads the prefs from the default branch. This is by
  // design, as explained in bug 302721 comment 43. If we are to successfully
  // modify them, that's where we need to make our changes.
  let defaultBranch = Services.prefs.getDefaultBranch(null);

  function syncCharPref(prefName) {
    SettingsListener.observe(prefName, null, function(value) {
      // If set, propagate setting value to pref.
      if (value) {
        defaultBranch.setCharPref(prefName, value);
        return;
      }
      // If unset, initialize setting to pref value.
      try {
        let value = defaultBranch.getCharPref(prefName);
        if (value) {
          let setting = {};
          setting[prefName] = value;
          window.navigator.mozSettings.createLock().set(setting);
        }
      } catch(e) {
        console.log('Unable to read pref ' + prefName + ': ' + e);
      }
    });
  }

  syncCharPref('app.update.url');
  syncCharPref('app.update.channel');
})();

// ================ Debug ================
(function Composer2DSettingToPref() {
  //layers.composer.enabled can be enabled in three ways
  //In order of precedence they are:
  //
  //1. mozSettings "layers.composer.enabled"
  //2. a gecko pref "layers.composer.enabled"
  //3. presence of ro.display.colorfill at the Gonk level

  var req = navigator.mozSettings.createLock().get('layers.composer2d.enabled');
  req.onsuccess = function() {
    if (typeof(req.result['layers.composer2d.enabled']) === 'undefined') {
      var enabled = false;
      if (Services.prefs.getPrefType('layers.composer2d.enabled') == Ci.nsIPrefBranch.PREF_BOOL) {
        enabled = Services.prefs.getBoolPref('layers.composer2d.enabled');
      } else {
#ifdef MOZ_WIDGET_GONK
        let androidVersion = libcutils.property_get("ro.build.version.sdk");
        if (androidVersion >= 17 ) {
          enabled = true;
        } else {
          enabled = (libcutils.property_get('ro.display.colorfill') === '1');
        }
#endif
      }
      navigator.mozSettings.createLock().set({'layers.composer2d.enabled': enabled });
    }

    SettingsListener.observe("layers.composer2d.enabled", true, function(value) {
      Services.prefs.setBoolPref("layers.composer2d.enabled", value);
    });
  };
  req.onerror = function() {
    dump("Error configuring layers.composer2d.enabled setting");
  };

})();

// ================ Accessibility ============
(function setupAccessibility() {
  let accessibilityScope = {};
  SettingsListener.observe("accessibility.screenreader", false, function(value) {
    if (!value) {
      return;
    }
    if (!('AccessFu' in accessibilityScope)) {
      Cu.import('resource://gre/modules/accessibility/AccessFu.jsm',
                accessibilityScope);
      accessibilityScope.AccessFu.attach(window);
    }
  });
})();

// ================ Theming ============
(function themingSettingsListener() {
  let themingPrefs = ['ui.menu', 'ui.menutext', 'ui.infobackground', 'ui.infotext',
                      'ui.window', 'ui.windowtext', 'ui.highlight'];

  themingPrefs.forEach(function(pref) {
    SettingsListener.observe('gaia.' + pref, null, function(value) {
      if (value) {
        Services.prefs.setCharPref(pref, value);
      }
    });
  });
})();

// =================== Telemetry  ======================
(function setupTelemetrySettings() {
  let gaiaSettingName = 'debug.performance_data.shared';
  let geckoPrefName = 'toolkit.telemetry.enabled';
  SettingsListener.observe(gaiaSettingName, null, function(value) {
    if (value !== null) {
      // Gaia setting has been set; update Gecko pref to that.
      Services.prefs.setBoolPref(geckoPrefName, value);
      return;
    }
    // Gaia setting has not been set; set the gaia setting to default.
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
    let prefValue = true;
#else
    let prefValue = false;
#endif
    try {
      prefValue = Services.prefs.getBoolPref(geckoPrefName);
    } catch (e) {
      // Pref not set; use default value.
    }
    let setting = {};
    setting[gaiaSettingName] = prefValue;
    window.navigator.mozSettings.createLock().set(setting);
  });
})();

// =================== Low-precision buffer ======================
(function setupLowPrecisionSettings() {
  // The gaia setting layers.low-precision maps to two gecko prefs
  SettingsListener.observe('layers.low-precision', null, function(value) {
    if (value !== null) {
      // Update gecko from the new Gaia setting
      Services.prefs.setBoolPref('layers.low-precision-buffer', value);
      Services.prefs.setBoolPref('layers.progressive-paint', value);
    } else {
      // Update gaia setting from gecko value
      try {
        let prefValue = Services.prefs.getBoolPref('layers.low-precision-buffer');
        let setting = { 'layers.low-precision': prefValue };
        window.navigator.mozSettings.createLock().set(setting);
      } catch (e) {
        console.log('Unable to read pref layers.low-precision-buffer: ' + e);
      }
    }
  });

  // The gaia setting layers.low-opacity maps to a string gecko pref (0.5/1.0)
  SettingsListener.observe('layers.low-opacity', null, function(value) {
    if (value !== null) {
      // Update gecko from the new Gaia setting
      Services.prefs.setCharPref('layers.low-precision-opacity', value ? '0.5' : '1.0');
    } else {
      // Update gaia setting from gecko value
      try {
        let prefValue = Services.prefs.getCharPref('layers.low-precision-opacity');
        let setting = { 'layers.low-opacity': (prefValue == '0.5') };
        window.navigator.mozSettings.createLock().set(setting);
      } catch (e) {
        console.log('Unable to read pref layers.low-precision-opacity: ' + e);
      }
    }
  });
})();

// ================ Theme selection ============
// theme.selected holds the manifest url of the currently used theme.
SettingsListener.observe("theme.selected",
                         "app://default_theme.gaiamobile.org/manifest.webapp",
                         function(value) {
  if (!value) {
    return;
  }

  let newTheme;
  try {
    let enabled = Services.prefs.getBoolPref("dom.mozApps.themable");
    if (!enabled) {
      return;
    }

    // Make sure this is a url, and only keep the host part to set the pref.
    let uri = Services.io.newURI(value, null, null);
    // We only support overriding in the app:// protocol handler.
    if (uri.scheme !== "app") {
      return;
    }
    newTheme = uri.host;
  } catch(e) {
    return;
  }

  let currentTheme;
  try {
    currentTheme = Services.prefs.getCharPref('dom.mozApps.selected_theme');
  } catch(e) {};

  if (currentTheme != newTheme) {
    debug("New theme selected " + value);
    Services.prefs.setCharPref('dom.mozApps.selected_theme', newTheme);
    Services.prefs.savePrefFile(null);
    Services.obs.notifyObservers(null, 'app-theme-changed', newTheme);
  }
});

// =================== Proxy server ======================
(function setupBrowsingProxySettings() {
  function setPAC() {
    let usePAC;
    try {
      usePAC = Services.prefs.getBoolPref('network.proxy.pac_generator');
    } catch (ex) {}

    if (usePAC) {
      Services.prefs.setCharPref('network.proxy.autoconfig_url',
                                 gPACGenerator.generate());
      Services.prefs.setIntPref('network.proxy.type',
                                Ci.nsIProtocolProxyService.PROXYCONFIG_PAC);
    }
  }

  SettingsListener.observe('browser.proxy.enabled', false, function(value) {
    Services.prefs.setBoolPref('network.proxy.browsing.enabled', value);
    setPAC();
  });

  SettingsListener.observe('browser.proxy.host', '', function(value) {
    Services.prefs.setCharPref('network.proxy.browsing.host', value);
    setPAC();
  });

  SettingsListener.observe('browser.proxy.port', 0, function(value) {
    Services.prefs.setIntPref('network.proxy.browsing.port', value);
    setPAC();
  });

  setPAC();
})();

// =================== Various simple mapping  ======================
let settingsToObserve = {
  'accessibility.screenreader_quicknav_modes': {
    prefName: 'accessibility.accessfu.quicknav_modes',
    resetToPref: true,
    defaultValue: ''
  },
  'accessibility.screenreader_quicknav_index': {
    prefName: 'accessibility.accessfu.quicknav_index',
    resetToPref: true,
    defaultValue: 0
  },
  'app.update.interval': 86400,
  'apz.overscroll.enabled': true,
  'debug.fps.enabled': {
    prefName: 'layers.acceleration.draw-fps',
    defaultValue: false
  },
  'debug.log-animations.enabled': {
    prefName: 'layers.offmainthreadcomposition.log-animations',
    defaultValue: false
  },
  'debug.paint-flashing.enabled': {
    prefName: 'nglayout.debug.paint_flashing',
    defaultValue: false
  },
  // FIXME: Bug 1185806 - Provide a common device name setting.
  // Borrow device name from developer's menu to avoid multiple name settings.
  'devtools.discovery.device': {
    prefName: 'dom.presentation.device.name',
    defaultValue: 'Firefox OS'
  },
  'devtools.eventlooplag.threshold': 100,
  'devtools.remote.wifi.visible': {
    resetToPref: true
  },
  'dom.mozApps.use_reviewer_certs': false,
  'dom.mozApps.signed_apps_installable_from': 'https://marketplace.firefox.com',
  'dom.presentation.discovery.enabled': false,
  'dom.presentation.discoverable': false,
  'dom.serviceWorkers.interception.enabled': true,
  'dom.serviceWorkers.testing.enabled': false,
  'gfx.layerscope.enabled': false,
  'layers.draw-borders': false,
  'layers.draw-tile-borders': false,
  'layers.dump': false,
  'layers.enable-tiles': true,
  'layers.effect.invert': false,
  'layers.effect.grayscale': false,
  'layers.effect.contrast': '0.0',
  'layout.display-list.dump': false,
  'mms.debugging.enabled': false,
  'network.debugging.enabled': false,
  'privacy.donottrackheader.enabled': false,
  'ril.debugging.enabled': false,
  'ril.radio.disabled': false,
  'ril.mms.requestReadReport.enabled': {
    prefName: 'dom.mms.requestReadReport',
    defaultValue: true
  },
  'ril.mms.requestStatusReport.enabled': {
    prefName: 'dom.mms.requestStatusReport',
    defaultValue: false
  },
  'ril.mms.retrieval_mode': {
    prefName: 'dom.mms.retrieval_mode',
    defaultValue: 'manual'
  },
  'ril.sms.requestStatusReport.enabled': {
    prefName: 'dom.sms.requestStatusReport',
    defaultValue: false
  },
  'ril.sms.strict7BitEncoding.enabled': {
    prefName: 'dom.sms.strict7BitEncoding',
    defaultValue: false
  },
  'ril.sms.maxReadAheadEntries': {
    prefName: 'dom.sms.maxReadAheadEntries',
    defaultValue: 7
  },
  'ui.touch.radius.leftmm': {
    resetToPref: true
  },
  'ui.touch.radius.topmm': {
    resetToPref: true
  },
  'ui.touch.radius.rightmm': {
    resetToPref: true
  },
  'ui.touch.radius.bottommm': {
    resetToPref: true
  },
  'wap.UAProf.tagname': 'x-wap-profile',
  'wap.UAProf.url': ''
};

for (let key in settingsToObserve) {
  let setting = settingsToObserve[key];

  // Allow setting to contain flags redefining prefName and defaultValue.
  let prefName = setting.prefName || key;
  let defaultValue = setting.defaultValue;
  if (defaultValue === undefined) {
    defaultValue = setting;
  }

  let prefs = Services.prefs;

  // If requested, reset setting value and defaultValue to the pref value.
  if (setting.resetToPref) {
    switch (prefs.getPrefType(prefName)) {
      case Ci.nsIPrefBranch.PREF_BOOL:
        defaultValue = prefs.getBoolPref(prefName);
        break;

      case Ci.nsIPrefBranch.PREF_INT:
        defaultValue = prefs.getIntPref(prefName);
        break;

      case Ci.nsIPrefBranch.PREF_STRING:
        defaultValue = prefs.getCharPref(prefName);
        break;
    }

    let setting = {};
    setting[key] = defaultValue;
    window.navigator.mozSettings.createLock().set(setting);
  }

  // Figure out the right setter function for this type of pref.
  let setPref;
  switch (typeof defaultValue) {
    case 'boolean':
      setPref = prefs.setBoolPref.bind(prefs);
      break;

    case 'number':
      setPref = prefs.setIntPref.bind(prefs);
      break;

    case 'string':
      setPref = prefs.setCharPref.bind(prefs);
      break;
  }

  SettingsListener.observe(key, defaultValue, function(value) {
    setPref(prefName, value);
  });
};
