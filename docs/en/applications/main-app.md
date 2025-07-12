# Main Application - Detailed Analysis

## Overview

The main application is the core of the Floorp browser, with scripts running on the Chrome toolbar. It provides essential browser functionality and user interface. It integrates SolidJS with Firefox's XUL system to maintain compatibility with the Firefox ecosystem while creating a modern, reactive browser experience.

## Architecture and Technology Stack

### Core Technologies

- **SolidJS**: Reactive UI framework for efficient DOM updates
- **TypeScript**: Type-safe JavaScript development
- **Tailwind CSS**: Utility-first CSS framework
- **Vite**: Fast build tool and development server
- **XUL**: Mozilla's XML-based user interface language

### Directory Structure

```
src/apps/main/
├── core/                   # Core application logic
│   ├── index.ts           # Main entry point
│   ├── modules.ts         # Module management system
│   ├── modules-hooks.ts   # Module lifecycle hooks
│   ├── common/            # Common utilities and types
│   ├── static/            # Static assets
│   ├── test/              # Test files
│   └── utils/             # Utility functions
├── i18n/                  # Internationalization files
├── docs/                  # Documentation
├── about/                 # About page components
├── @types/                # TypeScript type definitions
├── package.json           # Dependencies and scripts
├── vite.config.ts         # Vite configuration
├── tsconfig.json          # TypeScript configuration
├── tailwind.config.js     # Tailwind CSS configuration
└── deno.json              # Deno configuration
```

## Core Systems

### 1. Entry Point (index.ts)

The main entry point initializes the application and sets up integration with Firefox's XUL system.

```typescript
// Main application initialization
import { render } from "solid-js/web";
import { ModuleManager } from "./modules";
import { setupXULIntegration } from "@floorp/solid-xul";

// Initialize XUL integration
setupXULIntegration();

// Initialize module system
const moduleManager = new ModuleManager();

// Start application
async function initApp() {
  try {
    // Load core modules
    await moduleManager.loadCoreModules();

    // Initialize UI components
    await initializeUI();

    // Set up event listeners
    setupEventListeners();

    console.log("Floorp main application initialized");
  } catch (error) {
    console.error("Failed to initialize application:", error);
  }
}

// Initialize UI components
async function initializeUI() {
  // Find XUL elements to enhance
  const browserToolbox = document.getElementById("navigator-toolbox");
  const tabsToolbar = document.getElementById("TabsToolbar");

  if (browserToolbox && tabsToolbar) {
    // Render SolidJS components into XUL
    render(() => <BrowserEnhancements />, browserToolbox);
    render(() => <TabBarEnhancements />, tabsToolbar);
  }
}

// Set up global event listeners
function setupEventListeners() {
  // Browser window events
  window.addEventListener("load", handleWindowLoad);
  window.addEventListener("unload", handleWindowUnload);

  // Tab events
  gBrowser.tabContainer.addEventListener("TabOpen", handleTabOpen);
  gBrowser.tabContainer.addEventListener("TabClose", handleTabClose);
}

// Initialize when DOM is ready
if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initApp);
} else {
  initApp();
}
```

### 2. Module Management System (modules.ts)

The module system provides a way to organize and manage different browser features.

```typescript
// Module interface definition
export interface Module {
  id: string;
  name: string;
  version: string;
  dependencies?: string[];
  initialize(): Promise<void>;
  destroy(): Promise<void>;
}

// Module manager class
export class ModuleManager {
  private modules = new Map<string, Module>();
  private loadedModules = new Set<string>();

  // Register module
  registerModule(module: Module): void {
    this.modules.set(module.id, module);
  }

  // Load core modules
  async loadCoreModules(): Promise<void> {
    const coreModules = [
      "ui-enhancements",
      "tab-management",
      "sidebar-manager",
      "user-scripts",
      "theme-manager",
    ];

    for (const moduleId of coreModules) {
      await this.loadModule(moduleId);
    }
  }

  // Load specific module
  async loadModule(moduleId: string): Promise<void> {
    if (this.loadedModules.has(moduleId)) {
      return; // Already loaded
    }

    const module = this.modules.get(moduleId);
    if (!module) {
      throw new Error(`Module not found: ${moduleId}`);
    }

    // Load dependencies first
    if (module.dependencies) {
      for (const depId of module.dependencies) {
        await this.loadModule(depId);
      }
    }

    // Initialize module
    try {
      await module.initialize();
      this.loadedModules.add(moduleId);
      console.log(`Module loaded: ${moduleId}`);
    } catch (error) {
      console.error(`Failed to load module ${moduleId}:`, error);
      throw error;
    }
  }

  // Unload module
  async unloadModule(moduleId: string): Promise<void> {
    const module = this.modules.get(moduleId);
    if (module && this.loadedModules.has(moduleId)) {
      await module.destroy();
      this.loadedModules.delete(moduleId);
      console.log(`Module unloaded: ${moduleId}`);
    }
  }

  // Get loaded modules
  getLoadedModules(): string[] {
    return Array.from(this.loadedModules);
  }
}
```

