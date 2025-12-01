/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * TabManagerService - A service for managing and interacting with browser tabs.
 */

const { E10SUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/E10SUtils.sys.mjs",
);
const { setTimeout } = ChromeUtils.importESModule(
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
   * Retrieves an instance by its ID, throwing an error if not found.
   */
  private _getInstance(instanceId: string) {
    const instance = this._browserInstances.get(instanceId);
    if (!instance) {
      throw new Error(`Instance not found for ID: ${instanceId}`);
    }
    return instance;
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
      const entry = this._browserInstances.get(instanceId);
      if (!entry) {
        return;
      }

      const win = getBrowserWindow() as Window & { gBrowser: GBrowser };
      if (win.closed) {
        return;
      }

      if (Services?.ww?.activeWindow !== win) {
        try {
          win.focus();
        } catch (_) {
          // ignore focus errors
        }
      }

      if (win.gBrowser.selectedTab !== entry.tab) {
        try {
          win.gBrowser.selectedTab = entry.tab;
        } catch (_) {
          // ignore tab selection errors
        }
      }

      try {
        entry.browser?.focus();
      } catch (_) {
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
        } catch (_) {
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
            flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_SAME_DOCUMENT ||
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
          const STATE_STOP = Ci.nsIWebProgressListener.STATE_STOP;
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
        Ci.nsIWebProgress.NOTIFY_LOCATION |
          Ci.nsIWebProgress.NOTIFY_STATE_DOCUMENT,
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
    }

    await this._delayForUser();

    return instanceId;
  }

  public listTabs(): Promise<TabListEntry[]> {
    const win = getBrowserWindow() as Window & { gBrowser: GBrowser };
    const gBrowser = win.gBrowser;

    return Promise.resolve(
      gBrowser.tabs.map((tab: BrowserTab) => ({
        browserId: tab.linkedBrowser.browserId.toString(),
        title: tab.label,
        url: tab.linkedBrowser.currentURI.spec,
        selected: tab.selected,
        pinned: tab.pinned,
      })),
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
    return Promise.resolve(browser.browsingContext.currentURI.spec);
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
  ): Promise<boolean | null> {
    this._focusInstance(instanceId);
    const result = await this._queryActor<boolean>(
      instanceId,
      "WebScraper:FillForm",
      {
        formData,
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
    const entry = this._browserInstances.get(instanceId);
    if (!entry) return Promise.resolve([]);

    try {
      const uri = entry.browser.browsingContext?.currentURI;
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
      console.error("TabManager: Error getting cookies:", e);
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
    const entry = this._browserInstances.get(instanceId);
    if (!entry) return Promise.resolve(null);

    try {
      const uri = entry.browser.browsingContext?.currentURI;
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
    const entry = this._browserInstances.get(instanceId);
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
    const entry = this._browserInstances.get(instanceId);
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
    const entry = this._browserInstances.get(instanceId);
    if (!entry) return null;

    try {
      const browsingContext = entry.browser
        .browsingContext as BrowsingContext & {
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
    const entry = this._browserInstances.get(instanceId);
    if (!entry) return Promise.resolve(null);

    const browser = entry.browser;

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
      } catch (e) {
        console.error("TabManager: Error adding progress listener:", e);
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

// Export a singleton instance of the TabManager service
export const TabManagerServices = new TabManager();
