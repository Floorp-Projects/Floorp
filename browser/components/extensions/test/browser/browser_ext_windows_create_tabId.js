/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function assertNoLeaksInTabTracker() {
  // Check that no tabs have been leaked by the internal tabTracker helper class.
  const {ExtensionParent} = ChromeUtils.import("resource://gre/modules/ExtensionParent.jsm", {});
  const {tabTracker} = ExtensionParent.apiManager.global;

  for (const [tabId, nativeTab] of tabTracker._tabIds) {
    if (!nativeTab.ownerGlobal) {
      ok(false, `A tab with tabId ${tabId} has been leaked in the tabTracker ("${nativeTab.title}")`);
    }
  }
}

add_task(async function testWindowCreate() {
  async function background() {
    let promiseTabAttached = () => {
      return new Promise(resolve => {
        browser.tabs.onAttached.addListener(function listener() {
          browser.tabs.onAttached.removeListener(listener);
          resolve();
        });
      });
    };

    let promiseTabUpdated = (expected) => {
      return new Promise(resolve => {
        browser.tabs.onUpdated.addListener(function listener(tabId, changeInfo, tab) {
          if (changeInfo.url === expected) {
            browser.tabs.onUpdated.removeListener(listener);
            resolve();
          }
        });
      });
    };

    try {
      let window = await browser.windows.getCurrent();
      let windowId = window.id;

      browser.test.log("Create additional tab in window 1");
      let tab = await browser.tabs.create({windowId, url: "about:blank"});
      let tabId = tab.id;

      browser.test.log("Create a new window, adopting the new tab");

      // Note that we want to check against actual boolean values for
      // all of the `incognito` property tests.
      browser.test.assertEq(false, tab.incognito, "Tab is not private");

      {
        let [, window] = await Promise.all([
          promiseTabAttached(),
          browser.windows.create({tabId: tabId}),
        ]);
        browser.test.assertEq(false, window.incognito, "New window is not private");
        browser.test.assertEq(tabId, window.tabs[0].id, "tabs property populated correctly");

        browser.test.log("Close the new window");
        await browser.windows.remove(window.id);
      }

      {
        browser.test.log("Create a new private window");
        let privateWindow = await browser.windows.create({incognito: true});
        browser.test.assertEq(true, privateWindow.incognito, "Private window is private");

        browser.test.log("Create additional tab in private window");
        let privateTab = await browser.tabs.create({windowId: privateWindow.id});
        browser.test.assertEq(true, privateTab.incognito, "Private tab is private");

        browser.test.log("Create a new window, adopting the new private tab");
        let [, newWindow] = await Promise.all([
          promiseTabAttached(),
          browser.windows.create({tabId: privateTab.id}),
        ]);
        browser.test.assertEq(true, newWindow.incognito, "New private window is private");

        browser.test.log("Close the new private window");
        await browser.windows.remove(newWindow.id);

        browser.test.log("Close the private window");
        await browser.windows.remove(privateWindow.id);
      }


      browser.test.log("Try to create a window with both a tab and a URL");
      [tab] = await browser.tabs.query({windowId, active: true});
      await browser.test.assertRejects(
        browser.windows.create({tabId: tab.id, url: "http://example.com/"}),
        /`tabId` may not be used in conjunction with `url`/,
        "Create call failed as expected");

      browser.test.log("Try to create a window with both a tab and an invalid incognito setting");
      await browser.test.assertRejects(
        browser.windows.create({tabId: tab.id, incognito: true}),
        /`incognito` property must match the incognito state of tab/,
        "Create call failed as expected");


      browser.test.log("Try to create a window with an invalid tabId");
      await browser.test.assertRejects(
        browser.windows.create({tabId: 0}),
        /Invalid tab ID: 0/,
        "Create call failed as expected");


      browser.test.log("Try to create a window with two URLs");
      let readyPromise = Promise.all([
        // tabs.onUpdated can be invoked between the call of windows.create and
        // the invocation of its callback/promise, so set up the listeners
        // before creating the window.
        promiseTabUpdated("http://example.com/"),
        promiseTabUpdated("http://example.org/"),
      ]);

      window = await browser.windows.create({url: ["http://example.com/", "http://example.org/"]});
      await readyPromise;

      browser.test.assertEq(2, window.tabs.length, "2 tabs were opened in new window");
      browser.test.assertEq("about:blank", window.tabs[0].url, "about:blank, page not loaded yet");
      browser.test.assertEq("about:blank", window.tabs[1].url, "about:blank, page not loaded yet");

      window = await browser.windows.get(window.id, {populate: true});

      browser.test.assertEq(2, window.tabs.length, "2 tabs were opened in new window");
      browser.test.assertEq("http://example.com/", window.tabs[0].url, "Correct URL was loaded in tab 1");
      browser.test.assertEq("http://example.org/", window.tabs[1].url, "Correct URL was loaded in tab 2");

      await browser.windows.remove(window.id);

      browser.test.notifyPass("window-create");
    } catch (e) {
      browser.test.fail(`${e} :: ${e.stack}`);
      browser.test.notifyFail("window-create");
    }
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],
    },

    background,
  });

  await extension.startup();
  await extension.awaitFinish("window-create");
  await extension.unload();

  assertNoLeaksInTabTracker();
});

