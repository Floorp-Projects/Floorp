add_task(async function setup() {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);
  Services.prefs.setBoolPref("signon.management.page.enabled", true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
    Services.prefs.clearUserPref("signon.management.page.enabled");
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

  await ContentTask.spawn(browser, {}, async function() {
    info("origin " + content.document.nodePrincipal.origin);

    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "The principal of remote about:blank should be a NullPrincipal.");

    let str = content.document.nodePrincipal.originNoSuffix;
    let expectDomain = str.substring("moz-nullprincipal:{".length, str.length - 1) + ".mozilla";
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 expectDomain,
                 "remote about:blank should have firstPartyDomain set to " + expectDomain);
  });

  win.close();
});

/**
 * For loading the initial about:blank in non-e10s mode, it will be loaded with
 * a null principal. So we test if it has correct firstPartyDomain set.
 */
add_task(async function test_nonremote_window_open_aboutBlank() {
  let win = await BrowserTestUtils.openNewBrowserWindow({remote: false});
  let browser = win.gBrowser.selectedBrowser;

  Assert.ok(!browser.isRemoteBrowser, "shouldn't be a remote browser");

  await ContentTask.spawn(browser, {}, async function() {
    info("origin " + content.document.nodePrincipal.origin);

    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "The principal of remote about:blank should be a NullPrincipal.");

    let str = content.document.nodePrincipal.originNoSuffix;
    let expectDomain = str.substring("moz-nullprincipal:{".length, str.length - 1) + ".mozilla";
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 expectDomain,
                 "non-remote about:blank should have firstPartyDomain set to " + expectDomain);
  });

  win.close();
});

function frame_script() {
  content.document.body.innerHTML = `
    <a href="data:text/plain,hello" id="test">hello</a>
  `;

  let element = content.document.getElementById("test");
  element.click();
}

/**
 * Check if data: URI inherits firstPartyDomain from about:blank correctly.
 */
add_task(async function test_remote_window_open_data_uri() {
  // allow top level data: URI navigations, otherwise
  // <a href="data:" would fail.
  await SpecialPowers.pushPrefEnv({
    "set": [["security.data_uri.block_toplevel_data_uri_navigations", false]],
  });
  let win = await BrowserTestUtils.openNewBrowserWindow({ remote: true });
  let browser = win.gBrowser.selectedBrowser;
  let mm = browser.messageManager;
  mm.loadFrameScript("data:,(" + frame_script.toString() + ")();", true);

  await BrowserTestUtils.browserLoaded(browser, false, function(url) {
    return url == "data:text/plain,hello";
  });

  await ContentTask.spawn(browser, {}, async function() {
    info("origin: " + content.document.nodePrincipal.origin);

    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "The principal of data: document should be a NullPrincipal.");

    Assert.ok(content.document.nodePrincipal.originAttributes.firstPartyDomain != "",
              "data: URI should have firstPartyDomain set.");
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

  // The iframe test2.html will fetch test2.js, which will have cookies.
  const DATA_URI = `data:text/html,
                    <iframe id="iframe1" src="http://mochi.test:8888/browser/browser/components/originattributes/test/browser/test2.html"></iframe>`;
  BrowserTestUtils.loadURI(browser, DATA_URI);
  await BrowserTestUtils.browserLoaded(browser, true);

  await ContentTask.spawn(browser, {}, async function() {
    info("origin " + content.document.nodePrincipal.origin);

    let iframe = content.document.getElementById("iframe1");
    info("iframe principal: " + iframe.contentDocument.nodePrincipal.origin);

    Assert.ok(content.document.nodePrincipal.isNullPrincipal,
              "The principal of data: document should be a NullPrincipal.");

    Assert.ok(content.document.nodePrincipal.originAttributes.firstPartyDomain != "",
              "data: URI should have firstPartyDomain set.");

    Assert.equal(iframe.contentDocument.nodePrincipal.originAttributes.firstPartyDomain,
                 content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 "iframe should inherit firstPartyDomain from parent document.");
    Assert.equal(iframe.contentDocument.cookie, "test2=foo", "iframe should have cookies");
  });

  win.close();
});

/**
 * about: pages should have firstPartyDomain set when we enable first party isolation.
 */
add_task(async function test_aboutURL() {
  let aboutURLs = [];

  // List of about: URLs that will initiate network requests.
  let networkURLs = [
    "credits",
  ];

  for (let cid in Cc) {
    let result = cid.match(/@mozilla.org\/network\/protocol\/about;1\?what\=(.*)$/);
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
      // are not loaded with System Principal but with codebase principal.
      // Also we skip pages with HIDE_FROM_ABOUTABOUT, some of them may have
      // errors while loading.
      if ((flags & Ci.nsIAboutModule.URI_SAFE_FOR_UNTRUSTED_CONTENT) &&
          !(flags & Ci.nsIAboutModule.HIDE_FROM_ABOUTABOUT) &&
          !networkURLs.includes(aboutType) &&
          // handle about:newtab in browser_firstPartyIsolation_about_newtab.js
          aboutType !== "newtab") {
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

    let attrs = { firstPartyDomain: "about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla" };
    await ContentTask.spawn(tab.linkedBrowser, { attrs, url }, async function(args) {
      info("loading page about:" + args.url + ", origin is " + content.document.nodePrincipal.origin);
      info("principal " + content.document.nodePrincipal);
      Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                   args.attrs.firstPartyDomain, "The about page should have firstPartyDomain set");
      Assert.ok(content.document.nodePrincipal.isCodebasePrincipal,
                "The principal should be a codebase principal.");
    });

    gBrowser.removeTab(tab);
  }
});
