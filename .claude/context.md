# Floorp プロジェクトコンテキスト

このファイルは、Claude や他の AI アシスタントが Floorp プロジェクトを理解し、効果的に開発支援を行うためのコンテキスト情報を提供します。

## プロジェクト概要

**Floorp** は Mozilla Firefox をベースにした Web ブラウザで、「オープン、プライベート、サステナブルな Web を維持する」ことを目的としています。

- **ライセンス**: Mozilla Public License 2.0 (MPL-2.0)
- **ベース**: Mozilla Firefox (Gecko エンジン)
- **開発名**: 内部的に "Noraneko" としても参照される

## 重要なドキュメント

プロジェクトの詳細な情報は以下のドキュメントを参照してください：

### 必読ドキュメント

1. **[docs/llm/README.md](../docs/llm/README.md)** - LLM 向けドキュメントの索引
2. **[docs/llm/project-overview.md](../docs/llm/project-overview.md)** - プロジェクト全体の概要
3. **[docs/llm/development-notes.md](../docs/llm/development-notes.md)** - 開発時の注意点とベストプラクティス
4. **[docs/llm/architecture-deep-dive.md](../docs/llm/architecture-deep-dive.md)** - アーキテクチャの詳細

## クイックリファレンス

### 技術スタック

- **ランタイム**: Deno 2.x (メイン), Node.js 22 (一部)
- **UI フレームワーク**:
  - SolidJS (ブラウザクローム機能)
  - React (設定ページ)
- **ビルドツール**: Vite, feles-build (カスタム)
- **言語**: TypeScript, JavaScript
- **CSS**: Tailwind CSS

### ディレクトリ構造

```
Floorp/
├── browser-features/     # カスタムブラウザ機能
│   ├── chrome/          # ブラウザ UI 機能 (SolidJS)
│   ├── modules/         # Firefox ESM モジュール (.sys.mts)
│   ├── pages-*/         # React ベースのページ
│   └── skin/            # CSS テーマ
├── bridge/              # Firefox とカスタム機能の橋渡し
├── libs/                # 共有ライブラリ
├── tools/               # ビルドツール
├── static/              # 静的アセット、Gecko 設定
├── i18n/                # 国際化 (25+ 言語)
└── docs/                # ドキュメント
    └── llm/             # AI 向けドキュメント ★重要★
```

### 開発コマンド

```bash
# 開発環境の起動 (HMR 付き)
deno task feles-build dev

# 本番ビルド
deno task feles-build build

# ステージングビルド
deno task feles-build stage

# 依存関係のインストール
deno install
```

### ファイル拡張子の意味

- `.sys.mts` - Firefox ESM モジュール (Firefox API にアクセス可能)
- `.tsx` - React または SolidJS コンポーネント
- `.ts` - 通常の TypeScript ファイル

### モジュールパスエイリアス

```typescript
#i18n/          → i18n/
#chrome/        → chrome/
#libs/          → libs/
#modules/       → browser-features/modules/
#features-chrome/ → browser-features/chrome/
#ui/            → src/ui/
#themes/        → src/themes/
```

## 開発時の重要なポイント

### 1. ファイル配置の選択

**ブラウザ UI 機能を追加する場合**:
- 場所: `browser-features/chrome/common/{feature-name}/`
- 技術: SolidJS
- HMR: サポート
- デコレーター: `@noraComponent(import.meta.hot)` が必要

**Firefox API にアクセスする場合**:
- 場所: `browser-features/modules/modules/{module-name}.sys.mts`
- 技術: Firefox ESM
- API: Services, Components, ChromeUtils など
- 注意: `.sys.mts` ファイル内では型定義を同じファイルに書いても OK

**設定ページを追加する場合**:
- 場所: `browser-features/pages-settings/src/pages/`
- 技術: React + Tailwind CSS

### 2. 型定義のベストプラクティス

**重要**: 型定義は可能な限り別ファイルに分離してください。

```typescript
// Good: 型定義を別ファイルに
// types.ts
export interface FeatureConfig {
  enabled: boolean;
  timeout: number;
}

// index.ts
import type { FeatureConfig } from "./types.ts";

// Bad: 実装と型定義を混在
// index.ts
export interface FeatureConfig { /* ... */ }
export class Feature { /* ... */ }
```

**例外**: `.sys.mts` ファイルでは、Firefox API との統合のため型定義を同じファイルに書いても構いません。

