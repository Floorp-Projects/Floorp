"use strict";

async function doLoadAndGoBack(browser, ext) {
  let loaded = BrowserTestUtils.browserLoaded(browser);
  BrowserTestUtils.startLoadingURIString(browser, "https://example.com/");
  await ext.awaitMessage("redir-handled");
  await loaded;

  let pageShownPromise = BrowserTestUtils.waitForContentEvent(
    browser,
    "pageshow",
    true
  );
  await SpecialPowers.spawn(browser, [], () => {
    content.history.back();
  });
  return pageShownPromise;
}

add_task(async function test_back() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      permissions: ["webRequest", "webRequestBlocking", "https://example.com/"],
      web_accessible_resources: ["test.html"],
    },
    files: {
      "test.html":
        "<!DOCTYPE html><html><head><title>Test add-on</title></head><body></body></html>",
    },
    background: () => {
      let { browser } = this;
      browser.webRequest.onHeadersReceived.addListener(
        details => {
          if (details.statusCode != 200) {
            return undefined;
          }
          browser.test.sendMessage("redir-handled");
          return { redirectUrl: browser.runtime.getURL("test.html") };
        },
        {
          urls: ["https://example.com/"],
          types: ["main_frame"],
        },
        ["blocking"]
      );
    },
  });

  await extension.startup();

  await BrowserTestUtils.withNewTab("about:home", async function (browser) {
    await doLoadAndGoBack(browser, extension);

    await SpecialPowers.spawn(browser, [], () => {
      is(
        content.document.documentURI,
        "about:home",
        "Gone back to the right page"
      );
    });

    await doLoadAndGoBack(browser, extension);

    await SpecialPowers.spawn(browser, [], () => {
      is(
        content.document.documentURI,
        "about:home",
        "Gone back to the right page"
      );
    });
  });

  await extension.unload();
});