### 3. Module Hook System (modules-hooks.ts)

The hook system provides module lifecycle management and event handling.

```typescript
import { createSignal, createEffect, onCleanup } from "solid-js";

// Browser state hook
export function useBrowserState() {
  const [currentTab, setCurrentTab] = createSignal(gBrowser.selectedTab);
  const [tabCount, setTabCount] = createSignal(gBrowser.tabs.length);

  // Update current tab when selection changes
  const handleTabSelect = () => {
    setCurrentTab(gBrowser.selectedTab);
  };

  // Update tab count when tabs are added/removed
  const handleTabCountChange = () => {
    setTabCount(gBrowser.tabs.length);
  };

  // Set up event listeners
  gBrowser.tabContainer.addEventListener("TabSelect", handleTabSelect);
  gBrowser.tabContainer.addEventListener("TabOpen", handleTabCountChange);
  gBrowser.tabContainer.addEventListener("TabClose", handleTabCountChange);

  // Cleanup on component unmount
  onCleanup(() => {
    gBrowser.tabContainer.removeEventListener("TabSelect", handleTabSelect);
    gBrowser.tabContainer.removeEventListener("TabOpen", handleTabCountChange);
    gBrowser.tabContainer.removeEventListener("TabClose", handleTabCountChange);
  });

  return {
    currentTab,
    tabCount,
    switchToTab: (tab: XULElement) => (gBrowser.selectedTab = tab),
    closeTab: (tab: XULElement) => gBrowser.removeTab(tab),
  };
}

// Preferences hook
export function usePreferences() {
  const [prefs, setPrefs] = createSignal(new Map());

  // Load preferences
  const loadPreferences = () => {
    const prefService = Services.prefs;
    const prefMap = new Map();

    // Load Floorp-specific preferences
    const floorpPrefs = prefService.getChildList("floorp.");
    for (const prefName of floorpPrefs) {
      try {
        const value = prefService.getStringPref(prefName);
        prefMap.set(prefName, value);
      } catch (e) {
        // Handle different preference types
        try {
          const value = prefService.getBoolPref(prefName);
          prefMap.set(prefName, value);
        } catch (e) {
          try {
            const value = prefService.getIntPref(prefName);
            prefMap.set(prefName, value);
          } catch (e) {
            console.warn(`Could not load preference: ${prefName}`);
          }
        }
      }
    }

    setPrefs(prefMap);
  };

  // Save preference
  const setPref = (name: string, value: any) => {
    const prefService = Services.prefs;

    try {
      if (typeof value === "string") {
        prefService.setStringPref(name, value);
      } else if (typeof value === "boolean") {
        prefService.setBoolPref(name, value);
      } else if (typeof value === "number") {
        prefService.setIntPref(name, value);
      }

      // Update local state
      setPrefs((prev) => new Map(prev.set(name, value)));
    } catch (error) {
      console.error(`Failed to save preference ${name}:`, error);
    }
  };

  // Load preferences on initialization
  loadPreferences();

  return {
    prefs,
    setPref,
    getPref: (name: string) => prefs().get(name),
    reloadPrefs: loadPreferences,
  };
}

// Window management hook
export function useWindowManager() {
  const [windows, setWindows] = createSignal([]);

  // Get all browser windows
  const getAllWindows = () => {
    const windowList = [];
    const windowEnumerator = Services.wm.getEnumerator("navigator:browser");

    while (windowEnumerator.hasMoreElements()) {
      const win = windowEnumerator.getNext();
      windowList.push(win);
    }

    return windowList;
  };

  // Update window list
  const updateWindows = () => {
    setWindows(getAllWindows());
  };

  // Monitor window events
  const windowWatcher = {
    observe: (subject: any, topic: string) => {
      if (topic === "domwindowopened" || topic === "domwindowclosed") {
        updateWindows();
      }
    },
  };

  Services.ww.registerNotification(windowWatcher);

  // Initial load
  updateWindows();

  // Cleanup
  onCleanup(() => {
    Services.ww.unregisterNotification(windowWatcher);
  });

  return {
    windows,
    currentWindow: window,
    openNewWindow: () =>
      window.openDialog("chrome://browser/content/browser.xhtml"),
    closeWindow: (win: Window) => win.close(),
  };
}
```

