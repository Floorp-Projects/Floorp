/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const OFFSCREEN_POLYFILL_SOURCE = `
(function() {
  // Offscreen API Polyfill for Firefox
  // Generated from OffscreenPolyfill.sys.mts

  const OFFSCREEN_LOAD_TIMEOUT_MS = 10000;
  const DEBUG_PREF_KEY = "extensions.floorp.debug";

  class OffscreenPolyfillImpl {
    constructor(options) {
      this.offscreenFrame = null;
      this.debugMode = options?.debug ?? false;
    }

    async createDocument(options) {
      this.log("createDocument called with:", options);

      if (this.offscreenFrame) {
        throw new Error("Only one offscreen document may exist at a time. Call closeDocument() first.");
      }

      if (!options.reasons || options.reasons.length === 0) {
        throw new Error("Must specify at least one reason");
      }

      if (!options.url) {
        throw new Error("URL must be specified");
      }

      if (!options.justification) {
        throw new Error("Justification must be specified");
      }

      if (typeof document === "undefined") {
        throw new Error("Offscreen API polyfill requires DOM access. This context does not support offscreen documents.");
      }

      try {
        const iframe = this.createHiddenIframe(options.url);
        await this.waitForIframeLoad(iframe);
        this.offscreenFrame = iframe;
        this.log("Offscreen document created successfully");
      } catch (error) {
        this.cleanupFrame();
        throw new Error(\`Failed to create offscreen document: \${error.message}\`);
      }
    }

    async closeDocument() {
      this.log("closeDocument called");
      if (!this.offscreenFrame) {
        this.log("No offscreen document to close");
        return;
      }
      this.cleanupFrame();
      this.log("Offscreen document closed");
    }

    async hasDocument() {
      const exists = this.offscreenFrame !== null;
      this.log("hasDocument:", exists);
      return exists;
    }

    createHiddenIframe(url) {
      const iframe = document.createElement("iframe");
      iframe.style.cssText = \`
        display: none !important;
        position: absolute !important;
        left: -9999px !important;
        top: -9999px !important;
        width: 0 !important;
        height: 0 !important;
        visibility: hidden !important;
        opacity: 0 !important;
        pointer-events: none !important;
      \`;

      const resolvedUrl = typeof browser !== "undefined" && browser.runtime?.getURL
        ? browser.runtime.getURL(url)
        : url;

      iframe.src = resolvedUrl;
      iframe.setAttribute("data-floorp-offscreen", "true");
      iframe.setAttribute("data-floorp-offscreen-url", url);
      document.body.appendChild(iframe);
      return iframe;
    }

    waitForIframeLoad(iframe) {
      return new Promise((resolve, reject) => {
        const timeout = setTimeout(() => {
          reject(new Error("Offscreen document load timeout"));
        }, OFFSCREEN_LOAD_TIMEOUT_MS);

        iframe.addEventListener("load", () => {
          clearTimeout(timeout);
          resolve();
        }, { once: true });

        iframe.addEventListener("error", () => {
          clearTimeout(timeout);
          reject(new Error("Failed to load offscreen document"));
        }, { once: true });
      });
    }

    cleanupFrame() {
      if (this.offscreenFrame) {
        try {
          this.offscreenFrame.remove();
        } catch (error) {
          this.log("Error during cleanup:", error);
        }
        this.offscreenFrame = null;
      }
    }

    log(...args) {
      if (this.debugMode) {
        console.log("[Floorp Offscreen Polyfill]", ...args);
      }
    }
  }

  function installOffscreenPolyfill(options) {
    if (typeof chrome === "undefined") {
      console.warn("[Floorp Offscreen Polyfill] Not in Chrome extension context");
      return false;
    }

    if (chrome.offscreen && !options?.force) {
      return false;
    }

    const polyfill = new OffscreenPolyfillImpl({ debug: options?.debug });

    chrome.offscreen = {
      createDocument: polyfill.createDocument.bind(polyfill),
      closeDocument: polyfill.closeDocument.bind(polyfill),
      hasDocument: polyfill.hasDocument.bind(polyfill),
    };

    if (typeof browser !== "undefined") {
      browser.offscreen = chrome.offscreen;
    }

    if (options?.debug) {
      console.log("[Floorp Offscreen Polyfill] Successfully installed");
    }

    return true;
  }

  // Auto-install
  if (typeof window !== "undefined" && !("__FLOORP_OFFSCREEN_MODULE__" in window)) {
    window.__FLOORP_OFFSCREEN_MODULE__ = true;
    installOffscreenPolyfill({ debug: false });
  }
})();
`;
