/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test the initial value of Browser Error collection checkbox
add_task(async function testBrowserErrorInitialValue() {
  // Skip if non-Nightly since the checkbox will be missing.
  if (!AppConstants.NIGHTLY_BUILD) {
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [["browser.chrome.errorReporter.enabled", true]],
  });
  await openPreferencesViaOpenPreferencesAPI("privacy-reports", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  ok(
    doc.querySelector("#collectBrowserErrorsBox").checked,
    "Checkbox for collecting browser errors should be checked when the pref is true"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});

// Test that the Learn More link is set to the correct, formatted URL from a
// pref value
add_task(async function testBrowserErrorLearnMore() {
  // Skip if non-Nightly since the checkbox will be missing.
  if (!AppConstants.NIGHTLY_BUILD) {
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [["browser.chrome.errorReporter.infoURL", "https://example.com/%NAME%/"]],
  });
  await openPreferencesViaOpenPreferencesAPI("privacy-reports", {leaveOpen: true});

  let doc = gBrowser.contentDocument;
  is(
    doc.querySelector("#collectBrowserErrorsLearnMore").href,
    `https://example.com/${Services.appinfo.name}/`,
    "Learn More link for browser error collection should have an href set by a pref"
  );

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  await SpecialPowers.popPrefEnv();
});
