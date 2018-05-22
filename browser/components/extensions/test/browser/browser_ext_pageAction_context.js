/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

Services.scriptloader.loadSubScript(new URL("head_pageAction.js", gTestPath).href,
                                    this);

add_task(async function testTabSwitchContext() {
  await runTests({
    manifest: {
      "name": "Foo Extension",

      "page_action": {
        "default_icon": "default.png",
        "default_popup": "__MSG_popup__",
        "default_title": "Default __MSG_title__ \u263a",
      },

      "default_locale": "en",

      "permissions": ["tabs"],
    },

    "files": {
      "_locales/en/messages.json": {
        "popup": {
          "message": "default.html",
          "description": "Popup",
        },

        "title": {
          "message": "Title",
          "description": "Title",
        },
      },

      "_locales/es_ES/messages.json": {
        "popup": {
          "message": "default.html",
          "description": "Popup",
        },

        "title": {
          "message": "T\u00edtulo",
          "description": "Title",
        },
      },

      "default.png": imageBuffer,
      "1.png": imageBuffer,
      "2.png": imageBuffer,
    },

    getTests: function(tabs) {
      let defaultIcon = "chrome://browser/content/extension.svg";
      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "popup": browser.runtime.getURL("default.html"),
         "title": "Default T\u00edtulo \u263a"},
        {"icon": browser.runtime.getURL("1.png"),
         "popup": browser.runtime.getURL("default.html"),
         "title": "Default T\u00edtulo \u263a"},
        {"icon": browser.runtime.getURL("2.png"),
         "popup": browser.runtime.getURL("2.html"),
         "title": "Title 2"},
        {"icon": defaultIcon,
         "popup": "",
         "title": ""},
      ];

      let promiseTabLoad = details => {
        return new Promise(resolve => {
          browser.tabs.onUpdated.addListener(function listener(tabId, changed) {
            if (tabId == details.id && changed.url == details.url) {
              browser.tabs.onUpdated.removeListener(listener);
              resolve();
            }
          });
        });
      };

      return [
        expect => {
          browser.test.log("Initial state. No icon visible.");
          expect(null);
        },
        async expect => {
          browser.test.log("Show the icon on the first tab, expect default properties.");
          await browser.pageAction.show(tabs[0]);
          expect(details[0]);
        },
        expect => {
          browser.test.log("Change the icon. Expect default properties excluding the icon.");
          browser.pageAction.setIcon({tabId: tabs[0], path: "1.png"});
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Create a new tab. No icon visible.");
          let tab = await browser.tabs.create({active: true, url: "about:blank?0"});
          tabs.push(tab.id);
          expect(null);
        },
        async expect => {
          browser.test.log("Await tab load. No icon visible.");
          let promise = promiseTabLoad({id: tabs[1], url: "about:blank?0"});
          let {url} = await browser.tabs.get(tabs[1]);
          if (url === "about:blank") {
            await promise;
          }
          expect(null);
        },
        async expect => {
          browser.test.log("Change properties. Expect new properties.");
          let tabId = tabs[1];
          await browser.pageAction.show(tabId);

          browser.pageAction.setIcon({tabId, path: "2.png"});
          browser.pageAction.setPopup({tabId, popup: "2.html"});
          browser.pageAction.setTitle({tabId, title: "Title 2"});

          expect(details[2]);
        },
        async expect => {
          browser.test.log("Change the hash. Expect same properties.");

          let promise = promiseTabLoad({id: tabs[1], url: "about:blank?0#ref"});
          browser.tabs.update(tabs[1], {url: "about:blank?0#ref"});
          await promise;

          expect(details[2]);
        },
        expect => {
          browser.test.log("Set empty string values. Expect empty strings but default icon.");
          browser.pageAction.setIcon({tabId: tabs[1], path: ""});
          browser.pageAction.setPopup({tabId: tabs[1], popup: ""});
          browser.pageAction.setTitle({tabId: tabs[1], title: ""});

          expect(details[3]);
        },
        expect => {
          browser.test.log("Clear the values. Expect default ones.");
          browser.pageAction.setIcon({tabId: tabs[1], path: null});
          browser.pageAction.setPopup({tabId: tabs[1], popup: null});
          browser.pageAction.setTitle({tabId: tabs[1], title: null});

          expect(details[0]);
        },
        async expect => {
          browser.test.log("Navigate to a new page. Expect icon hidden.");

          // TODO: This listener should not be necessary, but the |tabs.update|
          // callback currently fires too early in e10s windows.
          let promise = promiseTabLoad({id: tabs[1], url: "about:blank?1"});

          browser.tabs.update(tabs[1], {url: "about:blank?1"});

          await promise;
          expect(null);
        },
        async expect => {
          browser.test.log("Show the icon. Expect default properties again.");

          await browser.pageAction.show(tabs[1]);
          expect(details[0]);
        },
        async expect => {
          browser.test.log("Switch back to the first tab. Expect previously set properties.");
          await browser.tabs.update(tabs[0], {active: true});
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Hide the icon on tab 2. Switch back, expect hidden.");
          await browser.pageAction.hide(tabs[1]);

          await browser.tabs.update(tabs[1], {active: true});
          expect(null);
        },
        async expect => {
          browser.test.log("Switch back to tab 1. Expect previous results again.");
          await browser.tabs.remove(tabs[1]);
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Hide the icon. Expect hidden.");

          await browser.pageAction.hide(tabs[0]);
          expect(null);
        },
        async expect => {
          browser.test.assertRejects(
            browser.pageAction.setPopup({tabId: tabs[0], popup: "about:addons"}),
            /Access denied for URL about:addons/,
            "unable to set popup to about:addons");

          expect(null);
        },
      ];
    },
  });
});

