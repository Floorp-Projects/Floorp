# ディレクトリ構造 - 詳細解説

## ルートディレクトリ構造

```
floorp/
├── .git/                    # Git リポジトリメタデータ
├── .github/                 # GitHub Actions ワークフローとテンプレート
├── .vscode/                 # VSCode ワークスペース設定
├── .claude/                 # Claude AI アシスタント設定
├── @types/                  # TypeScript 型定義
├── src/                     # メインソースコードディレクトリ
├── scripts/                 # ビルドと自動化スクリプト
├── crates/                  # Rust クレートとコンポーネント
├── gecko/                   # Firefox/Gecko 統合ファイル
├── _dist/                   # ビルド出力ディレクトリ（生成される）
├── build.ts                 # メインビルドスクリプト
├── deno.json               # Deno 設定とタスク
├── package.json            # Node.js 依存関係
├── Cargo.toml              # Rust ワークスペース設定
├── tsconfig.json           # TypeScript 設定
├── moz.build               # Mozilla ビルドシステム統合
├── biome.json              # Biome 設定（フォーマッター/リンター）
├── LICENSE                 # ライセンスファイル
├── README.md               # プロジェクトドキュメント
└── TODO.md                 # TODO リスト
```

## src/ ディレクトリ - メインソースコード

### 全体構造
```
src/
├── apps/                   # UI アプリケーション
├── packages/               # 再利用可能なパッケージ
├── installers/             # インストーラー
├── lib.rs                  # Rust ライブラリエントリーポイント
└── .oxlintrc.json         # Oxlint 設定
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
│   ├── about/             # About ページ
│   ├── @types/            # 型定義
│   ├── package.json       # 依存関係
│   ├── vite.config.ts     # Vite 設定
│   ├── tsconfig.json      # TypeScript 設定
│   ├── tailwind.config.js # Tailwind CSS 設定
│   └── deno.json          # Deno 設定
│
├── settings/              # 設定アプリケーション
│   ├── src/               # ソースコード
│   │   ├── components/    # UI コンポーネント
│   │   ├── pages/         # ページコンポーネント
│   │   ├── utils/         # ユーティリティ
│   │   └── main.tsx       # エントリーポイント
│   ├── package.json       # 依存関係
│   ├── vite.config.ts     # Vite 設定
│   ├── components.json    # shadcn/ui 設定
│   └── index.html         # HTML テンプレート
│
├── newtab/                # 新しいタブページ
│   ├── src/               # ソースコード
│   │   ├── components/    # UI コンポーネント
│   │   ├── stores/        # 状態管理
│   │   └── main.tsx       # エントリーポイント
│   ├── package.json       # 依存関係
│   ├── vite.config.ts     # Vite 設定
│   └── index.html         # HTML テンプレート
│
├── notes/                 # ノート機能
├── welcome/               # ウェルカム画面
├── startup/               # 起動時処理
├── modal-child/           # モーダルダイアログ
├── os/                    # OS 固有機能
├── i18n-supports/         # 国際化サポート
├── modules/               # 共有モジュール
├── common/                # 共通コンポーネント
└── README.md              # アプリケーション概要
```

### src/packages/ - パッケージ

```
src/packages/
├── solid-xul/             # SolidJS + XUL 統合
│   ├── index.ts           # メイン API
│   ├── jsx-runtime.ts     # JSX ランタイム
│   ├── package.json       # パッケージ設定
│   └── tsconfig.json      # TypeScript 設定
│
├── skin/                  # スキンシステム
│   ├── fluerial/          # Fluerial テーマ
│   ├── lepton/            # Lepton テーマ
│   ├── noraneko/          # Noraneko テーマ
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
└── stub-win64-installer/  # Windows 64bit インストーラー
    ├── src/               # Tauri アプリケーション
    ├── src-tauri/         # Tauri 設定
    ├── package.json       # 依存関係
    └── tauri.conf.json    # Tauri 設定
```

## scripts/ - ビルドと自動化スクリプト

