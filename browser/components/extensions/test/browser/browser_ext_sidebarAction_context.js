/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.import("resource://gre/modules/Services.jsm");

SpecialPowers.pushPrefEnv({
  // Ignore toolbarbutton stuff, other test covers it.
  set: [["extensions.sidebar-button.shown", true]],
});

async function runTests(options) {
  async function background(getTests) {
    async function checkDetails(expecting, details) {
      let title = await browser.sidebarAction.getTitle(details);
      browser.test.assertEq(expecting.title, title,
                            "expected value from getTitle in " + JSON.stringify(details));

      let panel = await browser.sidebarAction.getPanel(details);
      browser.test.assertEq(expecting.panel, panel,
                            "expected value from getPanel in " + JSON.stringify(details));
    }

    let tabs = [];
    let windows = [];
    let tests = getTests(tabs, windows);

    {
      let tabId = 0xdeadbeef;
      let calls = [
        () => browser.sidebarAction.setTitle({tabId, title: "foo"}),
        () => browser.sidebarAction.setIcon({tabId, path: "foo.png"}),
        () => browser.sidebarAction.setPanel({tabId, panel: "foo.html"}),
      ];

      for (let call of calls) {
        await browser.test.assertRejects(
          new Promise(resolve => resolve(call())),
          RegExp(`Invalid tab ID: ${tabId}`),
          "Expected invalid tab ID error");
      }
    }

    // Runs the next test in the `tests` array, checks the results,
    // and passes control back to the outer test scope.
    function nextTest() {
      let test = tests.shift();

      test(async (expectTab, expectWindow, expectGlobal, expectDefault) => {
        expectGlobal = {...expectDefault, ...expectGlobal};
        expectWindow = {...expectGlobal, ...expectWindow};
        expectTab = {...expectWindow, ...expectTab};

        // Check that the API returns the expected values, and then
        // run the next test.
        let [{windowId, id: tabId}] = await browser.tabs.query({active: true, currentWindow: true});
        await checkDetails(expectTab, {tabId});
        await checkDetails(expectWindow, {windowId});
        await checkDetails(expectGlobal, {});

        // Check that the actual icon has the expected values, then
        // run the next test.
        browser.test.sendMessage("nextTest", expectTab, windowId, tests.length);
      });
    }

    browser.test.onMessage.addListener((msg) => {
      if (msg != "runNextTest") {
        browser.test.fail("Expecting 'runNextTest' message");
      }

      nextTest();
    });

    let [{id, windowId}] = await browser.tabs.query({active: true, currentWindow: true});
    tabs.push(id);
    windows.push(windowId);
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: options.manifest,
    useAddonManager: "temporary",

    files: options.files || {},

    background: `(${background})(${options.getTests})`,
  });

  let sidebarActionId;
  function checkDetails(details, windowId) {
    let {document} = Services.wm.getOuterWindowWithId(windowId);
    if (!sidebarActionId) {
      sidebarActionId = `${makeWidgetId(extension.id)}-sidebar-action`;
    }

    let command = document.getElementById(sidebarActionId);
    ok(command, "command exists");

    let menuId = `menu_${sidebarActionId}`;
    let menu = document.getElementById(menuId);
    ok(menu, "menu exists");

    let title = details.title || options.manifest.name;

    is(getListStyleImage(menu), details.icon, "icon URL is correct");
    is(menu.getAttribute("label"), title, "image label is correct");
  }

  let awaitFinish = new Promise(resolve => {
    extension.onMessage("nextTest", (expecting, windowId, testsRemaining) => {
      checkDetails(expecting, windowId);

      if (testsRemaining) {
        extension.sendMessage("runNextTest");
      } else {
        resolve();
      }
    });
  });

  // Wait for initial sidebar load to start tests.
  SidebarUI.browser.addEventListener("load", event => {
    extension.sendMessage("runNextTest");
  }, {capture: true, once: true});

  await extension.startup();
  await awaitFinish;
  await extension.unload();
}

let sidebar = `
  <!DOCTYPE html>
  <html>
  <head><meta charset="utf-8"/></head>
  <body>
  A Test Sidebar
  </body></html>
`;

