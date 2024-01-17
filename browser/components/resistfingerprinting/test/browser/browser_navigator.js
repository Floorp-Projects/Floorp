/**
 * Bug 1333651 - A test case for making sure the navigator object has been
 *   spoofed/disabled correctly.
 */

"use strict";

const CC = Components.Constructor;

ChromeUtils.defineESModuleGetters(this, {
  WindowsVersionInfo:
    "resource://gre/modules/components-utils/WindowsVersionInfo.sys.mjs",
});

let expectedResults;

const DEFAULT_APPVERSION = {
  linux: "5.0 (X11)",
  win: "5.0 (Windows)",
  macosx: "5.0 (Macintosh)",
  android: "5.0 (Android 10)",
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

// Hard code the User-Agent string's CPU arch on Android, Linux, and other
// Unix-like platforms. This pref can be removed after we're confident there
// are no webcompat problems.
const freezeCpu = Services.prefs.getBoolPref(
  "network.http.useragent.freezeCpu",
  false
);

let defaultLinuxCpu;
if (freezeCpu) {
  defaultLinuxCpu = AppConstants.platform == "android" ? "armv81" : "x86_64";
} else {
  defaultLinuxCpu = cpuArch;
}

const DEFAULT_PLATFORM = {
  linux: `Linux ${defaultLinuxCpu}`,
  win: "Win32",
  macosx: "MacIntel",
  android: `Linux ${defaultLinuxCpu}`,
  other: `Linux ${defaultLinuxCpu}`,
};

const SPOOFED_PLATFORM = {
  linux: "Linux x86_64",
  win: "Win32",
  macosx: "MacIntel",
  android: "Linux armv81",
  other: "Linux x86_64",
};

// If comparison with the WindowsOscpu value fails in the future, it's time to
// evaluate if exposing a new Windows version to the Web is appropriate. See
// https://bugzilla.mozilla.org/show_bug.cgi?id=1693295
const WindowsOscpuPromise = (async () => {
  let WindowsOscpu = null;
  if (AppConstants.platform == "win") {
    let isWin11 = WindowsVersionInfo.get().buildNumber >= 22000;
    let isWow64 = (await Services.sysinfo.processInfo).isWow64;
    WindowsOscpu =
      cpuArch == "x86_64" || isWow64 || (cpuArch == "aarch64" && isWin11)
        ? "Windows NT 10.0; Win64; x64"
        : "Windows NT 10.0";
  }
  return WindowsOscpu;
})();

const DEFAULT_OSCPU = {
  linux: `Linux ${defaultLinuxCpu}`,
  macosx: "Intel Mac OS X 10.15",
  android: `Linux ${defaultLinuxCpu}`,
  other: `Linux ${defaultLinuxCpu}`,
};

const SPOOFED_OSCPU = {
  linux: "Linux x86_64",
  win: "Windows NT 10.0; Win64; x64",
  macosx: "Intel Mac OS X 10.15",
  android: "Linux armv81",
  other: "Linux x86_64",
};

const DEFAULT_UA_OS = {
  linux: `X11; Linux ${defaultLinuxCpu}`,
  macosx: "Macintosh; Intel Mac OS X 10.15",
  android: "Android 10; Mobile",
  other: `X11; Linux ${defaultLinuxCpu}`,
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
const CONST_LEGACY_BUILD_ID = "20181001000000";

const appVersion = parseInt(Services.appinfo.version);
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

add_setup(async () => {
  DEFAULT_OSCPU.win = DEFAULT_UA_OS.win = await WindowsOscpuPromise;
});

async function testUserAgentHeader() {
  const TEST_TARGET_URL = `${TEST_PATH}file_navigator_header.sjs?`;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TARGET_URL
  );

  let result = await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    return content.document.body.textContent;
  });

  is(
    result,
    expectedResults.userAgentHeader,
    `Checking ${expectedResults.testDesc} User Agent HTTP Header.`
  );

  BrowserTestUtils.removeTab(tab);
}

