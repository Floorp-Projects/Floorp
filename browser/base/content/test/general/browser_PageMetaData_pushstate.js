/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(function* () {
  let rooturi = "https://example.com/browser/toolkit/modules/tests/browser/";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, rooturi + "metadata_simple.html");
  yield ContentTask.spawn(gBrowser.selectedBrowser, { rooturi }, function* (args) {
    let result = PageMetadata.getData(content.document);
    // Result should have description.
    Assert.equal(result.url, args.rooturi + "metadata_simple.html", "metadata url is correct");
    Assert.equal(result.title, "Test Title", "metadata title is correct");
    Assert.equal(result.description, "A very simple test page", "description is correct");

    content.history.pushState({}, "2", "2.html");
    result = PageMetadata.getData(content.document);
    // Result should not have description.
    Assert.equal(result.url, args.rooturi + "2.html", "metadata url is correct");
    Assert.equal(result.title, "Test Title", "metadata title is correct");
    Assert.ok(!result.description, "description is undefined");

    Assert.equal(content.document.documentURI, args.rooturi + "2.html",
      "content.document has correct url");
  });

  is(gBrowser.currentURI.spec, rooturi + "2.html", "gBrowser has correct url");

  gBrowser.removeTab(gBrowser.selectedTab);
});
