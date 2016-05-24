"use strict";

const REDIRECTURL = "http://www.example.com/browser/docshell/test/browser/redirect_to_example.sjs"

add_task(function* () {
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  gURLBar.value = REDIRECTURL;
  gURLBar.select();
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  EventUtils.sendKey("return");
  yield errorPageLoaded;
  let [contentURL, originalURL] = yield ContentTask.spawn(tab.linkedBrowser, null, () => {
    return [
      content.document.documentURI,
      content.document.mozDocumentURIIfNotForErrorPages.spec,
    ];
  });
  info("Page that loaded: " + contentURL);
  ok(contentURL.startsWith("about:neterror?"), "Should be on an error page");
  originalURL = new URL(originalURL);
  is(originalURL.host, "example", "Should be an error for http://example, not http://www.example.com/");

  yield BrowserTestUtils.removeTab(tab);
});