async function testNavigator() {
  // Open a tab to collect result.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_navigator.html"
  );

  let result = await SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    return content.document.getElementById("result").innerHTML;
  });

  result = JSON.parse(result);

  let testDesc = expectedResults.testDesc;

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
    result.buildID,
    CONST_LEGACY_BUILD_ID,
    "Navigator.buildID reports correct constant value."
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

  BrowserTestUtils.removeTab(tab);
}

async function testWorkerNavigator() {
  // Open a tab to collect result from worker.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_dummy.html"
  );

  let result = await SpecialPowers.spawn(
    tab.linkedBrowser,
    [],
    async function () {
      let worker = new content.SharedWorker(
        "file_navigator.worker.js",
        "WorkerNavigatorTest"
      );

      let res = await new Promise(resolve => {
        worker.port.onmessage = function (e) {
          resolve(e.data);
        };
      });

      return res;
    }
  );

  result = JSON.parse(result);

  let testDesc = expectedResults.testDesc;

  is(
    result.appVersion,
    expectedResults.appVersion,
    `Checking ${testDesc} worker navigator.appVersion.`
  );
  is(
    result.platform,
    expectedResults.platform,
    `Checking ${testDesc} worker navigator.platform.`
  );
  is(
    result.userAgent,
    expectedResults.userAgentNavigator,
    `Checking ${testDesc} worker navigator.userAgent.`
  );
  is(
    result.hardwareConcurrency,
    expectedResults.hardwareConcurrency,
    `Checking ${testDesc} worker navigator.hardwareConcurrency.`
  );

  is(
    result.appCodeName,
    CONST_APPCODENAME,
    "worker Navigator.appCodeName reports correct constant value."
  );
  is(
    result.appName,
    CONST_APPNAME,
    "worker Navigator.appName reports correct constant value."
  );
  is(
    result.product,
    CONST_PRODUCT,
    "worker Navigator.product reports correct constant value."
  );

  BrowserTestUtils.removeTab(tab);

  // Ensure the content process is shut down entirely before we start the next
  // test in Fission.
  if (SpecialPowers.useRemoteSubframes) {
    await new Promise(resolve => {
      let observer = (subject, topic, data) => {
        if (topic === "ipc:content-shutdown") {
          Services.obs.removeObserver(observer, "ipc:content-shutdown");
          resolve();
        }
      };
      Services.obs.addObserver(observer, "ipc:content-shutdown");
    });
  }
}

add_task(async function setupDefaultUserAgent() {
  let defaultUserAgent = `Mozilla/5.0 (${
    DEFAULT_UA_OS[AppConstants.platform]
  }; rv:${appVersion}.0) Gecko/${
    DEFAULT_UA_GECKO_TRAIL[AppConstants.platform]
  } Firefox/${appVersion}.0`;
  expectedResults = {
    testDesc: "default",
    appVersion: DEFAULT_APPVERSION[AppConstants.platform],
    hardwareConcurrency: navigator.hardwareConcurrency,
    mimeTypesLength: 2,
    oscpu: DEFAULT_OSCPU[AppConstants.platform],
    platform: DEFAULT_PLATFORM[AppConstants.platform],
    pluginsLength: 5,
    userAgentNavigator: defaultUserAgent,
    userAgentHeader: defaultUserAgent,
  };

  await testNavigator();

  await testUserAgentHeader();

  await testWorkerNavigator();
});

add_task(async function setupRFPExemptions() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      ["privacy.resistFingerprinting.exemptedDomains", "example.net"],
    ],
  });

  let defaultUserAgent = `Mozilla/5.0 (${
    DEFAULT_UA_OS[AppConstants.platform]
  }; rv:${appVersion}.0) Gecko/${
    DEFAULT_UA_GECKO_TRAIL[AppConstants.platform]
  } Firefox/${appVersion}.0`;

  expectedResults = {
    testDesc: "RFP Exempted Domain",
    appVersion: DEFAULT_APPVERSION[AppConstants.platform],
    hardwareConcurrency: navigator.hardwareConcurrency,
    mimeTypesLength: 2,
    oscpu: DEFAULT_OSCPU[AppConstants.platform],
    platform: DEFAULT_PLATFORM[AppConstants.platform],
    pluginsLength: 5,
    userAgentNavigator: defaultUserAgent,
    userAgentHeader: defaultUserAgent,
  };

  await testNavigator();

  await testUserAgentHeader();

  await testWorkerNavigator();

  // Pop exempted domains
  await SpecialPowers.popPrefEnv();
});

