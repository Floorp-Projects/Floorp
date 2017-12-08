"use strict";

add_task(async function test_keyword_bookmarklet() {
  let bm = await PlacesUtils.bookmarks.insert({ parentGuid: PlacesUtils.bookmarks.unfiledGuid,
                                                title: "bookmarklet",
                                                url: "javascript:'1';" });
  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  registerCleanupFunction(async function() {
    gBrowser.removeTab(tab);
    await PlacesUtils.bookmarks.remove(bm);
  });
  await promisePageShow();
  let originalPrincipal = gBrowser.contentPrincipal;

  function getPrincipalURI() {
    return ContentTask.spawn(tab.linkedBrowser, null, function() {
      return content.document.nodePrincipal.URI.spec;
    });
  }

  let originalPrincipalURI = await getPrincipalURI();

  await PlacesUtils.keywords.insert({ keyword: "bm", url: "javascript:'1';" });

  // Enter bookmarklet keyword in the URL bar
  gURLBar.value = "bm";
  gURLBar.focus();
  EventUtils.synthesizeKey("VK_RETURN", {});

  await promisePageShow();

  let newPrincipalURI = await getPrincipalURI();
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

function promisePageShow() {
  return BrowserTestUtils.waitForContentEvent(gBrowser.selectedBrowser, "pageshow");
}
