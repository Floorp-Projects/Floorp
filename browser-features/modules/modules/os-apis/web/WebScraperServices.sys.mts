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

const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
);
const { HiddenFrame } = ChromeUtils.importESModule(
  "resource://gre/modules/HiddenFrame.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// Actor interface for WebScraper communication
interface WebScraperActor {
  sendQuery(name: string, data?: object): Promise<unknown>;
}

// Global sets to prevent garbage collection of active components
const SCRAPER_ACTOR_SETS: Set<XULBrowserElement> = new Set();
const PROGRESS_LISTENERS = new Set();
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

  private readonly _defaultActionDelay = 500;

  /**
   * Try to get NRWebScraper actor with small retries to avoid race after navigation
   */
  private async _getActorForBrowser(
    browser: XULBrowserElement,
    tries = 100,
    delayMs = 100,
  ): Promise<WebScraperActor | null> {
    let actor = browser.browsingContext?.currentWindowGlobal?.getActor(
      "NRWebScraper",
    ) as WebScraperActor | undefined;
    for (let i = 0; !actor && i < tries; i++) {
      await new Promise((r) => setTimeout(r, delayMs));
      actor = browser.browsingContext?.currentWindowGlobal?.getActor(
        "NRWebScraper",
      ) as WebScraperActor | undefined;
    }
    return actor ?? null;
  }

  /**
   * Delays execution for user interaction visibility
   */
  private async _delayForUser(delay?: number): Promise<void> {
    const ms = delay ?? this._defaultActionDelay;
    if (ms <= 0) {
      return;
    }
    await new Promise<void>((resolve) => setTimeout(resolve, ms));
  }

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

    // Set default size for browser element
    browser.style.setProperty("width", "1366px");
    browser.style.setProperty("min-width", "1366px");
    browser.style.setProperty("height", "768px");
    browser.style.setProperty("min-height", "768px");

    // Add browser element to document to properly initialize webNavigation
    doc.documentElement?.appendChild(browser);
    browser.browsingContext.allowJavascript = true;

    this._instanceId = await crypto.randomUUID();
    this._browserInstances.set(this._instanceId, browser);
    SCRAPER_ACTOR_SETS.add(browser);
    await this._delayForUser();
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
  public navigate(instanceId: string, url: string): Promise<void> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const principal = Services.scriptSecurityManager.getSystemPrincipal();
    return new Promise((resolve) => {
      let resolved = false;
      const complete = async () => {
        if (resolved) return;
        resolved = true;
        try {
          const actor = await this._getActorForBrowser(browser, 150, 100); // up to ~15s
          if (actor) {
            // Prefer explicit readiness wait in the child actor
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
        } catch (_) {
          // ignore
        }
        await this._delayForUser();
        resolve();
      };
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

      const { webProgress } = browser;

      // Delay before navigation
      this._delayForUser().then(() => {
        if (typeof browser.loadURI === "function") {
          browser.loadURI(uri, loadURIOptions);
        } else {
          throw new Error("browser.loadURI is not defined");
        }
      });

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
          if (flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT) {
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
          const STATE_STOP = Ci.nsIWebProgressListener.STATE_STOP;
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
        Ci.nsIWebProgress.NOTIFY_LOCATION |
          Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT,
      );
    });
  }

  public getURI(instanceId: string): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    return new Promise((resolve) => {
      if (browser.browsingContext.currentURI.spec != "about:blank") {
        resolve(browser.browsingContext.currentURI.spec);
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
  public async getHTML(instanceId: string): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    try {
      const actor = await this._getActorForBrowser(browser);
      if (!actor) return null;
      return (await actor.sendQuery("WebScraper:GetHTML")) as string | null;
    } catch (e) {
      console.error("Error getting HTML:", e);
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
  public async getElement(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    return (await actor.sendQuery("WebScraper:GetElementText", {
      selector,
    })) as string | null;
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:ClickElement", {
      selector,
    })) as boolean;
    await this._delayForUser(3500);
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
   * @returns Promise<boolean> - True if element was found, false if timeout reached
   * @throws Error - If the browser instance is not found
   */
  public async waitForElement(
    instanceId: string,
    selector: string,
    timeout = 5000,
  ): Promise<boolean> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return false;
    return (await actor.sendQuery("WebScraper:WaitForElement", {
      selector,
      timeout,
    })) as boolean;
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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
  ): Promise<boolean> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:FillForm", {
      formData,
    })) as boolean;
    await this._delayForUser(3500);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:ClearInput", {
      selector,
    })) as boolean;
    await this._delayForUser(3500);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return false;
    const result = (await actor.sendQuery("WebScraper:Submit", {
      selector,
    })) as boolean;
    await this._delayForUser(3500);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:SelectOption", {
      selector,
      optionValue: value,
    })) as boolean;
    await this._delayForUser(3500);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:SetChecked", {
      selector,
      checked,
    })) as boolean;
    await this._delayForUser(3500);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:HoverElement", {
      selector,
    })) as boolean;
    await this._delayForUser(2000);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:ScrollToElement", {
      selector,
    })) as boolean;
    await this._delayForUser(1000);
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

    const actor = await this._getActorForBrowser(browser);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:DoubleClick", {
      selector,
    })) as boolean;
    await this._delayForUser(3500);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:RightClick", {
      selector,
    })) as boolean;
    await this._delayForUser(3500);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:Focus", {
      selector,
    })) as boolean;
    await this._delayForUser(1000);
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

    const actor = await this._getActorForBrowser(browser);
    if (!actor) return null;
    const result = (await actor.sendQuery("WebScraper:DragAndDrop", {
      selector: sourceSelector,
      targetSelector,
    })) as boolean;
    await this._delayForUser(3500);
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
      const enumerator = cookieManager.getCookiesFromHost(host, {});

      for (const cookie of enumerator) {
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

      return Promise.resolve(cookies);
    } catch (e) {
      console.error("WebScraper: Error getting cookies:", e);
      return Promise.resolve([]);
    }
  }

  /**
   * Sets a cookie for the current page
   */
  public setCookie(
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

      Services.cookies.add(
        cookie.domain ?? uri.host,
        cookie.path ?? "/",
        cookie.name,
        cookie.value,
        cookie.secure ?? false,
        cookie.httpOnly ?? false,
        false, // isSession
        cookie.expirationDate ?? Math.floor(Date.now() / 1000) + 86400 * 365,
        {}, // originAttributes
        sameSiteMap[cookie.sameSite ?? "None"] ?? 0,
        Ci.nsICookie.SCHEME_HTTPS,
      );

      return Promise.resolve(true);
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
   * Saves the current page as PDF and returns base64 encoded data
   */
  public async saveAsPDF(instanceId: string): Promise<string | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    try {
      const browsingContext = browser.browsingContext as BrowsingContext & {
        print(settings: nsIPrintSettings): Promise<nsIInputStream>;
      };
      if (!browsingContext) return null;

      // Create print settings for PDF
      const printSettings = Cc["@mozilla.org/gfx/printsettings-service;1"]
        .getService(Ci.nsIPrintSettingsService)
        .createNewPrintSettings() as nsIPrintSettings & {
        showPrintProgress: boolean;
      };

      printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF;
      printSettings.printerName = "";
      printSettings.printSilent = true;
      printSettings.showPrintProgress = false;
      printSettings.printBGColors = true;
      printSettings.printBGImages = true;

      // Print to stream
      const pdfStream = await browsingContext.print(printSettings);

      // Read stream to array buffer
      const binaryStream = Cc[
        "@mozilla.org/binaryinputstream;1"
      ].createInstance(Ci.nsIBinaryInputStream);
      binaryStream.setInputStream(pdfStream);

      const bytes = binaryStream.readBytes(binaryStream.available());
      binaryStream.close();

      // Convert to base64
      const base64 = btoa(bytes);
      return base64;
    } catch (e) {
      console.error("WebScraper: Error saving as PDF:", e);
      return null;
    }
  }

  /**
   * Waits for network to become idle (no pending requests)
   */
  public waitForNetworkIdle(
    instanceId: string,
    timeout: number = 5000,
  ): Promise<boolean | null> {
    const browser = this._browserInstances.get(instanceId);
    if (!browser) {
      throw new Error(`Browser not found for instance ${instanceId}`);
    }

    return new Promise((resolve) => {
      let idleTimer: ReturnType<typeof setTimeout> | null = null;
      let resolved = false;
      const idleThreshold = 500; // Consider idle after 500ms of no activity

      const resetIdleTimer = () => {
        if (idleTimer) {
          clearTimeout(idleTimer);
        }
        idleTimer = setTimeout(() => {
          if (!resolved) {
            resolved = true;
            cleanup();
            resolve(true);
          }
        }, idleThreshold);
      };

      const cleanup = () => {
        if (idleTimer) {
          clearTimeout(idleTimer);
        }
        try {
          browser.webProgress?.removeProgressListener(progressListener);
          PROGRESS_LISTENERS.delete(progressListener);
        } catch {
          // Ignore cleanup errors
        }
      };

      const progressListener: nsIWebProgressListener = {
        onStateChange(
          _webProgress: nsIWebProgress,
          _request: nsIRequest,
          stateFlags: number,
        ) {
          if (stateFlags & Ci.nsIWebProgressListener.STATE_START) {
            // Network activity started
            if (idleTimer) {
              clearTimeout(idleTimer);
              idleTimer = null;
            }
          } else if (stateFlags & Ci.nsIWebProgressListener.STATE_STOP) {
            // Network activity stopped, start idle timer
            resetIdleTimer();
          }
        },
        onProgressChange() {},
        onLocationChange() {},
        onStatusChange() {},
        onSecurityChange() {},
        onContentBlockingEvent() {},
        QueryInterface: ChromeUtils.generateQI([Ci.nsIWebProgressListener]),
      };

      try {
        browser.webProgress?.addProgressListener(
          progressListener,
          Ci.nsIWebProgress.NOTIFY_STATE_ALL,
        );
        PROGRESS_LISTENERS.add(progressListener);
      } catch (e) {
        console.error("WebScraper: Error adding progress listener:", e);
        resolve(false);
        return;
      }

      // Start initial idle timer
      resetIdleTimer();

      // Timeout handler
      setTimeout(() => {
        if (!resolved) {
          resolved = true;
          cleanup();
          resolve(false);
        }
      }, timeout);
    });
  }
}

// Export a singleton instance of the WebScraper service
export const WebScraper = new webScraper();
