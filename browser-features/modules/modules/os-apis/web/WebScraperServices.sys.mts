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
   * Common helper: get browser ↁEget actor ↁEsendQuery.
   * Reduces boilerplate across all service methods.
   */
  private async withActor<T>(
    instanceId: string,
    queryName: string,
    data?: object,
    fallback: T | null = null,
  ): Promise<T | null> {
    try {
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

    // Wait for page load via progress listener
    await new Promise<void>((resolve) => {
      let resolved = false;
      const complete = () => {
        if (resolved) return;
        resolved = true;
        resolve();
      };

      const { webProgress } = browser;

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
    try {
      const actor = await this._getActorForBrowser(instanceId);
      if (!actor) return null;
      return (await actor.sendQuery(
        "WebScraper:GetHTML",
        options ?? {},
      )) as string | null;
    } catch (e) {
      console.error("Error getting HTML:", e);
      return null;
    }
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
    // Normalize: boolean ↁEold-style { includeSelectorMap }
    const data =
      typeof options === "boolean" ? { includeSelectorMap: options } : options;

    try {
      const actor = await this._getActorForBrowser(instanceId);
      if (!actor) return null;
      return (await actor.sendQuery("WebScraper:GetText", data)) as
        | string
        | null;
    } catch (e) {
      console.error("Error getting text:", e);
      return null;
    }
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
    try {
      const actor = await this._getActorForBrowser(instanceId);
      if (!actor) return null;
      return await actor.sendQuery("WebScraper:GetAccessibilityTree", options);
    } catch (e) {
      console.error("Error getting accessibility tree:", e);
      return null;
    }
  }

  public async getArticle(
    instanceId: string,
  ): Promise<{
    title: string;
    byline: string;
    markdown: string;
    length: number;
  } | null> {
    try {
      const actor = await this._getActorForBrowser(instanceId);
      if (!actor) return null;
      return (await actor.sendQuery("WebScraper:GetArticle")) as {
        title: string;
        byline: string;
        markdown: string;
        length: number;
      } | null;
    } catch (e) {
      console.error("Error getting article:", e);
      return null;
    }
  }

  public async getElement(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:GetElement", {
      selector,
    })) as string | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return [];
    return ((await actor.sendQuery("WebScraper:GetElements", {
      selector,
    })) ?? []) as string[];
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:GetElementByText", {
      textContent,
    })) as string | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:GetElementTextContent", {
      selector,
    })) as string | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:GetElementText", {
      selector,
    })) as string | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:ResolveFingerprint", {
      fingerprint,
    })) as string | null;
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
    try {
      const browser = this._browserInstances.get(instanceId);
      if (!browser) {
        throw new Error(`Browser not found for instance ${instanceId}`);
      }

      const actor = await this._getActorForBrowser(instanceId);
      if (!actor) return false;
      await actor.sendQuery("WebScraper:ClearEffects");
      return true;
    } catch {
      return false;
    }
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:ClickElement", {
      selector,
    })) as boolean;
    return result;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    return (await actor.sendQuery("WebScraper:WaitForElement", {
      selector,
      timeout,
      state,
    })) as boolean;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:WaitForReady", {
      timeout,
    })) as boolean | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:TakeScreenshot")) as
      | string
      | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:TakeElementScreenshot", {
      selector,
    })) as string | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:TakeFullPageScreenshot")) as
      | string
      | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:TakeRegionScreenshot", {
      rect,
    })) as string | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:FillForm", {
      formData,
      typingMode: options.typingMode,
      typingDelayMs: options.typingDelayMs,
    })) as boolean;
    return result;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:InputElement", {
      selector,
      value,
      typingMode: options.typingMode,
      typingDelayMs: options.typingDelayMs,
    })) as boolean;
    return result;
  }

  /**
   * Press a key or key combination.
   */
  public async pressKey(instanceId: string, key: string): Promise<boolean> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:PressKey", {
      key,
    })) as boolean;
    return result;
  }

  /**
   * Upload a file through input[type=file]
   */
  public async uploadFile(
    instanceId: string,
    selector: string,
    filePath: string,
  ): Promise<boolean> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:UploadFile", {
      selector,
      filePath,
    })) as boolean;
    return result;
  }

  /**
   * Gets the value of an input/textarea element
   */
  public async getValue(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:GetValue", {
      selector,
    })) as string | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:GetAttribute", {
      selector,
      attributeName,
    })) as string | null;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    return (await actor.sendQuery("WebScraper:IsVisible", {
      selector,
    })) as boolean;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    return (await actor.sendQuery("WebScraper:IsEnabled", {
      selector,
    })) as boolean;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:ClearInput", {
      selector,
    })) as boolean;
    return result;
  }

  /**
   * Submits form associated with the selector element or the form itself
   */
  public async submit(instanceId: string, selector: string): Promise<boolean> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:Submit", {
      selector,
    })) as boolean;
    return result;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:SelectOption", {
      selector,
      optionValue: value,
    })) as boolean;
    return result;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:SetChecked", {
      selector,
      checked,
    })) as boolean;
    return result;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:HoverElement", {
      selector,
    })) as boolean;
    return result;
  }

  /**
   * Scrolls the page to make an element visible
   */
  public async scrollToElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:ScrollToElement", {
      selector,
    })) as boolean;
    return result;
  }

  /**
   * Gets the page title
   */
  public async getPageTitle(instanceId: string): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:GetPageTitle")) as string | null;
  }

  /**
   * Performs a double click on an element
   */
  public async doubleClick(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:DoubleClick", {
      selector,
    })) as boolean;
    return result;
  }

  /**
   * Performs a right click (context menu) on an element
   */
  public async rightClick(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:RightClick", {
      selector,
    })) as boolean;
    return result;
  }

  /**
   * Focuses on an element
   */
  public async focusElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:Focus", {
      selector,
    })) as boolean;
    return result;
  }

  /**
   * Performs a drag and drop operation between two elements
   */
  public async dragAndDrop(
    instanceId: string,
    sourceSelector: string,
    targetSelector: string,
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:DragAndDrop", {
      selector: sourceSelector,
      targetSelector,
    })) as boolean;
    return result;
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

    try {
      const uri = browser.browsingContext?.currentURI;
      if (!uri) return Promise.resolve([]);

      const host = uri.host;
      const principal =
        browser.browsingContext?.currentWindowGlobal?.documentPrincipal;
      const originAttributes = principal?.originAttributes ?? {};
      const cookies: Array<{
        name: string;
        value: string;
        domain?: string;
        path?: string;
        secure?: boolean;
        httpOnly?: boolean;
        sameSite?: string;
        expirationDate?: number;
      }> = [];

      const cookieManager = Services.cookies;
      const originAttrCandidates = [originAttributes];
      // Always try empty originAttributes as fallback to find cookies
      // set without specific container/private browsing attributes
      originAttrCandidates.push({});

      const seen = new Set<string>();

      for (const attrs of originAttrCandidates) {
        const enumerator = cookieManager.getCookiesFromHost(host, attrs);

        for (const cookie of enumerator) {
          const key = `${cookie.name}\u0001${cookie.host}\u0001${cookie.path}`;
          if (seen.has(key)) continue;
          seen.add(key);

          cookies.push({
            name: cookie.name,
            value: cookie.value,
            domain: cookie.host,
            path: cookie.path,
            secure: cookie.isSecure,
            httpOnly: cookie.isHttpOnly,
            sameSite: ["None", "Lax", "Strict"][cookie.sameSite] ?? "None",
            expirationDate: cookie.expiry,
          });
        }
      }

      return Promise.resolve(cookies);
    } catch (e) {
      console.error("WebScraper: Error getting cookies:", e);
      return Promise.resolve([]);
    }
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
      const uri = browser.browsingContext?.currentURI;
      if (!uri) return Promise.resolve(false);

      const sameSiteMap: Record<string, number> = {
        None: 0,
        Lax: 1,
        Strict: 2,
      };

      const principal =
        browser.browsingContext?.currentWindowGlobal?.documentPrincipal;
      const originAttributes = principal?.originAttributes ?? {};

      const isSecure = cookie.secure ?? uri.schemeIs("https");
      let requestedSameSite = cookie.sameSite ?? "Lax";
      if (requestedSameSite === "None" && !isSecure) {
        // SameSite=None には Secure が忁E��、ETTP では Lax にフォールバックする、E
        requestedSameSite = "Lax";
      }
      const schemeMap = isSecure
        ? Ci.nsICookie.SCHEME_HTTPS
        : uri.schemeIs("http")
          ? Ci.nsICookie.SCHEME_HTTP
          : 0; // SCHEME_UNKNOWN

      const attrsList = [originAttributes];
      if (Object.keys(originAttributes).length > 0) {
        attrsList.push({});
      }

      const errors: Array<string | number> = [];

      const addWithAttrs = (attrs: Record<string, unknown>) => {
        try {
          const validation = Services.cookies.add(
            cookie.domain ?? uri.host,
            cookie.path ?? "/",
            cookie.name,
            cookie.value,
            isSecure,
            cookie.httpOnly ?? false,
            false, // isSession
            cookie.expirationDate ??
              Math.floor(Date.now() / 1000) + 86400 * 365,
            attrs,
            sameSiteMap[requestedSameSite] ?? sameSiteMap.Lax,
            schemeMap,
            Boolean(
              (attrs as unknown as { partitionKey?: unknown }).partitionKey,
            ),
          );

          if (validation?.result !== Ci.nsICookieValidation.eOK) {
            errors.push(validation?.result ?? "unknown");
            errors.push(validation?.errorString ?? "");
            console.error(
              "WebScraper: Cookie rejected:",
              validation?.result,
              validation?.errorString,
            );
            return false;
          }

          // Verify cookie was actually stored
          const storedDomain = cookie.domain ?? uri.host;

          // Check if cookie actually exists after add
          let cookieActuallyExists = false;
          try {
            cookieActuallyExists = Services.cookies.cookieExists(
              storedDomain,
              cookie.path ?? "/",
              cookie.name,
              attrs,
            );
          } catch {
            // cookieExists check failed, assume cookie not stored
          }

          if (cookieActuallyExists) {
            return true;
          } else {
            // Cookie was not actually stored despite validation passing
            // Try next attrs or fallback
            errors.push("cookie not stored despite validation success");
            return false;
          }
        } catch (err) {
          errors.push(String(err));
          return false;
        }
      };

      for (const attrs of attrsList) {
        if (addWithAttrs(attrs)) {
          return Promise.resolve(true);
        }
      }

      // Fallback: try document.cookie within the content process
      try {
        const actor = await this._getActorForBrowser(instanceId, 80, 100);
        if (actor) {
          const parts = [
            `${cookie.name}=${cookie.value}`,
            `Path=${cookie.path ?? "/"}`,
            `SameSite=${requestedSameSite}`,
          ];
          if (cookie.domain ?? uri.host) {
            parts.push(`Domain=${cookie.domain ?? uri.host}`);
          }
          if (cookie.expirationDate) {
            parts.push(
              `Expires=${new Date(cookie.expirationDate * 1000).toUTCString()}`,
            );
          }
          if (isSecure) {
            parts.push("Secure");
          }

          const cookieString = parts.filter(Boolean).join("; ");

          const fallback = await actor
            .sendQuery("WebScraper:SetCookieString", {
              cookieString: cookieString,
              cookieName: cookie.name,
              cookieValue: cookie.value,
            })
            .catch(() => null);
          if (fallback) {
            return true;
          }
        }
      } catch (err) {
        errors.push(String(err));
      }

      return Promise.resolve(false);
    } catch (e) {
      console.error("WebScraper: Error setting cookie:", e);
      return Promise.resolve(false);
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
   * Uses GlobalHTTPTracker for network state tracking (no duplicate observers)
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
    // Backward compatibility: number ↁE{ timeout }
    const opts =
      typeof options === "number" ? { timeout: options } : options;
    const {
      timeout = AutomationConstants.NETWORK_IDLE_TIMEOUT_MS,
      maxInflight = 0,
      idleDuration = AutomationConstants.NETWORK_IDLE_THRESHOLD_MS,
      ignorePatterns = [],
    } = opts;

    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const bcid = browser.browsingContext?.id;
    if (!bcid) {
      return Promise.resolve(null);
    }

    // Pre-compile ignore patterns
    const ignoreRegexps = ignorePatterns.map((p) => new RegExp(p));

    return new Promise((resolve) => {
      const state = {
        pollTimer: null as ReturnType<typeof setTimeout> | null,
        resolved: false,
        startTime: Date.now(),
        idleSince: 0,
      };

      const cleanup = () => {
        if (state.pollTimer) {
          clearTimeout(state.pollTimer);
          state.pollTimer = null;
        }
      };

      const poll = () => {
        if (state.resolved) return;

        let activeCount = GlobalHTTPTracker.getActiveCount(bcid);

        // Subtract requests matching ignore patterns
        if (ignoreRegexps.length > 0 && activeCount > 0) {
          const activeUrls = GlobalHTTPTracker.getActiveURLs?.(bcid) ?? [];
          const ignored = activeUrls.filter((url: string) =>
            ignoreRegexps.some((re) => re.test(url)),
          );
          activeCount = Math.max(0, activeCount - ignored.length);
        }

        const elapsed = Date.now() - state.startTime;

        if (activeCount <= maxInflight) {
          if (state.idleSince === 0) {
            state.idleSince = Date.now();
          }
          if (Date.now() - state.idleSince >= idleDuration) {
            console.log(
              `[WebScraper] Network idle for BCID ${bcid} after ${elapsed}ms`,
            );
            state.resolved = true;
            cleanup();
            resolve(true);
            return;
          }
        } else {
          state.idleSince = 0;
        }

        if (elapsed >= timeout) {
          console.log(
            `[WebScraper] waitForNetworkIdle timed out. Active: ${activeCount}`,
          );
          state.resolved = true;
          cleanup();
          resolve(false);
        } else {
          state.pollTimer = setTimeout(poll, 100);
        }
      };

      poll();
    });
  }

  /**
   * Sets the innerHTML of an element (for contenteditable elements like rich text editors)
   */
  public async setInnerHTML(
    instanceId: string,
    selector: string,
    html: string,
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:SetInnerHTML", {
      selector,
      innerHTML: html,
    })) as boolean;
    return result;
  }

  /**
   * Sets the textContent of an element (for contenteditable elements)
   */
  public async setTextContent(
    instanceId: string,
    selector: string,
    text: string,
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:SetTextContent", {
      selector,
      textContent: text,
    })) as boolean;
    return result;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:DispatchEvent", {
      selector,
      eventType,
      eventOptions: options,
    })) as boolean;
    return result;
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
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(instanceId);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:DispatchTextInput", {
      selector,
      text,
    })) as boolean;
    return result;
  }
}

// Export a singleton instance of the WebScraper service
export const WebScraper = new webScraper();
