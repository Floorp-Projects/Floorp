# 開発環境セットアップ

## システム要件

### 最低システム要件

#### Windows

- **OS**: Windows 10 (1903) 以降、Windows 11
- **アーキテクチャ**: x64, ARM64
- **メモリ**: 8GB RAM（推奨 16GB）
- **ストレージ**: 10GB 以上の空き容量
- **ネットワーク**: Firefox バイナリのダウンロードに安定したインターネット接続

#### macOS

- **OS**: macOS 10.15 (Catalina) 以降
- **アーキテクチャ**: Intel x64, Apple Silicon (M1/M2)
- **メモリ**: 8GB RAM（推奨 16GB）
- **ストレージ**: 10GB 以上の空き容量
- **ネットワーク**: Firefox バイナリのダウンロードに安定したインターネット接続

#### Linux

- **ディストリビューション**: Ubuntu 20.04+、Debian 11+、Fedora 35+、または同等
- **アーキテクチャ**: x64, ARM64
- **メモリ**: 8GB RAM（推奨 16GB）
- **ストレージ**: 10GB 以上の空き容量
- **ネットワーク**: Firefox バイナリのダウンロードに安定したインターネット接続

## 必要なツール

### 1. Deno（必須）

Floorp のビルドシステムのメインランタイムです。

#### インストール

```bash
# インストールスクリプト利用（Windows/macOS/Linux）
curl -fsSL https://deno.land/install.sh | sh

# Windows（PowerShell利用）
irm https://deno.land/install.ps1 | iex

# macOS（Homebrew利用）
brew install deno

# Linux（Snap利用）
sudo snap install deno

# インストール確認
deno --version
```

#### 必要バージョン

- **最低**: Deno 1.40.0 以降
- **推奨**: 最新安定版

### 2. Node.js（必須）

フロントエンド開発ツールやパッケージ管理に使用します。

#### インストール

```bash
# 公式サイトからダウンロード
# https://nodejs.org/

# Node Version Manager（推奨）
# まずnvmをインストール
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash

# Node.jsをインストール・利用
nvm install 20
nvm use 20

# インストール確認
node --version
npm --version
```

#### 必要バージョン

- **Node.js**: 18.0.0 以降（20.x 推奨）
- **npm**: 9.0.0 以降

### 3. Git（必須）

バージョン管理やパッチ管理に使用します。

#### インストール

```bash
# Windows
# https://git-scm.com/ からダウンロード

# macOS
brew install git

# Linux（Ubuntu/Debian）
sudo apt install git

# インストール確認
git --version
```

## 手順ごとのセットアップ

### 1. リポジトリのクローン

```bash
# リポジトリをクローン
git clone https://github.com/Floorp-Projects/Floorp.git -b main
cd Floorp

# SSHアクセスがある場合
git clone git@github.com:Floorp-Projects/Floorp.git -b main
cd Floorp
```

### 4. Windows のみ（PowerShell のインストール）

Floorp の起動に、PowerShell が必要です。

```powershell
# PowerShell のインストール
winget install --id Microsoft.PowerShell --source winget
```

### 2. 依存関係のインストール

```bash
deno install
```

### 3. 初回ビルド

```bash
# 初回ビルド（Firefoxバイナリをダウンロード）
deno task build

# この処理はネットワーク速度により 10～30分 かかる場合があります
# Firefoxバイナリ（約200～500MB）がダウンロード・展開されます
```

### 4. 開発サーバーの起動

```bash
# 開発モードで起動
deno task dev

# ブラウザが自動で起動します
# コード変更時はホットリロードが有効です
```

## 開発ツールの設定

### Visual Studio Code

#### 推奨拡張機能

```json
{
  "recommendations": [
    "denoland.vscode-deno",
    "bradlc.vscode-tailwindcss",
    "ms-vscode.vscode-typescript-next",
    "esbenp.prettier-vscode",
    "dbaeumer.vscode-eslint"
  ]
}
```

#### 設定例

`.vscode/settings.json` を作成:

