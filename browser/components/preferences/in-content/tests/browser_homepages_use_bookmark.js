/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = "http://example.com";

add_task(async function setup() {
  let oldHomepagePref = Services.prefs.getCharPref("browser.startup.homepage");

  await openPreferencesViaOpenPreferencesAPI("paneHome", {leaveOpen: true});

  Assert.ok(gBrowser.currentURI.spec, "about:preferences#home",
            "#home should be in the URI for about:preferences");

  registerCleanupFunction(async () => {
    Services.prefs.setCharPref("browser.startup.homepage", oldHomepagePref);
    BrowserTestUtils.removeTab(gBrowser.selectedTab);
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function testSetHomepageFromBookmark() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "TestHomepage",
    url: TEST_URL
  });

  let doc = gBrowser.contentDocument;

  // Select the custom URLs option.
  doc.getElementById("homeMode").value = 2;

  let promiseSubDialogLoaded = promiseLoadSubDialog("chrome://browser/content/preferences/selectBookmark.xul");

  doc.getElementById("useBookmarkBtn").click();

  let dialog = await promiseSubDialogLoaded;

  dialog.document.getElementById("bookmarks").selectItems([bm.guid]);

  dialog.document.documentElement.getButton("accept").click();

  Assert.ok(Services.prefs.getCharPref("browser.startup.homepage"), TEST_URL,
            "Should have set the homepage to the same as the bookmark.");
});
