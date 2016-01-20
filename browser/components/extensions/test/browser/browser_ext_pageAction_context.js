/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* runTests(options) {
  function background(getTests) {
    let tabs;
    let tests;

    // Gets the current details of the page action, and returns a
    // promise that resolves to an object containing them.
    function getDetails() {
      return new Promise(resolve => {
        return browser.tabs.query({ active: true, currentWindow: true }, resolve);
      }).then(([tab]) => {
        let tabId = tab.id;
        browser.test.log(`Get details: tab={id: ${tabId}, url: ${JSON.stringify(tab.url)}}`);
        return Promise.all([
          new Promise(resolve => browser.pageAction.getTitle({tabId}, resolve)),
          new Promise(resolve => browser.pageAction.getPopup({tabId}, resolve))]);
      }).then(details => {
        return Promise.resolve({ title: details[0],
                                 popup: details[1] });
      });
    }


    // Runs the next test in the `tests` array, checks the results,
    // and passes control back to the outer test scope.
    function nextTest() {
      let test = tests.shift();

      test(expecting => {
        function finish() {
          // Check that the actual icon has the expected values, then
          // run the next test.
          browser.test.sendMessage("nextTest", expecting, tests.length);
        }

        if (expecting) {
          // Check that the API returns the expected values, and then
          // run the next test.
          getDetails().then(details => {
            browser.test.assertEq(expecting.title, details.title,
                                  "expected value from getTitle");

            browser.test.assertEq(expecting.popup, details.popup,
                                  "expected value from getPopup");

            finish();
          });
        } else {
          finish();
        }
      });
    }

    function runTests() {
      tabs = [];
      tests = getTests(tabs);

      browser.tabs.query({ active: true, currentWindow: true }, resultTabs => {
        tabs[0] = resultTabs[0].id;

        nextTest();
      });
    }

    browser.test.onMessage.addListener((msg) => {
      if (msg == "runTests") {
        runTests();
      } else if (msg == "runNextTest") {
        nextTest();
      } else {
        browser.test.fail(`Unexpected message: ${msg}`);
      }
    });

    runTests();
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: options.manifest,

    background: `(${background})(${options.getTests})`,
  });

  let pageActionId = makeWidgetId(extension.id) + "-page-action";
  let currentWindow = window;
  let windows = [];

  function checkDetails(details) {
    let image = currentWindow.document.getElementById(pageActionId);
    if (details == null) {
      ok(image == null || image.hidden, "image is hidden");
    } else {
      ok(image, "image exists");

      is(image.src, details.icon, "icon URL is correct");

      let title = details.title || options.manifest.name;
      is(image.getAttribute("tooltiptext"), title, "image title is correct");
      is(image.getAttribute("aria-label"), title, "image aria-label is correct");
      // TODO: Popup URL.
    }
  }

  let testNewWindows = 1;

  let awaitFinish = new Promise(resolve => {
    extension.onMessage("nextTest", (expecting, testsRemaining) => {
      checkDetails(expecting);

      if (testsRemaining) {
        extension.sendMessage("runNextTest");
      } else if (testNewWindows) {
        testNewWindows--;

        BrowserTestUtils.openNewBrowserWindow().then(window => {
          windows.push(window);
          currentWindow = window;
          return focusWindow(window);
        }).then(() => {
          extension.sendMessage("runTests");
        });
      } else {
        resolve();
      }
    });
  });

  yield extension.startup();

  yield awaitFinish;

  yield extension.unload();

  let node = document.getElementById(pageActionId);
  is(node, null, "pageAction image removed from document");

  currentWindow = null;
  for (let win of windows.splice(0)) {
    node = win.document.getElementById(pageActionId);
    is(node, null, "pageAction image removed from second document");

    yield BrowserTestUtils.closeWindow(win);
  }
}

