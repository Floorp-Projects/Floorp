/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Helper function for encoding override tests, loads URL, runs check1,
 * forces encoding detection, runs check2.
 */
function runCharsetTest(url, check1, check2) {
  waitForExplicitFinish();

  BrowserTestUtils.openNewForegroundTab(gBrowser, url, true).then(afterOpen);

  function afterOpen() {
    BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(
      afterChangeCharset
    );

    SpecialPowers.spawn(gBrowser.selectedBrowser, [], check1).then(() => {
      BrowserForceEncodingDetection();
    });
  }

  function afterChangeCharset() {
    SpecialPowers.spawn(gBrowser.selectedBrowser, [], check2).then(() => {
      gBrowser.removeCurrentTab();
      finish();
    });
  }
}

/**
 * Helper function for charset tests. It loads |url| in a new tab,
 * runs |check|.
 */
function runCharsetCheck(url, check) {
  waitForExplicitFinish();

  BrowserTestUtils.openNewForegroundTab(gBrowser, url, true).then(afterOpen);

  function afterOpen() {
    SpecialPowers.spawn(gBrowser.selectedBrowser, [], check).then(() => {
      gBrowser.removeCurrentTab();
      finish();
    });
  }
}

async function pushState(url, frameId) {
  info(
    `Doing a pushState, expecting to load ${url} ${
      frameId ? "in an iframe" : ""
    }`
  );
  let browser = gBrowser.selectedBrowser;
  let bc = browser.browsingContext;
  if (frameId) {
    bc = await SpecialPowers.spawn(bc, [frameId], function (id) {
      return content.document.getElementById(id).browsingContext;
    });
  }
  let loaded = BrowserTestUtils.waitForLocationChange(gBrowser, url);
  await SpecialPowers.spawn(bc, [url], function (url) {
    content.history.pushState({}, "", url);
  });
  await loaded;
  info(`Loaded ${url} ${frameId ? "in an iframe" : ""}`);
}

async function loadURI(url) {
  info(`Doing a top-level loadURI, expecting to load ${url}`);
  let browser = gBrowser.selectedBrowser;
  let loaded = BrowserTestUtils.browserLoaded(browser, false, url);
  BrowserTestUtils.startLoadingURIString(browser, url);
  await loaded;
  info(`Loaded ${url}`);
}

async function followLink(url, frameId) {
  info(
    `Creating and following a link to ${url} ${frameId ? "in an iframe" : ""}`
  );
  let browser = gBrowser.selectedBrowser;
  let bc = browser.browsingContext;
  if (frameId) {
    bc = await SpecialPowers.spawn(bc, [frameId], function (id) {
      return content.document.getElementById(id).browsingContext;
    });
  }
  let loaded = BrowserTestUtils.browserLoaded(browser, !!frameId, url);
  await SpecialPowers.spawn(bc, [url], function (url) {
    let a = content.document.createElement("a");
    a.href = url;
    content.document.body.appendChild(a);
    a.click();
  });
  await loaded;
  info(`Loaded ${url} ${frameId ? "in an iframe" : ""}`);
}

async function goForward(url, useFrame = false) {
  info(
    `Clicking the forward button, expecting to load ${url} ${
      useFrame ? "in an iframe" : ""
    }`
  );
  let loaded = BrowserTestUtils.waitForLocationChange(gBrowser, url);
  let forwardButton = document.getElementById("forward-button");
  forwardButton.click();
  await loaded;
  info(`Loaded ${url} ${useFrame ? "in an iframe" : ""}`);
}

async function goBack(url, useFrame = false) {
  info(
    `Clicking the back button, expecting to load ${url} ${
      useFrame ? "in an iframe" : ""
    }`
  );
  let loaded = BrowserTestUtils.waitForLocationChange(gBrowser, url);
  let backButton = document.getElementById("back-button");
  backButton.click();
  await loaded;
  info(`Loaded ${url} ${useFrame ? "in an iframe" : ""}`);
}

function assertBackForwardState(canGoBack, canGoForward) {
  let backButton = document.getElementById("back-button");
  let forwardButton = document.getElementById("forward-button");

  is(
    backButton.disabled,
    !canGoBack,
    `${gBrowser.currentURI.spec}: back button is ${
      canGoBack ? "not" : ""
    } disabled`
  );
  is(
    forwardButton.disabled,
    !canGoForward,
    `${gBrowser.currentURI.spec}: forward button is ${
      canGoForward ? "not" : ""
    } disabled`
  );
}

class SHListener {
  static NewEntry = 0;
  static Reload = 1;
  static GotoIndex = 2;
  static Purge = 3;
  static ReplaceEntry = 4;
  static async waitForHistory(history, event) {
    return new Promise(resolve => {
      let listener = {
        OnHistoryNewEntry: () => {},
        OnHistoryReload: () => {
          return true;
        },
        OnHistoryGotoIndex: () => {},
        OnHistoryPurge: () => {},
        OnHistoryReplaceEntry: () => {},

        QueryInterface: ChromeUtils.generateQI([
          "nsISHistoryListener",
          "nsISupportsWeakReference",
        ]),
      };

      function finish() {
        history.removeSHistoryListener(listener);
        resolve();
      }
      switch (event) {
        case this.NewEntry:
          listener.OnHistoryNewEntry = finish;
          break;
        case this.Reload:
          listener.OnHistoryReload = () => {
            finish();
            return true;
          };
          break;
        case this.GotoIndex:
          listener.OnHistoryGotoIndex = finish;
          break;
        case this.Purge:
          listener.OnHistoryPurge = finish;
          break;
        case this.ReplaceEntry:
          listener.OnHistoryReplaceEntry = finish;
          break;
      }

      history.addSHistoryListener(listener);
    });
  }
}
