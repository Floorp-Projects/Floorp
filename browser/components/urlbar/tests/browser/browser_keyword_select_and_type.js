/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/**
 * This tests that changing away from a keyword result and back again, still
 * operates correctly.
 */

add_task(async function() {
  let bookmarks = [];
  bookmarks.push(
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "http://example.com/?q=%s",
      title: "test",
    })
  );
  await PlacesUtils.keywords.insert({
    keyword: "keyword",
    url: "http://example.com/?q=%s",
  });

  // This item is only needed so we can select the keyword item, select something
  // else, then select the keyword item again.
  bookmarks.push(
    await PlacesUtils.bookmarks.insert({
      parentGuid: PlacesUtils.bookmarks.unfiledGuid,
      url: "http://example.com/keyword",
      title: "keyword abc",
    })
  );

  registerCleanupFunction(async function() {
    for (let bm of bookmarks) {
      await PlacesUtils.bookmarks.remove(bm);
    }
  });

  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:mozilla"
  );
  await promiseAutocompleteResultPopup("keyword a");
  await waitForAutocompleteResultAt(1);

  // First item should already be selected
  Assert.equal(
    UrlbarTestUtils.getSelectedIndex(window),
    0,
    "Should have the first item selected"
  );

  // Select next one (important!)
  EventUtils.synthesizeKey("KEY_ArrowDown");
  Assert.equal(
    UrlbarTestUtils.getSelectedIndex(window),
    1,
    "Should have the second item selected"
  );

  // Re-select keyword item
  EventUtils.synthesizeKey("KEY_ArrowUp");
  Assert.equal(
    UrlbarTestUtils.getSelectedIndex(window),
    0,
    "Should have the first item selected"
  );

  EventUtils.sendString("b");
  await promiseSearchComplete();

  Assert.equal(
    gURLBar.value,
    "keyword ab",
    "urlbar should have expected input"
  );

  let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);

  Assert.equal(
    result.type,
    UrlbarUtils.RESULT_TYPE.KEYWORD,
    "Should have a result of type keyword"
  );
  Assert.equal(
    result.url,
    "http://example.com/?q=ab",
    "Should have the correct url"
  );

  gBrowser.removeTab(tab);
});