## UI Component System

### 1. Browser Enhancement Component

```typescript
import { Component, createSignal, For } from "solid-js";
import { useBrowserState, usePreferences } from "../core/modules-hooks";

const BrowserEnhancements: Component = () => {
  const { currentTab, tabCount } = useBrowserState();
  const { prefs, setPref } = usePreferences();
  const [showSidebar, setShowSidebar] = createSignal(false);

  // Toggle sidebar
  const toggleSidebar = () => {
    const newState = !showSidebar();
    setShowSidebar(newState);
    setPref("floorp.sidebar.enabled", newState);
  };

  return (
    <div class="floorp-browser-enhancements">
      {/* Sidebar toggle button */}
      <button
        class="sidebar-toggle-btn"
        onClick={toggleSidebar}
        title="Toggle sidebar"
      >
        <svg width="16" height="16" viewBox="0 0 16 16">
          <path d="M2 2h12v12H2V2zm1 1v10h3V3H3zm4 0v10h7V3H7z" />
        </svg>
      </button>

      {/* Tab counter */}
      <div class="tab-counter">
        <span class="tab-count">{tabCount()}</span>
        <span class="tab-label">Tabs</span>
      </div>

      {/* Quick actions */}
      <div class="quick-actions">
        <button
          class="quick-action-btn"
          onClick={() => BrowserTestUtils.addTab(gBrowser, "about:newtab")}
          title="New tab"
        >
          +
        </button>
        <button
          class="quick-action-btn"
          onClick={() =>
            gBrowser.selectedTab && gBrowser.removeTab(gBrowser.selectedTab)
          }
          title="Close tab"
        >
          ×
        </button>
      </div>
    </div>
  );
};

export default BrowserEnhancements;
```

### 2. Tab Bar Enhancement Component

```typescript
import { Component, createSignal, createEffect } from "solid-js";
import { useBrowserState } from "../core/modules-hooks";

const TabBarEnhancements: Component = () => {
  const { currentTab, tabCount } = useBrowserState();
  const [tabGroups, setTabGroups] = createSignal([]);

  // Group tabs by domain
  createEffect(() => {
    const tabs = Array.from(gBrowser.tabs);
    const groups = new Map();

    tabs.forEach((tab) => {
      try {
        const uri = tab.linkedBrowser.currentURI;
        const domain = uri.host || "local";

        if (!groups.has(domain)) {
          groups.set(domain, []);
        }
        groups.get(domain).push(tab);
      } catch (e) {
        // Handle tabs without valid URI
        if (!groups.has("other")) {
          groups.set("other", []);
        }
        groups.get("other").push(tab);
      }
    });

    setTabGroups(Array.from(groups.entries()));
  });

  return (
    <div class="floorp-tab-enhancements">
      {/* Tab group indicator */}
      <div class="tab-groups-indicator">
        <span class="groups-count">{tabGroups().length}</span>
        <span class="groups-label">Groups</span>
      </div>

      {/* Vertical tabs toggle */}
      <button
        class="vertical-tabs-toggle"
        onClick={() => toggleVerticalTabs()}
        title="Toggle vertical tabs"
      >
        <svg width="16" height="16" viewBox="0 0 16 16">
          <path d="M3 2h10v2H3V2zm0 4h10v2H3V6zm0 4h10v2H3v-2z" />
        </svg>
      </button>
    </div>
  );
};

// Toggle vertical tab layout
function toggleVerticalTabs() {
  const tabsToolbar = document.getElementById("TabsToolbar");
  const isVertical = tabsToolbar?.getAttribute("orient") === "vertical";

  if (tabsToolbar) {
    tabsToolbar.setAttribute("orient", isVertical ? "horizontal" : "vertical");
    Services.prefs.setBoolPref("floorp.tabs.vertical", !isVertical);
  }
}

export default TabBarEnhancements;
```

## XUL Integration

### 1. XUL Element Enhancement