add_task(async function testTabSwitchContext() {
  await runTests({
    manifest: {
      "sidebar_action": {
        "default_icon": "default.png",
        "default_panel": "__MSG_panel__",
        "default_title": "Default __MSG_title__",
      },

      "default_locale": "en",

      "permissions": ["tabs"],
    },

    "files": {
      "default.html": sidebar,
      "global.html": sidebar,
      "2.html": sidebar,

      "_locales/en/messages.json": {
        "panel": {
          "message": "default.html",
          "description": "Panel",
        },

        "title": {
          "message": "Title",
          "description": "Title",
        },
      },

      "default.png": imageBuffer,
      "global.png": imageBuffer,
      "1.png": imageBuffer,
      "2.png": imageBuffer,
    },

    getTests: function(tabs) {
      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "panel": browser.runtime.getURL("default.html"),
         "title": "Default Title",
        },
        {"icon": browser.runtime.getURL("1.png"),
        },
        {"icon": browser.runtime.getURL("2.png"),
         "panel": browser.runtime.getURL("2.html"),
         "title": "Title 2",
        },
        {"icon": browser.runtime.getURL("global.png"),
         "panel": browser.runtime.getURL("global.html"),
         "title": "Global Title",
        },
        {"icon": browser.runtime.getURL("1.png"),
         "panel": browser.runtime.getURL("2.html"),
        },
      ];

      return [
        async expect => {
          browser.test.log("Initial state, expect default properties.");

          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change the icon in the current tab. Expect default properties excluding the icon.");
          await browser.sidebarAction.setIcon({tabId: tabs[0], path: "1.png"});

          expect(details[1], null, null, details[0]);
        },
        async expect => {
          browser.test.log("Create a new tab. Expect default properties.");
          let tab = await browser.tabs.create({active: true, url: "about:blank?0"});
          tabs.push(tab.id);

          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change properties. Expect new properties.");
          let tabId = tabs[1];
          await Promise.all([
            browser.sidebarAction.setIcon({tabId, path: "2.png"}),
            browser.sidebarAction.setPanel({tabId, panel: "2.html"}),
            browser.sidebarAction.setTitle({tabId, title: "Title 2"}),
          ]);
          expect(details[2], null, null, details[0]);
        },
        expect => {
          browser.test.log("Navigate to a new page. Expect no changes.");

          // TODO: This listener should not be necessary, but the |tabs.update|
          // callback currently fires too early in e10s windows.
          browser.tabs.onUpdated.addListener(function listener(tabId, changed) {
            if (tabId == tabs[1] && changed.url) {
              browser.tabs.onUpdated.removeListener(listener);
              expect(details[2], null, null, details[0]);
            }
          });

          browser.tabs.update(tabs[1], {url: "about:blank?1"});
        },
        async expect => {
          browser.test.log("Switch back to the first tab. Expect previously set properties.");
          await browser.tabs.update(tabs[0], {active: true});
          expect(details[1], null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change global values, expect those changes reflected.");
          await Promise.all([
            browser.sidebarAction.setIcon({path: "global.png"}),
            browser.sidebarAction.setPanel({panel: "global.html"}),
            browser.sidebarAction.setTitle({title: "Global Title"}),
          ]);

          expect(details[1], null, details[3], details[0]);
        },
        async expect => {
          browser.test.log("Switch back to tab 2. Expect former tab values, and new global values from previous step.");
          await browser.tabs.update(tabs[1], {active: true});

          expect(details[2], null, details[3], details[0]);
        },
        async expect => {
          browser.test.log("Delete tab, switch back to tab 1. Expect previous results again.");
          await browser.tabs.remove(tabs[1]);
          expect(details[1], null, details[3], details[0]);
        },
        async expect => {
          browser.test.log("Create a new tab. Expect new global properties.");
          let tab = await browser.tabs.create({active: true, url: "about:blank?2"});
          tabs.push(tab.id);
          expect(null, null, details[3], details[0]);
        },
        async expect => {
          browser.test.log("Delete tab.");
          await browser.tabs.remove(tabs[2]);
          expect(details[1], null, details[3], details[0]);
        },
        async expect => {
          browser.test.log("Change tab panel.");
          let tabId = tabs[0];
          await browser.sidebarAction.setPanel({tabId, panel: "2.html"});
          expect(details[4], null, details[3], details[0]);
        },
        async expect => {
          browser.test.log("Revert tab panel.");
          let tabId = tabs[0];
          await browser.sidebarAction.setPanel({tabId, panel: null});
          expect(details[1], null, details[3], details[0]);
        },
      ];
    },
  });
});

