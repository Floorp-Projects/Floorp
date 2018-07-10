/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

async function runTests(options) {
  async function background(getTests) {
    async function checkDetails(expecting, details) {
      let title = await browser.browserAction.getTitle(details);
      browser.test.assertEq(expecting.title, title,
                            "expected value from getTitle");

      let popup = await browser.browserAction.getPopup(details);
      browser.test.assertEq(expecting.popup, popup,
                            "expected value from getPopup");

      let badge = await browser.browserAction.getBadgeText(details);
      browser.test.assertEq(expecting.badge, badge,
                            "expected value from getBadge");

      let badgeBackgroundColor = await browser.browserAction.getBadgeBackgroundColor(details);
      browser.test.assertEq(String(expecting.badgeBackgroundColor),
                            String(badgeBackgroundColor),
                            "expected value from getBadgeBackgroundColor");

      let badgeTextColor = await browser.browserAction.getBadgeTextColor(details);
      browser.test.assertEq(String(expecting.badgeTextColor),
                            String(badgeTextColor),
                            "expected value from getBadgeTextColor");

      let enabled = await browser.browserAction.isEnabled(details);
      browser.test.assertEq(expecting.enabled, enabled,
                            "expected value from isEnabled");
    }

    let tabs = [];
    let windows = [];
    let tests = getTests(tabs, windows);

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
        () => browser.browserAction.setBadgeTextColor({tabId, color: [0, 0xff, 0xff, 0xff]}),
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
    nextTest();
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: options.manifest,

    files: options.files || {},

    background: `(${background})(${options.getTests})`,
  });

  function serializeColor([r, g, b, a]) {
    if (a === 255) {
      return `rgb(${r}, ${g}, ${b})`;
    }
    return `rgba(${r}, ${g}, ${b}, ${a / 255})`;
  }

  let browserActionId;
  function checkDetails(details, windowId) {
    let {document} = Services.wm.getOuterWindowWithId(windowId);
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
    is(button.getAttribute("disabled") == "true", !details.enabled, "disabled state is correct");

    if (details.badge) {
      let badge = button.ownerDocument.getAnonymousElementByAttribute(
        button, "class", "toolbarbutton-badge");
      let style = window.getComputedStyle(badge);
      let expected = {
        backgroundColor: serializeColor(details.badgeBackgroundColor),
        color: serializeColor(details.badgeTextColor),
      };
      for (let [prop, value] of Object.entries(expected)) {
        is(style[prop], value, `${prop} is correct`);
      }
    }

    // TODO: Popup URL.
  }

  let awaitFinish = new Promise(resolve => {
    extension.onMessage("nextTest", async (expecting, windowId, testsRemaining) => {
      await promiseAnimationFrame();

      checkDetails(expecting, windowId);

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
      "global.png": imageBuffer,
      "1.png": imageBuffer,
      "2.png": imageBuffer,
    },

    getTests: function(tabs, windows) {
      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "popup": browser.runtime.getURL("default.html"),
         "title": "Default Title",
         "badge": "",
         "badgeBackgroundColor": [0xd9, 0, 0, 255],
         "badgeTextColor": [0xff, 0xff, 0xff, 0xff],
         "enabled": true},
        {"icon": browser.runtime.getURL("1.png")},
        {"icon": browser.runtime.getURL("2.png"),
         "popup": browser.runtime.getURL("2.html"),
         "title": "Title 2",
         "badge": "2",
         "badgeBackgroundColor": [0xff, 0, 0, 0xff],
         "badgeTextColor": [0, 0xff, 0xff, 0xff],
         "enabled": false},
        {"icon": browser.runtime.getURL("global.png"),
         "popup": browser.runtime.getURL("global.html"),
         "title": "Global Title",
         "badge": "g",
         "badgeBackgroundColor": [0, 0xff, 0, 0xff],
         "badgeTextColor": [0xff, 0, 0xff, 0xff],
         "enabled": false},
        {"icon": browser.runtime.getURL("global.png"),
         "popup": browser.runtime.getURL("global.html"),
         "title": "Global Title",
         "badge": "g",
         "badgeBackgroundColor": [0, 0xff, 0, 0xff],
         "badgeTextColor": [0xff, 0, 0xff, 0xff]},
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
        async expect => {
          browser.test.log("Initial state, expect default properties.");

          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change the icon in the current tab. Expect default properties excluding the icon.");
          browser.browserAction.setIcon({tabId: tabs[0], path: "1.png"});

          expect(details[1], null, null, details[0]);
        },
        async expect => {
          browser.test.log("Create a new tab. Expect default properties.");
          let tab = await browser.tabs.create({active: true, url: "about:blank?0"});
          tabs.push(tab.id);

          browser.test.log("Await tab load.");
          let promise = promiseTabLoad({id: tabs[1], url: "about:blank?0"});
          let {url} = await browser.tabs.get(tabs[1]);
          if (url === "about:blank") {
            await promise;
          }

          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change properties. Expect new properties.");
          let tabId = tabs[1];
          browser.browserAction.setIcon({tabId, path: "2.png"});
          browser.browserAction.setPopup({tabId, popup: "2.html"});
          browser.browserAction.setTitle({tabId, title: "Title 2"});
          browser.browserAction.setBadgeText({tabId, text: "2"});
          browser.browserAction.setBadgeBackgroundColor({tabId, color: "#ff0000"});
          browser.browserAction.setBadgeTextColor({tabId, color: "#00ffff"});
          browser.browserAction.disable(tabId);

          expect(details[2], null, null, details[0]);
        },
        async expect => {
          browser.test.log("Switch back to the first tab. Expect previously set properties.");
          await browser.tabs.update(tabs[0], {active: true});
          expect(details[1], null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change global values, expect those changes reflected.");
          browser.browserAction.setIcon({path: "global.png"});
          browser.browserAction.setPopup({popup: "global.html"});
          browser.browserAction.setTitle({title: "Global Title"});
          browser.browserAction.setBadgeText({text: "g"});
          browser.browserAction.setBadgeBackgroundColor({color: [0, 0xff, 0, 0xff]});
          browser.browserAction.setBadgeTextColor({color: [0xff, 0, 0xff, 0xff]});
          browser.browserAction.disable();

          expect(details[1], null, details[3], details[0]);
        },
        async expect => {
          browser.test.log("Re-enable globally. Expect enabled.");
          browser.browserAction.enable();

          expect(details[1], null, details[4], details[0]);
        },
        async expect => {
          browser.test.log("Switch back to tab 2. Expect former tab values, and new global values from previous steps.");
          await browser.tabs.update(tabs[1], {active: true});

          expect(details[2], null, details[4], details[0]);
        },
        async expect => {
          browser.test.log("Navigate to a new page. Expect tab-specific values to be cleared.");

          let promise = promiseTabLoad({id: tabs[1], url: "about:blank?1"});
          browser.tabs.update(tabs[1], {url: "about:blank?1"});
          await promise;

          expect(null, null, details[4], details[0]);
        },
        async expect => {
          browser.test.log("Delete tab, switch back to tab 1. Expect previous results again.");
          await browser.tabs.remove(tabs[1]);
          expect(details[1], null, details[4], details[0]);
        },
        async expect => {
          browser.test.log("Create a new tab. Expect new global properties.");
          let tab = await browser.tabs.create({active: true, url: "about:blank?2"});
          tabs.push(tab.id);
          expect(null, null, details[4], details[0]);
        },
        async expect => {
          browser.test.log("Delete tab.");
          await browser.tabs.remove(tabs[2]);
          expect(details[1], null, details[4], details[0]);
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

    getTests: function(tabs, windows) {
      let details = [
        {"title": "Foo Extension",
         "popup": "",
         "badge": "",
         "badgeBackgroundColor": [0xd9, 0, 0, 255],
         "badgeTextColor": [0xff, 0xff, 0xff, 0xff],
         "icon": browser.runtime.getURL("icon.png"),
         "enabled": true},
        {"title": "Foo Title"},
        {"title": "Bar Title"},
      ];

      return [
        async expect => {
          browser.test.log("Initial state. Expect default properties.");

          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change the tab title. Expect new title.");
          browser.browserAction.setTitle({tabId: tabs[0], title: "Foo Title"});

          expect(details[1], null, null, details[0]);
        },
        async expect => {
          browser.test.log("Change the global title. Expect same properties.");
          browser.browserAction.setTitle({title: "Bar Title"});

          expect(details[1], null, details[2], details[0]);
        },
        async expect => {
          browser.test.log("Clear the tab title. Expect new global title.");
          browser.browserAction.setTitle({tabId: tabs[0], title: null});

          expect(null, null, details[2], details[0]);
        },
        async expect => {
          browser.test.log("Clear the global title. Expect default title.");
          browser.browserAction.setTitle({title: null});

          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.assertRejects(
            browser.browserAction.setPopup({popup: "about:addons"}),
            /Access denied for URL about:addons/,
            "unable to set popup to about:addons");

          expect(null, null, null, details[0]);
        },
      ];
    },
  });
});

add_task(async function testBadgeColorPersistence() {
  const extension = ExtensionTestUtils.loadExtension({
    background() {
      browser.test.onMessage.addListener((msg, arg) => {
        browser.browserAction[msg](arg);
      });
    },
    manifest: {
      browser_action: {},
    },
  });
  await extension.startup();

  function getBadgeForWindow(win) {
    const widget = getBrowserActionWidget(extension).forWindow(win).node;
    return document.getAnonymousElementByAttribute(widget, "class", "toolbarbutton-badge");
  }

  let badge = getBadgeForWindow(window);
  const badgeChanged = new Promise((resolve) => {
    const observer = new MutationObserver(() => resolve());
    observer.observe(badge, {attributes: true, attributeFilter: ["style"]});
  });

  extension.sendMessage("setBadgeText", {text: "hi"});
  extension.sendMessage("setBadgeBackgroundColor", {color: [0, 255, 0, 255]});

  await badgeChanged;

  is(badge.value, "hi", "badge text is set in first window");
  is(badge.style.backgroundColor, "rgb(0, 255, 0)", "badge color is set in first window");

  let windowOpenedPromise = BrowserTestUtils.waitForNewWindow();
  let win = OpenBrowserWindow();
  await windowOpenedPromise;

  badge = getBadgeForWindow(win);
  is(badge.value, "hi", "badge text is set in new window");
  is(badge.style.backgroundColor, "rgb(0, 255, 0)", "badge color is set in new window");

  await BrowserTestUtils.closeWindow(win);
  await extension.unload();
});

add_task(async function testPropertyRemoval() {
  await runTests({
    manifest: {
      "browser_action": {
        "default_icon": "default.png",
        "default_popup": "default.html",
        "default_title": "Default Title",
      },
    },

    "files": {
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
         "popup": browser.runtime.getURL("default.html"),
         "title": "Default Title",
         "badge": "",
         "badgeBackgroundColor": [0xd9, 0x00, 0x00, 0xFF],
         "badgeTextColor": [0xff, 0xff, 0xff, 0xff],
         "enabled": true},
        {"icon": browser.runtime.getURL("global.png"),
         "popup": browser.runtime.getURL("global.html"),
         "title": "global",
         "badge": "global",
         "badgeBackgroundColor": [0x11, 0x11, 0x11, 0xFF],
         "badgeTextColor": [0x99, 0x99, 0x99, 0xff]},
        {"icon": browser.runtime.getURL("window.png"),
         "popup": browser.runtime.getURL("window.html"),
         "title": "window",
         "badge": "window",
         "badgeBackgroundColor": [0x22, 0x22, 0x22, 0xFF],
         "badgeTextColor": [0x88, 0x88, 0x88, 0xff]},
        {"icon": browser.runtime.getURL("tab.png"),
         "popup": browser.runtime.getURL("tab.html"),
         "title": "tab",
         "badge": "tab",
         "badgeBackgroundColor": [0x33, 0x33, 0x33, 0xFF],
         "badgeTextColor": [0x77, 0x77, 0x77, 0xff]},
        {"icon": defaultIcon,
         "popup": "",
         "title": "",
         "badge": "",
         "badgeBackgroundColor": [0x33, 0x33, 0x33, 0xFF],
         "badgeTextColor": [0x77, 0x77, 0x77, 0xff]},
        {"icon": browser.runtime.getURL("global2.png"),
         "popup": browser.runtime.getURL("global2.html"),
         "title": "global2",
         "badge": "global2",
         "badgeBackgroundColor": [0x44, 0x44, 0x44, 0xFF],
         "badgeTextColor": [0x66, 0x66, 0x66, 0xff]},
      ];

      return [
        async expect => {
          browser.test.log("Initial state, expect default properties.");
          expect(null, null, null, details[0]);
        },
        async expect => {
          browser.test.log("Set global values, expect the new values.");
          browser.browserAction.setIcon({path: "global.png"});
          browser.browserAction.setPopup({popup: "global.html"});
          browser.browserAction.setTitle({title: "global"});
          browser.browserAction.setBadgeText({text: "global"});
          browser.browserAction.setBadgeBackgroundColor({color: "#111"});
          browser.browserAction.setBadgeTextColor({color: "#999"});
          expect(null, null, details[1], details[0]);
        },
        async expect => {
          browser.test.log("Set window values, expect the new values.");
          let windowId = windows[0];
          browser.browserAction.setIcon({windowId, path: "window.png"});
          browser.browserAction.setPopup({windowId, popup: "window.html"});
          browser.browserAction.setTitle({windowId, title: "window"});
          browser.browserAction.setBadgeText({windowId, text: "window"});
          browser.browserAction.setBadgeBackgroundColor({windowId, color: "#222"});
          browser.browserAction.setBadgeTextColor({windowId, color: "#888"});
          expect(null, details[2], details[1], details[0]);
        },
        async expect => {
          browser.test.log("Set tab values, expect the new values.");
          let tabId = tabs[0];
          browser.browserAction.setIcon({tabId, path: "tab.png"});
          browser.browserAction.setPopup({tabId, popup: "tab.html"});
          browser.browserAction.setTitle({tabId, title: "tab"});
          browser.browserAction.setBadgeText({tabId, text: "tab"});
          browser.browserAction.setBadgeBackgroundColor({tabId, color: "#333"});
          browser.browserAction.setBadgeTextColor({tabId, color: "#777"});
          expect(details[3], details[2], details[1], details[0]);
        },
        async expect => {
          browser.test.log("Set empty tab values, expect empty values except for colors.");
          let tabId = tabs[0];
          browser.browserAction.setIcon({tabId, path: ""});
          browser.browserAction.setPopup({tabId, popup: ""});
          browser.browserAction.setTitle({tabId, title: ""});
          browser.browserAction.setBadgeText({tabId, text: ""});
          await browser.test.assertRejects(
            browser.browserAction.setBadgeBackgroundColor({tabId, color: ""}),
            /^Invalid badge background color: ""$/,
            "Expected invalid badge background color error"
          );
          await browser.test.assertRejects(
            browser.browserAction.setBadgeTextColor({tabId, color: ""}),
            /^Invalid badge text color: ""$/,
            "Expected invalid badge text color error"
          );
          expect(details[4], details[2], details[1], details[0]);
        },
        async expect => {
          browser.test.log("Remove tab values, expect window values.");
          let tabId = tabs[0];
          browser.browserAction.setIcon({tabId, path: null});
          browser.browserAction.setPopup({tabId, popup: null});
          browser.browserAction.setTitle({tabId, title: null});
          browser.browserAction.setBadgeText({tabId, text: null});
          browser.browserAction.setBadgeBackgroundColor({tabId, color: null});
          browser.browserAction.setBadgeTextColor({tabId, color: null});
          expect(null, details[2], details[1], details[0]);
        },
        async expect => {
          browser.test.log("Remove window values, expect global values.");
          let windowId = windows[0];
          browser.browserAction.setIcon({windowId, path: null});
          browser.browserAction.setPopup({windowId, popup: null});
          browser.browserAction.setTitle({windowId, title: null});
          browser.browserAction.setBadgeText({windowId, text: null});
          browser.browserAction.setBadgeBackgroundColor({windowId, color: null});
          browser.browserAction.setBadgeTextColor({windowId, color: null});
          expect(null, null, details[1], details[0]);
        },
        async expect => {
          browser.test.log("Change global values, expect the new values.");
          browser.browserAction.setIcon({path: "global2.png"});
          browser.browserAction.setPopup({popup: "global2.html"});
          browser.browserAction.setTitle({title: "global2"});
          browser.browserAction.setBadgeText({text: "global2"});
          browser.browserAction.setBadgeBackgroundColor({color: "#444"});
          browser.browserAction.setBadgeTextColor({color: "#666"});
          expect(null, null, details[5], details[0]);
        },
        async expect => {
          browser.test.log("Remove global values, expect defaults.");
          browser.browserAction.setIcon({path: null});
          browser.browserAction.setPopup({popup: null});
          browser.browserAction.setBadgeText({text: null});
          browser.browserAction.setTitle({title: null});
          browser.browserAction.setBadgeBackgroundColor({color: null});
          browser.browserAction.setBadgeTextColor({color: null});
          expect(null, null, null, details[0]);
        },
      ];
    },
  });
});

