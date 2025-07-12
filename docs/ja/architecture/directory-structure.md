# ディレクトリ構造 - 詳細ガイド

## ルートディレクトリ構造

```
floorp/
├── .git/                    # Gitリポジトリメタデータ
├── .github/                 # GitHub Actionsワークフローとテンプレート
├── .vscode/                 # VSCodeワークスペース設定
├── .claude/                 # Claude AIアシスタント設定
├── @types/                  # TypeScript型定義
├── src/                     # メインソースコードディレクトリ
├── scripts/                 # ビルドと自動化スクリプト
├── gecko/                   # Firefox/Gecko統合ファイル
├── _dist/                   # ビルド出力ディレクトリ (生成)
├── build.ts                 # メインビルドスクリプト
├── deno.json               # Deno設定とタスク
├── package.json            # Node.js依存関係
├── tsconfig.json           # TypeScript設定
├── moz.build               # Mozillaビルドシステム統合
├── biome.json              # Biome設定 (フォーマッター/リンター)
├── LICENSE                 # ライセンスファイル
├── README.md               # プロジェクトドキュメント
└── TODO.md                 # TODOリスト
```

## src/ディレクトリ - メインソースコード

### 全体構造

```
src/
├── apps/                   # UIアプリケーション
├── packages/               # 再利用可能なパッケージ
├── installers/             # インストーラー
└── .oxlintrc.json         # Oxlint設定
```

### src/apps/ - アプリケーション

```
src/apps/
├── main/                   # メインブラウザアプリケーション
│   ├── core/              # コア機能
│   │   ├── index.ts       # エントリーポイント
│   │   ├── modules.ts     # モジュール管理
│   │   ├── modules-hooks.ts # モジュールフック
│   │   ├── common/        # 共通ユーティリティ
│   │   ├── static/        # 静的アセット
│   │   ├── test/          # テストファイル
│   │   └── utils/         # ユーティリティ関数
│   ├── i18n/              # 国際化ファイル
│   ├── docs/              # ドキュメント
│   ├── about/             # Aboutページ
│   ├── @types/            # 型定義
│   ├── package.json       # 依存関係
│   ├── vite.config.ts     # Vite設定
│   ├── tsconfig.json      # TypeScript設定
│   ├── tailwind.config.js # Tailwind CSS設定
│   └── deno.json          # Deno設定
│
├── settings/              # 設定アプリケーション
│   ├── src/               # ソースコード
│   │   ├── components/    # UIコンポーネント
│   │   ├── pages/         # ページコンポーネント
│   │   ├── utils/         # ユーティリティ
│   │   └── main.tsx       # エントリーポイント
│   ├── package.json       # 依存関係
│   ├── vite.config.ts     # Vite設定
│   ├── components.json    # shadcn/ui設定
│   └── index.html         # HTMLテンプレート
│
├── newtab/                # 新規タブページ
│   ├── src/               # ソースコード
│   │   ├── components/    # UIコンポーネント
│   │   ├── stores/        # 状態管理
│   │   └── main.tsx       # エントリーポイント
│   ├── package.json       # 依存関係
│   ├── vite.config.ts     # Vite設定
│   └── index.html         # HTMLテンプレート
│
├── notes/                 # メモ機能
├── welcome/               # ウェルカム画面
├── startup/               # 起動処理
├── modal-child/           # モーダルダイアログ
├── os/                    # OS固有機能
├── i18n-supports/         # 国際化サポート
├── modules/               # 共有モジュール
├── common/                # 共通コンポーネント
└── README.md              # アプリケーション概要
```

### src/packages/ - パッケージ

```
src/packages/
├── solid-xul/             # SolidJS + XUL統合
│   ├── index.ts           # メインAPI
│   ├── jsx-runtime.ts     # JSXランタイム
│   ├── package.json       # パッケージ設定
│   └── tsconfig.json      # TypeScript設定
│
├── skin/                  # スキンシステム
│   ├── fluerial/          # Fluerialテーマ
│   ├── lepton/            # Leptonテーマ
│   ├── noraneko/          # Noranekoテーマ
│   └── package.json       # パッケージ設定
│
├── user-js-runner/        # ユーザースクリプト実行
│   ├── src/               # ソースコード
│   ├── package.json       # パッケージ設定
│   └── README.md          # ドキュメント
│
└── vitest-noraneko/       # テストユーティリティ
    ├── src/               # ソースコード
    ├── package.json       # パッケージ設定
    └── README.md          # ドキュメント
```

### src/installers/ - インストーラー

```
src/installers/
└── stub-win64-installer/  # Windows 64ビットインストーラー
    ├── src/               # Tauriアプリケーション
    ├── src-tauri/         # Tauri設定
    ├── package.json       # 依存関係
    └── tauri.conf.json    # Tauri設定
```

## scripts/ - ビルドと自動化スクリプト