```
scripts/
├── inject/                # コード注入関連
│   ├── manifest.ts        # マニフェスト注入
│   ├── xhtml.ts          # XHTML テンプレート注入
│   ├── mixin-loader.ts   # Mixin システムローダー
│   ├── mixins/           # UI Mixin
│   │   ├── browser/      # ブラウザ UI Mixin
│   │   ├── preferences/  # 設定 UI Mixin
│   │   └── shared/       # 共有 Mixin
│   ├── shared/           # 共有ユーティリティ
│   ├── wasm/             # WebAssembly モジュール
│   └── tsconfig.json     # TypeScript 設定
│
├── launchDev/            # 開発サーバー
│   ├── child-build.ts    # 子プロセスビルド
│   ├── child-dev.ts      # 開発サーバー
│   ├── child-browser.ts  # ブラウザ起動
│   ├── savePrefs.ts      # 設定保存
│   └── writeVersion.ts   # バージョン書き込み
│
├── update/               # 更新関連
│   ├── buildid2.ts       # ビルド ID 管理
│   ├── version.ts        # バージョン管理
│   └── xml.ts            # XML 処理
│
├── cssUpdate/            # CSS 更新
│   ├── processors/       # CSS プロセッサー
│   └── watchers/         # ファイル監視
│
├── git-patches/          # Git パッチ管理
│   ├── git-patches-manager.ts # パッチ管理メイン
│   └── patches/          # パッチファイル
│       ├── firefox/      # Firefox パッチ
│       └── gecko/        # Gecko パッチ
│
├── i18n/                 # 国際化
│   ├── extractors/       # 文字列抽出
│   ├── translators/      # 翻訳処理
│   └── validators/       # 翻訳検証
│
└── .oxlintrc.json       # Oxlint 設定
```

## crates/ - Rust コンポーネント

```
crates/
└── nora-inject/          # コード注入システム
    ├── src/              # Rust ソースコード
    │   ├── lib.rs        # ライブラリエントリーポイント
    │   ├── inject.rs     # 注入ロジック
    │   ├── parser.rs     # コードパーサー
    │   └── utils.rs      # ユーティリティ
    ├── wit/              # WebAssembly Interface Types
    │   └── world.wit     # インターフェース定義
    ├── Cargo.toml        # Rust パッケージ設定
    ├── package.json      # npm パッケージ設定
    └── *.wasm            # コンパイル済み WebAssembly
```

## gecko/ - Firefox/Gecko 統合

```
gecko/
├── config/               # Firefox ビルド設定
│   ├── moz.configure     # Mozilla 設定
│   ├── README.md         # 設定説明
│   └── .gitignore        # Git 無視設定
│
├── branding/             # ブランディング
│   ├── floorp-official/  # 公式 Floorp ブランディング
│   │   ├── icons/        # アイコンファイル
│   │   ├── locales/      # ローカライゼーション
│   │   └── configure.sh  # 設定スクリプト
│   ├── floorp-daylight/  # Daylight ビルドブランディング
│   └── noraneko-unofficial/ # 非公式 Noraneko ブランディング
│
└── pre-build/            # プリビルド設定
    ├── patches/          # プリビルドパッチ
    ├── configs/          # 設定ファイル
    └── scripts/          # プリビルドスクリプト
```

## @types/ - TypeScript 型定義

```
@types/
└── gecko/                # Firefox/Gecko 型定義
    └── dom/              # DOM API 型定義
        ├── index.d.ts    # メイン型定義
        ├── xul.d.ts      # XUL 型定義
        ├── xpcom.d.ts    # XPCOM 型定義
        └── webext.d.ts   # WebExtensions 型定義
```

## _dist/ - ビルド出力（生成される）

```
_dist/
├── bin/                  # バイナリファイル
│   └── floorp/           # Floorp バイナリ
├── noraneko/             # ビルドされたアプリケーション
│   ├── content/          # コンテンツファイル
│   ├── startup/          # 起動ファイル
│   ├── resource/         # リソースファイル
│   ├── settings/         # 設定アプリ
│   ├── newtab/           # 新しいタブアプリ
│   ├── welcome/          # ウェルカムアプリ
│   ├── notes/            # ノートアプリ
│   ├── modal-child/      # モーダルダイアログ
│   └── os/               # OS 固有ファイル
├── profile/              # 開発用プロファイル
└── buildid2              # ビルド ID
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

### Cargo.toml - Rust ワークスペース
```toml
[workspace]
members = ["crates/nora-inject", "src/installers/stub-win64-installer"]
```

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
- **設定ファイル**: kebab-case (`vite.config.ts`)

### Rust
- **ファイル**: snake_case (`code_inject.rs`)
- **クレート**: kebab-case (`nora-inject`)

### CSS/SCSS
- **ファイル**: kebab-case (`main-styles.css`)
- **クラス**: BEM 記法またはTailwind

### アセット
- **画像**: kebab-case (`app-icon.png`)
- **フォント**: kebab-case (`custom-font.woff2`)

## ディレクトリの役割と責任

### 開発関連
- `src/apps/`: UI アプリケーションの実装
- `src/packages/`: 再利用可能なライブラリ
- `scripts/`: ビルドと開発支援ツール

### ビルド関連
- `_dist/`: ビルド成果物（一時的）
- `build.ts`: ビルドオーケストレーション
- 各アプリの `vite.config.ts`: 個別ビルド設定

### 統合関連
- `gecko/`: Firefox との統合設定
- `crates/`: Rust コンポーネント
- `@types/`: 型安全性の確保

この構造により、Floorp は複雑なブラウザアプリケーションを整理された形で管理し、開発者が効率的に作業できる環境を提供しています。