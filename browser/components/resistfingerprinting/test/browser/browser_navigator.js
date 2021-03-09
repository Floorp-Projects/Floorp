/**
 * Bug 1333651 - A test case for making sure the navigator object has been
 *   spoofed/disabled correctly.
 */

"use strict";

const CC = Components.Constructor;

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);

const TEST_PATH =
  "http://example.net/browser/browser/" +
  "components/resistfingerprinting/test/browser/";

let expectedResults;

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
  android: "5.0 (Android 9)",
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

// If comparison with this value fails in the future,
// it's time to evaluate if exposing a new Windows
// version to the Web is appropriate. See
// https://bugzilla.mozilla.org/show_bug.cgi?id=1693295
const WindowsOscpu =
  cpuArch == "x86_64"
    ? `Windows NT ${osVersion}; Win64; x64`
    : `Windows NT ${osVersion}`;

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
  android: "Android 9; Mobile",
  other: "X11; Linux x86_64",
};
const SPOOFED_UA_HTTPHEADER_OS = {
  linux: "Windows NT 10.0",
  win: "Windows NT 10.0",
  macosx: "Windows NT 10.0",
  android: "Android 9; Mobile",
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
const spoofedVersion = appVersion - ((appVersion - 78) % 13);

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

async function testUserAgentHeader() {
  const BASE =
    "http://mochi.test:8888/browser/browser/components/resistfingerprinting/test/browser/";
  const TEST_TARGET_URL = `${BASE}browser_navigator_header.sjs?`;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_TARGET_URL
  );

  let result = await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
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

  let result = await SpecialPowers.spawn(tab.linkedBrowser, [], function() {
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
  is(result.mimeTypesLength, 0, "Navigator.mimeTypes has a length of 0.");
  is(result.pluginsLength, 0, "Navigator.plugins has a length of 0.");
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
    async function() {
      let worker = new content.SharedWorker(
        "file_navigatorWorker.js",
        "WorkerNavigatorTest"
      );

      let res = await new Promise(resolve => {
        worker.port.onmessage = function(e) {
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
    oscpu: DEFAULT_OSCPU[AppConstants.platform],
    platform: DEFAULT_PLATFORM[AppConstants.platform],
    userAgentNavigator: defaultUserAgent,
    userAgentHeader: defaultUserAgent,
  };
});

add_task(async function runDefaultNavigatorTest() {
  await testNavigator();
});

add_task(async function runDefaultHTTPHeaderTest() {
  await testUserAgentHeader();
});

add_task(async function runDefaultWorkerNavigatorTest() {
  await testWorkerNavigator();
});

add_task(async function setupResistFingerprinting() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  let spoofedGeckoTrail = SPOOFED_UA_GECKO_TRAIL[AppConstants.platform];

  let spoofedUserAgentNavigator = `Mozilla/5.0 (${
    SPOOFED_UA_NAVIGATOR_OS[AppConstants.platform]
  }; rv:${spoofedVersion}.0) Gecko/${spoofedGeckoTrail} Firefox/${spoofedVersion}.0`;

  let spoofedUserAgentHeader = `Mozilla/5.0 (${
    SPOOFED_UA_HTTPHEADER_OS[AppConstants.platform]
  }; rv:${spoofedVersion}.0) Gecko/${spoofedGeckoTrail} Firefox/${spoofedVersion}.0`;

  expectedResults = {
    testDesc: "spoofed",
    appVersion: SPOOFED_APPVERSION[AppConstants.platform],
    hardwareConcurrency: SPOOFED_HW_CONCURRENCY,
    oscpu: SPOOFED_OSCPU[AppConstants.platform],
    platform: SPOOFED_PLATFORM[AppConstants.platform],
    userAgentNavigator: spoofedUserAgentNavigator,
    userAgentHeader: spoofedUserAgentHeader,
  };
});

add_task(async function runSpoofedNavigatorTest() {
  await testNavigator();
});

add_task(async function runSpoofedHTTPHeaderTest() {
  await testUserAgentHeader();
});

add_task(async function runSpoofedWorkerNavigatorTest() {
  await testWorkerNavigator();
});

// This tests that 'general.*.override' should not override spoofed values.
add_task(async function runOverrideTest() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["general.appname.override", "appName overridden"],
      ["general.appversion.override", "appVersion overridden"],
      ["general.platform.override", "platform overridden"],
      ["general.useragent.override", "userAgent overridden"],
      ["general.oscpu.override", "oscpu overridden"],
    ],
  });

  await testNavigator();

  await testWorkerNavigator();

  await testUserAgentHeader();
});
