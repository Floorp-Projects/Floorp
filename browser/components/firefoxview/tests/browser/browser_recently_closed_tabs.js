/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(globalThis, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
});

const { TabsSetupFlowManager } = ChromeUtils.importESModule(
  "resource:///modules/firefox-view-tabs-setup-manager.sys.mjs"
);

const URLs = [
  "http://mochi.test:8888/browser/",
  "http://www.example.com/",
  "http://example.net",
  "http://example.org",
];

const RECENTLY_CLOSED_EVENT = [
  ["firefoxview", "entered", "firefoxview", undefined],
  ["firefoxview", "recently_closed", "tabs", undefined],
];

const CLOSED_TABS_OPEN_EVENT = [
  ["firefoxview", "closed_tabs_open", "tabs", "false"],
];

const RECENTLY_CLOSED_STATE_PREF =
  "browser.tabs.firefox-view.ui-state.recently-closed-tabs.open";

async function add_new_tab(URL) {
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  return tab;
}

async function close_tab(tab) {
  const sessionStorePromise = BrowserTestUtils.waitForSessionStoreUpdate(tab);
  BrowserTestUtils.removeTab(tab);
  await sessionStorePromise;
}

async function open_then_close(url) {
  let { updatePromise } = await BrowserTestUtils.withNewTab(
    url,
    async browser => {
      return {
        updatePromise: BrowserTestUtils.waitForSessionStoreUpdate({
          linkedBrowser: browser,
        }),
      };
    }
  );
  await updatePromise;
  return TestUtils.topicObserved("sessionstore-closed-objects-changed");
}

function clearHistory() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
}

add_task(async function test_empty_list() {
  clearHistory();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      let container = document.querySelector("#collapsible-tabs-container");
      ok(
        container.classList.contains("empty-container"),
        "collapsible container should have correct styling when the list is empty"
      );

      testVisibility(browser, {
        expectedVisible: {
          "#recently-closed-tabs-placeholder": true,
          "ol.closed-tabs-list": false,
        },
      });

      const tab1 = await add_new_tab(URLs[0]);

      await close_tab(tab1);

      // The UI update happens asynchronously as we learn of the new closed tab.
      await BrowserTestUtils.waitForMutationCondition(
        container,
        { attributeFilter: ["class"] },
        () => !container.classList.contains("empty-container")
      );
      ok(
        !container.classList.contains("empty-container"),
        "collapsible container should have correct styling when the list is not empty"
      );

      testVisibility(browser, {
        expectedVisible: {
          "#recently-closed-tabs-placeholder": false,
          "ol.closed-tabs-list": true,
        },
      });

      is(
        document.querySelector("ol.closed-tabs-list").children.length,
        1,
        "recently-closed-tabs-list should have one list item"
      );
    }
  );
});

