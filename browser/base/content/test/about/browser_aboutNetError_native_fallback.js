/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let oldProxyType = Services.prefs.getIntPref("network.proxy.type");

function reset() {
  Services.prefs.clearUserPref("network.trr.display_fallback_warning");
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
  Services.prefs.clearUserPref("doh-rollout.disable-heuristics");
  Services.prefs.setIntPref("network.proxy.type", oldProxyType);
  Services.prefs.clearUserPref("network.trr.uri");

  Services.dns.setHeuristicDetectionResult(Ci.nsITRRSkipReason.TRR_OK);
}

// This helper verifies that the given url loads correctly
async function verifyLoad(url, testName) {
  let browser;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
      browser = gBrowser.selectedBrowser;
    },
    true
  );

  await SpecialPowers.spawn(browser, [{ url, testName }], function (args) {
    const doc = content.document;
    Assert.equal(
      doc.documentURI,
      args.url,
      "Should have loaded page: " + args.testName
    );
  });
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

// This helper verifies that loading the given url will lead to an error -- the fallback warning if the parameter is true
async function verifyError(url, fallbackWarning, testName) {
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

  let browser;
  let pageLoaded;
  await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    () => {
      gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser, url);
      browser = gBrowser.selectedBrowser;
      pageLoaded = BrowserTestUtils.waitForErrorPage(browser);
    },
    false
  );

  info("Loading and waiting for the net error");
  await pageLoaded;

  await SpecialPowers.spawn(
    browser,
    [{ url, fallbackWarning, testName }],
    function (args) {
      const doc = content.document;

      ok(doc.documentURI.startsWith("about:neterror"));
      "Should be showing error page: " + args.testName;

      const titleEl = doc.querySelector(".title-text");
      const actualDataL10nID = titleEl.getAttribute("data-l10n-id");
      if (args.fallbackWarning) {
        is(
          actualDataL10nID,
          "dns-not-found-native-fallback-title2",
          "Correct fallback warning error page title is set: " + args.testName
        );
      } else {
        Assert.notEqual(
          actualDataL10nID,
          "dns-not-found-native-fallback-title2",
          "Should not show fallback warning: " + args.testName
        );
      }
    }
  );

  if (fallbackWarning) {
    let loadEvent = await TestUtils.waitForCondition(() => {
      let events = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        true
      ).content;
      return events?.find(
        e => e[1] == "security.doh.neterror" && e[2] == "load"
      );
    }, "recorded telemetry for the load");
    loadEvent.shift();
    Assert.deepEqual(loadEvent, [
      "security.doh.neterror",
      "load",
      "dohwarning",
      "NativeFallbackWarning",
      {
        mode: "0",
        provider_key: "0.0.0.0",
        skip_reason: "TRR_HEURISTIC_TRIPPED_CANARY",
      },
    ]);
  }
  BrowserTestUtils.removeTab(gBrowser.selectedTab);
}

// This test verifies that the native fallback warning appears in the desired scenarios, and only in those scenarios
add_task(async function nativeFallbackWarnings() {
  Services.prefs.setBoolPref("network.dns.native-is-localhost", true);

  // Disable heuristics since they will attempt to connect to external servers
  Services.prefs.setBoolPref("doh-rollout.disable-heuristics", true);

  // Set a local TRR to prevent external connections
  Services.prefs.setCharPref("network.trr.uri", "https://0.0.0.0/dns-query");

  registerCleanupFunction(reset);

  // Test without DoH
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );

  Services.dns.clearCache(true);
  await verifyLoad("https://www.example.com/", "valid url, no error");

  // Should not trigger the native fallback warning
  await verifyError("https://does-not-exist.test", false, "non existent url");

  // We need to disable proxy, otherwise TRR isn't used for name resolution.
  Services.prefs.setIntPref("network.proxy.type", 0);

  // Switch to TRR first
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  Services.prefs.setBoolPref("network.trr.display_fallback_warning", true);

  // Simulate a tripped canary network
  Services.dns.setHeuristicDetectionResult(
    Ci.nsITRRSkipReason.TRR_HEURISTIC_TRIPPED_CANARY
  );

  // We should see the fallback warning displayed in both of these scenarios
  Services.dns.clearCache(true);
  await verifyError(
    "https://www.example.com",
    true,
    "canary heuristic tripped"
  );
  await verifyError(
    "https://does-not-exist.test",
    true,
    "canary heuristic tripped - non existent url"
  );

  reset();
});
