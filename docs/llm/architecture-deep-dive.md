# Floorp アーキテクチャ詳細ガイド

このドキュメントは、Floorp プロジェクトのアーキテクチャを深く理解するための技術的なガイドです。

## 目次

1. [全体アーキテクチャ](#全体アーキテクチャ)
2. [レイヤー構造](#レイヤー構造)
3. [コンポーネント通信](#コンポーネント通信)
4. [ビルドシステムの内部](#ビルドシステムの内部)
5. [ランタイムアーキテクチャ](#ランタイムアーキテクチャ)
6. [主要機能の実装パターン](#主要機能の実装パターン)
7. [拡張性と保守性](#拡張性と保守性)

---

## 全体アーキテクチャ

### システム構成図

```
┌─────────────────────────────────────────────────────────────┐
│                      Floorp Browser                         │
├─────────────────────────────────────────────────────────────┤
│  ┌───────────────────┐  ┌───────────────────┐             │
│  │  React Pages      │  │  SolidJS Chrome   │             │
│  │  (Settings, etc)  │  │  (UI Features)    │             │
│  └─────────┬─────────┘  └─────────┬─────────┘             │
│            │                       │                        │
│            └───────────┬───────────┘                        │
│                        │                                    │
│            ┌───────────▼───────────┐                       │
│            │   Bridge Layer        │                       │
│            │   (Feature Loader)    │                       │
│            └───────────┬───────────┘                       │
│                        │                                    │
│            ┌───────────▼───────────┐                       │
│            │   ESM Modules         │                       │
│            │   (.sys.mts)          │                       │
│            └───────────┬───────────┘                       │
│                        │                                    │
├────────────────────────┼────────────────────────────────────┤
│                        │                                    │
│            ┌───────────▼───────────┐                       │
│            │   Firefox Base        │                       │
│            │   (Gecko Engine)      │                       │
│            └───────────────────────┘                       │
└─────────────────────────────────────────────────────────────┘

開発時:
┌─────────────────────────────────────────────────────────────┐
│                   Development System                        │
├─────────────────────────────────────────────────────────────┤
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐  │
│  │ Vite Server 1 │  │ Vite Server 2 │  │ Vite Server N │  │
│  │ (Port 5173)   │  │ (Port 5178)   │  │ (Port 518X)   │  │
│  └───────┬───────┘  └───────┬───────┘  └───────┬───────┘  │
│          │                   │                   │          │
│          └───────────────────┼───────────────────┘          │
│                              │                              │
│                  ┌───────────▼───────────┐                 │
│                  │  Hot Module Reload    │                 │
│                  │  (WebSocket)          │                 │
│                  └───────────┬───────────┘                 │
│                              │                              │
│                  ┌───────────▼───────────┐                 │
│                  │  Firefox Browser      │                 │
│                  │  (Development Mode)   │                 │
│                  └───────────────────────┘                 │
└─────────────────────────────────────────────────────────────┘
```

### データフロー

```
User Action
    │
    ▼
┌─────────────────┐
│ UI Component    │ (SolidJS/React)
│ (Click, Input)  │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Event Handler   │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Service Layer   │ (ビジネスロジック)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ ESM Module      │ (.sys.mts - Firefox API アクセス)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Firefox API     │ (Services, Components, etc)
└────────┬────────┘
         │
         ▼
    Result
         │
         ▼
┌─────────────────┐
│ State Update    │ (SolidJS Signals/React State)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ UI Re-render    │
└─────────────────┘
```

---

## レイヤー構造

Floorp のアーキテクチャは以下の 5 つの主要レイヤーに分かれています：

### Layer 1: Firefox Base Layer

**場所**: 外部（Mozilla Firefox）

**役割**:
- Gecko レンダリングエンジン
- ネットワークスタック
- セキュリティサンドボックス
- 基本的なブラウザ機能

**Floorp との接点**:
- パッチ適用（`tools/patches/`）
- プリファレンスオーバーライド（`static/gecko/pref/`）

### Layer 2: ESM Module Layer

**場所**: `browser-features/modules/`

**役割**:
- Firefox API への橋渡し
- システムレベルの機能提供
- Window Actor による親子プロセス通信

**特徴**:
- `.sys.mts` 拡張子（Firefox ESM）
- `ChromeUtils.importESModule()` でロード
- `resource://` プロトコルでアクセス

**主要モジュール**:

```typescript
// NoranekoStartup.sys.mts
// ブラウザ起動時の初期化
export const NoranekoStartup = {
  async init() {
    // Window Actor の登録
    // About ページの登録
    // 実験の初期化
  }
};

// BrowserGlue.sys.mts
// Window Actor の定義
ActorManagerParent.addActors({
  FeatureName: {
    parent: { esModuleURI: "..." },
    child: { esModuleURI: "..." }
  }
});
```

### Layer 3: Bridge Layer

**場所**: `bridge/`

**役割**:
- ESM モジュールとブラウザクローム機能を接続
- 開発モードでの HMR サポート
- リソースのロードと初期化

**コンポーネント**:

1. **Startup Scripts** (`bridge/startup/`)
   - Firefox 起動時に実行
   - 基本的な環境設定

2. **Feature Loader** (`bridge/loader-features/`)
   - ブラウザクローム機能のロード
   - HMR サポート
   - 開発/本番モードの切り替え

3. **Module Loader** (`bridge/loader-modules/`)
   - ESM モジュールの動的ロード
   - 依存関係の解決

### Layer 4: Browser Chrome Layer

**場所**: `browser-features/chrome/`

**役割**:
- ブラウザ UI のカスタマイズ
- ユーザーインタラクション処理
- ビジュアル機能の実装

**技術スタック**: SolidJS

**アーキテクチャパターン**:

```typescript
// Service-Controller-Config パターン

// Config - 設定管理
export class FeatureConfig {
  get enabled(): boolean { /* ... */ }
  set enabled(value: boolean) { /* ... */ }
}

// Service - ビジネスロジック
export class FeatureService {
  constructor(private config: FeatureConfig) {}

  async doSomething() { /* ... */ }
}

// Controller - UI と Service の接続
@noraComponent(import.meta.hot)
export default class Feature extends NoraComponentBase {
  private service: FeatureService;

  init(): void {
    this.service = new FeatureService(new FeatureConfig());
    this.setupUI();
  }

  private setupUI(): void {
    // SolidJS コンポーネントの作成
  }
}
```

### Layer 5: Page Layer

**場所**: `browser-features/pages-*`

**役割**:
- 設定ページなどのフルページ UI
- フォームヘビーなインターフェース

**技術スタック**: React + Tailwind CSS

**例**: 設定ページ（`pages-settings/`）

```typescript
// src/pages/Dashboard.tsx
export default function Dashboard() {
  return (
    <div className="p-4">
      <h1>Dashboard</h1>
      {/* React コンポーネント */}
    </div>
  );
}
```

---

## コンポーネント通信

### 1. Window Actor による親子プロセス通信

Firefox のマルチプロセスアーキテクチャ（e10s）における通信パターン。

```typescript
// Parent Actor (browser-features/modules/actors/MyFeatureParent.sys.mts)
export class MyFeatureParent extends JSWindowActorParent {
  receiveMessage(message: ReceiveMessageArgument) {
    switch (message.name) {
      case "MyFeature:DoSomething":
        // 親プロセスでの処理
        return this.handleDoSomething(message.data);
    }
  }

  private handleDoSomething(data: any) {
    // Firefox API にアクセス（親プロセスのみ可能）
    const { Services } = ChromeUtils.importESModule(
      "resource://gre/modules/Services.sys.mjs"
    );

    return Services.something.doThing(data);
  }
}

// Child Actor (browser-features/modules/actors/MyFeatureChild.sys.mts)
export class MyFeatureChild extends JSWindowActorChild {
  handleEvent(event: Event) {
    if (event.type === "DOMContentLoaded") {
      this.init();
    }
  }

  private init() {
    // 子プロセス（コンテンツプロセス）での初期化
  }

  async doSomething(data: any) {
    // 親プロセスにメッセージを送信
    const result = await this.sendQuery("MyFeature:DoSomething", data);
    return result;
  }
}
```

**Actor の登録** (`BrowserGlue.sys.mts`):

```typescript
ActorManagerParent.addActors({
  MyFeature: {
    parent: {
      esModuleURI: "resource://noraneko/actors/MyFeatureParent.sys.mjs",
    },
    child: {
      esModuleURI: "resource://noraneko/actors/MyFeatureChild.sys.mjs",
      events: {
        DOMContentLoaded: {},
      },
    },
    messageManagerGroups: ["browsers"],
  },
});
```

### 2. ブラウザクローム機能間の通信

**パターン 1: イベントバス**

```typescript
// イベントバスの実装
class EventBus {
  private listeners = new Map<string, Set<Function>>();

  on(event: string, callback: Function) {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event)!.add(callback);
  }

  emit(event: string, data?: any) {
    const callbacks = this.listeners.get(event);
    if (callbacks) {
      callbacks.forEach(callback => callback(data));
    }
  }
}

export const eventBus = new EventBus();

// 使用例
// Feature A
eventBus.emit("workspace-changed", { workspaceId: 123 });

// Feature B
eventBus.on("workspace-changed", (data) => {
  console.log("Workspace changed:", data.workspaceId);
});
```

**パターン 2: サービスの共有**

```typescript
// 共有サービスの定義
export class WorkspaceService {
  private static instance: WorkspaceService;

  static getInstance(): WorkspaceService {
    if (!this.instance) {
      this.instance = new WorkspaceService();
    }
    return this.instance;
  }

  getCurrentWorkspace() { /* ... */ }
}

// 使用例
// Feature A
const workspaceService = WorkspaceService.getInstance();
const workspace = workspaceService.getCurrentWorkspace();

// Feature B
const workspaceService = WorkspaceService.getInstance(); // 同じインスタンス
```

### 3. React ページと ESM モジュール間の通信

```typescript
// React コンポーネント内
import { useState, useEffect } from "react";

function SettingsPage() {
  const [config, setConfig] = useState(null);

  useEffect(() => {
    // ESM モジュールから設定を取得
    window.sendAsyncMessage("GetConfig").then(setConfig);
  }, []);

  const handleSave = () => {
    // ESM モジュールに保存を依頼
    window.sendAsyncMessage("SaveConfig", config);
  };

  return <div>{/* UI */}</div>;
}
```

---

## ビルドシステムの内部

### feles-build の動作フロー

```
deno task dev
    │
    ▼
┌─────────────────────────────────────────┐
│ 1. Firefox バイナリのダウンロード/確認  │
│    - GitHub Actions から取得            │
│    - キャッシュチェック                 │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│ 2. パッチ適用                           │
│    - tools/patches/ のパッチを適用      │
│    - Firefox ソースコードの修正         │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│ 3. シンボリックリンクの作成             │
│    - リソースファイルをリンク           │
│    - _dist/ ディレクトリの準備          │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│ 4. Vite 開発サーバーの起動              │
│    - 8つの並行サーバー                  │
│    - HMR WebSocket の設定               │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│ 5. マニフェストの注入                   │
│    - chrome.manifest の生成             │
│    - リソース登録                       │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│ 6. ブラウザの起動                       │
│    - 適切なプロファイルで起動           │
│    - デバッグフラグの設定               │
└─────────────────────────────────────────┘
```

### Vite 設定の詳細

各機能は独自の Vite 設定を持ちます：

```typescript
// browser-features/chrome/vite.config.ts
export default defineConfig({
  plugins: [
    solidPlugin(),
    genJarMn(), // JAR マニフェスト生成
    disableCSP(), // 開発時の CSP 無効化
  ],
  build: {
    lib: {
      entry: "./common/mod.ts",
      formats: ["es"],
    },
    rollupOptions: {
      output: {
        entryFileNames: "content/[name].js",
        chunkFileNames: "content/[name].js",
      },
    },
  },
  resolve: {
    alias: {
      "#chrome": "./common",
      "#libs": "../../libs",
      // ...
    },
  },
  server: {
    port: 5173,
    hmr: {
      protocol: "ws",
      host: "localhost",
    },
  },
});
```

### HMR の仕組み

```typescript
// @noraComponent デコレーターの実装
export function noraComponent(hot?: ImportMeta["hot"]) {
  return function <T extends { new (...args: any[]): NoraComponentBase }>(
    constructor: T
  ) {
    // HMR サポート
    if (hot) {
      hot.accept((newModule) => {
        if (newModule) {
          // 新しいモジュールで既存のインスタンスを置き換え
          const instance = new newModule.default();
          instance.init();
        }
      });

      hot.dispose(() => {
        // クリーンアップ
      });
    }

    return constructor;
  };
}

// NoraComponentBase
export abstract class NoraComponentBase {
  abstract init(): void;

  // HMR 時に呼ばれる
  protected onHotReload?(): void;
}
```

---

## ランタイムアーキテクチャ

### 起動シーケンス

```
Firefox 起動
    │
    ▼
┌──────────────────────────────────────┐
│ 1. Firefox の初期化                  │
└──────────────┬───────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│ 2. NoranekoStartup.sys.mts のロード  │
│    - Window Actor の登録             │
│    - About ページの登録              │
└──────────────┬───────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│ 3. Bridge Layer の初期化             │
│    - Feature Loader の起動           │
│    - Module Loader の起動            │
└──────────────┬───────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│ 4. ブラウザクローム機能のロード      │
│    - mod.ts の実行                   │
│    - 各機能の init() 呼び出し        │
└──────────────┬───────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│ 5. Experiments の初期化              │
│    - A/B テストの設定読み込み        │
│    - バリアントの決定                │
└──────────────┬───────────────────────┘
               │
               ▼
┌──────────────────────────────────────┐
│ 6. ブラウザウィンドウの表示          │
└──────────────────────────────────────┘
```

### メモリレイアウト

```
┌─────────────────────────────────────────┐
│ Parent Process (Main Process)           │
├─────────────────────────────────────────┤
│ - ESM Modules (.sys.mts)                │
│ - Window Actor Parents                  │
│ - Firefox Services                      │
│ - Experiments Manager                   │
│ - Workspace Manager                     │
└─────────────────┬───────────────────────┘
                  │
    ┌─────────────┼─────────────┐
    │             │             │
    ▼             ▼             ▼
┌─────────┐  ┌─────────┐  ┌─────────┐
│Content 1│  │Content 2│  │Content N│
│Process  │  │Process  │  │Process  │
├─────────┤  ├─────────┤  ├─────────┤
│ - Actor │  │ - Actor │  │ - Actor │
│   Child │  │   Child │  │   Child │
│ - Web   │  │ - Web   │  │ - Web   │
│   Page  │  │   Page  │  │   Page  │
└─────────┘  └─────────┘  └─────────┘
```

---

## 主要機能の実装パターン

### 1. Workspaces（ワークスペース）

**アーキテクチャ**:

```
┌─────────────────────────────────────────────┐
│ Workspace UI (SolidJS)                      │
│ - ツールバーボタン                          │
│ - ワークスペース切り替え UI                 │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ WorkspaceService                            │
│ - ワークスペースの作成/削除                 │
│ - タブの移動/コピー                         │
│ - 状態の永続化                              │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ Storage Layer                               │
│ - Preferences API (about:config)            │
│ - SessionStore API                          │
└─────────────────────────────────────────────┘
```

**データ構造**:

```typescript
interface Workspace {
  id: string;
  name: string;
  iconUrl?: string;
  tabs: TabInfo[];
  preferences?: Record<string, any>;
}

interface TabInfo {
  url: string;
  title: string;
  favIconUrl?: string;
  pinned: boolean;
}
```

### 2. PWA (Progressive Web Apps)

**アーキテクチャ**:

```
┌─────────────────────────────────────────────┐
│ PWA UI (browser-features/chrome/common/pwa)│
│ - インストールボタン                        │
│ - PWA 管理 UI                               │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ PWA Service                                 │
│ - マニフェストの解析                        │
│ - アイコンのダウンロードと処理              │
│ - ショートカットの作成                      │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ PWA Module (.sys.mts)                       │
│ - Firefox API との統合                      │
│ - SSB (Site-Specific Browser) の作成       │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ OS Integration                              │
│ - ショートカット作成 (Windows/Linux/macOS) │
│ - タスクバーピン (Windows)                  │
│ - アプリケーションメニュー (macOS)          │
└─────────────────────────────────────────────┘
```

**マニフェスト処理**:

```typescript
interface WebAppManifest {
  name: string;
  short_name?: string;
  description?: string;
  start_url: string;
  display: "standalone" | "fullscreen" | "minimal-ui" | "browser";
  icons: ManifestIcon[];
  theme_color?: string;
  background_color?: string;
}

interface ManifestIcon {
  src: string;
  sizes: string;
  type: string;
  purpose?: "any" | "maskable" | "monochrome";
}
```

### 3. Experiments (A/B Testing)

**アーキテクチャ**:

```
┌─────────────────────────────────────────────┐
│ Application Code                            │
│ - getVariant("experiment_name")             │
│ - getConfig("experiment_name")              │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ Experiments Module                          │
│ - バリアントの決定（FNV-1a ハッシュ）       │
│ - ロールアウト制御                          │
│ - 設定のキャッシュ                          │
└──────────────┬──────────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────────┐
│ Storage (Preferences)                       │
│ - experiments.{name}.variant                │
│ - experiments.{name}.config                 │
└─────────────────────────────────────────────┘
```

**バリアント決定アルゴリズム**:

```typescript
function assignVariant(
  installId: string,
  experimentId: string,
  variants: Variant[]
): string {
  // 1. ハッシュ計算
  const hash = fnv1a(installId + experimentId);

  // 2. 0-1 の範囲に正規化
  const normalized = (hash % 1000000) / 1000000;

  // 3. 重み付き選択
  let cumulative = 0;
  for (const variant of variants) {
    cumulative += variant.weight;
    if (normalized < cumulative) {
      return variant.name;
    }
  }

  return "control"; // フォールバック
}
```

---

## 拡張性と保守性

### 1. 新しい機能の追加

**手順**:

1. **ディレクトリ作成**:
   ```
   browser-features/chrome/common/my-feature/
   ├── index.ts           # メインエントリポイント
   ├── service.ts         # ビジネスロジック
   ├── config.ts          # 設定管理
   └── ui.tsx             # UI コンポーネント (SolidJS)
   ```

2. **機能の実装**:
   ```typescript
   // index.ts
   @noraComponent(import.meta.hot)
   export default class MyFeature extends NoraComponentBase {
     private service: MyFeatureService;

     init(): void {
       this.service = new MyFeatureService();
       this.setupUI();
     }

     private setupUI(): void {
       // UI の初期化
     }
   }
   ```

3. **機能の登録**:
   ```typescript
   // browser-features/chrome/common/mod.ts
   import MyFeature from "./my-feature";

   // features 配列に追加
   const features = [
     // ...existing features
     MyFeature,
   ];
   ```

4. **必要に応じて Actor を追加**:
   ```typescript
   // browser-features/modules/actors/MyFeatureParent.sys.mts
   export class MyFeatureParent extends JSWindowActorParent {
     // ...
   }

   // browser-features/modules/actors/MyFeatureChild.sys.mts
   export class MyFeatureChild extends JSWindowActorChild {
     // ...
   }

   // BrowserGlue.sys.mts で登録
   ActorManagerParent.addActors({
     MyFeature: {
       parent: { esModuleURI: "..." },
       child: { esModuleURI: "..." }
     }
   });
   ```

5. **翻訳の追加**:
   ```json
   // i18n/en-US/my-feature.json
   {
     "myFeature.title": "My Feature",
     "myFeature.description": "This is my feature"
   }
   ```

### 2. プラグインシステム（将来的な拡張）

現在は実装されていませんが、以下のようなプラグインシステムが考えられます：

```typescript
// プラグインインターフェース
interface Plugin {
  name: string;
  version: string;
  init(context: PluginContext): void;
  destroy(): void;
}

interface PluginContext {
  // プラグインが使用できる API
  registerCommand(name: string, handler: Function): void;
  registerUI(location: string, component: Component): void;
  getPreference(key: string): any;
  setPreference(key: string, value: any): void;
}

// プラグインマネージャー
class PluginManager {
  private plugins = new Map<string, Plugin>();

  register(plugin: Plugin): void {
    const context = this.createContext();
    plugin.init(context);
    this.plugins.set(plugin.name, plugin);
  }

  unregister(name: string): void {
    const plugin = this.plugins.get(name);
    if (plugin) {
      plugin.destroy();
      this.plugins.delete(name);
    }
  }
}
```

### 3. テスタビリティ

**ユニットテストの例**:

```typescript
// browser-features/chrome/test/unit/workspace.test.ts
import { assertEquals } from "@std/assert";
import { WorkspaceService } from "../../common/workspaces/service.ts";

Deno.test("WorkspaceService - create workspace", () => {
  const service = new WorkspaceService();
  const workspace = service.createWorkspace("Test");

  assertEquals(workspace.name, "Test");
  assertEquals(workspace.tabs.length, 0);
});

Deno.test("WorkspaceService - add tab to workspace", () => {
  const service = new WorkspaceService();
  const workspace = service.createWorkspace("Test");

  service.addTab(workspace.id, {
    url: "https://example.com",
    title: "Example",
  });

  assertEquals(workspace.tabs.length, 1);
  assertEquals(workspace.tabs[0].url, "https://example.com");
});
```

**モック使用の例**:

```typescript
// テスト用モック
class MockFirefoxAPI {
  private prefs = new Map<string, any>();

  getPref(key: string): any {
    return this.prefs.get(key);
  }

  setPref(key: string, value: any): void {
    this.prefs.set(key, value);
  }
}

// 依存性注入
class WorkspaceService {
  constructor(private api: FirefoxAPI = realFirefoxAPI) {}

  saveWorkspace(workspace: Workspace): void {
    this.api.setPref(`workspace.${workspace.id}`, JSON.stringify(workspace));
  }
}

// テスト
Deno.test("WorkspaceService - save workspace", () => {
  const mockAPI = new MockFirefoxAPI();
  const service = new WorkspaceService(mockAPI);

  service.saveWorkspace({ id: "1", name: "Test", tabs: [] });

  const saved = mockAPI.getPref("workspace.1");
  assertEquals(JSON.parse(saved).name, "Test");
});
```

---

## まとめ

Floorp のアーキテクチャは以下の特徴を持ちます：

1. **レイヤー化**: 明確な責任分離
2. **拡張性**: 新機能の追加が容易
3. **保守性**: 各レイヤーが独立してテスト可能
4. **パフォーマンス**: 遅延ロードと最適化
5. **開発体験**: HMR による高速な開発サイクル

このアーキテクチャにより、Firefox のコア機能を維持しながら、独自の機能を安全に追加できます。

---

## 参考リンク

- [プロジェクト概要](./project-overview.md)
- [開発ノート](./development-notes.md)
- [Experiments フレームワーク](../experiment/)
- [Firefox Actor ドキュメント](https://firefox-source-docs.mozilla.org/dom/ipc/jsactors.html)

---

最終更新: 2025-01-30
