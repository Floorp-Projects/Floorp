/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// See bug 1831731. This test should not actually try to create a connection to
// the real DoH endpoint. But that may happen when clearing the proxy type, and
// sometimes even in the next test.
// To prevent that we override the IP to a local address.
Cc["@mozilla.org/network/native-dns-override;1"]
  .getService(Ci.nsINativeDNSResolverOverride)
  .addIPOverride("mozilla.cloudflare-dns.com", "127.0.0.1");

let oldProxyType = Services.prefs.getIntPref("network.proxy.type");
function resetPrefs() {
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
  Services.prefs.setIntPref("network.proxy.type", oldProxyType);
}

async function loadErrorPage() {
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
  return browser;
}

// This test makes sure that the Add exception button only shows up
// when the skipReason indicates that the domain could not be resolved.
// If instead there is a problem with the TRR connection, then we don't
// show the exception button.
add_task(async function exceptionButtonTRROnly() {
  let browser = await loadErrorPage();

  await SpecialPowers.spawn(browser, [], function () {
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

add_task(async function TRROnlyExceptionButtonTelemetry() {
  // Clear everything.
  Services.telemetry.clearEvents();
  await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return !events || !events.length;
  });
  Services.telemetry.setEventRecordingEnabled("security.doh.neterror", true);

  let browser = await loadErrorPage();

  await SpecialPowers.spawn(browser, [], function () {
    const doc = content.document;
    ok(
      doc.documentURI.startsWith("about:neterror"),
      "Should be showing error page"
    );
  });

  let loadEvent = await TestUtils.waitForCondition(() => {
    let events = Services.telemetry.snapshotEvents(
      Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
      true
    ).content;
    return events?.find(e => e[1] == "security.doh.neterror" && e[2] == "load");
  }, "recorded telemetry for the load");

  loadEvent.shift();
  Assert.deepEqual(loadEvent, [
    "security.doh.neterror",
    "load",
    "dohwarning",
    "TRROnlyFailure",
    {
      mode: "3",
      provider_key: "mozilla.cloudflare-dns.com",
      skip_reason: "TRR_UNKNOWN_CHANNEL_FAILURE",
    },
  ]);

  await SpecialPowers.spawn(browser, [], function () {
    const doc = content.document;
    let buttons = ["neterrorTryAgainButton", "trrSettingsButton"];
    for (let buttonId of buttons) {
      let button = doc.getElementById(buttonId);
      button.click();
    }
  });

  // Since we click TryAgain, make sure the error page is loaded again.
  await BrowserTestUtils.waitForErrorPage(browser);

  is(
    gBrowser.tabs.length,
    3,
    "Should open about:preferences#privacy-doh in another tab"
  );

  let clickEvents = await TestUtils.waitForCondition(
    () => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        true
      ).content;
      return events?.filter(
        e => e[1] == "security.doh.neterror" && e[2] == "click"
      );
    },
    "recorded telemetry for clicking buttons",
    500,
    100
  );

  let firstEvent = clickEvents[0];
  firstEvent.shift(); // remove timestamp
  Assert.deepEqual(firstEvent, [
    "security.doh.neterror",
    "click",
    "try_again_button",
    "TRROnlyFailure",
    {
      mode: "3",
      provider_key: "mozilla.cloudflare-dns.com",
      skip_reason: "TRR_UNKNOWN_CHANNEL_FAILURE",
    },
  ]);

  let secondEvent = clickEvents[1];
  secondEvent.shift(); // remove timestamp
  Assert.deepEqual(secondEvent, [
    "security.doh.neterror",
    "click",
    "settings_button",
    "TRROnlyFailure",
    {
      mode: "3",
      provider_key: "mozilla.cloudflare-dns.com",
      skip_reason: "TRR_UNKNOWN_CHANNEL_FAILURE",
    },
  ]);

  BrowserTestUtils.removeTab(gBrowser.tabs[2]);
  BrowserTestUtils.removeTab(gBrowser.tabs[1]);
  resetPrefs();
});