add_task(async function testWebNavigationOnWindowCreateTabId() {
  async function background() {
    const webNavEvents = [];
    const onceTabsAttached = [];

    let promiseTabAttached = (tab) => {
      return new Promise(resolve => {
        browser.tabs.onAttached.addListener(function listener(tabId) {
          if (tabId !== tab.id) {
            return;
          }
          browser.tabs.onAttached.removeListener(listener);
          resolve();
        });
      });
    };

    // Listen to webNavigation.onCompleted events to ensure that
    // it is not going to be fired when we move the existent tabs
    // to new windows.
    browser.webNavigation.onCompleted.addListener((data) => {
      webNavEvents.push(data);
    });

    // Wait for the list of urls needed to select the test tabs,
    // and then move these tabs to a new window and assert that
    // no webNavigation.onCompleted events should be received
    // while the tabs are being adopted into the new windows.
    browser.test.onMessage.addListener(async (msg, testTabURLs) => {
      if (msg !== "testTabURLs") {
        return;
      }

      // Retrieve the tabs list and filter out the tabs that should
      // not be moved into a new window.
      let allTabs = await browser.tabs.query({});
      let testTabs = allTabs.filter(tab => {
        return testTabURLs.includes(tab.url);
      });

      browser.test.assertEq(2, testTabs.length, "Got the expected number of test tabs");

      for (let tab of testTabs) {
        onceTabsAttached.push(promiseTabAttached(tab));
        await browser.windows.create({tabId: tab.id});
      }

      // Wait the tabs to have been attached to the new window and then assert that no
      // webNavigation.onCompleted event has been received.
      browser.test.log("Waiting tabs move to new window to be attached");
      await Promise.all(onceTabsAttached);

      browser.test.assertEq("[]", JSON.stringify(webNavEvents),
                            "No webNavigation.onCompleted event should have been received");

      // Remove all the test tabs before exiting the test successfully.
      for (let tab of testTabs) {
        await browser.tabs.remove(tab.id);
      }

      browser.test.notifyPass("webNavigation-on-window-create-tabId");
    });
  }

  const testURLs = ["http://example.com/", "http://example.org/"];

  for (let url of testURLs) {
    await BrowserTestUtils.openNewForegroundTab(gBrowser, url);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs", "webNavigation"],
    },
    background,
  });

  await extension.startup();

  await extension.sendMessage("testTabURLs", testURLs);

  await extension.awaitFinish("webNavigation-on-window-create-tabId");
  await extension.unload();

  assertNoLeaksInTabTracker();
});

add_task(async function testGetLastFocusedDoesNotLeakDuringTabAdoption() {
  async function background() {
    const allTabs = await browser.tabs.query({});

    browser.test.onMessage.addListener(async (msg, testTabURL) => {
      if (msg !== "testTabURL") {
        return;
      }

      let tab = allTabs.filter(tab => tab.url === testTabURL).pop();

      // Keep calling getLastFocused while browser.windows.create is creating
      // a new window to adopt the test tab, so that the test recreates
      // conditions similar to the extension that has been triggered this leak
      // (See Bug 1458918 for a rationale).
      // The while loop is explicited exited right before the notifyPass
      // (but unloading the extension will stop it in any case).
      let stopGetLastFocusedLoop = false;
      Promise.resolve().then(async () => {
        while (!stopGetLastFocusedLoop) {
          browser.windows.getLastFocused({populate: true});
          // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
          await new Promise(resolve => setTimeout(resolve, 50));
        }
      });

      // Create a new window which adopt an existent tab and wait the tab to
      // be fully attached to the new window.
      await Promise.all([
        new Promise(resolve => {
          const listener = () => {
            browser.tabs.onAttached.removeListener(listener);
            resolve();
          };
          browser.tabs.onAttached.addListener(listener);
        }),
        browser.windows.create({tabId: tab.id}),
      ]);

      // Check that getLastFocused populate the tabs property once the tab adoption
      // has been completed.
      const lastFocusedPopulate = await browser.windows.getLastFocused({populate: true});
      browser.test.assertEq(1, lastFocusedPopulate.tabs.length,
                            "Got the expected number of tabs from windows.getLastFocused");

      // Remove the test tab.
      await browser.tabs.remove(tab.id);

      stopGetLastFocusedLoop = true;

      browser.test.notifyPass("tab-adopted");
    });
  }

  await BrowserTestUtils.openNewForegroundTab(gBrowser, "http://example.com/");

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs", "webNavigation"],
    },
    background,
  });

  await extension.startup();

  extension.sendMessage("testTabURL", "http://example.com/");

  await extension.awaitFinish("tab-adopted");

  await extension.unload();

  assertNoLeaksInTabTracker();
});