add_task(async function testDefaultTitle() {
  await runTests({
    manifest: {
      "name": "Foo Extension",

      "sidebar_action": {
        "default_icon": "icon.png",
        "default_panel": "sidebar.html",
      },

      "permissions": ["tabs"],
    },

    files: {
      "sidebar.html": sidebar,
      "icon.png": imageBuffer,
    },

    getTests: function(tabs) {
      let details = [
        {"title": "Foo Extension",
         "panel": browser.runtime.getURL("sidebar.html"),
         "icon": browser.runtime.getURL("icon.png")},
        {"title": "Foo Title"},
        {"title": "Bar Title"},
      ];

      return [
        async expect => {
          browser.test.log("Initial state. Expect default extension title.");

          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change the tab title. Expect new title.");
          browser.sidebarAction.setTitle({tabId: tabs[0], title: "Foo Title"});

          expect(details[1], null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change the global title. Expect same properties.");
          browser.sidebarAction.setTitle({title: "Bar Title"});

          expect(details[1], null, details[2], details[0]);
        },
        async expect => {
          browser.test.log("Clear the tab title. Expect new global title.");
          browser.sidebarAction.setTitle({tabId: tabs[0], title: null});

          expect(null, null, details[2], details[0]);
        },
        async expect => {
          browser.test.log("Clear the global title. Expect default title.");
          browser.sidebarAction.setTitle({title: null});

          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.assertRejects(
            browser.sidebarAction.setPanel({panel: "about:addons"}),
            /Access denied for URL about:addons/,
            "unable to set panel to about:addons");

          expect(null, null, null, details[0]);
        },
      ];
    },
  });
});

add_task(async function testPropertyRemoval() {
  await runTests({
    manifest: {
      "name": "Foo Extension",

      "sidebar_action": {
        "default_icon": "default.png",
        "default_panel": "default.html",
        "default_title": "Default Title",
      },

      "permissions": ["tabs"],
    },

    files: {
      "default.html": sidebar,
      "global.html": sidebar,
      "global2.html": sidebar,
      "window.html": sidebar,
      "tab.html": sidebar,
      "default.png": imageBuffer,
      "global.png": imageBuffer,
      "global2.png": imageBuffer,
      "window.png": imageBuffer,
      "tab.png": imageBuffer,
    },

    getTests: function(tabs, windows) {
      let defaultIcon = "chrome://browser/content/extension.svg";
      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "panel": browser.runtime.getURL("default.html"),
         "title": "Default Title"},
        {"icon": browser.runtime.getURL("global.png"),
         "panel": browser.runtime.getURL("global.html"),
         "title": "global"},
        {"icon": browser.runtime.getURL("window.png"),
         "panel": browser.runtime.getURL("window.html"),
         "title": "window"},
        {"icon": browser.runtime.getURL("tab.png"),
         "panel": browser.runtime.getURL("tab.html"),
         "title": "tab"},
        {"icon": defaultIcon,
         "title": ""},
        {"icon": browser.runtime.getURL("global2.png"),
         "panel": browser.runtime.getURL("global2.html"),
         "title": "global2"},
      ];

      return [
        async expect => {
          browser.test.log("Initial state, expect default properties.");
          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Set global values, expect the new values.");
          browser.sidebarAction.setIcon({path: "global.png"});
          browser.sidebarAction.setPanel({panel: "global.html"});
          browser.sidebarAction.setTitle({title: "global"});
          expect(null, null, details[1], details[0]);
        },
        async expect => {
          browser.test.log("Set window values, expect the new values.");
          let windowId = windows[0];
          browser.sidebarAction.setIcon({windowId, path: "window.png"});
          browser.sidebarAction.setPanel({windowId, panel: "window.html"});
          browser.sidebarAction.setTitle({windowId, title: "window"});
          expect(null, details[2], details[1], details[0]);
        },
        async expect => {
          browser.test.log("Set tab values, expect the new values.");
          let tabId = tabs[0];
          browser.sidebarAction.setIcon({tabId, path: "tab.png"});
          browser.sidebarAction.setPanel({tabId, panel: "tab.html"});
          browser.sidebarAction.setTitle({tabId, title: "tab"});
          expect(details[3], details[2], details[1], details[0]);
        },
        async expect => {
          browser.test.log("Set empty tab values.");
          let tabId = tabs[0];
          browser.sidebarAction.setIcon({tabId, path: ""});
          browser.sidebarAction.setPanel({tabId, panel: ""});
          browser.sidebarAction.setTitle({tabId, title: ""});
          expect(details[4], details[2], details[1], details[0]);
        },
        async expect => {
          browser.test.log("Remove tab values, expect window values.");
          let tabId = tabs[0];
          browser.sidebarAction.setIcon({tabId, path: null});
          browser.sidebarAction.setPanel({tabId, panel: null});
          browser.sidebarAction.setTitle({tabId, title: null});
          expect(null, details[2], details[1], details[0]);
        },
        async expect => {
          browser.test.log("Remove window values, expect global values.");
          let windowId = windows[0];
          browser.sidebarAction.setIcon({windowId, path: null});
          browser.sidebarAction.setPanel({windowId, panel: null});
          browser.sidebarAction.setTitle({windowId, title: null});
          expect(null, null, details[1], details[0]);
        },
        async expect => {
          browser.test.log("Change global values, expect the new values.");
          browser.sidebarAction.setIcon({path: "global2.png"});
          browser.sidebarAction.setPanel({panel: "global2.html"});
          browser.sidebarAction.setTitle({title: "global2"});
          expect(null, null, details[5], details[0]);
        },
        async expect => {
          browser.test.log("Remove global values, expect defaults.");
          browser.sidebarAction.setIcon({path: null});
          browser.sidebarAction.setPanel({panel: null});
          browser.sidebarAction.setTitle({title: null});
          expect(null, null, null, details[0]);
        },
      ];
    },
  });
});

