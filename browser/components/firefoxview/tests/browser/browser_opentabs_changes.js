const { getTabsTargetForWindow } = ChromeUtils.importESModule(
  "resource:///modules/OpenTabs.sys.mjs"
);
let privateTabsChanges;

const tabURL1 = "data:text/html,<title>Tab1</title>Tab1";
const tabURL2 = "data:text/html,<title>Tab2</title>Tab2";
const tabURL3 = "data:text/html,<title>Tab3</title>Tab3";
const tabURL4 = "data:text/html,<title>Tab4</title>Tab4";

const nonPrivateListener = sinon.stub();
const privateListener = sinon.stub();

function tabUrl(tab) {
  return tab.linkedBrowser.currentURI?.spec;
}

function getWindowId(win) {
  return win.windowGlobalChild.innerWindowId;
}

async function setup(tabChangeEventName) {
  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  NonPrivateTabs.addEventListener(tabChangeEventName, nonPrivateListener);

  await TestUtils.waitForTick();
  is(
    NonPrivateTabs.currentWindows.length,
    1,
    "NonPrivateTabs has 1 window a tick after adding the event listener"
  );

  info("Opening new windows");
  let win0 = window,
    win1 = await BrowserTestUtils.openNewBrowserWindow(),
    privateWin = await BrowserTestUtils.openNewBrowserWindow({
      private: true,
    });
  BrowserTestUtils.startLoadingURIString(
    win1.gBrowser.selectedBrowser,
    tabURL1
  );
  await BrowserTestUtils.browserLoaded(win1.gBrowser.selectedBrowser);

  // load a tab with a title/label we can easily verify
  BrowserTestUtils.startLoadingURIString(
    privateWin.gBrowser.selectedBrowser,
    tabURL2
  );
  await BrowserTestUtils.browserLoaded(privateWin.gBrowser.selectedBrowser);

  is(
    win1.gBrowser.selectedTab.label,
    "Tab1",
    "Check the tab label in the new non-private window"
  );
  is(
    privateWin.gBrowser.selectedTab.label,
    "Tab2",
    "Check the tab label in the new private window"
  );

  privateTabsChanges = getTabsTargetForWindow(privateWin);
  privateTabsChanges.addEventListener(tabChangeEventName, privateListener);
  is(
    privateTabsChanges,
    getTabsTargetForWindow(privateWin),
    "getTabsTargetForWindow reuses a single instance per exclusive window"
  );

  await TestUtils.waitForTick();
  is(
    NonPrivateTabs.currentWindows.length,
    2,
    "NonPrivateTabs has 2 windows once openNewBrowserWindow resolves"
  );
  is(
    privateTabsChanges.currentWindows.length,
    1,
    "privateTabsChanges has 1 window once openNewBrowserWindow resolves"
  );

  await SimpleTest.promiseFocus(win0);
  info("setup, win0 has id: " + getWindowId(win0));
  info("setup, win1 has id: " + getWindowId(win1));
  info("setup, privateWin has id: " + getWindowId(privateWin));
  info("setup,waiting for both private and nonPrivateListener to be called");
  await TestUtils.waitForCondition(() => {
    return nonPrivateListener.called && privateListener.called;
  });
  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  const cleanup = async eventName => {
    NonPrivateTabs.removeEventListener(eventName, nonPrivateListener);
    privateTabsChanges.removeEventListener(eventName, privateListener);
    await SimpleTest.promiseFocus(window);
    await promiseAllButPrimaryWindowClosed();
  };
  return { windows: [win0, win1, privateWin], cleanup };
}

