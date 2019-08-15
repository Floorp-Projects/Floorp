"use strict";

const PAGE =
  "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";

// On debug builds, crashing tabs results in much thinking, which
// slows down the test and results in intermittent test timeouts,
// so we'll pump up the expected timeout for this test.
requestLongerTimeout(2);

/**
 * Tests that we show the about:tabcrashed additional details form
 * if the "submit a crash report" checkbox was checked by default.
 */
add_task(async function test_show_form() {
  return BrowserTestUtils.withNewTab(
    {
      gBrowser,
      url: PAGE,
    },
    async function(browser) {
      // Flip the pref so that the checkbox should be checked
      // by default.
      let pref = TabCrashHandler.prefs.root + "sendReport";
      await SpecialPowers.pushPrefEnv({
        set: [[pref, true]],
      });

      // Now crash the browser.
      await BrowserTestUtils.crashBrowser(browser);

      let doc = browser.contentDocument;

      // Ensure the checkbox is checked. We can safely reach into
      // the content since about:tabcrashed is an in-process URL.
      let checkbox = doc.getElementById("sendReport");
      ok(checkbox.checked, "Send report checkbox is checked.");

      // Ensure the options form is displayed.
      let options = doc.getElementById("options");
      ok(!options.hidden, "Showing the crash report options form.");
    }
  );
});
