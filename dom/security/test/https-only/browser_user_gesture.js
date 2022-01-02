// Bug 1725026 - HTTPS Only Mode - Test if a load triggered by a user gesture
// https://bugzilla.mozilla.org/show_bug.cgi?id=1725026
// Test if a load triggered by a user gesture can be upgraded to HTTPS
// successfully.

"use strict";

const testPathUpgradeable = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://example.com"
);

const kTestURI = testPathUpgradeable + "file_user_gesture.html";

add_task(async function() {
  // Enable HTTPS-Only Mode and register console-listener
  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_only_mode", true]],
  });

  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    const loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    // 1. Upgrade a page to https://
    BrowserTestUtils.loadURI(browser, kTestURI);
    await loaded;
    await ContentTask.spawn(browser, {}, async args => {
      ok(
        content.document.location.href.startsWith("https://"),
        "Should be https"
      );

      // 2. Trigger a load by clicking button.
      // The scheme of the link url is `http` and the load should be able to
      // upgraded to `https` because of HTTPS-only mode.
      let button = content.document.getElementById("httpLinkButton");
      await EventUtils.synthesizeMouseAtCenter(
        button,
        { type: "mousedown" },
        content
      );
      await EventUtils.synthesizeMouseAtCenter(
        button,
        { type: "mouseup" },
        content
      );
      await ContentTaskUtils.waitForCondition(() => {
        return content.document.location.href.startsWith("https://");
      });
      ok(
        content.document.location.href.startsWith("https://"),
        "Should be https"
      );
    });
  });
});
