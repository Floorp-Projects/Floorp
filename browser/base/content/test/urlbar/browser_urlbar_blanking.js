"use strict";

add_task(function*() {
  for (let page of gInitialPages) {
    if (page == "about:newtab") {
      // New tab preloading makes this a pain to test, so skip
      continue;
    }
    let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, page);
    ok(!gURLBar.value, "The URL bar should be empty if we load a plain " + page + " page.");
    yield BrowserTestUtils.removeTab(tab);
  }
});

add_task(function*() {
  const URI = "http://www.example.com/browser/browser/base/content/test/urlbar/file_blank_but_not_blank.html";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, URI);
  is(gURLBar.value, URI, "The URL bar should match the URI");
  let browserLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  ContentTask.spawn(tab.linkedBrowser, null, function() {
    content.document.querySelector('a').click();
  });
  yield browserLoaded;
  ok(gURLBar.value.startsWith("javascript"), "The URL bar should have the JS URI");
  // When reloading, the javascript: uri we're using will throw an exception.
  // That's deliberate, so we need to tell mochitest to ignore it:
  SimpleTest.expectUncaughtException(true);
  yield ContentTask.spawn(tab.linkedBrowser, null, function*() {
    // This is sync, so by the time we return we should have changed the URL bar.
    content.location.reload();
  });
  ok(!!gURLBar.value, "URL bar should not be blank.");
  yield BrowserTestUtils.removeTab(tab);
  SimpleTest.expectUncaughtException(false);
});
