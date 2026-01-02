/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TabManagerService - A service for managing and interacting with browser tabs.
 */

const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
);
const { setTimeout, clearTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs",
);

// Actor interface for WebScraper communication
interface WebScraperActor {
  sendQuery(name: string, data?: object): Promise<unknown>;
}

// Type for browser tab element
interface BrowserTab {
  linkedBrowser: XULBrowserElement & { browserId: number };
  label: string;
  selected: boolean;
  pinned: boolean;
  setAttribute?(name: string, value: string): void;
  getAttribute?(name: string): string | null;
  removeAttribute?(name: string): void;
}

// Type for gBrowser
interface GBrowser {
  tabs: BrowserTab[];
  selectedTab: BrowserTab;
  addTab(
    url: string,
    options: {
      triggeringPrincipal: nsIPrincipal;
      skipAnimation?: boolean;
      inBackground?: boolean;
    },
  ): BrowserTab;
  getIcon(tab: BrowserTab): string | null;
}

// Response types
interface TabListEntry {
  browserId: string;
  instanceId?: string;
  title: string;
  url: string;
  selected: boolean;
  pinned: boolean;
}

interface TabInstanceInfo {
  browserId: string;
  url: string;
  title: string;
  favIconUrl: string | null;
  isActive: boolean;
  isPinned: boolean;
  isLoading: boolean;
  html: string | null;
  screenshot: string | null;
}

// Global sets to prevent garbage collection of active components
const TAB_MANAGER_ACTOR_SETS: Set<XULBrowserElement> = new Set();
const PROGRESS_LISTENERS = new Set();

/**
 * A utility function to get the most recent browser window.
 * @returns The browser window.
 */
function getBrowserWindow() {
  const window = Services.wm.getMostRecentWindow("navigator:browser");
  if (!window) {
    throw new Error("Could not get browser window.");
  }
  return window;
}

/**
 * Returns all currently open browser windows (best-effort).
 */
function getBrowserWindows(): Array<Window & { gBrowser: GBrowser }> {
  const windows: Array<Window & { gBrowser: GBrowser }> = [];
  try {
    const enumerator = Services.wm.getEnumerator(
      "navigator:browser",
    ) as nsISimpleEnumerator;
    while (enumerator?.hasMoreElements?.()) {
      const win = enumerator.getNext() as Window & { gBrowser: GBrowser };
      if (win && !win.closed) {
        windows.push(win);
      }
    }
  } catch {
    // ignore
  }
  if (windows.length === 0) {
    try {
      windows.push(getBrowserWindow() as Window & { gBrowser: GBrowser });
    } catch {
      // ignore
    }
  }
  return windows;
}

/**
 * TabManager class that manages browser tabs for automation and interaction.
 */
class TabManager {
  // Map to store tab instances with their unique IDs
  private _browserInstances: Map<
    string,
    { tab: BrowserTab; browser: XULBrowserElement }
  > = new Map();

  private readonly _defaultActionDelay = 500;

  constructor() {}

  // --- Private Helper Methods ---

