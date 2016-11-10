"use strict"

add_task(function* test_keyword_bookmarklet() {
  let bm = yield PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                title: "bookmarklet",
                                                url: "javascript:'1';" });
  let tab = gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function* () {
    gBrowser.removeTab(tab);
    yield PlacesUtils.bookmarks.remove(bm);
  });
  yield promisePageShow();
  let originalPrincipal = gBrowser.contentPrincipal;

  function getPrincipalURI() {
    return ContentTask.spawn(tab.linkedBrowser, null, function() {
      return content.document.nodePrincipal.URI.spec;
    });
  }

  let originalPrincipalURI = yield getPrincipalURI();

  yield PlacesUtils.keywords.insert({ keyword: "bm", url: "javascript:'1';" })

  // Enter bookmarklet keyword in the URL bar
  gURLBar.value = "bm";
  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});

  yield promisePageShow();

  let newPrincipalURI = yield getPrincipalURI();
  is(newPrincipalURI, originalPrincipalURI, "content has the same principal");

  // In e10s, null principals don't round-trip so the same null principal sent
  // from the child will be a new null principal. Verify that this is the
  // case.
  if (tab.linkedBrowser.isRemoteBrowser) {
    ok(originalPrincipal.isNullPrincipal && gBrowser.contentPrincipal.isNullPrincipal,
       "both principals should be null principals in the parent");
  } else {
    ok(gBrowser.contentPrincipal.equals(originalPrincipal),
       "javascript bookmarklet should inherit principal");
  }
});

function* promisePageShow() {
  return new Promise(resolve => {
    gBrowser.selectedBrowser.addEventListener("pageshow", function listen() {
      gBrowser.selectedBrowser.removeEventListener("pageshow", listen);
      resolve();
    });
  });
}