```json
{
  "deno.enable": true,
  "deno.lint": true,
  "deno.unstable": true,
  "typescript.preferences.importModuleSpecifier": "relative",
  "editor.defaultFormatter": "denoland.vscode-deno",
  "[typescript]": {
    "editor.defaultFormatter": "denoland.vscode-deno"
  },
  "[javascript]": {
    "editor.defaultFormatter": "denoland.vscode-deno"
  }
}
```

#### タスク設定例

`.vscode/tasks.json` を作成:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Floorp: Dev",
      "type": "shell",
      "command": "deno",
      "args": ["task", "dev"],
      "group": "build",
      "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared"
      },
      "problemMatcher": []
    },
    {
      "label": "Floorp: Build",
      "type": "shell",
      "command": "deno",
      "args": ["task", "build"],
      "group": "build",
      "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared"
      },
      "problemMatcher": []
    }
  ]
}
```

### Neovim 設定例

#### lazy.nvim 利用時

```lua
return {
  -- Denoサポート
  {
    "neovim/nvim-lspconfig",
    opts = {
      servers = {
        denols = {
          root_dir = require("lspconfig.util").root_pattern("deno.json", "deno.jsonc"),
          settings = {
            deno = {
              enable = true,
              lint = true,
              unstable = true,
            },
          },
        },
      },
    },
  },

  -- TypeScriptサポート
  {
    "jose-elias-alvarez/typescript.nvim",
    dependencies = "neovim/nvim-lspconfig",
    opts = {},
  },
}
```

### Vim 設定例

#### 基本設定

```vim
" Denoサポート
let g:deno_enable = v:true
let g:deno_lint = v:true
let g:deno_unstable = v:true

" TypeScript/JavaScript設定
autocmd FileType typescript,javascript setlocal expandtab shiftwidth=2 tabstop=2

" 保存時に自動フォーマット
autocmd BufWritePre *.ts,*.js,*.tsx,*.jsx :!deno fmt %
```

## プラットフォーム別設定

### Windows 設定

#### PowerShell 実行ポリシー

```powershell
# スクリプト実行を許可（管理者で実行）
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

#### Windows Defender 除外設定

パフォーマンス向上のため、以下のディレクトリを Windows Defender の除外に追加してください。ただし、Dev Drive を使用することを強く推奨します。

- プロジェクトルートディレクトリ
- `_dist/` ディレクトリ
- Node.js インストールディレクトリ
- Deno インストールディレクトリ

### macOS 設定

#### Xcode コマンドラインツール

```bash
# Xcodeコマンドラインツールのインストール
xcode-select --install
```

## トラブルシューティング

### 問題: Deno が見つからない

```bash
# 解決策: DenoをPATHに追加
echo 'export PATH="$HOME/.deno/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### 問題: スクリプト実行時に Permission denied

```bash
# 解決策: スクリプトに実行権限を付与
chmod +x build.ts
# または明示的に権限を付与して実行
deno run -A build.ts
```

### 問題: Windows 上でポート関連のエラーが出る

その場合、Windows の設定から開発者モードを有効にして、PC を再起動してください。その後、`deno task dev` を実行してください。

## 次のステップ

セットアップ完了後：

1. **アーキテクチャドキュメントを読む**: Floorp の構造を理解する
2. **コードベースを探索**: メインアプリは `src/apps/main/` から始める
3. **最初の変更を加える**: UI コンポーネントを修正し、ホットリロードを体験する
4. **コミュニティに参加**: Discord や GitHub Discussions で他の開発者と交流

## ヘルプの入手

セットアップ中に問題が発生した場合：

1. **ログを確認**: ビルドや開発サーバーのログに有用なエラーメッセージが含まれていることが多い
2. **既存の Issue を検索**: GitHub Issues で類似の問題を探す
3. **質問する**: GitHub Discussions や Discord で質問
4. **ツールを最新に**: すべての必須ツールが最新か確認

`deno task dev` を実行し、Floorp ブラウザがホットリロード付きで起動すれば開発環境セットアップは完了です。
