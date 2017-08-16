add_task(async function setup() {
  Services.prefs.setBoolPref("privacy.firstparty.isolate", true);
  // activity-stream is only enabled in Nightly, and if activity-stream is not
  // enabled, about:newtab is loaded without the flag
  // nsIAboutModule::URI_SAFE_FOR_UNTRUSTED_CONTENT, so it will be loaded with
  // System Principal.
  Services.prefs.setBoolPref("browser.newtabpage.activity-stream.enabled", true);

  registerCleanupFunction(function() {
    Services.prefs.clearUserPref("privacy.firstparty.isolate");
    Services.prefs.clearUserPref("browser.newtabpage.activity-stream.enabled");
  });
});

/**
 * Test about:newtab will have firstPartytDomain set when we enable the pref.
 *
 * We split about:newtab from browser_firstPartyIsolation_aboutPages.js because
 * about:newtab needs special care.
 *
 * In the original test browser_firstPartyIsolation_aboutPages.js, when calling
 * tabbrowser.addTab, if it found out the uri is about:newtab, it will use
 * the preloaded browser, however the preloaded browser is loaded before when we
 * turn on the firstPartyIsolation pref, which won't have the pref set.
 *
 * To prevent to use the preloaded browser, a simple trick is open a window
 * first.
 **/
add_task(async function test_aboutNewTab() {
  let win = await BrowserTestUtils.openNewBrowserWindow({remote: false});
  let gbrowser = win.gBrowser;
  let tab = BrowserTestUtils.addTab(gbrowser, "about:newtab");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let attrs = { firstPartyDomain: "about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla" };
  await ContentTask.spawn(tab.linkedBrowser, { attrs }, async function(args) {
    info("principal " + content.document.nodePrincipal.origin);
    Assert.equal(content.document.nodePrincipal.originAttributes.firstPartyDomain,
                 args.attrs.firstPartyDomain, "about:newtab should have firstPartyDomain set");
    Assert.ok(content.document.nodePrincipal.isCodebasePrincipal,
              "The principal should be a codebase principal.");
  });

  gbrowser.removeTab(tab);
  win.close();
});
