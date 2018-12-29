/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:support" }, async function(browser) {
    let keyGoogleStatus = await ContentTask.spawn(browser, null, async function() {
      let textBox = content.document.getElementById("key-google-box");
      await ContentTaskUtils.waitForCondition(() => content.document.l10n.getAttributes(textBox).id,
        "Google API key status loaded");
      return content.document.l10n.getAttributes(textBox).id;
    });
    ok(keyGoogleStatus, "Google API key status shown");

    let keyMozillaStatus = await ContentTask.spawn(browser, null, async function() {
      let textBox = content.document.getElementById("key-mozilla-box");
      await ContentTaskUtils.waitForCondition(() => content.document.l10n.getAttributes(textBox).id,
        "Mozilla API key status loaded");
      return content.document.l10n.getAttributes(textBox).id;
    });
    ok(keyMozillaStatus, "Mozilla API key status shown");
  });
});
