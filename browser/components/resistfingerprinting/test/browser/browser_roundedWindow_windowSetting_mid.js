/*
 * Bug 1330882 - A test case for setting window size through window.innerWidth/Height
 *   and window.outerWidth/Height when fingerprinting resistance is enabled. This
 *   test is for middle values.
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

const TEST_DOMAIN = "http://example.net/";
const TEST_PATH = TEST_DOMAIN + "browser/browser/components/resistfingerprinting/test/browser/";

let gMaxAvailWidth;
let gMaxAvailHeight;

// We need the chrome UI size of popup windows for testing outerWidth/Height.
let gPopupChromeUIWidth;
let gPopupChromeUIHeight;

const TESTCASES = [
  { settingWidth: 600, settingHeight: 600, targetWidth: 600, targetHeight: 600,
    initWidth: 200, initHeight: 100  },
  { settingWidth: 599, settingHeight: 599, targetWidth: 600, targetHeight: 600,
    initWidth: 200, initHeight: 100  },
  { settingWidth: 401, settingHeight: 501, targetWidth: 600, targetHeight: 600,
    initWidth: 200, initHeight: 100  },
];

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true]]
  });

  // Calculate the popup window's chrome UI size for tests of outerWidth/Height.
  let popUpChromeUISize = await calcPopUpWindowChromeUISize();

  gPopupChromeUIWidth = popUpChromeUISize.chromeWidth;
  gPopupChromeUIHeight = popUpChromeUISize.chromeHeight;

  // Calculate the maximum available size.
  let maxAvailSize = await calcMaximumAvailSize(gPopupChromeUIWidth,
                                                gPopupChromeUIHeight);

  gMaxAvailWidth = maxAvailSize.maxAvailWidth;
  gMaxAvailHeight = maxAvailSize.maxAvailHeight;
});

add_task(async function test_window_size_setting() {
  // Open a tab to test.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  for (let test of TESTCASES) {
    // Test window.innerWidth and window.innerHeight.
    await testWindowSizeSetting(tab.linkedBrowser, test.settingWidth, test.settingHeight,
                                test.targetWidth, test.targetHeight, test.initWidth,
                                test.initHeight, false, gMaxAvailWidth, gMaxAvailHeight,
                                gPopupChromeUIWidth, gPopupChromeUIHeight);

    // test window.outerWidth and window.outerHeight.
    await testWindowSizeSetting(tab.linkedBrowser, test.settingWidth, test.settingHeight,
                                test.targetWidth, test.targetHeight, test.initWidth,
                                test.initHeight, true, gMaxAvailWidth, gMaxAvailHeight,
                                gPopupChromeUIWidth, gPopupChromeUIHeight);
  }

  await BrowserTestUtils.removeTab(tab);
});
