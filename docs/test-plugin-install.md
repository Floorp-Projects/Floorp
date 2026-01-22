# Floorp OS プラグインインストール機能 テスト手順

## 概要

このドキュメントでは、Floorp OS プラグインストアからのインストール機能をテストするための環境構築と実行手順を説明します。

## アーキテクチャ

```
┌─────────────────────────────────────────────────────────────────┐
│                         ユーザー                                 │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  Web Store (React + Chakra UI)                                  │
│  http://localhost:5173                                          │
│                                                                 │
│  プラグイン一覧表示 → 詳細ページ → インストールボタン              │
└───────────────────────────┬─────────────────────────────────────┘
                            │ window.floorpPluginStore.installPlugin()
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  Floorp Browser                                                  │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  NRPluginStoreChild (Content Process)                    │   │
│  │  Web API の提供                                          │   │
│  └─────────────────────────┬───────────────────────────────┘   │
│                            │ sendAsyncMessage                   │
│  ┌─────────────────────────▼───────────────────────────────┐   │
│  │  NRPluginStoreParent (Parent Process)                    │   │
│  │  - モーダルダイアログ表示 (shadcn/ui風)                   │   │
│  │  - gRPC-Web呼び出し                                      │   │
│  └─────────────────────────┬───────────────────────────────┘   │
└─────────────────────────────┼───────────────────────────────────┘
                            │ HTTP POST (Connect protocol)
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│  Sapphillon Backend (Rust + tonic)                              │
│  http://localhost:50051                                         │
│                                                                 │
│  PluginService.InstallPlugin({ uri: "https://..." })            │
└─────────────────────────────────────────────────────────────────┘
```

## 必要な環境

### 共通

- macOS (Big Sur以降)、Linux (glibc 2.31以降)、または Windows 10以降
- Git

### Sapphillon Backend

- Rust (Edition 2024)
- Cargo
- SQLite

### Web Store

- Node.js (v18以降推奨)
- pnpm

### Floorp Browser

