/*
 * Bug 1330882 - A test case for opening new windows as rounded size when
 *   fingerprinting resistance is enabled.
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

const TEST_DOMAIN = "http://example.net/";
const TEST_PATH = TEST_DOMAIN + "browser/browser/components/resistfingerprinting/test/browser/";

let gMaxAvailWidth;
let gMaxAvailHeight;

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true]]
  });

  // Calculate the maximum available size.
  let maxAvailSize = await calcMaximumAvailSize();

  gMaxAvailWidth = maxAvailSize.maxAvailWidth;
  gMaxAvailHeight = maxAvailSize.maxAvailHeight;
});

add_task(async function test_new_window() {
  // Open a new window.
  let win = await BrowserTestUtils.openNewBrowserWindow();

  // Load a page and verify its window size.
  let tab = await BrowserTestUtils.openNewForegroundTab(
    win.gBrowser, TEST_PATH + "file_dummy.html");

  await ContentTask.spawn(tab.linkedBrowser, {gMaxAvailWidth, gMaxAvailHeight},
    async function(input) {
      is(content.screen.width, input.gMaxAvailWidth,
        "The screen.width has a correct rounded value");
      is(content.screen.height, input.gMaxAvailHeight,
        "The screen.height has a correct rounded value");
      is(content.innerWidth, input.gMaxAvailWidth,
        "The window.innerWidth has a correct rounded value");
      is(content.innerHeight, input.gMaxAvailHeight,
        "The window.innerHeight has a correct rounded value");
    }
  );

  await BrowserTestUtils.removeTab(tab);
  await BrowserTestUtils.closeWindow(win);
});
