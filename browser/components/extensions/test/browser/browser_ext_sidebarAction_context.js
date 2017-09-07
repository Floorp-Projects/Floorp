/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
Cu.import("resource://gre/modules/Services.jsm");

SpecialPowers.pushPrefEnv({
  // Ignore toolbarbutton stuff, other test covers it.
  set: [["extensions.sidebar-button.shown", true]],
});

async function runTests(options) {
  async function background(getTests) {
    async function checkDetails(expecting, tabId) {
      let title = await browser.sidebarAction.getTitle({tabId});
      browser.test.assertEq(expecting.title, title,
                            "expected value from getTitle");

      let panel = await browser.sidebarAction.getPanel({tabId});
      browser.test.assertEq(expecting.panel, panel,
                            "expected value from getPanel");
    }

    let expectDefaults = expecting => {
      return checkDetails(expecting);
    };

    let tabs = [];
    let tests = getTests(tabs, expectDefaults);

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

      test(async expecting => {
        // Check that the API returns the expected values, and then
        // run the next test.
        let tabs = await browser.tabs.query({active: true, currentWindow: true});
        await checkDetails(expecting, tabs[0].id);

        // Check that the actual icon has the expected values, then
        // run the next test.
        browser.test.sendMessage("nextTest", expecting, tests.length);
      });
    }

    browser.test.onMessage.addListener((msg) => {
      if (msg != "runNextTest") {
        browser.test.fail("Expecting 'runNextTest' message");
      }

      nextTest();
    });

    browser.tabs.query({active: true, currentWindow: true}, resultTabs => {
      tabs[0] = resultTabs[0].id;
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: options.manifest,
    useAddonManager: "temporary",

    files: options.files || {},

    background: `(${background})(${options.getTests})`,
  });

  let sidebarActionId;
  function checkDetails(details) {
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
    extension.onMessage("nextTest", (expecting, testsRemaining) => {
      checkDetails(expecting);

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
      "default-2.html": sidebar,
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
      "default-2.png": imageBuffer,
      "1.png": imageBuffer,
      "2.png": imageBuffer,
    },

    getTests: function(tabs, expectDefaults) {
      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "panel": browser.runtime.getURL("default.html"),
         "title": "Default Title",
        },
        {"icon": browser.runtime.getURL("1.png"),
         "panel": browser.runtime.getURL("default.html"),
         "title": "Default Title",
        },
        {"icon": browser.runtime.getURL("2.png"),
         "panel": browser.runtime.getURL("2.html"),
         "title": "Title 2",
        },
        {"icon": browser.runtime.getURL("1.png"),
         "panel": browser.runtime.getURL("default-2.html"),
         "title": "Default Title 2",
        },
        {"icon": browser.runtime.getURL("1.png"),
         "panel": browser.runtime.getURL("default-2.html"),
         "title": "Default Title 2",
        },
        {"icon": browser.runtime.getURL("default-2.png"),
         "panel": browser.runtime.getURL("default-2.html"),
         "title": "Default Title 2",
        },
        {"icon": browser.runtime.getURL("1.png"),
         "panel": browser.runtime.getURL("2.html"),
         "title": "Default Title 2",
        },
      ];

      return [
        async expect => {
          browser.test.log("Initial state, expect default properties.");

          await expectDefaults(details[0]);
          expect(details[0]);
        },
        async expect => {
          browser.test.log("Change the icon in the current tab. Expect default properties excluding the icon.");
          await browser.sidebarAction.setIcon({tabId: tabs[0], path: "1.png"});

          await expectDefaults(details[0]);
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Create a new tab. Expect default properties.");
          let tab = await browser.tabs.create({active: true, url: "about:blank?0"});
          tabs.push(tab.id);

          await expectDefaults(details[0]);
          expect(details[0]);
        },
        async expect => {
          browser.test.log("Change properties. Expect new properties.");
          let tabId = tabs[1];
          await Promise.all([
            browser.sidebarAction.setIcon({tabId, path: "2.png"}),
            browser.sidebarAction.setPanel({tabId, panel: "2.html"}),
            browser.sidebarAction.setTitle({tabId, title: "Title 2"}),
          ]);
          await expectDefaults(details[0]);
          expect(details[2]);
        },
        expect => {
          browser.test.log("Navigate to a new page. Expect no changes.");

          // TODO: This listener should not be necessary, but the |tabs.update|
          // callback currently fires too early in e10s windows.
          browser.tabs.onUpdated.addListener(function listener(tabId, changed) {
            if (tabId == tabs[1] && changed.url) {
              browser.tabs.onUpdated.removeListener(listener);
              expect(details[2]);
            }
          });

          browser.tabs.update(tabs[1], {url: "about:blank?1"});
        },
        async expect => {
          browser.test.log("Switch back to the first tab. Expect previously set properties.");
          await browser.tabs.update(tabs[0], {active: true});
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Change default values, expect those changes reflected.");
          await Promise.all([
            browser.sidebarAction.setIcon({path: "default-2.png"}),
            browser.sidebarAction.setPanel({panel: "default-2.html"}),
            browser.sidebarAction.setTitle({title: "Default Title 2"}),
          ]);

          await expectDefaults(details[3]);
          expect(details[3]);
        },
        async expect => {
          browser.test.log("Switch back to tab 2. Expect former value, unaffected by changes to defaults in previous step.");
          await browser.tabs.update(tabs[1], {active: true});

          await expectDefaults(details[3]);
          expect(details[2]);
        },
        async expect => {
          browser.test.log("Delete tab, switch back to tab 1. Expect previous results again.");
          await browser.tabs.remove(tabs[1]);
          expect(details[4]);
        },
        async expect => {
          browser.test.log("Create a new tab. Expect new default properties.");
          let tab = await browser.tabs.create({active: true, url: "about:blank?2"});
          tabs.push(tab.id);
          expect(details[5]);
        },
        async expect => {
          browser.test.log("Delete tab.");
          await browser.tabs.remove(tabs[2]);
          expect(details[4]);
        },
        async expect => {
          browser.test.log("Change tab panel.");
          let tabId = tabs[0];
          await browser.sidebarAction.setPanel({tabId, panel: "2.html"});
          expect(details[6]);
        },
        async expect => {
          browser.test.log("Revert tab panel.");
          let tabId = tabs[0];
          await browser.sidebarAction.setPanel({tabId, panel: ""});
          expect(details[4]);
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

    getTests: function(tabs, expectDefaults) {
      let details = [
        {"title": "Foo Extension",
         "panel": browser.runtime.getURL("sidebar.html"),
         "icon": browser.runtime.getURL("icon.png")},
        {"title": "Foo Title",
         "panel": browser.runtime.getURL("sidebar.html"),
         "icon": browser.runtime.getURL("icon.png")},
        {"title": "Bar Title",
         "panel": browser.runtime.getURL("sidebar.html"),
         "icon": browser.runtime.getURL("icon.png")},
        {"title": "",
         "panel": browser.runtime.getURL("sidebar.html"),
         "icon": browser.runtime.getURL("icon.png")},
      ];

      return [
        async expect => {
          browser.test.log("Initial state. Expect extension title as default title.");

          await expectDefaults(details[0]);
          expect(details[0]);
        },
        async expect => {
          browser.test.log("Change the title. Expect new title.");
          browser.sidebarAction.setTitle({tabId: tabs[0], title: "Foo Title"});

          await expectDefaults(details[0]);
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Change the default. Expect same properties.");
          browser.sidebarAction.setTitle({title: "Bar Title"});

          await expectDefaults(details[2]);
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Clear the title. Expect new default title.");
          browser.sidebarAction.setTitle({tabId: tabs[0], title: ""});

          await expectDefaults(details[2]);
          expect(details[2]);
        },
        async expect => {
          browser.test.log("Set default title to null string. Expect null string from API, extension title in UI.");
          browser.sidebarAction.setTitle({title: ""});

          await expectDefaults(details[3]);
          expect(details[3]);
        },
        async expect => {
          browser.test.assertRejects(
            browser.sidebarAction.setPanel({panel: "about:addons"}),
            /Access denied for URL about:addons/,
            "unable to set panel to about:addons");

          await expectDefaults(details[3]);
          expect(details[3]);
        },
      ];
    },
  });
});
