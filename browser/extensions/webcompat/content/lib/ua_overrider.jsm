/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Console.jsm");

const DefaultUA = Cc["@mozilla.org/network/protocol;1?name=http"].getService(Ci.nsIHttpProtocolHandler).userAgent;
const NS_HTTP_ON_USERAGENT_REQUEST_TOPIC = "http-on-useragent-request";

XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "eTLDService", "@mozilla.org/network/effective-tld-service;1", "nsIEffectiveTLDService");

class UAOverrider {
  constructor(overrides) {
    this._overrides = {};

    this.initOverrides(overrides);
  }

  initOverrides(overrides) {
    for (let override of overrides) {
      if (!this._overrides[override.baseDomain]) {
        this._overrides[override.baseDomain] = [];
      }

      if (!override.uriMatcher) {
        override.uriMatcher = () => true;
      }

      this._overrides[override.baseDomain].push(override);
    }
  }

  init() {
    Services.obs.addObserver(this, NS_HTTP_ON_USERAGENT_REQUEST_TOPIC, false);
  }

  uninit() {
    Services.obs.removeObserver(this, NS_HTTP_ON_USERAGENT_REQUEST_TOPIC);
  }

  observe(subject, topic) {
    if (topic !== NS_HTTP_ON_USERAGENT_REQUEST_TOPIC) {
      return;
    }

    let channel = subject.QueryInterface(Components.interfaces.nsIHttpChannel);
    let uaOverride = this.lookupUAOverride(channel.URI);

    if (uaOverride) {
      console.log("The user agent has been overridden for compatibility reasons.");
      channel.setRequestHeader("User-Agent", uaOverride, false);
    }
  }

  /**
   * Try to use the eTLDService to get the base domain (will return example.com
   * for http://foo.bar.example.com/foo/bar).
   *
   * However, the eTLDService is a bit picky and throws whenever we pass a
   * blank host name or an IP into it, see bug 1337785. Since we do not plan on
   * override UAs for such cases, we simply catch everything and return false.
   */
  getBaseDomainFromURI(uri) {
    try {
      return eTLDService.getBaseDomain(uri);
    } catch (_) {
      return false;
    }
  }

  /**
   * This function returns a User Agent based on the URI passed into. All
   * override rules are defined in data/ua_overrides.jsm and the required format
   * is explained there.
   *
   * Since it is expected and designed to have more than one override per base
   * domain, we have to loop over this._overrides[baseDomain], which contains
   * all available overrides.
   *
   * If the uriMatcher function returns true, the uaTransformer function gets
   * called and its result will be used as the Use Agent for the current
   * request.
   *
   * If there are more than one possible overrides, that is if two or more
   * uriMatchers would return true, the first one gets applied.
   */
  lookupUAOverride(uri) {
    let baseDomain = this.getBaseDomainFromURI(uri);
    if (baseDomain && this._overrides[baseDomain]) {
      for (let uaOverride of this._overrides[baseDomain]) {
        if (uaOverride.uriMatcher(uri.specIgnoringRef)) {
          return uaOverride.uaTransformer(DefaultUA);
        }
      }
    }

    return false;
  }
}

this.EXPORTED_SYMBOLS = ["UAOverrider"]; /* exported UAOverrider */
