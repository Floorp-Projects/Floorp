if (SpecialPowers.useRemoteSubframes) {
  requestLongerTimeout(2);
}

add_setup(async function () {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);

  registerCleanupFunction(function () {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
  });
});

/**
 * For loading the initial about:blank in e10s mode, it will be loaded with
 * NullPrincipal, and we also test if the firstPartyDomain of the origin
 * attributes is got from the origin itself.
 */
add_task(async function test_remote_window_open_aboutBlank() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;

  Assert.ok(browser.isRemoteBrowser, "should be a remote browser");

  await SpecialPowers.spawn(browser, [], async function () {
    Assert.ok(true, "origin " + content.document.nodePrincipal.origin);

    Assert.ok(
      content.document.nodePrincipal.isNullPrincipal,
      "The principal of remote about:blank should be a NullPrincipal."
    );

    let str = content.document.nodePrincipal.originNoSuffix;
    let expectDomain =
      str.substring("moz-nullprincipal:{".length, str.length - 1) + ".mozilla";
    Assert.equal(
      content.document.nodePrincipal.originAttributes.firstPartyDomain,
      expectDomain,
      "remote about:blank should have firstPartyDomain set to " + expectDomain
    );
  });

  win.close();
});

/**
 * For loading the initial about:blank in non-e10s mode, it will be loaded with
 * a null principal. So we test if it has correct firstPartyDomain set.
 */
add_task(async function test_nonremote_window_open_aboutBlank() {
  if (SpecialPowers.useRemoteSubframes) {
    ok(true, "We cannot test non-e10s mode in Fission. So, we skip this.");
    return;
  }

  let win = await BrowserTestUtils.openNewBrowserWindow({ remote: false });
  let browser = win.gBrowser.selectedBrowser;

  Assert.ok(!browser.isRemoteBrowser, "shouldn't be a remote browser");

  await SpecialPowers.spawn(browser, [], async function () {
    info("origin " + content.document.nodePrincipal.origin);

    Assert.ok(
      content.document.nodePrincipal.isNullPrincipal,
      "The principal of remote about:blank should be a NullPrincipal."
    );

    let str = content.document.nodePrincipal.originNoSuffix;
    let expectDomain =
      str.substring("moz-nullprincipal:{".length, str.length - 1) + ".mozilla";
    Assert.equal(
      content.document.nodePrincipal.originAttributes.firstPartyDomain,
      expectDomain,
      "non-remote about:blank should have firstPartyDomain set to " +
        expectDomain
    );
  });

  win.close();
});

/**
 * Check if data: URI inherits firstPartyDomain from about:blank correctly.
 */
add_task(async function test_remote_window_open_data_uri() {
  // allow top level data: URI navigations, otherwise
  // <a href="data:" would fail.
  await SpecialPowers.pushPrefEnv({
    set: [["security.data_uri.block_toplevel_data_uri_navigations", false]],
  });
  let win = await BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;

  await SpecialPowers.spawn(browser, [], () => {
    content.document.body.innerHTML = `
      <a href="data:text/plain,hello" id="test">hello</a>`;

    let element = content.document.getElementById("test");
    element.click();
  });

  await BrowserTestUtils.browserLoaded(browser, false, function (url) {
    return url == "data:text/plain,hello";
  });

  await SpecialPowers.spawn(browser, [], async function () {
    Assert.ok(true, "origin: " + content.document.nodePrincipal.origin);

    Assert.ok(
      content.document.nodePrincipal.isNullPrincipal,
      "The principal of data: document should be a NullPrincipal."
    );

    Assert.ok(
      content.document.nodePrincipal.originAttributes.firstPartyDomain != "",
      "data: URI should have firstPartyDomain set."
    );
  });

  win.close();
});

/**
 * data: document contains an iframe, and we test that iframe should inherit
 * origin attributes from the data: document.
 */
