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

var spoofedUserAgentNavigator;
var spoofedUserAgentHeader;

const SPOOFED_APPNAME = "Netscape";

const SPOOFED_APPVERSION = {
  linux: "5.0 (X11)",
  win: "5.0 (Windows)",
  macosx: "5.0 (Macintosh)",
  android: "5.0 (Android 9)",
  other: "5.0 (X11)",
};
const SPOOFED_PLATFORM = {
  linux: "Linux x86_64",
  win: "Win32",
  macosx: "MacIntel",
  android: "Linux aarch64",
  other: "Linux x86_64",
};
const SPOOFED_OSCPU = {
  linux: "Linux x86_64",
  win: "Windows NT 10.0; Win64; x64",
  macosx: "Intel Mac OS X 10.15",
  android: "Linux aarch64",
  other: "Linux x86_64",
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
const CONST_PRODUCT = "Gecko";
const CONST_PRODUCTSUB = "20100101";
const CONST_VENDOR = "";
const CONST_VENDORSUB = "";

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
    spoofedUserAgentHeader,
    "User Agent HTTP Header is correctly spoofed."
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

  is(
    result.appName,
    SPOOFED_APPNAME,
    "Navigator.appName is correctly spoofed."
  );
  is(
    result.appVersion,
    SPOOFED_APPVERSION[AppConstants.platform],
    "Navigator.appVersion is correctly spoofed."
  );
  is(
    result.platform,
    SPOOFED_PLATFORM[AppConstants.platform],
    "Navigator.platform is correctly spoofed."
  );
  is(
    result.userAgent,
    spoofedUserAgentNavigator,
    "Navigator.userAgent is correctly spoofed."
  );
  is(result.mimeTypesLength, 0, "Navigator.mimeTypes has a length of 0.");
  is(result.pluginsLength, 0, "Navigator.plugins has a length of 0.");
  is(
    result.oscpu,
    SPOOFED_OSCPU[AppConstants.platform],
    "Navigator.oscpu is correctly spoofed."
  );
  is(
    result.hardwareConcurrency,
    SPOOFED_HW_CONCURRENCY,
    "Navigator.hardwareConcurrency is correctly spoofed."
  );

  is(
    result.appCodeName,
    CONST_APPCODENAME,
    "Navigator.appCodeName reports correct constant value."
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

  is(
    result.appName,
    SPOOFED_APPNAME,
    "Navigator.appName is correctly spoofed."
  );
  is(
    result.appVersion,
    SPOOFED_APPVERSION[AppConstants.platform],
    "Navigator.appVersion is correctly spoofed."
  );
  is(
    result.platform,
    SPOOFED_PLATFORM[AppConstants.platform],
    "Navigator.platform is correctly spoofed."
  );
  is(
    result.userAgent,
    spoofedUserAgentNavigator,
    "Navigator.userAgent is correctly spoofed."
  );
  is(
    result.hardwareConcurrency,
    SPOOFED_HW_CONCURRENCY,
    "Navigator.hardwareConcurrency is correctly spoofed."
  );

  is(
    result.appCodeName,
    CONST_APPCODENAME,
    "Navigator.appCodeName reports correct constant value."
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

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  let appVersion = parseInt(Services.appinfo.version);
  let spoofedVersion;
  if (appVersion < 78) {
    // 68 is the last ESR version from the old six-week release cadence. After
    // 78 we can assume the four-week new release cadence.
    spoofedVersion = 68;
  } else {
    spoofedVersion = appVersion - ((appVersion - 78) % 13);
  }

  spoofedUserAgentNavigator = `Mozilla/5.0 (${
    SPOOFED_UA_NAVIGATOR_OS[AppConstants.platform]
  }; rv:${spoofedVersion}.0) Gecko/20100101 Firefox/${spoofedVersion}.0`;
  spoofedUserAgentHeader = `Mozilla/5.0 (${
    SPOOFED_UA_HTTPHEADER_OS[AppConstants.platform]
  }; rv:${spoofedVersion}.0) Gecko/20100101 Firefox/${spoofedVersion}.0`;
});

add_task(async function runNavigatorTest() {
  await testNavigator();
});

add_task(async function runHTTPHeaderTest() {
  await testUserAgentHeader();
});

add_task(async function runWorkerNavigatorTest() {
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
