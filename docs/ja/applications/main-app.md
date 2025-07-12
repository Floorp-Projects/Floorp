# メインアプリケーション - 詳細分析

## 概要

メインアプリケーションは Floorp ブラウザのコアであり、Chrome ツールバー上で動作するスクリプトが存在します。主要なブラウザ機能とユーザーインターフェースを提供します。SolidJS と Firefox の XUL システムを統合して、Firefox のエコシステムとの互換性を維持しながら、モダンでリアクティブなブラウザ体験を作り出します。

## アーキテクチャと技術スタック

### コア技術

- **SolidJS**: 効率的な DOM 更新のためのリアクティブ UI フレームワーク
- **TypeScript**: 型安全な JavaScript 開発
- **Tailwind CSS**: ユーティリティファーストの CSS フレームワーク
- **Vite**: 高速なビルドツールと開発サーバー
- **XUL**: Mozilla の XML ベースのユーザーインターフェース言語

### ディレクトリ構造

```
src/apps/main/
├── core/                   # コアアプリケーションロジック
│   ├── index.ts           # メインエントリーポイント
│   ├── modules.ts         # モジュール管理システム
│   ├── modules-hooks.ts   # モジュールライフサイクルフック
│   ├── common/            # 共通ユーティリティとタイプ
│   ├── static/            # 静的アセット
│   ├── test/              # テストファイル
│   └── utils/             # ユーティリティ関数
├── i18n/                  # 国際化ファイル
├── docs/                  # ドキュメント
├── about/                 # Aboutページコンポーネント
├── @types/                # TypeScript型定義
├── package.json           # 依存関係とスクリプト
├── vite.config.ts         # Vite設定
├── tsconfig.json          # TypeScript設定
├── tailwind.config.js     # Tailwind CSS設定
└── deno.json              # Deno設定
```

## コアシステム

### 1. エントリーポイント (index.ts)

メインエントリーポイントはアプリケーションを初期化し、Firefox の XUL システムとの統合を設定します。

```typescript
// メインアプリケーション初期化
import { render } from "solid-js/web";
import { ModuleManager } from "./modules";
import { setupXULIntegration } from "@floorp/solid-xul";

// XUL統合を初期化
setupXULIntegration();

// モジュールシステムを初期化
const moduleManager = new ModuleManager();

// アプリケーションを開始
async function initApp() {
  try {
    // コアモジュールを読み込み
    await moduleManager.loadCoreModules();

    // UIコンポーネントを初期化
    await initializeUI();

    // イベントリスナーを設定
    setupEventListeners();

    console.log("Floorpメインアプリケーションが初期化されました");
  } catch (error) {
    console.error("アプリケーションの初期化に失敗しました:", error);
  }
}

// UIコンポーネントを初期化
async function initializeUI() {
  // 強化するXUL要素を見つける
  const browserToolbox = document.getElementById("navigator-toolbox");
  const tabsToolbar = document.getElementById("TabsToolbar");

  if (browserToolbox && tabsToolbar) {
    // SolidJSコンポーネントをXULにレンダリング
    render(() => <BrowserEnhancements />, browserToolbox);
    render(() => <TabBarEnhancements />, tabsToolbar);
  }
}

// グローバルイベントリスナーを設定
function setupEventListeners() {
  // ブラウザウィンドウイベント
  window.addEventListener("load", handleWindowLoad);
  window.addEventListener("unload", handleWindowUnload);

  // タブイベント
  gBrowser.tabContainer.addEventListener("TabOpen", handleTabOpen);
  gBrowser.tabContainer.addEventListener("TabClose", handleTabClose);
}

// DOMが準備できたときに初期化
if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", initApp);
} else {
  initApp();
}
```

### 2. モジュール管理システム (modules.ts)

モジュールシステムは、ブラウザの異なる機能を整理・管理する方法を提供します。

