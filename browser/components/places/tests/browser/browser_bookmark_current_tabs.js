/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { MockRegistrar } = ChromeUtils.importESModule(
  "resource://testing-common/MockRegistrar.sys.mjs"
);

add_setup(() => {
  let mockPromptService = {
    confirmExBC() {
      return 0;
    },
    QueryInterface: ChromeUtils.generateQI(["nsIPromptService"]),
  };
  let mockPromptServiceCID = MockRegistrar.register(
    "@mozilla.org/prompter;1",
    mockPromptService
  );
  registerCleanupFunction(() => {
    MockRegistrar.unregister(mockPromptServiceCID);
  });
});

add_task(async function bookmarkPage() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://user:pass@example.com/"
  );

  let promiseBookmark = PlacesTestUtils.waitForNotification(
    "bookmark-added",
    () => true
  );
  PlacesCommandHook.bookmarkPage();
  await promiseBookmark;

  let bookmark = await PlacesUtils.bookmarks.fetch({
    url: "https://example.com/",
  });
  Assert.ok(bookmark, "Found the expected bookmark");

  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.bookmarks.eraseEverything();
});

add_task(async function bookmarkTabs() {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://user:pass@example.com/"
  );

  await withBookmarksDialog(
    false,
    PlacesCommandHook.bookmarkTabs,
    async dialog => {
      dialog.document
        .getElementById("bookmarkpropertiesdialog")
        .getButton("accept")
        .click();
    }
  );

  let bookmark = await PlacesUtils.bookmarks.fetch({
    url: "https://example.com/",
  });
  Assert.ok(bookmark, "Found the expected bookmark");

  BrowserTestUtils.removeTab(tab);
  await PlacesUtils.bookmarks.eraseEverything();
});
