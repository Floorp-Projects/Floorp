/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * This module implements the policy to block websites from being visited,
 * or to only allow certain websites to be visited.
 *
 * The blocklist takes as input an array of MatchPattern strings, as documented
 * at https://developer.mozilla.org/en-US/Add-ons/WebExtensions/Match_patterns.
 *
 * The exceptions list takes the same as input. This list opens up
 * exceptions for rules on the blocklist that might be too strict.
 *
 * In addition to that, this allows the user to create a whitelist approach,
 * by using the special "<all_urls>" pattern for the blocklist, and then
 * adding all whitelisted websites on the exceptions list.
 *
 * Note that this module only blocks top-level website navigations. It doesn't
 * block any other accesses to these urls: image tags, scripts, XHR, etc.,
 * because that could cause unexpected breakage. This is a policy to block
 * users from visiting certain websites, and not from blocking any network
 * connections to those websites. If the admin is looking for that, the recommended
 * way is to configure that with extensions or through a company firewall.
 */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

const LIST_LENGTH_LIMIT = 1000;

const PREF_LOGLEVEL = "browser.policies.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm", {});
  return new ConsoleAPI({
    prefix: "WebsiteFilter Policy",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

var EXPORTED_SYMBOLS = [ "WebsiteFilter" ];

function WebsiteFilter(blocklist, exceptionlist) {
  let blockArray = [], exceptionArray = [];

  for (let i = 0; i < blocklist.length && i < LIST_LENGTH_LIMIT; i++) {
    try {
      let pattern = new MatchPattern(blocklist[i]);
      blockArray.push(pattern);
      log.debug(`Pattern added to WebsiteFilter.Block list: ${blocklist[i]}`);
    } catch (e) {
      log.error(`Invalid pattern on WebsiteFilter.Block: ${blocklist[i]}`);
    }
  }

  this._blockPatterns = new MatchPatternSet(blockArray);

  for (let i = 0; i < exceptionlist.length && i < LIST_LENGTH_LIMIT; i++) {
    try {
      let pattern = new MatchPattern(exceptionlist[i]);
      exceptionArray.push(pattern);
      log.debug(`Pattern added to WebsiteFilter.Exceptions list: ${exceptionlist[i]}`);
    } catch (e) {
      log.error(`Invalid pattern on WebsiteFilter.Exceptions: ${exceptionlist[i]}`);
    }
  }

  if (exceptionArray.length) {
    this._exceptionsPatterns = new MatchPatternSet(exceptionArray);
  }

  Services.obs.addObserver(this, "http-on-modify-request", true);
}

WebsiteFilter.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver,
                                          Ci.nsISupportsWeakReference]),

  observe(subject, topic, data) {
    let channel, isDocument = false;
    try {
      channel = subject.QueryInterface(Ci.nsIHttpChannel);
      isDocument = channel.isDocument;
    } catch (e) {
      return;
    }

    // Only filter document accesses
    if (!isDocument) {
      return;
    }

    if (this._blockPatterns.matches(channel.URI)) {
      if (!this._exceptionsPatterns ||
          !this._exceptionsPatterns.matches(channel.URI)) {
        // NS_ERROR_BLOCKED_BY_POLICY displays the error message
        // designed for policy-related blocks.
        channel.cancel(Cr.NS_ERROR_BLOCKED_BY_POLICY);
      }
    }
  }
};