add_task(function* testTabSwitchContext() {
  yield runTests({
    manifest: {
      "name": "Foo Extension",

      "page_action": {
        "default_icon": "default.png",
        "default_popup": "default.html",
        "default_title": "Default Title \u263a",
      },

      "permissions": ["tabs"],
    },

    getTests(tabs) {
      let details = [
        { "icon": browser.runtime.getURL("default.png"),
          "popup": browser.runtime.getURL("default.html"),
          "title": "Default Title \u263a" },
        { "icon": browser.runtime.getURL("1.png"),
          "popup": browser.runtime.getURL("default.html"),
          "title": "Default Title \u263a" },
        { "icon": browser.runtime.getURL("2.png"),
          "popup": browser.runtime.getURL("2.html"),
          "title": "Title 2" },
        { "icon": browser.runtime.getURL("2.png"),
          "popup": browser.runtime.getURL("2.html"),
          "title": "Default Title \u263a" },
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
      let tabLoadPromise;

      return [
        expect => {
          browser.test.log("Initial state. No icon visible.");
          expect(null);
        },
        expect => {
          browser.test.log("Show the icon on the first tab, expect default properties.");
          browser.pageAction.show(tabs[0]);
          expect(details[0]);
        },
        expect => {
          browser.test.log("Change the icon. Expect default properties excluding the icon.");
          browser.pageAction.setIcon({ tabId: tabs[0], path: "1.png" });
          expect(details[1]);
        },
        expect => {
          browser.test.log("Create a new tab. No icon visible.");
          browser.tabs.create({ active: true, url: "about:blank?0" }, tab => {
            tabLoadPromise = promiseTabLoad({ url: "about:blank?0", id: tab.id });
            tabs.push(tab.id);
            expect(null);
          });
        },
        expect => {
          browser.test.log("Await tab load. No icon visible.");
          tabLoadPromise.then(() => {
            expect(null);
          });
        },
        expect => {
          browser.test.log("Change properties. Expect new properties.");
          let tabId = tabs[1];
          browser.pageAction.show(tabId);
          browser.pageAction.setIcon({ tabId, path: "2.png" });
          browser.pageAction.setPopup({ tabId, popup: "2.html" });
          browser.pageAction.setTitle({ tabId, title: "Title 2" });

          expect(details[2]);
        },
        expect => {
          browser.test.log("Clear the title. Expect default title.");
          browser.pageAction.setTitle({ tabId: tabs[1], title: "" });

          expect(details[3]);
        },
        expect => {
          browser.test.log("Navigate to a new page. Expect icon hidden.");

          // TODO: This listener should not be necessary, but the |tabs.update|
          // callback currently fires too early in e10s windows.
          promiseTabLoad({ id: tabs[1], url: "about:blank?1" }).then(() => {
            expect(null);
          });

          browser.tabs.update(tabs[1], { url: "about:blank?1" });
        },
        expect => {
          browser.test.log("Show the icon. Expect default properties again.");
          browser.pageAction.show(tabs[1]);
          expect(details[0]);
        },
        expect => {
          browser.test.log("Switch back to the first tab. Expect previously set properties.");
          browser.tabs.update(tabs[0], { active: true }, () => {
            expect(details[1]);
          });
        },
        expect => {
          browser.test.log("Hide the icon on tab 2. Switch back, expect hidden.");
          browser.pageAction.hide(tabs[1]);
          browser.tabs.update(tabs[1], { active: true }, () => {
            expect(null);
          });
        },
        expect => {
          browser.test.log("Switch back to tab 1. Expect previous results again.");
          browser.tabs.remove(tabs[1], () => {
            expect(details[1]);
          });
        },
        expect => {
          browser.test.log("Hide the icon. Expect hidden.");
          browser.pageAction.hide(tabs[0]);
          expect(null);
        },
      ];
    },
  });
});

add_task(function* testDefaultTitle() {
  yield runTests({
    manifest: {
      "name": "Foo Extension",

      "page_action": {
        "default_icon": "icon.png",
      },

      "permissions": ["tabs"],
    },

    getTests(tabs) {
      let details = [
        { "title": "Foo Extension",
          "popup": "",
          "icon": browser.runtime.getURL("icon.png") },
        { "title": "Foo Title",
          "popup": "",
          "icon": browser.runtime.getURL("icon.png") },
      ];

      return [
        expect => {
          browser.test.log("Initial state. No icon visible.");
          expect(null);
        },
        expect => {
          browser.test.log("Show the icon on the first tab, expect extension title as default title.");
          browser.pageAction.show(tabs[0]);
          expect(details[0]);
        },
        expect => {
          browser.test.log("Change the title. Expect new title.");
          browser.pageAction.setTitle({ tabId: tabs[0], title: "Foo Title" });
          expect(details[1]);
        },
        expect => {
          browser.test.log("Clear the title. Expect extension title.");
          browser.pageAction.setTitle({ tabId: tabs[0], title: "" });
          expect(details[0]);
        },
      ];
    },
  });
});
