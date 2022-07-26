/*
 * This test checks that window activation state is set properly with multiple tabs.
 */

const testPageChrome =
  getRootDirectory(gTestPath) + "file_window_activation.html";
const testPageHttp = testPageChrome.replace(
  "chrome://mochitests/content",
  "http://example.com"
);
const testPageWindow =
  getRootDirectory(gTestPath) + "file_window_activation2.html";

add_task(async function reallyRunTests() {
  let chromeTab1 = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    testPageChrome
  );
  let chromeBrowser1 = chromeTab1.linkedBrowser;

  // This can't use openNewForegroundTab because if we focus chromeTab2 now, we
  // won't send a focus event during test 6, further down in this file.
  let chromeTab2 = BrowserTestUtils.addTab(gBrowser, testPageChrome);
  let chromeBrowser2 = chromeTab2.linkedBrowser;
  await BrowserTestUtils.browserLoaded(chromeBrowser2);

  let httpTab = BrowserTestUtils.addTab(gBrowser, testPageHttp);
  let httpBrowser = httpTab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(httpBrowser);

  function failTest() {
    ok(false, "Test received unexpected activate/deactivate event");
  }

  // chrome:// url tabs should not receive "activate" or "deactivate" events
  // as they should be sent to the top-level window in the parent process.
  for (let b of [chromeBrowser1, chromeBrowser2]) {
    BrowserTestUtils.waitForContentEvent(b, "activate", true).then(failTest);
    BrowserTestUtils.waitForContentEvent(b, "deactivate", true).then(failTest);
  }

  gURLBar.focus();

  gBrowser.selectedTab = chromeTab1;

  // The test performs four checks, using -moz-window-inactive on three child
  // tabs (2 loading chrome:// urls and one loading an http:// url).
  // First, the initial state should be transparent. The second check is done
  // while another window is focused. The third check is done after that window
  // is closed and the main window focused again. The fourth check is done after
  // switching to the second tab.

  // Step 1 - check the initial state
  let colorChromeBrowser1 = await getBackgroundColor(chromeBrowser1, true);
  let colorChromeBrowser2 = await getBackgroundColor(chromeBrowser2, true);
  let colorHttpBrowser = await getBackgroundColor(httpBrowser, true);
  is(colorChromeBrowser1, "rgba(0, 0, 0, 0)", "first tab initial");
  is(colorChromeBrowser2, "rgba(0, 0, 0, 0)", "second tab initial");
  is(colorHttpBrowser, "rgba(0, 0, 0, 0)", "third tab initial");

  // Step 2 - open and focus another window
  let otherWindow = window.open(testPageWindow, "", "chrome");
  await SimpleTest.promiseFocus(otherWindow);
  colorChromeBrowser1 = await getBackgroundColor(chromeBrowser1, false);
  colorChromeBrowser2 = await getBackgroundColor(chromeBrowser2, false);
  colorHttpBrowser = await getBackgroundColor(httpBrowser, false);
  is(colorChromeBrowser1, "rgb(255, 0, 0)", "first tab lowered");
  is(colorChromeBrowser2, "rgb(255, 0, 0)", "second tab lowered");
  is(colorHttpBrowser, "rgb(255, 0, 0)", "third tab lowered");

  // Step 3 - close the other window again
  otherWindow.close();
  colorChromeBrowser1 = await getBackgroundColor(chromeBrowser1, true);
  colorChromeBrowser2 = await getBackgroundColor(chromeBrowser2, true);
  colorHttpBrowser = await getBackgroundColor(httpBrowser, true);
  is(colorChromeBrowser1, "rgba(0, 0, 0, 0)", "first tab raised");
  is(colorChromeBrowser2, "rgba(0, 0, 0, 0)", "second tab raised");
  is(colorHttpBrowser, "rgba(0, 0, 0, 0)", "third tab raised");

  // Step 4 - switch to the second tab
  gBrowser.selectedTab = chromeTab2;
  colorChromeBrowser1 = await getBackgroundColor(chromeBrowser1, true);
  colorChromeBrowser2 = await getBackgroundColor(chromeBrowser2, true);
  colorHttpBrowser = await getBackgroundColor(httpBrowser, true);
  is(colorChromeBrowser1, "rgba(0, 0, 0, 0)", "first tab after tab switch");
  is(colorChromeBrowser2, "rgba(0, 0, 0, 0)", "second tab after tab switch");
  is(colorHttpBrowser, "rgba(0, 0, 0, 0)", "third tab after tab switch");

  BrowserTestUtils.removeTab(chromeTab1);
  BrowserTestUtils.removeTab(chromeTab2);
  BrowserTestUtils.removeTab(httpTab);
  otherWindow = null;
});

function getBackgroundColor(browser, expectedActive) {
  return SpecialPowers.spawn(
    browser,
    [!expectedActive],
    async hasPseudoClass => {
      let area = content.document.getElementById("area");
      await ContentTaskUtils.waitForCondition(() => {
        return area;
      }, "Page has loaded");
      await ContentTaskUtils.waitForCondition(() => {
        return area.matches(":-moz-window-inactive") == hasPseudoClass;
      }, `Window is considered ${hasPseudoClass ? "inactive" : "active"}`);

      return content.getComputedStyle(area).backgroundColor;
    }
  );
}
