# メインアプリケーション - 詳細解説

## 概要

メインアプリケーション (`src/apps/main/`) は Floorp ブラウザのコア機能を担当する最も重要なコンポーネントです。SolidJS と TypeScript で構築され、Firefox の XUL システムと統合してブラウザの主要な UI とロジックを提供します。

## アーキテクチャ

### 全体構造

```
src/apps/main/
├── core/                   # コア機能とロジック
│   ├── index.ts           # アプリケーションエントリーポイント
│   ├── modules.ts         # モジュール管理システム
│   ├── modules-hooks.ts   # モジュールライフサイクルフック
│   ├── common/            # 共通ユーティリティ
│   ├── static/            # 静的アセット
│   ├── test/              # テストファイル
│   └── utils/             # ユーティリティ関数
├── i18n/                  # 国際化ファイル
├── docs/                  # ドキュメント
├── about/                 # About ページ
├── @types/                # 型定義
├── package.json           # 依存関係
├── vite.config.ts         # Vite ビルド設定
├── tsconfig.json          # TypeScript 設定
├── tailwind.config.js     # Tailwind CSS 設定
├── postcss.config.js      # PostCSS 設定
├── deno.json              # Deno 設定
└── indexx.html            # HTML テンプレート
```

### 技術スタック

- **SolidJS**: リアクティブ UI フレームワーク
- **TypeScript**: 型安全な JavaScript
- **Tailwind CSS**: ユーティリティファーストCSS
- **Vite**: 高速ビルドツール
- **@nora/solid-xul**: XUL 統合ライブラリ
- **@nora/skin**: テーマシステム

## コアシステム

### 1. アプリケーションエントリーポイント (core/index.ts)

```typescript
// メインアプリケーションの初期化
import { render } from "solid-js/web";
import { createSignal, onMount } from "solid-js";
import { ModuleManager } from "./modules";
import { setupModuleHooks } from "./modules-hooks";

// アプリケーション状態
const [appState, setAppState] = createSignal({
  initialized: false,
  modules: new Map(),
  theme: "default"
});

// メインアプリケーションコンポーネント
function MainApp() {
  onMount(async () => {
    // モジュールシステムの初期化
    await ModuleManager.initialize();
    
    // フックの設定
    setupModuleHooks();
    
    // UI の初期化
    await initializeUI();
    
    setAppState(prev => ({ ...prev, initialized: true }));
  });

  return (
    <div class="floorp-main-app">
      {appState().initialized ? (
        <ModuleContainer />
      ) : (
        <LoadingScreen />
      )}
    </div>
  );
}

// DOM への描画
render(() => <MainApp />, document.getElementById("app")!);
```

### 2. モジュール管理システム (core/modules.ts)

```typescript
// モジュール管理の中核システム
export interface Module {
  id: string;
  name: string;
  version: string;
  dependencies: string[];
  initialize: () => Promise<void>;
  cleanup: () => Promise<void>;
  exports: Record<string, any>;
}

export class ModuleManager {
  private static modules = new Map<string, Module>();
  private static loadOrder: string[] = [];
  
  // モジュールの登録
  static register(module: Module): void {
    if (this.modules.has(module.id)) {
      throw new Error(`Module ${module.id} is already registered`);
    }
    
    this.modules.set(module.id, module);
    this.calculateLoadOrder();
  }
  
  // 依存関係を考慮した読み込み順序の計算
  private static calculateLoadOrder(): void {
    const visited = new Set<string>();
    const visiting = new Set<string>();
    const order: string[] = [];
    
    const visit = (moduleId: string) => {
      if (visiting.has(moduleId)) {
        throw new Error(`Circular dependency detected: ${moduleId}`);
      }
      
      if (!visited.has(moduleId)) {
        visiting.add(moduleId);
        
        const module = this.modules.get(moduleId);
        if (module) {
          for (const dep of module.dependencies) {
            visit(dep);
          }
        }
        
        visiting.delete(moduleId);
        visited.add(moduleId);
        order.push(moduleId);
      }
    };
    
    for (const moduleId of this.modules.keys()) {
      visit(moduleId);
    }
    
    this.loadOrder = order;
  }
  
  // 全モジュールの初期化
  static async initialize(): Promise<void> {
    for (const moduleId of this.loadOrder) {
      const module = this.modules.get(moduleId);
      if (module) {
        try {
          await module.initialize();
          console.log(`Module ${moduleId} initialized successfully`);
        } catch (error) {
          console.error(`Failed to initialize module ${moduleId}:`, error);
          throw error;
        }
      }
    }
  }
  
  // モジュールの取得
  static getModule(id: string): Module | undefined {
    return this.modules.get(id);
  }
  
  // 全モジュールのクリーンアップ
  static async cleanup(): Promise<void> {
    // 逆順でクリーンアップ
    for (const moduleId of this.loadOrder.reverse()) {
      const module = this.modules.get(moduleId);
      if (module) {
        try {
          await module.cleanup();
        } catch (error) {
          console.error(`Failed to cleanup module ${moduleId}:`, error);
        }
      }
    }
  }
}
```

