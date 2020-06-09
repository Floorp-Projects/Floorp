/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global browser */
/* exported runHeuristics */

const GLOBAL_CANARY = "use-application-dns.net";

const NXDOMAIN_ERR = "NS_ERROR_UNKNOWN_HOST";

async function dnsLookup(hostname, resolveCanonicalName = false) {
  let flags = ["disable_trr", "disable_ipv6", "bypass_cache"];
  if (resolveCanonicalName) {
    flags.push("canonical_name");
  }

  let addresses, canonicalName, err;

  try {
    let response = await browser.dns.resolve(hostname, flags);
    addresses = response.addresses;
    canonicalName = response.canonicalName;
  } catch (e) {
    addresses = [null];
    err = e.message;
  }

  return { addresses, canonicalName, err };
}

async function dnsListLookup(domainList) {
  let results = [];

  for (let domain of domainList) {
    let { addresses } = await dnsLookup(domain);
    results = results.concat(addresses);
  }

  return results;
}

async function safeSearch() {
  const providerList = [
    {
      name: "google",
      unfiltered: ["www.google.com", "google.com"],
      safeSearch: ["forcesafesearch.google.com"],
    },
    {
      name: "youtube",
      unfiltered: [
        "www.youtube.com",
        "m.youtube.com",
        "youtubei.googleapis.com",
        "youtube.googleapis.com",
        "www.youtube-nocookie.com",
      ],
      safeSearch: ["restrict.youtube.com", "restrictmoderate.youtube.com"],
    },
  ];

  // Compare strict domain lookups to non-strict domain lookups
  let safeSearchChecks = {};
  for (let provider of providerList) {
    let providerName = provider.name;
    safeSearchChecks[providerName] = "enable_doh";

    let results = {};
    results.unfilteredAnswers = await dnsListLookup(provider.unfiltered);
    results.safeSearchAnswers = await dnsListLookup(provider.safeSearch);

    // Given a provider, check if the answer for any safe search domain
    // matches the answer for any default domain
    for (let answer of results.safeSearchAnswers) {
      if (answer && results.unfilteredAnswers.includes(answer)) {
        safeSearchChecks[providerName] = "disable_doh";
      }
    }
  }

  return safeSearchChecks;
}

async function zscalerCanary() {
  const ZSCALER_CANARY = "sitereview.zscaler.com";

  let { addresses } = await dnsLookup(ZSCALER_CANARY);
  for (let address of addresses) {
    if (
      ["213.152.228.242", "199.168.151.251", "8.25.203.30"].includes(address)
    ) {
      // if sitereview.zscaler.com resolves to either one of the 3 IPs above,
      // Zscaler Shift service is in use, don't enable DoH
      return "disable_doh";
    }
  }

  return "enable_doh";
}

// TODO: Confirm the expected behavior when filtering is on
async function globalCanary() {
  let { addresses, err } = await dnsLookup(GLOBAL_CANARY);

  if (err === NXDOMAIN_ERR || !addresses.length) {
    return "disable_doh";
  }

  return "enable_doh";
}

async function modifiedRoots() {
  // Check for presence of enterprise_roots cert pref. If enabled, disable DoH
  let rootsEnabled = await browser.experiments.preferences.getBoolPref(
    "security.enterprise_roots.enabled",
    false
  );

  if (rootsEnabled) {
    return "disable_doh";
  }

  return "enable_doh";
}

// Check if the network provides a DoH endpoint to use. Returns the name of the
// provider if the check is successful, else null.
async function providerSteering() {
  if (
    !(await browser.experiments.preferences.getBoolPref(
      "doh-rollout.provider-steering.enabled",
      false
    ))
  ) {
    return null;
  }
  const TEST_DOMAIN = "doh.test";

  // Array of { name, canonicalName, uri } where name is an identifier for
  // telemetry, canonicalName is the expected CNAME when looking up doh.test,
  // and uri is the provider's DoH endpoint.
  let steeredProviders = await browser.experiments.preferences.getCharPref(
    "doh-rollout.provider-steering.provider-list",
    "[]"
  );
  try {
    steeredProviders = JSON.parse(steeredProviders);
  } catch (e) {
    console.log("Provider list is invalid JSON, moving on.");
    return null;
  }

  if (!steeredProviders || !steeredProviders.length) {
    return null;
  }

  let { canonicalName, err } = await dnsLookup(TEST_DOMAIN, true);
  if (err || !canonicalName) {
    return null;
  }

  let provider = steeredProviders.find(p => {
    return p && p.canonicalName == canonicalName;
  });
  if (!provider || !provider.uri || !provider.name) {
    return null;
  }

  // We handle this here instead of background.js since we need to set this
  // override every time we run heuristics.
  browser.experiments.heuristics.setDetectedTrrURI(provider.uri);

  return provider.name;
}

async function runHeuristics() {
  let safeSearchChecks = await safeSearch();
  let results = {
    google: safeSearchChecks.google,
    youtube: safeSearchChecks.youtube,
    zscalerCanary: await zscalerCanary(),
    canary: await globalCanary(),
    modifiedRoots: await modifiedRoots(),
    browserParent: await browser.experiments.heuristics.checkParentalControls(),
    thirdPartyRoots: await browser.experiments.heuristics.checkThirdPartyRoots(),
    policy: await browser.experiments.heuristics.checkEnterprisePolicies(),
    steeredProvider: "",
  };

  // If any of those were triggered, return the results immediately.
  if (Object.values(results).includes("disable_doh")) {
    return results;
  }

  // Check for provider steering only after the other heuristics have passed.
  results.steeredProvider = (await providerSteering()) || "";
  return results;
}
