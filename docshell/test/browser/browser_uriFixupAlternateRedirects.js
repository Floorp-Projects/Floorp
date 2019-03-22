"use strict";

const REDIRECTURL = "http://www.example.com/browser/docshell/test/browser/redirect_to_example.sjs";

add_task(async function() {
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "about:blank");
  gURLBar.value = REDIRECTURL;
  gURLBar.select();
  let errorPageLoaded = BrowserTestUtils.waitForErrorPage(tab.linkedBrowser);
  EventUtils.sendKey("return");
  await errorPageLoaded;
  let [contentURL, originalURL] = await ContentTask.spawn(tab.linkedBrowser, null, () => {
    return [
      content.document.documentURI,
      content.document.mozDocumentURIIfNotForErrorPages.spec,
    ];
  });
  info("Page that loaded: " + contentURL);
  const errorURI = "about:neterror?";
  ok(contentURL.startsWith(errorURI), "Should be on an error page");

  const contentPrincipal = tab.linkedBrowser.contentPrincipal;
  ok(contentPrincipal.URI.spec.startsWith(errorURI), "Principal should be for the error page");

  originalURL = new URL(originalURL);
  is(originalURL.host, "example", "Should be an error for http://example, not http://www.example.com/");

  BrowserTestUtils.removeTab(tab);
});