add_task(async function test_TabChanges() {
  const { windows, cleanup } = await setup("TabChange");
  const [win0, win1, privateWin] = windows;
  let tabChangeRaised;
  let changeEvent;

  info(
    "Verify that manipulating tabs in a non-private window dispatches events on the correct target"
  );
  for (let win of [win0, win1]) {
    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );
    let newTab = await BrowserTestUtils.openNewForegroundTab(
      win.gBrowser,
      tabURL1
    );
    changeEvent = await tabChangeRaised;
    Assert.deepEqual(
      changeEvent.detail.windowIds,
      [getWindowId(win)],
      "The event had the correct window id"
    );

    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );
    const navigateUrl = "https://example.org/";
    BrowserTestUtils.startLoadingURIString(newTab.linkedBrowser, navigateUrl);
    await BrowserTestUtils.browserLoaded(
      newTab.linkedBrowser,
      null,
      navigateUrl
    );
    // navigation in a tab changes the label which should produce a change event
    changeEvent = await tabChangeRaised;
    Assert.deepEqual(
      changeEvent.detail.windowIds,
      [getWindowId(win)],
      "The event had the correct window id"
    );

    tabChangeRaised = BrowserTestUtils.waitForEvent(
      NonPrivateTabs,
      "TabChange"
    );
    BrowserTestUtils.removeTab(newTab);
    // navigation in a tab changes the label which should produce a change event
    changeEvent = await tabChangeRaised;
    Assert.deepEqual(
      changeEvent.detail.windowIds,
      [getWindowId(win)],
      "The event had the correct window id"
    );
  }

  info(
    "make sure a change to a private window doesnt dispatch on a nonprivate target"
  );
  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  tabChangeRaised = BrowserTestUtils.waitForEvent(
    privateTabsChanges,
    "TabChange"
  );
  BrowserTestUtils.addTab(privateWin.gBrowser, tabURL1);
  changeEvent = await tabChangeRaised;
  info(
    `Check windowIds adding tab to private window: ${getWindowId(
      privateWin
    )}: ${JSON.stringify(changeEvent.detail.windowIds)}`
  );
  Assert.deepEqual(
    changeEvent.detail.windowIds,
    [getWindowId(privateWin)],
    "The event had the correct window id"
  );
  await TestUtils.waitForTick();
  Assert.ok(
    nonPrivateListener.notCalled,
    "A private tab change shouldnt raise a tab change event on the non-private target"
  );

  info("testTabChanges complete");
  await cleanup("TabChange");
});

