/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function testLocalStorage() {
  async function background() {
    function openTabs() {
      let promise = new Promise(resolve => {
        // Open tabs on every one of the urls that the test extension
        // content script is going to match.
        const manifest = browser.runtime.getManifest();
        const tabURLs = manifest.content_scripts[0].matches;
        browser.test.log(`Opening tabs on ${JSON.stringify(tabURLs)}`);

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
          return browser.tabs.create({ url: url });
        });
      });

      return promise;
    }

    function sendMessageToTabs(tabs, message) {
      return Promise.all(
        tabs.map(tab => {
          return browser.tabs.sendMessage(tab.id, message);
        })
      );
    }

    let tabs = await openTabs();

    browser.test.assertRejects(
      browser.browsingData.removeLocalStorage({ since: Date.now() }),
      "Firefox does not support clearing localStorage with 'since'.",
      "Expected error received when using unimplemented parameter 'since'."
    );

    await sendMessageToTabs(tabs, "resetLocalStorage");
    await browser.browsingData.removeLocalStorage({
      hostnames: ["example.com"],
    });
    await browser.tabs.sendMessage(tabs[0].id, "checkLocalStorageCleared");
    await browser.tabs.sendMessage(tabs[1].id, "checkLocalStorageSet");

    if (
      SpecialPowers.Services.domStorageManager.nextGenLocalStorageEnabled ===
      false
    ) {
      // This assertion fails when localStorage is using the legacy
      // implementation (See Bug 1595431).
      browser.test.log("Skipped assertion on nextGenLocalStorageEnabled=false");
    } else {
      await browser.tabs.sendMessage(tabs[2].id, "checkLocalStorageSet");
    }

    await sendMessageToTabs(tabs, "resetLocalStorage");
    await sendMessageToTabs(tabs, "checkLocalStorageSet");
    await browser.browsingData.removeLocalStorage({});
    await sendMessageToTabs(tabs, "checkLocalStorageCleared");

    await sendMessageToTabs(tabs, "resetLocalStorage");
    await sendMessageToTabs(tabs, "checkLocalStorageSet");
    await browser.browsingData.remove({}, { localStorage: true });
    await sendMessageToTabs(tabs, "checkLocalStorageCleared");

    // Cleanup (checkLocalStorageCleared creates empty LS databases).
    await browser.browsingData.removeLocalStorage({});

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
      permissions: ["browsingData"],
      content_scripts: [
        {
          matches: [
            "http://example.com/",
            "http://example.net/",
            "http://test1.example.com/",
          ],
          js: ["content-script.js"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "content-script.js": contentScript,
    },
  });

  await extension.startup();
  await extension.awaitFinish("done");
  await extension.unload();
});

// Verify that browsingData.removeLocalStorage doesn't break on data stored
// in about:newtab or file principals.
add_task(async function test_browserData_on_aboutnewtab_and_file_data() {
  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      await browser.browsingData.removeLocalStorage({}).catch(err => {
        browser.test.fail(`${err} :: ${err.stack}`);
      });
      browser.test.sendMessage("done");
    },
    manifest: {
      permissions: ["browsingData"],
    },
  });

  // Let's create some data on about:newtab origin.
  const { SiteDataTestUtils } = ChromeUtils.import(
    "resource://testing-common/SiteDataTestUtils.jsm"
  );
  await SiteDataTestUtils.addToIndexedDB("about:newtab", "foo", "bar", {});
  await SiteDataTestUtils.addToIndexedDB("file:///fake/file", "foo", "bar", {});

  await extension.startup();
  await extension.awaitMessage("done");
  await extension.unload();
});

add_task(async function test_browserData_should_not_remove_extension_data() {
  if (!Services.prefs.getBoolPref("dom.storage.next_gen")) {
    // When LSNG isn't enabled, the browsingData API does still clear
    // all the extensions localStorage if called without a list of specific
    // origins to clear.
    info("Test skipped because LSNG is currently disabled");
    return;
  }

  let extension = ExtensionTestUtils.loadExtension({
    async background() {
      window.localStorage.setItem("key", "value");
      await browser.browsingData.removeLocalStorage({}).catch(err => {
        browser.test.fail(`${err} :: ${err.stack}`);
      });
      browser.test.sendMessage("done", window.localStorage.getItem("key"));
    },
    manifest: {
      permissions: ["browsingData"],
    },
  });

  await extension.startup();
  const lsValue = await extension.awaitMessage("done");
  is(lsValue, "value", "Got the expected localStorage data");
  await extension.unload();
});
