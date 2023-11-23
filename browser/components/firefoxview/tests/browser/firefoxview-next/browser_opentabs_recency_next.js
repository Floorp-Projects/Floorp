/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

/*
   This test checks the recent-browsing view of open tabs in about:firefoxview next
   presents the correct tab data in the correct order.
*/

const tabURL1 = "data:,Tab1";
const tabURL2 = "data:,Tab2";
const tabURL3 = "data:,Tab3";
const tabURL4 = "data:,Tab4";

let gInitialTab;
let gInitialTabURL;

add_setup(function () {
  gInitialTab = gBrowser.selectedTab;
  gInitialTabURL = tabUrl(gInitialTab);
});

function tabUrl(tab) {
  return tab.linkedBrowser.currentURI?.spec;
}

async function minimizeWindow(win) {
  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    win,
    "sizemodechange"
  );
  win.minimize();
  await promiseSizeModeChange;
  ok(
    !win.gBrowser.selectedTab.linkedBrowser.docShellIsActive,
    "Docshell should be Inactive"
  );
  ok(win.document.hidden, "Top level window should be hidden");
}

async function restoreWindow(win) {
  ok(win.document.hidden, "Top level window should be hidden");
  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    win,
    "sizemodechange"
  );
  info("Calling window.restore");
  win.restore();
  // From browser/base/content/test/general/browser_minimize.js:
  // On Ubuntu `window.restore` doesn't seem to work, use a timer to make the
  // test fail faster and more cleanly than with a test timeout.
  info("Waiting for sizemodechange event");
  let timer;
  await Promise.race([
    promiseSizeModeChange,
    new Promise((resolve, reject) => {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      timer = setTimeout(() => {
        reject("timed out waiting for sizemodechange event");
      }, 5000);
    }),
  ]);
  clearTimeout(timer);
  info(
    "Waiting occlusionstatechange if win.isFullyOccluded: " +
      win.isFullyOccluded
  );
  // From browser/base/content/test/general/browser_minimize.js:
  // The sizemodechange event can sometimes be fired before the
  // occlusionstatechange event, especially in chaos mode.
  if (win.isFullyOccluded) {
    await BrowserTestUtils.waitForEvent(win, "occlusionstatechange");
  }
  ok(
    win.gBrowser.selectedTab.linkedBrowser.docShellIsActive,
    "Docshell should be active again"
  );
  ok(!win.document.hidden, "Top level window should be visible");
}

async function prepareOpenTabs(urls, win = window) {
  const reusableTabURLs = ["about:newtab", "about:blank"];
  const gBrowser = win.gBrowser;

  for (let url of urls) {
    if (
      gBrowser.visibleTabs.length == 1 &&
      reusableTabURLs.includes(gBrowser.selectedBrowser.currentURI.spec)
    ) {
      // we'll load into this tab rather than opening a new one
      info(
        `Loading ${url} into blank tab: ${gBrowser.selectedBrowser.currentURI.spec}`
      );
      BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url);
      await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, null, url);
    } else {
      info(`Loading ${url} into new tab`);
      await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
    }
    await new Promise(res => win.requestAnimationFrame(res));
  }
  Assert.equal(
    gBrowser.visibleTabs.length,
    urls.length,
    `Prepared ${urls.length} tabs as expected`
  );
  Assert.equal(
    tabUrl(gBrowser.selectedTab),
    urls[urls.length - 1],
    "The selectedTab is the last of the URLs given as expected"
  );
}

async function cleanup(...windowsToClose) {
  await Promise.all(
    windowsToClose.map(win => BrowserTestUtils.closeWindow(win))
  );

  while (gBrowser.visibleTabs.length > 1) {
    await SessionStoreTestUtils.closeTab(gBrowser.tabs.at(-1));
  }
  if (gBrowser.selectedBrowser.currentURI.spec !== gInitialTabURL) {
    BrowserTestUtils.startLoadingURIString(
      gBrowser.selectedBrowser,
      gInitialTabURL
    );
    await BrowserTestUtils.browserLoaded(
      gBrowser.selectedBrowser,
      null,
      gInitialTabURL
    );
  }
}

function getOpenTabsComponent(browser) {
  return browser.contentDocument.querySelector(
    "view-recentbrowsing view-opentabs"
  );
}

async function checkTabList(browser, expected) {
  const tabsView = getOpenTabsComponent(browser);
  const openTabsCard = tabsView.shadowRoot.querySelector("view-opentabs-card");
  await tabsView.getUpdateComplete();
  const tabList = openTabsCard.shadowRoot.querySelector("fxview-tab-list");
  Assert.ok(tabList, "Found the tab list element");
  await TestUtils.waitForCondition(() => tabList.rowEls.length);
  let actual = Array.from(tabList.rowEls).map(row => row.url);
  Assert.deepEqual(
    actual,
    expected,
    "Tab list has items with URLs in the expected order"
  );
}

add_task(async function test_single_window_tabs() {
  await prepareOpenTabs([tabURL1, tabURL2]);
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkTabList(browser, [tabURL2, tabURL1]);

    // switch to the first tab
    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.visibleTabs[0]);
    await promiseHidden;
  });

  // and check the results in the open tabs section of Recent Browsing
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkTabList(browser, [tabURL1, tabURL2]);
  });
  await cleanup();
});