```typescript
// モジュールインターフェース定義
export interface Module {
  id: string;
  name: string;
  version: string;
  dependencies?: string[];
  initialize(): Promise<void>;
  destroy(): Promise<void>;
}

// モジュールマネージャークラス
export class ModuleManager {
  private modules = new Map<string, Module>();
  private loadedModules = new Set<string>();

  // モジュールを登録
  registerModule(module: Module): void {
    this.modules.set(module.id, module);
  }

  // コアモジュールを読み込み
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

  // 特定のモジュールを読み込み
  async loadModule(moduleId: string): Promise<void> {
    if (this.loadedModules.has(moduleId)) {
      return; // 既に読み込み済み
    }

    const module = this.modules.get(moduleId);
    if (!module) {
      throw new Error(`モジュールが見つかりません: ${moduleId}`);
    }

    // 依存関係を最初に読み込み
    if (module.dependencies) {
      for (const depId of module.dependencies) {
        await this.loadModule(depId);
      }
    }

    // モジュールを初期化
    try {
      await module.initialize();
      this.loadedModules.add(moduleId);
      console.log(`モジュールが読み込まれました: ${moduleId}`);
    } catch (error) {
      console.error(`モジュール ${moduleId} の読み込みに失敗しました:`, error);
      throw error;
    }
  }

  // モジュールをアンロード
  async unloadModule(moduleId: string): Promise<void> {
    const module = this.modules.get(moduleId);
    if (module && this.loadedModules.has(moduleId)) {
      await module.destroy();
      this.loadedModules.delete(moduleId);
      console.log(`モジュールがアンロードされました: ${moduleId}`);
    }
  }

  // 読み込まれたモジュールを取得
  getLoadedModules(): string[] {
    return Array.from(this.loadedModules);
  }
}
```

### 3. モジュールフックシステム (modules-hooks.ts)

フックシステムは、モジュールのライフサイクル管理とイベント処理を提供します。

```typescript
import { createSignal, createEffect, onCleanup } from "solid-js";

// ブラウザ状態のフック
export function useBrowserState() {
  const [currentTab, setCurrentTab] = createSignal(gBrowser.selectedTab);
  const [tabCount, setTabCount] = createSignal(gBrowser.tabs.length);

  // 選択が変更されたときに現在のタブを更新
  const handleTabSelect = () => {
    setCurrentTab(gBrowser.selectedTab);
  };

  // タブが追加/削除されたときにタブ数を更新
  const handleTabCountChange = () => {
    setTabCount(gBrowser.tabs.length);
  };

  // イベントリスナーを設定
  gBrowser.tabContainer.addEventListener("TabSelect", handleTabSelect);
  gBrowser.tabContainer.addEventListener("TabOpen", handleTabCountChange);
  gBrowser.tabContainer.addEventListener("TabClose", handleTabCountChange);

  // コンポーネントのアンマウント時にクリーンアップ
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

// 設定のフック
export function usePreferences() {
  const [prefs, setPrefs] = createSignal(new Map());

  // 設定を読み込み
  const loadPreferences = () => {
    const prefService = Services.prefs;
    const prefMap = new Map();

    // Floorp固有の設定を読み込み
    const floorpPrefs = prefService.getChildList("floorp.");
    for (const prefName of floorpPrefs) {
      try {
        const value = prefService.getStringPref(prefName);
        prefMap.set(prefName, value);
      } catch (e) {
        // 異なる設定タイプを処理
        try {
          const value = prefService.getBoolPref(prefName);
          prefMap.set(prefName, value);
        } catch (e) {
          try {
            const value = prefService.getIntPref(prefName);
            prefMap.set(prefName, value);
          } catch (e) {
            console.warn(`設定を読み込めませんでした: ${prefName}`);
          }
        }
      }
    }

    setPrefs(prefMap);
  };

  // 設定を保存
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

      // ローカル状態を更新
      setPrefs((prev) => new Map(prev.set(name, value)));
    } catch (error) {
      console.error(`設定 ${name} の保存に失敗しました:`, error);
    }
  };

  // 初期化時に設定を読み込み
  loadPreferences();

  return {
    prefs,
    setPref,
    getPref: (name: string) => prefs().get(name),
    reloadPrefs: loadPreferences,
  };
}

// ウィンドウ管理のフック
export function useWindowManager() {
  const [windows, setWindows] = createSignal([]);

  // すべてのブラウザウィンドウを取得
  const getAllWindows = () => {
    const windowList = [];
    const windowEnumerator = Services.wm.getEnumerator("navigator:browser");

    while (windowEnumerator.hasMoreElements()) {
      const win = windowEnumerator.getNext();
      windowList.push(win);
    }

    return windowList;
  };

  // ウィンドウリストを更新
  const updateWindows = () => {
    setWindows(getAllWindows());
  };

  // ウィンドウイベントを監視
  const windowWatcher = {
    observe: (subject: any, topic: string) => {
      if (topic === "domwindowopened" || topic === "domwindowclosed") {
        updateWindows();
      }
    },
  };

  Services.ww.registerNotification(windowWatcher);

  // 初期読み込み
  updateWindows();

  // クリーンアップ
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

## UI コンポーネントシステム

### 1. ブラウザ拡張コンポーネント

```typescript
import { Component, createSignal, For } from "solid-js";
import { useBrowserState, usePreferences } from "../core/modules-hooks";