- Mozilla Build Tools (mach)
- Python 3
- 詳細は [Floorp Build Instructions](https://github.com/AuroraModules/floorp) を参照

---

## 環境構築

### 1. Sapphillon Backend のセットアップ

```bash
# ディレクトリに移動
cd /Users/user/dev-source/sapphillon-dev/Floorp-OS-Automator-Backend

# 依存関係のビルド
cargo build

# データベースのセットアップ（初回のみ）
# SQLiteファイルが自動作成されます
```

### 2. Web Store のセットアップ

```bash
# ディレクトリに移動
cd /Users/user/dev-source/sapphillon-dev/web-store

# 依存関係のインストール
pnpm install
```

### 3. Floorp Browser のビルド

```bash
# ディレクトリに移動
cd /Users/user/dev-source/floorp-dev/floorp

# artifact build を使用する場合（高速）
./mach artifact install

# ビルド
./mach build

# または完全ビルド
# ./mach build
```

---

## テスト実行手順

### Step 1: Sapphillon Backend の起動

```bash
cd /Users/user/dev-source/sapphillon-dev/Floorp-OS-Automator-Backend

# 環境変数の設定（必要に応じて）
export OPENROUTER_API_KEY="your-api-key-here"

# サーバー起動
cargo run -- --db-url "sqlite://./sqlite.db" start
```

**確認ポイント:**

- ログに `gRPC Server starting on 0.0.0.0:50051` が表示されること

### Step 2: Web Store の起動

新しいターミナルを開いて:

```bash
cd /Users/user/dev-source/sapphillon-dev/web-store

# 開発サーバー起動
pnpm dev
```

**確認ポイント:**

- `http://localhost:5173` でアクセス可能であること

### Step 3: Floorp Browser の起動

新しいターミナルを開いて:

```bash
cd /Users/user/dev-source/floorp-dev/floorp

# Floorp 起動
./mach run
```

### Step 4: インストール機能のテスト

1. **Floorp で Web Store を開く**
   - Floorp のアドレスバーに `http://localhost:5173` を入力

2. **プラグインを選択**
   - 任意のプラグインカードをクリック（例: "File System Manager"）
   - プラグイン詳細ページが表示される

3. **インストールボタンをクリック**
   - 緑色の「インストール」ボタンをクリック

4. **モーダルダイアログの確認**
   - 画面中央に shadcn/ui 風のモーダルダイアログが表示される
   - 以下の要素が表示されていることを確認:
     - プラグイン名、作者、バージョンバッジ
     - 説明文
     - 使用する機能一覧（🔐アイコン付き）
     - 警告メッセージ（⚠アイコン付き）
     - 「キャンセル」「追加」ボタン

5. **インストール実行**
   - 「追加」ボタンをクリック

6. **コンソールログの確認**
   - Floorp のブラウザコンソール（`Ctrl+Shift+J`）で以下を確認:

   ```
   [NRPluginStoreParent] User confirmed installation
   [NRPluginStoreParent] Calling Sapphillon InstallPlugin API
   [gRPC-Web] Calling sapphillon.v1.PluginService/InstallPlugin
   [gRPC-Web] URL: http://localhost:50051/sapphillon.v1.PluginService/InstallPlugin
   [gRPC-Web] Request: { "uri": "https://plugins.floorp.app/packages/..." }
   ```

   - Sapphillon Backend のターミナルで gRPC リクエストを受信したログを確認

---

## 期待される動作

### 正常系

1. モーダルが表示される
2. 「追加」クリック後、gRPC-Web リクエストが送信される
3. インストール成功メッセージが表示される
4. ボタンが「インストール済み」に変わる

### 異常系（Backend未起動時）

1. モーダルは正常に表示される
2. 「追加」クリック後、コンソールにエラーが表示される:
   ```
   [NRPluginStoreParent] Failed to call Sapphillon API: TypeError: NetworkError...
   ```
3. 現在の実装ではエラーでもインストール成功として処理される（後で変更可能）

---

## デバッグ方法

### Floorp ブラウザコンソール

- `Ctrl+Shift+J` (Windows/Linux) または `Cmd+Option+J` (macOS)
- `[NRPluginStoreParent]` や `[gRPC-Web]` でフィルタリング

### Sapphillon Backend ログ

- ターミナルに gRPC リクエストの詳細が出力される
- `RUST_LOG=debug` 環境変数でより詳細なログを表示

### Web Store デベロッパーツール

- 通常のブラウザ DevTools を使用
- `useFloorpPluginStore` フックの動作を確認

---

## トラブルシューティング

### 「gRPC-Web call failed」エラー

**原因:** Sapphillon Backend が起動していない、またはポートが異なる

**解決策:**

1. Backend が `0.0.0.0:50051` で起動していることを確認
2. ファイアウォール設定を確認
3. `NRPluginStoreParent.sys.mts` の `SAPPHILLON_GRPC_BASE_URL` を確認

### モーダルが表示されない

**原因:** Actor が正しく登録されていない可能性

**解決策:**

1. `./mach build` で再ビルド
2. ブラウザコンソールでエラーを確認
3. `about:config` で `browser.tabs.remote.autostart` を確認

### Web Store が Floorp を検出しない

**原因:** `window.floorpPluginStore` が注入されていない

**解決策:**

1. URL が許可リスト（`localhost:5173`）に含まれていることを確認
2. ページをリロード
3. コンソールで `window.floorpPluginStore` が存在するか確認

---

## 関連ファイル

| ファイル                                                      | 説明                                       |
| ------------------------------------------------------------- | ------------------------------------------ |
| `browser-features/modules/actors/NRPluginStoreParent.sys.mts` | 親プロセス Actor、モーダルUI、gRPC呼び出し |
| `browser-features/modules/actors/NRPluginStoreChild.sys.mts`  | 子プロセス Actor、Web API提供              |
| `web-store/src/pages/PluginDetailPage.tsx`                    | プラグイン詳細ページ                       |
| `web-store/src/hooks/useFloorpPluginStore.ts`                 | Floorp API フック                          |
| `web-store/src/data/mockPlugins.ts`                           | モックプラグインデータ                     |