add_task(async function testMultipleWindows() {
  await runTests({
    manifest: {
      "browser_action": {
        "default_icon": "default.png",
        "default_popup": "default.html",
        "default_title": "Default Title",
      },
    },

    "files": {
      "default.png": imageBuffer,
      "window1.png": imageBuffer,
      "window2.png": imageBuffer,
    },

    getTests: function(tabs, windows) {
      let details = [
        {"icon": browser.runtime.getURL("default.png"),
         "popup": browser.runtime.getURL("default.html"),
         "title": "Default Title",
         "badge": "",
         "badgeBackgroundColor": [0xd9, 0x00, 0x00, 0xFF],
         "badgeTextColor": [0xff, 0xff, 0xff, 0xff],
         "enabled": true},
        {"icon": browser.runtime.getURL("window1.png"),
         "popup": browser.runtime.getURL("window1.html"),
         "title": "window1",
         "badge": "w1",
         "badgeBackgroundColor": [0x11, 0x11, 0x11, 0xFF],
         "badgeTextColor": [0x99, 0x99, 0x99, 0xff]},
        {"icon": browser.runtime.getURL("window2.png"),
         "popup": browser.runtime.getURL("window2.html"),
         "title": "window2",
         "badge": "w2",
         "badgeBackgroundColor": [0x22, 0x22, 0x22, 0xFF],
         "badgeTextColor": [0x88, 0x88, 0x88, 0xff]},
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
          browser.browserAction.setIcon({windowId, path: "window1.png"});
          browser.browserAction.setPopup({windowId, popup: "window1.html"});
          browser.browserAction.setTitle({windowId, title: "window1"});
          browser.browserAction.setBadgeText({windowId, text: "w1"});
          browser.browserAction.setBadgeBackgroundColor({windowId, color: "#111"});
          browser.browserAction.setBadgeTextColor({windowId, color: "#999"});
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
          await browser.browserAction.setTitle({tabId: tabs[1], title: "tab"});
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
          browser.browserAction.setIcon({windowId, path: "window2.png"});
          browser.browserAction.setPopup({windowId, popup: "window2.html"});
          browser.browserAction.setTitle({windowId, title: "window2"});
          browser.browserAction.setBadgeText({windowId, text: "w2"});
          browser.browserAction.setBadgeBackgroundColor({windowId, color: "#222"});
          browser.browserAction.setBadgeTextColor({windowId, color: "#888"});
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
            setPopup: {popup: "default.html"},
            setTitle: {title: "Default Title"},
            setBadgeText: {text: ""},
            setBadgeBackgroundColor: {color: [0xd9, 0x00, 0x00, 0xFF]},
            setBadgeTextColor: {color: [0xff, 0xff, 0xff, 0xff]},
            getPopup: {},
            getTitle: {},
            getBadgeText: {},
            getBadgeBackgroundColor: {},
          };
          for (let [method, arg] of Object.entries(calls)) {
            browser.test.assertThrows(
              () => browser.browserAction[method]({...arg, windowId: -3}),
              /-3 is too small \(must be at least -2\)/,
              method + " with invalid windowId",
            );
            await browser.test.assertRejects(
              browser.browserAction[method]({...arg, tabId: tabs[0], windowId: windows[0]}),
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
    await sendMessage("getBadgeText", {tabId}, "foo", msg);
    await sendMessage("getTitle", {tabId}, tab_title, msg);
  }
  async function expectDefaultData(tab, msg) {
    let tabId = tabTracker.getId(tab);
    await sendMessage("getBadgeText", {tabId}, "", msg);
    await sendMessage("getTitle", {tabId}, default_title, msg);
  }
  async function setTabSpecificData(tab) {
    let tabId = tabTracker.getId(tab);
    await expectDefaultData(tab, "Expect default data before setting tab-specific data.");
    await sendMessage("setBadgeText", {tabId, text: "foo"});
    await sendMessage("setTitle", {tabId, title: tab_title});
    await expectTabSpecificData(tab, "Expect tab-specific data after setting it.");
  }

  info("Load a tab before installing the extension");
  let tab1 = await addTab(url, true, true);

  extension = ExtensionTestUtils.loadExtension({
    manifest: {
      browser_action: {default_title},
    },
    background: function() {
      browser.test.onMessage.addListener(async ({method, param, expect, msg}) => {
        let result = await browser.browserAction[method](param);
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
    await BrowserTestUtils.removeTab(tab);
  }
  await extension.unload();
});
