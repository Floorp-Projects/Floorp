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
      Services.obs.addObserver(this, "browsing-context-detached");
    } catch (e) {
      console.error("GlobalHTTPTracker: init failed:", e);
    }
  },

  observe(subject: nsISupports, topic: string, _data: string | null) {
    // Auto-cleanup when a browsing context is destroyed (tab closed, process crash)
    if (topic === "browsing-context-detached") {
      try {
        // BrowsingContext is a WebIDL object, not XPCOM — no QueryInterface needed
        const bc = subject as { id?: number };
        if (bc.id != null) this.clearForContext(bc.id);
      } catch {
        // ignore
      }
      return;
    }

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

  getActiveURLs(bcid: number): string[] {
    const requests = this.activeRequests.get(bcid);
    if (!requests) return [];
    const urls: string[] = [];
    for (const req of requests) {
      try {
        // deno-lint-ignore no-explicit-any
        const channel = req as any;
        if (channel.URI?.spec) {
          urls.push(channel.URI.spec);
        }
      } catch {
        // ignore
      }
    }
    return urls;
  },

  clearForContext(bcid: number): void {
    this.activeRequests.delete(bcid);
  },
};

GlobalHTTPTracker.init();

export { GlobalHTTPTracker };
