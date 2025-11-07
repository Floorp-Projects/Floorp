# 開発ノート - 重要なポイントと注意事項

このドキュメントは、Floorp プロジェクトで開発する際に知っておくべき重要な情報、よくある落とし穴、ベストプラクティスをまとめたものです。

## 目次

1. [開発環境のセットアップ](#開発環境のセットアップ)
2. [アーキテクチャ上の重要な決定](#アーキテクチャ上の重要な決定)
3. [よくある問題と解決策](#よくある問題と解決策)
4. [ベストプラクティス](#ベストプラクティス)
5. [パフォーマンスに関する考慮事項](#パフォーマンスに関する考慮事項)
6. [デバッグとトラブルシューティング](#デバッグとトラブルシューティング)
7. [今後の改善提案](#今後の改善提案)

---

## 開発環境のセットアップ

### 必須要件

1. **Deno 2.x**
   - プライマリランタイム
   - `deno install` で依存関係をインストール
   - Deno の権限システムに注意（`--allow-all` が必要な場合がある）

2. **PowerShell 7（Windows）**
   - ビルドスクリプトは PowerShell 7 を前提
   - Windows PowerShell 5.x では動作しない可能性

3. **ディスク容量**
   - 最低 2.5GB（Firefox バイナリダウンロード用）
   - ビルドアーティファクトで追加容量が必要

### 初回セットアップ時の注意

```bash
# 1. 依存関係のインストール
deno install

# 2. 開発モードでの起動
deno task dev
```

**重要**: 初回起動時は Firefox バイナリのダウンロードに時間がかかります（約 2.5GB）。キャッシュされた後は高速になります。

### 開発サーバーのポート

開発モードでは以下のポートが使用されます：

| ポート    | 用途                                               |
| --------- | -------------------------------------------------- |
| 5173      | メイン機能                                         |
| 5178      | 設定ページ                                         |
| 5186      | 新しいタブページ                                   |
| 5174-5177 | その他のページ（ウェルカム、ノート、モーダルなど） |

**注意**: これらのポートが他のプロセスで使用されていないことを確認してください。

---

## アーキテクチャ上の重要な決定

### 1. Deno vs Node.js の使い分け

**Deno を使用する場合**:

- ビルドツール（`tools/` 内のスクリプト）
- 新しいユーティリティやスクリプト
- 型安全性が重要な場合

**Node.js を使用する場合**:

- 一部の既存依存関係（`package.json` 内）
- Deno で互換性がないパッケージ

**ベストプラクティス**: 新しいコードは Deno で書くことを推奨。

### 2. SolidJS vs React の使い分け

**SolidJS を使用する場合**（`browser-features/chrome/`）:

- ブラウザクローム（UI）機能
- XUL のような動作が必要な場合
- パフォーマンスが重要な UI コンポーネント
- リアクティブな状態管理が必要な場合

**React を使用する場合**（`browser-features/pages-*`）:

- 設定ページ
- 新しいタブページ
- フォームヘビーなインターフェース
- 既存の React エコシステムを活用したい場合

### 3. `.sys.mts` vs `.ts` ファイル

**`.sys.mts`（`browser-features/modules/`）**:

- Firefox の ESM モジュールシステムに統合
- `ChromeUtils.importESModule()` でロード
- Firefox の chrome: プロトコルからアクセス可能
- Firefox の API（Services、Components など）に直接アクセス

**`.ts`（`browser-features/chrome/`）**:

- Vite でバンドルされる通常の TypeScript
- ブラウザクローム機能用
- HMR サポート
- モダンな Web API を使用

**重要**: Firefox の内部 API にアクセスする必要がある場合は `.sys.mts` を使用。それ以外は `.ts` を推奨。

### 4. モジュールパスのエイリアス

プロジェクトは以下のパスエイリアスを使用：

```typescript
import { foo } from "#i18n/utils"; // i18n リソース
import { bar } from "#chrome/common/tab"; // Chrome 機能
import { baz } from "#libs/shared"; // 共有ライブラリ
import { qux } from "#modules/experiments"; // システムモジュール
```

**設定場所**:

- Deno: [`deno.json`](../../deno.json) の `imports`
- Vite: 各 `vite.config.ts` の `resolve.alias`

**注意**: エイリアスの追加・変更時は両方の設定ファイルを更新してください。

---

## よくある問題と解決策

### 問題 1: HMR が動作しない

**症状**: コードを変更してもブラウザに反映されない。

**原因**:

- `@noraComponent` デコレーターが付いていない
- `NoraComponentBase` を継承していない
- Vite 開発サーバーが起動していない

**解決策**:

```typescript
@noraComponent(import.meta.hot) // これを忘れずに！
export default class MyFeature extends NoraComponentBase {
  init(): void {
    // ...
  }
}
```

### 問題 2: Firefox API にアクセスできない

**症状**: `Services`、`Components`、`ChromeUtils` などが undefined。

**原因**: `.ts` ファイルから Firefox API にアクセスしようとしている。

**解決策**: `.sys.mts` モジュールを作成し、そこから Firefox API にアクセス：

```typescript
// browser-features/modules/modules/MyModule.sys.mts
export const MyModule = {
  doSomething() {
    const { Services } = ChromeUtils.importESModule(
      "resource://gre/modules/Services.sys.mjs",
    );
    // Firefox API を使用
  },
};
```

### 問題 3: ビルドが失敗する

**症状**: `deno task dev` や `deno task build` が失敗する。

**チェックリスト**:

1. Deno のバージョンを確認（2.x が必要）
2. ポートが他のプロセスで使用されていないか確認
3. ディスク容量を確認（最低 2.5GB）
4. 依存関係を再インストール: `deno install`
5. キャッシュをクリア: `deno cache --reload tools/feles-build.ts`

### 問題 4: 翻訳が表示されない

**症状**: UI にキー（例: `browser.tab.close`）が表示される。

**原因**:

- 翻訳ファイルが存在しない
- i18n の初期化が完了していない
- ロケールが正しく設定されていない

**解決策**:

1. 翻訳ファイルを確認: `i18n/{locale}/`
2. キーが正しく定義されているか確認
3. `NRI18n` モジュールが正しく初期化されているか確認

### 問題 5: Window Actor が機能しない

**症状**: Parent-Child 間の通信が動作しない。

**チェックリスト**:

1. Actor が `BrowserGlue.sys.mts` で登録されているか確認
2. `allFrames` と `includeChrome` の設定を確認
3. Parent と Child の両方が実装されているか確認
4. メッセージ名が一致しているか確認

**例**:

```typescript
// BrowserGlue.sys.mts
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
  },
});
```

---

## ベストプラクティス

### 1. 新しい機能を追加する際のチェックリスト

1. **適切な場所に配置**:
   - ブラウザ UI 機能 → `browser-features/chrome/common/`
   - システムモジュール → `browser-features/modules/modules/`
   - 設定ページ → `browser-features/pages-settings/`
   - 新しいページ → `browser-features/pages-{name}/`

2. **HMR サポートを追加**:

   ```typescript
   @noraComponent(import.meta.hot)
   export default class MyFeature extends NoraComponentBase {
     init(): void {
       /* ... */
     }
   }
   ```

3. **機能を登録**:
   - `browser-features/chrome/common/mod.ts` に追加

4. **必要に応じて Actor を追加**:
   - Parent: `browser-features/modules/actors/{Name}Parent.sys.mts`
   - Child: `browser-features/modules/actors/{Name}Child.sys.mts`
   - 登録: `browser-features/modules/modules/BrowserGlue.sys.mts`

5. **翻訳を追加**:
   - 少なくとも `i18n/en-US/` と `i18n/ja-JP/` に追加
   - 他の言語は Crowdin で管理

6. **設定を追加**（必要に応じて）:
   - `static/gecko/pref/override.ini` に追加

### 2. コーディングスタイル

#### TypeScript

```typescript
// Good: 明確な型定義
interface FeatureConfig {
  enabled: boolean;
  timeout: number;
}

class MyFeature {
  private config: FeatureConfig;

  constructor(config: FeatureConfig) {
    this.config = config;
  }
}

// Bad: any 型の乱用
class MyFeature {
  private config: any; // 避けるべき
}
```

#### SolidJS コンポーネント

```typescript
// Good: リアクティブな状態管理
import { createSignal } from "solid-js";

const [count, setCount] = createSignal(0);

function increment() {
  setCount((c) => c + 1);
}

// Bad: 直接変更
let count = 0;

function increment() {
  count++; // SolidJS では動作しない
}
```

#### Firefox ESM モジュール

```typescript
// Good: 遅延インポート
export const MyModule = {
  doSomething() {
    const { Services } = ChromeUtils.importESModule(
      "resource://gre/modules/Services.sys.mjs",
    );
    return Services.appinfo.version;
  },
};

// Bad: トップレベルインポート（初期化時にロードされる）
const { Services } = ChromeUtils.importESModule(
  "resource://gre/modules/Services.sys.mjs",
);
```

### 3. パフォーマンス最適化

#### ブラウザ起動時間

- `init()` メソッドで重い処理を避ける
- 遅延初期化を活用
- 必要になるまで UI コンポーネントを作成しない

```typescript
@noraComponent(import.meta.hot)
export default class MyFeature extends NoraComponentBase {
  private _ui?: MyUI;

  init(): void {
    // 軽量な初期化のみ
    this.registerEventListeners();
  }

  private get ui(): MyUI {
    // 必要になった時に作成
    if (!this._ui) {
      this._ui = new MyUI();
    }
    return this._ui;
  }
}
```

#### メモリ使用量

- イベントリスナーを適切にクリーンアップ
- 不要な参照を保持しない
- WeakMap/WeakSet を活用

```typescript
class MyFeature {
  private listeners = new Map<Element, EventListener>();

  addListener(element: Element, listener: EventListener) {
    element.addEventListener("click", listener);
    this.listeners.set(element, listener);
  }

  cleanup() {
    // クリーンアップを忘れずに
    for (const [element, listener] of this.listeners) {
      element.removeEventListener("click", listener);
    }
    this.listeners.clear();
  }
}
```

### 4. エラーハンドリング

```typescript
// Good: 適切なエラーハンドリング
export const MyModule = {
  async fetchData(url: string) {
    try {
      const response = await fetch(url);
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }
      return await response.json();
    } catch (error) {
      console.error("Failed to fetch data:", error);
      // フォールバック処理
      return null;
    }
  },
};

// Bad: エラーを無視
export const MyModule = {
  async fetchData(url: string) {
    const response = await fetch(url); // 失敗する可能性がある
    return await response.json(); // エラーが伝播
  },
};
```

### 5. Workspaces の保存と復元

- **スナップショット取得**: `WorkspacesService.captureWorkspaceSnapshot` と `WorkspacesArchiveService.saveSnapshot` が、選択したワークスペースのタブ状態を `profile/workspaces/archive/` 配下に JSON として保存します。
- **保存/復元 API**: `WorkspacesService.archiveWorkspace()` がワークスペースを保存してタブを閉じ、`restoreArchivedWorkspace()` がファイルから復元し、必要なタブ属性（`floorpWorkspaceId` など）を再設定します。
- **UI 連携**: ツールバーポップアップのフッターにフォルダボタンを追加し、クリックで「復元モード」に切り替えて保存済みワークスペース一覧から復元できるようにしました（一覧は `listArchivedWorkspaces()` で動的取得）。

---

## パフォーマンスに関する考慮事項

### 1. ブラウザ起動時間への影響

**測定方法**:

```bash
# Firefox の起動時間を測定
deno task dev --measure-startup
```

**最適化のポイント**:

- `init()` メソッドを軽量に保つ
- 同期的な I/O を避ける
- 重い計算は遅延実行
- 不要なモジュールのロードを避ける

### 2. メモリフットプリント

**モニタリング**:

- Firefox の `about:memory` ページを活用
- `about:performance` でタブごとのメモリ使用量を確認

**最適化のポイント**:

- 大きなデータ構造をキャッシュする際は注意
- DOM ノードへの参照を保持し続けない
- イベントリスナーを適切にクリーンアップ

### 3. バンドルサイズ

**現在のバンドルサイズ**:

- メイン機能: 確認が必要
- 設定ページ: 確認が必要

**最適化のポイント**:

- 動的インポートを活用
- 不要な依存関係を削除
- Tree-shaking を最大限活用
- Vite のバンドル分析を使用: `vite build --analyze`

---

## デバッグとトラブルシューティング

### 1. ブラウザコンソール

Firefox の開発者ツールを活用：

- `Ctrl+Shift+J` (Windows/Linux) または `Cmd+Option+J` (macOS): ブラウザコンソール
- `Ctrl+Shift+I` (Windows/Linux) または `Cmd+Option+I` (macOS): ページコンソール

### 2. リモートデバッグ

```bash
# Firefox をデバッグモードで起動
deno task dev -- --remote-debugging-port=9222
```

その後、Chrome で `chrome://inspect` を開いて接続。

### 3. ログ出力

```typescript
// ブラウザコンソールにログ出力
console.log("[MyFeature]", "Initializing...");

// Firefox のログシステムを使用
const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  Log: "resource://gre/modules/Log.sys.mjs",
});

const logger = lazy.Log.repository.getLogger("Floorp.MyFeature");
logger.info("Initializing feature");
```

### 4. ブレークポイント

```typescript
// デバッガーをトリガー
debugger;

// 条件付きブレークポイント
if (someCondition) {
  debugger;
}
```

### 5. よく使うコマンド

```bash
# 開発サーバーを起動
deno task dev

# クリーンビルド
deno task clean && deno task dev

# ログを詳細に
deno task dev -- --log-level=debug

# 特定の機能を無効化してテスト
# (機能の init() メソッドをコメントアウト)
```

---

## 今後の改善提案

### 1. テストカバレッジの向上

**現状**: テストが限定的（`browser-features/chrome/test/unit/` のみ）

**提案**:

- ユニットテストの追加（各機能に対して）
- 統合テストの追加（複数機能の連携）
- E2E テストの追加（Puppeteer を活用）
- CI でのテスト自動実行

**優先度**: 高

### 2. ドキュメントの充実

**現状**: 一部の機能のみドキュメント化

**提案**:

- 各機能に README を追加
- API ドキュメントの自動生成（TypeDoc など）
- アーキテクチャ図の作成
- コントリビューションガイドの拡充

**優先度**: 中

### 3. パフォーマンス最適化

**提案**:

- バンドルサイズの分析と最適化
- 起動時間の継続的なモニタリング
- レイジーローディングの拡大
- メモリプロファイリングの定期実施

**優先度**: 中

### 4. 開発体験の向上

**提案**:

- ホットリロードの改善（より高速に）
- エラーメッセージの改善（より分かりやすく）
- デバッグツールの追加（開発者向け UI）
- セットアップスクリプトの改善

**優先度**: 中

### 5. CI/CD の改善

**提案**:

- ビルド時間の短縮（並列化、キャッシュ活用）
- プレビュービルドの自動作成（PR ごと）
- 自動化されたリリースノート生成
- ロールバック機能の整備

**優先度**: 低

### 6. コード品質

**提案**:

- ESLint の導入（現在は oxlint のみ）
- コードレビューチェックリストの作成
- 静的解析ツールの追加
- 循環依存の検出と修正

**優先度**: 低

---

## まとめ

Floorp プロジェクトは、モダンな技術スタックとよく設計されたアーキテクチャを持つ野心的なプロジェクトです。開発時は以下の点に注意してください：

1. **適切なツールを選択**: SolidJS vs React、.sys.mts vs .ts など
2. **HMR を活用**: 開発サイクルを高速化
3. **パフォーマンスを意識**: 起動時間とメモリ使用量
4. **テストを書く**: 品質を保証
5. **ドキュメントを更新**: 他の開発者のために

開発中に新しい発見や問題があれば、このドキュメントを更新してください。

---

## 参考リンク

- [プロジェクト概要](./project-overview.md)
- [Experiments フレームワーク](../experiment/)
- [Deno マニュアル](https://docs.deno.com/)
- [SolidJS ドキュメント](https://www.solidjs.com/)
- [Firefox Source Docs](https://firefox-source-docs.mozilla.org/)

---

最終更新: 2025-01-30
