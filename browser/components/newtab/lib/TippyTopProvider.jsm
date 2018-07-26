/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["fetch", "URL"]);

const TIPPYTOP_JSON_PATH = "resource://activity-stream/data/content/tippytop/top_sites.json";
const TIPPYTOP_URL_PREFIX = "resource://activity-stream/data/content/tippytop/images/";

function getDomain(url) {
  let domain;
  try {
    domain = new URL(url).hostname;
  } catch (ex) {}
  if (domain && domain.startsWith("www.")) {
    domain = domain.slice(4);
  }
  return domain;
}

this.TippyTopProvider = class TippyTopProvider {
  constructor() {
    this._sitesByDomain = new Map();
    this.initialized = false;
  }

  async init() {
    // Load the Tippy Top sites from the json manifest.
    try {
      for (const site of await (await fetch(TIPPYTOP_JSON_PATH)).json()) {
        // The tippy top manifest can have a url property (string) or a
        // urls property (array of strings)
        for (const url of site.url ? [site.url] : site.urls || []) {
          this._sitesByDomain.set(getDomain(url), site);
        }
      }
      this.initialized = true;
    } catch (error) {
      Cu.reportError("Failed to load tippy top manifest.");
    }
  }

  processSite(site) {
    const tippyTop = this._sitesByDomain.get(getDomain(site.url));
    if (tippyTop) {
      site.tippyTopIcon = TIPPYTOP_URL_PREFIX + tippyTop.image_url;
      site.backgroundColor = tippyTop.background_color;
    }
    return site;
  }
};

const EXPORTED_SYMBOLS = ["TippyTopProvider", "getDomain"];