const BrowserEnhancements: Component = () => {
  const { currentTab, tabCount } = useBrowserState();
  const { prefs, setPref } = usePreferences();
  const [showSidebar, setShowSidebar] = createSignal(false);

  // サイドバーを切り替え
  const toggleSidebar = () => {
    const newState = !showSidebar();
    setShowSidebar(newState);
    setPref("floorp.sidebar.enabled", newState);
  };

  return (
    <div class="floorp-browser-enhancements">
      {/* サイドバー切り替えボタン */}
      <button
        class="sidebar-toggle-btn"
        onClick={toggleSidebar}
        title="サイドバーを切り替え"
      >
        <svg width="16" height="16" viewBox="0 0 16 16">
          <path d="M2 2h12v12H2V2zm1 1v10h3V3H3zm4 0v10h7V3H7z" />
        </svg>
      </button>

      {/* タブカウンター */}
      <div class="tab-counter">
        <span class="tab-count">{tabCount()}</span>
        <span class="tab-label">タブ</span>
      </div>

      {/* クイックアクション */}
      <div class="quick-actions">
        <button
          class="quick-action-btn"
          onClick={() => BrowserTestUtils.addTab(gBrowser, "about:newtab")}
          title="新しいタブ"
        >
          +
        </button>
        <button
          class="quick-action-btn"
          onClick={() =>
            gBrowser.selectedTab && gBrowser.removeTab(gBrowser.selectedTab)
          }
          title="タブを閉じる"
        >
          ×
        </button>
      </div>
    </div>
  );
};

export default BrowserEnhancements;
```

### 2. タブバー拡張コンポーネント

```typescript
import { Component, createSignal, createEffect } from "solid-js";
import { useBrowserState } from "../core/modules-hooks";

const TabBarEnhancements: Component = () => {
  const { currentTab, tabCount } = useBrowserState();
  const [tabGroups, setTabGroups] = createSignal([]);

  // ドメイン別にタブをグループ化
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
        // 有効なURIがないタブを処理
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
      {/* タブグループインジケーター */}
      <div class="tab-groups-indicator">
        <span class="groups-count">{tabGroups().length}</span>
        <span class="groups-label">グループ</span>
      </div>

      {/* 垂直タブ切り替え */}
      <button
        class="vertical-tabs-toggle"
        onClick={() => toggleVerticalTabs()}
        title="垂直タブを切り替え"
      >
        <svg width="16" height="16" viewBox="0 0 16 16">
          <path d="M3 2h10v2H3V2zm0 4h10v2H3V6zm0 4h10v2H3v-2z" />
        </svg>
      </button>
    </div>
  );
};

// 垂直タブレイアウトを切り替え
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

## XUL 統合

### 1. XUL 要素拡張

```typescript
// 既存のXUL要素をSolidJSリアクティビティで拡張
export function enhanceXULElement(element: XULElement, component: Component) {
  const container = document.createElement("div");
  container.className = "solid-xul-container";

  // コンテナをXUL要素の前に挿入
  element.parentNode?.insertBefore(container, element);

  // SolidJSコンポーネントをレンダリング
  render(() => component, container);

  return container;
}

// カスタムXUL要素を作成
export function createCustomXULElement(tagName: string, component: Component) {
  class CustomXULElement extends XULElement {
    connectedCallback() {
      super.connectedCallback();

      // XUL要素内にSolidJSコンポーネントをレンダリング
      render(() => component, this);
    }

    disconnectedCallback() {
      super.disconnectedCallback();
      // クリーンアップはSolidJSが処理
    }
  }

  customElements.define(tagName, CustomXULElement);
}
```

### 2. Firefox API 統合

