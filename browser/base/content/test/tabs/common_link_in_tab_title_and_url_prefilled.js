/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { sinon } = ChromeUtils.importESModule(
  "resource://testing-common/Sinon.sys.mjs"
);

const TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);
const HOME_URL = `${TEST_ROOT}link_in_tab_title_and_url_prefilled.html`;
const HOME_TITLE = HOME_URL.substring("https://".length);
const WAIT_A_BIT_URL = `${TEST_ROOT}wait-a-bit.sjs`;
const WAIT_A_BIT_LOADING_TITLE = WAIT_A_BIT_URL.substring("https://".length);
const WAIT_A_BIT_PAGE_TITLE = "wait a bit";
const REQUEST_TIMEOUT_URL = `${TEST_ROOT}request-timeout.sjs`;
const REQUEST_TIMEOUT_LOADING_TITLE = REQUEST_TIMEOUT_URL.substring(
  "https://".length
);
const BLANK_URL = "about:blank";
const BLANK_TITLE = "New Tab";

const OPEN_BY = {
  CLICK: "click",
  CONTEXT_MENU: "context_menu",
};

const OPEN_AS = {
  FOREGROUND: "foreground",
  BACKGROUND: "background",
};

async function doTestInSameWindow({
  link,
  openBy,
  openAs,
  loadingState,
  actionWhileLoading,
  finalState,
}) {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    // NOTE: The behavior after the click <a href="about:blank">link</a>
    // (no target) is different when the URL is opened directly with
    // BrowserTestUtils.withNewTab() and when it is loaded later.
    // Specifically, if we load `about:blank`, expect to see `New Tab` as the
    // title of the tab,  but the former will continue to display the URL that
    // was previously displayed. Therefore, use the latter way.
    BrowserTestUtils.startLoadingURIString(browser, HOME_URL);
    await BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      HOME_URL
    );

    info(`Open link for ${link} by ${openBy} as ${openAs}`);
    const onNewTabCreated = waitForNewTabWithLoadRequest();
    const href = await openLink(browser, link, openBy, openAs);

    info("Wait until starting to load in the target tab");
    const target = await onNewTabCreated;
    Assert.equal(target.selected, openAs === OPEN_AS.FOREGROUND);
    Assert.equal(gURLBar.value, loadingState.urlbar);
    Assert.equal(target.textLabel.textContent, loadingState.tab);

    await actionWhileLoading(
      BrowserTestUtils.browserLoaded(target.linkedBrowser, false, href)
    );

    info("Check the final result");
    Assert.equal(gURLBar.value, finalState.urlbar);
    Assert.equal(target.textLabel.textContent, finalState.tab);
    const sessionHistory = await new Promise(r =>
      SessionStore.getSessionHistory(target, r)
    );
    Assert.deepEqual(
      sessionHistory.entries.map(e => e.url),
      finalState.history
    );

    BrowserTestUtils.removeTab(target);
  });
}

async function doTestWithNewWindow({ link, expectedSetURICalled }) {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.link.open_newwindow", 2]],
  });

  await BrowserTestUtils.withNewTab(HOME_URL, async browser => {
    const onNewWindowOpened = BrowserTestUtils.domWindowOpenedAndLoaded();

    info(`Open link for ${link}`);
    const href = await openLink(
      browser,
      link,
      OPEN_BY.CLICK,
      OPEN_AS.FOREGROUND
    );

    info("Wait until opening a new window");
    const win = await onNewWindowOpened;

    info("Check whether gURLBar.setURI is called while loading the page");
    const sandbox = sinon.createSandbox();
    registerCleanupFunction(() => {
      sandbox.restore();
    });
    let isSetURIWhileLoading = false;
    sandbox.stub(win.gURLBar, "setURI").callsFake(uri => {
      if (
        !uri &&
        win.gBrowser.selectedBrowser.browsingContext.nonWebControlledBlankURI
      ) {
        isSetURIWhileLoading = true;
      }
    });
    await BrowserTestUtils.browserLoaded(
      win.gBrowser.selectedBrowser,
      false,
      href
    );
    sandbox.restore();

    Assert.equal(isSetURIWhileLoading, expectedSetURICalled);
    Assert.equal(
      !!win.gBrowser.selectedBrowser.browsingContext.nonWebControlledBlankURI,
      expectedSetURICalled
    );

    await BrowserTestUtils.closeWindow(win);
  });

  await SpecialPowers.popPrefEnv();
}

