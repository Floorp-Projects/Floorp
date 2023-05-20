/* Any copyright is dedicated to the Public Domain.
 * https://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test that if browser.bookmarks.defaultLocation contains an invalid GUID,
 * PlacesUIUtils.defaultParentGuid will return a proper default value.
 */

add_task(async function () {
  Services.prefs.setCharPref(
    "browser.bookmarks.defaultLocation",
    "useOtherBookmarks"
  );

  info(
    "Checking that default parent guid was set back to the toolbar because of invalid preferable guid"
  );
  Assert.equal(
    await PlacesUIUtils.defaultParentGuid,
    PlacesUtils.bookmarks.toolbarGuid,
    "Default parent guid is a toolbar guid"
  );
});
