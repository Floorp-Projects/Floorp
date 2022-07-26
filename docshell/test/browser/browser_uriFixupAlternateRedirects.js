"use strict";

const { UrlbarTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/UrlbarTestUtils.sys.mjs"
);

const REDIRECTURL =
  "http://www.example.com/browser/docshell/test/browser/redirect_to_example.sjs";

add_task(async function() {
  // Test both directly setting a value and pressing enter, or setting the
  // value through input events, like the user would do.
  const setValueFns = [
    value => {
      gURLBar.value = value;
    },
    value => {
      return UrlbarTestUtils.promiseAutocompleteResultPopup({
        window,
        waitForFocus,
        value,
      });
    },
  ];
  for (let setValueFn of setValueFns) {
    let tab = await BrowserTestUtils.openNewForegroundTab(
      gBrowser,
      "about:blank"
    );
    // Enter search terms and start a search.
    gURLBar.focus();
    await setValueFn(REDIRECTURL);
    let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
    EventUtils.synthesizeKey("KEY_Enter");
    await errorPageLoaded;
    let [contentURL, originalURL] = await SpecialPowers.spawn(
      tab.linkedBrowser,
      [],
      () => {
        return [
          content.document.documentURI,
          content.document.mozDocumentURIIfNotForErrorPages.spec,
        ];
      }
    );
    info("Page that loaded: " + contentURL);
    const errorURI = "about:neterror?";
    ok(contentURL.startsWith(errorURI), "Should be on an error page");

    const contentPrincipal = tab.linkedBrowser.contentPrincipal;
    ok(
      contentPrincipal.spec.startsWith(errorURI),
      "Principal should be for the error page"
    );

    originalURL = new URL(originalURL);
    is(
      originalURL.host,
      "example",
      "Should be an error for http://example, not http://www.example.com/"
    );

    BrowserTestUtils.removeTab(tab);
  }
});
