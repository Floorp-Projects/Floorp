/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");

loader.lazyRequireGetter(this, "Services");
loader.lazyRequireGetter(this, "promise");
loader.lazyRequireGetter(this, "defer", "devtools/shared/defer");
loader.lazyRequireGetter(this, "DebuggerServer", "devtools/server/main", true);
loader.lazyRequireGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm", true);
loader.lazyGetter(this, "screenManager", () => {
  return Cc["@mozilla.org/gfx/screenmanager;1"].getService(Ci.nsIScreenManager);
});
loader.lazyGetter(this, "hostname", () => {
  try {
    // On some platforms (Linux according to try), this service does not exist and fails.
    return Cc["@mozilla.org/network/dns-service;1"]
              .getService(Ci.nsIDNSService).myHostName;
  } catch (e) {
    return "";
  }
});
loader.lazyGetter(this, "endianness", () => {
  if ((new Uint32Array((new Uint8Array([1, 2, 3, 4])).buffer))[0] === 0x04030201) {
    return "LE";
  }
  return "BE";
});

const APP_MAP = {
  "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}": "firefox",
  "{3550f703-e582-4d05-9a08-453d09bdfdc6}": "thunderbird",
  "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}": "seamonkey",
  "{718e30fb-e89b-41dd-9da7-e25a45638b28}": "sunbird",
  "{aa3c5121-dab2-40e2-81ca-7ea25febc110}": "mobile/android"
};

var CACHED_INFO = null;

async function getSystemInfo() {
  if (CACHED_INFO) {
    return CACHED_INFO;
  }

  const appInfo = Services.appinfo;
  const win = Services.wm.getMostRecentWindow(DebuggerServer.chromeWindowType);
  const [processor, compiler] = appInfo.XPCOMABI.split("-");
  let dpi,
    useragent,
    width,
    height,
    physicalWidth,
    physicalHeight,
    brandName;
  const appid = appInfo.ID;
  const apptype = APP_MAP[appid];
  const geckoVersion = appInfo.platformVersion;
  const hardware = "unknown";
  let version = "unknown";

  const os = appInfo.OS;
  version = appInfo.version;

  const bundle = Services.strings.createBundle("chrome://branding/locale/brand.properties");
  if (bundle) {
    brandName = bundle.GetStringFromName("brandFullName");
  } else {
    brandName = null;
  }

  if (win) {
    const utils = win.QueryInterface(Ci.nsIInterfaceRequestor)
                   .getInterface(Ci.nsIDOMWindowUtils);
    dpi = utils.displayDPI;
    useragent = win.navigator.userAgent;
    width = win.screen.width;
    height = win.screen.height;
    physicalWidth = win.screen.width * win.devicePixelRatio;
    physicalHeight = win.screen.height * win.devicePixelRatio;
  }

  const info = {

    /**
     * Information from nsIXULAppInfo, regarding
     * the application itself.
     */

    // The XUL application's UUID.
    appid,

    // Name of the app, "firefox", "thunderbird", etc., listed in APP_MAP
    apptype,

    // Mixed-case or empty string of vendor, like "Mozilla"
    vendor: appInfo.vendor,

    // Name of the application, like "Firefox", "Thunderbird".
    name: appInfo.name,

    // The application's version, for example "0.8.0+" or "3.7a1pre".
    // Typically, the version of Firefox, for example.
    // It is different than the version of Gecko or the XULRunner platform.
    version,

    // The application's build ID/date, for example "2004051604".
    appbuildid: appInfo.appBuildID,

    // The build ID/date of Gecko and the XULRunner platform.
    platformbuildid: appInfo.platformBuildID,
    geckobuildid: appInfo.platformBuildID,

    // The version of Gecko or XULRunner platform, for example "1.8.1.19" or
    // "1.9.3pre". In "Firefox 3.7 alpha 1" the application version is "3.7a1pre"
    // while the platform version is "1.9.3pre"
    platformversion: geckoVersion,
    geckoversion: geckoVersion,

    // Locale used in this build
    locale: Services.locale.getAppLocaleAsLangTag(),

    /**
     * Information regarding the operating system.
     */

    // Returns the endianness of the architecture: either "LE" or "BE"
    endianness: endianness,

    // Returns the hostname of the machine
    hostname: hostname,

    // Name of the OS type. Typically the same as `uname -s`. Possible values:
    // https://developer.mozilla.org/en/OS_TARGET
    os,
    platform: os,

    // hardware and version info from `deviceinfo.hardware`
    // and `deviceinfo.os`.
    hardware,

    // Type of process architecture running:
    // "arm", "ia32", "x86", "x64"
    // Alias to both `arch` and `processor` for node/deviceactor compat
    arch: processor,
    processor,

    // Name of compiler used for build:
    // `'msvc', 'n32', 'gcc2', 'gcc3', 'sunc', 'ibmc'...`
    compiler,

    // Location for the current profile
    profile: getProfileLocation(),

    // Update channel
    channel: AppConstants.MOZ_UPDATE_CHANNEL,

    dpi,
    useragent,
    width,
    height,
    physicalWidth,
    physicalHeight,
    brandName,
  };

  CACHED_INFO = info;
  return info;
}

function getProfileLocation() {
  // In child processes, we cannot access the profile location.
  try {
    const profd = Services.dirsvc.get("ProfD", Ci.nsIFile);
    const profservice = Cc["@mozilla.org/toolkit/profile-service;1"]
                        .getService(Ci.nsIToolkitProfileService);
    const profiles = profservice.profiles;
    while (profiles.hasMoreElements()) {
      const profile = profiles.getNext().QueryInterface(Ci.nsIToolkitProfile);
      if (profile.rootDir.path == profd.path) {
        return profile.name;
      }
    }

    return profd.leafName;
  } catch (e) {
    return "";
  }
}

/**
 * Function for fetching screen dimensions and returning
 * an enum for Telemetry.
 */
function getScreenDimensions() {
  const width = {};
  const height = {};

  screenManager.primaryScreen.GetRect({}, {}, width, height);
  const dims = width.value + "x" + height.value;

  if (width.value < 800 || height.value < 600) {
    return 0;
  }
  if (dims === "800x600") {
    return 1;
  }
  if (dims === "1024x768") {
    return 2;
  }
  if (dims === "1280x800") {
    return 3;
  }
  if (dims === "1280x1024") {
    return 4;
  }
  if (dims === "1366x768") {
    return 5;
  }
  if (dims === "1440x900") {
    return 6;
  }
  if (dims === "1920x1080") {
    return 7;
  }
  if (dims === "2560×1440") {
    return 8;
  }
  if (dims === "2560×1600") {
    return 9;
  }
  if (dims === "2880x1800") {
    return 10;
  }
  if (width.value > 2880 || height.value > 1800) {
    return 12;
  }

  // Other dimension such as a VM.
  return 11;
}

function getSetting(name) {
  const deferred = defer();

  if ("@mozilla.org/settingsService;1" in Cc) {
    let settingsService;

    // TODO bug 1205797, make this work in child processes.
    try {
      settingsService = Cc["@mozilla.org/settingsService;1"]
                          .getService(Ci.nsISettingsService);
    } catch (e) {
      return promise.reject(e);
    }

    settingsService.createLock().get(name, {
      handle: (_, value) => deferred.resolve(value),
      handleError: (error) => deferred.reject(error),
    });
  } else {
    deferred.reject(new Error("No settings service"));
  }
  return deferred.promise;
}

exports.getSystemInfo = getSystemInfo;
exports.getSetting = getSetting;
exports.getScreenDimensions = getScreenDimensions;
