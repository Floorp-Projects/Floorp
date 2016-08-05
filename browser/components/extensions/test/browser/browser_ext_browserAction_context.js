/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

function* runTests(options) {
  function background(getTests) {
    // Gets the current details of the browser action, and returns a
    // promise that resolves to an object containing them.
    function getDetails(tabId) {
      return Promise.all([
        browser.browserAction.getTitle({tabId}),
        browser.browserAction.getPopup({tabId}),
        browser.browserAction.getBadgeText({tabId}),
        browser.browserAction.getBadgeBackgroundColor({tabId})]
      ).then(details => {
        return Promise.resolve({title: details[0],
                                popup: details[1],
                                badge: details[2],
                                badgeBackgroundColor: details[3]});
      });
    }

    function checkDetails(expecting, tabId) {
      return getDetails(tabId).then(details => {
        browser.test.assertEq(expecting.title, details.title,
                              "expected value from getTitle");

        browser.test.assertEq(expecting.popup, details.popup,
                              "expected value from getPopup");

        browser.test.assertEq(expecting.badge, details.badge,
                              "expected value from getBadge");

        browser.test.assertEq(String(expecting.badgeBackgroundColor),
                              String(details.badgeBackgroundColor),
                              "expected value from getBadgeBackgroundColor");
      });
    }

    let expectDefaults = expecting => {
      return checkDetails(expecting);
    };

    let tabs = [];
    let tests = getTests(tabs, expectDefaults);

    // Runs the next test in the `tests` array, checks the results,
    // and passes control back to the outer test scope.
    function nextTest() {
      let test = tests.shift();

      test(expecting => {
        // Check that the API returns the expected values, and then
        // run the next test.
        new Promise(resolve => {
          return browser.tabs.query({active: true, currentWindow: true}, resolve);
        }).then(tabs => {
          return checkDetails(expecting, tabs[0].id);
        }).then(() => {
          // Check that the actual icon has the expected values, then
          // run the next test.
          browser.test.sendMessage("nextTest", expecting, tests.length);
        });
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


  let browserActionId = makeWidgetId(extension.id) + "-browser-action";

  function checkDetails(details) {
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
    extension.onMessage("nextTest", (expecting, testsRemaining) => {
      checkDetails(expecting);

      if (testsRemaining) {
        extension.sendMessage("runNextTest");
      } else {
        resolve();
      }
    });
  });

  yield extension.startup();

  yield awaitFinish;

  yield extension.unload();
}

add_task(function* testTabSwitchContext() {
  yield runTests({
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

    getTests(tabs, expectDefaults) {
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
        expect => {
          browser.test.log("Initial state, expect default properties.");
          expectDefaults(details[0]).then(() => {
            expect(details[0]);
          });
        },
        expect => {
          browser.test.log("Change the icon in the current tab. Expect default properties excluding the icon.");
          browser.browserAction.setIcon({tabId: tabs[0], path: "1.png"});
          expectDefaults(details[0]).then(() => {
            expect(details[1]);
          });
        },
        expect => {
          browser.test.log("Create a new tab. Expect default properties.");
          browser.tabs.create({active: true, url: "about:blank?0"}, tab => {
            tabs.push(tab.id);
            expectDefaults(details[0]).then(() => {
              expect(details[0]);
            });
          });
        },
        expect => {
          browser.test.log("Change properties. Expect new properties.");
          let tabId = tabs[1];
          browser.browserAction.setIcon({tabId, path: "2.png"});
          browser.browserAction.setPopup({tabId, popup: "2.html"});
          browser.browserAction.setTitle({tabId, title: "Title 2"});
          browser.browserAction.setBadgeText({tabId, text: "2"});
          browser.browserAction.setBadgeBackgroundColor({tabId, color: "#ff0000"});
          browser.browserAction.disable(tabId);

          expectDefaults(details[0]).then(() => {
            expect(details[2]);
          });
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
        expect => {
          browser.test.log("Switch back to the first tab. Expect previously set properties.");
          browser.tabs.update(tabs[0], {active: true}, () => {
            expect(details[1]);
          });
        },
        expect => {
          browser.test.log("Change default values, expect those changes reflected.");
          browser.browserAction.setIcon({path: "default-2.png"});
          browser.browserAction.setPopup({popup: "default-2.html"});
          browser.browserAction.setTitle({title: "Default Title 2"});
          browser.browserAction.setBadgeText({text: "d2"});
          browser.browserAction.setBadgeBackgroundColor({color: [0, 0xff, 0, 0xff]});
          browser.browserAction.disable();
          expectDefaults(details[3]).then(() => {
            expect(details[3]);
          });
        },
        expect => {
          browser.test.log("Re-enable by default. Expect enabled.");
          browser.browserAction.enable();
          expectDefaults(details[4]).then(() => {
            expect(details[4]);
          });
        },
        expect => {
          browser.test.log("Switch back to tab 2. Expect former value, unaffected by changes to defaults in previous step.");
          browser.tabs.update(tabs[1], {active: true}, () => {
            expectDefaults(details[3]).then(() => {
              expect(details[2]);
            });
          });
        },
        expect => {
          browser.test.log("Delete tab, switch back to tab 1. Expect previous results again.");
          browser.tabs.remove(tabs[1], () => {
            expect(details[4]);
          });
        },
        expect => {
          browser.test.log("Create a new tab. Expect new default properties.");
          browser.tabs.create({active: true, url: "about:blank?2"}, tab => {
            tabs.push(tab.id);
            expect(details[5]);
          });
        },
        expect => {
          browser.test.log("Delete tab.");
          browser.tabs.remove(tabs[2], () => {
            expect(details[4]);
          });
        },
      ];
    },
  });
});

add_task(function* testDefaultTitle() {
  yield runTests({
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

    getTests(tabs, expectDefaults) {
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
        expect => {
          browser.test.log("Initial state. Expect extension title as default title.");
          expectDefaults(details[0]).then(() => {
            expect(details[0]);
          });
        },
        expect => {
          browser.test.log("Change the title. Expect new title.");
          browser.browserAction.setTitle({tabId: tabs[0], title: "Foo Title"});
          expectDefaults(details[0]).then(() => {
            expect(details[1]);
          });
        },
        expect => {
          browser.test.log("Change the default. Expect same properties.");
          browser.browserAction.setTitle({title: "Bar Title"});
          expectDefaults(details[2]).then(() => {
            expect(details[1]);
          });
        },
        expect => {
          browser.test.log("Clear the title. Expect new default title.");
          browser.browserAction.setTitle({tabId: tabs[0], title: ""});
          expectDefaults(details[2]).then(() => {
            expect(details[2]);
          });
        },
        expect => {
          browser.test.log("Set default title to null string. Expect null string from API, extension title in UI.");
          browser.browserAction.setTitle({title: ""});
          expectDefaults(details[3]).then(() => {
            expect(details[3]);
          });
        },
      ];
    },
  });
});