add_task(async function test_TabRecencyChange() {
  const { windows, cleanup } = await setup("TabRecencyChange");
  const [win0, win1, privateWin] = windows;

  let tabChangeRaised;
  let changeEvent;
  let sortedTabs;

  info("Open some tabs in the non-private windows");
  for (let win of [win0, win1]) {
    for (let url of [tabURL1, tabURL2]) {
      let tab = BrowserTestUtils.addTab(win.gBrowser, url);
      tabChangeRaised = BrowserTestUtils.waitForEvent(
        NonPrivateTabs,
        "TabChange"
      );
      await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
      await tabChangeRaised;
    }
  }

  info("Verify switching tabs produces the expected event and result");
  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabRecencyChange"
  );
  BrowserTestUtils.switchTab(win0.gBrowser, win0.gBrowser.tabs.at(-1));
  changeEvent = await tabChangeRaised;

  Assert.deepEqual(
    changeEvent.detail.windowIds,
    [getWindowId(win0)],
    "The recency change event had the correct window id"
  );
  Assert.ok(
    nonPrivateListener.called,
    "Sanity check that the non-private tabs listener was called"
  );
  Assert.ok(
    privateListener.notCalled,
    "The private tabs listener was not called"
  );

  sortedTabs = NonPrivateTabs.getRecentTabs();
  is(
    sortedTabs[0],
    win0.gBrowser.selectedTab,
    "The most-recent tab is the selected tab"
  );

  info("Verify switching window produces the expected event and result");
  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabRecencyChange"
  );
  await SimpleTest.promiseFocus(win1);
  changeEvent = await tabChangeRaised;
  Assert.deepEqual(
    changeEvent.detail.windowIds,
    [getWindowId(win1)],
    "The recency change event had the correct window id"
  );
  Assert.ok(
    nonPrivateListener.called,
    "Sanity check that the non-private tabs listener was called"
  );
  Assert.ok(
    privateListener.notCalled,
    "The private tabs listener was not called"
  );

  sortedTabs = NonPrivateTabs.getRecentTabs();
  is(
    sortedTabs[0],
    win1.gBrowser.selectedTab,
    "The most-recent tab is the selected tab in the current window"
  );

  info("Verify behavior with private window changes");
  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  tabChangeRaised = BrowserTestUtils.waitForEvent(
    privateTabsChanges,
    "TabRecencyChange"
  );
  await SimpleTest.promiseFocus(privateWin);
  changeEvent = await tabChangeRaised;
  Assert.deepEqual(
    changeEvent.detail.windowIds,
    [getWindowId(privateWin)],
    "The recency change event had the correct window id"
  );
  Assert.ok(
    nonPrivateListener.notCalled,
    "The non-private listener got no recency-change events from the private window"
  );
  Assert.ok(
    privateListener.called,
    "Sanity check the private tabs listener was called"
  );

  sortedTabs = privateTabsChanges.getRecentTabs();
  is(
    sortedTabs[0],
    privateWin.gBrowser.selectedTab,
    "The most-recent tab is the selected tab in the current window"
  );
  sortedTabs = NonPrivateTabs.getRecentTabs();
  is(
    sortedTabs[0],
    win1.gBrowser.selectedTab,
    "The most-recent non-private tab is still the selected tab in the previous non-private window"
  );

  info("Verify adding a tab to a private window does the right thing");
  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  tabChangeRaised = BrowserTestUtils.waitForEvent(
    privateTabsChanges,
    "TabRecencyChange"
  );
  await BrowserTestUtils.openNewForegroundTab(privateWin.gBrowser, tabURL3);
  changeEvent = await tabChangeRaised;
  Assert.deepEqual(
    changeEvent.detail.windowIds,
    [getWindowId(privateWin)],
    "The event had the correct window id"
  );
  Assert.ok(
    nonPrivateListener.notCalled,
    "The non-private listener got no recency-change events from the private window"
  );
  sortedTabs = privateTabsChanges.getRecentTabs();
  is(
    tabUrl(sortedTabs[0]),
    tabURL3,
    "The most-recent tab is the tab we just opened in the private window"
  );

  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  tabChangeRaised = BrowserTestUtils.waitForEvent(
    privateTabsChanges,
    "TabRecencyChange"
  );
  BrowserTestUtils.switchTab(privateWin.gBrowser, privateWin.gBrowser.tabs[0]);
  changeEvent = await tabChangeRaised;
  Assert.deepEqual(
    changeEvent.detail.windowIds,
    [getWindowId(privateWin)],
    "The event had the correct window id"
  );
  Assert.ok(
    nonPrivateListener.notCalled,
    "The non-private listener got no recency-change events from the private window"
  );
  sortedTabs = privateTabsChanges.getRecentTabs();
  is(
    sortedTabs[0],
    privateWin.gBrowser.selectedTab,
    "The most-recent tab is the selected tab in the private window"
  );

  info("Verify switching back to a non-private does the right thing");
  nonPrivateListener.resetHistory();
  privateListener.resetHistory();

  tabChangeRaised = BrowserTestUtils.waitForEvent(
    NonPrivateTabs,
    "TabRecencyChange"
  );
  await SimpleTest.promiseFocus(win1);
  await tabChangeRaised;
  if (privateListener.called) {
    info(`The private listener was called ${privateListener.callCount} times`);
  }
  Assert.ok(
    privateListener.notCalled,
    "The private listener got no recency-change events for the non-private window"
  );
  Assert.ok(
    nonPrivateListener.called,
    "Sanity-check the non-private listener got a recency-change event for the non-private window"
  );

  sortedTabs = privateTabsChanges.getRecentTabs();
  is(
    sortedTabs[0],
    privateWin.gBrowser.selectedTab,
    "The most-recent private tab is unchanged"
  );

  sortedTabs = NonPrivateTabs.getRecentTabs();
  is(
    sortedTabs[0],
    win1.gBrowser.selectedTab,
    "The most-recent non-private tab is the selected tab in the current window"
  );

  await cleanup("TabRecencyChange");
  while (win0.gBrowser.tabs.length > 1) {
    info(
      "Removing last tab:" +
        win0.gBrowser.tabs.at(-1).linkedBrowser.currentURI.spec
    );
    BrowserTestUtils.removeTab(win0.gBrowser.tabs.at(-1));
    info("Removed, tabs.length:" + win0.gBrowser.tabs.length);
  }
});

