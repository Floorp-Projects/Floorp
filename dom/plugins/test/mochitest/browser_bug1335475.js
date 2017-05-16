var rootDir = getRootDirectory(gTestPath);
const gTestRoot = rootDir.replace("chrome://mochitests/content/", "http://127.0.0.1:8888/");

add_task(function*() {
  is(navigator.plugins.length, 0,
     "plugins should not be available to chrome-privilege pages");
  ok(!("application/x-test" in navigator.mimeTypes),
     "plugins should not be available to chrome-privilege pages");

  yield BrowserTestUtils.withNewTab({ gBrowser, url: "about:blank" }, function*(browser) {
    // about:blank triggered from a toplevel load should not inherit permissions
    yield ContentTask.spawn(browser, null, function*() {
      is(content.window.navigator.plugins.length, 0,
         "plugins should not be available to null-principal about:blank");
      ok(!("application/x-test" in content.window.navigator.mimeTypes),
         "plugins should not be available to null-principal about:blank");
    });

    let promise = BrowserTestUtils.browserLoaded(browser);
    browser.loadURI(gTestRoot + "plugin_test.html");
    yield promise;

    yield ContentTask.spawn(browser, null, function*() {
      ok(content.window.navigator.plugins.length > 0,
         "plugins should be available to HTTP-loaded pages");
      ok("application/x-test" in content.window.navigator.mimeTypes,
         "plugins should be available to HTTP-loaded pages");

      let subwindow = content.document.getElementById("subf").contentWindow;

      ok("application/x-test" in subwindow.navigator.mimeTypes,
         "plugins should be available to an about:blank subframe loaded from a site");
    });

    // navigate from the HTTP page to an about:blank page which ought to
    // inherit permissions
    promise = BrowserTestUtils.browserLoaded(browser);
    yield ContentTask.spawn(browser, null, function*() {
      content.document.getElementById("aboutlink").click();
    });
    yield promise;

    yield ContentTask.spawn(browser, null, function*() {
      is(content.window.location.href, "about:blank", "sanity-check about:blank load");
      ok("application/x-test" in content.window.navigator.mimeTypes,
         "plugins should be available when a site triggers an about:blank load");
    });

    // navigate to the file: URI, which shouldn't allow plugins. This might
    // be wrapped in jar:, but that shouldn't matter for this test
    promise = BrowserTestUtils.browserLoaded(browser);
    let converteduri = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry).convertChromeURL(Services.io.newURI(rootDir + "plugin_test.html"));
    browser.loadURI(converteduri.spec);
    yield promise;

    yield ContentTask.spawn(browser, null, function*() {
      ok(!("application/x-test" in content.window.navigator.mimeTypes),
         "plugins should not be available to file: URI content");
    });
  });

  // As much as it would be nice, this doesn't actually check ftp:// because
  // we don't have a synthetic server.
});
