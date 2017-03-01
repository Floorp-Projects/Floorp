add_task(function* setup() {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
  });
});

add_task(function* test_remote_window_open_js_uri() {
  let win = yield BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;

  Assert.ok(browser.isRemoteBrowser, "should be a remote browser");

  browser.loadURI(`javascript:1;`);
  let attrs = { firstPartyDomain: "1f1841ad-0395-48ba-aec4-c98ee3f6e614.mozilla" };
  yield ContentTask.spawn(browser, attrs, function* (expectAttrs) {
    info("origin " + content.document.nodePrincipal.origin);

    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "The principal of remote javascript: should be a NullPrincipal.");
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 expectAttrs.firstPartyDomain,
                 "remote javascript: should have firstPartyDomain set");
  });

  win.close();
});

add_task(function* test_remote_window_open_js_uri2() {
  let win = yield BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;

  Assert.ok(browser.isRemoteBrowser, "should be a remote browser");

  browser.loadURI(`javascript:
    let iframe = document.createElement("iframe");
    iframe.src = "http://example.com";
    iframe.id = "iframe1";
    document.body.appendChild(iframe);
    alert("connect to http://example.com");
 `);

  yield BrowserTestUtils.browserLoaded(browser, true, function(url) {
    info("URL:" + url);
    return url == "http://example.com/";
  });

  let attrs = { firstPartyDomain: "1f1841ad-0395-48ba-aec4-c98ee3f6e614.mozilla" };
  yield ContentTask.spawn(browser, attrs, function* (expectAttrs) {
    info("origin " + content.document.nodePrincipal.origin);

    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "The principal of remote javascript: should be a NullPrincipal.");
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 expectAttrs.firstPartyDomain,
                 "remote javascript: should have firstPartyDomain set");

    let iframe = content.document.getElementById("iframe1");
    info("iframe principal: " + iframe.contentDocument.nodePrincipal.origin);
  });

  win.close();
});