add_task(async function test_tabNavigations() {
  const { windows, cleanup } = await setup("TabChange");
  const [, win1, privateWin] = windows;

  // also listen for TabRecencyChange events
  const nonPrivateRecencyListener = sinon.stub();
  const privateRecencyListener = sinon.stub();
  privateTabsChanges.addEventListener(
    "TabRecencyChange",
    privateRecencyListener
  );
  NonPrivateTabs.addEventListener(
    "TabRecencyChange",
    nonPrivateRecencyListener
  );

  info(
    `Verify navigating in tab generates TabChange & TabRecencyChange events`
  );
  let loaded = BrowserTestUtils.browserLoaded(win1.gBrowser.selectedBrowser);
  win1.gBrowser.selectedBrowser.loadURI(Services.io.newURI(tabURL4), {
    triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
  });
  info("waiting for the load into win1 tab to complete");
  await loaded;
  info("waiting for listeners to be called");
  await BrowserTestUtils.waitForCondition(() => {
    return nonPrivateListener.called && nonPrivateRecencyListener.called;
  });
  ok(!privateListener.called, "The private TabChange listener was not called");
  ok(
    !privateRecencyListener.called,
    "The private TabRecencyChange listener was not called"
  );

  nonPrivateListener.resetHistory();
  privateListener.resetHistory();
  nonPrivateRecencyListener.resetHistory();
  privateRecencyListener.resetHistory();

  // Now verify the same with a private window
  info(
    `Verify navigating in private tab generates TabChange & TabRecencyChange events`
  );
  ok(
    !nonPrivateListener.called,
    "The non-private TabChange listener is not yet called"
  );

  loaded = BrowserTestUtils.browserLoaded(privateWin.gBrowser.selectedBrowser);
  privateWin.gBrowser.selectedBrowser.loadURI(
    Services.io.newURI("about:robots"),
    {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    }
  );
  info("waiting for the load into privateWin tab to complete");
  await loaded;
  info("waiting for the privateListeners to be called");
  await BrowserTestUtils.waitForCondition(() => {
    return privateListener.called && privateRecencyListener.called;
  });
  ok(
    !nonPrivateListener.called,
    "The non-private TabChange listener was not called"
  );
  ok(
    !nonPrivateRecencyListener.called,
    "The non-private TabRecencyChange listener was not called"
  );

  // cleanup
  privateTabsChanges.removeEventListener(
    "TabRecencyChange",
    privateRecencyListener
  );
  NonPrivateTabs.removeEventListener(
    "TabRecencyChange",
    nonPrivateRecencyListener
  );

  await cleanup();
});

add_task(async function test_tabsFromPrivateWindows() {
  const { cleanup } = await setup("TabChange");
  const private2Listener = sinon.stub();

  const private2Win = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
    waitForTabURL: "about:privatebrowsing",
  });
  const private2TabsChanges = getTabsTargetForWindow(private2Win);
  private2TabsChanges.addEventListener("TabChange", private2Listener);
  Assert.notStrictEqual(
    privateTabsChanges,
    getTabsTargetForWindow(private2Win),
    "getTabsTargetForWindow creates a distinct instance for a different private window"
  );

  await BrowserTestUtils.waitForCondition(() => private2Listener.called);

  ok(
    !privateListener.called,
    "No TabChange event was raised by opening a different private window"
  );
  privateListener.resetHistory();
  private2Listener.resetHistory();

  BrowserTestUtils.addTab(private2Win.gBrowser, tabURL1);
  await BrowserTestUtils.waitForCondition(() => private2Listener.called);
  ok(
    !privateListener.called,
    "No TabChange event was raised by adding tab to a different private window"
  );

  is(
    privateTabsChanges.getRecentTabs().length,
    1,
    "The recent tab count for the first private window tab target only reports the tabs for its associated windodw"
  );
  is(
    private2TabsChanges.getRecentTabs().length,
    2,
    "The recent tab count for a 2nd private window tab target only reports the tabs for its associated windodw"
  );

  await cleanup("TabChange");
});
