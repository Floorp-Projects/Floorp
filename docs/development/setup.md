# 開発環境セットアップ

## 必要な環境

### システム要件

#### Windows
- **OS**: Windows 10 以降 (Windows 7/8 はサポート外)
- **アーキテクチャ**: x86_64 (AArch64 はサポート外)
- **PowerShell**: PowerShell 7 推奨
- **メモリ**: 8GB 以上推奨
- **ストレージ**: 10GB 以上の空き容量

#### macOS
- **OS**: macOS 10.15 以降
- **アーキテクチャ**: x86_64 & ARM64 (Universal Binary)
- **Xcode**: 最新版推奨
- **メモリ**: 8GB 以上推奨
- **ストレージ**: 10GB 以上の空き容量

#### Linux
- **ディストリビューション**: Ubuntu 20.04+, Debian 11+, Arch Linux
- **アーキテクチャ**: x86_64 & AArch64
- **必要パッケージ**: build-essential, curl, git
- **メモリ**: 8GB 以上推奨
- **ストレージ**: 10GB 以上の空き容量

### 必要なツール

#### 1. Deno (必須)
```bash
# Linux/macOS
curl -fsSL https://deno.land/install.sh | sh

# Windows (PowerShell)
irm https://deno.land/install.ps1 | iex

# Homebrew (macOS)
brew install deno

# Chocolatey (Windows)
choco install deno

# Scoop (Windows)
scoop install deno
```

#### 2. Node.js (推奨)
```bash
# Node.js 18+ 推奨
# nvm を使用した場合
nvm install 18
nvm use 18

# 直接インストール
# https://nodejs.org/ からダウンロード
```

#### 3. Rust (Rust コンポーネント開発時)
```bash
# Rustup を使用
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh

# WebAssembly ターゲットを追加
rustup target add wasm32-wasi
```

#### 4. Python (一部スクリプト用)
```bash
# Python 3.8+ 推奨
# システムのパッケージマネージャーまたは公式サイトからインストール
```

## セットアップ手順

### 1. リポジトリのクローン

```bash
# HTTPS
git clone https://github.com/Floorp-Projects/Floorp.git
cd Floorp

# SSH (推奨)
git clone git@github.com:Floorp-Projects/Floorp.git
cd Floorp
```

### 2. 依存関係のインストール

```bash
# Deno 依存関係のインストール
deno install

# Node.js 依存関係のインストール (オプション)
npm install

# または pnpm を使用
pnpm install
```

### 3. 初回ビルド

```bash
# 開発用ビルドの実行
deno task dev
```

初回実行時は以下の処理が自動的に行われます：

1. **Firefox バイナリのダウンロード**
   - GitHub Releases から最新のビルド済みバイナリを取得
   - プラットフォーム別のアーカイブを自動選択

2. **バイナリの展開**
   - ダウンロードしたアーカイブを `_dist/bin/` に展開
   - プラットフォーム固有の処理を実行

3. **パッチの適用**
   - Firefox に必要なカスタマイズパッチを適用
   - Git パッチシステムを使用

4. **アプリケーションのビルド**
   - TypeScript/SolidJS アプリケーションをコンパイル
   - Rust コンポーネントをビルド (必要に応じて)

5. **開発サーバーの起動**
   - 各アプリケーションの開発サーバーを起動
   - ホットリロード機能を有効化

### 4. 開発環境の確認

ビルドが成功すると、カスタマイズされた Firefox ブラウザが起動します。以下を確認してください：

- ブラウザが正常に起動する
- 開発者ツールが利用可能
- ホットリロードが機能する
- カスタム UI が表示される

## 開発ツールの設定

### Visual Studio Code

推奨される拡張機能：

```json
{
  "recommendations": [
    "denoland.vscode-deno",
    "rust-lang.rust-analyzer",
    "bradlc.vscode-tailwindcss",
    "esbenp.prettier-vscode",
    "ms-vscode.vscode-typescript-next"
  ]
}
```

VS Code 設定 (`.vscode/settings.json`):

```json
{
  "deno.enable": true,
  "deno.lint": true,
  "deno.unstable": true,
  "typescript.preferences.importModuleSpecifier": "relative",
  "editor.formatOnSave": true,
  "editor.defaultFormatter": "denoland.vscode-deno",
  "[typescript]": {
    "editor.defaultFormatter": "denoland.vscode-deno"
  },
  "[rust]": {
    "editor.defaultFormatter": "rust-lang.rust-analyzer"
  }
}
```

### その他のエディタ

