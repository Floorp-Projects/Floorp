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
  yield ContentTask.spawn(browser, {}, function* () {
    info("origin " + content.document.nodePrincipal.origin);

    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "The principal of remote javascript: should be a NullPrincipal.");

    let str = content.document.nodePrincipal.originNoSuffix;
    let expectDomain = str.substring("moz-nullprincipal:{".length, str.length - 1) + ".mozilla";
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 expectDomain,
                 "remote javascript: should have firstPartyDomain set to " + expectDomain);
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

  yield ContentTask.spawn(browser, {}, function* () {
    info("origin " + content.document.nodePrincipal.origin);

    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "The principal of remote javascript: should be a NullPrincipal.");

    let str = content.document.nodePrincipal.originNoSuffix;
    let expectDomain = str.substring("moz-nullprincipal:{".length, str.length - 1) + ".mozilla";
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 expectDomain,
                 "remote javascript: should have firstPartyDomain set to " + expectDomain);

    let iframe = content.document.getElementById("iframe1");
    Assert.equal(iframe.contentDocument.nodePrincipal.originAttributes.firstPartyDomain,
                 expectDomain,
                 "iframe should have firstPartyDomain set to " + expectDomain);
  });

  win.close();
});

