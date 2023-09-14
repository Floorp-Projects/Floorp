/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_sender_url() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          matches: ["http://mochi.test/*"],
          run_at: "document_start",
          js: ["script.js"],
        },
      ],
    },

    background() {
      browser.runtime.onMessage.addListener((msg, sender) => {
        browser.test.log("Message received.");
        browser.test.sendMessage("sender.url", sender.url);
      });
    },

    files: {
      "script.js"() {
        browser.test.log("Content script loaded.");
        browser.runtime.sendMessage(0);
      },
    },
  });

  const image =
    "http://mochi.test:8888/browser/browser/components/extensions/test/browser/ctxmenu-image.png";

  // Bug is only visible and test only works without Fission,
  // or with Fission but without BFcache in parent.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  function awaitNewTab() {
    return BrowserTestUtils.waitForLocationChange(gBrowser, "about:newtab");
  }

  await extension.startup();

  await BrowserTestUtils.withNewTab({ gBrowser }, async browser => {
    let newTab = awaitNewTab();
    BrowserTestUtils.startLoadingURIString(browser, "about:newtab");
    await newTab;

    BrowserTestUtils.startLoadingURIString(browser, image);
    let url = await extension.awaitMessage("sender.url");
    is(url, image, `Correct sender.url: ${url}`);

    let wentBack = awaitNewTab();
    await browser.goBack();
    await wentBack;

    await browser.goForward();
    url = await extension.awaitMessage("sender.url");
    is(url, image, `Correct sender.url: ${url}`);
  });

  await extension.unload();
  await SpecialPowers.popPrefEnv();
});
