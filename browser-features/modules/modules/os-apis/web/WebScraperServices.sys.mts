/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * WebScraperService - A service for creating and managing headless browser instances
 *
 * This module provides functionality to:
 * - Create isolated browser instances using HiddenFrame
 * - Navigate to URLs and wait for page loads
 * - Manage multiple browser instances with unique IDs
 * - Clean up resources when instances are no longer needed
 */

import type { WaitForElementState } from "../../os-server/shared/types.ts";
import { GlobalHTTPTracker } from "./shared/GlobalHTTPTracker.sys.mts";
import { AutomationConstants } from "../../os-server/shared/http-tracker.sys.mts";
import { PROGRESS_LISTENERS } from "./shared/ProgressListeners.sys.mts";
import { waitForActor } from "./shared/waitForActor.sys.mts";
import { CookieHelper } from "./shared/CookieHelper.sys.mts";
import { NetworkIdleHelper } from "./shared/NetworkIdleHelper.sys.mts";

const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
);
const { HiddenFrame } = ChromeUtils.importESModule(
  "resource://gre/modules/HiddenFrame.sys.mjs",
);
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// Actor interface for WebScraper communication
interface WebScraperActor {
  sendQuery(name: string, data?: object): Promise<unknown>;
}

// Global sets to prevent garbage collection of active components
const SCRAPER_ACTOR_SETS: Set<XULBrowserElement> = new Set();

function _perfLog(label: string, ms: number) {
  if (!Services.prefs.getBoolPref("floorp.os.perf.log", false)) return;
  console.debug(`[Floorp OS-Perf] IPC ${label} ${ms}ms`);
}
const FRAME = new HiddenFrame();

/**
 * WebScraper class that manages headless browser instances for web scraping
 *
 * This class provides a high-level interface for creating, managing, and
 * navigating browser instances without visible UI elements.
 */
class webScraper {
  // Map to store browser instances with their unique IDs
  private _browserInstances: Map<string, XULBrowserElement> = new Map();
  private _instanceId!: string;
  private _windowlessBrowser!: nsIWindowlessBrowser;

  /**
   * Try to get NRWebScraper actor with exponential backoff.
   *
   * Uses instanceId-based lookup so the browser reference is refreshed each
   * poll iteration, correctly handling process swaps (remoteness changes).
   */
  private _getActorForBrowser(
    instanceId: string,
    tries = 100,
    delayMs = 100,
  ): Promise<WebScraperActor | null> {
    // Preserve original total timeout budget from fixed-interval polling.
    return waitForActor<WebScraperActor>(
      () => this._browserInstances.get(instanceId) ?? null,
      "NRWebScraper",
      { maxMs: tries * delayMs },
    );
  }

  /**
   * Common helper: check browser → get actor → sendQuery.
   * Reduces boilerplate across all service methods.
   * Includes browser existence check and performance logging.
   */
  private async withActor<T>(
    instanceId: string,
    queryName: string,
    data?: object,
    fallback: T | null = null,
  ): Promise<T | null> {
    try {
      if (!this._browserInstances.has(instanceId)) {
        throw new Error(`Browser not found for instance ${instanceId}`);
      }
      const actor = await this._getActorForBrowser(instanceId);
      if (!actor) return fallback;
      const _t0 = Date.now();
      const result = (await actor.sendQuery(queryName, data)) as T;
      _perfLog(queryName, Date.now() - _t0);
      return result;
    } catch (e) {
      console.error(`Error in ${queryName}:`, e);
      return fallback;
    }
  }

  /**
   * Delays execution for user interaction visibility
   */
  constructor() {
    this._initializeWindowlessBrowser();
  }

  private async _initializeWindowlessBrowser(): Promise<void> {
    this._windowlessBrowser = await FRAME.get();
  }

