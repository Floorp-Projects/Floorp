/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we don't show moreFromMozilla pane when it's disabled.
 */
add_task(async function testwhenPrefDisabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.moreFromMozilla", false]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );
  ok(moreFromMozillaCategory, "The category exists");
  ok(moreFromMozillaCategory.hidden, "The category is hidden");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});

add_task(async function testwhenPrefEnabled() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.preferences.moreFromMozilla", true]],
  });

  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  let doc = gBrowser.contentDocument;

  let moreFromMozillaCategory = doc.getElementById(
    "category-more-from-mozilla"
  );
  ok(moreFromMozillaCategory, "The category exists");
  ok(!moreFromMozillaCategory.hidden, "The category is not hidden");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
