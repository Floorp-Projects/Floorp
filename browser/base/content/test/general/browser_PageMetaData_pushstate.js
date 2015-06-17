/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

add_task(function* () {
  let rooturi = "https://example.com/browser/toolkit/modules/tests/browser/";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, rooturi + "metadata_simple.html");
  let result = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    return PageMetadata.getData(content.document);
  });
  // result should have description
  is(result.url, rooturi + "metadata_simple.html", "metadata url is correct");
  is(result.title, "Test Title", "metadata title is correct");
  is(result.description, "A very simple test page", "description is correct");

  result = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    content.history.pushState({}, "2", "2.html");
    return PageMetadata.getData(content.document);
  });
  // result should not have description
  is(result.url, rooturi + "2.html", "metadata url is correct");
  is(result.title, "Test Title", "metadata title is correct");
  ok(!result.description, "description is undefined");

  let documentURI = yield ContentTask.spawn(gBrowser.selectedBrowser, {}, function* () {
    return content.document.documentURI;
  });
  is(gBrowser.currentURI.spec, rooturi + "2.html", "gBrowser has correct url");
  is(documentURI, rooturi + "2.html", "content.document has correct url");

  gBrowser.removeTab(gBrowser.selectedTab);
});
