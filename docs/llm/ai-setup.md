# AI アシスタントのセットアップガイド

このドキュメントは、Floorp プロジェクトで AI アシスタント（Claude、GitHub Copilot、Cursor など）を効果的に使用するためのセットアップガイドです。

## セットアップ済みのファイル

プロジェクトには以下の AI 向けコンテキストファイルが用意されています：

### 1. `.claude/context.md`

**用途**: Claude (Anthropic) や Claude Code 向けのコンテキストファイル

**内容**:
- プロジェクトの概要
- クイックリファレンス
- 技術スタック
- 開発コマンド
- コーディング規約
- よくある質問

**使い方**:
- Claude Code は自動的にこのファイルを読み込みます
- Claude.ai で手動で参照することも可能

### 2. `.cursorrules`

**用途**: Cursor エディタおよび他の AI コーディングアシスタント向けのルールファイル

**内容**:
- コーディング規約
- 型定義のベストプラクティス
- 新機能追加のチェックリスト
- よくある問題と解決策
- 開発コマンド

**使い方**:
- Cursor エディタが自動的に読み込みます
- 他の `.cursorrules` 対応エディタでも使用可能

### 3. `.vscode/settings.json`

**AI 関連設定**:
```json
{
  "github.copilot.enable": {
    "*": true,
    "yaml": true,
    "plaintext": false,
    "markdown": true
  },
  "continue.telemetryEnabled": false,
  "continue.enableTabAutocomplete": true
}
```

### 4. `.vscode/extensions.json`

**推奨 AI 拡張機能**:
- `github.copilot` - GitHub Copilot
- `github.copilot-chat` - GitHub Copilot Chat
- `continue.continue` - Continue (オープンソース AI アシスタント)

## AI アシスタント別セットアップ

### Claude Code (VS Code 拡張機能)

1. **インストール**:
   - VS Code で Claude Code 拡張機能をインストール
   - Anthropic API キーを設定

2. **プロジェクトコンテキストの読み込み**:
   - Claude Code は自動的に `.claude/context.md` を読み込みます
   - 追加で `docs/llm/` ディレクトリのドキュメントも参照できます

3. **使用方法**:
   - コマンドパレット (`Ctrl+Shift+P`) から "Claude Code" を検索
   - チャット機能でプロジェクトについて質問
   - コード生成時にコンテキストを活用

### GitHub Copilot

1. **インストール**:
   ```bash
   code --install-extension github.copilot
   code --install-extension github.copilot-chat
   ```

2. **認証**:
   - VS Code で GitHub アカウントにサインイン
   - Copilot サブスクリプションが必要

3. **プロジェクトコンテキスト**:
   - Copilot は `.cursorrules` を参照
   - プロジェクトのファイル構造から自動的にコンテキストを学習

4. **推奨設定**:
   - `.vscode/settings.json` に既に設定済み
   - YAML や Markdown でも有効

### Cursor エディタ

1. **プロジェクトを開く**:
   ```bash
   cursor .
   ```

2. **コンテキストの読み込み**:
   - Cursor は自動的に `.cursorrules` を読み込みます
   - `docs/llm/` ディレクトリを "@docs" として参照可能

3. **使用方法**:
   - `Ctrl+K` でインラインコード生成
   - `Ctrl+L` でチャット
   - `@codebase` でプロジェクト全体を参照

### Continue (オープンソース)

1. **インストール**:
   ```bash
   code --install-extension continue.continue
   ```

2. **設定**:
   - `.vscode/settings.json` に設定済み
   - `~/.continue/config.json` でモデルを設定

3. **プロジェクトコンテキスト**:
   - `.cursorrules` を自動的に参照
   - カスタムプロンプトで `docs/llm/` を参照可能

### その他の AI ツール

#### Codeium

- `.cursorrules` を参照
- VS Code 拡張機能として利用可能

#### Tabnine

- プロジェクトのコードから学習
- 特別な設定ファイルは不要

#### Amazon CodeWhisperer

- `.cursorrules` を参照
- AWS アカウントが必要

### MCP サーバー: Chrome DevTools MCP

