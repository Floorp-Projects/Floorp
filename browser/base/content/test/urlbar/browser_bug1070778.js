/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function is_selected(index) {
  is(gURLBar.popup.richlistbox.selectedIndex, index, `Item ${index + 1} should be selected`);
}

add_task(async function() {
  let bookmarks = [];
  bookmarks.push((await PlacesUtils.bookmarks
                                   .insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                             url: "http://example.com/?q=%s",
                                             title: "test" })));
  await PlacesUtils.keywords.insert({ keyword: "keyword",
                                      url: "http://example.com/?q=%s" });

  // This item only needed so we can select the keyword item, select something
  // else, then select the keyword item again.
  bookmarks.push((await PlacesUtils.bookmarks
                                   .insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                             url: "http://example.com/keyword",
                                             title: "keyword abc" })));

  registerCleanupFunction(async function() {
    for (let bm of bookmarks) {
      await PlacesUtils.bookmarks.remove(bm);
    }
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:mozilla");
  await promiseAutocompleteResultPopup("keyword a");
  await waitForAutocompleteResultAt(1);

  // First item should already be selected
  is_selected(0);
  // Select next one (important!)
  EventUtils.synthesizeKey("VK_DOWN", {});
  is_selected(1);
  // Re-select keyword item
  EventUtils.synthesizeKey("VK_UP", {});
  is_selected(0);

  EventUtils.synthesizeKey("b", {});
  await promiseSearchComplete();

  is(gURLBar.textValue, "keyword ab", "urlbar should have expected input");
  let result = await waitForAutocompleteResultAt(0);
  isnot(result, null, "Should have first item");
  let uri = NetUtil.newURI(result.getAttribute("url"));
  is(uri.spec, PlacesUtils.mozActionURI("keyword", {url: "http://example.com/?q=ab", input: "keyword ab"}), "Expect correct url");

  EventUtils.synthesizeKey("VK_ESCAPE", {});
  await promisePopupHidden(gURLBar.popup);
  gBrowser.removeTab(tab);
});
