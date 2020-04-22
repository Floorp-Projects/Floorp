/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * Test that we don't show sync pane when it's disabled.
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1536752.
 */
add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["identity.fxaccounts.enabled", false]],
  });
  await openPreferencesViaOpenPreferencesAPI("paneGeneral", {
    leaveOpen: true,
  });
  ok(
    !gBrowser.contentDocument.getElementById("template-paneSync"),
    "sync pane removed"
  );
  ok(
    gBrowser.contentDocument.getElementById("category-sync").hidden,
    "sync category hidden"
  );

  // Check that we don't get any results in sync when searching:
  await evaluateSearchResults("sync", "no-results-message");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