  /**
   * Retrieves an instance entry by its ID, handling fallback to browserId.
   */
  private _getEntry(instanceId: string) {
    let entry = this._browserInstances.get(instanceId);

    // Fallback: try to recover by scanning tabs with stored instanceId attribute.
    if (!entry) {
      try {
        let targetTab: BrowserTab | undefined;
        for (const win of getBrowserWindows()) {
          targetTab = win.gBrowser.tabs.find((tab: BrowserTab) => {
            const storedId = tab.getAttribute?.("data-floorp-os-instance-id");
            return storedId === instanceId;
          });
          if (targetTab) break;
        }

        if (targetTab) {
          entry = { tab: targetTab, browser: targetTab.linkedBrowser };
          this._browserInstances.set(instanceId, entry);
          TAB_MANAGER_ACTOR_SETS.add(entry.browser);
          if (targetTab.setAttribute) {
            targetTab.setAttribute("data-floorp-os-automated", "true");
          }
        }
      } catch {
        // ignore errors from getBrowserWindow or tab search
      }
    }

    // Fallback: If not found in instances, check if it's a browserId of an existing tab
    if (!entry) {
      try {
        let targetTab: BrowserTab | undefined;
        for (const win of getBrowserWindows()) {
          targetTab = win.gBrowser.tabs.find(
            (tab: BrowserTab) =>
              tab.linkedBrowser.browserId.toString() === instanceId,
          );
          if (targetTab) break;
        }
        if (targetTab) {
          entry = { tab: targetTab, browser: targetTab.linkedBrowser };
          // Auto-register to allow subsequent calls to use this ID
          this._browserInstances.set(instanceId, entry);
          TAB_MANAGER_ACTOR_SETS.add(entry.browser);
          if (targetTab.setAttribute) {
            targetTab.setAttribute("data-floorp-os-automated", "true");
          }
        }
      } catch {
        // ignore errors from getBrowserWindow or tab search
      }
    }

    // Last resort recovery: if there is exactly one automated tab, bind this instanceId to it.
    // This helps when the service is reloaded and the map was lost, but the client still holds the ID.
    if (!entry) {
      try {
        const automatedTabs: BrowserTab[] = [];
        for (const win of getBrowserWindows()) {
          for (const tab of win.gBrowser.tabs) {
            const flag = tab.getAttribute?.("data-floorp-os-automated");
            if (flag === "true") {
              automatedTabs.push(tab);
            }
          }
        }
        if (automatedTabs.length === 1) {
          const targetTab = automatedTabs[0];
          entry = { tab: targetTab, browser: targetTab.linkedBrowser };
          this._browserInstances.set(instanceId, entry);
          TAB_MANAGER_ACTOR_SETS.add(entry.browser);
          if (targetTab.setAttribute) {
            targetTab.setAttribute("data-floorp-os-instance-id", instanceId);
          }
        }
      } catch {
        // ignore
      }
    }

    if (entry) {
      // Verify tab is still alive and in a window
      const browser = entry.tab.linkedBrowser;
      if (!browser || !browser.ownerGlobal || browser.ownerGlobal.closed) {
        this._browserInstances.delete(instanceId);
        TAB_MANAGER_ACTOR_SETS.delete(entry.browser);
        return null;
      }
      // Always use the latest linkedBrowser in case it changed (e.g. remoteness change)
      entry.browser = browser;
    }

    return entry;
  }

  /**
   * Retrieves an instance by its ID, throwing an error if not found.
   */
  private _getInstance(instanceId: string) {
    const entry = this._getEntry(instanceId);

    if (!entry) {
      throw new Error(`Instance not found for ID: ${instanceId}`);
    }

    return {
      tab: entry.tab,
      browser: entry.tab.linkedBrowser as XULBrowserElement & {
        browserId: number;
      },
    };
  }

  private async _delayForUser(delay?: number): Promise<void> {
    const ms = delay ?? this._defaultActionDelay;
    if (ms <= 0) {
      return;
    }
    await new Promise<void>((resolve) => setTimeout(resolve, ms));
  }

  private _focusInstance(instanceId: string): void {
    try {
      const entry = this._getEntry(instanceId);
      if (!entry) {
        console.warn(`TabManager: Instance not found for ID: ${instanceId}`);
        return;
      }

      const win =
        (entry.browser.ownerGlobal as
          | (Window & { gBrowser: GBrowser })
          | null) ?? (getBrowserWindow() as Window & { gBrowser: GBrowser });
      if (win.closed) {
        return;
      }

      if (Services?.ww?.activeWindow !== win) {
        try {
          win.focus();
        } catch {
          // ignore focus errors
        }
      }

      if (win.gBrowser.selectedTab !== entry.tab) {
        try {
          win.gBrowser.selectedTab = entry.tab;
        } catch {
          // ignore tab selection errors
        }
      }

      try {
        entry.browser?.focus();
      } catch {
        // ignore browser focus failures
      }
    } catch (error) {
      console.error("TabManager: Failed to focus instance", error);
    }
  }

