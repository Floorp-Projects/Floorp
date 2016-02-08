/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function* test() {
  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
                                    function* (browser) {
    BrowserTestUtils.loadURI(browser, "http://example.com");
    yield BrowserTestUtils.browserLoaded(browser);
    ok(!gBrowser.canGoBack, "about:newtab wasn't added to the session history");
  });
});
