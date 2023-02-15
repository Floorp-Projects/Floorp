/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  Heuristics: "resource:///modules/DoHHeuristics.sys.mjs",
});

add_task(setup);

add_task(async function testPlatformIndications() {
  // Check if the platform heuristics actually cause a "disable_doh" event

  let { MockRegistrar } = ChromeUtils.importESModule(
    "resource://testing-common/MockRegistrar.sys.mjs"
  );

  let mockedLinkService = {
    isLinkUp: true,
    linkStatusKnown: true,
    linkType: Ci.nsINetworkLinkService.LINK_TYPE_WIFI,
    networkID: "abcd",
    dnsSuffixList: [],
    platformDNSIndications: Ci.nsINetworkLinkService.NONE_DETECTED,
    QueryInterface: ChromeUtils.generateQI(["nsINetworkLinkService"]),
  };

  let networkLinkServiceCID = MockRegistrar.register(
    "@mozilla.org/network/network-link-service;1",
    mockedLinkService
  );

  Heuristics._setMockLinkService(mockedLinkService);
  registerCleanupFunction(async () => {
    MockRegistrar.unregister(networkLinkServiceCID);
    Heuristics._setMockLinkService(undefined);
  });

  setPassingHeuristics();
  let prefPromise = TestUtils.waitForPrefChange(prefs.BREADCRUMB_PREF);
  Preferences.set(prefs.ENABLED_PREF, true);
  await prefPromise;
  is(Preferences.get(prefs.BREADCRUMB_PREF), true, "Breadcrumb saved.");
  await checkHeuristicsTelemetry("enable_doh", "startup");

  await ensureTRRMode(2);

  mockedLinkService.platformDNSIndications =
    Ci.nsINetworkLinkService.VPN_DETECTED;
  simulateNetworkChange();
  await ensureTRRMode(0);
  await checkHeuristicsTelemetry("disable_doh", "netchange");

  mockedLinkService.platformDNSIndications =
    Ci.nsINetworkLinkService.PROXY_DETECTED;
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  await checkHeuristicsTelemetry("disable_doh", "netchange");

  mockedLinkService.platformDNSIndications =
    Ci.nsINetworkLinkService.NRPT_DETECTED;
  simulateNetworkChange();
  await ensureNoTRRModeChange(0);
  await checkHeuristicsTelemetry("disable_doh", "netchange");

  mockedLinkService.platformDNSIndications =
    Ci.nsINetworkLinkService.NONE_DETECTED;
  simulateNetworkChange();
  await ensureTRRMode(2);
  await checkHeuristicsTelemetry("enable_doh", "netchange");
});
