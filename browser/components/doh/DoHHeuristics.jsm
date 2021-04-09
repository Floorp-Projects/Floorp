/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * This module implements the heuristics used to determine whether to enable
 * or disable DoH on different networks. DoHController is responsible for running
 * these at startup and upon network changes.
 */
var EXPORTED_SYMBOLS = ["Heuristics", "parentalControls"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gDNSService",
  "@mozilla.org/network/dns-service;1",
  "nsIDNSService"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gNetworkLinkService",
  "@mozilla.org/network/network-link-service;1",
  "nsINetworkLinkService"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gParentalControlsService",
  "@mozilla.org/parental-controls-service;1",
  "nsIParentalControlsService"
);

ChromeUtils.defineModuleGetter(
  this,
  "Config",
  "resource:///modules/DoHConfig.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const GLOBAL_CANARY = "use-application-dns.net.";

const NXDOMAIN_ERR = "NS_ERROR_UNKNOWN_HOST";

const Heuristics = {
  // String constants used to indicate outcome of heuristics.
  ENABLE_DOH: "enable_doh",
  DISABLE_DOH: "disable_doh",

  async run() {
    // Run all the heuristics at the same time.
    let [safeSearchChecks, zscaler, canary] = await Promise.all([
      safeSearch(),
      zscalerCanary(),
      globalCanary(),
    ]);

    let platformChecks = await platform();
    let results = {
      google: safeSearchChecks.google,
      youtube: safeSearchChecks.youtube,
      zscalerCanary: zscaler,
      canary,
      modifiedRoots: await modifiedRoots(),
      browserParent: await parentalControls(),
      thirdPartyRoots: await thirdPartyRoots(),
      policy: await enterprisePolicy(),
      vpn: platformChecks.vpn,
      proxy: platformChecks.proxy,
      nrpt: platformChecks.nrpt,
      steeredProvider: "",
    };

    // If any of those were triggered, return the results immediately.
    if (Object.values(results).includes("disable_doh")) {
      return results;
    }

    // Check for provider steering only after the other heuristics have passed.
    results.steeredProvider = (await providerSteering()) || "";
    return results;
  },

  async checkEnterprisePolicy() {
    return enterprisePolicy();
  },

  // Test only
  async _setMockLinkService(mockLinkService) {
    this.mockLinkService = mockLinkService;
  },
};

async function dnsLookup(hostname, resolveCanonicalName = false) {
  let lookupPromise = new Promise((resolve, reject) => {
    let request;
    let response = {
      addresses: [],
    };
    let listener = {
      onLookupComplete(inRequest, inRecord, inStatus) {
        if (inRequest === request) {
          if (!Components.isSuccessCode(inStatus)) {
            reject({ message: new Components.Exception("", inStatus).name });
            return;
          }
          inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
          if (resolveCanonicalName) {
            try {
              response.canonicalName = inRecord.canonicalName;
            } catch (e) {
              // no canonicalName
            }
          }
          while (inRecord.hasMore()) {
            let addr = inRecord.getNextAddrAsString();
            // Sometimes there are duplicate records with the same ip.
            if (!response.addresses.includes(addr)) {
              response.addresses.push(addr);
            }
          }
          resolve(response);
        }
      },
    };
    let dnsFlags =
      Ci.nsIDNSService.RESOLVE_TRR_DISABLED_MODE |
      Ci.nsIDNSService.RESOLVE_DISABLE_IPV6 |
      Ci.nsIDNSService.RESOLVE_BYPASS_CACHE |
      Ci.nsIDNSService.RESOLVE_CANONICAL_NAME;
    try {
      request = gDNSService.asyncResolve(
        hostname,
        Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
        dnsFlags,
        null,
        listener,
        null,
        {} /* defaultOriginAttributes */
      );
    } catch (e) {
      // handle exceptions such as offline mode.
      reject({ message: e.name });
    }
  });

  let addresses, canonicalName, err;

  try {
    let response = await lookupPromise;
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

  let resolutions = await Promise.all(
    domainList.map(domain => dnsLookup(domain))
  );
  for (let { addresses } of resolutions) {
    results = results.concat(addresses);
  }

  return results;
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
  let rootsEnabled = Preferences.get(
    "security.enterprise_roots.enabled",
    false
  );

  if (rootsEnabled) {
    return "disable_doh";
  }

  return "enable_doh";
}

async function parentalControls() {
  if (gParentalControlsService.parentalControlsEnabled) {
    return "disable_doh";
  }

  return "enable_doh";
}

async function thirdPartyRoots() {
  if (Cu.isInAutomation) {
    return "enable_doh";
  }

  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );

  let hasThirdPartyRoots = await new Promise(resolve => {
    certdb.asyncHasThirdPartyRoots(resolve);
  });

  if (hasThirdPartyRoots) {
    return "disable_doh";
  }

  return "enable_doh";
}

async function enterprisePolicy() {
  if (Services.policies.status === Services.policies.ACTIVE) {
    let policies = Services.policies.getActivePolicies();

    if (!policies.hasOwnProperty("DNSOverHTTPS")) {
      // If DoH isn't in the policy, return that there is a policy (but no DoH specifics)
      return "policy_without_doh";
    }

    if (policies.DNSOverHTTPS.Enabled === true) {
      // If DoH is enabled in the policy, enable it
      return "enable_doh";
    }

    // If DoH is disabled in the policy, disable it
    return "disable_doh";
  }

  // Default return, meaning no policy related to DNSOverHTTPS
  return "no_policy_set";
}

async function safeSearch() {
  const providerList = [
    {
      name: "google",
      unfiltered: ["www.google.com.", "google.com."],
      safeSearch: ["forcesafesearch.google.com."],
    },
    {
      name: "youtube",
      unfiltered: [
        "www.youtube.com.",
        "m.youtube.com.",
        "youtubei.googleapis.com.",
        "youtube.googleapis.com.",
        "www.youtube-nocookie.com.",
      ],
      safeSearch: ["restrict.youtube.com.", "restrictmoderate.youtube.com."],
    },
  ];

  async function checkProvider(provider) {
    let [unfilteredAnswers, safeSearchAnswers] = await Promise.all([
      dnsListLookup(provider.unfiltered),
      dnsListLookup(provider.safeSearch),
    ]);

    // Given a provider, check if the answer for any safe search domain
    // matches the answer for any default domain
    for (let answer of safeSearchAnswers) {
      if (answer && unfilteredAnswers.includes(answer)) {
        return { name: provider.name, result: "disable_doh" };
      }
    }

    return { name: provider.name, result: "enable_doh" };
  }

  // Compare strict domain lookups to non-strict domain lookups.
  // Resolutions has a type of [{ name, result }]
  let resolutions = await Promise.all(
    providerList.map(provider => checkProvider(provider))
  );

  // Reduce that array entries into a single map
  return resolutions.reduce(
    (accumulator, check) => {
      accumulator[check.name] = check.result;
      return accumulator;
    },
    {} // accumulator
  );
}

async function zscalerCanary() {
  const ZSCALER_CANARY = "sitereview.zscaler.com.";

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

async function platform() {
  let platformChecks = {};

  let indications = Ci.nsINetworkLinkService.NONE_DETECTED;
  try {
    let linkService = gNetworkLinkService;
    if (Heuristics.mockLinkService) {
      linkService = Heuristics.mockLinkService;
    }
    indications = linkService.platformDNSIndications;
  } catch (e) {
    if (e.result != Cr.NS_ERROR_NOT_IMPLEMENTED) {
      Cu.reportError(e);
    }
  }

  platformChecks.vpn =
    indications & Ci.nsINetworkLinkService.VPN_DETECTED
      ? "disable_doh"
      : "enable_doh";
  platformChecks.proxy =
    indications & Ci.nsINetworkLinkService.PROXY_DETECTED
      ? "disable_doh"
      : "enable_doh";
  platformChecks.nrpt =
    indications & Ci.nsINetworkLinkService.NRPT_DETECTED
      ? "disable_doh"
      : "enable_doh";

  return platformChecks;
}

// Check if the network provides a DoH endpoint to use. Returns the name of the
// provider if the check is successful, else null. Currently we only support
// this for Comcast networks.
async function providerSteering() {
  if (!Config.providerSteering.enabled) {
    return null;
  }
  const TEST_DOMAIN = "doh.test.";

  // Array of { name, canonicalName, uri } where name is an identifier for
  // telemetry, canonicalName is the expected CNAME when looking up doh.test,
  // and uri is the provider's DoH endpoint.
  let steeredProviders = Config.providerSteering.providerList;
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
    return p.canonicalName == canonicalName;
  });
  if (!provider || !provider.uri || !provider.name) {
    return null;
  }

  return provider;
}
