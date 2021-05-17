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
 * Note that this module only blocks top-level website navigations and embeds.
 * It does not block any other accesses to these urls: image tags, scripts, XHR, etc.,
 * because that could cause unexpected breakage. This is a policy to block
 * users from visiting certain websites, and not from blocking any network
 * connections to those websites. If the admin is looking for that, the recommended
 * way is to configure that with extensions or through a company firewall.
 */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const LIST_LENGTH_LIMIT = 1000;

const PREF_LOGLEVEL = "browser.policies.loglevel";

XPCOMUtils.defineLazyGetter(this, "log", () => {
  let { ConsoleAPI } = ChromeUtils.import("resource://gre/modules/Console.jsm");
  return new ConsoleAPI({
    prefix: "WebsiteFilter Policy",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.jsm for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

var EXPORTED_SYMBOLS = ["WebsiteFilter"];

let WebsiteFilter = {
  init(blocklist, exceptionlist) {
    let blockArray = [],
      exceptionArray = [];

    for (let i = 0; i < blocklist.length && i < LIST_LENGTH_LIMIT; i++) {
      try {
        let pattern = new MatchPattern(blocklist[i].toLowerCase());
        blockArray.push(pattern);
        log.debug(`Pattern added to WebsiteFilter. Block: ${blocklist[i]}`);
      } catch (e) {
        log.error(`Invalid pattern on WebsiteFilter. Block: ${blocklist[i]}`);
      }
    }

    this._blockPatterns = new MatchPatternSet(blockArray);

    for (let i = 0; i < exceptionlist.length && i < LIST_LENGTH_LIMIT; i++) {
      try {
        let pattern = new MatchPattern(exceptionlist[i].toLowerCase());
        exceptionArray.push(pattern);
        log.debug(
          `Pattern added to WebsiteFilter. Exception: ${exceptionlist[i]}`
        );
      } catch (e) {
        log.error(
          `Invalid pattern on WebsiteFilter. Exception: ${exceptionlist[i]}`
        );
      }
    }

    if (exceptionArray.length) {
      this._exceptionsPatterns = new MatchPatternSet(exceptionArray);
    }

    let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

    if (!registrar.isContractIDRegistered(this.contractID)) {
      registrar.registerFactory(
        this.classID,
        this.classDescription,
        this.contractID,
        this
      );

      Services.catMan.addCategoryEntry(
        "content-policy",
        this.contractID,
        this.contractID,
        false,
        true
      );
    }
  },

  shouldLoad(contentLocation, loadInfo, mimeTypeGuess) {
    let contentType = loadInfo.externalContentPolicyType;
    let url = contentLocation.spec;
    if (contentLocation.scheme == "view-source") {
      url = contentLocation.pathQueryRef;
    } else if (url.startsWith("about:reader")) {
      url = decodeURIComponent(url.substr("about:reader?url=".length));
    }
    if (
      contentType == Ci.nsIContentPolicy.TYPE_DOCUMENT ||
      contentType == Ci.nsIContentPolicy.TYPE_SUBDOCUMENT
    ) {
      if (this._blockPatterns.matches(url.toLowerCase())) {
        if (
          !this._exceptionsPatterns ||
          !this._exceptionsPatterns.matches(url.toLowerCase())
        ) {
          return Ci.nsIContentPolicy.REJECT_POLICY;
        }
      }
    }
    return Ci.nsIContentPolicy.ACCEPT;
  },
  shouldProcess(contentLocation, loadInfo, mimeTypeGuess) {
    return Ci.nsIContentPolicy.ACCEPT;
  },
  classDescription: "Policy Engine File Content Policy",
  contractID: "@mozilla-org/policy-engine-file-content-policy-service;1",
  classID: Components.ID("{c0bbb557-813e-4e25-809d-b46a531a258f}"),
  QueryInterface: ChromeUtils.generateQI(["nsIContentPolicy"]),
  createInstance(outer, iid) {
    return this.QueryInterface(iid);
  },
  isAllowed(url) {
    if (this._blockPatterns?.matches(url.toLowerCase())) {
      if (
        !this._exceptionsPatterns ||
        !this._exceptionsPatterns.matches(url.toLowerCase())
      ) {
        return false;
      }
    }
    return true;
  },
};