```typescript
// SolidJSリアクティビティを持つFirefox APIのラッパー
export class FirefoxAPIWrapper {
  // ブックマークAPI
  static createBookmarksSignal() {
    const [bookmarks, setBookmarks] = createSignal([]);

    const loadBookmarks = async () => {
      try {
        const bookmarkTree = await PlacesUtils.promiseBookmarksTree();
        setBookmarks(bookmarkTree.children || []);
      } catch (error) {
        console.error("ブックマークの読み込みに失敗しました:", error);
      }
    };

    // ブックマーク変更を監視
    const bookmarkObserver = {
      onItemAdded: loadBookmarks,
      onItemRemoved: loadBookmarks,
      onItemChanged: loadBookmarks,
    };

    PlacesUtils.bookmarks.addObserver(bookmarkObserver);

    // 初期読み込み
    loadBookmarks();

    // クリーンアップ
    onCleanup(() => {
      PlacesUtils.bookmarks.removeObserver(bookmarkObserver);
    });

    return bookmarks;
  }

  // 履歴API
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
        console.error("履歴の読み込みに失敗しました:", error);
      }
    };

    loadHistory();

    return history;
  }
}
```

## 状態管理

### 1. グローバル状態ストア

```typescript
import { createStore } from "solid-js/store";

// グローバルアプリケーション状態
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

// 初期状態
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

// グローバルストアを作成
export const [appState, setAppState] = createStore(initialState);

// 状態アクション
export const appActions = {
  // UIアクション
  toggleSidebar: () =>
    setAppState("ui", "sidebarOpen", !appState.ui.sidebarOpen),
  toggleVerticalTabs: () =>
    setAppState("ui", "verticalTabs", !appState.ui.verticalTabs),
  setTheme: (theme: string) => setAppState("ui", "theme", theme),

  // ブラウザアクション
  setCurrentUrl: (url: string) => setAppState("browser", "currentUrl", url),
  setTabCount: (count: number) => setAppState("browser", "tabCount", count),
  setLoading: (loading: boolean) =>
    setAppState("browser", "isLoading", loading),

  // ユーザーアクション
  setPreference: (key: string, value: any) => {
    setAppState("user", "preferences", (prev) => new Map(prev.set(key, value)));
  },
  addCustomization: (customization: any) => {
    setAppState("user", "customizations", (prev) => [...prev, customization]);
  },
};
```

## パフォーマンス最適化

### 1. 遅延読み込み

```typescript
// コンポーネントの遅延読み込み
const LazySettingsPanel = lazy(() => import("./SettingsPanel"));
const LazyBookmarksManager = lazy(() => import("./BookmarksManager"));

// モジュールの遅延読み込み
const loadModuleAsync = async (moduleId: string) => {
  const module = await import(`./modules/${moduleId}`);
  return module.default;
};
```

### 2. 大きなリストの仮想スクロール

```typescript
import { createVirtualizer } from "@tanstack/solid-virtual";

const VirtualTabList: Component<{ tabs: Tab[] }> = (props) => {
  let containerRef: HTMLDivElement;

  const virtualizer = createVirtualizer({
    get count() {
      return props.tabs.length;
    },
    getScrollElement: () => containerRef,
    estimateSize: () => 32, // 各タブアイテムの高さ
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

## テスト戦略

### 1. ユニットテスト

```typescript
import { describe, it, expect } from "vitest";
import { render } from "@solidjs/testing-library";
import { ModuleManager } from "../core/modules";

describe("ModuleManager", () => {
  it("モジュールを登録して読み込むべき", async () => {
    const manager = new ModuleManager();

    const testModule = {
      id: "test-module",
      name: "テストモジュール",
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

### 2. 統合テスト

```typescript
import { describe, it, expect } from "vitest";
import { setupTestEnvironment } from "../test/setup";

describe("ブラウザ統合", () => {
  beforeEach(async () => {
    await setupTestEnvironment();
  });

  it("XUL要素をSolidJSコンポーネントで拡張すべき", async () => {
    const toolbox = document.getElementById("navigator-toolbox");
    expect(toolbox).toBeTruthy();

    // コンポーネントレンダリングをシミュレート
    const container = document.createElement("div");
    container.className = "solid-xul-container";
    toolbox?.appendChild(container);

    expect(toolbox?.querySelector(".solid-xul-container")).toBeTruthy();
  });
});
```

## コード例

### 1. カスタムブラウザ機能の作成

```typescript
// カスタム機能: タブグループ
export class TabGroupsFeature implements Module {
  id = "tab-groups";
  name = "タブグループ";
  version = "1.0.0";

  private groups = new Map<string, Tab[]>();

  async initialize() {
    // UI要素を追加
    this.addTabGroupsUI();

    // イベントリスナーを設定
    this.setupEventListeners();

    console.log("タブグループ機能が初期化されました");
  }

  async destroy() {
    // クリーンアップ
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
    // ドメイン別にタブを自動グループ化
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
      console.error("タブのグループ化に失敗しました:", e);
    }
  }
}
```
