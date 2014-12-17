/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function is_selected(index) {
  is(gURLBar.popup.richlistbox.selectedIndex, index, `Item ${index + 1} should be selected`);
}

add_task(function*() {
  // This test is only relevant if UnifiedComplete is enabled.
  if (!Services.prefs.getBoolPref("browser.urlbar.unifiedcomplete")) {
    todo(false, "Stop supporting old autocomplete components.");
    return;
  }

  registerCleanupFunction(() => {
    PlacesUtils.bookmarks.removeFolderChildren(PlacesUtils.unfiledBookmarksFolderId);
  });

  let itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         NetUtil.newURI("http://example.com/?q=%s"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "test");
  PlacesUtils.bookmarks.setKeywordForBookmark(itemId, "keyword");

  // This item only needed so we can select the keyword item, select something
  // else, then select the keyword item again.
  itemId =
    PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                         NetUtil.newURI("http://example.com/keyword"),
                                         PlacesUtils.bookmarks.DEFAULT_INDEX,
                                         "keyword abc");

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:mozilla", {animate: false});
  yield promiseTabLoaded(tab);
  yield promiseAutocompleteResultPopup("keyword a");

  // First item should already be selected
  is_selected(0);
  // Select next one (important!)
  EventUtils.synthesizeKey("VK_DOWN", {});
  is_selected(1);
  // Re-select keyword item
  EventUtils.synthesizeKey("VK_UP", {});
  is_selected(0);

  EventUtils.synthesizeKey("b", {});
  yield promiseSearchComplete();

  is(gURLBar.value, "keyword ab", "urlbar should have expected input");

  let result = gURLBar.popup.richlistbox.firstChild;
  isnot(result, null, "Should have first item");
  let uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, makeActionURI("keyword", {url: "http://example.com/?q=ab", input: "keyword ab"}).spec, "Expect correct url");

  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});
