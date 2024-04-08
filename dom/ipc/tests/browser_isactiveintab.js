/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function documentURL(origin, html) {
  let params = new URLSearchParams();
  params.append("html", html.trim());
  return `${origin}/document-builder.sjs?${params.toString()}`;
}

add_task(async function checkIsActiveInTab() {
  // This test creates a few tricky situations with navigation and iframes and
  // examines the results of various ways you might think to check if a page
  // is currently visible, and confirms that isActiveInTab works, even if the
  // others don't.

  // Make a top level page with two nested iframes.
  const IFRAME2_URL = documentURL("https://example.com", `<h1>iframe2</h1>`);
  const IFRAME1_URL = documentURL(
    "https://example.com",
    `<iframe src=${JSON.stringify(IFRAME2_URL)}></iframe><h1>iframe1</h1>`
  );
  const TEST_URL = documentURL(
    "https://example.com",
    `<iframe src=${JSON.stringify(IFRAME1_URL)}></iframe><h1>top window</h1>`
  );

  await BrowserTestUtils.withNewTab(TEST_URL, async browser => {
    let topBC1 = browser.browsingContext;
    let topWindowGlobal1 = topBC1.currentWindowGlobal;

    is(
      browser.browsingContext.children.length,
      1,
      "only one child for top window"
    );
    let iframe1 = browser.browsingContext.children[0];
    let iframeWindowGlobal1a = iframe1.currentWindowGlobal;

    is(iframe1.children.length, 1, "only one child for iframe");
    let iframe2 = iframe1.children[0];
    let iframeWindowGlobal2 = iframe2.currentWindowGlobal;

    ok(topWindowGlobal1.isActiveInTab, "top window global is active in tab");
    ok(iframeWindowGlobal1a.isActiveInTab, "topmost iframe is active in tab");
    ok(iframeWindowGlobal2.isActiveInTab, "innermost iframe is active in tab");

    // Do a same-origin navigation on the topmost iframe.
    let iframe1bURI =
      "https://example.com/browser/dom/ipc/tests/file_dummy.html";
    let loadedIframe = BrowserTestUtils.browserLoaded(
      browser,
      true,
      iframe1bURI
    );
    await SpecialPowers.spawn(
      iframe1,
      [iframe1bURI],
      async function (_iframe1bURI) {
        content.location = _iframe1bURI;
      }
    );
    await loadedIframe;

    ok(
      topWindowGlobal1.isActiveInTab,
      "top window global is still active in tab"
    );

    let iframeWindowGlobal1b = iframe1.currentWindowGlobal;
    isnot(
      iframeWindowGlobal1a,
      iframeWindowGlobal1b,
      "navigating changed the iframe's current window"
    );

    // This tests the !CanSend() case but unfortunately not the
    // `bc->GetCurrentWindowGlobal() != this` case. Apparently the latter will
    // only hold temporarily, so that is likely hard to test.
    ok(
      !iframeWindowGlobal1a.isActiveInTab,
      "topmost iframe is not active in tab"
    );

    ok(iframeWindowGlobal1b.isActiveInTab, "new iframe is active in tab");

    is(
      iframe2.currentWindowGlobal,
      iframeWindowGlobal2,
      "innermost iframe current window global has not changed"
    );

    ok(
      iframeWindowGlobal2.isCurrentGlobal,
      "innermost iframe is still the current global for its BC"
    );

    // With a same-origin navigation, this hits the !bc->AncestorsAreCurrent()
    // case. (With a cross-origin navigation, this hits the !CanSend() case.)
    ok(
      !iframeWindowGlobal2.isActiveInTab,
      "innermost iframe is not active in tab"
    );

    // Load a new page into the tab to test the behavior when a page is in
    // the BFCache.
    let newTopURI = "https://example.com/browser/dom/ipc/tests/file_dummy.html";
    let loadedTop2 = BrowserTestUtils.browserLoaded(browser, false, newTopURI);
    await BrowserTestUtils.startLoadingURIString(browser, newTopURI);
    await loadedTop2;

    isnot(browser.browsingContext, topBC1, "Navigated to a new BC");

    is(
      topBC1.currentWindowGlobal,
      topWindowGlobal1,
      "old top window is still the current window global for the old BC"
    );
    ok(topWindowGlobal1.isInBFCache, "old top window's BC is in the BFCache");
    ok(!topWindowGlobal1.isCurrentGlobal, "old top frame isn't current");
    ok(!topWindowGlobal1.isActiveInTab, "old top frame not active in tab");

    is(
      iframe1.currentWindowGlobal,
      iframeWindowGlobal1b,
      "old iframe is still the current window global for the BC"
    );
    ok(!iframeWindowGlobal1b.isCurrentGlobal, "newer top iframe isn't current");
    ok(
      iframeWindowGlobal1b.isInBFCache,
      "old top window's BC is in the BFCache"
    );
    ok(iframe1.ancestorsAreCurrent, "ancestors of iframe are current");
    ok(
      !iframeWindowGlobal1b.isActiveInTab,
      "newer top iframe is active in not active in tab after top level navigation"
    );
  });
});
