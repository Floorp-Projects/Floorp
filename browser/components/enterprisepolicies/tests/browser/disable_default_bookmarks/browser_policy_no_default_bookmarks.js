/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test must run in a separate folder because the
// No Default Bookmarks policy needs to be present on
// the first run of the profile, and not dinamically loaded
// like most of the policies tested in the main test folder.

add_task(async function test_no_default_bookmarks() {
  let firstBookmarkOnToolbar = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    index: 0,
  });

  let firstBookmarkOnMenu = await PlacesUtils.bookmarks.fetch({
    parentGuid: PlacesUtils.bookmarks.menuGuid,
    index: 0,
  });

  is(firstBookmarkOnToolbar, null, "No bookmarks on toolbar");
  is(firstBookmarkOnMenu, null, "No bookmarks on menu");
});
