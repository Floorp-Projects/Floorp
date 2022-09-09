/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci } = require("chrome");

loader.lazyRequireGetter(
  this,
  "DevToolsServer",
  "devtools/server/devtools-server",
  true
);
loader.lazyRequireGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm",
  true
);
loader.lazyGetter(this, "hostname", () => {
  try {
    // On some platforms (Linux according to try), this service does not exist and fails.
    return Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService)
      .myHostName;
  } catch (e) {
    return "";
  }
});
loader.lazyGetter(this, "endianness", () => {
  if (new Uint32Array(new Uint8Array([1, 2, 3, 4]).buffer)[0] === 0x04030201) {
    return "LE";
  }
  return "BE";
});

const APP_MAP = {
  "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}": "firefox",
  "{3550f703-e582-4d05-9a08-453d09bdfdc6}": "thunderbird",
  "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}": "seamonkey",
  "{718e30fb-e89b-41dd-9da7-e25a45638b28}": "sunbird",
  "{aa3c5121-dab2-40e2-81ca-7ea25febc110}": "mobile/android",
};

var CACHED_INFO = null;

function getSystemInfo() {
  if (CACHED_INFO) {
    return CACHED_INFO;
  }

  const appInfo = Services.appinfo;
  const win = Services.wm.getMostRecentWindow(DevToolsServer.chromeWindowType);
  const [processor, compiler] = appInfo.XPCOMABI.split("-");
  let dpi, useragent, width, height, physicalWidth, physicalHeight, brandName;
  const appid = appInfo.ID;
  const apptype = APP_MAP[appid];
  const geckoVersion = appInfo.platformVersion;
  const hardware = "unknown";
  let version = "unknown";

  const os = appInfo.OS;
  version = appInfo.version;

  const bundle = Services.strings.createBundle(
    "chrome://branding/locale/brand.properties"
  );
  if (bundle) {
    brandName = bundle.GetStringFromName("brandFullName");
  } else {
    brandName = null;
  }

  if (win) {
    const utils = win.windowUtils;
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
    locale: Services.locale.appLocaleAsBCP47,

    /**
     * Information regarding the operating system.
     */

    // Returns the endianness of the architecture: either "LE" or "BE"
    endianness,

    // Returns the hostname of the machine
    hostname,

    // Name of the OS type. Typically the same as `uname -s`. Possible values:
    // https://developer.mozilla.org/en/OS_TARGET
    os,
    platform: os,

    // hardware and version info from `deviceinfo.hardware`
    // and `deviceinfo.os`.
    hardware,

    // Device name. This property is only available on Android.
    // e.g. "Pixel 2"
    deviceName: getDeviceName(),

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

function getDeviceName() {
  try {
    // Will throw on other platforms than Firefox for Android.
    return Services.sysinfo.getProperty("device");
  } catch (e) {
    return null;
  }
}

function getProfileLocation() {
  // In child processes, we cannot access the profile location.
  try {
    // For some reason this line must come first or in xpcshell tests
    // nsXREDirProvider never gets initialised and so the profile service
    // crashes on initialisation.
    const profd = Services.dirsvc.get("ProfD", Ci.nsIFile);
    const profservice = Cc["@mozilla.org/toolkit/profile-service;1"].getService(
      Ci.nsIToolkitProfileService
    );
    if (profservice.currentProfile) {
      return profservice.currentProfile.name;
    }

    return profd.leafName;
  } catch (e) {
    return "";
  }
}

exports.getSystemInfo = getSystemInfo;
