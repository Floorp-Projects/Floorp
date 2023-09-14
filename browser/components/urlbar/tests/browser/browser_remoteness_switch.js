"use strict";

/**
 * Verify that when loading and going back/forward through history between URLs
 * loaded in the content process, and URLs loaded in the parent process, we
 * don't set the URL for the tab to about:blank inbetween the loads.
 */
add_task(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.navigation.requireUserInteraction", false]],
  });
  let url = "http://www.example.com/foo.html";
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      let wpl = {
        onLocationChange(unused, unused2, location) {
          if (location.schemeIs("about")) {
            is(
              location.spec,
              "about:config",
              "Only about: location change should be for about:preferences"
            );
          } else {
            is(
              location.spec,
              url,
              "Only non-about: location change should be for the http URL we're dealing with."
            );
          }
        },
      };
      gBrowser.addProgressListener(wpl);

      let didLoad = BrowserTestUtils.browserLoaded(
        browser,
        null,
        function (loadedURL) {
          return loadedURL == "about:config";
        }
      );
      BrowserTestUtils.startLoadingURIString(browser, "about:config");
      await didLoad;

      gBrowser.goBack();
      await BrowserTestUtils.browserLoaded(browser, null, function (loadedURL) {
        return url == loadedURL;
      });
      gBrowser.goForward();
      await BrowserTestUtils.browserLoaded(browser, null, function (loadedURL) {
        return loadedURL == "about:config";
      });
      gBrowser.removeProgressListener(wpl);
    }
  );
});
