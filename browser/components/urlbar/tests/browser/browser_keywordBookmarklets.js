/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  let bm = await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title: "bookmarklet",
    url: "javascript:'%sx'%20",
  });
  await PlacesUtils.keywords.insert({ keyword: "bm", url: bm.url });
  registerCleanupFunction(async function () {
    await PlacesUtils.bookmarks.remove(bm);
  });

  let testFns = [
    function () {
      info("Type keyword and immediately press enter");
      gURLBar.value = "bm";
      gURLBar.focus();
      EventUtils.synthesizeKey("KEY_Enter");
      return "x";
    },
    function () {
      info("Type keyword with searchstring and immediately press enter");
      gURLBar.value = "bm a";
      gURLBar.focus();
      EventUtils.synthesizeKey("KEY_Enter");
      return "ax";
    },
    async function () {
      info("Search keyword, then press enter");
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "bm",
      });
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(result.title, "javascript:'x' ", "Check title");
      EventUtils.synthesizeKey("KEY_Enter");
      return "x";
    },
    async function () {
      info("Search keyword with searchstring, then press enter");
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "bm a",
      });
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(result.title, "javascript:'ax' ", "Check title");
      EventUtils.synthesizeKey("KEY_Enter");
      return "ax";
    },
    async function () {
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "bm",
      });
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(result.title, "javascript:'x' ", "Check title");
      let element = UrlbarTestUtils.getSelectedRow(window);
      EventUtils.synthesizeMouseAtCenter(element, {});
      return "x";
    },
    async function () {
      info("Search keyword with searchstring, then click");
      await UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        value: "bm a",
      });
      let result = await UrlbarTestUtils.getDetailsOfResultAt(window, 0);
      Assert.equal(result.title, "javascript:'ax' ", "Check title");
      let element = UrlbarTestUtils.getSelectedRow(window);
      EventUtils.synthesizeMouseAtCenter(element, {});
      return "ax";
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
      const expectedTextContent = await loadFn();
      info("Awaiting pageshow event");
      await promise;
      // URI should not change when we run a javascript: URL.
      Assert.equal(gBrowser.currentURI.spec, "about:blank");
      const textContent = await ContentTask.spawn(browser, [], function () {
        return content.document.documentElement.textContent;
      });
      Assert.equal(textContent, expectedTextContent);

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
  return SpecialPowers.spawn(browser, [], function () {
    return content.document.nodePrincipal.spec;
  });
}