### 3. モジュールフックシステム (core/modules-hooks.ts)

```typescript
// モジュールのライフサイクルフック
import { createSignal, createEffect } from "solid-js";

export interface ModuleHooks {
  onBeforeInit?: () => Promise<void>;
  onAfterInit?: () => Promise<void>;
  onBeforeCleanup?: () => Promise<void>;
  onAfterCleanup?: () => Promise<void>;
  onStateChange?: (newState: any, oldState: any) => void;
}

class HookManager {
  private hooks = new Map<string, ModuleHooks>();
  
  // フックの登録
  registerHooks(moduleId: string, hooks: ModuleHooks): void {
    this.hooks.set(moduleId, hooks);
  }
  
  // フックの実行
  async executeHook(
    hookName: keyof ModuleHooks, 
    moduleId?: string,
    ...args: any[]
  ): Promise<void> {
    const targets = moduleId 
      ? [this.hooks.get(moduleId)].filter(Boolean)
      : Array.from(this.hooks.values());
    
    for (const hooks of targets) {
      const hook = hooks[hookName];
      if (typeof hook === 'function') {
        try {
          await hook(...args);
        } catch (error) {
          console.error(`Hook ${hookName} failed for module ${moduleId}:`, error);
        }
      }
    }
  }
}

export const hookManager = new HookManager();

// グローバルフックの設定
export function setupModuleHooks(): void {
  // ウィンドウクローズ時のクリーンアップ
  window.addEventListener('beforeunload', async () => {
    await hookManager.executeHook('onBeforeCleanup');
    await ModuleManager.cleanup();
    await hookManager.executeHook('onAfterCleanup');
  });
  
  // テーマ変更の監視
  createEffect(() => {
    const theme = getCurrentTheme();
    hookManager.executeHook('onStateChange', undefined, { theme }, { theme: getPreviousTheme() });
  });
}
```

## UI コンポーネントシステム

### 1. ベースコンポーネント

```typescript
// 基本的な UI コンポーネント
import { Component, JSX, children } from "solid-js";
import { Dynamic } from "solid-js/web";

// Button コンポーネント
export const Button: Component<{
  variant?: "primary" | "secondary" | "danger";
  size?: "sm" | "md" | "lg";
  disabled?: boolean;
  onClick?: () => void;
  children: JSX.Element;
}> = (props) => {
  const c = children(() => props.children);
  
  const baseClasses = "px-4 py-2 rounded-md font-medium focus:outline-none focus:ring-2";
  const variantClasses = {
    primary: "bg-blue-600 text-white hover:bg-blue-700 focus:ring-blue-500",
    secondary: "bg-gray-200 text-gray-900 hover:bg-gray-300 focus:ring-gray-500",
    danger: "bg-red-600 text-white hover:bg-red-700 focus:ring-red-500"
  };
  const sizeClasses = {
    sm: "px-2 py-1 text-sm",
    md: "px-4 py-2",
    lg: "px-6 py-3 text-lg"
  };
  
  return (
    <button
      class={`${baseClasses} ${variantClasses[props.variant || "primary"]} ${sizeClasses[props.size || "md"]}`}
      disabled={props.disabled}
      onClick={props.onClick}
    >
      {c()}
    </button>
  );
};

// Modal コンポーネント
export const Modal: Component<{
  isOpen: boolean;
  onClose: () => void;
  title?: string;
  children: JSX.Element;
}> = (props) => {
  const c = children(() => props.children);
  
  return (
    <div 
      class={`fixed inset-0 z-50 ${props.isOpen ? 'block' : 'hidden'}`}
      onClick={props.onClose}
    >
      <div class="fixed inset-0 bg-black bg-opacity-50" />
      <div class="flex items-center justify-center min-h-screen px-4">
        <div 
          class="bg-white rounded-lg shadow-xl max-w-md w-full p-6"
          onClick={(e) => e.stopPropagation()}
        >
          {props.title && (
            <div class="flex justify-between items-center mb-4">
              <h3 class="text-lg font-semibold">{props.title}</h3>
              <button 
                onClick={props.onClose}
                class="text-gray-400 hover:text-gray-600"
              >
                ×
              </button>
            </div>
          )}
          <div>{c()}</div>
        </div>
      </div>
    </div>
  );
};
```

