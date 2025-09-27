# Floorp OS Server API Specification

このリポジトリには、Floorp OS Local HTTP Server のOpenAPI仕様と型定義が含まれています。

**このリポジトリは独立したAPI仕様リポジトリで、Floorpメインリポジトリでサブモジュールとして使用されています。**

## 概要

Floorp OS Local HTTP Serverは、ローカルアプリケーション向けにブラウザ機能へのHTTP/JSON APIアクセスを提供します。

- ループバック専用サーバー (127.0.0.1)
- オプションのBearer token認証
- ブラウザ情報、Webスクレイピング、タブ管理機能を公開

## ファイル構成

- `openapi.yaml` - OpenAPI 3.0.3仕様書
- `types.ts` - TypeScript型定義
- `index.sys.mts` - 型定義のエクスポート
- `deno.json` - Denoプロジェクト設定とタスク定義

## 使用方法

### 1. TypeScript クライアントの生成

```bash
deno task generate-typescript
```

### 2. API仕様の検証

```bash
deno task validate
deno task lint-spec
```

### 3. TypeScript型チェック

```bash
deno task check
```

### 4. コードフォーマット

```bash
deno task fmt
```

### 5. Denoリント

```bash
deno task lint
```

### 6. ドキュメント生成

```bash
deno task docs
```

## API エンドポイント概要

### Browser Information
- `GET /browser/tabs` - ブラウザタブの取得
- `GET /browser/history` - 閲覧履歴の取得
- `GET /browser/downloads` - ダウンロード履歴の取得
- `GET /browser/context` - 統合ブラウザコンテキストの取得

### Web Scraper
- `POST /scraper/instances` - スクレイパーインスタンスの作成
- `POST /scraper/instances/{id}/navigate` - ページナビゲーション
- `GET /scraper/instances/{id}/html` - HTMLコンテンツの取得
- `POST /scraper/instances/{id}/click` - 要素のクリック
- `GET /scraper/instances/{id}/screenshot` - スクリーンショットの取得

### Tab Manager
- `GET /tabs/list` - ブラウザタブの一覧
- `POST /tabs/instances` - タブインスタンスの作成
- `POST /tabs/attach` - 既存タブへのアタッチ

## 認証

デフォルトでは認証は無効ですが、オプションでBearer token認証を有効にできます。

## Protocol Buffers からの移行

このOpenAPI仕様は、従来のProtocol Buffers定義を置き換えるものです。主な利点：

1. **標準化**: OpenAPIは業界標準のAPI仕様記述フォーマット
2. **ツールサポート**: 豊富なコード生成ツールとドキュメント生成ツール
3. **可読性**: YAML形式による人間にとって読みやすい仕様
4. **エコシステム**: Swagger UI、Postman等の既存ツールとの統合

## サブモジュールとしての使用

このリポジトリは、Floorpメインリポジトリでgitサブモジュールとして使用されます：

```bash
# メインリポジトリでサブモジュールを更新
git submodule update --remote

# サブモジュールの変更をコミット
git add api-spec
git commit -m "Update API specification"
```

## ライセンス

Mozilla Public License 2.0
