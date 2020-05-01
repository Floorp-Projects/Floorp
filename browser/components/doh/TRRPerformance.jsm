/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * This module tests TRR performance by issuing DNS requests to TRRs and
 * recording telemetry for the network time for each request.
 *
 * We test each TRR with 5 random subdomains of a canonical domain and also
 * a "popular" domain (which the TRR likely have cached).
 *
 * To ensure data integrity, we run the requests in an aggregator wrapper
 * and collect all the results before sending telemetry. If we detect network
 * loss, the results are discarded. A new run is triggered upon detection of
 * usable network until a full set of results has been captured. We stop retrying
 * after 5 attempts.
 */
var EXPORTED_SYMBOLS = ["TRRRacer", "DNSLookup", "LookupAggregator"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

Services.telemetry.setEventRecordingEnabled(
  "security.doh.trrPerformance",
  true
);

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gNetworkLinkService",
  "@mozilla.org/network/network-link-service;1",
  "nsINetworkLinkService"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gCaptivePortalService",
  "@mozilla.org/network/captive-portal-service;1",
  "nsICaptivePortalService"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gDNSService",
  "@mozilla.org/network/dns-service;1",
  "nsIDNSService"
);

XPCOMUtils.defineLazyServiceGetter(
  this,
  "gUUIDGenerator",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
);

// The list of participating TRRs.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "kTRRs",
  "network.trr.resolvers",
  null,
  null,
  val =>
    val
      ? JSON.parse(val).map(trr => trr.url)
      : [
          "https://mozilla.cloudflare-dns.com/dns-query",
          "https://trr.dns.nextdns.io/",
          "https://doh.xfinity.com/dns-query",
        ]
);

// The canonical domain whose subdomains we will be resolving.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "kCanonicalDomain",
  "doh-rollout.trrRace.canonicalDomain",
  "firefox-dns-perf-test.net"
);

// The number of random subdomains to resolve per TRR.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "kRepeats",
  "doh-rollout.trrRace.randomSubdomainCount",
  5
);

// The "popular" domain that we expect the TRRs to have cached.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "kPopularDomains",
  "doh-rollout.trrRace.popularDomains",
  null,
  null,
  val =>
    val
      ? val.split(",").map(t => t.trim())
      : ["google.com", "youtube.com", "amazon.com", "facebook.com", "yahoo.com"]
);

function getRandomSubdomain() {
  let uuid = gUUIDGenerator
    .generateUUID()
    .toString()
    .slice(1, -1); // Discard surrounding braces
  return `${uuid}.${kCanonicalDomain}`;
}

// A wrapper around async DNS lookups. The results are passed on to the supplied
// callback. The wrapper attempts the lookup 3 times before passing on a failure.
// If a false-y `domain` is supplied, a random subdomain will be used. Each retry
// will use a different random subdomain to ensure we bypass chached responses.
class DNSLookup {
  constructor(domain, trrServer, callback) {
    this._domain = domain;
    this.trrServer = trrServer;
    this.callback = callback;
    this.retryCount = 0;
  }

  doLookup() {
    this.retryCount++;
    try {
      this.usedDomain = this._domain || getRandomSubdomain();
      gDNSService.asyncResolveWithTrrServer(
        this.usedDomain,
        this.trrServer,
        Ci.nsIDNSService.RESOLVE_BYPASS_CACHE,
        this,
        Services.tm.currentThread,
        {}
      );
    } catch (e) {
      Cu.reportError(e);
    }
  }

  onLookupComplete(request, record, status) {
    // Try again if we failed...
    if (!Components.isSuccessCode(status) && this.retryCount < 3) {
      this.doLookup();
      return;
    }

    // But after the third try, just pass the status on.
    this.callback(request, record, status, this.usedDomain, this.retryCount);
  }
}

DNSLookup.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIDNSListener,
]);

// A wrapper around a single set of measurements. The required lookups are
// triggered and the results aggregated before telemetry is sent. If aborted,
// any aggregated results are discarded.
class LookupAggregator {
  constructor(onCompleteCallback) {
    this.onCompleteCallback = onCompleteCallback;
    this.aborted = false;
    this.networkUnstable = false;
    this.captivePortal = false;

    this.domains = [];
    for (let i = 0; i < kRepeats; ++i) {
      // false-y domain will cause DNSLookup to generate a random one.
      this.domains.push(null);
    }
    this.domains.push(...kPopularDomains);
    this.totalLookups = kTRRs.length * this.domains.length;
    this.completedLookups = 0;
    this.results = [];
  }

  run() {
    if (this._ran || this._aborted) {
      Cu.reportError("Trying to re-run a LookupAggregator.");
      return;
    }

    this._ran = true;
    for (let trr of kTRRs) {
      for (let domain of this.domains) {
        new DNSLookup(
          domain,
          trr,
          (request, record, status, usedDomain, retryCount) => {
            this.results.push({
              domain: usedDomain,
              trr,
              status,
              time: record ? record.trrFetchDurationNetworkOnly : -1,
              retryCount,
            });

            this.completedLookups++;
            if (this.completedLookups == this.totalLookups) {
              this.recordResults();
            }
          }
        ).doLookup();
      }
    }
  }

  abort() {
    this.aborted = true;
  }

  markUnstableNetwork() {
    this.networkUnstable = true;
  }

  markCaptivePortal() {
    this.captivePortal = true;
  }

