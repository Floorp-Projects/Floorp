/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function runTests(options) {
  async function background(getTests) {
    async function checkDetails(expecting, tabId) {
      let title = await browser.browserAction.getTitle({tabId});
      browser.test.assertEq(expecting.title, title,
                            "expected value from getTitle");

      let popup = await browser.browserAction.getPopup({tabId});
      browser.test.assertEq(expecting.popup, popup,
                            "expected value from getPopup");

      let badge = await browser.browserAction.getBadgeText({tabId});
      browser.test.assertEq(expecting.badge, badge,
                            "expected value from getBadge");

      let badgeBackgroundColor = await browser.browserAction.getBadgeBackgroundColor({tabId});
      browser.test.assertEq(String(expecting.badgeBackgroundColor),
                            String(badgeBackgroundColor),
                            "expected value from getBadgeBackgroundColor");
    }

    let expectDefaults = expecting => {
      return checkDetails(expecting);
    };

    let tabs = [];
    let tests = getTests(tabs, expectDefaults);

    {
      let tabId = 0xdeadbeef;
      let calls = [
        () => browser.browserAction.enable(tabId),
        () => browser.browserAction.disable(tabId),
        () => browser.browserAction.setTitle({tabId, title: "foo"}),
        () => browser.browserAction.setIcon({tabId, path: "foo.png"}),
        () => browser.browserAction.setPopup({tabId, popup: "foo.html"}),
        () => browser.browserAction.setBadgeText({tabId, text: "foo"}),
        () => browser.browserAction.setBadgeBackgroundColor({tabId, color: [0xff, 0, 0, 0xff]}),
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

      nextTest();
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: options.manifest,

    files: options.files || {},

    background: `(${background})(${options.getTests})`,
  });

  let browserActionId;
  function checkDetails(details) {
    if (!browserActionId) {
      browserActionId = `${makeWidgetId(extension.id)}-browser-action`;
    }

    let button = document.getElementById(browserActionId);

    ok(button, "button exists");

    let title = details.title || options.manifest.name;

    is(getListStyleImage(button), details.icon, "icon URL is correct");
    is(button.getAttribute("tooltiptext"), title, "image title is correct");
    is(button.getAttribute("label"), title, "image label is correct");
    is(button.getAttribute("badge"), details.badge, "badge text is correct");
    is(button.getAttribute("disabled") == "true", Boolean(details.disabled), "disabled state is correct");

    if (details.badge && details.badgeBackgroundColor) {
      let badge = button.ownerDocument.getAnonymousElementByAttribute(
        button, "class", "toolbarbutton-badge");

      let badgeColor = window.getComputedStyle(badge).backgroundColor;
      let color = details.badgeBackgroundColor;
      let expectedColor = `rgb(${color[0]}, ${color[1]}, ${color[2]})`;

      is(badgeColor, expectedColor, "badge color is correct");
    }


    // TODO: Popup URL.
  }

  let awaitFinish = new Promise(resolve => {
    extension.onMessage("nextTest", async (expecting, testsRemaining) => {
      await promiseAnimationFrame();

      checkDetails(expecting);

      if (testsRemaining) {
        extension.sendMessage("runNextTest");
      } else {
        resolve();
      }
    });
  });

  await extension.startup();

  await awaitFinish;

  await extension.unload();
}

add_task(async function testTabSwitchContext() {
  await runTests({
    manifest: {
      "browser_action": {
        "default_icon": "default.png",
        "default_popup": "__MSG_popup__",
        "default_title": "Default __MSG_title__",
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

      "default.png": imageBuffer,
      "default-2.png": imageBuffer,
      "1.png": imageBuffer,
      "2.png": imageBuffer,
    },

    getTests: function(tabs, expectDefaults) {
      const DEFAULT_BADGE_COLOR = [0xd9, 0, 0, 255];

      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "popup": browser.runtime.getURL("default.html"),
         "title": "Default Title",
         "badge": "",
         "badgeBackgroundColor": DEFAULT_BADGE_COLOR},
        {"icon": browser.runtime.getURL("1.png"),
         "popup": browser.runtime.getURL("default.html"),
         "title": "Default Title",
         "badge": "",
         "badgeBackgroundColor": DEFAULT_BADGE_COLOR},
        {"icon": browser.runtime.getURL("2.png"),
         "popup": browser.runtime.getURL("2.html"),
         "title": "Title 2",
         "badge": "2",
         "badgeBackgroundColor": [0xff, 0, 0, 0xff],
         "disabled": true},
        {"icon": browser.runtime.getURL("1.png"),
         "popup": browser.runtime.getURL("default-2.html"),
         "title": "Default Title 2",
         "badge": "d2",
         "badgeBackgroundColor": [0, 0xff, 0, 0xff],
         "disabled": true},
        {"icon": browser.runtime.getURL("1.png"),
         "popup": browser.runtime.getURL("default-2.html"),
         "title": "Default Title 2",
         "badge": "d2",
         "badgeBackgroundColor": [0, 0xff, 0, 0xff],
         "disabled": false},
        {"icon": browser.runtime.getURL("default-2.png"),
         "popup": browser.runtime.getURL("default-2.html"),
         "title": "Default Title 2",
         "badge": "d2",
         "badgeBackgroundColor": [0, 0xff, 0, 0xff]},
      ];

      return [
        async expect => {
          browser.test.log("Initial state, expect default properties.");

          await expectDefaults(details[0]);
          expect(details[0]);
        },
        async expect => {
          browser.test.log("Change the icon in the current tab. Expect default properties excluding the icon.");
          browser.browserAction.setIcon({tabId: tabs[0], path: "1.png"});

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
          browser.browserAction.setIcon({tabId, path: "2.png"});
          browser.browserAction.setPopup({tabId, popup: "2.html"});
          browser.browserAction.setTitle({tabId, title: "Title 2"});
          browser.browserAction.setBadgeText({tabId, text: "2"});
          browser.browserAction.setBadgeBackgroundColor({tabId, color: "#ff0000"});
          browser.browserAction.disable(tabId);

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
          browser.browserAction.setIcon({path: "default-2.png"});
          browser.browserAction.setPopup({popup: "default-2.html"});
          browser.browserAction.setTitle({title: "Default Title 2"});
          browser.browserAction.setBadgeText({text: "d2"});
          browser.browserAction.setBadgeBackgroundColor({color: [0, 0xff, 0, 0xff]});
          browser.browserAction.disable();

          await expectDefaults(details[3]);
          expect(details[3]);
        },
        async expect => {
          browser.test.log("Re-enable by default. Expect enabled.");
          browser.browserAction.enable();

          await expectDefaults(details[4]);
          expect(details[4]);
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
      ];
    },
  });
});

add_task(async function testDefaultTitle() {
  await runTests({
    manifest: {
      "name": "Foo Extension",

      "browser_action": {
        "default_icon": "icon.png",
      },

      "permissions": ["tabs"],
    },

    files: {
      "icon.png": imageBuffer,
    },

    getTests: function(tabs, expectDefaults) {
      const DEFAULT_BADGE_COLOR = [0xd9, 0, 0, 255];

      let details = [
        {"title": "Foo Extension",
         "popup": "",
         "badge": "",
         "badgeBackgroundColor": DEFAULT_BADGE_COLOR,
         "icon": browser.runtime.getURL("icon.png")},
        {"title": "Foo Title",
         "popup": "",
         "badge": "",
         "badgeBackgroundColor": DEFAULT_BADGE_COLOR,
         "icon": browser.runtime.getURL("icon.png")},
        {"title": "Bar Title",
         "popup": "",
         "badge": "",
         "badgeBackgroundColor": DEFAULT_BADGE_COLOR,
         "icon": browser.runtime.getURL("icon.png")},
        {"title": "",
         "popup": "",
         "badge": "",
         "badgeBackgroundColor": DEFAULT_BADGE_COLOR,
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
          browser.browserAction.setTitle({tabId: tabs[0], title: "Foo Title"});

          await expectDefaults(details[0]);
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Change the default. Expect same properties.");
          browser.browserAction.setTitle({title: "Bar Title"});

          await expectDefaults(details[2]);
          expect(details[1]);
        },
        async expect => {
          browser.test.log("Clear the title. Expect new default title.");
          browser.browserAction.setTitle({tabId: tabs[0], title: ""});

          await expectDefaults(details[2]);
          expect(details[2]);
        },
        async expect => {
          browser.test.log("Set default title to null string. Expect null string from API, extension title in UI.");
          browser.browserAction.setTitle({title: ""});

          await expectDefaults(details[3]);
          expect(details[3]);
        },
        async expect => {
          browser.test.assertRejects(
            browser.browserAction.setPopup({popup: "about:addons"}),
            /Access denied for URL about:addons/,
            "unable to set popup to about:addons");

          await expectDefaults(details[3]);
          expect(details[3]);
        },
      ];
    },
  });
});