add_task(async function test_multiple_window_tabs() {
  const fxViewURL = getFirefoxViewURL();
  const win1 = window;
  await prepareOpenTabs([tabURL1, tabURL2]);
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await prepareOpenTabs([tabURL3, tabURL4], win2);

  // to avoid confusing the results by activating different windows,
  // check fxview in the current window - which is win2
  info("Switching to fxview tab in win2");
  await openFirefoxViewTab(win2).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkTabList(browser, [tabURL4, tabURL3, tabURL2, tabURL1]);

    Assert.equal(
      tabUrl(win2.gBrowser.selectedTab),
      fxViewURL,
      `The selected tab in window 2 is ${fxViewURL}`
    );

    info("Switching to first tab (tab3) in win2");
    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    await BrowserTestUtils.switchTab(
      win2.gBrowser,
      win2.gBrowser.visibleTabs[0]
    );
    Assert.equal(
      tabUrl(win2.gBrowser.selectedTab),
      tabURL3,
      `The selected tab in window 2 is ${tabURL3}`
    );
    await promiseHidden;
  });

  info("Opening fxview in win2 to confirm tab3 is most recent");
  await openFirefoxViewTab(win2).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    info("Check result of selecting 1ist tab in window 2");
    await checkTabList(browser, [tabURL3, tabURL4, tabURL2, tabURL1]);
  });

  info("Focusing win1, where tab2 should be selected");
  await SimpleTest.promiseFocus(win1);
  Assert.equal(
    tabUrl(win1.gBrowser.selectedTab),
    tabURL2,
    `The selected tab in window 1 is ${tabURL2}`
  );

  info("Opening fxview in win1 to confirm tab2 is most recent");
  await openFirefoxViewTab(win1).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    info(
      "In fxview, check result  of activating window 1, where tab 2 is selected"
    );
    await checkTabList(browser, [tabURL2, tabURL3, tabURL4, tabURL1]);

    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    info("Switching to first visible tab (tab1) in win1");
    await BrowserTestUtils.switchTab(
      win1.gBrowser,
      win1.gBrowser.visibleTabs[0]
    );
    await promiseHidden;
  });

  // check result in the fxview in the 1st window
  info("Opening fxview in win1 to confirm tab1 is most recent");
  await openFirefoxViewTab(win1).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    info("Check result of selecting 1st tab in win1");
    await checkTabList(browser, [tabURL1, tabURL2, tabURL3, tabURL4]);
  });

  await cleanup(win2);
});

add_task(async function test_windows_activation() {
  const win1 = window;
  await prepareOpenTabs([tabURL1], win1);
  let fxViewTab;
  info("switch to firefox-view and leave it selected");
  await openFirefoxViewTab(win1).then(tab => (fxViewTab = tab));

  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await prepareOpenTabs([tabURL2], win2);
  const win3 = await BrowserTestUtils.openNewBrowserWindow();
  await prepareOpenTabs([tabURL3], win3);

  await SimpleTest.promiseFocus(win1);

  const browser = fxViewTab.linkedBrowser;
  await checkTabList(browser, [tabURL3, tabURL2, tabURL1]);

  info("switch to win2 and confirm its selected tab becomes most recent");
  await SimpleTest.promiseFocus(win2);
  await checkTabList(browser, [tabURL2, tabURL3, tabURL1]);
  await cleanup(win2, win3);
});

add_task(async function test_minimize_restore_windows() {
  const win1 = window;
  await prepareOpenTabs([tabURL1, tabURL2]);
  const win2 = await BrowserTestUtils.openNewBrowserWindow();
  await prepareOpenTabs([tabURL3, tabURL4], win2);

  // to avoid confusing the results by activating different windows,
  // check fxview in the current window - which is win2
  info("Opening fxview in win2 to confirm tab4 is most recent");
  await openFirefoxViewTab(win2).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkTabList(browser, [tabURL4, tabURL3, tabURL2, tabURL1]);

    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    info("Switching to the first tab (tab3) in 2nd window");
    await BrowserTestUtils.switchTab(
      win2.gBrowser,
      win2.gBrowser.visibleTabs[0]
    );
    await promiseHidden;
  });

  // then minimize the window, focusing the 1st window
  info("Minimizing win2, leaving tab 3 selected");
  await minimizeWindow(win2);
  info("Focusing win1, where tab2 is selected - making it most recent");
  await SimpleTest.promiseFocus(win1);

  Assert.equal(
    tabUrl(win1.gBrowser.selectedTab),
    tabURL2,
    `The selected tab in window 1 is ${tabURL2}`
  );

  info("Opening fxview in win1 to confirm tab2 is most recent");
  await openFirefoxViewTab(win1).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkTabList(browser, [tabURL2, tabURL3, tabURL4, tabURL1]);
    info(
      "Restoring win2 and focusing it - which should make its selected tab most recent"
    );
    await restoreWindow(win2);
    await SimpleTest.promiseFocus(win2);

    info(
      "Checking tab order in fxview in win1, to confirm tab3 is most recent"
    );
    await checkTabList(browser, [tabURL3, tabURL2, tabURL4, tabURL1]);
  });

  await cleanup(win2);
});