### 2. レイアウトシステム

```typescript
// レイアウトコンポーネント
export const Layout: Component<{
  sidebar?: JSX.Element;
  header?: JSX.Element;
  footer?: JSX.Element;
  children: JSX.Element;
}> = (props) => {
  return (
    <div class="flex h-screen bg-gray-100">
      {props.sidebar && (
        <aside class="w-64 bg-white shadow-md">
          {props.sidebar}
        </aside>
      )}
      
      <div class="flex-1 flex flex-col overflow-hidden">
        {props.header && (
          <header class="bg-white shadow-sm border-b">
            {props.header}
          </header>
        )}
        
        <main class="flex-1 overflow-auto p-6">
          {props.children}
        </main>
        
        {props.footer && (
          <footer class="bg-white border-t p-4">
            {props.footer}
          </footer>
        )}
      </div>
    </div>
  );
};
```

## XUL 統合

### 1. XUL ブリッジ

```typescript
// XUL システムとの統合
import { createSignal, onMount, onCleanup } from "solid-js";

// XUL 要素の監視
export function useXULElement(elementId: string) {
  const [element, setElement] = createSignal<XULElement | null>(null);
  
  onMount(() => {
    const xulElement = document.getElementById(elementId) as XULElement;
    if (xulElement) {
      setElement(xulElement);
      
      // XUL イベントリスナーの設定
      const handleXULEvent = (event: Event) => {
        // SolidJS の状態更新
        console.log('XUL event:', event);
      };
      
      xulElement.addEventListener('command', handleXULEvent);
      
      onCleanup(() => {
        xulElement.removeEventListener('command', handleXULEvent);
      });
    }
  });
  
  return element;
}

// XUL メニューの動的生成
export function createXULMenu(items: MenuItemConfig[]): void {
  const menubar = document.getElementById('main-menubar');
  if (!menubar) return;
  
  const menu = document.createXULElement('menu');
  menu.setAttribute('label', 'Floorp');
  
  const menupopup = document.createXULElement('menupopup');
  
  items.forEach(item => {
    const menuitem = document.createXULElement('menuitem');
    menuitem.setAttribute('label', item.label);
    menuitem.setAttribute('oncommand', item.command);
    menupopup.appendChild(menuitem);
  });
  
  menu.appendChild(menupopup);
  menubar.appendChild(menu);
}
```

### 2. Firefox API 統合

```typescript
// Firefox の内部 API との統合
declare global {
  interface Window {
    gBrowser: any;
    BrowserApp: any;
    Services: any;
  }
}

// タブ管理
export class TabManager {
  static getCurrentTab() {
    return window.gBrowser?.selectedTab;
  }
  
  static async openTab(url: string, options: { background?: boolean } = {}) {
    const tab = window.gBrowser?.addTab(url, {
      inBackground: options.background
    });
    
    if (!options.background) {
      window.gBrowser?.selectedTab = tab;
    }
    
    return tab;
  }
  
  static closeTab(tab: any) {
    window.gBrowser?.removeTab(tab);
  }
  
  static async getAllTabs() {
    return Array.from(window.gBrowser?.tabs || []);
  }
}

// ブックマーク管理
export class BookmarkManager {
  static async addBookmark(url: string, title: string, folder?: string) {
    const { PlacesUtils } = window.Services;
    
    return await PlacesUtils.bookmarks.insert({
      parentGuid: folder || PlacesUtils.bookmarks.unfiledGuid,
      url,
      title
    });
  }
  
  static async getBookmarks(folderId?: string) {
    const { PlacesUtils } = window.Services;
    
    const bookmarks = await PlacesUtils.bookmarks.fetch({
      parentGuid: folderId || PlacesUtils.bookmarks.unfiledGuid
    });
    
    return bookmarks;
  }
}
```

## 状態管理

### 1. グローバル状態

