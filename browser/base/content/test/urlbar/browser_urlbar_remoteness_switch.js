"use strict";

/**
 * Verify that when loading and going back/forward through history between URLs
 * loaded in the content process, and URLs loaded in the parent process, we
 * don't set the URL for the tab to about:blank inbetween the loads.
 */
add_task(function*() {
  let url = "http://www.example.com/foo.html";
  yield BrowserTestUtils.withNewTab({gBrowser, url}, function*(browser) {
    let wpl = {
      onLocationChange(unused, unused2, location) {
        if (location.schemeIs("about")) {
          is(location.spec, "about:config", "Only about: location change should be for about:preferences");
        } else {
          is(location.spec, url, "Only non-about: location change should be for the http URL we're dealing with.");
        }
      },
    };
    gBrowser.addProgressListener(wpl);

    let didLoad = BrowserTestUtils.browserLoaded(browser, null, function(loadedURL) {
      return loadedURL == "about:config";
    });
    yield BrowserTestUtils.loadURI(browser, "about:config");
    yield didLoad;

    gBrowser.goBack();
    yield BrowserTestUtils.browserLoaded(browser, null, function(loadedURL) {
      return url == loadedURL;
    });
    gBrowser.goForward();
    yield BrowserTestUtils.browserLoaded(browser, null, function(loadedURL) {
      return loadedURL == "about:config";
    });
    gBrowser.removeProgressListener(wpl);
  });
});

