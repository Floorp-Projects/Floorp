/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
   This test checks that the recent-browsing view of open tabs in about:firefoxview
   presents the correct tab data in the correct order.
*/
SimpleTest.requestCompleteLog();

const { ObjectUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ObjectUtils.sys.mjs"
);
let origBrowserState;

const tabURL1 = "data:,Tab1";
const tabURL2 = "data:,Tab2";
const tabURL3 = "data:,Tab3";
const tabURL4 = "data:,Tab4";

add_setup(function () {
  origBrowserState = SessionStore.getBrowserState();
});

async function cleanup() {
  await switchToWindow(window);
  await SessionStoreTestUtils.promiseBrowserState(origBrowserState);
}

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

function getAllSelectedTabURLs() {
  return BrowserWindowTracker.orderedWindows.map(win =>
    tabUrl(win.gBrowser.selectedTab)
  );
}

async function restoreWindow(win) {
  ok(win.document.hidden, "Top level window should be hidden");
  let promiseSizeModeChange = BrowserTestUtils.waitForEvent(
    win,
    "sizemodechange"
  );

  // Check if we also need to wait for occlusion to be updated.
  let promiseOcclusion;
  let willWaitForOcclusion = win.isFullyOccluded;
  if (willWaitForOcclusion) {
    // Not only do we need to wait for the occlusionstatechange event,
    // we also have to wait *one more event loop* to ensure that the
    // other listeners to the occlusionstatechange events have fired.
    // Otherwise, our browsing context might not have become active
    // at the point where we receive the occlusionstatechange event.
    promiseOcclusion = BrowserTestUtils.waitForEvent(
      win,
      "occlusionstatechange"
    ).then(() => new Promise(resolve => SimpleTest.executeSoon(resolve)));
  } else {
    promiseOcclusion = Promise.resolve();
  }

  info("Calling window.restore");
  win.restore();
  // From browser/base/content/test/general/browser_minimize.js:
  // On Ubuntu `window.restore` doesn't seem to work, use a timer to make the
  // test fail faster and more cleanly than with a test timeout.
  info(
    `Waiting for sizemodechange ${
      willWaitForOcclusion ? "and occlusionstatechange " : ""
    }event`
  );
  let timer;
  await Promise.race([
    Promise.all([promiseSizeModeChange, promiseOcclusion]),
    new Promise((resolve, reject) => {
      // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
      timer = setTimeout(() => {
        reject(
          `timed out waiting for sizemodechange sizemodechange ${
            willWaitForOcclusion ? "and occlusionstatechange " : ""
          }event`
        );
      }, 5000);
    }),
  ]);
  clearTimeout(timer);
  ok(
    win.gBrowser.selectedTab.linkedBrowser.docShellIsActive,
    "Docshell should be active again"
  );
  ok(!win.document.hidden, "Top level window should be visible");
}

async function prepareOpenWindowsAndTabs(windowsData) {
  // windowsData selected tab URL should be unique so we can map tab URL to window
  const browserState = {
    windows: windowsData.map((winData, index) => {
      const tabs = winData.tabs.map(url => ({
        entries: [{ url, triggeringPrincipal_base64 }],
      }));
      return {
        tabs,
        selected: winData.selectedIndex + 1,
        zIndex: index + 1,
      };
    }),
  };
  await SessionStoreTestUtils.promiseBrowserState(browserState);
  await NonPrivateTabs.readyWindowsPromise;
  const selectedTabURLOrder = browserState.windows.map(winData => {
    return winData.tabs[winData.selected - 1].entries[0].url;
  });
  const windowByTabURL = new Map();
  for (let win of BrowserWindowTracker.orderedWindows) {
    windowByTabURL.set(tabUrl(win.gBrowser.selectedTab), win);
  }
  is(
    windowByTabURL.size,
    windowsData.length,
    "The tab URL to window mapping includes an entry for each window"
  );
  info(
    `After promiseBrowserState, selected tab order is: ${Array.from(
      windowByTabURL.keys()
    )}`
  );

  // Make any corrections to the window order by selecting each in reverse order
  for (let url of selectedTabURLOrder.toReversed()) {
    await switchToWindow(windowByTabURL.get(url));
  }
  // Verify windows are in the expected order
  Assert.deepEqual(
    getAllSelectedTabURLs(),
    selectedTabURLOrder,
    "The windows and their selected tabs are in the expected order"
  );
  Assert.deepEqual(
    BrowserWindowTracker.orderedWindows.map(win =>
      win.gBrowser.visibleTabs.map(tab => tabUrl(tab))
    ),
    windowsData.map(winData => winData.tabs),
    "We opened all the tabs in each window"
  );
}

function getRecentOpenTabsComponent(browser) {
  return browser.contentDocument.querySelector(
    "view-recentbrowsing view-opentabs"
  );
}

