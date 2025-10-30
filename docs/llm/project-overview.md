# Floorp プロジェクト全体概要

このドキュメントは、Floorp プロジェクトの全体像を理解するための包括的なガイドです。

## 目次

1. [プロジェクト概要](#プロジェクト概要)
2. [アーキテクチャ構造](#アーキテクチャ構造)
3. [主要コンポーネント](#主要コンポーネント)
4. [ビルドシステム](#ビルドシステム)
5. [開発ワークフロー](#開発ワークフロー)
6. [重要な技術的決定](#重要な技術的決定)

---

## プロジェクト概要

### Floorp とは？

**Floorp** は Mozilla Firefox をベースにした Web ブラウザで、「オープン、プライベート、サステナブルな Web を維持する」ことを目的としています。

- **ライセンス**: Mozilla Public License 2.0 (MPL-2.0)
- **現在のバージョン**:
  - Gecko バージョン: 1.0.0
  - パッケージバージョン: 12.4.0
- **ベース**: Mozilla Firefox (Gecko エンジン)
- **開発名**: 内部的に "Noraneko" としても参照される

### 基本情報

- **公式サイト**: https://floorp.app
- **ドキュメント**: https://docs.floorp.app
- **コミュニティ**: Discord (https://discord.floorp.app)
- **スポンサー**: CubeSoft, Inc. など

---

## アーキテクチャ構造

### ディレクトリ構造

```
Floorp/
├── browser-features/     # カスタムブラウザ機能（コアカスタマイズ）
│   ├── chrome/          # ブラウザ UI 機能
│   ├── modules/         # Firefox ESM モジュール
│   ├── pages-*/         # React ベースの設定ページ
│   └── skin/            # CSS テーマとビジュアルカスタマイズ
├── bridge/              # Firefox とカスタム機能の橋渡し
├── libs/                # 共有ライブラリとユーティリティ
├── tools/               # ビルドツールとスクリプト
├── static/              # 静的アセット、Gecko 設定
├── i18n/                # 国際化（25+ 言語対応）
├── docs/                # ドキュメント
├── _dist/               # ビルド出力ディレクトリ
├── deno.json            # Deno ワークスペース設定
├── package.json         # npm 依存関係
└── moz.build            # Mozilla ビルドシステム統合
```

### 技術スタック

#### コア技術

- **ランタイム**: Deno 2.x（メイン） + Node.js 22（一部依存関係）
- **ビルドシステム**:
  - カスタム feles-build（ビルドオーケストレーション）
  - Mozilla mach ビルドシステム（Firefox ベース）
  - Vite（フロントエンドバンドリング）
- **UI フレームワーク**:
  - **SolidJS**: ブラウザクローム用（XUL 的なコンポーネント）
  - **React**: 設定ページとモダン UI 用
- **言語**: TypeScript（メイン）、JavaScript、CSS
- **パッケージマネージャ**: Deno（JSR）、pnpm 10.14.0

#### 主要依存関係

| パッケージ | バージョン | 用途 |
|-----------|-----------|------|
| solid-js | ^1.9.9 | リアクティブ UI フレームワーク |
| vite | ^7.1.4 | ビルドツール |
| tailwindcss | ^4.1.13 | CSS フレームワーク |
| i18next | ^25.5.2 | 国際化 |
| typescript | ^5.9.2 | 型システム |
| fp-ts | ^2.16.11 | 関数型プログラミング |
| puppeteer-core | - | ブラウザ自動化テスト |

---

## 主要コンポーネント

### 1. Browser Features (`browser-features/chrome/`)

ブラウザの UI カスタマイズの中心部分です。

#### 主要機能一覧

| 機能 | ディレクトリ | 説明 |
|-----|------------|------|
| **Workspaces** | `workspaces/` | タブをワークスペースに整理する機能 |
| **PWA サポート** | `pwa/` | Progressive Web App のインストールと管理 |
| **Panel Sidebar** | `panel-sidebar/` | カスタマイズ可能なサイドパネルシステム |
| **マウスジェスチャー** | `mouse-gesture/` | ジェスチャー認識システム |
| **キーボードショートカット** | `keyboard-shortcut/` | カスタムキーボードショートカット |
| **タブ拡張** | `tab/`, `tabbar/` | 複数行タブバー、タブサイズ指定など |
| **Split View** | `splitView/` | 分割画面ブラウジング |
| **デザインシステム** | `designs/` | UI カスタマイズフレームワーク |
| **ステータスバー** | `statusbar/` | カスタムステータスバー |
| **QR コードジェネレーター** | `qr-code-generator/` | QR コード生成機能 |
| **プライベートコンテナ** | `private-container/` | プライベートコンテナ機能 |
| **プロファイルマネージャー** | `profile-manager/` | マルチプロファイル管理 |

#### コンポーネントパターン

ほとんどの機能は以下のパターンに従います：

```typescript
@noraComponent(import.meta.hot)  // HMR デコレーター
export default class FeatureName extends NoraComponentBase {
  init(): void {
    // 機能の初期化
  }
}
```

**特徴**:
- Hot Module Replacement (HMR) 対応
- SolidJS 統合によるリアクティブ UI
- Service ベースアーキテクチャ（Service、Controller、Config パターン）
- 設定アップグレード用の Migration システム

### 2. Modules (`browser-features/modules/`)

Firefox ESM モジュール（`.sys.mts` ファイル）です。

#### コアモジュール

- `NoranekoStartup.sys.mts` - スタートアップ初期化
- `BrowserGlue.sys.mts` - Window Actor 登録
- `NoranekoConstants.sys.mts` - 共有定数

#### 特化モジュール

| モジュール | 説明 |
|-----------|------|
| **experiments/** | A/B テストフレームワーク |
| **i18n/** | 国際化ユーティリティ |
| **os-apis/** | OS 統合 API |
| **os-server/** | ローカル API サーバー |
| **pwa/** | PWA バックエンドサービス |

#### Actors (`browser-features/modules/actors/`)

マルチプロセスアーキテクチャ用の Window Actor：

- `NRAboutPreferencesChild/Parent` - 設定統合
- `NRSettings`, `NRPanelSidebar`, `NRTabManager` - 機能 Actor
- `NRProgressiveWebApp`, `NRPwaManager` - PWA Actor
- `NRProfileManager` - プロファイル管理
- `NRStartPage`, `NRWelcomePage` - 特別ページ Actor
- `NRSearchEngine`, `NRWebScraper` - Web インタラクション
- `NRI18n` - 翻訳サポート

### 3. Pages (`browser-features/pages-*`)

React ベースのモダン UI ページです。

| ページ | ディレクトリ | 説明 |
|--------|-------------|------|
| **設定ページ** | `pages-settings/` | Floorp について、アカウント管理、デバッグツールなど |
| **新しいタブ** | `pages-newtab/` | カスタム新規タブページ |
| **ウェルカムページ** | `pages-welcome/` | 初回起動時のエクスペリエンス |
| **ノート** | `pages-notes/` | 組み込みノート機能 |
| **プロファイルマネージャー** | `pages-profile-manager/` | マルチプロファイル管理 UI |
| **モーダル** | `pages-modal-child/` | モーダルダイアログシステム |

**技術**: React + Tailwind CSS

### 4. Skin/Themes (`browser-features/skin/`)

CSS テーマとビジュアルカスタマイズ：

- **Fluerial** デザインシステム
- Firefox UI Fix (Lepton) 統合
- userChrome.css ローダーシステム

---

## ビルドシステム

### feles-build（カスタムビルドツール）

場所: [`tools/feles-build.ts`](../tools/feles-build.ts)

#### 主要コマンド

```bash
# 開発モード（HMR 付き）
deno task feles-build dev

# ステージング（本番アセット + 開発ブラウザ）
deno task feles-build stage

# 本番ビルド
deno task feles-build build
```

#### ビルドプロセス

**1. 開発モード（`dev`）**:
- GitHub Actions から Firefox バイナリをダウンロード（またはキャッシュ使用）
- Firefox ソースにパッチを適用
- シンボリックリンクを設定
- Vite で Floorp 機能をビルド（開発モード）
- 複数の開発サーバーを起動（8 つの Vite インスタンス）:
  - メイン機能（ポート 5173）
  - 設定（ポート 5178）
  - 新しいタブ（ポート 5186）
  - ウェルカムページ、ノート、モーダルなど
- マニフェストを注入し、HMR 付きでブラウザを起動

**2. 本番ビルド（`build --phase before-mach`）**:
- リソースをシンボリックリンク
- すべての機能を本番モードでビルド
- 最適化されたバンドルを作成

**3. Mozilla ビルド後（`build --phase after-mach`）**:
- XHTML 修正を注入
- ビルドされた Firefox にパッチを適用
- インストーラーパッケージを作成

### ビルドツール（`tools/src/`）

| モジュール | 説明 |
|-----------|------|
| `builder.ts` | 並列ビルド実行、バージョン管理 |
| `injector.ts` | XHTML 注入、マニフェスト作成 |
| `patcher.ts` | Firefox ソースへのパッチ適用 |
| `symlinker.ts` | リソース用シンボリックリンク作成 |
| `dev_server.ts` | 8 つの並行 Vite 開発サーバー管理 |
| `browser_launcher.ts` | 適切なフラグで Firefox を起動 |
| `update.ts` | バージョンとビルド ID 管理 |
| `initializer.ts` | 初期セットアップタスク |

### Bridge Layer（`bridge/`）

Floorp 機能と Firefox を接続：

1. `startup/` - Firefox に注入されるスタートアップスクリプト
2. `loader-features/` - HMR サポート付き機能ローダー
3. `loader-modules/` - ESM モジュール用モジュールローダー

---

## 開発ワークフロー

### セットアップ

**必要なもの**:
- Deno 2.x
- PowerShell 7（Windows）
- Git
- ~2.5GB（Firefox バイナリダウンロード用）

**初期セットアップ**:
```bash
deno install               # 依存関係をインストール
deno task dev             # 開発を開始
```

### Hot Module Replacement (HMR)

プロジェクトは洗練された HMR システムを実装しています：

- `@noraComponent(import.meta.hot)` でデコレートされた機能
- 自動 HMR サポート付きの基底クラス `NoraComponentBase`
- `createRootHMR` を介した SolidJS 統合
- ブラウザを再起動せずにライブ更新が可能

### 開発パターン

#### コード構成パターン

1. **機能ベース構造** - 各機能は自己完結型
2. **Service-Controller-Config パターン** - 複雑な機能用
3. **Actor パターン** - プロセス間通信用
4. **Decorator パターン** - HMR とコンポーネント登録用
5. **Migration システム** - 下位互換性用

#### 命名規則

- **ファイル名**:
  - `.sys.mts` - Firefox システムモジュール（ESM）
  - `.sys.mjs` - コンパイル済みシステムモジュール
  - `.tsx` - React/SolidJS コンポーネント
  - `.ts` - TypeScript モジュール

- **モジュールパス**:
  - `#i18n/` - i18n リソース
  - `#chrome/` - Chrome 機能
  - `#libs/` - 共有ライブラリ
  - `#modules/` - システムモジュール
  - `#features-chrome/` - ブラウザ機能
  - `resource://noraneko/` - ランタイムリソース

---

## 重要な技術的決定

### なぜこれらの選択が重要か

#### 1. Deno をプライマリランタイムに

**理由**:
- ビルトイン TypeScript を備えたモダンな JavaScript ランタイム
- デフォルトでセキュア
- より良い依存関係管理（JSR + npm）
- ネイティブ TypeScript 実行

#### 2. ブラウザクロームに SolidJS

**理由**:
- UI ヘビーなブラウザクロームでは React より高性能
- XUL のようなリアクティビティモデル
- ブラウザ拡張機能のような機能により適している

#### 3. 設定ページに React

**理由**:
- コントリビューターに馴染みのあるエコシステム
- 豊富なコンポーネントライブラリ
- フォームヘビーなインターフェースに適している

#### 4. ビルドシステムに Vite

**理由**:
- 高速 HMR（Hot Module Replacement）
- 複数の同時開発サーバー
- Rollup によるモダンなバンドリング

#### 5. アーティファクトモードビルディング

**理由**:
- プリビルドされた Firefox バイナリをダウンロード
- Floorp 固有のコードのみをコンパイル
- 開発サイクルが劇的に高速化
- ビルド要件の削減（C++ コンパイル不要）

#### 6. パッチベースの Firefox 修正

**理由**:
- Firefox コアへの変更を最小限に
- 新しい Firefox バージョンへの更新が容易
- 関心事の明確な分離
- 場所: `tools/patches/`

### 注目すべき技術的成果

1. **マルチプロセスアーキテクチャ**: Window Actor を通じた Firefox の E10S（マルチプロセス）の適切な使用

2. **PWA 実装**: 完全な Progressive Web App サポート：
   - マニフェスト解析
   - アイコン処理（複数サイズ、フォーマット）
   - OS 統合（タスクバー、アプリメニュー）
   - 個別のウィンドウ管理

3. **ワークスペースシステム**: 永続化と同期を備えたタブ整理

4. **Experiments フレームワーク**: 外部サービスなしの本番対応 A/B テスト

5. **ホットリロードシステム**: ブラウザを再起動せずに開発のためのライブ更新

6. **マルチプラットフォームサポート**: Windows、macOS、Linux（ARM を含む）用の単一コードベース

---

## テストと品質保証

### テスト構造

限定的だが存在する：
- ユニットテスト: `browser-features/chrome/test/unit/`
- 例: `onModuleLoaded.test.ts`
- Deno のテストフレームワークを使用（`@jsr/std__assert`）

### 開発ツール

- ブラウザ自動化用の **Puppeteer 統合**
- Vite 開発サーバーによる**ライブリロード**
- デバッグ用の**ソースマップ**
- **リンティング**: oxlint、Prettier
- **型チェック**: TypeScript strict モード

---

## CI/CD とリリースプロセス

### GitHub Actions ワークフロー（`.github/workflows/`）

#### 1. パッケージワークフロー（`package.yml`）

メインのビルドパイプライン：
- マルチプラットフォームビルド（Windows x64、Linux x64/aarch64、macOS Universal）
- アーティファクトビルド（完全なコンパイルなし）
- コード署名（Windows 用 SignPath、macOS 用 Apple 公証）
- インストーラー作成: `.exe`、`.dmg`、`.tar.xz`、`.deb`
- MAR（Mozilla Archive）更新パッケージの生成
- **1290 行**の洗練されたビルドオーケストレーション

#### 2. インストーラービルド（`build_installers.yml`）

- Windows 用 Tauri ベースのスタブインストーラー
- コード署名統合

#### 3. 公開ワークフロー

- `publish_release.yml` - リリースチャネル
- `publish_alpha.yml` - Alpha/Beta ビルド
- `publish_ppa.yml` - Ubuntu PPA 配布

#### 4. メンテナンス

- `update_lepton.yml` - Firefox UI Fix の更新
- `organize_issue.yml` - Issue 管理

### サポートされているプラットフォーム

- **Windows**: x86_64（Certum 証明書で署名）
- **Linux**: x86_64 & aarch64（tarball、DEB、Flatpak、AUR）
- **macOS**: Universal binary（x86_64 + ARM64、公証済み）

### 配布チャネル

- GitHub Releases
- PPA（Ubuntu/Debian）: https://ppa.floorp.app
- Flatpak: https://flathub.org/apps/one.ablaze.floorp
- Winget、Scoop（Windows）
- Homebrew（macOS）
- AUR（Arch Linux、非公式）

---

## 国際化（i18n）

### サポート言語

25+ の言語をサポート：
- 日本語（ja-JP）
- 関西弁（ja-JP-x-kansai）- 特別なロケール
- 英語（en-US）
- 中国語（zh-CN、zh-TW）
- ドイツ語（de-DE）
- フランス語（fr-FR）
- スペイン語（es-ES）
- その他多数

### 翻訳システム

- **i18next** ベースの翻訳システム
- **Crowdin** 統合（コミュニティ翻訳用）
- 設定: [`i18n/config.ts`](../i18n/config.ts)
- 翻訳ファイル: `i18n/{locale}/` ディレクトリ

---

## カスタマイズと設定

### Gecko 設定（`static/gecko/`）

Firefox 固有の設定：
- `config/` - バージョンファイル、moz.configure
- `pref/` - プリファレンスオーバーライド
  - `override.ini` - プリファレンス変更
  - `override.sh` / `override.ps1` - オーバーライド適用スクリプト
  - `pref.ts` - プリファレンス管理

### ユーザーカスタマイズ

ブラウザはデフォルトのカスタマイズファイルを作成します：
- `chrome/userChrome.css` - UI カスタマイズ
- `chrome/userContent.css` - Web コンテンツスタイリング
- 初回起動時に役立つテンプレートと共に自動作成

### デザインシステム

ユーザーはカスタマイズ可能：
- タブバースタイル（複数行、垂直など）
- ブラウザテーマ（Fluerial、Lepton ベース）
- ツールバーレイアウト（flex-order カスタマイズ）
- キーボードショートカット
- マウスジェスチャー

---

## カスタム About ページ

カスタム `about:` URL の登録：
- `about:hub` - 設定ページ
- `about:welcome` - ウェルカムページ
- `about:newtab`、`about:home` - カスタム新しいタブ（オプション）

実装: [`browser-features/modules/modules/NoranekoStartup.sys.mts`](../browser-features/modules/modules/NoranekoStartup.sys.mts) での動的登録

---

## セキュリティとプライバシー

- **オープンソース**: 完全なソースコード公開（MPL-2.0）
- **プライバシーポリシー**: https://floorp.app/privacy
- **デフォルトでテレメトリーなし**（Firefox のプライバシー設定を継承）
- **コード署名**:
  - Windows: Certum Open Source 証明書
  - macOS: Apple 公証
- **サンドボックス**: Firefox のセキュリティモデルを継承

---

## コミュニティと貢献

- **スポンサーシップ**: GitHub Sponsors 利用可能
- **スペシャルスポンサー**: CubeSoft, Inc.
- **貢献**: Issue と PR を歓迎
- **Discord コミュニティ**: アクティブなサポートチャネル
- **Crowdin**: コミュニティ翻訳プラットフォーム

### 貢献ガイドライン

詳細は [`CONTRIBUTING.md`](../../CONTRIBUTING.md) を参照してください。

---

## 開発者向けサマリー

**Floorp は、モダンな Web 技術を使用して構築された広範なカスタマイズを備えた Firefox ベースのブラウザです。**

プロジェクトは以下を示しています：

- **モダンなツーリング**（Deno、Vite、TypeScript）
- **洗練されたアーキテクチャ**（マルチプロセス、リアクティブ UI、HMR）
- **プロフェッショナルな CI/CD**（マルチプラットフォームビルド、コード署名）
- **拡張性**（プラグインシステム、カスタム機能）
- **アクティブな開発**（Experiments フレームワーク、ワークスペースシステム）

コードベースは明確な分離で適切に整理されています：
- Firefox ベース（パッチのみ）
- ブラウザクローム機能（SolidJS）
- 設定ページ（React）
- システムモジュール（ESM）
- ビルドツーリング（Deno）

### プロジェクトを理解するための重要なファイル

1. [`tools/feles-build.ts`](../tools/feles-build.ts) - ビルドオーケストレーション
2. [`browser-features/modules/modules/NoranekoStartup.sys.mts`](../browser-features/modules/modules/NoranekoStartup.sys.mts) - スタートアップ初期化
3. [`browser-features/chrome/common/mod.ts`](../browser-features/chrome/common/mod.ts) - 機能レジストリ
4. [`.github/workflows/package.yml`](../../.github/workflows/package.yml) - ビルドパイプライン
5. [`deno.json`](../../deno.json) - ワークスペース設定

---

## 次のステップ

詳細については、以下のドキュメントを参照してください：

- [開発ガイド](./development-guide.md) - 開発の詳細な手順
- [アーキテクチャガイド](./architecture-guide.md) - 技術的な深掘り
- [Experiments フレームワーク](../experiment/) - A/B テストシステム

---

最終更新: 2025-01-30