add_task(async function testMultipleWindows() {
  await runTests({
    manifest: {
      "name": "Foo Extension",

      "sidebar_action": {
        "default_icon": "default.png",
        "default_panel": "default.html",
        "default_title": "Default Title",
      },

      "permissions": ["tabs"],
    },

    files: {
      "default.html": sidebar,
      "window1.html": sidebar,
      "window2.html": sidebar,
      "default.png": imageBuffer,
      "window1.png": imageBuffer,
      "window2.png": imageBuffer,
    },

    getTests: function(tabs, windows) {
      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "panel": browser.runtime.getURL("default.html"),
         "title": "Default Title"},
        {"icon": browser.runtime.getURL("window1.png"),
         "panel": browser.runtime.getURL("window1.html"),
         "title": "window1"},
        {"icon": browser.runtime.getURL("window2.png"),
         "panel": browser.runtime.getURL("window2.html"),
         "title": "window2"},
        {"title": "tab"},
      ];

      return [
        async expect => {
          browser.test.log("Initial state, expect default properties.");
          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Set window values, expect the new values.");
          let windowId = windows[0];
          browser.sidebarAction.setIcon({windowId, path: "window1.png"});
          browser.sidebarAction.setPanel({windowId, panel: "window1.html"});
          browser.sidebarAction.setTitle({windowId, title: "window1"});
          expect(null, details[1], null, details[0]);
        },
        async expect => {
          browser.test.log("Create a new tab, expect window values.");
          let tab = await browser.tabs.create({active: true});
          tabs.push(tab.id);
          expect(null, details[1], null, details[0]);
        },
        async expect => {
          browser.test.log("Set a tab title, expect it.");
          await browser.sidebarAction.setTitle({tabId: tabs[1], title: "tab"});
          expect(details[3], details[1], null, details[0]);
        },
        async expect => {
          browser.test.log("Open a new window, expect default values.");
          let {id} = await browser.windows.create();
          windows.push(id);
          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Set window values, expect the new values.");
          let windowId = windows[1];
          browser.sidebarAction.setIcon({windowId, path: "window2.png"});
          browser.sidebarAction.setPanel({windowId, panel: "window2.html"});
          browser.sidebarAction.setTitle({windowId, title: "window2"});
          expect(null, details[2], null, details[0]);
        },
        async expect => {
          browser.test.log("Move tab from old window to the new one. Tab-specific data"
            + " is preserved but inheritance is from the new window");
          await browser.tabs.move(tabs[1], {windowId: windows[1], index: -1});
          await browser.tabs.update(tabs[1], {active: true});
          expect(details[3], details[2], null, details[0]);
        },
        async expect => {
          browser.test.log("Close the initial tab of the new window.");
          let [{id}] = await browser.tabs.query({windowId: windows[1], index: 0});
          await browser.tabs.remove(id);
          expect(details[3], details[2], null, details[0]);
        },
        async expect => {
          browser.test.log("Move the previous tab to a 3rd window, the 2nd one will close.");
          await browser.windows.create({tabId: tabs[1]});
          expect(details[3], null, null, details[0]);
        },
        async expect => {
          browser.test.log("Close the tab, go back to the 1st window.");
          await browser.tabs.remove(tabs[1]);
          expect(null, details[1], null, details[0]);
        },
        async expect => {
          browser.test.log("Assert failures for bad parameters. Expect no change");

          let calls = {
            setIcon: {path: "default.png"},
            setPanel: {panel: "default.html"},
            setTitle: {title: "Default Title"},
            getPanel: {},
            getTitle: {},
          };
          for (let [method, arg] of Object.entries(calls)) {
            browser.test.assertThrows(
              () => browser.sidebarAction[method]({...arg, windowId: -3}),
              /-3 is too small \(must be at least -2\)/,
              method + " with invalid windowId",
            );
            await browser.test.assertRejects(
              browser.sidebarAction[method]({...arg, tabId: tabs[0], windowId: windows[0]}),
              /Only one of tabId and windowId can be specified/,
              method + " with both tabId and windowId",
            );
          }

          expect(null, details[1], null, details[0]);
        },
      ];
    },
  });
});