  recordResults() {
    if (this.aborted) {
      return;
    }

    for (let { domain, trr, status, time, retryCount } of this.results) {
      if (
        !(kPopularDomains.includes(domain) || domain.includes(kCanonicalDomain))
      ) {
        Cu.reportError("Expected known domain for reporting, got " + domain);
        return;
      }

      Services.telemetry.recordEvent(
        "security.doh.trrPerformance",
        "resolved",
        "record",
        "success",
        {
          domain,
          trr,
          status: status.toString(),
          time: time.toString(),
          retryCount: retryCount.toString(),
          networkUnstable: this.networkUnstable.toString(),
          captivePortal: this.captivePortal.toString(),
        }
      );
    }

    this.onCompleteCallback();
  }
}

// This class monitors the network and spawns a new LookupAggregator when ready.
// When the network goes down, an ongoing aggregator is aborted and a new one
// spawned next time we get a link, up to 5 times. On the fifth time, we just
// let the aggegator complete and mark it as tainted.
class TRRRacer {
  constructor(onCompleteCallback) {
    this._aggregator = null;
    this._retryCount = 0;
    this._complete = false;
    this._onCompleteCallback = onCompleteCallback;
  }

  run() {
    if (
      gNetworkLinkService.isLinkUp &&
      gCaptivePortalService.state != gCaptivePortalService.LOCKED_PORTAL
    ) {
      this._runNewAggregator();
      if (
        gCaptivePortalService.state == gCaptivePortalService.UNLOCKED_PORTAL
      ) {
        this._aggregator.markCaptivePortal();
      }
    }

    Services.obs.addObserver(this, "ipc:network:captive-portal-set-state");
    Services.obs.addObserver(this, "network:link-status-changed");
  }

  onComplete() {
    Services.obs.removeObserver(this, "ipc:network:captive-portal-set-state");
    Services.obs.removeObserver(this, "network:link-status-changed");

    this._complete = true;

    if (this._onCompleteCallback) {
      this._onCompleteCallback();
    }
  }

  getFastestTRR(returnRandomDefault = false) {
    if (!this._complete) {
      throw new Error("getFastestTRR: Measurement still running.");
    }

    return this._getFastestTRRFromResults(
      this._aggregator.results,
      returnRandomDefault
    );
  }

  /*
   * Given an array of { trr, time }, returns the trr with smallest mean time.
   * Separate from _getFastestTRR for easy unit-testing.
   *
   * @returns The TRR with the fastest average time.
   *          If returnRandomDefault is false-y, returns undefined if no valid
   *          times were present in the results. Otherwise, returns one of the
   *          present TRRs at random.
   */
  _getFastestTRRFromResults(results, returnRandomDefault = false) {
    // First, organize the results into a map of TRR -> array of times
    let TRRTimingMap = new Map();
    let TRRErrorCount = new Map();
    for (let { trr, time } of results) {
      if (!TRRTimingMap.has(trr)) {
        TRRTimingMap.set(trr, []);
      }
      if (time != -1) {
        TRRTimingMap.get(trr).push(time);
      } else {
        TRRErrorCount.set(trr, 1 + (TRRErrorCount.get(trr) || 0));
      }
    }

    // Loop through each TRR's array of times, compute the geometric means,
    // and remember the fastest TRR. Geometric mean is a bit more forgiving
    // in the presence of noise (anomalously high values).
    // We don't need the full geometric mean, we simply calculate the arithmetic
    // means in log-space and then compare those values.
    let fastestTRR;
    let fastestAverageTime = -1;
    let trrs = [...TRRTimingMap.keys()];
    for (let trr of trrs) {
      let times = TRRTimingMap.get(trr);
      if (!times.length) {
        continue;
      }

      // Skip TRRs that had an error rate of more than 30%.
      let errorCount = TRRErrorCount.get(trr) || 0;
      let totalResults = times.length + errorCount;
      if (errorCount / totalResults > 0.3) {
        continue;
      }

      // Arithmetic mean in log space. Take log of (a + 1) to ensure we never
      // take log(0) which would be -Infinity.
      let averageTime =
        times.map(a => Math.log(a + 1)).reduce((a, b) => a + b) / times.length;
      if (fastestAverageTime == -1 || averageTime < fastestAverageTime) {
        fastestAverageTime = averageTime;
        fastestTRR = trr;
      }
    }

    if (returnRandomDefault && !fastestTRR) {
      fastestTRR = trrs[Math.floor(Math.random() * trrs.length)];
    }

    return fastestTRR;
  }

  _runNewAggregator() {
    this._aggregator = new LookupAggregator(() => this.onComplete());
    this._aggregator.run();
    this._retryCount++;
  }

  // When the link goes *down*, or when we detect a locked captive portal, we
  // abort any ongoing LookupAggregator run. When the link goes *up*, or we
  // detect a newly unlocked portal, we start a run if one isn't ongoing.
  observe(subject, topic, data) {
    switch (topic) {
      case "network:link-status-changed":
        if (this._aggregator && data == "down") {
          if (this._retryCount < 5) {
            this._aggregator.abort();
          } else {
            this._aggregator.markUnstableNetwork();
          }
        } else if (
          data == "up" &&
          (!this._aggregator || this._aggregator.aborted)
        ) {
          this._runNewAggregator();
        }
        break;
      case "ipc:network:captive-portal-set-state":
        if (
          this._aggregator &&
          gCaptivePortalService.state == gCaptivePortalService.LOCKED_PORTAL
        ) {
          if (this._retryCount < 5) {
            this._aggregator.abort();
          } else {
            this._aggregator.markCaptivePortal();
          }
        } else if (
          gCaptivePortalService.state ==
            gCaptivePortalService.UNLOCKED_PORTAL &&
          (!this._aggregator || this._aggregator.aborted)
        ) {
          this._runNewAggregator();
        }
        break;
    }
  }
}
