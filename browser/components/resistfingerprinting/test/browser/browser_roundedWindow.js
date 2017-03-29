/*
 * Bug 1330882 - A test case for opening new windows as rounded size when
 *   fingerprinting resistance is enabled.
 */

const { classes: Cc, Constructor: CC, interfaces: Ci, utils: Cu } = Components;

const TEST_DOMAIN = "http://example.net/";
const TEST_PATH = TEST_DOMAIN + "browser/browser/components/resistFingerprinting/test/browser/";

let desiredWidth;
let desiredHeight;

add_task(function* setup() {
  yield SpecialPowers.pushPrefEnv({"set":
    [["privacy.resistFingerprinting", true]]
  });
  // Calculate the desire window size which is depending on the available screen
  // space.
  let chromeUIWidth = window.outerWidth - window.innerWidth;
  let chromeUIHeight = window.outerHeight - window.innerHeight;

  let availWidth = window.screen.availWidth;
  let availHeight = window.screen.availHeight;

  // Ideally, we would round the window size as 1000x1000.
  let availContentWidth = Math.min(1000, availWidth - chromeUIWidth);
  let availContentHeight = Math.min(1000, 0.95 * availHeight - chromeUIHeight);

  // Rounded the desire size to the nearest 200x100.
  desiredWidth = availContentWidth - (availContentWidth % 200);
  desiredHeight = availContentHeight - (availContentHeight % 100);
});

add_task(function* () {
  // Open a tab to test window.open().
  let tab = yield BrowserTestUtils.openNewForegroundTab(
    gBrowser, TEST_PATH + "file_dummy.html");

  yield ContentTask.spawn(tab.linkedBrowser, {desiredWidth, desiredHeight},
    function* (obj) {
      // Create a new window with the size which is not rounded.
      let win = content.open("http://example.net/", "", "width=1030,height=1025");

      win.onresize = () => {
        is(win.screen.width, obj.desiredWidth,
          "The screen.width has a correct rounded value");
        is(win.screen.height, obj.desiredHeight,
          "The screen.height has a correct rounded value");
        is(win.innerWidth, obj.desiredWidth,
          "The window.innerWidth has a correct rounded value");
        is(win.innerHeight, obj.desiredHeight,
          "The window.innerHeight has a correct rounded value");
      };

      win.onload = () => win.close();
    }
  );

  yield BrowserTestUtils.removeTab(tab);


  // Open a new window.
  let win = yield BrowserTestUtils.openNewBrowserWindow();

  // Load a page and verify its window size.
  tab = yield BrowserTestUtils.openNewForegroundTab(
    win.gBrowser, TEST_PATH + "file_dummy.html");

  yield ContentTask.spawn(tab.linkedBrowser, {desiredWidth, desiredHeight},
    function* (obj) {
      is(content.screen.width, obj.desiredWidth,
        "The screen.width has a correct rounded value");
      is(content.screen.height, obj.desiredHeight,
        "The screen.height has a correct rounded value");
      is(content.innerWidth, obj.desiredWidth,
        "The window.innerWidth has a correct rounded value");
      is(content.innerHeight, obj.desiredHeight,
        "The window.innerHeight has a correct rounded value");
    }
  );

  yield BrowserTestUtils.removeTab(tab);
  yield BrowserTestUtils.closeWindow(win);
});
