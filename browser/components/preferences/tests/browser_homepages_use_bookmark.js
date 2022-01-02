/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL1 = "http://example.com/1";
const TEST_URL2 = "http://example.com/2";

add_task(async function setup() {
  let oldHomepagePref = Services.prefs.getCharPref("browser.startup.homepage");

  await openPreferencesViaOpenPreferencesAPI("paneHome", { leaveOpen: true });

  Assert.equal(
    gBrowser.currentURI.spec,
    "about:preferences#home",
    "#home should be in the URI for about:preferences"
  );

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
    url: TEST_URL1,
  });

  let doc = gBrowser.contentDocument;
  // Select the custom URLs option.
  doc.getElementById("homeMode").value = 2;

  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/selectBookmark.xhtml"
  );
  doc.getElementById("useBookmarkBtn").click();

  let dialog = await promiseSubDialogLoaded;
  dialog.document.getElementById("bookmarks").selectItems([bm.guid]);
  dialog.document
    .getElementById("selectBookmarkDialog")
    .getButton("accept")
    .click();

  await TestUtils.waitForCondition(() => HomePage.get() == TEST_URL1);

  Assert.equal(
    HomePage.get(),
    TEST_URL1,
    "Should have set the homepage to the same as the bookmark."
  );
});

add_task(async function testSetHomepageFromTopLevelFolder() {
  // Insert a second item into the menu folder
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    title: "TestHomepage",
    url: TEST_URL2,
  });

  let doc = gBrowser.contentDocument;
  // Select the custom URLs option.
  doc.getElementById("homeMode").value = 2;

  let promiseSubDialogLoaded = promiseLoadSubDialog(
    "chrome://browser/content/preferences/dialogs/selectBookmark.xhtml"
  );
  doc.getElementById("useBookmarkBtn").click();

  let dialog = await promiseSubDialogLoaded;
  dialog.document
    .getElementById("bookmarks")
    .selectItems([PlacesUtils.bookmarks.menuGuid]);
  dialog.document
    .getElementById("selectBookmarkDialog")
    .getButton("accept")
    .click();

  await TestUtils.waitForCondition(
    () => HomePage.get() == `${TEST_URL1}|${TEST_URL2}`
  );

  Assert.equal(
    HomePage.get(),
    `${TEST_URL1}|${TEST_URL2}`,
    "Should have set the homepage to the same as the bookmark."
  );
});
