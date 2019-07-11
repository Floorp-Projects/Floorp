/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function setup() {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmarklet",
    url: "javascript:'%s'%20",
  });
  await PlacesUtils.keywords.insert({ keyword: "bm", url: bm.url });
  registerCleanupFunction(async function() {
    await PlacesUtils.bookmarks.remove(bm);
  });

  let testFns = [
    function() {
      info("Type keyword and immediately press enter");
      gURLBar.value = "bm";
      gURLBar.focus();
      EventUtils.synthesizeKey("KEY_Enter");
      return "javascript:''%20";
    },
    function() {
      info("Type keyword with searchstring and immediately press enter");
      gURLBar.value = "bm a";
      gURLBar.focus();
      EventUtils.synthesizeKey("KEY_Enter");
      return "javascript:'a'%20";
    },
    async function() {
      info("Search keyword, then press enter");
      await promiseAutocompleteResultPopup("bm");
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(result.title, "javascript:'' ", "Check title");
      EventUtils.synthesizeKey("KEY_Enter");
      return "javascript:''%20";
    },
    async function() {
      info("Search keyword with searchstring, then press enter");
      await promiseAutocompleteResultPopup("bm a");
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(result.title, "javascript:'a' ", "Check title");
      EventUtils.synthesizeKey("KEY_Enter");
      return "javascript:'a'%20";
    },
    async function() {
      await promiseAutocompleteResultPopup("bm");
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(result.title, "javascript:'' ", "Check title");
      let element = UrlbarTestUtils.getSelectedElement(window);
      EventUtils.synthesizeMouseAtCenter(element, {});
      return "javascript:''%20";
    },
    async function() {
      info("Search keyword with searchstring, then click");
      await promiseAutocompleteResultPopup("bm a");
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(result.title, "javascript:'a' ", "Check title");
      let element = UrlbarTestUtils.getSelectedElement(window);
      EventUtils.synthesizeMouseAtCenter(element, {});
      return "javascript:'a'%20";
    },
  ];
  for (let testFn of testFns) {
    await do_test(testFn);
  }
});

async function do_test(loadFn) {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
    },
    async browser => {
      let originalPrincipal = gBrowser.contentPrincipal;
      let originalPrincipalURI = await getPrincipalURI(browser);

      let promise = BrowserTestUtils.waitForContentEvent(browser, "pageshow");
      let expectedUrl = await loadFn();
      info("Awaiting pageshow event");
      await promise;
      Assert.equal(gBrowser.currentURI.spec, expectedUrl);

      let newPrincipalURI = await getPrincipalURI(browser);
      Assert.equal(
        newPrincipalURI,
        originalPrincipalURI,
        "content has the same principal"
      );

      // In e10s, null principals don't round-trip so the same null principal sent
      // from the child will be a new null principal. Verify that this is the
      // case.
      if (browser.isRemoteBrowser) {
        Assert.ok(
          originalPrincipal.isNullPrincipal &&
            gBrowser.contentPrincipal.isNullPrincipal,
          "both principals should be null principals in the parent"
        );
      } else {
        Assert.ok(
          gBrowser.contentPrincipal.equals(originalPrincipal),
          "javascript bookmarklet should inherit principal"
        );
      }
    }
  );
}

function getPrincipalURI(browser) {
  return ContentTask.spawn(browser, null, function() {
    return content.document.nodePrincipal.URI.spec;
  });
}