```typescript
// Enhance existing XUL elements with SolidJS reactivity
export function enhanceXULElement(element: XULElement, component: Component) {
  const container = document.createElement("div");
  container.className = "solid-xul-container";

  // Insert container before XUL element
  element.parentNode?.insertBefore(container, element);

  // Render SolidJS component
  render(() => component, container);

  return container;
}

// Create custom XUL element
export function createCustomXULElement(tagName: string, component: Component) {
  class CustomXULElement extends XULElement {
    connectedCallback() {
      super.connectedCallback();

      // Render SolidJS component inside XUL element
      render(() => component, this);
    }

    disconnectedCallback() {
      super.disconnectedCallback();
      // Cleanup handled by SolidJS
    }
  }

  customElements.define(tagName, CustomXULElement);
}
```

### 2. Firefox API Integration

```typescript
// Firefox API wrapper with SolidJS reactivity
export class FirefoxAPIWrapper {
  // Bookmarks API
  static createBookmarksSignal() {
    const [bookmarks, setBookmarks] = createSignal([]);

    const loadBookmarks = async () => {
      try {
        const bookmarkTree = await PlacesUtils.promiseBookmarksTree();
        setBookmarks(bookmarkTree.children || []);
      } catch (error) {
        console.error("Failed to load bookmarks:", error);
      }
    };

    // Monitor bookmark changes
    const bookmarkObserver = {
      onItemAdded: loadBookmarks,
      onItemRemoved: loadBookmarks,
      onItemChanged: loadBookmarks,
    };

    PlacesUtils.bookmarks.addObserver(bookmarkObserver);

    // Initial load
    loadBookmarks();

    // Cleanup
    onCleanup(() => {
      PlacesUtils.bookmarks.removeObserver(bookmarkObserver);
    });

    return bookmarks;
  }

  // History API
  static createHistorySignal(limit: number = 50) {
    const [history, setHistory] = createSignal([]);

    const loadHistory = async () => {
      try {
        const db = await PlacesUtils.promiseDBConnection();
        const rows = await db.executeCached(
          `
          SELECT url, title, visit_date
          FROM moz_places p
          JOIN moz_historyvisits h ON p.id = h.place_id
          ORDER BY visit_date DESC
          LIMIT :limit
        `,
          { limit }
        );

        const historyItems = rows.map((row) => ({
          url: row.getResultByName("url"),
          title: row.getResultByName("title"),
          visitDate: new Date(row.getResultByName("visit_date") / 1000),
        }));

        setHistory(historyItems);
      } catch (error) {
        console.error("Failed to load history:", error);
      }
    };

    loadHistory();

    return history;
  }
}
```

## State Management

### 1. Global State Store

```typescript
import { createStore } from "solid-js/store";

// Global application state
export interface AppState {
  ui: {
    sidebarOpen: boolean;
    verticalTabs: boolean;
    theme: string;
    compactMode: boolean;
  };
  browser: {
    currentUrl: string;
    tabCount: number;
    isLoading: boolean;
  };
  user: {
    preferences: Map<string, any>;
    customizations: any[];
  };
}

// Initial state
const initialState: AppState = {
  ui: {
    sidebarOpen: false,
    verticalTabs: false,
    theme: "system",
    compactMode: false,
  },
  browser: {
    currentUrl: "",
    tabCount: 0,
    isLoading: false,
  },
  user: {
    preferences: new Map(),
    customizations: [],
  },
};

// Create global store
export const [appState, setAppState] = createStore(initialState);

// State actions
export const appActions = {
  // UI actions
  toggleSidebar: () =>
    setAppState("ui", "sidebarOpen", !appState.ui.sidebarOpen),
  toggleVerticalTabs: () =>
    setAppState("ui", "verticalTabs", !appState.ui.verticalTabs),
  setTheme: (theme: string) => setAppState("ui", "theme", theme),

  // Browser actions
  setCurrentUrl: (url: string) => setAppState("browser", "currentUrl", url),
  setTabCount: (count: number) => setAppState("browser", "tabCount", count),
  setLoading: (loading: boolean) =>
    setAppState("browser", "isLoading", loading),

  // User actions
  setPreference: (key: string, value: any) => {
    setAppState("user", "preferences", (prev) => new Map(prev.set(key, value)));
  },
  addCustomization: (customization: any) => {
    setAppState("user", "customizations", (prev) => [...prev, customization]);
  },
};
```

## Performance Optimization

### 1. Lazy Loading