  /**
   * Creates a new headless browser instance
   *
   * This method:
   * - Creates a HiddenFrame to host the browser
   * - Initializes a browser element with remote content capabilities
   * - Sets up proper sizing and attributes
   * - Returns a unique instance ID for future operations
   *
   * @returns Promise<string> - A unique identifier for the created browser instance
   */
  public async createInstance(): Promise<string> {
    const doc = this._windowlessBrowser.document;
    const browser = doc.createXULElement("browser");
    browser.setAttribute("remote", "true");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("manualactiveness", "true");

    // Set default size for browser element
    browser.style.setProperty("width", "1366px");
    browser.style.setProperty("min-width", "1366px");
    browser.style.setProperty("height", "768px");
    browser.style.setProperty("min-height", "768px");

    // Add browser element to document to properly initialize webNavigation
    doc.documentElement?.appendChild(browser);
    browser.docShellIsActive = true;
    if (browser.browsingContext) {
      browser.browsingContext.allowJavascript = true;
    }

    this._instanceId = await crypto.randomUUID();
    this._browserInstances.set(this._instanceId, browser);
    SCRAPER_ACTOR_SETS.add(browser);
    return this._instanceId;
  }

  /**
   * Destroys a browser instance and cleans up associated resources
   *
   * This method:
   * - Closes the browser instance
   * - Removes it from internal tracking maps
   * - Cleans up global references to prevent memory leaks
   *
   * @param instanceId - The unique identifier of the browser instance to destroy
   */
  public destroyInstance(instanceId: string): void {
    const browser = this._browserInstances.get(instanceId);
    if (browser) {
      // Clean up GlobalHTTPTracker for this instance's browsing context
      const bcid = browser.browsingContext?.id;
      if (bcid !== undefined && bcid !== null) {
        GlobalHTTPTracker.clearForContext(bcid);
      }
      browser.remove();
      this._browserInstances.delete(instanceId);
      SCRAPER_ACTOR_SETS.delete(browser);
    }
  }

  /**
   * Navigates a browser instance to a specified URL and waits for the page to load
   *
   * This method:
   * - Validates the browser instance exists
   * - Sets up proper security principals and origin attributes
   * - Loads the URL with appropriate remote type configuration
   * - Monitors page load progress and resolves when navigation is complete
   * - Handles various edge cases like inner-frame events and same-document changes
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param url - The URL to navigate to
   * @returns Promise<void> - Resolves when the page has finished loading
   * @throws Error - If the browser instance is not found
   */
  public async navigate(instanceId: string, url: string): Promise<void> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const principal = Services.scriptSecurityManager.getSystemPrincipal();
    const oa = E10SUtils.predictOriginAttributes({
      browser,
    });
    const loadURIOptions = {
      triggeringPrincipal: principal,
      remoteType: E10SUtils.getRemoteTypeForURI(
        url,
        true,
        false,
        E10SUtils.DEFAULT_REMOTE_TYPE,
        null,
        oa,
      ),
    };

    const uri = Services.io.newURI(url);

    if (typeof browser.loadURI === "function") {
      browser.loadURI(uri, loadURIOptions);
    } else {
      throw new Error("browser.loadURI is not defined");
    }

