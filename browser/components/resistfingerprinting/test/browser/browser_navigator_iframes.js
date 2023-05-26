/**
 * Bug 1737829 and Bug 1770498 - A test case for making sure the navigator object has been
 *   spoofed/disabled correctly respecting cross-origin resources, iframes
 *   and exemption behavior.
 *
 * This test only tests values in the iframe, it does not test them on the framer
 * We use the cross-origin domain as the base URI of a resource we fetch (on both the framer and framee)
 * so we can check that the HTTP header is as expected.
 *
 * Covers the following cases:
 *  - RFP is disabled entirely
 *  - RFP is enabled entirely
 *
 *  - (A) RFP is exempted on the framer and framee and (if needed) on another cross-origin domain
 *  - (B) RFP is exempted on the framer and framee but is not on another (if needed) cross-origin domain
 *  - (C) RFP is exempted on the framer and (if needed) on another cross-origin domain, but not the framee
 *  - (D) RFP is exempted on the framer but not the framee nor another (if needed) cross-origin domain
 *  - (E) RFP is not exempted on the framer nor the framee but (if needed) is exempted on another cross-origin domain
 *  - (F) RFP is not exempted on the framer nor the framee nor another (if needed) cross-origin domain
 *  - (G) RFP is not exempted on the framer but is on the framee and (if needed) on another cross-origin domain
 *  - (H) RFP is not exempted on the framer nor another (if needed) cross-origin domain but is on the framee
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.sys.mjs",
  WindowsVersionInfo:
    "resource://gre/modules/components-utils/WindowsVersionInfo.sys.mjs",
});

let osVersion = Services.sysinfo.get("version");
if (AppConstants.platform == "macosx") {
  // Convert Darwin version to macOS version: 19.x.x -> 10.15 etc.
  // https://en.wikipedia.org/wiki/Darwin_%28operating_system%29
  let DarwinVersionParts = osVersion.split(".");
  let DarwinMajorVersion = +DarwinVersionParts[0];
  let macOsMinorVersion = DarwinMajorVersion - 4;
  if (macOsMinorVersion > 15) {
    macOsMinorVersion = 15;
  }
  osVersion = `10.${macOsMinorVersion}`;
}

const DEFAULT_APPVERSION = {
  linux: "5.0 (X11)",
  win: "5.0 (Windows)",
  macosx: "5.0 (Macintosh)",
  android: `5.0 (Android ${osVersion})`,
  other: "5.0 (X11)",
};

const SPOOFED_APPVERSION = {
  linux: "5.0 (X11)",
  win: "5.0 (Windows)",
  macosx: "5.0 (Macintosh)",
  android: "5.0 (Android 10)",
  other: "5.0 (X11)",
};

let cpuArch = Services.sysinfo.get("arch");
if (cpuArch == "x86-64") {
  // Convert CPU arch "x86-64" to "x86_64" used in Linux and Android UAs.
  cpuArch = "x86_64";
}

const DEFAULT_PLATFORM = {
  linux: `Linux ${cpuArch}`,
  win: "Win32",
  macosx: "MacIntel",
  android: `Linux ${cpuArch}`,
  other: `Linux ${cpuArch}`,
};

const SPOOFED_PLATFORM = {
  linux: "Linux x86_64",
  win: "Win32",
  macosx: "MacIntel",
  android: "Linux aarch64",
  other: "Linux x86_64",
};

// If comparison with the WindowsOscpu value fails in the future, it's time to
// evaluate if exposing a new Windows version to the Web is appropriate. See
// https://bugzilla.mozilla.org/show_bug.cgi?id=1693295
let WindowsOscpu = null;
if (AppConstants.platform == "win") {
  let isWin11 = WindowsVersionInfo.get().buildNumber >= 22000;
  WindowsOscpu =
    cpuArch == "x86_64" || (cpuArch == "aarch64" && isWin11)
      ? `Windows NT ${osVersion}; Win64; x64`
      : `Windows NT ${osVersion}`;
}

const DEFAULT_OSCPU = {
  linux: `Linux ${cpuArch}`,
  win: WindowsOscpu,
  macosx: `Intel Mac OS X ${osVersion}`,
  android: `Linux ${cpuArch}`,
  other: `Linux ${cpuArch}`,
};

const SPOOFED_OSCPU = {
  linux: "Linux x86_64",
  win: "Windows NT 10.0; Win64; x64",
  macosx: "Intel Mac OS X 10.15",
  android: "Linux aarch64",
  other: "Linux x86_64",
};

const DEFAULT_UA_OS = {
  linux: `X11; Linux ${cpuArch}`,
  win: WindowsOscpu,
  macosx: `Macintosh; Intel Mac OS X ${osVersion}`,
  android: `Android ${osVersion}; Mobile`,
  other: `X11; Linux ${cpuArch}`,
};

const SPOOFED_UA_NAVIGATOR_OS = {
  linux: "X11; Linux x86_64",
  win: "Windows NT 10.0; Win64; x64",
  macosx: "Macintosh; Intel Mac OS X 10.15",
  android: "Android 10; Mobile",
  other: "X11; Linux x86_64",
};
const SPOOFED_UA_HTTPHEADER_OS = {
  linux: "Windows NT 10.0",
  win: "Windows NT 10.0",
  macosx: "Windows NT 10.0",
  android: "Android 10; Mobile",
  other: "Windows NT 10.0",
};
const SPOOFED_HW_CONCURRENCY = 2;

const CONST_APPCODENAME = "Mozilla";
const CONST_APPNAME = "Netscape";
const CONST_PRODUCT = "Gecko";
const CONST_PRODUCTSUB = "20100101";
const CONST_VENDOR = "";
const CONST_VENDORSUB = "";

const appVersion = parseInt(Services.appinfo.version);
const rvVersion =
  parseInt(
    Services.prefs.getIntPref("network.http.useragent.forceRVOnly", 0),
    0
  ) || appVersion;
const spoofedVersion = AppConstants.platform == "android" ? "115" : appVersion;

const LEGACY_UA_GECKO_TRAIL = "20100101";

const DEFAULT_UA_GECKO_TRAIL = {
  linux: LEGACY_UA_GECKO_TRAIL,
  win: LEGACY_UA_GECKO_TRAIL,
  macosx: LEGACY_UA_GECKO_TRAIL,
  android: `${appVersion}.0`,
  other: LEGACY_UA_GECKO_TRAIL,
};

const SPOOFED_UA_GECKO_TRAIL = {
  linux: LEGACY_UA_GECKO_TRAIL,
  win: LEGACY_UA_GECKO_TRAIL,
  macosx: LEGACY_UA_GECKO_TRAIL,
  android: `${spoofedVersion}.0`,
  other: LEGACY_UA_GECKO_TRAIL,
};

const DEFAULT_HARDWARE_CONCURRENCY = navigator.hardwareConcurrency;

// =============================================================================================
// =============================================================================================

async function testNavigator(result, expectedResults, extraData) {
  let testDesc = extraData.testDesc;

  is(
    result.appVersion,
    expectedResults.appVersion,
    `Checking ${testDesc} navigator.appVersion.`
  );
  is(
    result.platform,
    expectedResults.platform,
    `Checking ${testDesc} navigator.platform.`
  );
  is(
    result.userAgent,
    expectedResults.userAgentNavigator,
    `Checking ${testDesc} navigator.userAgent.`
  );
  is(
    result.userAgentHTTPHeader,
    expectedResults.userAgentHTTPHeader,
    `Checking ${testDesc} userAgentHTTPHeader.`
  );
  is(
    result.framer_crossOrigin_userAgentHTTPHeader,
    expectedResults.framer_crossOrigin_userAgentHTTPHeader,
    `Checking ${testDesc} framer contacting cross-origin userAgentHTTPHeader.`
  );
  is(
    result.framee_crossOrigin_userAgentHTTPHeader,
    expectedResults.framee_crossOrigin_userAgentHTTPHeader,
    `Checking ${testDesc} framee contacting cross-origin userAgentHTTPHeader.`
  );
  is(
    result.mimeTypesLength,
    expectedResults.mimeTypesLength,
    `Navigator.mimeTypes has a length of ${expectedResults.mimeTypesLength}.`
  );
  is(
    result.pluginsLength,
    expectedResults.pluginsLength,
    `Navigator.plugins has a length of ${expectedResults.pluginsLength}.`
  );
  is(
    result.oscpu,
    expectedResults.oscpu,
    `Checking ${testDesc} navigator.oscpu.`
  );
  is(
    result.hardwareConcurrency,
    expectedResults.hardwareConcurrency,
    `Checking ${testDesc} navigator.hardwareConcurrency.`
  );

  is(
    result.appCodeName,
    CONST_APPCODENAME,
    "Navigator.appCodeName reports correct constant value."
  );
  is(
    result.appName,
    CONST_APPNAME,
    "Navigator.appName reports correct constant value."
  );
  is(
    result.product,
    CONST_PRODUCT,
    "Navigator.product reports correct constant value."
  );
  is(
    result.productSub,
    CONST_PRODUCTSUB,
    "Navigator.productSub reports correct constant value."
  );
  is(
    result.vendor,
    CONST_VENDOR,
    "Navigator.vendor reports correct constant value."
  );
  is(
    result.vendorSub,
    CONST_VENDORSUB,
    "Navigator.vendorSub reports correct constant value."
  );

  is(
    result.worker_appVersion,
    expectedResults.appVersion,
    `Checking ${testDesc} worker navigator.appVersion.`
  );
  is(
    result.worker_platform,
    expectedResults.platform,
    `Checking ${testDesc} worker navigator.platform.`
  );
  is(
    result.worker_userAgent,
    expectedResults.userAgentNavigator,
    `Checking ${testDesc} worker navigator.userAgent.`
  );
  is(
    result.worker_hardwareConcurrency,
    expectedResults.hardwareConcurrency,
    `Checking ${testDesc} worker navigator.hardwareConcurrency.`
  );

  is(
    result.worker_appCodeName,
    CONST_APPCODENAME,
    "worker Navigator.appCodeName reports correct constant value."
  );
  is(
    result.worker_appName,
    CONST_APPNAME,
    "worker Navigator.appName reports correct constant value."
  );
  is(
    result.worker_product,
    CONST_PRODUCT,
    "worker Navigator.product reports correct constant value."
  );
}

const defaultUserAgent = `Mozilla/5.0 (${
  DEFAULT_UA_OS[AppConstants.platform]
}; rv:${rvVersion}.0) Gecko/${
  DEFAULT_UA_GECKO_TRAIL[AppConstants.platform]
} Firefox/${appVersion}.0`;

const spoofedUserAgentNavigator = `Mozilla/5.0 (${
  SPOOFED_UA_NAVIGATOR_OS[AppConstants.platform]
}; rv:${rvVersion}.0) Gecko/${
  SPOOFED_UA_GECKO_TRAIL[AppConstants.platform]
} Firefox/${appVersion}.0`;

const spoofedUserAgentHeader = `Mozilla/5.0 (${
  SPOOFED_UA_HTTPHEADER_OS[AppConstants.platform]
}; rv:${rvVersion}.0) Gecko/${
  SPOOFED_UA_GECKO_TRAIL[AppConstants.platform]
} Firefox/${appVersion}.0`;

// The following are convenience objects that allow you to quickly see what is
//   and is not modified from a logical set of values.
// Be sure to always use `let expectedResults = structuredClone(allNotSpoofed)` to do a
//   deep copy and avoiding corrupting the original 'const' object
const allNotSpoofed = {
  appVersion: DEFAULT_APPVERSION[AppConstants.platform],
  hardwareConcurrency: navigator.hardwareConcurrency,
  mimeTypesLength: 2,
  oscpu: DEFAULT_OSCPU[AppConstants.platform],
  platform: DEFAULT_PLATFORM[AppConstants.platform],
  pluginsLength: 5,
  userAgentNavigator: defaultUserAgent,
  userAgentHTTPHeader: defaultUserAgent,
  framer_crossOrigin_userAgentHTTPHeader: defaultUserAgent,
  framee_crossOrigin_userAgentHTTPHeader: defaultUserAgent,
};
const allSpoofed = {
  appVersion: SPOOFED_APPVERSION[AppConstants.platform],
  hardwareConcurrency: SPOOFED_HW_CONCURRENCY,
  mimeTypesLength: 2,
  oscpu: SPOOFED_OSCPU[AppConstants.platform],
  platform: SPOOFED_PLATFORM[AppConstants.platform],
  pluginsLength: 5,
  userAgentNavigator: spoofedUserAgentNavigator,
  userAgentHTTPHeader: spoofedUserAgentHeader,
  framer_crossOrigin_userAgentHTTPHeader: spoofedUserAgentHeader,
  framee_crossOrigin_userAgentHTTPHeader: spoofedUserAgentHeader,
};

const uri = `https://${FRAMER_DOMAIN}/browser/browser/components/resistfingerprinting/test/browser/file_navigator_iframer.html`;

requestLongerTimeout(2);

let expectedResults = {};

expectedResults = structuredClone(allNotSpoofed);
add_task(defaultsTest.bind(null, uri, testNavigator, expectedResults));

expectedResults = structuredClone(allSpoofed);
add_task(simpleRFPTest.bind(null, uri, testNavigator, expectedResults));

// In the below tests, we use the cross-origin domain as the base URI of a resource we fetch (on both the framer and framee)
// so we can check that the HTTP header is as expected.

// (A) RFP is exempted on the framer and framee and (if needed) on another cross-origin domain
expectedResults = structuredClone(allNotSpoofed);
add_task(testA.bind(null, uri, testNavigator, expectedResults));

// (B) RFP is exempted on the framer and framee but is not on another (if needed) cross-origin domain
expectedResults = structuredClone(allNotSpoofed);
add_task(testB.bind(null, uri, testNavigator, expectedResults));

// (C) RFP is exempted on the framer and (if needed) on another cross-origin domain, but not the framee
expectedResults = structuredClone(allSpoofed);
expectedResults.framer_crossOrigin_userAgentHTTPHeader = defaultUserAgent;
expectedResults.framee_crossOrigin_userAgentHTTPHeader = spoofedUserAgentHeader;
add_task(testC.bind(null, uri, testNavigator, expectedResults));

// (D) RFP is exempted on the framer but not the framee nor another (if needed) cross-origin domain
expectedResults = structuredClone(allSpoofed);
expectedResults.framer_crossOrigin_userAgentHTTPHeader = defaultUserAgent;
expectedResults.framee_crossOrigin_userAgentHTTPHeader = spoofedUserAgentHeader;
add_task(testD.bind(null, uri, testNavigator, expectedResults));

// (E) RFP is not exempted on the framer nor the framee but (if needed) is exempted on another cross-origin domain
expectedResults = structuredClone(allSpoofed);
add_task(testE.bind(null, uri, testNavigator, expectedResults));

// (F) RFP is not exempted on the framer nor the framee nor another (if needed) cross-origin domain
expectedResults = structuredClone(allSpoofed);
add_task(testF.bind(null, uri, testNavigator, expectedResults));

// (G) RFP is not exempted on the framer but is on the framee and (if needed) on another cross-origin domain
expectedResults = structuredClone(allSpoofed);
add_task(testG.bind(null, uri, testNavigator, expectedResults));

// (H) RFP is not exempted on the framer nor another (if needed) cross-origin domain but is on the framee
expectedResults = structuredClone(allSpoofed);
add_task(testH.bind(null, uri, testNavigator, expectedResults));
