/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global browser */
/* exported runHeuristics */

const GLOBAL_CANARY = "use-application-dns.net";

const NXDOMAIN_ERR = "NS_ERROR_UNKNOWN_HOST";

async function dnsLookup(hostname) {
  let flags = ["disable_trr", "disable_ipv6", "bypass_cache"];
  let addresses, err;

  try {
    let response = await browser.dns.resolve(hostname, flags);
    addresses = response.addresses;
  } catch (e) {
    addresses = [null];
    err = e.message;
  }

  return { addresses, err };
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

async function runHeuristics() {
  let safeSearchChecks = await safeSearch();
  let zscalerCheck = await zscalerCanary();
  let canaryCheck = await globalCanary();
  let modifiedRootsCheck = await modifiedRoots();

  // Check other heuristics through privileged code
  let browserParentCheck = await browser.experiments.heuristics.checkParentalControls();
  let enterpriseCheck = await browser.experiments.heuristics.checkEnterprisePolicies();
  let thirdPartyRootsCheck = await browser.experiments.heuristics.checkThirdPartyRoots();

  // Return result of each heuristic
  return {
    google: safeSearchChecks.google,
    youtube: safeSearchChecks.youtube,
    zscalerCanary: zscalerCheck,
    canary: canaryCheck,
    modifiedRoots: modifiedRootsCheck,
    browserParent: browserParentCheck,
    thirdPartyRoots: thirdPartyRootsCheck,
    policy: enterpriseCheck,
  };
}
