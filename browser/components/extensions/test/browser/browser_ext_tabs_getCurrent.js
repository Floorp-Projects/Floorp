"use strict";

add_task(function* () {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      "permissions": ["tabs"],

      "browser_action": { "default_popup": "popup.html" },
    },

    files: {
      "tab.js": function() {
        let url = document.location.href;

        browser.tabs.getCurrent(currentTab => {
          browser.test.assertEq(currentTab.url, url, "getCurrent in non-active background tab");

          // Activate the tab.
          browser.tabs.onActivated.addListener(function listener({ tabId }) {
            if (tabId == currentTab.id) {
              browser.tabs.onActivated.removeListener(listener);

              browser.tabs.getCurrent(currentTab => {
                browser.test.assertEq(currentTab.id, tabId, "in active background tab");
                browser.test.assertEq(currentTab.url, url, "getCurrent in non-active background tab");

                browser.test.sendMessage("tab-finished");
                browser.tabs.remove(tabId);
              });
            }
          });
          browser.tabs.update(currentTab.id, { active: true });
        });
      },

      "popup.js": function() {
        browser.tabs.getCurrent(tab => {
          browser.test.assertEq(tab, undefined, "getCurrent in popup script");
          browser.test.sendMessage("popup-finished");
        });
      },

      "tab.html": `<head><meta charset="utf-8"><script src="tab.js"></script></head>`,
      "popup.html": `<head><meta charset="utf-8"><script src="popup.js"></script></head>`,
    },

    background: function() {
      browser.tabs.getCurrent(tab => {
        browser.test.assertEq(tab, undefined, "getCurrent in background script");
        browser.test.sendMessage("background-finished");
      });

      browser.tabs.create({ url: "tab.html", active: false });
    },
  });

  yield extension.startup();

  yield extension.awaitMessage("background-finished");
  yield extension.awaitMessage("tab-finished");

  clickBrowserAction(extension);
  yield extension.awaitMessage("popup-finished");

  yield extension.unload();
});