#### Neovim
```lua
-- Deno LSP 設定
require('lspconfig').denols.setup{
  root_dir = require('lspconfig').util.root_pattern("deno.json", "deno.jsonc"),
  init_options = {
    lint = true,
    unstable = true,
  },
}
```

#### Vim
```vim
" Deno プラグイン
Plug 'vim-denops/denops.vim'
Plug 'lambdalisue/deno.vim'
```

## 環境変数の設定

開発に必要な環境変数：

```bash
# .env ファイル (ルートディレクトリに作成)
# Firefox バイナリのカスタムパス (オプション)
FIREFOX_BINARY_PATH=/path/to/custom/firefox

# 開発モードの設定
NODE_ENV=development
DENO_ENV=development

# ログレベル
LOG_LEVEL=debug

# プロキシ設定 (必要に応じて)
HTTP_PROXY=http://proxy.example.com:8080
HTTPS_PROXY=http://proxy.example.com:8080
```

## プラットフォーム固有の設定

### Windows

#### PowerShell の設定
```powershell
# 実行ポリシーの設定
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser

# 開発者モードの有効化 (Windows 10/11)
# 設定 > 更新とセキュリティ > 開発者向け > 開発者モード
```

#### Windows Defender の除外設定
```powershell
# プロジェクトディレクトリを除外
Add-MpPreference -ExclusionPath "C:\path\to\Floorp"
```

### macOS

#### Xcode Command Line Tools
```bash
xcode-select --install
```

#### Homebrew の設定
```bash
# Homebrew がインストールされていない場合
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 必要なパッケージのインストール
brew install git curl
```

### Linux

#### Ubuntu/Debian
```bash
# 必要なパッケージのインストール
sudo apt update
sudo apt install -y build-essential curl git pkg-config libssl-dev

# Firefox の依存関係
sudo apt install -y libgtk-3-dev libdbus-glib-1-dev libxt6
```

#### Arch Linux
```bash
# 必要なパッケージのインストール
sudo pacman -S base-devel curl git

# Firefox の依存関係
sudo pacman -S gtk3 dbus-glib libxt
```

#### CentOS/RHEL/Fedora
```bash
# 必要なパッケージのインストール
sudo dnf install -y gcc gcc-c++ make curl git openssl-devel

# Firefox の依存関係
sudo dnf install -y gtk3-devel dbus-glib-devel libXt-devel
```

## トラブルシューティング

### よくある問題と解決方法

#### 1. Deno のパーミッションエラー
```bash
# 問題: Deno がファイルアクセスを拒否される
# 解決: --allow-all フラグを使用
deno run --allow-all build.ts
```

#### 2. Firefox バイナリのダウンロード失敗
```bash
# 問題: ネットワークエラーでバイナリがダウンロードできない
# 解決: 手動でダウンロードして配置
curl -L -o floorp-linux-amd64-moz-artifact.tar.xz \
  https://github.com/Floorp-Projects/Floorp-runtime/releases/latest/download/floorp-linux-amd64-moz-artifact.tar.xz
```

#### 3. ポート競合エラー
```bash
# 問題: 開発サーバーのポートが使用中
# 解決: 使用中のプロセスを終了
lsof -ti:3000 | xargs kill -9

# または別のポートを使用
export VITE_PORT=3001
```

#### 4. メモリ不足エラー
```bash
# 問題: ビルド中にメモリ不足
# 解決: Node.js のメモリ制限を増加
export NODE_OPTIONS="--max-old-space-size=4096"
```

#### 5. Rust コンパイルエラー
```bash
# 問題: Rust コンポーネントのビルド失敗
# 解決: Rust ツールチェーンの更新
rustup update
rustup target add wasm32-wasi
```

### デバッグ情報の収集

問題が発生した場合、以下の情報を収集してください：

```bash
# システム情報
deno --version
node --version
rustc --version

# 環境情報
echo $PATH
echo $DENO_DIR
echo $NODE_ENV

# ログの確認
deno task dev --verbose
```

## 次のステップ

セットアップが完了したら、以下のドキュメントを参照してください：

- [開発ワークフロー](./workflow.md) - 日常的な開発作業の流れ
- [デバッグ・テスト](./debugging-testing.md) - デバッグとテストの方法
- [コーディング規約](./coding-standards.md) - コードスタイルと規約

## サポート

セットアップで問題が発生した場合：

1. **GitHub Issues**: バグ報告と質問
2. **Discord**: [公式 Discord サーバー](https://discord.floorp.app)
3. **ドキュメント**: [公式ドキュメント](https://docs.floorp.app)

開発環境が正常に動作することを確認してから、実際の開発作業を開始してください。