add_setup(async function () {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
  });
});

add_task(async function test_remote_window_open_js_uri() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;

  Assert.ok(browser.isRemoteBrowser, "should be a remote browser");

  BrowserTestUtils.startLoadingURIString(browser, `javascript:1;`);

  await BrowserTestUtils.browserLoaded(browser);

  await SpecialPowers.spawn(browser, [], async function () {
    Assert.ok(true, "origin " + content.document.nodePrincipal.origin);

    Assert.ok(
      content.document.nodePrincipal.isNullPrincipal,
      "The principal of remote javascript: should be a NullPrincipal."
    );

    let str = content.document.nodePrincipal.originNoSuffix;
    let expectDomain =
      str.substring("moz-nullprincipal:{".length, str.length - 1) + ".mozilla";
    Assert.equal(
      content.document.nodePrincipal.originAttributes.firstPartyDomain,
      expectDomain,
      "remote javascript: should have firstPartyDomain set to " + expectDomain
    );
  });

  win.close();
});

add_task(async function test_remote_window_open_js_uri2() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;

  Assert.ok(browser.isRemoteBrowser, "should be a remote browser");

  BrowserTestUtils.startLoadingURIString(
    browser,
    `javascript:
    let iframe = document.createElement("iframe");
    iframe.src = "http://example.com";
    iframe.id = "iframe1";
    document.body.appendChild(iframe);
    void(0);
 `
  );

  await BrowserTestUtils.browserLoaded(browser, true, function (url) {
    info("URL:" + url);
    return url == "http://example.com/";
  });

  await SpecialPowers.spawn(browser, [], async function () {
    Assert.ok(true, "origin " + content.document.nodePrincipal.origin);

    Assert.ok(
      content.document.nodePrincipal.isNullPrincipal,
      "The principal of remote javascript: should be a NullPrincipal."
    );

    let str = content.document.nodePrincipal.originNoSuffix;
    let expectDomain =
      str.substring("moz-nullprincipal:{".length, str.length - 1) + ".mozilla";
    Assert.equal(
      content.document.nodePrincipal.originAttributes.firstPartyDomain,
      expectDomain,
      "remote javascript: should have firstPartyDomain set to " + expectDomain
    );

    let iframe = content.document.getElementById("iframe1");
    await SpecialPowers.spawn(iframe, [expectDomain], function (domain) {
      Assert.equal(
        content.document.nodePrincipal.originAttributes.firstPartyDomain,
        domain,
        "iframe should have firstPartyDomain set to " + domain
      );
    });
  });

  win.close();
});
