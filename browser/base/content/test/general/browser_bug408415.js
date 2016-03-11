add_task(function* test() {
  let testPath = getRootDirectory(gTestPath);

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" },
    function* (tabBrowser) {
      const URI = testPath + "file_with_favicon.html";
      const expectedIcon = testPath + "file_generic_favicon.ico";

      let got_favicon = Promise.defer();
      let listener = {
        onLinkIconAvailable(browser, iconURI) {
          if (got_favicon && iconURI && browser === tabBrowser) {
            got_favicon.resolve(iconURI);
            got_favicon = null;
          }
        }
      };
      gBrowser.addTabsProgressListener(listener);

      BrowserTestUtils.loadURI(tabBrowser, URI);

      let iconURI = yield got_favicon.promise;
      is(iconURI, expectedIcon, "Correct icon before pushState.");

      got_favicon = Promise.defer();
      got_favicon.promise.then(() => { ok(false, "shouldn't be called"); }, (e) => e);
      yield ContentTask.spawn(tabBrowser, null, function() {
        content.location.href += "#foo";
      });

      // We've navigated and shouldn't get a call to onLinkIconAvailable.
      TestUtils.executeSoon(() => {
        got_favicon.reject(gBrowser.getIcon(gBrowser.getTabForBrowser(tabBrowser)));
      });
      try {
        yield got_favicon.promise;
      } catch (e) {
        iconURI = e;
      }
      is(iconURI, expectedIcon, "Correct icon after pushState.");

      gBrowser.removeTabsProgressListener(listener);
    });
});

