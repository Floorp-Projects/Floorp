/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

add_task(async function test_setup() {
  await SpecialPowers.pushPrefEnv({set: [["extensions.pocket.site",
        "example.com/browser/browser/components/pocket/test/pocket_actions_test.html"]],
  });
});

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser,
    "https://example.com/browser/browser/components/pocket/test/test.html");

  let libraryButton = document.getElementById("library-button");
  let libraryView = document.getElementById("appMenu-libraryView");

  info("opening library menu");
  let libraryPromise = BrowserTestUtils.waitForEvent(libraryView, "ViewShown");
  libraryButton.click();
  await libraryPromise;

  let pocketLibraryButton = document.getElementById("appMenu-library-pocket-button");
  ok(pocketLibraryButton, "library menu should have pocket button");
  is(pocketLibraryButton.disabled, false, "element appMenu-library-pocket-button is not disabled");

  info("clicking on pocket library button");
  let pocketPagePromise = BrowserTestUtils.waitForNewTab(gBrowser,
    "https://example.com/browser/browser/components/pocket/test/pocket_actions_test.html/firefox_learnmore?src=ff_library");
  pocketLibraryButton.click();
  await pocketPagePromise;

  is(gBrowser.currentURI.spec,
    "https://example.com/browser/browser/components/pocket/test/pocket_actions_test.html/firefox_learnmore?src=ff_library",
    "pocket button in library menu button opens correct page");

  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
});