add_task(async function setupETPToggleExemptions() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.fingerprintingProtection", true],
      ["privacy.fingerprintingProtection.overrides", "+AllTargets"],
    ],
  });

  // Open a tab to toggle the ETP state.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_navigator.html"
  );
  let loaded = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    TEST_PATH + "file_navigator.html"
  );
  gProtectionsHandler.disableForCurrentPage();
  await loaded;
  BrowserTestUtils.removeTab(tab);

  let defaultUserAgent = `Mozilla/5.0 (${
    DEFAULT_UA_OS[AppConstants.platform]
  }; rv:${appVersion}.0) Gecko/${
    DEFAULT_UA_GECKO_TRAIL[AppConstants.platform]
  } Firefox/${appVersion}.0`;

  expectedResults = {
    testDesc: "ETP toggle Exempted Domain",
    appVersion: DEFAULT_APPVERSION[AppConstants.platform],
    hardwareConcurrency: navigator.hardwareConcurrency,
    mimeTypesLength: 2,
    oscpu: DEFAULT_OSCPU[AppConstants.platform],
    platform: DEFAULT_PLATFORM[AppConstants.platform],
    pluginsLength: 5,
    userAgentNavigator: defaultUserAgent,
    userAgentHeader: defaultUserAgent,
  };

  await testNavigator();

  await testUserAgentHeader();

  await testWorkerNavigator();

  // Toggle the ETP state back.
  tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_PATH + "file_navigator.html"
  );
  loaded = BrowserTestUtils.browserLoaded(
    tab.linkedBrowser,
    false,
    TEST_PATH + "file_navigator.html"
  );
  gProtectionsHandler.enableForCurrentPage();
  await loaded;
  BrowserTestUtils.removeTab(tab);

  // Pop exempted domains
  await SpecialPowers.popPrefEnv();
});

add_task(async function setupResistFingerprinting() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  let spoofedGeckoTrail = SPOOFED_UA_GECKO_TRAIL[AppConstants.platform];

  let spoofedUserAgentNavigator = `Mozilla/5.0 (${
    SPOOFED_UA_NAVIGATOR_OS[AppConstants.platform]
  }; rv:${appVersion}.0) Gecko/${spoofedGeckoTrail} Firefox/${appVersion}.0`;

  let spoofedUserAgentHeader = `Mozilla/5.0 (${
    SPOOFED_UA_HTTPHEADER_OS[AppConstants.platform]
  }; rv:${appVersion}.0) Gecko/${spoofedGeckoTrail} Firefox/${appVersion}.0`;

  expectedResults = {
    testDesc: "spoofed",
    appVersion: SPOOFED_APPVERSION[AppConstants.platform],
    hardwareConcurrency: SPOOFED_HW_CONCURRENCY,
    mimeTypesLength: 2,
    oscpu: SPOOFED_OSCPU[AppConstants.platform],
    platform: SPOOFED_PLATFORM[AppConstants.platform],
    pluginsLength: 5,
    userAgentNavigator: spoofedUserAgentNavigator,
    userAgentHeader: spoofedUserAgentHeader,
  };

  await testNavigator();

  await testUserAgentHeader();

  await testWorkerNavigator();
});

// This tests that 'general.*.override' should not override spoofed values.
add_task(async function runOverrideTest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["general.appversion.override", "appVersion overridden"],
      ["general.platform.override", "platform overridden"],
      ["general.useragent.override", "userAgent overridden"],
      ["general.oscpu.override", "oscpu overridden"],
      ["general.buildID.override", "buildID overridden"],
    ],
  });

  await testNavigator();

  await testWorkerNavigator();

  await testUserAgentHeader();

  // Pop general.appversion.override etc
  await SpecialPowers.popPrefEnv();

  // Pop privacy.resistFingerprinting
  await SpecialPowers.popPrefEnv();
});