add_task(async function testMultipleWindows() {
  await runTests({
    manifest: {
      "page_action": {
        "default_icon": "default.png",
        "default_popup": "default.html",
        "default_title": "Default Title",
      },
    },

    "files": {
      "default.png": imageBuffer,
      "tab.png": imageBuffer,
    },

    getTests: function(tabs, windows) {
      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "popup": browser.runtime.getURL("default.html"),
         "title": "Default Title"},
        {"icon": browser.runtime.getURL("tab.png"),
         "popup": browser.runtime.getURL("tab.html"),
         "title": "tab"},
      ];

      return [
        async expect => {
          browser.test.log("Create a new tab, expect hidden pageAction.");
          let tab = await browser.tabs.create({active: true});
          tabs.push(tab.id);
          expect(null);
        },
        async expect => {
          browser.test.log("Show the pageAction, expect default values.");
          await browser.pageAction.show(tabs[1]);
          expect(details[0]);
        },
        async expect => {
          browser.test.log("Set tab-specific values, expect them.");
          await browser.pageAction.setIcon({tabId: tabs[1], path: "tab.png"});
          await browser.pageAction.setPopup({tabId: tabs[1], popup: "tab.html"});
          await browser.pageAction.setTitle({tabId: tabs[1], title: "tab"});
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Open a new window, expect hidden pageAction.");
          let {id} = await browser.windows.create();
          windows.push(id);
          expect(null);
        },
        async expect => {
          browser.test.log("Move tab from old window to the new one, expect old values.");
          await browser.tabs.move(tabs[1], {windowId: windows[1], index: -1});
          await browser.tabs.update(tabs[1], {active: true});
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Close the new window and go back to the previous one.");
          await browser.windows.remove(windows[1]);
          expect(null);
        },
      ];
    },
  });
});

add_task(async function testNavigationClearsData() {
  let url = "http://example.com/";
  let default_title = "Default title";
  let tab_title = "Tab title";

  let {Management: {global: {tabTracker}}} = ChromeUtils.import("resource://gre/modules/Extension.jsm", {});
  let extension, tabs = [];
  async function addTab(...args) {
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, ...args);
    tabs.push(tab);
    return tab;
  }
  async function sendMessage(method, param, expect, msg) {
    extension.sendMessage({method, param, expect, msg});
    await extension.awaitMessage("done");
  }
  async function expectTabSpecificData(tab, msg) {
    let tabId = tabTracker.getId(tab);
    await sendMessage("isShown", {tabId}, true, msg);
    await sendMessage("getTitle", {tabId}, tab_title, msg);
  }
  async function expectDefaultData(tab, msg) {
    let tabId = tabTracker.getId(tab);
    await sendMessage("isShown", {tabId}, false, msg);
    await sendMessage("getTitle", {tabId}, default_title, msg);
  }
  async function setTabSpecificData(tab) {
    let tabId = tabTracker.getId(tab);
    await expectDefaultData(tab, "Expect default data before setting tab-specific data.");
    await sendMessage("show", tabId);
    await sendMessage("setTitle", {tabId, title: tab_title});
    await expectTabSpecificData(tab, "Expect tab-specific data after setting it.");
  }

  info("Load a tab before installing the extension");
  let tab1 = await addTab(url, true, true);

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      page_action: {default_title},
    },
    background: function() {
      browser.test.onMessage.addListener(async ({method, param, expect, msg}) => {
        let result = await browser.pageAction[method](param);
        if (expect !== undefined) {
          browser.test.assertEq(expect, result, msg);
        }
        browser.test.sendMessage("done");
      });
    },
  });
  await extension.startup();

  info("Set tab-specific data to the existing tab.");
  await setTabSpecificData(tab1);

  info("Add a hash. Does not cause navigation.");
  await navigateTab(tab1, url + "#hash");
  await expectTabSpecificData(tab1, "Adding a hash does not clear tab-specific data");

  info("Remove the hash. Causes navigation.");
  await navigateTab(tab1, url);
  await expectDefaultData(tab1, "Removing hash clears tab-specific data");

  info("Open a new tab, set tab-specific data to it.");
  let tab2 = await addTab("about:newtab", false, false);
  await setTabSpecificData(tab2);

  info("Load a page in that tab.");
  await navigateTab(tab2, url);
  await expectDefaultData(tab2, "Loading a page clears tab-specific data.");

  info("Set tab-specific data.");
  await setTabSpecificData(tab2);

  info("Push history state. Does not cause navigation.");
  await historyPushState(tab2, url + "/path");
  await expectTabSpecificData(tab2, "history.pushState() does not clear tab-specific data");

  info("Navigate when the tab is not selected");
  gBrowser.selectedTab = tab1;
  await navigateTab(tab2, url);
  await expectDefaultData(tab2, "Navigating clears tab-specific data, even when not selected.");

  for (let tab of tabs) {
    BrowserTestUtils.removeTab(tab);
  }
  await extension.unload();
});
