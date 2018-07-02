/**
 * Bug 1333651 - A test case for making sure the navigator object has been
 *   spoofed/disabled correctly.
 */

const CC = Components.Constructor;

ChromeUtils.defineModuleGetter(this, "AppConstants",
  "resource://gre/modules/AppConstants.jsm");

const TEST_PATH = "http://example.net/browser/browser/" +
                  "components/resistfingerprinting/test/browser/";

var spoofedUserAgent;

const SPOOFED_APPNAME = "Netscape";

const SPOOFED_APPVERSION = {
  linux: "5.0 (X11)",
  win: "5.0 (Windows)",
  macosx: "5.0 (Macintosh)",
  android: "5.0 (Android 6.0)",
  other: "5.0 (X11)",
};
const SPOOFED_PLATFORM = {
  linux: "Linux x86_64",
  win: "Win32",
  macosx: "MacIntel",
  android: "Linux armv7l",
  other: "Linux x86_64",
};
const SPOOFED_OSCPU = {
  linux: "Linux x86_64",
  win: "Windows NT 6.1; Win64; x64",
  macosx: "Intel Mac OS X 10.13",
  android: "Linux armv7l",
  other: "Linux x86_64",
};
const SPOOFED_UA_OS = {
  linux: "X11; Linux x86_64",
  win: "Windows NT 6.1; Win64; x64",
  macosx: "Macintosh; Intel Mac OS X 10.13",
  android: "Android 6.0; Mobile",
  other: "X11; Linux x86_64",
};
const SPOOFED_BUILDID        = "20100101";
const SPOOFED_HW_CONCURRENCY = 2;

const CONST_APPCODENAME = "Mozilla";
const CONST_PRODUCT     = "Gecko";
const CONST_PRODUCTSUB  = "20100101";
const CONST_VENDOR      = "";
const CONST_VENDORSUB   = "";

async function testNavigator() {
  // Open a tab to collect result.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_navigator.html");

  let result = await ContentTask.spawn(tab.linkedBrowser, null, function() {
    return content.document.getElementById("result").innerHTML;
  });

  result = JSON.parse(result);

  is(result.appName, SPOOFED_APPNAME, "Navigator.appName is correctly spoofed.");
  is(result.appVersion, SPOOFED_APPVERSION[AppConstants.platform], "Navigator.appVersion is correctly spoofed.");
  is(result.platform, SPOOFED_PLATFORM[AppConstants.platform], "Navigator.platform is correctly spoofed.");
  is(result.userAgent, spoofedUserAgent, "Navigator.userAgent is correctly spoofed.");
  is(result.mimeTypesLength, 0, "Navigator.mimeTypes has a length of 0.");
  is(result.pluginsLength, 0, "Navigator.plugins has a length of 0.");
  is(result.oscpu, SPOOFED_OSCPU[AppConstants.platform], "Navigator.oscpu is correctly spoofed.");
  is(result.buildID, SPOOFED_BUILDID, "Navigator.buildID is correctly spoofed.");
  is(result.hardwareConcurrency, SPOOFED_HW_CONCURRENCY, "Navigator.hardwareConcurrency is correctly spoofed.");

  is(result.appCodeName, CONST_APPCODENAME, "Navigator.appCodeName reports correct constant value.");
  is(result.product, CONST_PRODUCT, "Navigator.product reports correct constant value.");
  is(result.productSub, CONST_PRODUCTSUB, "Navigator.productSub reports correct constant value.");
  is(result.vendor, CONST_VENDOR, "Navigator.vendor reports correct constant value.");
  is(result.vendorSub, CONST_VENDORSUB, "Navigator.vendorSub reports correct constant value.");

  BrowserTestUtils.removeTab(tab);
}

async function testWorkerNavigator() {
  // Open a tab to collect result from worker.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  let result = await ContentTask.spawn(tab.linkedBrowser, null, async function() {
    let worker = new content.SharedWorker("file_navigatorWorker.js", "WorkerNavigatorTest");

    let res = await new Promise(resolve => {
      worker.port.onmessage = function(e) {
        resolve(e.data);
      };
    });

    return res;
  });

  result = JSON.parse(result);

  is(result.appName, SPOOFED_APPNAME, "Navigator.appName is correctly spoofed.");
  is(result.appVersion, SPOOFED_APPVERSION[AppConstants.platform], "Navigator.appVersion is correctly spoofed.");
  is(result.platform, SPOOFED_PLATFORM[AppConstants.platform], "Navigator.platform is correctly spoofed.");
  is(result.userAgent, spoofedUserAgent, "Navigator.userAgent is correctly spoofed.");
  is(result.hardwareConcurrency, SPOOFED_HW_CONCURRENCY, "Navigator.hardwareConcurrency is correctly spoofed.");

  is(result.appCodeName, CONST_APPCODENAME, "Navigator.appCodeName reports correct constant value.");
  is(result.product, CONST_PRODUCT, "Navigator.product reports correct constant value.");

  BrowserTestUtils.removeTab(tab);
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true]]
  });

  let appVersion = parseInt(Services.appinfo.version);
  let spoofedVersion = appVersion - ((appVersion - 4) % 7);
  spoofedUserAgent = `Mozilla/5.0 (${SPOOFED_UA_OS[AppConstants.platform]}; rv:${spoofedVersion}.0) Gecko/20100101 Firefox/${spoofedVersion}.0`;
});

add_task(async function runNavigatorTest() {
  await testNavigator();
});

add_task(async function runWorkerNavigatorTest() {
  await testWorkerNavigator();
});

// This tests that 'general.*.override' should not override spoofed values.
add_task(async function runOverrideTest() {
  await SpecialPowers.pushPrefEnv({"set":
    [
      ["general.appname.override", "appName overridden"],
      ["general.appversion.override", "appVersion overridden"],
      ["general.platform.override", "platform overridden"],
      ["general.useragent.override", "userAgent overridden"],
      ["general.oscpu.override", "oscpu overridden"],
      ["general.buildID.override", "buildID overridden"],
    ]
  });

  await testNavigator();

  await testWorkerNavigator();
});
