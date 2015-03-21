"use strict"

add_task(function* test_keyword_bookmarklet() {
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                title: "bookmarklet",
                                                url: "javascript:1;" });
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction (function* () {
    gBrowser.removeTab(tab);
    yield PlacesUtils.bookmarks.remove(bm);
  });
  yield promisePageShow();
  let originalPrincipal = gBrowser.contentPrincipal;

  yield PlacesUtils.keywords.insert({ keyword: "bm", url: "javascript:1;" })

  // Enter bookmarklet keyword in the URL bar
  gURLBar.value = "bm";
  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});

  yield promisePageShow();

  ok(gBrowser.contentPrincipal.equals(originalPrincipal), "javascript bookmarklet should inherit principal");
});

function* promisePageShow() {
  return new Promise(resolve => {
    gBrowser.selectedBrowser.addEventListener("pageshow", function listen() {
      gBrowser.selectedBrowser.removeEventListener("pageshow", listen);
      resolve();
    });
  });
}