```typescript
// アプリケーション全体の状態管理
import { createSignal, createContext, useContext } from "solid-js";

// アプリケーション状態の型定義
interface AppState {
  user: {
    preferences: UserPreferences;
    session: SessionData;
  };
  ui: {
    theme: string;
    language: string;
    layout: LayoutConfig;
  };
  browser: {
    tabs: TabData[];
    bookmarks: BookmarkData[];
    history: HistoryEntry[];
  };
}

// 状態管理コンテキスト
const AppStateContext = createContext<{
  state: () => AppState;
  setState: (updater: Partial<AppState> | ((prev: AppState) => AppState)) => void;
}>();

// 状態プロバイダー
export function AppStateProvider(props: { children: any }) {
  const [state, setState] = createSignal<AppState>(getInitialState());
  
  const contextValue = {
    state,
    setState: (updater: any) => {
      if (typeof updater === 'function') {
        setState(updater);
      } else {
        setState(prev => ({ ...prev, ...updater }));
      }
    }
  };
  
  return (
    <AppStateContext.Provider value={contextValue}>
      {props.children}
    </AppStateContext.Provider>
  );
}

// 状態フック
export function useAppState() {
  const context = useContext(AppStateContext);
  if (!context) {
    throw new Error('useAppState must be used within AppStateProvider');
  }
  return context;
}
```

### 2. 永続化

```typescript
// 状態の永続化
export class StateManager {
  private static readonly STORAGE_KEY = 'floorp-app-state';
  
  // 状態の保存
  static async saveState(state: AppState): Promise<void> {
    try {
      const serialized = JSON.stringify(state);
      await window.Services.prefs.setStringPref(this.STORAGE_KEY, serialized);
    } catch (error) {
      console.error('Failed to save state:', error);
    }
  }
  
  // 状態の読み込み
  static async loadState(): Promise<AppState | null> {
    try {
      const serialized = window.Services.prefs.getStringPref(this.STORAGE_KEY, '');
      if (serialized) {
        return JSON.parse(serialized);
      }
    } catch (error) {
      console.error('Failed to load state:', error);
    }
    return null;
  }
  
  // 状態のリセット
  static async resetState(): Promise<void> {
    try {
      window.Services.prefs.clearUserPref(this.STORAGE_KEY);
    } catch (error) {
      console.error('Failed to reset state:', error);
    }
  }
}
```

## パフォーマンス最適化

### 1. 遅延読み込み

```typescript
// コンポーネントの遅延読み込み
import { lazy, Suspense } from "solid-js";

// 遅延読み込みコンポーネント
const LazySettingsPanel = lazy(() => import("./components/SettingsPanel"));
const LazyBookmarkManager = lazy(() => import("./components/BookmarkManager"));

// 使用例
function App() {
  return (
    <div>
      <Suspense fallback={<div>Loading settings...</div>}>
        <LazySettingsPanel />
      </Suspense>
      
      <Suspense fallback={<div>Loading bookmarks...</div>}>
        <LazyBookmarkManager />
      </Suspense>
    </div>
  );
}
```

### 2. メモ化

```typescript
// 計算結果のメモ化
import { createMemo, createSignal } from "solid-js";

function ExpensiveComponent() {
  const [data, setData] = createSignal([]);
  
  // 重い計算のメモ化
  const processedData = createMemo(() => {
    return data().map(item => {
      // 重い処理
      return processItem(item);
    });
  });
  
  return (
    <div>
      {processedData().map(item => (
        <div key={item.id}>{item.name}</div>
      ))}
    </div>
  );
}
```

## テスト戦略

### 1. ユニットテスト

```typescript
// Vitest を使用したテスト
import { describe, it, expect } from "vitest";
import { render } from "@solidjs/testing-library";
import { Button } from "../components/Button";

describe("Button Component", () => {
  it("renders correctly", () => {
    const { getByText } = render(() => <Button>Click me</Button>);
    expect(getByText("Click me")).toBeInTheDocument();
  });
  
  it("handles click events", () => {
    let clicked = false;
    const { getByText } = render(() => 
      <Button onClick={() => clicked = true}>Click me</Button>
    );
    
    getByText("Click me").click();
    expect(clicked).toBe(true);
  });
});
```

### 2. 統合テスト

```typescript
// モジュール統合テスト
import { describe, it, expect, beforeEach } from "vitest";
import { ModuleManager } from "../core/modules";

describe("Module System", () => {
  beforeEach(() => {
    ModuleManager.reset();
  });
  
  it("loads modules in correct order", async () => {
    const loadOrder: string[] = [];
    
    const moduleA = {
      id: "a",
      dependencies: ["b"],
      initialize: async () => loadOrder.push("a")
    };
    
    const moduleB = {
      id: "b", 
      dependencies: [],
      initialize: async () => loadOrder.push("b")
    };
    
    ModuleManager.register(moduleA);
    ModuleManager.register(moduleB);
    
    await ModuleManager.initialize();
    
    expect(loadOrder).toEqual(["b", "a"]);
  });
});
```

このメインアプリケーションは Floorp ブラウザの中核として、モジュラーで拡張可能な設計により、複雑なブラウザ機能を効率的に管理しています。