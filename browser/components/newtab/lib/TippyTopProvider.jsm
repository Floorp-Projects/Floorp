/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

XPCOMUtils.defineLazyGlobalGetters(lazy, ["fetch"]);

const TIPPYTOP_PATH = "chrome://activity-stream/content/data/content/tippytop/";
const TIPPYTOP_JSON_PATH =
  "chrome://activity-stream/content/data/content/tippytop/top_sites.json";

/*
 * Get a domain from a url optionally stripping subdomains.
 */
function getDomain(url, strip = "www.") {
  let domain = "";
  try {
    domain = new URL(url).hostname;
  } catch (ex) {}
  if (strip === "*") {
    try {
      domain = Services.eTLD.getBaseDomainFromHost(domain);
    } catch (ex) {}
  } else if (domain.startsWith(strip)) {
    domain = domain.slice(strip.length);
  }
  return domain;
}

class TippyTopProvider {
  constructor() {
    this._sitesByDomain = new Map();
    this.initialized = false;
  }

  async init() {
    // Load the Tippy Top sites from the json manifest.
    try {
      for (const site of await (
        await lazy.fetch(TIPPYTOP_JSON_PATH, {
          credentials: "omit",
        })
      ).json()) {
        for (const domain of site.domains) {
          this._sitesByDomain.set(domain, site);
        }
      }
      this.initialized = true;
    } catch (error) {
      Cu.reportError("Failed to load tippy top manifest.");
    }
  }

  processSite(site, strip) {
    const tippyTop = this._sitesByDomain.get(getDomain(site.url, strip));
    if (tippyTop) {
      site.tippyTopIcon = TIPPYTOP_PATH + tippyTop.image_url;
      site.smallFavicon = TIPPYTOP_PATH + tippyTop.favicon_url;
      site.backgroundColor = tippyTop.background_color;
    }
    return site;
  }
}

const EXPORTED_SYMBOLS = ["TippyTopProvider", "getDomain"];
