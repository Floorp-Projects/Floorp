/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let oldProxyType = Services.prefs.getIntPref("network.proxy.type");
function resetPrefs() {
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
  Services.prefs.setIntPref("network.proxy.type", oldProxyType);
  Services.prefs.clearUserPref("network.trr.display_fallback_warning");
}

// This test makes sure that the Add exception button only shows up
// when the skipReason indicates that the domain could not be resolved.
// If instead there is a problem with the TRR connection, then we don't
// show the exception button.
add_task(async function exceptionButtonTRROnly() {
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
  // We need to disable proxy, otherwise TRR isn't used for name resolution.
  Services.prefs.setIntPref("network.proxy.type", 0);
  registerCleanupFunction(resetPrefs);

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        "https://does-not-exist.test"
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  await SpecialPowers.spawn(browser, [], function() {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const titleEl = doc.querySelector(".title-text");
    const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
    is(
      actualDataL10nID,
      "dns-not-found-trr-only-title2",
      "Correct error page title is set"
    );

    let trrExceptionButton = doc.getElementById("trrExceptionButton");
    Assert.equal(
      trrExceptionButton.hidden,
      true,
      "Exception button should be hidden for TRR service failures"
    );
  });

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  resetPrefs();
});

add_task(async function exceptionButtonTRROnly() {
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);
  Services.prefs.setIntPref("network.proxy.type", 0);
  // We need to disable proxy, otherwise TRR isn't used for name resolution.
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  Services.prefs.setBoolPref("network.trr.display_fallback_warning", true);
  registerCleanupFunction(resetPrefs);

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(
        gBrowser,
        "https://does-not-exist.test"
      );
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  await SpecialPowers.spawn(browser, [], function() {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );

    const titleEl = doc.querySelector(".title-text");
    const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
    is(
      actualDataL10nID,
      "dns-not-found-native-fallback-title",
      "Correct error page title is set"
    );

    let description = doc.getElementById("nativeFallbackDescription");
    Assert.equal(
      description.getAttribute("data-l10n-id"),
      "neterror-dns-not-found-native-fallback-not-confirmed2",
      "Unexpected content"
    );
  });

  // await new Promise(resolve => {});
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  resetPrefs();
});
