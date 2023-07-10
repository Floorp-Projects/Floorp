/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

ChromeUtils.defineESModuleGetters(globalThis, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.sys.mjs",
});

const FXVIEW_NEXT_ENABLED_PREF = "browser.tabs.firefox-view-next";

function isElInViewport(element) {
  const boundingRect = element.getBoundingClientRect();
  return (
    boundingRect.top >= 0 &&
    boundingRect.left >= 0 &&
    boundingRect.bottom <=
      (window.innerHeight || document.documentElement.clientHeight) &&
    boundingRect.right <=
      (window.innerWidth || document.documentElement.clientWidth)
  );
}

async function openFirefoxView(win) {
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#firefox-view-button",
    { type: "mousedown" },
    win.browsingContext
  );
}

async function dismiss_tab(tab, content, document) {
  // Make sure the firefoxview-next tab still has focus
  is(
    document.location.href,
    "about:firefoxview-next#recentlyclosed",
    "about:firefoxview-next is the selected tab and showing the Recently closed view page"
  );

  // Scroll to the tab element to ensure dismiss button is visible
  tab.scrollIntoView();
  is(isElInViewport(tab), true, "Tab is visible in viewport");

  info(`Dismissing tab ${tab.url}`);
  const closedObjectsChanged = () =>
    TestUtils.topicObserved("sessionstore-closed-objects-changed");
  let dismissButton = tab.buttonEl;
  EventUtils.synthesizeMouseAtCenter(dismissButton, {}, content);
  await closedObjectsChanged();
}

function navigateToRecentlyClosed(document) {
  // Navigate to Recently closed tabs page/view
  const navigation = document.querySelector("fxview-category-navigation");
  let recentlyClosedNavButton = Array.from(navigation.categoryButtons).find(
    categoryButton => {
      return categoryButton.name === "recentlyclosed";
    }
  );
  recentlyClosedNavButton.buttonEl.click();
}

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({ set: [[FXVIEW_NEXT_ENABLED_PREF, true]] });
  registerCleanupFunction(async () => {
    await SpecialPowers.popPrefEnv();
    clearHistory();
  });
});

add_task(async function test_list_ordering() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCountForWindow(window),
    0,
    "Closed tab count after purging session history"
  );

  await open_then_close(URLs[0]);
  await open_then_close(URLs[1]);
  await open_then_close(URLs[2]);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");

    navigateToRecentlyClosed(document);

    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed:not([slot=recentlyclosed])"
    );

    // Check that tabs list is rendered
    await TestUtils.waitForCondition(() => {
      return recentlyClosedComponent.cardEl;
    });
    let cardContainer = recentlyClosedComponent.cardEl;
    let cardMainSlotNode = Array.from(
      cardContainer?.mainSlot?.assignedNodes()
    )[0];
    is(
      cardMainSlotNode.tagName.toLowerCase(),
      "fxview-tab-list",
      "The tab list component is rendered."
    );

    let tabList = cardMainSlotNode.rowEls;

    is(tabList.length, 3, "Three tabs are shown in the list.");
    is(
      tabList[0].url,
      "https://example.net/",
      "First list item in recentlyclosed is in the correct order"
    );
    is(
      tabList[2].url,
      "http://mochi.test:8888/browser/",
      "Last list item in recentlyclosed is in the correct order"
    );

    let uri = tabList[0].url;
    let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, uri);
    tabList[0].mainEl.click();
    await newTabPromise;

    gBrowser.removeTab(gBrowser.selectedTab);
  });
});

add_task(async function test_list_updates() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCountForWindow(window),
    0,
    "Closed tab count after purging session history"
  );

  await open_then_close(URLs[0]);
  await open_then_close(URLs[1]);
  await open_then_close(URLs[2]);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;
    is(document.location.href, "about:firefoxview-next");

    navigateToRecentlyClosed(document);

    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed:not([slot=recentlyclosed])"
    );

    // Check that tabs list is rendered
    await TestUtils.waitForCondition(() => {
      return recentlyClosedComponent.cardEl;
    });
    let cardContainer = recentlyClosedComponent.cardEl;
    let cardMainSlotNode = Array.from(
      cardContainer?.mainSlot?.assignedNodes()
    )[0];
    is(
      cardMainSlotNode.tagName.toLowerCase(),
      "fxview-tab-list",
      "The tab list component is rendered."
    );

    let tabList = cardMainSlotNode.rowEls;

    is(tabList.length, 3, "Three tabs are shown in the list.");
    const closedObjectsChanged = () =>
      TestUtils.topicObserved("sessionstore-closed-objects-changed");
    SessionStore.undoCloseById(tabList[0].closedId);
    await closedObjectsChanged();
    await openFirefoxView(window);
    tabList = cardMainSlotNode.rowEls;
    is(tabList.length, 2, "Two tabs are shown in the list.");

    const closedObjectsChangedAgain = () =>
      TestUtils.topicObserved("sessionstore-closed-objects-changed");
    SessionStore.forgetClosedTab(window, 0);
    await closedObjectsChangedAgain();
    await openFirefoxView(window);
    tabList = cardMainSlotNode.rowEls;
    is(tabList.length, 1, "One tab is shown in the list.");

    // clean up extra tabs
    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
});

/**
 * Asserts that tabs that have been recently closed can be
 * dismissed by clicking on their respective dismiss buttons.
 */
add_task(async function test_dismiss_tab() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCountForWindow(window),
    0,
    "Closed tab count after purging session history"
  );

  await open_then_close(URLs[0]);
  await open_then_close(URLs[1]);
  await open_then_close(URLs[2]);
  await withFirefoxView({}, async browser => {
    const { document } = browser.contentWindow;

    navigateToRecentlyClosed(document);

    let recentlyClosedComponent = document.querySelector(
      "view-recentlyclosed:not([slot=recentlyclosed])"
    );

    // Check that tabs list is rendered
    await TestUtils.waitForCondition(() => {
      return recentlyClosedComponent.cardEl;
    });
    let cardContainer = recentlyClosedComponent.cardEl;
    let cardMainSlotNode = Array.from(
      cardContainer?.mainSlot?.assignedNodes()
    )[0];
    let tabList = cardMainSlotNode.rowEls;

    await dismiss_tab(tabList[0], content, document);
    await recentlyClosedComponent.getUpdateComplete();
    Assert.equal(SessionStore.getClosedTabCountForWindow(window), 2);
    tabList = cardMainSlotNode.rowEls;

    Assert.equal(
      tabList[0].url,
      URLs[1],
      `First recently closed item should be ${URLs[1]}`
    );

    Assert.equal(
      tabList.length,
      2,
      "recentlyclosed should have two list items"
    );

    while (gBrowser.tabs.length > 1) {
      BrowserTestUtils.removeTab(gBrowser.tabs.at(-1));
    }
  });
});
