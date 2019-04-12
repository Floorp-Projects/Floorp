/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_keyword_bookmarklet() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmarklet",
    url: "javascript:'1';",
  });

  registerCleanupFunction(async function() {
    BrowserTestUtils.removeTab(tab);
    await PlacesUtils.bookmarks.remove(bm);
  });

  let originalPrincipal = gBrowser.contentPrincipal;
  let originalPrincipalURI = await getPrincipalURI(tab.linkedBrowser);

  await PlacesUtils.keywords.insert({ keyword: "bm", url: "javascript:'1';" });

  // Enter bookmarklet keyword in the URL bar
  gURLBar.value = "bm";
  gURLBar.focus();
  EventUtils.synthesizeKey("KEY_Enter");

  await BrowserTestUtils.waitForContentEvent(gBrowser.selectedBrowser, "pageshow");

  let newPrincipalURI = await getPrincipalURI(tab.linkedBrowser);
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

function getPrincipalURI(browser) {
  return ContentTask.spawn(browser, null, function() {
    return content.document.nodePrincipal.URI.spec;
  });
}
