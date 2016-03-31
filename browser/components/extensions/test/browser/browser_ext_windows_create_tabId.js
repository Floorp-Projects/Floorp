/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWindowCreate() {
  function background() {
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

    let windowId;
    browser.windows.getCurrent().then(window => {
      windowId = window.id;

      browser.test.log("Create additional tab in window 1");
      return browser.tabs.create({windowId, url: "about:blank"});
    }).then(tab => {
      browser.test.log("Create a new window, adopting the new tab");

      // Note that we want to check against actual boolean values for
      // all of the `incognito` property tests.
      browser.test.assertEq(false, tab.incognito, "Tab is not private");

      return Promise.all([
        promiseTabAttached(),
        browser.windows.create({tabId: tab.id}),
      ]);
    }).then(([, window]) => {
      browser.test.assertEq(false, window.incognito, "New window is not private");

      browser.test.log("Close the new window");
      return browser.windows.remove(window.id);
    }).then(() => {
      browser.test.log("Create a new private window");

      return browser.windows.create({incognito: true});
    }).then(privateWindow => {
      browser.test.assertEq(true, privateWindow.incognito, "Private window is private");

      browser.test.log("Create additional tab in private window");
      return browser.tabs.create({windowId: privateWindow.id}).then(privateTab => {
        browser.test.assertEq(true, privateTab.incognito, "Private tab is private");

        browser.test.log("Create a new window, adopting the new private tab");

        return Promise.all([
          promiseTabAttached(),
          browser.windows.create({tabId: privateTab.id}),
        ]);
      }).then(([, newWindow]) => {
        browser.test.assertEq(true, newWindow.incognito, "New private window is private");

        browser.test.log("Close the new private window");
        return browser.windows.remove(newWindow.id);
      }).then(() => {
        browser.test.log("Close the private window");
        return browser.windows.remove(privateWindow.id);
      });
    }).then(() => {
      return browser.tabs.query({windowId, active: true});
    }).then(([tab]) => {
      browser.test.log("Try to create a window with both a tab and a URL");

      return browser.windows.create({tabId: tab.id, url: "http://example.com/"}).then(
        window => {
          browser.test.fail("Create call should have failed");
        },
        error => {
          browser.test.assertTrue(/`tabId` may not be used in conjunction with `url`/.test(error.message),
                                  "Create call failed as expected");
        }).then(() => {
          browser.test.log("Try to create a window with both a tab and an invalid incognito setting");

          return browser.windows.create({tabId: tab.id, incognito: true});
        }).then(
          window => {
            browser.test.fail("Create call should have failed");
          },
          error => {
            browser.test.assertTrue(/`incognito` property must match the incognito state of tab/.test(error.message),
                                    "Create call failed as expected");
          });
    }).then(() => {
      browser.test.log("Try to create a window with an invalid tabId");

      return browser.windows.create({tabId: 0}).then(
        window => {
          browser.test.fail("Create call should have failed");
        },
        error => {
          browser.test.assertTrue(/Invalid tab ID: 0/.test(error.message),
                                  "Create call failed as expected");
        }
      );
    }).then(() => {
      browser.test.log("Try to create a window with two URLs");

      return browser.windows.create({url: ["http://example.com/", "http://example.org/"]});
    }).then(window => {
      return Promise.all([
        promiseTabUpdated("http://example.com/"),
        promiseTabUpdated("http://example.org/"),
        Promise.resolve(window),
      ]);
    }).then(([, , window]) => {
      return browser.windows.get(window.id, {populate: true});
    }).then(window => {
      browser.test.assertEq(2, window.tabs.length, "2 tabs were opened in new window");
      browser.test.assertEq("http://example.com/", window.tabs[0].url, "Correct URL was loaded in tab 1");
      browser.test.assertEq("http://example.org/", window.tabs[1].url, "Correct URL was loaded in tab 2");
      return browser.windows.remove(window.id);
    }).then(() => {
      browser.test.notifyPass("window-create");
    }).catch(e => {
      browser.test.fail(`${e} :: ${e.stack}`);
      browser.test.notifyFail("window-create");
    });
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