add_task(async function test_list_ordering() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Closed tab count after purging session history"
  );
  await clearAllParentTelemetryEvents();

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      const closedObjectsChanged = () =>
        TestUtils.topicObserved("sessionstore-closed-objects-changed");

      const tab1 = await add_new_tab(URLs[0]);
      const tab2 = await add_new_tab(URLs[1]);
      const tab3 = await add_new_tab(URLs[2]);

      gBrowser.selectedTab = tab3;

      await close_tab(tab3);
      await closedObjectsChanged();

      await close_tab(tab2);
      await closedObjectsChanged();

      await close_tab(tab1);
      await closedObjectsChanged();

      const tabsList = document.querySelector("ol.closed-tabs-list");
      await BrowserTestUtils.waitForMutationCondition(
        tabsList,
        { childList: true },
        () => tabsList.children.length > 1
      );

      is(
        document.querySelector("ol.closed-tabs-list").children.length,
        3,
        "recently-closed-tabs-list should have three list items"
      );

      // check that the ordering is correct when user navigates to another tab, and then closes multiple tabs.
      ok(
        document
          .querySelector("ol.closed-tabs-list")
          .firstChild.textContent.includes("mochi.test"),
        "first list item in recently-closed-tabs-list is in the correct order"
      );

      ok(
        document
          .querySelector("ol.closed-tabs-list")
          .children[2].textContent.includes("example.net"),
        "last list item in recently-closed-tabs-list is in the correct order"
      );

      let ele = document.querySelector("ol.closed-tabs-list").firstElementChild;
      let uri = ele.getAttribute("data-target-u-r-i");
      let newTabPromise = BrowserTestUtils.waitForNewTab(gBrowser, uri);
      ele.click();
      await newTabPromise;

      await TestUtils.waitForCondition(
        () => {
          let events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).parent;
          return events && events.length >= 2;
        },
        "Waiting for entered and recently_closed firefoxview telemetry events.",
        200,
        100
      );

      TelemetryTestUtils.assertEvents(
        RECENTLY_CLOSED_EVENT,
        { category: "firefoxview" },
        { clear: true, process: "parent" }
      );

      gBrowser.removeTab(gBrowser.selectedTab);

      await clearAllParentTelemetryEvents();

      await waitForElementVisible(
        browser,
        "#recently-closed-tabs-container > summary"
      );
      document
        .querySelector("#recently-closed-tabs-container > summary")
        .click();

      await TestUtils.waitForCondition(
        () => {
          let events = Services.telemetry.snapshotEvents(
            Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
            false
          ).parent;
          return events && events.length >= 1;
        },
        "Waiting for closed_tabs_open firefoxview telemetry event.",
        200,
        100
      );

      TelemetryTestUtils.assertEvents(
        CLOSED_TABS_OPEN_EVENT,
        { category: "firefoxview" },
        { clear: true, process: "parent" }
      );
    }
  );
});

add_task(async function test_max_list_items() {
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Closed tab count after purging session history"
  );

  await open_then_close(URLs[0]);
  await open_then_close(URLs[1]);
  await open_then_close(URLs[2]);

  // Seed the closed tabs count. We've assured that we've opened and
  // closed at least three tabs because of the calls to open_then_close
  // above.
  let mockMaxTabsLength = 3;

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      // override this value for testing purposes
      document.querySelector(
        "recently-closed-tabs-list"
      ).maxTabsLength = mockMaxTabsLength;

      ok(
        !document
          .querySelector("#collapsible-tabs-container")
          .classList.contains("empty-container"),
        "collapsible container should have correct styling when the list is not empty"
      );

      testVisibility(browser, {
        expectedVisible: {
          "#recently-closed-tabs-placeholder": false,
          "ol.closed-tabs-list": true,
        },
      });

      is(
        document.querySelector("ol.closed-tabs-list").childNodes.length,
        mockMaxTabsLength,
        `recently-closed-tabs-list should have ${mockMaxTabsLength} list items`
      );

      const closedObjectsChanged = TestUtils.topicObserved(
        "sessionstore-closed-objects-changed"
      );
      // add another tab
      const tab = await add_new_tab(URLs[3]);
      await close_tab(tab);
      await closedObjectsChanged;

      let firstListItem = document.querySelector("ol.closed-tabs-list")
        .firstChild;
      await BrowserTestUtils.waitForMutationCondition(
        firstListItem,
        { characterData: true, childList: true, subtree: true },
        () => firstListItem.textContent.includes(".org")
      );
      ok(
        firstListItem.textContent.includes("example.org"),
        "first list item in recently-closed-tabs-list should have been updated"
      );

      is(
        document.querySelector("ol.closed-tabs-list").childNodes.length,
        mockMaxTabsLength,
        `recently-closed-tabs-list should still have ${mockMaxTabsLength} list items`
      );
    }
  );
});

