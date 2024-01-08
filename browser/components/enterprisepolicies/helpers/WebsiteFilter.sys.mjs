/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

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
 * In addition to that, this allows the user to create an allowlist approach,
 * by using the special "<all_urls>" pattern for the blocklist, and then
 * adding all allowlisted websites on the exceptions list.
 *
 * Note that this module only blocks top-level website navigations and embeds.
 * It does not block any other accesses to these urls: image tags, scripts, XHR, etc.,
 * because that could cause unexpected breakage. This is a policy to block
 * users from visiting certain websites, and not from blocking any network
 * connections to those websites. If the admin is looking for that, the recommended
 * way is to configure that with extensions or through a company firewall.
 */

const LIST_LENGTH_LIMIT = 1000;

const PREF_LOGLEVEL = "browser.policies.loglevel";

const lazy = {};

ChromeUtils.defineLazyGetter(lazy, "log", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  return new ConsoleAPI({
    prefix: "WebsiteFilter Policy",
    // tip: set maxLogLevel to "debug" and use log.debug() to create detailed
    // messages during development. See LOG_LEVELS in Console.sys.mjs for details.
    maxLogLevel: "error",
    maxLogLevelPref: PREF_LOGLEVEL,
  });
});

export let WebsiteFilter = {
  init(blocklist, exceptionlist) {
    let blockArray = [],
      exceptionArray = [];

    for (let i = 0; i < blocklist.length && i < LIST_LENGTH_LIMIT; i++) {
      try {
        let pattern = new MatchPattern(blocklist[i].toLowerCase());
        blockArray.push(pattern);
        lazy.log.debug(
          `Pattern added to WebsiteFilter. Block: ${blocklist[i]}`
        );
      } catch (e) {
        lazy.log.error(
          `Invalid pattern on WebsiteFilter. Block: ${blocklist[i]}`
        );
      }
    }

    this._blockPatterns = new MatchPatternSet(blockArray);

    for (let i = 0; i < exceptionlist.length && i < LIST_LENGTH_LIMIT; i++) {
      try {
        let pattern = new MatchPattern(exceptionlist[i].toLowerCase());
        exceptionArray.push(pattern);
        lazy.log.debug(
          `Pattern added to WebsiteFilter. Exception: ${exceptionlist[i]}`
        );
      } catch (e) {
        lazy.log.error(
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
    // We have to do this to catch 30X redirects.
    // See bug 456957.
    Services.obs.addObserver(this, "http-on-examine-response", true);
  },

  shouldLoad(contentLocation, loadInfo) {
    let contentType = loadInfo.externalContentPolicyType;
    let url = contentLocation.spec;
    if (contentLocation.scheme == "view-source") {
      url = contentLocation.pathQueryRef;
    } else if (url.toLowerCase().startsWith("about:reader")) {
      url = decodeURIComponent(
        url.toLowerCase().substr("about:reader?url=".length)
      );
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
  shouldProcess(contentLocation, loadInfo) {
    return Ci.nsIContentPolicy.ACCEPT;
  },
  observe(subject, topic, data) {
    try {
      let channel = subject.QueryInterface(Ci.nsIHttpChannel);
      if (
        !channel.isDocument ||
        channel.responseStatus < 300 ||
        channel.responseStatus >= 400
      ) {
        return;
      }
      let location = channel.getResponseHeader("location");
      // location might not be a fully qualified URL
      let url;
      try {
        url = new URL(location);
      } catch (e) {
        url = new URL(location, channel.URI.spec);
      }
      if (this._blockPatterns.matches(url.href.toLowerCase())) {
        if (
          !this._exceptionsPatterns ||
          !this._exceptionsPatterns.matches(url.href.toLowerCase())
        ) {
          channel.cancel(Cr.NS_ERROR_BLOCKED_BY_POLICY);
        }
      }
    } catch (e) {}
  },
  classDescription: "Policy Engine File Content Policy",
  contractID: "@mozilla-org/policy-engine-file-content-policy-service;1",
  classID: Components.ID("{c0bbb557-813e-4e25-809d-b46a531a258f}"),
  QueryInterface: ChromeUtils.generateQI([
    "nsIContentPolicy",
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
  createInstance(iid) {
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
