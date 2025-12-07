/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Offscreen API Polyfill for Firefox
 *
 * This module provides a polyfill for Chrome's Offscreen API using hidden iframes
 * in Firefox's MV3 background scripts (which have DOM access).
 *
 * Background:
 * - Chrome Offscreen API allows extensions to run DOM-based operations in hidden contexts
 * - Firefox doesn't have a native equivalent
 * - Firefox MV3 background scripts have DOM access (unlike Chrome's service workers)
 * - This polyfill leverages that DOM access to emulate the Offscreen API
 *
 * See: docs/investigations/offscreen-api-support.md for full investigation details
 *
 * Status: DRAFT - Not yet integrated into build pipeline
 * TODO: Integrate with CRXConverter to inject during extension conversion
 */

// =============================================================================
// Constants
// =============================================================================

/**
 * Timeout for offscreen document loading (in milliseconds)
 */
const OFFSCREEN_LOAD_TIMEOUT_MS = 10000;

/**
 * Firefox preference key for debug mode
 */
const DEBUG_PREF_KEY = "extensions.floorp.debug";

// =============================================================================
// Types
// =============================================================================

/**
 * Options for creating an offscreen document
 */
export interface OffscreenDocumentOptions {
  /** URL of the offscreen document (must be within extension) */
  url: string;
  /** Reasons for creating the offscreen document */
  reasons: OffscreenReason[];
  /** Justification for creating the offscreen document (for auditing) */
  justification: string;
}

/**
 * Reasons for creating an offscreen document
 * These match Chrome's Offscreen API enum values
 */
export type OffscreenReason =
  | "AUDIO_PLAYBACK"
  | "BLOBS"
  | "CLIPBOARD"
  | "DOM_PARSER"
  | "DOM_SCRAPING"
  | "IFRAME_SCRIPTING"
  | "LOCAL_STORAGE"
  | "MEDIA_STREAM"
  | "OFFSCREEN_CANVAS"
  | "TESTING"
  | "USER_MEDIA"
  | "WEB_RTC";

/**
 * Chrome Offscreen API interface
 */
export interface OffscreenAPI {
  createDocument(options: OffscreenDocumentOptions): Promise<void>;
  closeDocument(): Promise<void>;
  hasDocument(): Promise<boolean>;
}

// =============================================================================
// Polyfill Implementation
// =============================================================================

/**
 * Polyfill implementation for Chrome Offscreen API
 *
 * Limitations:
 * - Only works in contexts with DOM access (background scripts)
 * - Won't work in pure service worker contexts
 * - Lifecycle may differ slightly from Chrome's implementation
 * - Performance characteristics may differ from native implementation
 */
class OffscreenPolyfillImpl implements OffscreenAPI {
  private offscreenFrame: HTMLIFrameElement | null = null;
  private readonly debugMode: boolean = false;

  constructor(options?: { debug?: boolean }) {
    this.debugMode = options?.debug ?? false;
  }

  /**
   * Create an offscreen document
   * @param options - Configuration for the offscreen document
   * @throws Error if document already exists or options are invalid
   */
  async createDocument(options: OffscreenDocumentOptions): Promise<void> {
    this.log("createDocument called with:", options);

    // Validate: Only one offscreen document allowed
    if (this.offscreenFrame) {
      throw new Error(
        "Only one offscreen document may exist at a time. " +
          "Call closeDocument() first.",
      );
    }

    // Validate: Must have at least one reason
    if (!options.reasons || options.reasons.length === 0) {
      throw new Error("Must specify at least one reason");
    }

    // Validate: URL must be provided
    if (!options.url) {
      throw new Error("URL must be specified");
    }

    // Validate: Justification must be provided
    if (!options.justification) {
      throw new Error("Justification must be specified");
    }

    // Ensure we're in a context with DOM access
    if (typeof document === "undefined") {
      throw new Error(
        "Offscreen API polyfill requires DOM access. " +
          "This context does not support offscreen documents.",
      );
    }

    try {
      // Create the hidden iframe
      const iframe = this.createHiddenIframe(options.url);

      // Wait for iframe to load
      await this.waitForIframeLoad(iframe);

      // Store reference
      this.offscreenFrame = iframe;

      this.log("Offscreen document created successfully");
    } catch (error) {
      // Clean up on error
      this.cleanupFrame();
      throw new Error(
        `Failed to create offscreen document: ${(error as Error).message}`,
      );
    }
  }

  /**
   * Close the offscreen document
   */
  async closeDocument(): Promise<void> {
    this.log("closeDocument called");

    if (!this.offscreenFrame) {
      // Chrome doesn't throw an error if no document exists, so we don't either
      this.log("No offscreen document to close");
      return;
    }

    this.cleanupFrame();
    this.log("Offscreen document closed");
  }

  /**
   * Check if an offscreen document exists
   * @returns true if document exists, false otherwise
   */
  async hasDocument(): Promise<boolean> {
    const exists = this.offscreenFrame !== null;
    this.log("hasDocument:", exists);
    return exists;
  }

  // =============================================================================
  // Private Helper Methods
  // =============================================================================

  /**
   * Create a hidden iframe element
   */
  private createHiddenIframe(url: string): HTMLIFrameElement {
    const iframe = document.createElement("iframe");

    // Make iframe completely hidden and inaccessible
    iframe.style.cssText = `
      display: none !important;
      position: absolute !important;
      left: -9999px !important;
      top: -9999px !important;
      width: 0 !important;
      height: 0 !important;
      visibility: hidden !important;
      opacity: 0 !important;
      pointer-events: none !important;
    `;

    // Set URL using browser.runtime.getURL for proper extension URL resolution
    // Note: In Firefox extensions, browser.runtime.getURL is available
    const resolvedUrl =
      typeof browser !== "undefined" && browser.runtime?.getURL
        ? browser.runtime.getURL(url)
        : url;

    iframe.src = resolvedUrl;

    // Add metadata for debugging
    iframe.setAttribute("data-floorp-offscreen", "true");
    iframe.setAttribute("data-floorp-offscreen-url", url);

    // Append to document body
    document.body.appendChild(iframe);

    return iframe;
  }

  /**
   * Wait for iframe to finish loading
   */
  private waitForIframeLoad(iframe: HTMLIFrameElement): Promise<void> {
    return new Promise((resolve, reject) => {
      const timeout = setTimeout(() => {
        reject(new Error("Offscreen document load timeout"));
      }, OFFSCREEN_LOAD_TIMEOUT_MS);

      iframe.addEventListener(
        "load",
        () => {
          clearTimeout(timeout);
          resolve();
        },
        { once: true },
      );

      iframe.addEventListener(
        "error",
        () => {
          clearTimeout(timeout);
          reject(new Error("Failed to load offscreen document"));
        },
        { once: true },
      );
    });
  }

  /**
   * Clean up the iframe and references
   */
  private cleanupFrame(): void {
    if (this.offscreenFrame) {
      try {
        this.offscreenFrame.remove();
      } catch (error) {
        // Ignore errors during cleanup
        this.log("Error during cleanup:", error);
      }
      this.offscreenFrame = null;
    }
  }

  /**
   * Log debug messages
   */
  private log(...args: unknown[]): void {
    if (this.debugMode) {
      console.log("[Floorp Offscreen Polyfill]", ...args);
    }
  }
}

// =============================================================================
// Installation
// =============================================================================

/**
 * Install the Offscreen API polyfill into the chrome object
 *
 * This function should be called early in the background script lifecycle,
 * before any extension code that might use the Offscreen API.
 *
 * @param options - Configuration options
 * @returns true if polyfill was installed, false if already exists
 */
export function installOffscreenPolyfill(options?: {
  debug?: boolean;
  force?: boolean;
}): boolean {
  // Check if we're in a browser extension context
  if (typeof chrome === "undefined") {
    console.warn(
      "[Floorp Offscreen Polyfill] Not in Chrome extension context",
    );
    return false;
  }

  // Check if Offscreen API already exists (e.g., if Chrome adds native support)
  if (chrome.offscreen && !options?.force) {
    if (options?.debug) {
      console.log(
        "[Floorp Offscreen Polyfill] Native Offscreen API detected, skipping polyfill",
      );
    }
    return false;
  }

  // Create polyfill instance
  const polyfill = new OffscreenPolyfillImpl({ debug: options?.debug });

  // Install into chrome object
  // @ts-expect-error - We're polyfilling a Chrome API that doesn't exist in types
  chrome.offscreen = {
    createDocument: polyfill.createDocument.bind(polyfill),
    closeDocument: polyfill.closeDocument.bind(polyfill),
    hasDocument: polyfill.hasDocument.bind(polyfill),
  };

  // Also install into browser object for compatibility
  if (typeof browser !== "undefined") {
    // @ts-expect-error - We're polyfilling a Chrome API that doesn't exist in types
    browser.offscreen = chrome.offscreen;
  }

  if (options?.debug) {
    console.log("[Floorp Offscreen Polyfill] Successfully installed");
  }

  return true;
}

// =============================================================================
// Exports
// =============================================================================

export { OffscreenPolyfillImpl as OffscreenPolyfill };

// Auto-install if this is loaded as a script (not as a module import)
// This allows the polyfill to be injected as a standalone script
if (typeof window !== "undefined" && !("__FLOORP_OFFSCREEN_MODULE__" in window)) {
  // Mark as loaded
  // @ts-expect-error - Adding custom property for polyfill tracking
  window.__FLOORP_OFFSCREEN_MODULE__ = true;

  // Auto-install with debug mode based on environment
  const debugMode =
    typeof Services !== "undefined" &&
    Services.prefs?.getBoolPref(DEBUG_PREF_KEY, false);

  installOffscreenPolyfill({ debug: debugMode });
}