```typescript
// Lazy load components
const LazySettingsPanel = lazy(() => import("./SettingsPanel"));
const LazyBookmarksManager = lazy(() => import("./BookmarksManager"));

// Lazy load modules
const loadModuleAsync = async (moduleId: string) => {
  const module = await import(`./modules/${moduleId}`);
  return module.default;
};
```

### 2. Virtual Scrolling for Large Lists

```typescript
import { createVirtualizer } from "@tanstack/solid-virtual";

const VirtualTabList: Component<{ tabs: Tab[] }> = (props) => {
  let containerRef: HTMLDivElement;

  const virtualizer = createVirtualizer({
    get count() {
      return props.tabs.length;
    },
    getScrollElement: () => containerRef,
    estimateSize: () => 32, // Height of each tab item
    overscan: 5,
  });

  return (
    <div ref={containerRef!} class="virtual-tab-list">
      <div
        style={{
          height: `${virtualizer.getTotalSize()}px`,
          position: "relative",
        }}
      >
        <For each={virtualizer.getVirtualItems()}>
          {(item) => (
            <div
              style={{
                position: "absolute",
                top: 0,
                left: 0,
                width: "100%",
                height: `${item.size}px`,
                transform: `translateY(${item.start}px)`,
              }}
            >
              <TabItem tab={props.tabs[item.index]} />
            </div>
          )}
        </For>
      </div>
    </div>
  );
};
```

## Testing Strategy

### 1. Unit Tests

```typescript
import { describe, it, expect } from "vitest";
import { render } from "@solidjs/testing-library";
import { ModuleManager } from "../core/modules";

describe("ModuleManager", () => {
  it("should register and load modules", async () => {
    const manager = new ModuleManager();

    const testModule = {
      id: "test-module",
      name: "Test Module",
      version: "1.0.0",
      initialize: vi.fn().mockResolvedValue(undefined),
      destroy: vi.fn().mockResolvedValue(undefined),
    };

    manager.registerModule(testModule);
    await manager.loadModule("test-module");

    expect(testModule.initialize).toHaveBeenCalled();
    expect(manager.getLoadedModules()).toContain("test-module");
  });
});
```

### 2. Integration Tests

```typescript
import { describe, it, expect } from "vitest";
import { setupTestEnvironment } from "../test/setup";

describe("Browser Integration", () => {
  beforeEach(async () => {
    await setupTestEnvironment();
  });

  it("should enhance XUL elements with SolidJS components", async () => {
    const toolbox = document.getElementById("navigator-toolbox");
    expect(toolbox).toBeTruthy();

    // Simulate component rendering
    const container = document.createElement("div");
    container.className = "solid-xul-container";
    toolbox?.appendChild(container);

    expect(toolbox?.querySelector(".solid-xul-container")).toBeTruthy();
  });
});
```

## Code Examples

### 1. Creating Custom Browser Features

```typescript
// Custom feature: Tab groups
export class TabGroupsFeature implements Module {
  id = "tab-groups";
  name = "Tab Groups";
  version = "1.0.0";

  private groups = new Map<string, Tab[]>();

  async initialize() {
    // Add UI elements
    this.addTabGroupsUI();

    // Set up event listeners
    this.setupEventListeners();

    console.log("Tab groups feature initialized");
  }

  async destroy() {
    // Cleanup
    this.removeTabGroupsUI();
    this.removeEventListeners();
  }

  private addTabGroupsUI() {
    const tabsToolbar = document.getElementById("TabsToolbar");
    if (tabsToolbar) {
      const container = document.createElement("div");
      container.id = "tab-groups-container";

      render(() => <TabGroupsComponent groups={this.groups} />, container);
      tabsToolbar.appendChild(container);
    }
  }

  private setupEventListeners() {
    gBrowser.tabContainer.addEventListener(
      "TabOpen",
      this.handleTabOpen.bind(this)
    );
    gBrowser.tabContainer.addEventListener(
      "TabClose",
      this.handleTabClose.bind(this)
    );
  }

  private handleTabOpen(event: Event) {
    const tab = event.target as Tab;
    // Auto-group tabs by domain
    this.autoGroupTab(tab);
  }

  private autoGroupTab(tab: Tab) {
    try {
      const uri = tab.linkedBrowser.currentURI;
      const domain = uri.host || "ungrouped";

      if (!this.groups.has(domain)) {
        this.groups.set(domain, []);
      }

      this.groups.get(domain)?.push(tab);
    } catch (e) {
      console.error("Failed to group tab:", e);
    }
  }
}
```
