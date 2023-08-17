/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.privatebrowsing.felt-privacy-v1", true]],
  });
});

add_task(async function check_for_simplified_pbm_ui() {
  let pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });

  let pocketButton = pbmWindow.document.getElementById("save-to-pocket-button");
  ok(!pocketButton, "Pocket button is removed from PBM window");
  let bookmarksBar = pbmWindow.document.getElementById("PersonalToolbar");
  ok(
    bookmarksBar.getAttribute("collapsed"),
    "Bookmarks bar is hidden in PBM window"
  );

  await BrowserTestUtils.openNewForegroundTab(pbmWindow, "about:blank", true);
  ok(
    bookmarksBar.getAttribute("collapsed"),
    "Bookmarks bar is hidden in PBM window after loading a new tab"
  );

  await BrowserTestUtils.closeWindow(pbmWindow);
  await SpecialPowers.pushPrefEnv({
    set: [["browser.toolbars.bookmarks.showInPrivateBrowsing", true]],
  });
  pbmWindow = await BrowserTestUtils.openNewBrowserWindow({
    private: true,
  });
  bookmarksBar = pbmWindow.document.getElementById("PersonalToolbar");
  console.info(bookmarksBar.getAttribute("collapsed"));
  ok(
    bookmarksBar.getAttribute("collapsed").toString() == "false",
    "Bookmarks bar is visible in PBM window when showInPrivateBrowsing pref is true"
  );

  await BrowserTestUtils.closeWindow(pbmWindow);
});
