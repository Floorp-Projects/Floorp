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
}

// Export a singleton instance of the TabManager service
export const TabManagerServices = new TabManager();
