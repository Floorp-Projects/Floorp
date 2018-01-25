/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/AppConstants.jsm");
Cu.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  await BrowserTestUtils.withNewTab({ gBrowser, url: "about:support" }, async function(browser) {
    const strings = Services.strings.createBundle(
                      "chrome://global/locale/aboutSupport.properties");
    let allowedStates = [strings.GetStringFromName("found"),
                         strings.GetStringFromName("missing")];

    let keyGoogleStatus = await ContentTask.spawn(browser, null, async function() {
      let textBox = content.document.getElementById("key-google-box");
      await ContentTaskUtils.waitForCondition(() => textBox.textContent.trim(),
        "Google API key status loaded");
      return textBox.textContent;
    });
    ok(allowedStates.includes(keyGoogleStatus), "Google API key status shown");

    let keyMozillaStatus = await ContentTask.spawn(browser, null, async function() {
      let textBox = content.document.getElementById("key-mozilla-box");
      await ContentTaskUtils.waitForCondition(() => textBox.textContent.trim(),
        "Mozilla API key status loaded");
      return textBox.textContent;
    });
    ok(allowedStates.includes(keyMozillaStatus), "Mozilla API key status shown");
  });
});
