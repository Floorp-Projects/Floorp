"use strict";

const PAGE = "data:text/html,<html><body>A%20regular,%20everyday,%20normal%20page.";
const AUTOSUBMIT_PREF = "browser.crashReports.unsubmittedCheck.autoSubmit";

const {TabStateFlusher} =
  Cu.import("resource:///modules/sessionstore/TabStateFlusher.jsm", {});

// On debug builds, crashing tabs results in much thinking, which
// slows down the test and results in intermittent test timeouts,
// so we'll pump up the expected timeout for this test.
requestLongerTimeout(2);

/**
 * Tests that if the user is not configured to autosubmit
 * backlogged crash reports, that we offer to do that, and
 * that the user can accept that offer.
 */
add_task(function* test_show_form() {
  yield SpecialPowers.pushPrefEnv({
    set: [[AUTOSUBMIT_PREF, false]],
  })

  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, function*(browser) {
    // Make sure we've flushed the browser messages so that
    // we can restore it.
    yield TabStateFlusher.flush(browser);

    // Now crash the browser.
    yield BrowserTestUtils.crashBrowser(browser);

    let doc = browser.contentDocument;

    // Ensure the request is visible. We can safely reach into
    // the content since about:tabcrashed is an in-process URL.
    let requestAutoSubmit = doc.getElementById("requestAutoSubmit");
    Assert.ok(!requestAutoSubmit.hidden,
              "Request for autosubmission is visible.");

    // Since the pref is set to false, the checkbox should be
    // unchecked.
    let autoSubmit = doc.getElementById("autoSubmit");
    Assert.ok(!autoSubmit.checked,
              "Checkbox for autosubmission is not checked.")

    // Check the checkbox, and then restore the tab.
    autoSubmit.checked = true;
    let restoreButton = doc.getElementById("restoreTab");
    restoreButton.click();

    yield BrowserTestUtils.browserLoaded(browser, false, PAGE);

    // The autosubmission pref should now be set.
    Assert.ok(Services.prefs.getBoolPref(AUTOSUBMIT_PREF),
              "Autosubmission pref should have been set.");
  });
});

/**
 * Tests that if the user is autosubmitting backlogged crash reports
 * that we don't make the offer again.
 */
add_task(function* test_show_form() {
  yield SpecialPowers.pushPrefEnv({
    set: [[AUTOSUBMIT_PREF, true]],
  })

  return BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE,
  }, function*(browser) {
    yield TabStateFlusher.flush(browser);
    // Now crash the browser.
    yield BrowserTestUtils.crashBrowser(browser);

    let doc = browser.contentDocument;

    // Ensure the request is NOT visible. We can safely reach into
    // the content since about:tabcrashed is an in-process URL.
    let requestAutoSubmit = doc.getElementById("requestAutoSubmit");
    Assert.ok(requestAutoSubmit.hidden,
              "Request for autosubmission is not visible.");

    // Restore the tab.
    let restoreButton = doc.getElementById("restoreTab");
    restoreButton.click();

    yield BrowserTestUtils.browserLoaded(browser, false, PAGE);

    // The autosubmission pref should still be set to true.
    Assert.ok(Services.prefs.getBoolPref(AUTOSUBMIT_PREF),
              "Autosubmission pref should have been set.");
  });
});
