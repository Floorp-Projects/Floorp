/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(async function setup() {
  await PlacesUtils.bookmarks.eraseEverything();
  const TEST_EXAMPLE_URL = "http://example.com/";
  const TEST_EXAMPLE_PARAMS = "?foo=1|2";
  const TEST_EXAMPLE_TITLE = "Example Domain";

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_EXAMPLE_URL + TEST_EXAMPLE_PARAMS,
    title: TEST_EXAMPLE_TITLE,
  });

  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.toolbarGuid,
    url: TEST_EXAMPLE_URL,
    title: TEST_EXAMPLE_TITLE,
  });

  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.eraseEverything();
  });
});

add_task(async function test_open_all_in_tabs_from_library() {
  let gLibrary = await promiseLibrary("AllBookmarks");
  gLibrary.PlacesOrganizer.selectLeftPaneBuiltIn("BookmarksToolbar");
  gLibrary.ContentTree.view.selectAll();
  let placesContext = gLibrary.document.getElementById("placesContext");
  let promiseContextMenu = BrowserTestUtils.waitForEvent(
    placesContext,
    "popupshown"
  );
  synthesizeClickOnSelectedTreeCell(gLibrary.ContentTree.view, {
    button: 2,
    type: "contextmenu",
  });
  await promiseContextMenu;
  let openTabs = gLibrary.document.getElementById(
    "placesContext_openBookmarkLinks:tabs"
  );
  let promiseWaitForWindow = BrowserTestUtils.waitForNewWindow();
  placesContext.activateItem(openTabs, { shiftKey: true });
  let newWindow = await promiseWaitForWindow;

  Assert.equal(
    newWindow.browserDOMWindow.tabCount,
    2,
    "Expected number of tabs opened in new window"
  );

  await BrowserTestUtils.closeWindow(newWindow);
  await promiseLibraryClosed(gLibrary);
});
