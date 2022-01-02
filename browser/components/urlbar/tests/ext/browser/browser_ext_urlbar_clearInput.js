/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/* global browser */

// This tests the browser.experiments.urlbar.clearInput WebExtension Experiment
// API.

"use strict";

add_task(async function test() {
  // Load a page so that pageproxystate is valid.  When the extension calls
  // clearInput, the pageproxystate should become invalid.
  await BrowserTestUtils.withNewTab("http://example.com/", async () => {
    Assert.notEqual(gURLBar.value, "", "Input is not empty");
    Assert.equal(gURLBar.getAttribute("pageproxystate"), "valid");

    let ext = await loadExtension({
      background: async () => {
        await browser.experiments.urlbar.clearInput();
        browser.test.sendMessage("done");
      },
    });
    await ext.awaitMessage("done");

    Assert.equal(gURLBar.value, "", "Input is empty");
    Assert.equal(gURLBar.getAttribute("pageproxystate"), "invalid");

    await ext.unload();
  });
});