async function checkRecentTabList(browser, expected) {
  const tabsView = getRecentOpenTabsComponent(browser);
  const [openTabsCard] = getOpenTabsCards(tabsView);
  await openTabsCard.updateComplete;

  const tabListRows = await getTabRowsForCard(openTabsCard);
  Assert.ok(tabListRows, "Found the tab list element");
  let actual = Array.from(tabListRows).map(row => row.url);
  await BrowserTestUtils.waitForCondition(
    () => ObjectUtils.deepEqual(actual, expected),
    "Waiting for tab list to hvae items with URLs in the expected order"
  );
}

add_task(async function test_single_window_tabs() {
  const testData = [
    {
      tabs: [tabURL1, tabURL2],
      selectedIndex: 1, // the 2nd tab should be selected
    },
  ];
  await prepareOpenWindowsAndTabs(testData);

  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkRecentTabList(browser, [tabURL2, tabURL1]);

    // switch to the first tab
    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );

    let tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabRecencyChange"
    );
    await BrowserTestUtils.switchTab(gBrowser, gBrowser.visibleTabs[0]);
    await promiseHidden;
    await tabChangeRaised;
  });

  // and check the results in the open tabs section of Recent Browsing
  await openFirefoxViewTab(window).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkRecentTabList(browser, [tabURL1, tabURL2]);
  });
  await cleanup();
});

add_task(async function test_multiple_window_tabs() {
  const fxViewURL = getFirefoxViewURL();
  const testData = [
    {
      // this window should be active after restore
      tabs: [tabURL1, tabURL2],
      selectedIndex: 0, // tabURL1 should be selected
    },
    {
      tabs: [tabURL3, tabURL4],
      selectedIndex: 0, // tabURL3 should be selected
    },
  ];
  await prepareOpenWindowsAndTabs(testData);

  Assert.deepEqual(
    getAllSelectedTabURLs(),
    [tabURL1, tabURL3],
    "The windows and their selected tabs are in the expected order"
  );
  let tabChangeRaised;
  const [win1, win2] = BrowserWindowTracker.orderedWindows;

  info(`Switch to window 1's 2nd tab: ${tabUrl(win1.gBrowser.visibleTabs[1])}`);
  await BrowserTestUtils.switchTab(gBrowser, win1.gBrowser.visibleTabs[1]);
  await switchToWindow(win2);

  Assert.deepEqual(
    getAllSelectedTabURLs(),
    [tabURL3, tabURL2],
    `Window 2 has selected the ${tabURL3} tab, window 1 has ${tabURL2}`
  );
  info(`Switch to window 2's 2nd tab: ${tabUrl(win2.gBrowser.visibleTabs[1])}`);
  tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabRecencyChange"
  );
  await BrowserTestUtils.switchTab(win2.gBrowser, win2.gBrowser.visibleTabs[1]);
  await tabChangeRaised;
  Assert.deepEqual(
    getAllSelectedTabURLs(),
    [tabURL4, tabURL2],
    `window 2 has selected the ${tabURL4} tab, ${tabURL2} remains selected in window 1`
  );

  // to avoid confusing the results by activating different windows,
  // check fxview in the current window - which is win2
  info("Switching to fxview tab in win2");
  await openFirefoxViewTab(win2).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkRecentTabList(browser, [tabURL4, tabURL3, tabURL2, tabURL1]);

    Assert.equal(
      tabUrl(win2.gBrowser.selectedTab),
      fxViewURL,
      `The selected tab in window 2 is ${fxViewURL}`
    );

    info("Switching to first tab in win2");
    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabRecencyChange"
    );
    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    await BrowserTestUtils.switchTab(
      win2.gBrowser,
      win2.gBrowser.visibleTabs[0]
    );
    await tabChangeRaised;
    await promiseHidden;
    Assert.deepEqual(
      getAllSelectedTabURLs(),
      [tabURL3, tabURL2],
      `window 2 has switched to ${tabURL3}, ${tabURL2} remains selected in window 1`
    );
  });

  info("Opening fxview in win2 to confirm tab3 is most recent");
  await openFirefoxViewTab(win2).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    info("Check result of selecting 1ist tab in window 2");
    await checkRecentTabList(browser, [tabURL3, tabURL4, tabURL2, tabURL1]);
  });

  info("Focusing win1, where tab2 should be selected");
  tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabRecencyChange"
  );
  await switchToWindow(win1);
  await tabChangeRaised;
  Assert.deepEqual(
    getAllSelectedTabURLs(),
    [tabURL2, fxViewURL],
    `The selected tab in window 1 is ${tabURL2}, ${fxViewURL} remains selected in window 2`
  );

  info("Opening fxview in win1 to confirm tab2 is most recent");
  await openFirefoxViewTab(win1).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    info(
      "In fxview, check result  of activating window 1, where tab 2 is selected"
    );
    await checkRecentTabList(browser, [tabURL2, tabURL3, tabURL4, tabURL1]);

    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabRecencyChange"
    );
    info("Switching to first visible tab (tab1) in win1");
    await BrowserTestUtils.switchTab(
      win1.gBrowser,
      win1.gBrowser.visibleTabs[0]
    );
    await promiseHidden;
    await tabChangeRaised;
  });
  Assert.deepEqual(
    getAllSelectedTabURLs(),
    [tabURL1, fxViewURL],
    `The selected tab in window 1 is ${tabURL1}, ${fxViewURL} remains selected in window 2`
  );

  // check result in the fxview in the 1st window
  info("Opening fxview in win1 to confirm tab1 is most recent");
  await openFirefoxViewTab(win1).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    info("Check result of selecting 1st tab in win1");
    await checkRecentTabList(browser, [tabURL1, tabURL2, tabURL3, tabURL4]);
  });

  await cleanup();
});

