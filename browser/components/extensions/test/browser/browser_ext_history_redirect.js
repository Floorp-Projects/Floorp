"use strict";

const BASE = "http://mochi.test:8888/browser/browser/components/extensions/test/browser";
const REDIRECT_URL = BASE + "/redirection.sjs";
const REDIRECTED_URL = BASE + "/dummy_page.html";

add_task(async function history_redirect() {
  function background() {
    browser.test.onMessage.addListener(async (msg, url) => {
      switch (msg) {
        case "delete-all": {
          let results = await browser.history.deleteAll();
          browser.test.sendMessage("delete-all-result", results);
          break;
        }
        case "search": {
          let results = await browser.history.search({text: url, startTime: new Date(0)});
          browser.test.sendMessage("search-result", results);
          break;
        }
        case "get-visits": {
          let results = await browser.history.getVisits({url});
          browser.test.sendMessage("get-visits-result", results);
          break;
        }
      }
    });

    browser.test.sendMessage("ready");
  }

  let extensionData = {
    manifest: {
      permissions: [
        "history",
      ],
    },
    background,
  };

  let extension = ExtensionTestUtils.loadExtension(extensionData);

  await Promise.all([extension.startup(), extension.awaitMessage("ready")]);
  info("extension loaded");

  extension.sendMessage("delete-all");
  await extension.awaitMessage("delete-all-result");

  await BrowserTestUtils.withNewTab({gBrowser, url: REDIRECT_URL}, async (browser) => {
    is(browser.currentURI.spec, REDIRECTED_URL, "redirected to the expected location");

    extension.sendMessage("search", REDIRECT_URL);
    let results = await extension.awaitMessage("search-result");
    is(results.length, 1, "search returned expected length of results");

    extension.sendMessage("get-visits", REDIRECT_URL);
    let visits = await extension.awaitMessage("get-visits-result");
    is(visits.length, 1, "getVisits returned expected length of visits");
  });

  await extension.unload();
  info("extension unloaded");
});
