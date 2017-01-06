/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const DefaultUA = Cc["@mozilla.org/network/protocol;1?name=http"].getService(Ci.nsIHttpProtocolHandler).userAgent;
const NS_HTTP_ON_USERAGENT_REQUEST_TOPIC = "http-on-useragent-request";

XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyServiceGetter(this, "eTLDService", "@mozilla.org/network/effective-tld-service;1", "nsIEffectiveTLDService");

class UAOverrider {
  constructor(overrides) {
    this._overrides = {};
    this._overrideForURICache = new Map();

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
    let uaOverride = this.getUAForURI(channel.URI);

    if (uaOverride) {
      channel.setRequestHeader("User-Agent", uaOverride, false);
    }
  }

  getUAForURI(uri) {
    let bareUri = uri.specIgnoringRef;
    if (this._overrideForURICache.has(bareUri)) {
      // Although the cache could have an entry to a bareUri, `false` is also
      // a value that could be cached. A `false` cache entry means that there
      // is no override for this URI.
      // We cache these to avoid having to walk through all overrides to see
      // if a domain matches.
      let cachedUA = this._overrideForURICache.get(bareUri);
      return (cachedUA ? cachedUA : DefaultUA);
    }

    let finalUA = this.lookupUAOverride(uri);
    this._overrideForURICache.set(bareUri, finalUA);

    return finalUA;
  }

  /**
   * This function gets called from within the embedded webextension to check
   * if the current site has been overriden or not. We only check the cached
   * URI list here, but that's safe in our case since the tabUpdateHandler will
   * always run after our message observer.
   */
  hasUAForURIInCache(uri) {
    let bareUri = uri.specIgnoringRef;
    if (this._overrideForURICache.has(bareUri)) {
      return !!this._overrideForURICache.get(bareUri);
    }

    return false;
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
    let baseDomain = eTLDService.getBaseDomain(uri);
    if (this._overrides[baseDomain]) {
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
