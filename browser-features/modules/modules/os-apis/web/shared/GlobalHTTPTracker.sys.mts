/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Global HTTP request tracker to accurately monitor network idle state.
 * This tracker lives for the lifetime of the module and monitors all instances.
 * Shared between TabManagerServices and WebScraperServices.
 */
const GlobalHTTPTracker = {
  activeRequests: new Map<number, Set<nsIRequest>>(),
  _initialized: false,

  init() {
    if (this._initialized) return;
    this._initialized = true;
    try {
      Services.obs.addObserver(this, "http-on-opening-request");
      Services.obs.addObserver(this, "http-on-stop-request");
    } catch (e) {
      console.error("GlobalHTTPTracker: init failed:", e);
    }
  },

  observe(subject: nsISupports, topic: string, _data: string | null) {
    try {
      // deno-lint-ignore no-explicit-any
      const channel = (subject as any).QueryInterface(Ci.nsIHttpChannel);
      const bcid = channel.loadInfo?.browsingContextID;
      if (!bcid) return;

      if (topic === "http-on-opening-request") {
        const url = channel.URI.spec;
        if (url.startsWith("http") || url.startsWith("https")) {
          let requests = this.activeRequests.get(bcid);
          if (!requests) {
            requests = new Set();
            this.activeRequests.set(bcid, requests);
          }
          requests.add(channel);
        }
      } else if (topic === "http-on-stop-request") {
        const requests = this.activeRequests.get(bcid);
        if (requests) {
          requests.delete(channel);
          if (requests.size === 0) {
            this.activeRequests.delete(bcid);
          }
        }
      }
    } catch {
      // Ignore
    }
  },

  getActiveCount(bcid: number): number {
    return this.activeRequests.get(bcid)?.size || 0;
  },
};

GlobalHTTPTracker.init();

export { GlobalHTTPTracker };