    // Wait for page load via progress listener (30s timeout)
    await new Promise<void>((resolve) => {
      let resolved = false;
      let timeoutId: ReturnType<typeof setTimeout> | null = null;
      const complete = () => {
        if (resolved) return;
        resolved = true;
        if (timeoutId !== null) clearTimeout(timeoutId);
        resolve();
      };

      const { webProgress } = browser;

      // Timeout guard: prevent hanging if page load never completes
      timeoutId = setTimeout(() => {
        if (resolved) return;
        console.warn("[WebScraperServices] navigate timed out after 30 seconds");
        resolved = true;
        PROGRESS_LISTENERS.delete(progressListener);
        try {
          webProgress.removeProgressListener(
            progressListener as nsIWebProgressListener,
          );
        } catch {
          // Listener may already be removed
        }
        resolve();
      }, 30000);

      /**
       * Progress listener to monitor page load completion
       *
       * This listener:
       * - Filters out non-top-level navigation events
       * - Ignores same-document location changes
       * - Handles about:blank page initialization
       * - Cleans up listeners when navigation is complete
       */
      const progressListener = {
        onLocationChange(
          progress: nsIWebProgress,
          _request: nsIRequest,
          location: nsIURI,
          flags: number,
        ) {
          // Ignore inner-frame events
          if (!progress.isTopLevel) {
            return;
          }
          // Ignore events that don't change the document
          if (
            flags &
            (Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT ?? 0)
          ) {
            return;
          }
          // Ignore the initial about:blank, unless about:blank is requested
          if (location.spec == "about:blank" && uri.spec != "about:blank") {
            return;
          }

          PROGRESS_LISTENERS.delete(progressListener);
          webProgress.removeProgressListener(
            progressListener as nsIWebProgressListener,
          );
          complete();
        },
        onStateChange(
          progress: nsIWebProgress,
          _request: nsIRequest,
          flags: number,
          _status: nsresult,
        ) {
          if (!progress.isTopLevel) return;
          const STATE_STOP = Ci.nsIWebProgressListener.STATE_STOP ?? 0;
          if (flags & STATE_STOP) {
            PROGRESS_LISTENERS.delete(progressListener);
            webProgress.removeProgressListener(
              progressListener as nsIWebProgressListener,
            );
            complete();
          }
        },
        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
      };
      PROGRESS_LISTENERS.add(progressListener);
      webProgress.addProgressListener(
        progressListener as nsIWebProgressListener,
        (Ci.nsIWebProgress.NOTIFY_LOCATION ?? 0) |
          (Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT ?? 0),
      );
    });