### 3. よくあるパターン

**新しい機能の追加**:
```typescript
// browser-features/chrome/common/my-feature/index.ts
import { NoraComponentBase, noraComponent } from "#libs/shared";

@noraComponent(import.meta.hot)
export default class MyFeature extends NoraComponentBase {
  init(): void {
    // 初期化処理
  }
}
```

**Window Actor (親子プロセス通信)**:
```typescript
// Parent: browser-features/modules/actors/MyFeatureParent.sys.mts
export class MyFeatureParent extends JSWindowActorParent {
  receiveMessage(message) {
    // 親プロセスでの処理
  }
}

// Child: browser-features/modules/actors/MyFeatureChild.sys.mts
export class MyFeatureChild extends JSWindowActorChild {
  handleEvent(event) {
    // 子プロセスでの処理
  }
}
```

### 4. 開発サーバーのポート

開発時は 8 つの Vite サーバーが並行稼働：
- 5173: メイン機能
- 5178: 設定ページ
- 5186: 新しいタブページ
- 5174-5177: その他のページ

### 5. デバッグ方法

- **ブラウザコンソール**: `Ctrl+Shift+J` (Windows/Linux) / `Cmd+Option+J` (macOS)
- **開発者ツール**: `Ctrl+Shift+I` (Windows/Linux) / `Cmd+Option+I` (macOS)
- **ログ出力**: `console.log("[FeatureName]", ...)`

## タスク別の推奨アプローチ

### バグ修正
1. [docs/llm/development-notes.md](../docs/llm/development-notes.md) の「よくある問題」を確認
2. デバッグツールで原因を特定
3. 関連するテストを実行

### 新機能の追加
1. [docs/llm/project-overview.md](../docs/llm/project-overview.md) で配置場所を決定
2. [docs/llm/architecture-deep-dive.md](../docs/llm/architecture-deep-dive.md) で類似機能の実装パターンを確認
3. [docs/llm/development-notes.md](../docs/llm/development-notes.md) のチェックリストに従って実装

### リファクタリング
1. [docs/llm/architecture-deep-dive.md](../docs/llm/architecture-deep-dive.md) で現在のアーキテクチャを理解
2. [docs/llm/development-notes.md](../docs/llm/development-notes.md) のベストプラクティスを確認
3. 段階的に変更を実施

### パフォーマンス最適化
1. [docs/llm/development-notes.md](../docs/llm/development-notes.md) のパフォーマンスセクションを確認
2. プロファイリングツールで測定
3. ボトルネックを特定して最適化

## 主要機能一覧

### ブラウザクローム機能 (browser-features/chrome/common/)

| 機能 | ディレクトリ | 説明 |
|------|------------|------|
| **Workspaces** | `workspaces/` | タブをワークスペースに整理する機能 |
| **PWA サポート** | `pwa/` | Progressive Web App のインストールと管理 |
| **Panel Sidebar** | `panel-sidebar/` | カスタマイズ可能なサイドパネルシステム |
| **マウスジェスチャー** | `mouse-gesture/` | ジェスチャー認識システム |
| **キーボードショートカット** | `keyboard-shortcut/` | カスタムキーボードショートカット |
| **タブ機能** | `tab/` | タブの拡張機能 |
| **タブバー** | `tabbar/` | 複数行タブバー、タブサイズ指定など |
| **Split View** | `splitView/` | 分割画面ブラウジング |
| **デザインシステム** | `designs/` | UI カスタマイズフレームワーク |
| **ステータスバー** | `statusbar/` | カスタムステータスバー |
| **QR コードジェネレーター** | `qr-code-generator/` | QR コード生成機能 |
| **コンテキストメニュー** | `context-menu/` | コンテキストメニューのカスタマイズ |
| **プライベートコンテナ** | `private-container/` | プライベートコンテナ機能 |
| **プロファイルマネージャー** | `profile-manager/` | マルチプロファイル管理 |
| **ブラウザシェアモード** | `browser-share-mode/` | ブラウザ共有モード |
| **ブラウザタブカラー** | `browser-tab-color/` | タブの色カスタマイズ |
| **Flex Order** | `flex-order/` | UI 要素の並び替え |
| **Reverse Sidebar Position** | `reverse-sidebar-position/` | サイドバー位置の反転 |
| **閉じたタブを元に戻す** | `undo-closed-tab/` | 閉じたタブの復元機能 |
| **UI カスタム** | `ui-custom/` | UI のカスタマイズ |
| **Chrome CSS** | `chrome-css/` | Chrome スタイルの CSS |
| **モーダル親** | `modal-parent/` | モーダルダイアログの親管理 |
| **再起動パネルメニュー** | `reboot-panel-menu/` | 再起動メニュー |