add_task(async function test_time_updates_correctly() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;

      // Use session store data from previous tests; the last child is the oldest and has
      // a data-timestamp of approx one minute ago which is below the 'Just now' threshold
      const lastListItem = document.querySelector("ol.closed-tabs-list")
        .lastChild;
      const timeLabel = lastListItem.querySelector("span.closed-tab-li-time");

      let initialTimeText = timeLabel.textContent;
      Assert.stringContains(
        initialTimeText,
        "Just now",
        "recently-closed-tabs list item time is 'Just now'"
      );

      await SpecialPowers.pushPrefEnv({
        set: [["browser.tabs.firefox-view.updateTimeMs", 5]],
      });

      await BrowserTestUtils.waitForMutationCondition(
        timeLabel,
        { childList: true },
        () => !timeLabel.textContent.includes("now")
      );

      isnot(
        timeLabel.textContent,
        initialTimeText,
        "recently-closed-tabs list item time has updated"
      );

      await SpecialPowers.popPrefEnv();
    }
  );
});

add_task(async function test_arrow_keys() {
  await SpecialPowers.clearUserPref(RECENTLY_CLOSED_STATE_PREF);
  Services.obs.notifyObservers(null, "browser:purge-session-history");
  is(
    SessionStore.getClosedTabCount(window),
    0,
    "Closed tab count after purging session history"
  );

  const sandbox = sinon.createSandbox();
  let setupCompleteStub = sandbox.stub(
    TabsSetupFlowManager,
    "isTabSyncSetupComplete"
  );
  setupCompleteStub.returns(true);

  await open_then_close(URLs[0]);
  await open_then_close(URLs[1]);
  await open_then_close(URLs[2]);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      const list = document.querySelectorAll(".closed-tab-li");
      const arrowDown = () => {
        info("Arrow down");
        EventUtils.synthesizeKey("KEY_ArrowDown");
      };
      const arrowUp = () => {
        info("Arrow up");
        EventUtils.synthesizeKey("KEY_ArrowUp");
      };
      list[0].focus();
      ok(list[0].matches(":focus"), "The first link is focused");
      arrowDown();
      ok(list[1].matches(":focus"), "The second link is focused");
      arrowDown();
      ok(list[2].matches(":focus"), "The third link is focused");
      arrowDown();
      ok(list[2].matches(":focus"), "The third link is still focused");
      arrowUp();
      ok(list[1].matches(":focus"), "The second link is focused");
      arrowUp();
      ok(list[0].matches(":focus"), "The first link is focused");
      arrowUp();
      ok(list[0].matches(":focus"), "The first link is still focused");
    }
  );
});

add_task(async function test_switch_before_closing() {
  clearHistory();

  const INITIAL_URL = "https://example.org/iwilldisappear";
  const FINAL_URL = "https://example.com/ishouldappear";
  await withFirefoxView({}, async function(browser) {
    let gBrowser = browser.getTabBrowser();
    let newTab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      INITIAL_URL
    );

    // Switch back to FxView:
    await BrowserTestUtils.switchTab(
      gBrowser,
      gBrowser.getTabForBrowser(browser)
    );

    // Update the tab we opened to a different site:
    let loadPromise = BrowserTestUtils.browserLoaded(
      newTab.linkedBrowser,
      null,
      FINAL_URL
    );
    BrowserTestUtils.loadURI(newTab.linkedBrowser, FINAL_URL);
    await loadPromise;

    // Close the added tab
    BrowserTestUtils.removeTab(newTab);

    const { document } = browser.contentWindow;
    const tabsList = document.querySelector("ol.closed-tabs-list");
    await BrowserTestUtils.waitForMutationCondition(
      tabsList,
      { childList: true },
      () => !!tabsList.children.length
    );
    info("A tab appeared in the list, ensure it has the right URL.");
    let urlBit = tabsList.firstElementChild.querySelector(".closed-tab-li-url");
    await BrowserTestUtils.waitForMutationCondition(
      urlBit,
      { characterData: true, attributeFilter: ["title"] },
      () => urlBit.textContent.includes(".com")
    );
    is(
      urlBit.textContent,
      "example.com",
      "Item should end up with the correct URL."
    );
  });
});
