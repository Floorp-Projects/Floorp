add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtab.preload", false],
      ["privacy.firstparty.isolate", true],
    ],
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
  // In Fission, we cannot open a non-remote window.
  let win = await BrowserTestUtils.openNewBrowserWindow({
    remote: SpecialPowers.useRemoteSubframes,
  });
  let gbrowser = win.gBrowser;
  let tab = BrowserTestUtils.addTab(gbrowser, "about:newtab");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  let attrs = {
    firstPartyDomain: "about.ef2a7dd5-93bc-417f-a698-142c3116864f.mozilla",
  };
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [{ attrs }],
    async function (args) {
      Assert.ok(true, "principal " + content.document.nodePrincipal.origin);
      Assert.equal(
        content.document.nodePrincipal.originAttributes.firstPartyDomain,
        args.attrs.firstPartyDomain,
        "about:newtab should have firstPartyDomain set"
      );
      Assert.ok(
        content.document.nodePrincipal.isContentPrincipal,
        "The principal should be a content principal."
      );
    }
  );

  gbrowser.removeTab(tab);
  win.close();
});