- 参照: [ChromeDevTools/chrome-devtools-mcp](https://github.com/ChromeDevTools/chrome-devtools-mcp)
- 提供ツール（抜粋）:
  - UI 操作: `click` / `fill_form` / `hover` など
  - ネットワーク・コンソール・スクリーンショット: `list_network_requests` / `list_console_messages` / `take_screenshot`
  - パフォーマンス: `performance_start_trace` / `performance_analyze_insight`
- 設定例（各 MCP 対応クライアントで共通）:
  ```json
  {
    "mcpServers": {
      "chrome-devtools": {
        "command": "npx",
        "args": ["-y", "chrome-devtools-mcp@latest"]
      }
    }
  }
  ```
- サンドボックス環境や既存ブラウザに接続する場合:
  - Chrome を `--remote-debugging-port=9222 --user-data-dir=/tmp/chrome-profile-stable` などで起動する
  - MCP 設定の `args` に `--browser-url=http://127.0.0.1:9222` を追加して接続する
- 注意: 接続中のブラウザ状態（タブ/ネットワーク/コンソール）が MCP クライアントから操作・閲覧可能になるため、機密ページを開いたままにしないこと

## AI に提供されるコンテキスト

### 常に参照すべきドキュメント

AI アシスタントに以下のドキュメントを参照させることを推奨します：

1. **[docs/llm/README.md](./README.md)** - ドキュメント索引
2. **[docs/llm/project-overview.md](./project-overview.md)** - プロジェクト概要
3. **[docs/llm/development-notes.md](./development-notes.md)** - 開発ガイド
4. **[docs/llm/architecture-deep-dive.md](./architecture-deep-dive.md)** - アーキテクチャ詳細
5. **[.claude/context.md](../../.claude/context.md)** - クイックリファレンス
6. **[.cursorrules](../../.cursorrules)** - コーディングルール

### タスク別のコンテキスト提供

#### バグ修正
```
@docs/llm/development-notes.md の「よくある問題」を参照して、
{問題の説明} を解決してください。
```

#### 新機能追加
```
@docs/llm/project-overview.md と @docs/llm/architecture-deep-dive.md を参照して、
{機能の説明} を実装してください。
@.cursorrules のチェックリストに従ってください。
```

#### リファクタリング
```
@docs/llm/architecture-deep-dive.md を参照して、
{コードの場所} をリファクタリングしてください。
既存のアーキテクチャパターンに従ってください。
```

## ベストプラクティス

### 1. 明確なコンテキストを提供

AI に質問する際は、以下を明確にしてください：

```
タスク: 新しいブラウザ機能「タブグループ」を追加

背景:
- 場所: browser-features/chrome/common/tab-groups/
- 技術: SolidJS
- 参考: workspaces/ の実装

要件:
1. タブをグループ化する機能
2. グループごとに色を設定
3. グループの折りたたみ/展開
4. 設定の永続化

参照ドキュメント:
- @docs/llm/development-notes.md
- @.cursorrules
```

### 2. 段階的に進める

複雑な機能は段階的に実装：

```
ステップ 1: 型定義を types.ts に作成
ステップ 2: サービスクラスを実装
ステップ 3: UI コンポーネントを作成
ステップ 4: 機能を登録
ステップ 5: 翻訳を追加
```

### 3. コーディング規約を守る

AI に生成させたコードは必ず以下を確認：

- [ ] 型定義が別ファイル（`types.ts`）に分離されているか（`.sys.mts` 以外）
- [ ] `any` 型を使用していないか
- [ ] SolidJS で直接変数を変更していないか
- [ ] エラーハンドリングが適切か
- [ ] コメントが適切に追加されているか

### 4. レビューは人間が行う

AI が生成したコードは必ず人間がレビューしてください：

- ロジックが正しいか
- パフォーマンスが適切か
- セキュリティ上の問題がないか
- プロジェクトの規約に従っているか

## トラブルシューティング

### AI が古い情報を参照している

**症状**: AI が `.cursorrules` や `.claude/context.md` の内容を反映していない

**解決策**:
1. エディタを再起動
2. 明示的にファイルを参照させる: `@.cursorrules を参照してください`
3. キャッシュをクリア（エディタの設定から）

### AI の提案がプロジェクトの規約に合わない

**症状**: 生成されたコードがコーディング規約に従っていない

**解決策**:
1. `.cursorrules` の内容を確認
2. より具体的な指示を与える
3. 生成後に手動で修正

### AI がプロジェクト構造を理解していない

**症状**: ファイルを間違った場所に配置しようとする

**解決策**:
1. `@docs/llm/project-overview.md` を明示的に参照
2. 正しいディレクトリ構造を指示
3. 類似機能の例を示す

## 更新履歴

### 2025-01-30
- 初版作成
- `.claude/context.md` 追加
- `.cursorrules` 追加
- VS Code 設定更新

---

このセットアップにより、AI アシスタントが Floorp プロジェクトのコンテキストを理解し、より適切な支援を提供できるようになります。

## フィードバック

AI セットアップに関する改善提案があれば、Issue または PR を作成してください。