async function doSessionRestoreTest({
  link,
  openBy,
  openAs,
  expectedSessionHistory,
  expectedSessionRestored,
}) {
  await BrowserTestUtils.withNewTab("about:blank", async browser => {
    BrowserTestUtils.startLoadingURIString(browser, HOME_URL);
    await BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      false,
      HOME_URL
    );

    info(`Open link for ${link} by ${openBy} as ${openAs}`);
    const onNewTabCreated = waitForNewTabWithLoadRequest();
    const href = await openLink(browser, link, openBy, openAs);
    const target = await onNewTabCreated;
    await BrowserTestUtils.waitForCondition(
      () =>
        target.linkedBrowser.browsingContext
          .mostRecentLoadingSessionHistoryEntry
    );

    info("Close the session");
    const sessionPromise = BrowserTestUtils.waitForSessionStoreUpdate(target);
    BrowserTestUtils.removeTab(target);
    await sessionPromise;

    info("Restore the session");
    const restoredTab = SessionStore.undoCloseTab(window, 0);
    await BrowserTestUtils.browserLoaded(restoredTab.linkedBrowser);

    info("Check the loaded URL of restored tab");
    Assert.equal(
      restoredTab.linkedBrowser.currentURI.spec === href,
      expectedSessionRestored
    );

    if (expectedSessionRestored) {
      info("Check the session history of restored tab");
      const sessionHistory = await new Promise(r =>
        SessionStore.getSessionHistory(restoredTab, r)
      );
      Assert.deepEqual(
        sessionHistory.entries.map(e => e.url),
        expectedSessionHistory
      );
    }

    BrowserTestUtils.removeTab(restoredTab);
  });
}

async function openLink(browser, link, openBy, openAs) {
  let href;
  const openAsBackground = openAs === OPEN_AS.BACKGROUND;
  if (openBy === OPEN_BY.CLICK) {
    href = await synthesizeMouse(browser, link, {
      ctrlKey: openAsBackground,
      metaKey: openAsBackground,
    });
  } else if (openBy === OPEN_BY.CONTEXT_MENU) {
    await SpecialPowers.pushPrefEnv({
      set: [["browser.tabs.loadInBackground", openAsBackground]],
    });

    const contextMenu = document.getElementById("contentAreaContextMenu");
    const onPopupShown = BrowserTestUtils.waitForEvent(
      contextMenu,
      "popupshown"
    );

    href = await synthesizeMouse(browser, link, {
      type: "contextmenu",
      button: 2,
    });

    await onPopupShown;

    const openLinkMenuItem = contextMenu.querySelector(
      "#context-openlinkintab"
    );
    contextMenu.activateItem(openLinkMenuItem);

    await SpecialPowers.popPrefEnv();
  } else {
    throw new Error("Invalid openBy");
  }

  return href;
}

async function synthesizeMouse(browser, link, event) {
  return SpecialPowers.spawn(
    browser,
    [link, event],
    (linkInContent, eventInContent) => {
      const target = content.document.getElementById(linkInContent);
      EventUtils.synthesizeMouseAtCenter(target, eventInContent, content);
      return target.href;
    }
  );
}

async function waitForNewTabWithLoadRequest() {
  return new Promise(resolve =>
    gBrowser.addTabsProgressListener({
      onStateChange(aBrowser, aWebProgress, aRequest, aStateFlags, aStatus) {
        if (aStateFlags & Ci.nsIWebProgressListener.STATE_START) {
          gBrowser.removeTabsProgressListener(this);
          resolve(gBrowser.getTabForBrowser(aBrowser));
        }
      },
    })
  );
}
