/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { SiteSpecificBrowser } = ChromeUtils.import(
  "resource:///modules/SiteSpecificBrowserService.jsm"
);

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

// A different secure site to use.
const gHttpsOtherRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  "https://example.org/"
);

// The chrome url for the SSB UI.
const SSB_WINDOW = "chrome://browser/content/ssb/ssb.html";

// Waits for an SSB window to open.
async function waitForSSB() {
  let ssbwin = await BrowserTestUtils.domWindowOpened(null, async domwin => {
    await BrowserTestUtils.waitForEvent(domwin, "load");
    return domwin.location.toString() == SSB_WINDOW;
  });

  await BrowserTestUtils.waitForEvent(getBrowser(ssbwin), "SSBLoad");
  return ssbwin;
}

// Directly opens an SSB for the given URI. Resolves to the SSB DOM window after
// the SSB content has loaded.
async function openSSB(uri) {
  if (!(uri instanceof Ci.nsIURI)) {
    uri = Services.io.newURI(uri);
  }

  let openPromise = waitForSSB();
  let ssb = SiteSpecificBrowser.createFromURI(uri);
  ssb.launch();
  return openPromise;
}

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

  let openPromise = waitForSSB();
  EventUtils.synthesizeMouseAtCenter(openItem, {}, win);
  return openPromise;
}

// Given the SSB UI DOM window gets the browser element showing the content.
function getBrowser(ssbwin) {
  return ssbwin.document.getElementById("browser");
}

// Given the SSB UI DOM window gets the ssb instance showing the content.
function getSSB(ssbwin) {
  return ssbwin.gSSB;
}

/**
 * Waits for a load in response to an attempt to navigate from the SSB. It
 * listens for new tab opens in the main window, new window opens and loads in
 * the SSB itself. It returns a promise.
 *
 * The `where` argument is a string saying where the load is expected to
 * happen, "ssb", "tab" or "window". The promise rejects if the load happens
 * somewhere else and the offending new item (tab or window) get closed. When
 * the load is seen in the correct location different things are returned
 * depending on `where`. For "tab" the new tab is returned (it will have
 * finished loading), for "window" the new window is returned (it will have
 * finished loading). The "ssb" case doesn't return anything.
 *
 * Generally use the methods below this as they look more obvious.
 */
function expectLoadSomewhere(ssb, where, win = window) {
  return new Promise((resolve, reject) => {
    // Listens for a new tab opening in the main window.
    const tabListener = async ({ target: tab }) => {
      cleanup();

      await BrowserTestUtils.browserLoaded(
        tab.linkedBrowser,
        true,
        uri => uri != "about:blank"
      );

      if (where != "tab") {
        Assert.ok(
          false,
          `Did not expect ${
            tab.linkedBrowser.currentURI.spec
          } to load in a new tab.`
        );
        BrowserTestUtils.removeTab(tab);
        reject(new Error("Page unexpectedly loaded in a new tab."));
        return;
      }
      Assert.ok(
        true,
        `${tab.linkedBrowser.currentURI.spec} loaded in a new tab as expected.`
      );
      resolve(tab);
    };
    win.gBrowser.tabContainer.addEventListener("TabOpen", tabListener);

    // Listens for new top-level windows.
    const winObserver = async (domwin, topic) => {
      if (topic != "domwindowopened") {
        return;
      }

      cleanup();

      await BrowserTestUtils.waitForEvent(
        domwin,
        "load",
        uri => uri != "about:blank"
      );

      if (where != "window") {
        Assert.ok(false, `Did not expect a new ${domwin.location} to open.`);
        await BrowserTestUtils.closeWindow(domwin);
        reject(new Error("New window unexpectedly opened."));
        return;
      }
      Assert.ok(true, `${domwin.location} opened as expected.`);
      resolve(domwin);
    };
    Services.ww.registerNotification(winObserver);

    const ssbListener = () => {
      cleanup();

      if (where != "ssb") {
        Assert.ok(
          false,
          `Did not expect ${
            getBrowser(ssb).currentURI.spec
          } to load in the ssb window.`
        );
        reject(new Error("Page unexpectedly loaded in the ssb window."));
        return;
      }
      Assert.ok(
        true,
        `${
          getBrowser(ssb).currentURI.spec
        } loaded in the ssb window as expected.`
      );
      resolve();
    };
    getBrowser(ssb).addEventListener("SSBLoad", ssbListener);

    // Makes sure that no notifications fire after the test is done.
    const cleanup = () => {
      win.gBrowser.tabContainer.removeEventListener("TabOpen", tabListener);
      Services.ww.unregisterNotification(winObserver);
      getBrowser(ssb).removeEventListener("SSBLoad", ssbListener);
    };
  });
}

/**
 * Waits for a load to occur in the ssb window but rejects if a new tab or
 * window get opened.
 */
function expectSSBLoad(ssb, win = window) {
  return expectLoadSomewhere(ssb, "ssb", win);
}

/**
 * Waits for a new tab to be opened and loaded. Rejects if a new window is
 * opened or the ssb loads something before. Resolves with the new loaded tab.
 */
function expectTabLoad(ssb, win = window) {
  return expectLoadSomewhere(ssb, "tab", win);
}

/**
 * Waits for a new window to be opened and loaded. Rejects if a new tab is
 * opened or the ssb loads something before. Resolves with the new loaded
 * window.
 */
function expectWindowOpen(ssb, win = window) {
  return expectLoadSomewhere(ssb, "window", win);
}

add_task(async () => {
  let list = await SiteSpecificBrowserService.list();
  Assert.equal(
    list.length,
    0,
    "Should be no installed SSBs at the start of a test."
  );
});

registerCleanupFunction(async () => {
  let list = await SiteSpecificBrowserService.list();
  Assert.equal(
    list.length,
    0,
    "Should be no installed SSBs at the end of a test."
  );
});
