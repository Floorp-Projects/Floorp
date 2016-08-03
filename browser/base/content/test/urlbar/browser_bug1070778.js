/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function is_selected(index) {
  is(gURLBar.popup.richlistbox.selectedIndex, index, `Item ${index + 1} should be selected`);
}

add_task(function*() {
  let bookmarks = [];
  bookmarks.push((yield PlacesUtils.bookmarks
                                   .insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                             url: "http://example.com/?q=%s",
                                             title: "test" })));
  yield PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example.com/?q=%s" });

  // This item only needed so we can select the keyword item, select something
  // else, then select the keyword item again.
  bookmarks.push((yield PlacesUtils.bookmarks
                                   .insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                             url: "http://example.com/keyword",
                                             title: "keyword abc" })));

  registerCleanupFunction(function* () {
    for (let bm of bookmarks) {
      yield PlacesUtils.bookmarks.remove(bm);
    }
  });

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

  is(gURLBar.textValue, "keyword ab", "urlbar should have expected input");

  let result = gURLBar.popup.richlistbox.firstChild;
  isnot(result, null, "Should have first item");
  let uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, makeActionURI("keyword", {url: "http://example.com/?q=ab", input: "keyword ab"}).spec, "Expect correct url");

  EventUtils.synthesizeKey("VK_ESCAPE", {});
  yield promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});
