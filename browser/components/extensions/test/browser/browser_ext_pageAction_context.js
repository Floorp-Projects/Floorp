/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/* global runTests */

Services.scriptloader.loadSubScript(new URL("head_pageAction.js", gTestPath).href,
                                    this);

add_task(function* testTabSwitchContext() {
  yield runTests({
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

    getTests(tabs) {
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
        {"icon": browser.runtime.getURL("2.png"),
         "popup": browser.runtime.getURL("2.html"),
         "title": "Default T\u00edtulo \u263a"},
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
        expect => {
          browser.test.log("Show the icon on the first tab, expect default properties.");
          browser.pageAction.show(tabs[0]).then(() => {
            expect(details[0]);
          });
        },
        expect => {
          browser.test.log("Change the icon. Expect default properties excluding the icon.");
          browser.pageAction.setIcon({tabId: tabs[0], path: "1.png"});
          expect(details[1]);
        },
        expect => {
          browser.test.log("Create a new tab. No icon visible.");
          browser.tabs.create({active: true, url: "about:blank?0"}, tab => {
            tabs.push(tab.id);
            expect(null);
          });
        },
        expect => {
          browser.test.log("Await tab load. No icon visible.");
          expect(null);
        },
        expect => {
          browser.test.log("Change properties. Expect new properties.");
          let tabId = tabs[1];
          browser.pageAction.show(tabId).then(() => {
            browser.pageAction.setIcon({tabId, path: "2.png"});
            browser.pageAction.setPopup({tabId, popup: "2.html"});
            browser.pageAction.setTitle({tabId, title: "Title 2"});

            expect(details[2]);
          });
        },
        expect => {
          browser.test.log("Change the hash. Expect same properties.");

          promiseTabLoad({id: tabs[1], url: "about:blank?0#ref"}).then(() => {
            expect(details[2]);
          });

          browser.tabs.update(tabs[1], {url: "about:blank?0#ref"});
        },
        expect => {
          browser.test.log("Clear the title. Expect default title.");
          browser.pageAction.setTitle({tabId: tabs[1], title: ""});

          expect(details[3]);
        },
        expect => {
          browser.test.log("Navigate to a new page. Expect icon hidden.");

          // TODO: This listener should not be necessary, but the |tabs.update|
          // callback currently fires too early in e10s windows.
          promiseTabLoad({id: tabs[1], url: "about:blank?1"}).then(() => {
            expect(null);
          });

          browser.tabs.update(tabs[1], {url: "about:blank?1"});
        },
        expect => {
          browser.test.log("Show the icon. Expect default properties again.");
          browser.pageAction.show(tabs[1]).then(() => {
            expect(details[0]);
          });
        },
        expect => {
          browser.test.log("Switch back to the first tab. Expect previously set properties.");
          browser.tabs.update(tabs[0], {active: true}, () => {
            expect(details[1]);
          });
        },
        expect => {
          browser.test.log("Hide the icon on tab 2. Switch back, expect hidden.");
          browser.pageAction.hide(tabs[1]).then(() => {
            browser.tabs.update(tabs[1], {active: true}, () => {
              expect(null);
            });
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
          browser.pageAction.hide(tabs[0]).then(() => {
            expect(null);
          });
        },
      ];
    },
  });
});