```
scripts/
├── inject/                # コード注入関連
│   ├── manifest.ts        # マニフェスト注入
│   ├── xhtml.ts          # XHTMLテンプレート注入
│   ├── mixin-loader.ts   # ミキシンシステムローダー
│   ├── mixins/           # UIミキシン
│   │   ├── browser/      # ブラウザUIミキシン
│   │   ├── preferences/  # 設定UIミキシン
│   │   └── shared/       # 共有ミキシン
│   ├── shared/           # 共有ユーティリティ
│   ├── wasm/             # WebAssemblyモジュール
│   └── tsconfig.json     # TypeScript設定
│
├── launchDev/            # 開発サーバー
│   ├── child-build.ts    # 子プロセスビルド
│   ├── child-dev.ts      # 開発サーバー
│   ├── child-browser.ts  # ブラウザ起動
│   ├── savePrefs.ts      # 設定保存
│   └── writeVersion.ts   # バージョン書き込み
│
├── update/               # 更新関連
│   ├── buildid2.ts       # ビルドID管理
│   ├── version.ts        # バージョン管理
│   └── xml.ts            # XML処理
│
├── cssUpdate/            # CSS更新
│   ├── processors/       # CSSプロセッサー
│   └── watchers/         # ファイル監視
│
├── git-patches/          # Gitパッチ管理
│   ├── git-patches-manager.ts # メインパッチマネージャー
│   └── patches/          # パッチファイル
│       ├── firefox/      # Firefoxパッチ
│       └── gecko/        # Geckoパッチ
│
├── i18n/                 # 国際化
│   ├── extractors/       # 文字列抽出
│   ├── translators/      # 翻訳処理
│   └── validators/       # 翻訳検証
│
└── .oxlintrc.json       # Oxlint設定
```

## gecko/ - Firefox/Gecko 統合

```
gecko/
├── config/               # Firefoxビルド設定
│   ├── moz.configure     # Mozilla設定
│   ├── README.md         # 設定ドキュメント
│   └── .gitignore        # Git無視設定
│
├── branding/             # ブランディング
│   ├── floorp-official/  # 公式Floorpブランディング
│   │   ├── icons/        # アイコンファイル
│   │   ├── locales/      # ローカライゼーション
│   │   └── configure.sh  # 設定スクリプト
│   ├── floorp-daylight/  # Daylightビルドブランディング
│   └── noraneko-unofficial/ # 非公式Noranekoブランディング
│
└── pre-build/            # プリビルド設定
    ├── patches/          # プリビルドパッチ
    ├── configs/          # 設定ファイル
    └── scripts/          # プリビルドスクリプト
```

## @types/ - TypeScript 型定義

```
@types/
└── gecko/                # Firefox/Gecko型定義
    └── dom/              # DOM API型定義
        ├── index.d.ts    # メイン型定義
        ├── xul.d.ts      # XUL型定義
        ├── xpcom.d.ts    # XPCOM型定義
        └── webext.d.ts   # WebExtensions型定義
```

## \_dist/ - ビルド出力 (生成)

```
_dist/
├── bin/                  # バイナリファイル
│   └── floorp/           # Floorpバイナリ
├── noraneko/             # ビルド済みアプリケーション
│   ├── content/          # コンテンツファイル
│   ├── startup/          # 起動ファイル
│   ├── resource/         # リソースファイル
│   ├── settings/         # 設定アプリ
│   ├── newtab/           # 新規タブアプリ
│   ├── welcome/          # ウェルカムアプリ
│   ├── notes/            # メモアプリ
│   ├── modal-child/      # モーダルダイアログ
│   └── os/               # OS固有ファイル
├── profile/              # 開発プロファイル
└── buildid2              # ビルドID
```

## 設定ファイル詳細

### deno.json - Deno 設定

```json
{
  "workspace": ["./src/packages/*", "./src/apps/*"],
  "imports": {
    "@std/encoding": "jsr:@std/encoding@^1.0.7",
    "@std/fs": "jsr:@std/fs@^1.0.11"
  },
  "tasks": {
    "dev": "deno run -A build.ts --run",
    "build": "deno run -A build.ts"
  }
}
```

### package.json - Node.js 依存関係

- フロントエンド開発ツール
- ビルドツール
- UI ライブラリ

### moz.build - Mozilla ビルドシステム統合

```python
DIRS += [
    "_dist/noraneko/content",
    "_dist/noraneko/startup",
    # ... その他のディレクトリ
]
```

## ファイル命名規則

### TypeScript/JavaScript

- **コンポーネント**: PascalCase (`MainComponent.tsx`)
- **ユーティリティ**: camelCase (`utilityFunction.ts`)
- **設定**: kebab-case (`vite.config.ts`)

### CSS/SCSS

- **ファイル**: kebab-case (`main-styles.css`)
- **クラス**: BEM 記法または Tailwind

### アセット

- **画像**: kebab-case (`app-icon.png`)
- **フォント**: kebab-case (`custom-font.woff2`)

## ディレクトリの役割と責任

### 開発関連

- `src/apps/`: UI アプリケーション実装
- `src/packages/`: 再利用可能なライブラリ
- `scripts/`: ビルドと開発サポートツール

### ビルド関連

- `_dist/`: ビルド成果物 (一時的)
- `build.ts`: ビルドオーケストレーション
- 各アプリの`vite.config.ts`: 個別ビルド設定

### 統合関連

- `gecko/`: Firefox 統合設定
- `@types/`: 型安全性保証

この構造により、Floorp は複雑なブラウザアプリケーションを整理された方法で管理し、開発者が効率的に作業できる環境を提供します。
