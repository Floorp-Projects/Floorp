/*
 * This test checks that window activation state is set properly with multiple tabs.
 */

/* eslint-env mozilla/frame-script */

const testPage = getRootDirectory(gTestPath) + "file_window_activation.html";
const testPage2 = getRootDirectory(gTestPath) + "file_window_activation2.html";

add_task(async function reallyRunTests() {
  let tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, testPage);
  let browser1 = tab1.linkedBrowser;

  // This can't use openNewForegroundTab because if we focus tab2 now, we
  // won't send a focus event during test 6, further down in this file.
  let tab2 = BrowserTestUtils.addTab(gBrowser, testPage);
  let browser2 = tab2.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser2);

  function failTest() {
    ok(false, "Test received unexpected activate/deactivate event");
  }

  // The content should not receive activate or deactivate events. If it
  // does, fail the test.
  BrowserTestUtils.waitForContentEvent(browser1, "activate", true).then(
    failTest
  );
  BrowserTestUtils.waitForContentEvent(browser2, "activate", true).then(
    failTest
  );
  BrowserTestUtils.waitForContentEvent(browser1, "deactivate", true).then(
    failTest
  );
  BrowserTestUtils.waitForContentEvent(browser2, "deactivate", true).then(
    failTest
  );

  gURLBar.focus();

  gBrowser.selectedTab = tab1;

  // The test performs four checks, using -moz-window-inactive on two child tabs.
  // First, the initial state should be transparent. The second check is done
  // while another window is focused. The third check is done after that window
  // is closed and the main window focused again. The fourth check is done after
  // switching to the second tab.

  // Step 1 - check the initial state
  let colorBrowser1 = await getBackgroundColor(browser1, true);
  let colorBrowser2 = await getBackgroundColor(browser2, true);
  is(colorBrowser1, "rgba(0, 0, 0, 0)", "first window initial");
  is(colorBrowser2, "rgba(0, 0, 0, 0)", "second window initial");

  // Step 2 - open and focus another window
  let otherWindow = window.open(testPage2, "", "chrome");
  await SimpleTest.promiseFocus(otherWindow);
  colorBrowser1 = await getBackgroundColor(browser1, true);
  colorBrowser2 = await getBackgroundColor(browser2, true);
  is(colorBrowser1, "rgb(255, 0, 0)", "first window lowered");
  is(colorBrowser2, "rgb(255, 0, 0)", "second window lowered");

  // Step 3 - close the other window again
  otherWindow.close();
  colorBrowser1 = await getBackgroundColor(browser1, true);
  colorBrowser2 = await getBackgroundColor(browser2, true);
  is(colorBrowser1, "rgba(0, 0, 0, 0)", "first window raised");
  is(colorBrowser2, "rgba(0, 0, 0, 0)", "second window raised");

  // Step 4 - switch to the second tab
  gBrowser.selectedTab = tab2;
  colorBrowser1 = await getBackgroundColor(browser1, false);
  colorBrowser2 = await getBackgroundColor(browser2, false);
  is(colorBrowser1, "rgba(0, 0, 0, 0)", "first window after tab switch");
  is(colorBrowser2, "rgba(0, 0, 0, 0)", "second window after tab switch");

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
  otherWindow = null;
});

function getBackgroundColor(browser, ifChanged) {
  return SpecialPowers.spawn(browser, [], () => {
    return new Promise(resolve => {
      let oldColor = null;
      let timer = content.setInterval(() => {
        let area = content.document.getElementById("area");
        if (!area) {
          return; /* hasn't loaded yet */
        }

        let color = content.getComputedStyle(area).backgroundColor;
        if (oldColor != color || !ifChanged) {
          content.clearInterval(timer);
          oldColor = color;
          resolve(color);
        }
      }, 20);
    });
  });
}
