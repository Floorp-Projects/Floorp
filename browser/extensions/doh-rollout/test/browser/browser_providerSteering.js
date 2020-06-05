"use strict";

const TEST_DOMAIN = "doh.test";

add_task(setup);

add_task(async function testProviderSteering() {
  setPassingHeuristics();
  let prefPromise = TestUtils.waitForPrefChange(prefs.DOH_SELF_ENABLED_PREF);
  Preferences.set(prefs.DOH_ENABLED_PREF, true);
  await prefPromise;
  is(Preferences.get(prefs.DOH_SELF_ENABLED_PREF), true, "Breadcrumb saved.");
  await checkHeuristicsTelemetry("enable_doh", "startup");

  let providerTestcases = [
    {
      name: "provider1",
      canonicalName: "foo.provider1.com",
      uri: "https://foo.provider1.com/query",
    },
    {
      name: "provider2",
      canonicalName: "bar.provider2.com",
      uri: "https://bar.provider2.com/query",
    },
  ];
  Preferences.set(
    prefs.DOH_PROVIDER_STEERING_LIST_PREF,
    JSON.stringify(providerTestcases)
  );

  for (let { name, canonicalName, uri } of providerTestcases) {
    gDNSOverride.addIPOverride(TEST_DOMAIN, "9.9.9.9");
    gDNSOverride.setCnameOverride(TEST_DOMAIN, canonicalName);
    let trrURIChanged = TestUtils.topicObserved(
      "network:trr-uri-changed",
      () => {
        // We need this check because this topic is observed once immediately
        // after the network change when the URI is reset, and then when the
        // provider steering heuristic runs and sets it to our uri.
        return gDNSService.currentTrrURI == uri;
      }
    );
    simulateNetworkChange();
    await trrURIChanged;
    is(gDNSService.currentTrrURI, uri, "TRR URI set to provider endpoint");
    await checkHeuristicsTelemetry("enable_doh", "netchange", name);
    gDNSOverride.clearHostOverride(TEST_DOMAIN);
  }

  let trrURIChanged = TestUtils.topicObserved("network:trr-uri-changed");
  simulateNetworkChange();
  await trrURIChanged;
  is(
    gDNSService.currentTrrURI,
    "https://dummytrr.com/query",
    "TRR URI set to auto-selected"
  );
  await checkHeuristicsTelemetry("enable_doh", "netchange");

  // Set enterprise roots enabled and ensure provider steering is disabled.
  Preferences.set("security.enterprise_roots.enabled", true);
  gDNSOverride.setCnameOverride(
    TEST_DOMAIN,
    providerTestcases[0].canonicalName
  );
  simulateNetworkChange();
  await checkHeuristicsTelemetry("disable_doh", "netchange");
  is(
    gDNSService.currentTrrURI,
    "https://dummytrr.com/query",
    "TRR URI set to auto-selected"
  );
  Preferences.reset("security.enterprise_roots.enabled");
  gDNSOverride.clearHostOverride(TEST_DOMAIN);
});
