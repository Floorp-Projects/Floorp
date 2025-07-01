# Floorp Browser Documentation

Floorp Browser の包括的なドキュメントへようこそ。このドキュメントは Floorp の開発者と貢献者のために作成されています。

## 📚 ドキュメント構成

### 🏗️ アーキテクチャ
- [**プロジェクト概要**](./architecture/overview.md) - Floorp の全体的なアーキテクチャと設計思想
- [**ディレクトリ構造**](./architecture/directory-structure.md) - プロジェクトのディレクトリ構成の詳細解説
- [**技術スタック**](./architecture/tech-stack.md) - 使用している技術とフレームワーク
- [**ビルドシステム**](./architecture/build-system.md) - ビルドパイプラインと処理フロー

### 🔧 開発
- [**開発環境セットアップ**](./development/setup.md) - 開発環境の構築方法
- [**開発ワークフロー**](./development/workflow.md) - 日常的な開発作業の流れ
- [**デバッグ・テスト**](./development/debugging-testing.md) - デバッグとテストの方法
- [**コーディング規約**](./development/coding-standards.md) - コードスタイルと規約

### 📱 アプリケーション
- [**メインアプリケーション**](./applications/main-app.md) - ブラウザのメイン機能
- [**設定アプリケーション**](./applications/settings.md) - 設定・環境設定UI
- [**新しいタブ**](./applications/newtab.md) - 新しいタブページ
- [**その他のアプリケーション**](./applications/other-apps.md) - ノート、ウェルカム画面など

### 📦 パッケージ・ライブラリ
- [**Solid-XUL**](./packages/solid-xul.md) - SolidJS と XUL の統合
- [**スキンシステム**](./packages/skin-system.md) - テーマとスタイリング
- [**ユーザースクリプト**](./packages/user-scripts.md) - ユーザースクリプト実行エンジン
- [**共有ライブラリ**](./packages/shared-libraries.md) - 共通ライブラリとユーティリティ

### 🦀 Rust コンポーネント
- [**Nora-Inject**](./rust/nora-inject.md) - Rust ベースのコード注入システム
- [**WebAssembly 統合**](./rust/webassembly.md) - WASM モジュールの構成と使用
- [**パフォーマンス最適化**](./rust/performance.md) - Rust による高速化

### 🔌 Firefox 統合
- [**Gecko 統合**](./firefox/gecko-integration.md) - Firefox エンジンとの統合
- [**パッチシステム**](./firefox/patch-system.md) - Firefox へのパッチ適用
- [**XUL と WebExtensions**](./firefox/xul-webextensions.md) - UI 拡張とブラウザ API

### 🚀 デプロイメント
- [**ビルドとリリース**](./deployment/build-release.md) - 本番環境向けビルド
- [**インストーラー**](./deployment/installers.md) - プラットフォーム別インストーラー
- [**配布と更新**](./deployment/distribution.md) - 配布方法と自動更新

### 🔐 セキュリティ
- [**セキュリティ考慮事項**](./security/considerations.md) - セキュリティ設計と対策
- [**コード注入の安全性**](./security/code-injection.md) - 安全なコード注入の実装
- [**更新メカニズム**](./security/update-mechanism.md) - セキュアな更新システム

### 🤝 貢献
- [**貢献ガイド**](./contributing/guide.md) - プロジェクトへの貢献方法
- [**コードレビュー**](./contributing/code-review.md) - コードレビューのプロセス
- [**イシューとPR**](./contributing/issues-prs.md) - イシューとプルリクエストの管理

## 🚀 クイックスタート

1. **開発環境のセットアップ**
   ```bash
   # Deno をインストール
   curl -fsSL https://deno.land/install.sh | sh
   
   # リポジトリをクローン
   git clone <repository-url>
   cd floorp
   
   # 依存関係をインストール
   deno install
   ```

2. **開発サーバーの起動**
   ```bash
   # 開発モードで起動（ホットリロード有効）
   deno task dev
   ```

3. **ビルド**
   ```bash
   # 本番環境向けビルド
   deno task build
   ```

## 📋 必要な知識

Floorp の開発に参加するために推奨される技術知識：

- **TypeScript/JavaScript** - アプリケーション開発
- **SolidJS** - リアクティブ UI フレームワーク
- **Rust** - パフォーマンス重要な部分（オプション）
- **Firefox/Gecko** - ブラウザエンジンの基礎知識
- **Deno** - ランタイムとビルドツール

## 🆘 サポート

- **Discord**: [公式 Discord サーバー](https://discord.floorp.app)
- **GitHub Issues**: バグ報告と機能要求
- **GitHub Discussions**: 一般的な質問と議論

## 📄 ライセンス

このプロジェクトは [Mozilla Public License 2.0](../LICENSE) の下でライセンスされています。

---

**注意**: このドキュメントは継続的に更新されています。最新の情報については、各セクションの個別ドキュメントを参照してください。