### システムモジュール (browser-features/modules/)

- **Experiments** (`modules/experiments/`) - A/B テストフレームワーク
- **i18n** (`modules/i18n/`) - 国際化ユーティリティ
- **os-apis** (`modules/os-apis/`) - OS 統合 API
- **os-server** (`modules/os-server/`) - ローカル API サーバー
- **pwa** (`modules/pwa/`) - PWA バックエンドサービス

### ページ (browser-features/pages-*)

- **Settings** (`pages-settings/`) - 設定ページ (React + Tailwind)
- **New Tab** (`pages-newtab/`) - 新しいタブページ
- **Welcome** (`pages-welcome/`) - ウェルカムページ
- **Notes** (`pages-notes/`) - ノート機能
- **Profile Manager** (`pages-profile-manager/`) - プロファイル管理 UI
- **Modal Child** (`pages-modal-child/`) - モーダルダイアログ

## コーディング規約

### TypeScript

```typescript
// Good: 型定義を別ファイルに
// types.ts
export interface Config {
  enabled: boolean;
  timeout: number;
}

// index.ts
import type { Config } from "./types.ts";

class MyFeature {
  constructor(private config: Config) {}
}

// Bad: any 型の乱用
let config: any;  // 避ける
```

### SolidJS

```typescript
// Good: リアクティブな状態管理
import { createSignal } from "solid-js";

const [count, setCount] = createSignal(0);
setCount(c => c + 1);

// Bad: 直接変更
let count = 0;
count++;  // SolidJS では動作しない
```

### エラーハンドリング

```typescript
// Good: 適切なエラーハンドリング
try {
  const data = await fetchData(url);
  return data;
} catch (error) {
  console.error("Failed to fetch:", error);
  return null; // フォールバック
}
```

## テスト

現在のテスト構造:
- ユニットテスト: `browser-features/chrome/test/unit/`
- テストフレームワーク: Deno の標準テストツール
- **注意**: テストカバレッジは限定的（改善が必要）

## CI/CD

- **メインワークフロー**: `.github/workflows/package.yml`
- **プラットフォーム**: Windows x64, Linux x64/aarch64, macOS Universal
- **コード署名**: Windows (Certum), macOS (Apple 公証)
- **配布**: GitHub Releases, PPA, Flatpak, Winget, Homebrew

## よくある質問

### Q: .sys.mts と .ts の違いは？
A: `.sys.mts` は Firefox の ESM モジュールで Firefox API に直接アクセス可能。`.ts` は Vite でバンドルされる通常の TypeScript。

### Q: SolidJS と React どちらを使うべき？
A: ブラウザクローム機能は SolidJS、設定ページなどは React を使用。

### Q: 型定義はどこに書くべき？
A: 可能な限り別ファイル（`types.ts`）に分離。ただし `.sys.mts` ファイルは例外。

### Q: HMR が動作しない
A: `@noraComponent(import.meta.hot)` デコレーターが付いているか確認。

### Q: Firefox API にアクセスできない
A: `.sys.mts` ファイルから ChromeUtils.importESModule() を使用。

## 追加リソース

- **公式サイト**: https://floorp.app
- **ドキュメント**: https://docs.floorp.app
- **Discord**: https://discord.floorp.app
- **GitHub**: https://github.com/Floorp-Projects/Floorp

## AI アシスタント向けの注意事項

1. **常に最新のコードを確認**: ドキュメントより実際のコードが正
2. **既存のパターンに従う**: プロジェクトの一貫性を保つ
3. **型定義は別ファイルに**: `.sys.mts` 以外では型を別ファイルに分離
4. **段階的な説明**: 複雑な概念は段階的に説明
5. **具体的なパスを提供**: ファイル参照時は具体的なパスを明示
6. **ベストプラクティスを推奨**: 確立されたパターンを優先

---

**最終更新**: 2025-01-30
**保守**: プロジェクト変更時に更新してください