add_task(async function test_remote_window_open_data_uri2() {
  let win = await BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;
  const TEST_PAGE =
    "https://example.net/browser/browser/components/originattributes/test/browser/test2.html";

  // The iframe test2.html will fetch test2.js, which will have cookies.
  const DATA_URI = `data:text/html,
                    <iframe id="iframe1" src="${TEST_PAGE}"></iframe>`;
  BrowserTestUtils.startLoadingURIString(browser, DATA_URI);
  await BrowserTestUtils.browserLoaded(browser, true, TEST_PAGE);

  await SpecialPowers.spawn(browser, [], async function () {
    Assert.ok(true, "origin " + content.document.nodePrincipal.origin);

    Assert.ok(
      content.document.nodePrincipal.isNullPrincipal,
      "The principal of data: document should be a NullPrincipal."
    );

    Assert.ok(
      content.document.nodePrincipal.originAttributes.firstPartyDomain != "",
      "data: URI should have firstPartyDomain set."
    );

    let iframe = content.document.getElementById("iframe1");
    await SpecialPowers.spawn(
      iframe,
      [content.document.nodePrincipal.originAttributes.firstPartyDomain],
      function (firstPartyDomain) {
        Assert.ok(
          true,
          "iframe principal: " + content.document.nodePrincipal.origin
        );

        Assert.equal(
          content.document.nodePrincipal.originAttributes.firstPartyDomain,
          firstPartyDomain,
          "iframe should inherit firstPartyDomain from parent document."
        );
        Assert.equal(
          content.document.cookie,
          "test2=foo",
          "iframe should have cookies"
        );
      }
    );
  });

  win.close();
});

/**
 * about: pages should have firstPartyDomain set when we enable first party isolation.
 */
add_task(async function test_aboutURL() {
  let aboutURLs = [];

  // List of about: URLs that will initiate network requests.
  let networkURLs = ["credits", "logins"];

  for (let cid in Cc) {
    let result = cid.match(
      /@mozilla.org\/network\/protocol\/about;1\?what\=(.*)$/
    );
    if (!result) {
      continue;
    }

    let aboutType = result[1];
    let contract = "@mozilla.org/network/protocol/about;1?what=" + aboutType;
    try {
      let am = Cc[contract].getService(Ci.nsIAboutModule);
      let uri = Services.io.newURI("about:" + aboutType);
      let flags = am.getURIFlags(uri);

      // We load pages with URI_SAFE_FOR_UNTRUSTED_CONTENT set, this means they
      // are not loaded with System Principal but with content principal.
      // Also we skip pages with HIDE_FROM_ABOUTABOUT, some of them may have
      // errors while loading.
      if (
        flags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT &&
        !(flags & Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT) &&
        !networkURLs.includes(aboutType) &&
        // handle about:newtab in browser_firstPartyIsolation_about_newtab.js
        aboutType !== "newtab" &&
        // protections kicks of async messaging as soon as it loads,
        // this test closes the tab too soon causing errors
        aboutType !== "protections"
      ) {
        aboutURLs.push(aboutType);
      }
    } catch (e) {
      // getService might have thrown if the component doesn't actually
      // implement nsIAboutModule
    }
  }

  for (let url of aboutURLs) {
    let tab = BrowserTestUtils.addTab(gBrowser, "about:" + url);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

    let attrs = {
      firstPartyDomain: "about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla",
    };
    await SpecialPowers.spawn(
      tab.linkedBrowser,
      [{ attrs, url }],
      async function (args) {
        Assert.ok(
          true,
          "loading page about:" +
            args.url +
            ", origin is " +
            content.document.nodePrincipal.origin
        );
        Assert.ok(true, "principal " + content.document.nodePrincipal);
        Assert.equal(
          content.document.nodePrincipal.originAttributes.firstPartyDomain,
          args.attrs.firstPartyDomain,
          "The about page should have firstPartyDomain set"
        );
        Assert.ok(
          content.document.nodePrincipal.isContentPrincipal,
          "The principal should be a content principal."
        );
      }
    );

    gBrowser.removeTab(tab);
  }
});
