/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that copying from the urlbar page works correctly after a result is
// confirmed but takes a while to load.

add_task(async function () {
  const SLOW_PAGE =
    getRootDirectory(gTestPath).replace(
      "chrome://mochitests/content",
      "http://www.example.com"
    ) + "slow-page.sjs";

  await BrowserTestUtils.withNewTab(gBrowser, async () => {
    gURLBar.focus();
    gURLBar.value = SLOW_PAGE;
    let promise = TestUtils.waitForCondition(
      () => gURLBar.getAttribute("pageproxystate") == "invalid"
    );
    EventUtils.synthesizeKey("KEY_Enter");
    info("wait for the initial conditions");
    await promise;

    info("Copy the whole url");
    await SimpleTest.promiseClipboardChange(SLOW_PAGE, () => {
      gURLBar.select();
      goDoCommand("cmd_copy");
    });

    info("Copy the initial part of the url, as a different valid url");
    await SimpleTest.promiseClipboardChange(
      SLOW_PAGE.substring(0, SLOW_PAGE.indexOf("slow-page.sjs")),
      () => {
        gURLBar.selectionStart = 0;
        gURLBar.selectionEnd = gURLBar.value.indexOf("slow-page.sjs");
        goDoCommand("cmd_copy");
      }
    );

    // This is apparently necessary to avoid a timeout on mochitest shutdown(!?)
    let browserStoppedPromise = BrowserTestUtils.browserStopped(
      gBrowser,
      null,
      true
    );
    BrowserCommands.stop();
    await browserStoppedPromise;
  });
});
