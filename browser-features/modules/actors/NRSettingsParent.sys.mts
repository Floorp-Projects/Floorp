//TODO: make reject when the name is invalid
export class NRSettingsParent extends JSWindowActorParent {
  constructor() {
    super();
  }

  private _formatError(error: unknown): string {
    if (error instanceof Error && error.message) {
      return error.message;
    }
    if (typeof error === "string" && error.trim() !== "") {
      return error;
    }
    try {
      const json = JSON.stringify(error);
      if (json && json !== "{}" && json !== "[]") {
        return json;
      }
    } catch {
      // ignore
    }
    return "Unknown actor error";
  }

  /**
   * Get NRWebScraper actor from the currently active browser tab
   * This enables direct DOM manipulation via Actor IPC instead of HTTP
   */
  private _getActiveTabActor(): {
    actor: { sendQuery: (name: string, data?: object) => Promise<unknown> };
    browser: XULBrowserElement;
  } | null {
    try {
      const win = Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as Window & {
        gBrowser: {
          selectedBrowser: XULBrowserElement;
        };
      };
      if (!win?.gBrowser?.selectedBrowser) return null;
      const browser = win.gBrowser.selectedBrowser;
      const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
        "NRWebScraper",
      ) as
        | { sendQuery: (name: string, data?: object) => Promise<unknown> }
        | undefined;
      if (!actor) return null;
      return { actor, browser };
    } catch {
      return null;
    }
  }

  /**
   * Get actor for a specific tab by index
   */
  private _getTabActorByIndex(
    tabIndex: number,
  ): {
    actor: { sendQuery: (name: string, data?: object) => Promise<unknown> };
    browser: XULBrowserElement;
  } | null {
    try {
      const win = Services.wm.getMostRecentWindow(
        "navigator:browser",
      ) as Window & {
        gBrowser: {
          tabs: Array<{ linkedBrowser: XULBrowserElement }>;
        };
      };
      if (!win?.gBrowser?.tabs) return null;
      const tab = win.gBrowser.tabs[tabIndex];
      if (!tab?.linkedBrowser) return null;
      const browser = tab.linkedBrowser;
      const actor = browser.browsingContext?.currentWindowGlobal?.getActor(
        "NRWebScraper",
      ) as
        | { sendQuery: (name: string, data?: object) => Promise<unknown> }
        | undefined;
      if (!actor) return null;
      return { actor, browser };
    } catch {
      return null;
    }
  }

  async receiveMessage(message: {
    name: string;
    data?: unknown;
  }): Promise<unknown> {
    const data = message.data as Record<string, unknown> | undefined;
    switch (message.name) {
      case "getBoolPref": {
        const name = data && typeof data.name === "string" ? data.name : null;
        if (!name) return null;
        if (Services.prefs.getPrefType(name) != Services.prefs.PREF_BOOL) {
          return null;
        }
        return Services.prefs.getBoolPref(name);
      }
      case "getIntPref": {
        const name = data && typeof data.name === "string" ? data.name : null;
        if (!name) return null;
        if (Services.prefs.getPrefType(name) != Services.prefs.PREF_INT) {
          return null;
        }
        return Services.prefs.getIntPref(name);
      }
      case "getStringPref": {
        const name = data && typeof data.name === "string" ? data.name : null;
        if (!name) return null;
        if (Services.prefs.getPrefType(name) != Services.prefs.PREF_STRING) {
          return null;
        }
        return Services.prefs.getStringPref(name);
      }
      case "setBoolPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val =
            data && typeof data.prefValue === "boolean" ? data.prefValue : null;
          if (!name || val === null) return null;
          Services.prefs.setBoolPref(name, val);
        }
        break;
      }
      case "setIntPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val =
            data && typeof data.prefValue === "number" ? data.prefValue : null;
          if (!name || val === null) return null;
          Services.prefs.setIntPref(name, val);
        }
        break;
      }
      case "setStringPref": {
        {
          const name = data && typeof data.name === "string" ? data.name : null;
          const val =
            data && typeof data.prefValue === "string" ? data.prefValue : null;
          if (!name || val === null) return null;
          Services.prefs.setStringPref(name, val);
        }
        break;
      }
      // Browser operations for LLM Chat
      case "listTabs": {
        const win = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as Window & {
          gBrowser: {
            tabs: Array<{
              linkedBrowser: {
                currentURI: { spec: string };
                contentTitle: string;
              };
            }>;
          };
        };
        if (!win || !win.gBrowser) return [];
        return win.gBrowser.tabs.map((tab, index) => ({
          id: String(index),
          title: tab.linkedBrowser.contentTitle || "",
          url: tab.linkedBrowser.currentURI.spec,
        }));
      }
      case "createTab": {
        const url =
          data && typeof data.url === "string" ? data.url : "about:blank";
        const win = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as Window & {
          gBrowser: {
            addTab: (url: string, options: unknown) => unknown;
            selectedTab: unknown;
          };
        };
        if (!win || !win.gBrowser)
          return { error: "No browser window available" };
        const tab = win.gBrowser.addTab(url, {
          relatedToCurrent: true,
          triggeringPrincipal:
            Services.scriptSecurityManager.getSystemPrincipal(),
        });
        win.gBrowser.selectedTab = tab;
        return { success: true, id: String((tab as { _tPos: number })._tPos) };
      }
      case "navigateTab": {
        const tabId =
          data && typeof data.tabId === "string" ? data.tabId : null;
        const url = data && typeof data.url === "string" ? data.url : null;
        if (!tabId || !url) return { error: "Missing tabId or url" };
        const win = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as Window & {
          gBrowser: {
            tabs: Array<{
              linkedBrowser: {
                loadURI: (uri: unknown, options: unknown) => void;
              };
            }>;
          };
        };
        if (!win || !win.gBrowser)
          return { error: "No browser window available" };
        const tabIndex = parseInt(tabId, 10);
        const tab = win.gBrowser.tabs[tabIndex];
        if (!tab) return { error: "Tab not found" };
        try {
          const uri = Services.io.newURI(url);
          tab.linkedBrowser.loadURI(uri, {
            triggeringPrincipal:
              Services.scriptSecurityManager.getSystemPrincipal(),
          });
          return { success: true };
        } catch (e) {
          return { success: false, error: this._formatError(e) };
        }
      }
      case "closeTab": {
        const tabId =
          data && typeof data.tabId === "string" ? data.tabId : null;
        if (!tabId) return { error: "Missing tabId" };
        const win = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as Window & {
          gBrowser: { removeTab: (tab: unknown) => void; tabs: unknown[] };
        };
        if (!win || !win.gBrowser)
          return { error: "No browser window available" };
        const tabIndex = parseInt(tabId, 10);
        const tab = win.gBrowser.tabs[tabIndex];
        if (!tab) return { error: "Tab not found" };
        win.gBrowser.removeTab(tab);
        return { success: true };
      }
      case "searchWeb": {
        const query =
          data && typeof data.query === "string" ? data.query : null;
        if (!query) return { error: "Missing query" };
        const win = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as Window & {
          BrowserSearch: { loadSearchFromContext: (query: string) => void };
        };
        if (!win) return { error: "No browser window available" };
        const engine = Services.search.defaultEngine;
        const searchUrl = engine.getSubmission(query).uri.spec;
        const browserWin = win as Window & {
          gBrowser: {
            addTab: (url: string, options: unknown) => unknown;
            selectedTab: unknown;
          };
        };
        const tab = browserWin.gBrowser.addTab(searchUrl, {
          relatedToCurrent: true,
          triggeringPrincipal:
            Services.scriptSecurityManager.getSystemPrincipal(),
        });
        browserWin.gBrowser.selectedTab = tab;
        return { success: true };
      }
      case "getBrowserHistory": {
        const limit = data && typeof data.limit === "number" ? data.limit : 10;
        const history: Array<{ url: string; title: string }> = [];
        // Simple history query - this is a basic implementation
        // Note: For full history access, we'd need PlacesUtils
        return history.slice(0, limit);
      }
      // Scraper operations - Use Actor IPC instead of HTTP
      // These operate on the currently active tab via NRWebScraper actor
      case "createScraperInstance": {
        // No-op: Actor IPC doesn't need instance management
        // Return a pseudo ID for compatibility
        return { success: true, id: "active-tab" };
      }
      case "destroyScraperInstance": {
        // No-op: Actor IPC doesn't need instance management
        return { success: true };
      }
      case "scraperNavigate": {
        // Navigate active tab using existing navigateTab handler
        const url = data && typeof data.url === "string" ? data.url : null;
        if (!url) return { success: false, error: "Missing url" };
        const win = Services.wm.getMostRecentWindow(
          "navigator:browser",
        ) as Window & {
          gBrowser: {
            selectedBrowser: {
              loadURI: (uri: unknown, options: unknown) => void;
            };
          };
        };
        if (!win?.gBrowser?.selectedBrowser)
          return { success: false, error: "No active browser" };
        try {
          const uri = Services.io.newURI(url);
          win.gBrowser.selectedBrowser.loadURI(uri, {
            triggeringPrincipal:
              Services.scriptSecurityManager.getSystemPrincipal(),
          });
          return { success: true };
        } catch (e) {
          return { success: false, error: this._formatError(e) };
        }
      }
      case "scraperGetHtml": {
        const result = this._getActiveTabActor();
        if (!result) return { html: "", error: "No active tab actor" };
        try {
          const html = await result.actor.sendQuery("WebScraper:GetHTML");
          return { html: String(html ?? "") };
        } catch (e) {
          return { html: "", error: this._formatError(e) };
        }
      }
      case "scraperGetText": {
        const result = this._getActiveTabActor();
        if (!result) return { text: "", error: "No active tab actor" };
        try {
          // Get body text content via HTML and extract text
          const html = await result.actor.sendQuery("WebScraper:GetHTML");
          return { text: String(html ?? "") };
        } catch (e) {
          return { text: "", error: this._formatError(e) };
        }
      }
      case "scraperGetElementText": {
        const selector =
          data && typeof data.selector === "string" ? data.selector : null;
        if (!selector) return { text: "" };
        const result = this._getActiveTabActor();
        if (!result) return { text: "", error: "No active tab actor" };
        try {
          const text = await result.actor.sendQuery(
            "WebScraper:GetElementTextContent",
            { selector },
          );
          return { text: String(text ?? "") };
        } catch (e) {
          return { text: "", error: this._formatError(e) };
        }
      }
      case "scraperGetElements": {
        const selector =
          data && typeof data.selector === "string" ? data.selector : null;
        if (!selector) return { elements: [] };
        const result = this._getActiveTabActor();
        if (!result) return { elements: [], error: "No active tab actor" };
        try {
          const elements = await result.actor.sendQuery(
            "WebScraper:GetElements",
            { selector },
          );
          return { elements: Array.isArray(elements) ? elements : [] };
        } catch (e) {
          return { elements: [], error: this._formatError(e) };
        }
      }
      case "scraperGetElementAttribute": {
        const selector =
          data && typeof data.selector === "string" ? data.selector : null;
        const attribute =
          data && typeof data.attribute === "string" ? data.attribute : null;
        if (!selector || !attribute) return { value: "" };
        const result = this._getActiveTabActor();
        if (!result) return { value: "", error: "No active tab actor" };
        try {
          const value = await result.actor.sendQuery(
            "WebScraper:GetAttribute",
            {
              selector,
              attributeName: attribute,
            },
          );
          return { value: String(value ?? "") };
        } catch (e) {
          return { value: "", error: this._formatError(e) };
        }
      }
      // Scraper operations - Actions
      case "scraperClick": {
        const selector =
          data && typeof data.selector === "string" ? data.selector : null;
        if (!selector) return { success: false, error: "Missing selector" };
        const result = this._getActiveTabActor();
        if (!result) return { success: false, error: "No active tab actor" };
        try {
          await result.actor.sendQuery("WebScraper:ClickElement", { selector });
          return { success: true };
        } catch (e) {
          return { success: false, error: this._formatError(e) };
        }
      }
      case "scraperFillForm": {
        const fields = data && Array.isArray(data.fields) ? data.fields : null;
        if (!fields) return { success: false, error: "Missing fields" };
        const result = this._getActiveTabActor();
        if (!result) return { success: false, error: "No active tab actor" };
        try {
          // Convert fields array to formData object format expected by actor
          const formData: Record<string, string> = {};
          for (const field of fields) {
            if (
              field &&
              typeof field === "object" &&
              "selector" in field &&
              "value" in field
            ) {
              formData[(field as { selector: string }).selector] = (
                field as { value: string }
              ).value;
            }
          }
          await result.actor.sendQuery("WebScraper:FillForm", { formData });
          return { success: true };
        } catch (e) {
          return { success: false, error: this._formatError(e) };
        }
      }
      case "scraperClearInput": {
        const selector =
          data && typeof data.selector === "string" ? data.selector : null;
        if (!selector) return { success: false, error: "Missing selector" };
        const result = this._getActiveTabActor();
        if (!result) return { success: false, error: "No active tab actor" };
        try {
          await result.actor.sendQuery("WebScraper:ClearInput", { selector });
          return { success: true };
        } catch (e) {
          return { success: false, error: this._formatError(e) };
        }
      }
      case "scraperSubmit": {
        const selector =
          data && typeof data.selector === "string" ? data.selector : null;
        if (!selector) return { success: false, error: "Missing selector" };
        const result = this._getActiveTabActor();
        if (!result) return { success: false, error: "No active tab actor" };
        try {
          await result.actor.sendQuery("WebScraper:Submit", { selector });
          return { success: true };
        } catch (e) {
          return { success: false, error: this._formatError(e) };
        }
      }
      case "scraperWaitForElement": {
        const selector =
          data && typeof data.selector === "string" ? data.selector : null;
        const timeout =
          data && typeof data.timeout === "number" ? data.timeout : 5000;
        if (!selector) return { success: false, error: "Missing selector" };
        const result = this._getActiveTabActor();
        if (!result) return { success: false, error: "No active tab actor" };
        try {
          await result.actor.sendQuery("WebScraper:WaitForElement", {
            selector,
            timeout,
          });
          return { success: true };
        } catch (e) {
          return { success: false, error: this._formatError(e) };
        }
      }
      case "scraperExecuteScript": {
        const script =
          data && typeof data.script === "string" ? data.script : null;
        if (!script) return { value: "" };
        const result = this._getActiveTabActor();
        if (!result) return { value: "", error: "No active tab actor" };
        try {
          const value = await result.actor.sendQuery(
            "WebScraper:DispatchEvent",
            {
              selector: "body",
              eventType: "executeScript",
              eventOptions: { script },
            },
          );
          return { value: String(value ?? "") };
        } catch (e) {
          return { value: "", error: this._formatError(e) };
        }
      }
      case "scraperTakeScreenshot": {
        const selector =
          data && typeof data.selector === "string" ? data.selector : undefined;
        const result = this._getActiveTabActor();
        if (!result) return { data: "", error: "No active tab actor" };
        try {
          if (selector) {
            const data = await result.actor.sendQuery(
              "WebScraper:TakeElementScreenshot",
              { selector },
            );
            return { data: String(data ?? "") };
          } else {
            const data = await result.actor.sendQuery(
              "WebScraper:TakeFullPageScreenshot",
            );
            return { data: String(data ?? "") };
          }
        } catch (e) {
          return { data: "", error: this._formatError(e) };
        }
      }
    }
  }
}
