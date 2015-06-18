add_task(function* test_multiple_pushState() {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: "http://example.org/browser/docshell/test/browser/file_multiple_pushState.html",
  }, function* (browser) {
    const kExpected = "http://example.org/bar/ABC/DEF?key=baz";

    let contentLocation = yield ContentTask.spawn(browser, null, function* () {
      return content.document.location.href;
    });

    is(contentLocation, kExpected);
    is(browser.documentURI.spec, kExpected);
  });
});
