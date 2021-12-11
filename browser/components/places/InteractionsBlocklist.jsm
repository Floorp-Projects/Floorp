/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["InteractionsBlocklist"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  FilterAdult: "resource://activity-stream/lib/FilterAdult.jsm",
  Services: "resource://gre/modules/Services.jsm",
  UrlbarUtils: "resource:///modules/UrlbarUtils.jsm",
});

XPCOMUtils.defineLazyGetter(this, "logConsole", function() {
  return console.createInstance({
    prefix: "InteractionsBlocklist",
    maxLogLevel: Services.prefs.getBoolPref(
      "browser.places.interactions.log",
      false
    )
      ? "Debug"
      : "Warn",
  });
});

// A blocklist of regular expressions. Maps base hostnames to a list regular
// expressions for URLs with that base hostname. In this context, "base
// hostname" means the hostname without any subdomains or a public suffix. For
// example, the base hostname for "https://www.maps.google.com/a/place" is
// "google". We do this mapping to improve performance; otherwise we'd have to
// check all URLs against a long list of regular expressions. The regexes are
// defined as escaped strings so that we build them lazily.
// We may want to migrate this list to Remote Settings in the future.
let HOST_BLOCKLIST = {
  baidu: [
    // Baidu SERP
    "^(https?:\\/\\/)?(www\\.)?baidu\\.com\\/s.*(\\?|&)wd=.*",
  ],
  bing: [
    // Bing SERP
    "^(https?:\\/\\/)?(www\\.)?bing\\.com\\/search.*(\\?|&)q=.*",
  ],
  duckduckgo: [
    // DuckDuckGo SERP
    "^(https?:\\/\\/)?(www\\.)?duckduckgo\\.com\\/.*(\\?|&)q=.*",
  ],
  google: [
    // Google SERP
    "^(https?:\\/\\/)?(www\\.)?google\\.(\\w|\\.){2,}\\/search.*(\\?|&)q=.*",
  ],
  yandex: [
    // Yandex SERP
    "^(https?:\\/\\/)?(www\\.)?yandex\\.(\\w|\\.){2,}\\/search.*(\\?|&)text=.*",
  ],
  zoom: [
    // Zoom meeting interstitial
    "^(https?:\\/\\/)?(www\\.)?.*\\.zoom\\.us\\/j\\/\\d+",
  ],
};

HOST_BLOCKLIST = new Proxy(HOST_BLOCKLIST, {
  get(target, property) {
    let regexes = target[property];
    if (!regexes || !Array.isArray(regexes)) {
      return null;
    }

    for (let i = 0; i < regexes.length; i++) {
      let regex = regexes[i];
      if (typeof regex === "string") {
        regex = new RegExp(regex, "i");
        if (regex) {
          regexes[i] = regex;
        } else {
          throw new Error("Blocklist contains invalid regex.");
        }
      }
    }
    return regexes;
  },
});

/**
 * A class that maintains a blocklist of URLs. The class exposes a method to
 * check if a particular URL is contained on the blocklist.
 */
class _InteractionsBlocklist {
  constructor() {
    // Load custom blocklist items from pref.
    try {
      let customBlocklist = JSON.parse(
        Services.prefs.getStringPref(
          "places.interactions.customBlocklist",
          "[]"
        )
      );
      if (!Array.isArray(customBlocklist)) {
        throw new Error();
      }
      let parsedBlocklist = customBlocklist.map(
        regexStr => new RegExp(regexStr)
      );
      HOST_BLOCKLIST["*"] = parsedBlocklist;
    } catch (ex) {
      logConsole.warn("places.interactions.customBlocklist is corrupted.");
    }
  }

  /**
   * Checks a URL against a blocklist of URLs. If the URL is blocklisted, we
   * should not record an interaction.
   *
   * @param {string} urlToCheck
   *   The URL we are looking for on the blocklist.
   * @returns {boolean}
   *  True if `url` is on a blocklist. False otherwise.
   */
  isUrlBlocklisted(urlToCheck) {
    if (FilterAdult.isAdultUrl(urlToCheck)) {
      return true;
    }

    // First, find the URL's base host: the hostname without any subdomains or a
    // public suffix.
    let url;
    try {
      url = new URL(urlToCheck);
      if (!url) {
        throw new Error();
      }
    } catch (ex) {
      logConsole.warn(
        `Invalid URL passed to InteractionsBlocklist.isUrlBlocklisted: ${url}`
      );
      return false;
    }
    let hostWithoutSuffix = UrlbarUtils.stripPublicSuffixFromHost(url.host);
    let [hostWithSubdomains] = UrlbarUtils.stripPrefixAndTrim(
      hostWithoutSuffix,
      {
        stripWww: true,
        trimTrailingDot: true,
      }
    );
    let baseHost = hostWithSubdomains.substring(
      hostWithSubdomains.lastIndexOf(".") + 1
    );
    // Then fetch blocked regexes for that baseHost and compare them to the full
    // URL. Also check the URL against the custom blocklist.
    let regexes = HOST_BLOCKLIST[baseHost.toLocaleLowerCase()] || [];
    regexes.push(...(HOST_BLOCKLIST["*"] || []));
    if (!regexes) {
      return false;
    }

    return regexes.some(r => r.test(url.href));
  }

  /**
   * Adds a regex to HOST_BLOCKLIST. Since we can't parse the base host from
   * the regex, we add it to a list of wildcard regexes. All URLs are checked
   * against these wildcard regexes.
   *
   * @param {string|RegExp} regexToAdd
   *   The regular expression to add to our blocklist.
   * @note Currently only exposed for tests and use in the console. In the
   *       future we could hook this up to a UI component.
   */
  addRegexToBlocklist(regexToAdd) {
    let regex;
    try {
      regex = new RegExp(regexToAdd, "i");
    } catch (ex) {
      this.logConsole.warn("Invalid regex passed to addRegexToBlocklist.");
      return;
    }

    if (!HOST_BLOCKLIST["*"]) {
      HOST_BLOCKLIST["*"] = [];
    }
    HOST_BLOCKLIST["*"].push(regex);
    Services.prefs.setStringPref(
      "places.interactions.customBlocklist",
      JSON.stringify(HOST_BLOCKLIST["*"].map(reg => reg.toString()))
    );
  }

  /**
   * Removes a regex from HOST_BLOCKLIST. If `regexToRemove` is not in the
   * blocklist, this is a no-op.
   *
   * @param {string|RegExp} regexToRemove
   *   The regular expression to add to our blocklist.
   * @note Currently only exposed for tests and use in the console. In the
   *       future we could hook this up to a UI component.
   */
  removeRegexFromBlocklist(regexToRemove) {
    let regex;
    try {
      regex = new RegExp(regexToRemove, "i");
    } catch (ex) {
      this.logConsole.warn("Invalid regex passed to addRegexToBlocklist.");
      return;
    }

    if (!HOST_BLOCKLIST["*"] || !Array.isArray(HOST_BLOCKLIST["*"])) {
      return;
    }
    HOST_BLOCKLIST["*"] = HOST_BLOCKLIST["*"].filter(
      curr => curr.source != regex.source
    );
    Services.prefs.setStringPref(
      "places.interactions.customBlocklist",
      JSON.stringify(HOST_BLOCKLIST["*"].map(reg => reg.toString()))
    );
  }
}

const InteractionsBlocklist = new _InteractionsBlocklist();