add_task(async function test_windows_activation() {
  // use Session restore to batch-open windows and tabs
  const testData = [
    {
      // this window should be active after restore
      tabs: [tabURL1],
      selectedIndex: 0, // tabURL1 should be selected
    },
    {
      tabs: [tabURL2],
      selectedIndex: 0, // tabURL2 should be selected
    },
    {
      tabs: [tabURL3],
      selectedIndex: 0, // tabURL3 should be selected
    },
  ];
  await prepareOpenWindowsAndTabs(testData);

  let tabChangeRaised;
  const [win1, win2] = BrowserWindowTracker.orderedWindows;

  info("switch to firefox-view and leave it selected");
  await openFirefoxViewTab(win1).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkRecentTabList(browser, [tabURL1, tabURL2, tabURL3]);
  });

  info("switch to win2 and confirm its selected tab becomes most recent");
  tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabRecencyChange"
  );
  await switchToWindow(win2);
  await tabChangeRaised;
  await openFirefoxViewTab(win1).then(async viewTab => {
    await checkRecentTabList(viewTab.linkedBrowser, [
      tabURL2,
      tabURL1,
      tabURL3,
    ]);
  });
  await cleanup();
});

add_task(async function test_minimize_restore_windows() {
  const fxViewURL = getFirefoxViewURL();
  const testData = [
    {
      // this window should be active after restore
      tabs: [tabURL1, tabURL2],
      selectedIndex: 1, // tabURL2 should be selected
    },
    {
      tabs: [tabURL3, tabURL4],
      selectedIndex: 0, // tabURL3 should be selected
    },
  ];
  await prepareOpenWindowsAndTabs(testData);
  const [win1, win2] = BrowserWindowTracker.orderedWindows;

  // switch to the last (tabURL4) tab in window 2
  await switchToWindow(win2);
  let tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabRecencyChange"
  );
  await BrowserTestUtils.switchTab(win2.gBrowser, win2.gBrowser.visibleTabs[1]);
  await tabChangeRaised;
  Assert.deepEqual(
    getAllSelectedTabURLs(),
    [tabURL4, tabURL2],
    "The windows and their selected tabs are in the expected order"
  );

  // to avoid confusing the results by activating different windows,
  // check fxview in the current window - which is win2
  info("Opening fxview in win2 to confirm tab4 is most recent");
  await openFirefoxViewTab(win2).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkRecentTabList(browser, [tabURL4, tabURL3, tabURL2, tabURL1]);

    let promiseHidden = BrowserTestUtils.waitForEvent(
      browser.contentDocument,
      "visibilitychange"
    );
    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabRecencyChange"
    );
    info("Switching to the first tab (tab3) in 2nd window");
    await BrowserTestUtils.switchTab(
      win2.gBrowser,
      win2.gBrowser.visibleTabs[0]
    );
    await promiseHidden;
    await tabChangeRaised;
  });
  Assert.deepEqual(
    getAllSelectedTabURLs(),
    [tabURL3, tabURL2],
    `Window 2 has ${tabURL3} selected, window 1 remains at ${tabURL2}`
  );

  // then minimize the window, focusing the 1st window
  info("Minimizing win2, leaving tab 3 selected");
  tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabRecencyChange"
  );
  await minimizeWindow(win2);
  info("Focusing win1, where tab2 is selected - making it most recent");
  await switchToWindow(win1);
  await tabChangeRaised;

  Assert.deepEqual(
    getAllSelectedTabURLs(),
    [tabURL2, tabURL3],
    `Window 1 has ${tabURL2} selected, window 2 remains at ${tabURL3}`
  );

  info("Opening fxview in win1 to confirm tab2 is most recent");
  await openFirefoxViewTab(win1).then(async viewTab => {
    const browser = viewTab.linkedBrowser;
    await checkRecentTabList(browser, [tabURL2, tabURL3, tabURL4, tabURL1]);
    info(
      "Restoring win2 and focusing it - which should make its selected tab most recent"
    );
    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabRecencyChange",
      false,
      event => event.detail.sourceEvents?.includes("activate")
    );
    await restoreWindow(win2);
    await switchToWindow(win2);
    // make sure we wait for the activate event from OpenTabs.
    await tabChangeRaised;

    Assert.deepEqual(
      getAllSelectedTabURLs(),
      [tabURL3, fxViewURL],
      `Window 2 was restored and has ${tabURL3} selected, window 1 remains at ${fxViewURL}`
    );

    info(
      "Checking tab order in fxview in win1, to confirm tab3 is most recent"
    );
    await checkRecentTabList(browser, [tabURL3, tabURL2, tabURL4, tabURL1]);
  });
  info("test done, waiting for cleanup");
  await cleanup();
});