  /**
   * Waits for a tab to finish loading a specific URL.
   */
  private _waitForLoad(browser: XULBrowserElement, url: string): Promise<void> {
    return new Promise((resolve) => {
      const uri = Services.io.newURI(url);
      let resolved = false;
      const complete = async () => {
        if (resolved) return;
        resolved = true;
        // After load, ensure content actor is ready and body exists
        try {
          let actor = browser.browsingContext?.currentWindowGlobal?.getActor(
            "NRWebScraper",
          ) as WebScraperActor | undefined;
          if (!actor) {
            for (let i = 0; i < 150; i++) {
              await new Promise((r) => setTimeout(r, 100));
              actor = browser.browsingContext?.currentWindowGlobal?.getActor(
                "NRWebScraper",
              ) as WebScraperActor | undefined;
              if (actor) break;
            }
          }
          if (actor) {
            // Prefer readiness wait; fallback to element waits for heavy SPAs (X.com等)
            let ok = await actor
              .sendQuery("WebScraper:WaitForReady", { timeout: 20000 })
              .catch(() => false);
            const tryWait = async (sel: string, to = 20000) =>
              await actor!.sendQuery("WebScraper:WaitForElement", {
                selector: sel,
                timeout: to,
              });
            if (!ok)
              ok = (await tryWait("body", 20000).catch(() => false)) as boolean;
            if (!ok)
              ok = (await tryWait("html", 8000).catch(() => false)) as boolean;
            if (!ok) await tryWait("main", 8000).catch(() => false);
          }
        } catch {
          // ignore
        }
        resolve();
      };

      const progressListener = {
        onLocationChange(
          progress: nsIWebProgress,
          _request: nsIRequest,
          location: nsIURI,
          flags: number,
        ) {
          if (
            !progress.isTopLevel ||
            flags &
              (Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT ?? 0) ||
            (location.spec === "about:blank" && uri.spec !== "about:blank")
          ) {
            return;
          }

          PROGRESS_LISTENERS.delete(progressListener);
          browser.webProgress.removeProgressListener(
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
            browser.webProgress.removeProgressListener(
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
      browser.webProgress.addProgressListener(
        progressListener as nsIWebProgressListener,
        (Ci.nsIWebProgress.NOTIFY_LOCATION ?? 0) |
          (Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT ?? 0),
      );
    });
  }

  /**
   * Sends a query to the NRWebScraper content actor and returns the result.
   */
  private async _queryActor<T>(
    instanceId: string,
    query: string,
    data?: object,
  ): Promise<T | null> {
    try {
      const { browser } = this._getInstance(instanceId);
      let actor = browser.browsingContext?.currentWindowGlobal?.getActor(
        "NRWebScraper",
      ) as WebScraperActor | undefined;

      // Retry a few times after navigation for actor readiness
      if (!actor) {
        for (let i = 0; i < 150; i++) {
          await new Promise((r) => setTimeout(r, 100));
          actor = browser.browsingContext?.currentWindowGlobal?.getActor(
            "NRWebScraper",
          ) as WebScraperActor | undefined;
          if (actor) break;
        }
      }

      if (!actor) {
        console.warn(`NRWebScraper actor not found for instance ${instanceId}`);
        return null;
      }
      return (await actor.sendQuery(query, data)) as T;
    } catch (e) {
      console.error(`Error in actor query '${query}':`, e);
      return null;
    }
  }

  // --- Public API Methods ---

  public async createInstance(
    url: string,
    options?: { inBackground?: boolean },
  ): Promise<string> {
    const win = getBrowserWindow() as Window & { gBrowser: GBrowser };
    const gBrowser = win.gBrowser;

    const tab = await gBrowser.addTab(url, {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
      skipAnimation: true,
      inBackground: options?.inBackground ?? false,
    });

    const browser = tab.linkedBrowser;
    await this._waitForLoad(browser, url);
    await this._delayForUser();

    const instanceId = crypto.randomUUID();
    this._browserInstances.set(instanceId, { tab, browser });
    TAB_MANAGER_ACTOR_SETS.add(browser);

    // Mark tab as automated
    if (tab.setAttribute) {
      tab.setAttribute("data-floorp-os-automated", "true");
      tab.setAttribute("data-floorp-os-instance-id", instanceId);
    }

    return instanceId;
  }

  public async attachToTab(browserId: string): Promise<string | null> {
    const win = getBrowserWindow() as Window & { gBrowser: GBrowser };
    const gBrowser = win.gBrowser;

    const targetTab = gBrowser.tabs.find(
      (tab: BrowserTab) => tab.linkedBrowser.browserId.toString() === browserId,
    );

    if (!targetTab) {
      return null;
    }

    const browser = targetTab.linkedBrowser;
    const instanceId = crypto.randomUUID();
    this._browserInstances.set(instanceId, { tab: targetTab, browser });
    TAB_MANAGER_ACTOR_SETS.add(browser);

    // Mark tab as automated
    if (targetTab.setAttribute) {
      targetTab.setAttribute("data-floorp-os-automated", "true");
      targetTab.setAttribute("data-floorp-os-instance-id", instanceId);
    }

    await this._delayForUser();

    return instanceId;
  }

  public listTabs(): Promise<TabListEntry[]> {
    const win = getBrowserWindow() as Window & { gBrowser: GBrowser };
    const gBrowser = win.gBrowser;

    return Promise.resolve(
      gBrowser.tabs.map((tab: BrowserTab) => {
        const browserId = tab.linkedBrowser.browserId.toString();
        // Find if this tab is already an instance
        let instanceId: string | undefined;
        for (const [id, entry] of this._browserInstances.entries()) {
          if (entry.tab === tab) {
            instanceId = id;
            break;
          }
        }

        return {
          browserId,
          instanceId,
          title: tab.label,
          url: tab.linkedBrowser.currentURI.spec,
          selected: tab.selected,
          pinned: tab.pinned,
        };
      }),
    );
  }

  public async getInstanceInfo(
    instanceId: string,
  ): Promise<TabInstanceInfo | null> {
    const { tab, browser } = this._getInstance(instanceId);
    const win = getBrowserWindow() as Window & { gBrowser: GBrowser };
    const gBrowser = win.gBrowser;

    const [html, screenshot] = await Promise.all([
      this.getHTML(instanceId),
      this.takeScreenshot(instanceId),
    ]);

    return {
      browserId: (
        browser as XULBrowserElement & { browserId: number }
      ).browserId.toString(),
      url: browser.currentURI.spec,
      title: (browser as XULBrowserElement & { contentTitle: string })
        .contentTitle,
      favIconUrl: gBrowser.getIcon(tab),
      isActive: tab.selected,
      isPinned: tab.pinned,
      isLoading: browser.webProgress.isLoadingDocument,
      html,
      screenshot,
    };
  }

  public async destroyInstance(instanceId: string): Promise<void> {
    const { tab, browser } = this._getInstance(instanceId);
    try {
      await this._queryActor(instanceId, "WebScraper:ClearEffects");
    } catch (error) {
      void error;
    }
    // Remove automated attribute from tab
    if (tab && tab.removeAttribute) {
      tab.removeAttribute("data-floorp-os-automated");
      tab.removeAttribute("data-floorp-os-instance-id");
    }
    this._browserInstances.delete(instanceId);
    TAB_MANAGER_ACTOR_SETS.delete(browser);
  }

  public async navigate(instanceId: string, url: string): Promise<void> {
    const { browser } = this._getInstance(instanceId);
    const principal = Services.scriptSecurityManager.getSystemPrincipal();

    const oa = E10SUtils.predictOriginAttributes({ browser });
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

    // Check if browser.loadURI is defined before calling it
    if (typeof browser.loadURI === "function") {
      await this._delayForUser();
      browser.loadURI(Services.io.newURI(url), loadURIOptions);
    } else {
      // Throw an error if loadURI is not available
      throw new Error("browser.loadURI is not defined");
    }
    await this._waitForLoad(browser, url);
    await this._delayForUser();
  }

  public getURI(instanceId: string): Promise<string> {
    const { browser } = this._getInstance(instanceId);
    return Promise.resolve(browser.browsingContext?.currentURI.spec ?? "");
  }

  public getHTML(instanceId: string): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetHTML");
  }

  public getElements(instanceId: string, selector: string): Promise<string[]> {
    return this._queryActor<string[]>(instanceId, "WebScraper:GetElements", {
      selector,
    }).then((r) => r ?? []);
  }

  public getElementByText(
    instanceId: string,
    textContent: string,
  ): Promise<string | null> {
    return this._queryActor<string | null>(
      instanceId,
      "WebScraper:GetElementByText",
      { textContent },
    );
  }

  public getElementTextContent(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this._queryActor<string | null>(
      instanceId,
      "WebScraper:GetElementTextContent",
      { selector },
    );
  }

  public getElement(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetElement", {
      selector,
    });
  }

  public getElementText(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetElementText", {
      selector,
    });
  }

  public async clickElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:ClickElement",
      {
        selector,
      },
    );

    await this._delayForUser(3500);
    return result;
  }

  public waitForElement(
    instanceId: string,
    selector: string,
    timeout = 5000,
  ): Promise<boolean | null> {
    return this._queryActor<boolean>(instanceId, "WebScraper:WaitForElement", {
      selector,
      timeout,
    });
  }

  public takeScreenshot(instanceId: string): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:TakeScreenshot");
  }

  public takeElementScreenshot(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this._queryActor<string>(
      instanceId,
      "WebScraper:TakeElementScreenshot",
      { selector },
    );
  }

  public takeFullPageScreenshot(instanceId: string): Promise<string | null> {
    return this._queryActor<string>(
      instanceId,
      "WebScraper:TakeFullPageScreenshot",
    );
  }

  public takeRegionScreenshot(
    instanceId: string,
    rect?: object,
  ): Promise<string | null> {
    return this._queryActor<string>(
      instanceId,
      "WebScraper:TakeRegionScreenshot",
      { rect },
    );
  }

  public async fillForm(
    instanceId: string,
    formData: Record<string, string>,
    options: { typingMode?: boolean; typingDelayMs?: number } = {},
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:FillForm",
      {
        formData,
        typingMode: options.typingMode,
        typingDelayMs: options.typingDelayMs,
      },
    );

    await this._delayForUser(3500);
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
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:InputElement",
      {
        selector,
        value,
        typingMode: options.typingMode,
        typingDelayMs: options.typingDelayMs,
      },
    );
    await this._delayForUser(3500);
    return result;
  }

  /**
   * Press a key or key combination.
   */
  public async pressKey(
    instanceId: string,
    key: string,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:PressKey",
      {
        key,
      },
    );
    await this._delayForUser(500);
    return result;
  }

  /**
   * Upload a file through input[type=file]
   */
  public async uploadFile(
    instanceId: string,
    selector: string,
    filePath: string,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:UploadFile",
      {
        selector,
        filePath,
      },
    );
    await this._delayForUser(3500);
    return result;
  }

  public getValue(
    instanceId: string,
    selector: string,
  ): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetValue", {
      selector,
    });
  }

  /**
   * Gets the value of a specific attribute from an element
   */
  public getAttribute(
    instanceId: string,
    selector: string,
    attributeName: string,
  ): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetAttribute", {
      selector,
      attributeName,
    });
  }

  /**
   * Checks if an element is visible in the viewport
   */
  public isVisible(instanceId: string, selector: string): Promise<boolean> {
    return this._queryActor<boolean>(instanceId, "WebScraper:IsVisible", {
      selector,
    }).then((r) => r ?? false);
  }

  /**
   * Checks if an element is enabled (not disabled)
   */
  public isEnabled(instanceId: string, selector: string): Promise<boolean> {
    return this._queryActor<boolean>(instanceId, "WebScraper:IsEnabled", {
      selector,
    }).then((r) => r ?? false);
  }

  /**
   * Clears the value of an input or textarea element
   */
  public async clearInput(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:ClearInput",
      { selector },
    );
    await this._delayForUser(3500);
    return result;
  }

  public async submit(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:Submit",
      {
        selector,
      },
    );

    await this._delayForUser(3500);
    return result;
  }

  /**
   * Selects an option in a select element
   */
  public async selectOption(
    instanceId: string,
    selector: string,
    value: string,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:SelectOption",
      {
        selector,
        optionValue: value,
      },
    );
    await this._delayForUser(3500);
    return result;
  }

  /**
   * Sets the checked state of a checkbox or radio button
   */
  public async setChecked(
    instanceId: string,
    selector: string,
    checked: boolean,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:SetChecked",
      {
        selector,
        checked,
      },
    );
    await this._delayForUser(3500);
    return result;
  }

  /**
   * Simulates a mouse hover over an element
   */
  public async hoverElement(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:HoverElement",
      { selector },
    );
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
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:ScrollToElement",
      { selector },
    );
    await this._delayForUser(1000);
    return result;
  }

  /**
   * Gets the page title
   */
  public getPageTitle(instanceId: string): Promise<string | null> {
    return this._queryActor<string>(instanceId, "WebScraper:GetPageTitle");
  }

  /**
   * Performs a double click on an element
   */
  public async doubleClick(
    instanceId: string,
    selector: string,
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:DoubleClick",
      { selector },
    );
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
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:RightClick",
      { selector },
    );
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
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:Focus",
      { selector },
    );
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
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:DragAndDrop",
      {
        selector: sourceSelector,
        targetSelector,
      },
    );
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
    const entry = this._getEntry(instanceId);
    if (!entry) return Promise.resolve([]);

    try {
      const uri = entry.browser.browsingContext?.currentURI;
      if (!uri) return Promise.resolve([]);

      const host = uri.host;
      const principal =
        entry.browser.browsingContext?.currentWindowGlobal?.documentPrincipal;
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
      if (Object.keys(originAttributes).length > 0) {
        originAttrCandidates.push({});
      }

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
      console.error("TabManager: Error getting cookies:", e);
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
    const entry = this._getEntry(instanceId);
    if (!entry) return Promise.resolve(null);

    try {
      const uri = entry.browser.browsingContext?.currentURI;
      if (!uri) return Promise.resolve(false);

      const sameSiteMap: Record<string, number> = {
        None: 0,
        Lax: 1,
        Strict: 2,
      };

      const principal =
        entry.browser.browsingContext?.currentWindowGlobal?.documentPrincipal;
      const originAttributes = principal?.originAttributes ?? {};

      const isSecure = cookie.secure ?? uri.schemeIs("https");
      let requestedSameSite = cookie.sameSite ?? "Lax";
      if (requestedSameSite === "None" && !isSecure) {
        // Firefox requires Secure for SameSite=None; fall back to Lax on http.
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

      const cookieString = (() => {
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
        return parts.filter(Boolean).join("; ");
      })();

      try {
        const actorResult = await this._queryActor<boolean>(
          instanceId,
          "WebScraper:SetCookieString",
          {
            cookieString,
            cookieName: cookie.name,
            cookieValue: cookie.value,
          },
        );
        if (actorResult) {
          return true;
        }
      } catch (err) {
        errors.push(String(err));
      }

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
              "TabManager: Cookie rejected:",
              validation?.result,
              validation?.errorString,
            );
            return false;
          }

          // Cookie was successfully added (validation passed)
          // Note: cookieExists() may return false immediately after add() due to timing
          // or originAttributes mismatch, so we trust the validation result instead
          return true;
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

      console.error(
        "TabManager: Cookie could not be set (all attempts failed)",
        errors,
      );
      return Promise.resolve(false);
    } catch (e) {
      console.error("TabManager: Error setting cookie:", e);
      return Promise.resolve(false);
    }
  }

  /**
   * Accepts (clicks OK on) an alert dialog
   * Note: Full dialog handling requires access to gBrowser which is not stored.
   * This is a simplified implementation that attempts to find dialogs via the window.
   */
  public acceptAlert(instanceId: string): Promise<boolean | null> {
    const entry = this._getEntry(instanceId);
    if (!entry) return Promise.resolve(null);

    try {
      // Try to find gBrowser from the browser's ownerGlobal
      const win = entry.browser.ownerGlobal as Window & {
        gBrowser?: GBrowser & {
          getTabDialogBox?: (tab: BrowserTab) => {
            getTabDialogManager?: () => {
              dialogs?: Array<{ frameContentWindow?: Window }>;
            };
          };
        };
      };
      const gBrowser = win?.gBrowser;
      if (gBrowser?.getTabDialogBox) {
        const tabDialogBox = gBrowser.getTabDialogBox(entry.tab);
        const dialogs = tabDialogBox?.getTabDialogManager?.()?.dialogs ?? [];
        for (const dialog of dialogs) {
          const dialogElement =
            dialog.frameContentWindow?.document?.querySelector(
              "dialog",
            ) as HTMLDialogElement | null;
          if (dialogElement) {
            const acceptButton = dialogElement.querySelector(
              '[dlgtype="accept"]',
            ) as HTMLButtonElement | null;
            if (acceptButton) {
              acceptButton.click();
              return Promise.resolve(true);
            }
          }
        }
      }
      return Promise.resolve(false);
    } catch (e) {
      console.error("TabManager: Error accepting alert:", e);
      return Promise.resolve(false);
    }
  }

  /**
   * Dismisses (clicks Cancel on) an alert dialog
   * Note: Full dialog handling requires access to gBrowser which is not stored.
   * This is a simplified implementation that attempts to find dialogs via the window.
   */
  public dismissAlert(instanceId: string): Promise<boolean | null> {
    const entry = this._getEntry(instanceId);
    if (!entry) return Promise.resolve(null);

    try {
      // Try to find gBrowser from the browser's ownerGlobal
      const win = entry.browser.ownerGlobal as Window & {
        gBrowser?: GBrowser & {
          getTabDialogBox?: (tab: BrowserTab) => {
            getTabDialogManager?: () => {
              dialogs?: Array<{ frameContentWindow?: Window }>;
            };
          };
        };
      };
      const gBrowser = win?.gBrowser;
      if (gBrowser?.getTabDialogBox) {
        const tabDialogBox = gBrowser.getTabDialogBox(entry.tab);
        const dialogs = tabDialogBox?.getTabDialogManager?.()?.dialogs ?? [];
        for (const dialog of dialogs) {
          const dialogElement =
            dialog.frameContentWindow?.document?.querySelector(
              "dialog",
            ) as HTMLDialogElement | null;
          if (dialogElement) {
            const cancelButton = dialogElement.querySelector(
              '[dlgtype="cancel"]',
            ) as HTMLButtonElement | null;
            if (cancelButton) {
              cancelButton.click();
              return Promise.resolve(true);
            }
          }
        }
      }
      return Promise.resolve(false);
    } catch (e) {
      console.error("TabManager: Error dismissing alert:", e);
      return Promise.resolve(false);
    }
  }

  /**
   * Saves the current page as PDF and returns base64 encoded data
   */
  public async saveAsPDF(instanceId: string): Promise<string | null> {
    const entry = this._getEntry(instanceId);
    if (!entry) return null;

    try {
      const browsingContext = entry.browser
        .browsingContext as BrowsingContext & {
        print(settings: nsIPrintSettings): Promise<void>;
      };
      if (!browsingContext) return null;

      // Create a storage stream for PDF output
      const storageStream = Cc["@mozilla.org/storagestream;1"].createInstance(
        Ci.nsIStorageStream,
      );
      storageStream.init(4096, 0xffffffff);

      // Create print settings for PDF
      const printSettingsService = Cc[
        "@mozilla.org/gfx/printsettings-service;1"
      ].getService(Ci.nsIPrintSettingsService);

      const printSettings = printSettingsService.createNewPrintSettings();

      // Configure print settings
      printSettings.outputFormat = Ci.nsIPrintSettings.kOutputFormatPDF ?? 0;
      printSettings.printSilent = true;
      printSettings.showPrintProgress = false;

      // Output to stream
      printSettings.outputDestination =
        Ci.nsIPrintSettings.kOutputDestinationStream;
      printSettings.outputStream = storageStream.getOutputStream(0);

      // Initialize from printer if possible
      try {
        const defaultPrinter = printSettingsService.lastUsedPrinterName;
        if (defaultPrinter) {
          printSettings.printerName = defaultPrinter;
        } else {
          printSettings.printerName = "marionette";
        }
        printSettings.isInitializedFromPrinter = true;
        printSettings.isInitializedFromPrefs = true;
      } catch (_e) {
        printSettings.isInitializedFromPrinter = false;
        printSettings.isInitializedFromPrefs = false;
      }

      // Paper settings
      printSettings.paperSizeUnit = Ci.nsIPrintSettings.kPaperSizeInches ?? 0;
      printSettings.paperWidth = 8.5; // US Letter width in inches
      printSettings.paperHeight = 11; // US Letter height in inches
      printSettings.usePageRuleSizeAsPaperSize = true;

      // Margins (1cm = ~0.394 inches)
      printSettings.marginTop = 0.4;
      printSettings.marginBottom = 0.4;
      printSettings.marginLeft = 0.4;
      printSettings.marginRight = 0.4;

      // Override unwriteable margins
      printSettings.unwriteableMarginTop = 0;
      printSettings.unwriteableMarginLeft = 0;
      printSettings.unwriteableMarginBottom = 0;
      printSettings.unwriteableMarginRight = 0;

      // Background and scaling
      printSettings.printBGColors = true;
      printSettings.printBGImages = true;
      printSettings.scaling = 1.0;
      printSettings.shrinkToFit = true;

      // Clear headers and footers
      printSettings.headerStrLeft = "";
      printSettings.headerStrCenter = "";
      printSettings.headerStrRight = "";
      printSettings.footerStrLeft = "";
      printSettings.footerStrCenter = "";
      printSettings.footerStrRight = "";

      // Print to stream
      await browsingContext.print(printSettings);

      // Read from storage stream
      const binaryStream = Cc[
        "@mozilla.org/binaryinputstream;1"
      ].createInstance(Ci.nsIBinaryInputStream);
      binaryStream.setInputStream(storageStream.newInputStream(0));

      const available = binaryStream.available();
      const bytes = binaryStream.readBytes(available);

      binaryStream.close();

      // Convert to base64
      const base64 = btoa(bytes);
      return base64;
    } catch (e) {
      console.error("TabManager: Error saving as PDF:", e);
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
    const entry = this._getEntry(instanceId);
    if (!entry) return Promise.resolve(null);

    const browser = entry.browser;

    return new Promise((resolve) => {
      const idleThreshold = 500; // Consider idle after 500ms of no activity
      const state = {
        idleTimer: null as ReturnType<typeof setTimeout> | null,
        resolved: false,
      };

      const resetIdleTimer = () => {
        if (state.idleTimer) {
          clearTimeout(state.idleTimer);
        }
        state.idleTimer = setTimeout(() => {
          if (!state.resolved) {
            state.resolved = true;
            cleanup();
            resolve(true);
          }
        }, idleThreshold);
      };

      const cleanup = () => {
        if (state.idleTimer) {
          clearTimeout(state.idleTimer);
        }
        try {
          browser.webProgress?.removeProgressListener(progressListener);
        } catch {
          // Ignore cleanup errors
        }
      };

      const progressListener = {
        QueryInterface: ChromeUtils.generateQI([
          "nsIWebProgressListener",
          "nsISupportsWeakReference",
        ]),
        _state: state,
        _resetIdleTimer: resetIdleTimer,
        onStateChange(
          _webProgress: nsIWebProgress,
          _request: nsIRequest,
          stateFlags: number,
          _status: number,
        ) {
          try {
            if (stateFlags & (Ci.nsIWebProgressListener.STATE_START ?? 0)) {
              // Network activity started
              if (this._state.idleTimer) {
                clearTimeout(this._state.idleTimer);
                this._state.idleTimer = null;
              }
            } else if (
              stateFlags & (Ci.nsIWebProgressListener.STATE_STOP ?? 0)
            ) {
              // Network activity stopped, start idle timer
              this._resetIdleTimer();
            }
          } catch (_e) {
            // Ignore errors in callback
          }
        },
        onProgressChange(
          _webProgress: nsIWebProgress,
          _request: nsIRequest,
          _curSelfProgress: number,
          _maxSelfProgress: number,
          _curTotalProgress: number,
          _maxTotalProgress: number,
        ) {
          // No-op
        },
        onLocationChange(
          _webProgress: nsIWebProgress,
          _request: nsIRequest,
          _location: nsIURI,
          _flags: number,
        ) {
          // No-op
        },
        onStatusChange(
          _webProgress: nsIWebProgress,
          _request: nsIRequest,
          _status: number,
          _message: string,
        ) {
          // No-op
        },
        onSecurityChange(
          _webProgress: nsIWebProgress,
          _request: nsIRequest,
          _state: number,
        ) {
          // No-op
        },
        onContentBlockingEvent(
          _webProgress: nsIWebProgress,
          _request: nsIRequest,
          _event: number,
        ) {
          // No-op
        },
      };

      try {
        browser.webProgress?.addProgressListener(
          progressListener,
          Ci.nsIWebProgress.NOTIFY_STATE_ALL ?? 0,
        );
      } catch (e) {
        console.error("TabManager: Error adding progress listener:", e);
        resolve(false);
        return;
      }

      // Start initial idle timer
      resetIdleTimer();

      // Timeout handler
      setTimeout(() => {
        if (!state.resolved) {
          state.resolved = true;
          cleanup();
          resolve(false);
        }
      }, timeout);
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
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:SetInnerHTML",
      { selector, innerHTML: html },
    );
    await this._delayForUser(2000);
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
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:SetTextContent",
      { selector, textContent: text },
    );
    await this._delayForUser(2000);
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
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:DispatchEvent",
      { selector, eventType, eventOptions: options },
    );
    await this._delayForUser(1000);
    return result;
  }
}

// Export a singleton instance of the TabManager service
export const TabManagerServices = new TabManager();
