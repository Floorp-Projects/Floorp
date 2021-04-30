// This test ensures that various configurable upgrade exceptions work
"use strict";

async function runTest(desc, url, expectedURI) {
  await BrowserTestUtils.withNewTab("about:blank", async function(browser) {
    let loaded = BrowserTestUtils.browserLoaded(browser, false, null, true);
    BrowserTestUtils.loadURI(browser, url);
    await loaded;

    await SpecialPowers.spawn(browser, [desc, expectedURI], async function(
      desc,
      expectedURI
    ) {
      // XXX ckerschb: generally we use the documentURI, but our test infra
      // can not handle .onion, hence we use the URI of the failed channel
      // stored on the docshell to see if the scheme was upgraded to https.
      let loadedURI = content.document.documentURI;
      if (loadedURI.startsWith("about:neterror")) {
        loadedURI = content.docShell.failedChannel.URI.spec;
      }
      is(loadedURI, expectedURI, desc);
    });
  });
}

// by default local addresses and .onion should *not* get upgraded
add_task(async function() {
  requestLongerTimeout(2);

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", true],
      ["dom.security.https_only_mode", false],
      ["dom.security.https_only_mode.upgrade_local", false],
      ["dom.security.https_only_mode.upgrade_onion", false],
    ],
  });

  await runTest(
    "Hosts ending with .onion should be be exempt from HTTPS-First upgrades by default",
    "http://grocery.shopping.for.one.onion/",
    "http://grocery.shopping.for.one.onion/"
  );

  await SpecialPowers.pushPrefEnv({
    set: [
      ["dom.security.https_first", true],
      ["dom.security.https_only_mode", false],
      ["dom.security.https_only_mode.upgrade_local", false],
      ["dom.security.https_only_mode.upgrade_onion", true],
    ],
  });

  await runTest(
    "Hosts ending with .onion should get upgraded when 'dom.security.https_only_mode.upgrade_onion' is set to true",
    "http://grocery.shopping.for.one.onion/",
    "https://grocery.shopping.for.one.onion/"
  );
});
