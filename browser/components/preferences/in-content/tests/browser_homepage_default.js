/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function default_homepage_test() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.startup.page", 1]],
  });
  let defaults = Services.prefs.getDefaultBranch("");
  // Simulate a homepage set via policy or a distribution.
  defaults.setStringPref("browser.startup.homepage", "https://example.com");

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });

  let doc = gBrowser.contentDocument;
  let homeMode = doc.getElementById("homeMode");
  Assert.equal(homeMode.value, 2, "homeMode should be 2 (Custom URL)");

  let homePageUrl = doc.getElementById("homePageUrl");
  Assert.equal(
    homePageUrl.value,
    "https://example.com",
    "homePageUrl should be example.com"
  );

  registerCleanupFunction(async () => {
    defaults.setStringPref("browser.startup.homepage", "about:home");
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
  });
});