    // Wait for content actor readiness
    try {
      const actor = await this._getActorForBrowser(instanceId, 150, 100);
      if (actor) {
        const ok = await actor
          .sendQuery("WebScraper:WaitForReady", { timeout: 15000 })
          .catch(() => false);
        if (!ok) {
          await actor
            .sendQuery("WebScraper:WaitForElement", {
              selector: "body",
              timeout: 15000,
            })
            .catch(() => null);
        }
      }
    } catch {
      // ignore
    }
  }

  public getURI(instanceId: string): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    return new Promise((resolve) => {
      if (browser.browsingContext?.currentURI.spec != "about:blank") {
        resolve(browser.browsingContext?.currentURI.spec ?? "about:blank");
      } else {
        resolve(null);
      }
    });
  }

  /**
   * Retrieves the HTML content from a browser instance using the NRWebScraper actor
   *
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to extract HTML content from the content process
   * - Handles cases where the actor is not available or errors occur
   *
   * The actor communication allows safe access to content process DOM
   * without violating cross-process security boundaries.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - The HTML content as a string, or null if unavailable
   * @throws Error - If the browser instance is not found
   */
  public async getHTML(
    instanceId: string,
    options?: { selector?: string },
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:GetHTML", options ?? {});
  }

  /**
   * Retrieves the visible text content from a browser instance
   *
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to extract visible text content
   * - Returns text that excludes hidden elements (display:none), making it much smaller than raw HTML
   *
   * The actor communication allows safe access to content process DOM
   * without violating cross-process security boundaries.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - The visible text content, or null if unavailable
   * @throws Error - If the browser instance is not found
   */
  public async getText(
    instanceId: string,
    options:
      | boolean
      | {
          mode?: "full" | "scoped" | "visible";
          selector?: string;
          viewportMargin?: number;
          enableFingerprints?: boolean;
          includeSelectorMap?: boolean;
        } = false,
  ): Promise<string | null> {
    // Normalize: boolean old-style { includeSelectorMap }
    const data =
      typeof options === "boolean" ? { includeSelectorMap: options } : options;
    return this.withActor(instanceId, "WebScraper:GetText", data);
  }

  /**
   * Retrieves a specific element from a browser instance using the NRWebScraper actor
   *
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query with a CSS selector to find and return a specific element
   * - Returns null if the actor is not available
   *
   * The actor communication enables safe DOM element access across
   * process boundaries while maintaining security isolation.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<string | null> - The element's HTML content, or null if not found
   * @throws Error - If the browser instance is not found
   */
  public async getAccessibilityTree(
    instanceId: string,
    options: { interestingOnly?: boolean; root?: string } = {},
  ): Promise<unknown> {
    return this.withActor(instanceId, "WebScraper:GetAccessibilityTree", options);
  }

  public async getArticle(
    instanceId: string,
  ): Promise<{
    title: string;
    byline: string;
    markdown: string;
    length: number;
  } | null> {
    return this.withActor(instanceId, "WebScraper:GetArticle");
  }

  public async getElement(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:GetElement", { selector });
  }

  /** Get all elements matching a selector from a browser instance
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query with a CSS selector to find and return all matching elements
   * - Returns an array of elements' HTML content, or an empty array if none found
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target elements
   * @returns Promise<string[]> - An array of matching elements' HTML content
   * @throws Error - If the browser instance is not found
   */
  public async getElements(
    instanceId: string,
    selector: string,
  ): Promise<string[]> {
    return this.withActor<string[]>(instanceId, "WebScraper:GetElements", { selector }) as Promise<string[]>;
  }

  /**  Get Element By Text Content
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query with text content to find and return the matching element
   * - Returns null if the actor is not available or element not found
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param textContent - The text content to search forS
   * @returns Promise<string | null> - The element's HTML content, or null if not found
   * @throws Error - If the browser instance is not found
   */

  public async getElementByText(
    instanceId: string,
    textContent: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:GetElementByText", { textContent });
  }

  /** Get Element Text Content
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query with a CSS selector to find and return the text content of the element
   * - Returns null if the actor is not available or element not found
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<string | null> - The element's text content, or null if not found
   * @throws Error - If the browser instance is not found
   */

  public async getElementTextContent(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:GetElementTextContent", { selector });
  }

  /**
   * Retrieves the text content of an element from a browser instance
   *
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to extract text content from a specific element
   * - Returns null if the actor is not available or element not found
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<string | null> - The element's text content, or null if not found
   * @throws Error - If the browser instance is not found
   */
  public async getElementText(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:GetElementText", { selector });
  }

  /**
   * Resolves an element fingerprint to a CSS selector.
   *
   * This method enables LLMs to use fingerprints from Markdown output
   * to interact with elements. Fingerprints are stable djb2 hashes
   * of element paths in the DOM.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param fingerprint - Element fingerprint (8 or 16 lowercase alphanumeric chars)
   * @returns Promise<string | null> - CSS selector for the element, or null if not found
   * @throws Error - If the browser instance is not found
   */
  public async resolveFingerprint(
    instanceId: string,
    fingerprint: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:ResolveFingerprint", { fingerprint });
  }

  /**
   * Clears all visual effects (highlights, overlays, info panels).
   * Useful before operations to get a clean DOM state.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<boolean> - True if effects were cleared successfully
   * @throws Error - If the browser instance is not found
   */
  public async clearEffects(instanceId: string): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:ClearEffects", undefined, false) as Promise<boolean>;
  }

  /**
   * Clicks on an element in a browser instance
   *
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to click on a specific element
   * - Returns boolean indicating success or failure
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if click was successful, false otherwise
   * @throws Error - If the browser instance is not found
   */
  public async clickElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:ClickElement", { selector }, false) as Promise<boolean>;
  }

  /**
   * Waits for an element to appear in a browser instance
   *
   * This method:
   * - Validates the browser instance exists
   * - Gets the NRWebScraper actor from the current window global
   * - Sends a query to wait for a specific element to appear
   * - Returns boolean indicating if element was found within timeout
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @param timeout - Maximum time to wait in milliseconds (default: 5000)
   * @param state - The state to wait for (attached, visible, hidden, detached)
   * @returns Promise<boolean> - True if element was found, false if timeout reached
   * @throws Error - If the browser instance is not found
   */
  public async waitForElement(
    instanceId: string,
    selector: string,
    timeout = 5000,
    state: WaitForElementState = "attached",
  ): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:WaitForElement", { selector, timeout, state }, false) as Promise<boolean>;
  }

  /**
   * Waits for the document to be ready (DOMContentLoaded)
   *
   * This method waits until the document's readyState is "interactive" or "complete",
   * meaning the DOM is fully parsed and ready for manipulation.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param timeout - Maximum time to wait in milliseconds (default: 15000)
   * @returns Promise<boolean | null> - True if document is ready, false if timeout, null if error
   * @throws Error - If the browser instance is not found
   */
  public async waitForReady(
    instanceId: string,
    timeout = 15000,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:WaitForReady", { timeout });
  }

  /**
   * Takes a screenshot of the current viewport
   *
   * This method captures the visible area of the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeScreenshot(instanceId: string): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:TakeScreenshot");
  }

  /**
   * Takes a screenshot of a specific element
   *
   * This method captures a specific element on the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeElementScreenshot(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:TakeElementScreenshot", { selector });
  }

  /**
   * Takes a screenshot of the full page
   *
   * This method captures the entire scrollable area of the page and returns it
   * as a base64 encoded PNG image. It includes error handling.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeFullPageScreenshot(
    instanceId: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:TakeFullPageScreenshot");
  }

  /**
   * Takes a screenshot of a specific region of the page.
   * If properties are omitted, they default to the maximum possible size.
   *
   * This method captures a specific area of the page defined by a rectangle.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param rect The rectangular area to capture { x, y, width, height }.
   * @returns Promise<string | null> - Base64 encoded PNG image, or null on error
   * @throws Error - If the browser instance is not found
   */
  public async takeRegionScreenshot(
    instanceId: string,
    rect?: { x?: number; y?: number; width?: number; height?: number },
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:TakeRegionScreenshot", { rect });
  }

  /**
   * Fills multiple form fields based on a selector-value map.
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param formData A map where keys are CSS selectors for input fields
   * and values are the corresponding values to set.
   * @returns Promise<boolean> - True if all fields were filled successfully, false otherwise.
   * @throws Error - If the browser instance is not found
   */
  public async fillForm(
    instanceId: string,
    formData: { [selector: string]: string },
    options: { typingMode?: boolean; typingDelayMs?: number } = {},
  ): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:FillForm", {
      formData,
      typingMode: options.typingMode,
      typingDelayMs: options.typingDelayMs,
    }, false) as Promise<boolean>;
  }

  /**
   * Types or sets a value into an element.
   */
  public async inputElement(
    instanceId: string,
    selector: string,
    value: string,
    options: { typingMode?: boolean; typingDelayMs?: number } = {},
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:InputElement", {
      selector,
      value,
      typingMode: options.typingMode,
      typingDelayMs: options.typingDelayMs,
    });
  }

  /**
   * Press a key or key combination.
   */
  public async pressKey(instanceId: string, key: string): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:PressKey", { key }, false) as Promise<boolean>;
  }

  /**
   * Upload a file through input[type=file]
   */
  public async uploadFile(
    instanceId: string,
    selector: string,
    filePath: string,
  ): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:UploadFile", { selector, filePath }, false) as Promise<boolean>;
  }

  /**
   * Gets the value of an input/textarea element
   */
  public async getValue(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:GetValue", { selector });
  }

  /**
   * Gets the value of a specific attribute from an element
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @param attributeName - Name of the attribute to retrieve
   * @returns Promise<string | null> - The attribute value, or null if not found
   */
  public async getAttribute(
    instanceId: string,
    selector: string,
    attributeName: string,
  ): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:GetAttribute", { selector, attributeName });
  }

  /**
   * Checks if an element is visible in the viewport
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if visible, false otherwise
   */
  public async isVisible(
    instanceId: string,
    selector: string,
  ): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:IsVisible", { selector }, false) as Promise<boolean>;
  }

  /**
   * Checks if an element is enabled (not disabled)
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if enabled, false if disabled or not found
   */
  public async isEnabled(
    instanceId: string,
    selector: string,
  ): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:IsEnabled", { selector }, false) as Promise<boolean>;
  }

  /**
   * Clears the value of an input or textarea element
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if cleared successfully
   */
  public async clearInput(
    instanceId: string,
    selector: string,
  ): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:ClearInput", { selector }, false) as Promise<boolean>;
  }

  /**
   * Submits form associated with the selector element or the form itself
   */
  public async submit(instanceId: string, selector: string): Promise<boolean> {
    return this.withActor(instanceId, "WebScraper:Submit", { selector }, false) as Promise<boolean>;
  }

  /**
   * Selects an option in a select element
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target select element
   * @param value - The value of the option to select
   * @returns Promise<boolean> - True if selected successfully
   */
  public async selectOption(
    instanceId: string,
    selector: string,
    value: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:SelectOption", { selector, optionValue: value });
  }

  /**
   * Sets the checked state of a checkbox or radio button
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target input element
   * @param checked - The desired checked state
   * @returns Promise<boolean> - True if set successfully
   */
  public async setChecked(
    instanceId: string,
    selector: string,
    checked: boolean,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:SetChecked", { selector, checked });
  }

  /**
   * Simulates a mouse hover over an element
   *
   * @param instanceId - The unique identifier of the browser instance
   * @param selector - CSS selector to find the target element
   * @returns Promise<boolean> - True if hover was simulated successfully
   */
  public async hoverElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:HoverElement", { selector });
  }

  /**
   * Scrolls the page to make an element visible
   */
  public async scrollToElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:ScrollToElement", { selector });
  }

  /**
   * Gets the page title
   */
  public async getPageTitle(instanceId: string): Promise<string | null> {
    return this.withActor(instanceId, "WebScraper:GetPageTitle");
  }

  /**
   * Performs a double click on an element
   */
  public async doubleClick(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:DoubleClick", { selector });
  }

  /**
   * Performs a right click (context menu) on an element
   */
  public async rightClick(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:RightClick", { selector });
  }

  /**
   * Focuses on an element
   */
  public async focusElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:Focus", { selector });
  }

  /**
   * Performs a drag and drop operation between two elements
   */
  public async dragAndDrop(
    instanceId: string,
    sourceSelector: string,
    targetSelector: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:DragAndDrop", {
      selector: sourceSelector,
      targetSelector,
    });
  }

  /**
   * Gets all cookies for the current page
   */
  public getCookies(instanceId: string): Promise<
    Array<{
      name: string;
      value: string;
      domain?: string;
      path?: string;
      secure?: boolean;
      httpOnly?: boolean;
      sameSite?: string;
      expirationDate?: number;
    }>
  > {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      return Promise.reject(
        new Error(`Browser not found for instance ${instanceId}`),
      );
    }
    return Promise.resolve(CookieHelper.getCookies(browser));
  }

  /**
   * Sets a cookie for the current page
   */
  public async setCookie(
    instanceId: string,
    cookie: {
      name: string;
      value: string;
      domain?: string;
      path?: string;
      secure?: boolean;
      httpOnly?: boolean;
      sameSite?: "Strict" | "Lax" | "None";
      expirationDate?: number;
    },
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      return Promise.reject(
        new Error(`Browser not found for instance ${instanceId}`),
      );
    }

    try {
      // Strategy 1: Services.cookies.add() (parent-process)
      if (CookieHelper.setCookieDirect(browser, cookie)) {
        return true;
      }

      // Strategy 2: Actor-based fallback (content-process document.cookie)
      const uri = browser.browsingContext?.currentURI;
      if (uri) {
        try {
          const actor = await this._getActorForBrowser(instanceId, 80, 100);
          if (actor) {
            const cookieString = CookieHelper.buildCookieString(cookie, uri.host);
            const result = await actor
              .sendQuery("WebScraper:SetCookieString", {
                cookieString,
                cookieName: cookie.name,
                cookieValue: cookie.value,
              })
              .catch(() => null);
            if (result) return true;
          }
        } catch (err) {
          console.error("WebScraper: Actor cookie fallback failed:", err);
        }
      }

      return false;
    } catch (e) {
      console.error("WebScraper: Error setting cookie:", e);
      return false;
    }
  }

  /**
   * Accepts (clicks OK on) an alert dialog
   * Note: This is a simplified implementation for headless context
   */
  public acceptAlert(_instanceId: string): Promise<boolean | null> {
    // In headless mode, alerts are typically auto-dismissed
    // This is a placeholder for future implementation
    console.warn(
      "WebScraper: acceptAlert is not fully supported in headless mode",
    );
    return Promise.resolve(true);
  }

  /**
   * Dismisses (clicks Cancel on) an alert dialog
   * Note: This is a simplified implementation for headless context
   */
  public dismissAlert(_instanceId: string): Promise<boolean | null> {
    // In headless mode, alerts are typically auto-dismissed
    // This is a placeholder for future implementation
    console.warn(
      "WebScraper: dismissAlert is not fully supported in headless mode",
    );
    return Promise.resolve(true);
  }

  /**
   * Waits for network to become idle (no pending requests)
   * Delegates to shared NetworkIdleHelper using GlobalHTTPTracker polling.
   */
  public waitForNetworkIdle(
    instanceId: string,
    options:
      | number
      | {
          timeout?: number;
          maxInflight?: number;
          idleDuration?: number;
          ignorePatterns?: string[];
        } = {},
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const bcid = browser.browsingContext?.id;
    if (!bcid) {
      return Promise.resolve(null);
    }

    return NetworkIdleHelper.waitForIdle(bcid, options);
  }

  /**
   * Sets the innerHTML of an element (for contenteditable elements like rich text editors)
   */
  public async setInnerHTML(
    instanceId: string,
    selector: string,
    html: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:SetInnerHTML", { selector, innerHTML: html });
  }

  /**
   * Sets the textContent of an element (for contenteditable elements)
   */
  public async setTextContent(
    instanceId: string,
    selector: string,
    text: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:SetTextContent", { selector, textContent: text });
  }

  /**
   * Dispatches a custom event on an element (for triggering framework-specific handlers)
   */
  public async dispatchEvent(
    instanceId: string,
    selector: string,
    eventType: string,
    options?: { bubbles?: boolean; cancelable?: boolean },
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:DispatchEvent", {
      selector,
      eventType,
      eventOptions: options,
    });
  }

  /**
   * Dispatches a text input event on an element, simulating user typing.
   * This triggers the same `beforeinput`/`input` events that many frameworks
   * listen for to update their internal state (e.g., Draft.js).
   *
   * @param instanceId - The ID of the browser instance to use.
   * @param selector - CSS selector for the target element.
   * @param text - The text value to dispatch as input.
   * @returns Promise<boolean | null> - True if successful, false on error, null if instance not found.
   */
  public async dispatchTextInput(
    instanceId: string,
    selector: string,
    text: string,
  ): Promise<boolean | null> {
    return this.withActor(instanceId, "WebScraper:DispatchTextInput", { selector, text });
  }

  /**
   * Evaluates a JavaScript string in the page context.
   * Supports async/await expressions. Returns a structured result
   * with JSON-serializable values.
   */
  public async evaluate(
    instanceId: string,
    script: string,
  ): Promise<{ success: boolean; result?: unknown; resultType?: string; error?: string; errorType?: string } | null> {
    return this.withActor(
      instanceId,
      "WebScraper:Evaluate",
      { script },
    );
  }
}

// Export a singleton instance of the WebScraper service
export const WebScraper = new webScraper();
