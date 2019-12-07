/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: "about:support" },
    async function(browser) {
      let keyLocationServiceGoogleStatus = await ContentTask.spawn(
        browser,
        null,
        async function() {
          let textBox = content.document.getElementById(
            "key-location-service-google-box"
          );
          await ContentTaskUtils.waitForCondition(
            () => content.document.l10n.getAttributes(textBox).id,
            "Google location service API key status loaded"
          );
          return content.document.l10n.getAttributes(textBox).id;
        }
      );
      ok(
        keyLocationServiceGoogleStatus,
        "Google location service API key status shown"
      );

      let keySafebrowsingGoogleStatus = await ContentTask.spawn(
        browser,
        null,
        async function() {
          let textBox = content.document.getElementById(
            "key-safebrowsing-google-box"
          );
          await ContentTaskUtils.waitForCondition(
            () => content.document.l10n.getAttributes(textBox).id,
            "Google Safebrowsing API key status loaded"
          );
          return content.document.l10n.getAttributes(textBox).id;
        }
      );
      ok(
        keySafebrowsingGoogleStatus,
        "Google Safebrowsing API key status shown"
      );

      let keyMozillaStatus = await ContentTask.spawn(
        browser,
        null,
        async function() {
          let textBox = content.document.getElementById("key-mozilla-box");
          await ContentTaskUtils.waitForCondition(
            () => content.document.l10n.getAttributes(textBox).id,
            "Mozilla API key status loaded"
          );
          return content.document.l10n.getAttributes(textBox).id;
        }
      );
      ok(keyMozillaStatus, "Mozilla API key status shown");
    }
  );
});
