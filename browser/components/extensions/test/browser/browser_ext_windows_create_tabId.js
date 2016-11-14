/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWindowCreate() {
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

      await new Promise(resolve => setTimeout(resolve, 0));

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

  yield extension.startup();
  yield extension.awaitFinish("window-create");
  yield extension.unload();
});
