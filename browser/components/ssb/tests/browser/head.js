/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// An insecure site to use. SSBs cannot be insecure.
const gHttpTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "http://example.com/"
);

// A secure site to use.
const gHttpsTestRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.com/"
);

// The chrome url for the SSB UI.
const SSB_WINDOW = "chrome://browser/content/ssb/ssb.html";

// Simulates opening a SSB from the main browser window. Resolves to the SSB
// DOM window after the SSB content has loaded.
async function openSSBFromBrowserWindow(win = window) {
  let doc = win.document;
  let pageActionButton = doc.getElementById("pageActionButton");
  let panel = doc.getElementById("pageActionPanel");
  let popupShown = BrowserTestUtils.waitForEvent(panel, "popupshown");

  EventUtils.synthesizeMouseAtCenter(pageActionButton, {}, win);
  await popupShown;

  let openItem = doc.getElementById("pageAction-panel-launchSSB");
  Assert.ok(!openItem.disabled, "Open menu item should not be disabled");
  Assert.ok(!openItem.hidden, "Open menu item should not be hidden");

  let openPromise = BrowserTestUtils.domWindowOpened(null, async domwin => {
    await BrowserTestUtils.waitForEvent(domwin, "load");
    return domwin.location.toString() == SSB_WINDOW;
  });

  EventUtils.synthesizeMouseAtCenter(openItem, {}, win);
  let ssbwin = await openPromise;
  let browser = ssbwin.document.getElementById("browser");
  await BrowserTestUtils.browserLoaded(browser, true);
  return ssbwin;
}

// Given the SSB UI DOM window gets the browser element showing the content.
function getBrowser(ssbwin) {
  return ssbwin.document.getElementById("browser");
}
