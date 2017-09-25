/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testLocalStorage() {
  async function background() {
    function openTabs() {
      let promise = new Promise(resolve => {
        let tabURLs = [
          "http://example.com/",
          "http://example.net/",
        ];

        let tabs;
        let waitingCount = tabURLs.length;

        let listener = async msg => {
          if (msg !== "content-script-ready" || --waitingCount) {
            return;
          }
          browser.runtime.onMessage.removeListener(listener);
          resolve(Promise.all(tabs));
        };

        browser.runtime.onMessage.addListener(listener);

        tabs = tabURLs.map(url => {
          return browser.tabs.create({url: url});
        });
      });

      return promise;
    }

    function sendMessageToTabs(tabs, message) {
      return Promise.all(
        tabs.map(tab => { return browser.tabs.sendMessage(tab.id, message); }));
    }

    let tabs = await openTabs();

    browser.test.assertRejects(
      browser.browsingData.removeLocalStorage({since: Date.now()}),
      "Firefox does not support clearing localStorage with 'since'.",
      "Expected error received when using unimplemented parameter 'since'."
    );

    await sendMessageToTabs(tabs, "resetLocalStorage");
    await browser.browsingData.removeLocalStorage({hostnames: ["example.com"]});
    await browser.tabs.sendMessage(tabs[0].id, "checkLocalStorageCleared");
    await browser.tabs.sendMessage(tabs[1].id, "checkLocalStorageSet");

    await sendMessageToTabs(tabs, "resetLocalStorage");
    await sendMessageToTabs(tabs, "checkLocalStorageSet");
    await browser.browsingData.removeLocalStorage({});
    await sendMessageToTabs(tabs, "checkLocalStorageCleared");

    await sendMessageToTabs(tabs, "resetLocalStorage");
    await sendMessageToTabs(tabs, "checkLocalStorageSet");
    await browser.browsingData.remove({}, {localStorage: true});
    await sendMessageToTabs(tabs, "checkLocalStorageCleared");

    browser.tabs.remove(tabs.map(tab => tab.id));

    browser.test.notifyPass("done");
  }

  function contentScript() {
    browser.runtime.onMessage.addListener(msg => {
      if (msg === "resetLocalStorage") {
        localStorage.clear();
        localStorage.setItem("test", "test");
      } else if (msg === "checkLocalStorageSet") {
        browser.test.assertEq("test", localStorage.getItem("test"));
      } else if (msg === "checkLocalStorageCleared") {
        browser.test.assertEq(null, localStorage.getItem("test"));
      }
    });
    browser.runtime.sendMessage("content-script-ready");
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      "permissions": ["browsingData"],
      "content_scripts": [{
        "matches": [
          "http://example.com/",
          "http://example.net/",
        ],
        "js": ["content-script.js"],
        "run_at": "document_start",
      }],
    },
    files: {
      "content-script.js": contentScript,
    },
  });

  await extension.startup();
  await extension.awaitFinish("done");
  await extension.unload();
});

