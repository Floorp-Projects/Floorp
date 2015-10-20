/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testTabSwitchContext() {

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "page_action": {
        "default_icon": "default.png",
        "default_popup": "default.html",
        "default_title": "Default Title",
      },
      "permissions": ["tabs"],
    },

    background: function () {
      var details = [
        { "icon": browser.runtime.getURL("default.png"),
          "popup": browser.runtime.getURL("default.html"),
          "title": "Default Title" },
        { "icon": browser.runtime.getURL("1.png"),
          "popup": browser.runtime.getURL("default.html"),
          "title": "Default Title" },
        { "icon": browser.runtime.getURL("2.png"),
          "popup": browser.runtime.getURL("2.html"),
          "title": "Title 2" },
      ];

      var tabs = [];

      var tests = [
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
            tabs.push(tab.id);
            expect(null);
          });
        },
        expect => {
          browser.test.log("Change properties. Expect new properties.");
          var tabId = tabs[1];
          browser.pageAction.show(tabId);
          browser.pageAction.setIcon({ tabId, path: "2.png" });
          browser.pageAction.setPopup({ tabId, popup: "2.html" });
          browser.pageAction.setTitle({ tabId, title: "Title 2" });

          expect(details[2]);
        },
        expect => {
          browser.test.log("Navigate to a new page. Expect icon hidden.");

          // TODO: This listener should not be necessary, but the |tabs.update|
          // callback currently fires too early in e10s windows.
          browser.tabs.onUpdated.addListener(function listener(tabId, changed) {
            if (tabId == tabs[1] && changed.url) {
              browser.tabs.onUpdated.removeListener(listener);
              expect(null);
            }
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

      // Gets the current details of the page action, and returns a
      // promise that resolves to an object containing them.
      function getDetails() {
        return new Promise(resolve => {
          return browser.tabs.query({ active: true, currentWindow: true }, resolve);
        }).then(tabs => {
          var tabId = tabs[0].id;

          return Promise.all([
            new Promise(resolve => browser.pageAction.getTitle({tabId}, resolve)),
            new Promise(resolve => browser.pageAction.getPopup({tabId}, resolve))])
        }).then(details => {
          return Promise.resolve({ title: details[0],
                                   popup: details[1] });
        });
      }


      // Runs the next test in the `tests` array, checks the results,
      // and passes control back to the outer test scope.
      function nextTest() {
        var test = tests.shift();

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

      browser.test.onMessage.addListener((msg) => {
        if (msg != "runNextTest") {
          browser.test.fail("Expecting 'runNextTest' message");
        }

        nextTest();
      });

      browser.tabs.query({ active: true, currentWindow: true }, resultTabs => {
        tabs[0] = resultTabs[0].id;

        nextTest();
      });
    },
  });

  let pageActionId = makeWidgetId(extension.id) + "-page-action";

  function checkDetails(details) {
    let image = document.getElementById(pageActionId);
    if (details == null) {
      ok(image == null || image.hidden, "image is hidden");
    } else {
      ok(image, "image exists");

      is(image.src, details.icon, "icon URL is correct");
      is(image.getAttribute("tooltiptext"), details.title, "image title is correct");
      is(image.getAttribute("aria-label"), details.title, "image aria-label is correct");
      // TODO: Popup URL.
    }
  }

  let awaitFinish = new Promise(resolve => {
    extension.onMessage("nextTest", (expecting, testsRemaining) => {
      checkDetails(expecting);

      if (testsRemaining) {
        extension.sendMessage("runNextTest")
      } else {
        resolve();
      }
    });
  });

  yield extension.startup();

  yield awaitFinish;

  yield extension.unload();

  let node = document.getElementById(pageActionId);
  is(node, undefined, "pageAction image removed from document");
});
