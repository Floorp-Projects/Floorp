/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyModuleGetters(globalThis, {
  SessionStore: "resource:///modules/sessionstore/SessionStore.jsm",
});

const URLs = [
  "http://mochi.test:8888/browser/",
  "http://www.example.com/",
  "http://example.net",
  "http://example.org",
];

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
      const closedObjectsChanged = TestUtils.topicObserved(
        "sessionstore-closed-objects-changed"
      );

      ok(
        document
          .querySelector("#collapsible-tabs-container")
          .classList.contains("empty-container"),
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
      await closedObjectsChanged;

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

      ok(
        document.querySelector("ol.closed-tabs-list").children.length === 1,
        "recently-closed-tabs-list should have one list item"
      );
    }
  );
});

add_task(async function test_list_ordering() {
  const existingData = SessionStore.getClosedTabCount(window);

  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: "about:firefoxview",
    },
    async browser => {
      const { document } = browser.contentWindow;
      const closedObjectsChanged = TestUtils.topicObserved(
        "sessionstore-closed-objects-changed"
      );

      const tab1 = await add_new_tab(URLs[0]);
      const tab2 = await add_new_tab(URLs[1]);
      const tab3 = await add_new_tab(URLs[2]);

      gBrowser.selectedTab = tab3;

      await close_tab(tab3);
      await closedObjectsChanged;

      await close_tab(tab2);
      await closedObjectsChanged;

      await close_tab(tab1);
      await closedObjectsChanged;

      const tabsList = document.querySelector("ol.closed-tabs-list");
      await BrowserTestUtils.waitForMutationCondition(
        tabsList,
        { childList: true },
        () => tabsList.children.length > 1
      );

      ok(
        document.querySelector("ol.closed-tabs-list").children.length ===
          3 + existingData,
        "recently-closed-tabs-list should have one list item"
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
    }
  );
});

add_task(async function test_max_list_items() {
  // the tabs opened from the previous test provide seed data
  const mockMaxTabsLength = SessionStore.getClosedTabCount(window);
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

      ok(
        document.querySelector("ol.closed-tabs-list").childNodes.length ===
          mockMaxTabsLength,
        `recently-closed-tabs-list should have ${mockMaxTabsLength} list items`
      );

      ok(
        document
          .querySelector("ol.closed-tabs-list")
          .firstChild.textContent.includes("about:firefoxview"),
        "first list item in recently-closed-tabs-list is from previous test (session store)"
      );

      const closedObjectsChanged = TestUtils.topicObserved(
        "sessionstore-closed-objects-changed"
      );
      // add another tab
      const tab = await add_new_tab(URLs[3]);
      await close_tab(tab);
      await closedObjectsChanged;

      ok(
        document
          .querySelector("ol.closed-tabs-list")
          .firstChild.textContent.includes("example.org"),
        "first list item in recently-closed-tabs-list should have been updated"
      );

      ok(
        document.querySelector("ol.closed-tabs-list").childNodes.length ===
          mockMaxTabsLength,
        `recently-closed-tabs-list should still have ${mockMaxTabsLength} list items`
      );
    }
  );
});
