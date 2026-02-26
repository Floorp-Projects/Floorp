## 本記事は何？

本記事は、Firefox フォークの **Floorp** を開発している大学 2 年生が、未踏 IT（2025）採択プロジェクト **「Floorp OS」** の開発で得た知見を、**「拡張機能や Selenium に頼らず、ブラウザ内部に LLM 実行基盤を組み込む」** という観点で公開するために書いた記事です。文章の校正などに生成 AI を用いていますが、設計判断や実装方針は実装経験に基づいて記述します。

LLM ブラウザの実装方法は複数あると思いますが、ここでは筆者（私）が実装した方法を採用します。非常にベーシックな構成を採用しているはずなので、他の実装と比べて機能・洗練度で劣る部分はあるかもしれません。とはいえ、入門として **十分に機能する構成** を狙います。

**注意：本記事を読んで「安直」にブラウザ開発を始めることは推奨されません。** ブラウザ開発は一定以上の労力が必要で、途中で心が折れてもユーザーがいるなら **セキュリティ修正をマージし続け、リリースし続ける責任** が伴います。その覚悟がある場合のみ、本記事を参考にしてください。

また、未踏 IT の Floorp OS では **Workflow 形式**（静的に構成・解析しやすい）も採用しましたが、この記事では説明を簡単にするため、**Agent 形式**（動的に判断しながらツールを使う）を採用します。基盤となる要素（Tools、実行ログ、ガードレール、UI）は共通です。

---

## このリポジトリでの対応先（Floorp）

本記事のコード例・設計は、以下の実装/配置に対応しています。（ブランチが違うので、https://github.com/Floorp-Projects/Floorp/tree/Do-Not-Release を参考にしてください）

- チャット UI: `browser-features/pages-llm-chat/`
- LLM 呼び出し/Agent 実装の一部（ページ側）: `browser-features/pages-llm-chat/src/lib/`
- ブラウザ側 LLM モジュール: `browser-features/modules/modules/llm/`
- プロバイダー設定 UI: `browser-features/pages-settings/src/app/llm-providers/`
- Actor 通信基盤（`NRSettings` など）: `browser-features/modules/actors/`

---

## 対象読者と前提

- Firefox / Chromium のどちらかのコードを **ビルドしてデバッグできる**
- TypeScript/React の基礎
- 「LLM のツール呼び出し（function calling）」の概念を薄く知っている（詳説はしません）

> **ゴール**: 「チャット UI → LLM → ツール実行 → ブラウザ操作 → 結果を LLM に返す」までを **ブラウザ内**で完結させ、ストリーミング表示と実行ログ（ツール実行状態）を備えた最小の LLM ブラウザ機能を作る。

---

## LLM ブラウザとは？

OpenAI の Atlas、The Browser Company の Dia、GenSpark などが提供するような、Web ページに対する「読み取り・操作（自動操縦）」を LLM が担うタイプのブラウザ（またはブラウザ機能）をここでは **LLM ブラウザ** と呼びます。

### 既存アプローチとの違い

従来の「ブラウザ × AI」アプローチには大きく 3 つの方法があります。

| アプローチ         | 代表例                | 特徴                                    | 課題                                                                         |
| ------------------ | --------------------- | --------------------------------------- | ---------------------------------------------------------------------------- |
| **拡張機能**       | 各種 Chrome 拡張      | 導入が容易                              | WebExtension API の制約が大きい。ブラウザ内部 API にアクセス不可             |
| **外部自動化**     | Selenium / Playwright | 任意操作が可能                          | 別プロセスで動作し、オーバーヘッドが大きい。ユーザー向けブラウザとして不自然 |
| **ネイティブ統合** | 本記事のアプローチ    | ブラウザ内部 API に直接アクセス、低遅延 | ブラウザ本体の改造が必要                                                     |

本記事では 3 番目の **ネイティブ統合** を採用します。ブラウザのソースコードに直接手を入れることで、拡張機能の権限制約やプロセス間通信のオーバーヘッドを回避し、LLM がブラウザをシームレスに操作できる環境を構築します。

本記事では、この LLM ブラウザを作るための **設計（アーキテクチャ、ツール設計、ガードレール）** から **実装（チャット、ストリーミング、ツール実行ループ）** までを、実装者視点でまとめます。

---

# 設計と準備

## アーキテクチャ

LLM ブラウザを作る上で必要な基盤となるパーツは、マシンスペックや採用技術の差を除けば概ね次の **5 つ**です。

1. **ベースとなる Web ブラウザ**：Chromium / Firefox など。フォーク元となるブラウザエンジン
2. **実行機構**：Agent / Workflow をブラウザ内部で動かすためのループ処理
3. **LLM**：テキスト生成とツール選択を担う大規模言語モデル（OpenAI API など）
4. **Tools**：LLM が呼び出せるブラウザ操作（ページ読み取り、フォーム入力、タブ操作など）
5. **実装言語**：TypeScript

これらの関係を図にすると次のようになります。

```
┌───────────┐    ユーザー入力     ┌──────────────┐
│  ユーザー  │ ───────────────→ │  チャット UI  │
└───────────┘                   └──────┬───────┘
                                       │ messages[]
                                       ▼
                               ┌──────────────┐
                               │   LLM API    │
                               │  (OpenAI等)   │
                               └──────┬───────┘
                                      │ text / tool_calls(delta)
                                      ▼
                               ┌──────────────┐
                               │ Agentic Loop │ ← tool 結果を受けて再度 LLM へ
                               └──────┬───────┘
                                      │ tool 実行
                                      ▼
                               ┌──────────────┐
                               │ Actor (RPC)  │ ← Firefox の親プロセスと通信
                               └──────┬───────┘
                                      │ ブラウザ API
                                      ▼
                               ┌──────────────┐
                               │ Firefox 内部  │
                               └──────────────┘
```

筆者の都合上 Chromium を触れないため、Firefox 系を採用します。それ以外についても、Firefox のアーキテクチャに合わせた構成を採用します。

- ベース：**Floorp**（Firefox fork）
- 実行機構：**Agentic Loop**（この記事の中心テーマ）
- LLM：**OpenAI API**（OpenAI 互換 API であれば Ollama なども利用可能）
- Tools 呼び出し：**Actor**（Firefox のマルチプロセス通信機構）
- 実装言語：**TypeScript**（UI / Tools 定義 / RPC クライアント）

---

## 下準備

下準備として、まずは Floorp をビルドして変更を加えられる（hack できる）状態にします。ビルド手順は以下を参照してください。

- [https://docs.floorp.app/docs/building/](https://docs.floorp.app/docs/building/)

ビルドを終えてデバッグ版 Floorp が起動したら次へ進みます。

なお、本記事で実装した内容は `Do-Not-Release` ブランチで見ることができます（削除する可能性あり）。公式の Floorp に本コードは統合する予定はないためです。

- [https://github.com/Floorp-Projects/Floorp/tree/Do-Not-Release](https://github.com/Floorp-Projects/Floorp/tree/Do-Not-Release)

---

## Floorp の構造を理解する（超ざっくり）

Floorp は [https://github.com/f3liz-dev/noraneko](https://github.com/f3liz-dev/noraneko) を利用した Firefox ベースの Web ブラウザです。

```
┌─────────────────────────────────────────────────────┐
│               Floorp Browser                        │
├─────────────────────────────────────────────────────┤
│  React Pages        │  SolidJS Chrome               │
│  (設定画面など)      │  (ブラウザ UI 機能)            │
├─────────────────────┴───────────────────────────────┤
│              Bridge Layer (HMR対応ローダー)          │
├─────────────────────────────────────────────────────┤
│  ESM Modules (.sys.mts)                             │
│  - Actor (親子プロセス通信)                           │
│  - OS APIs (ブラウザ操作サービス)                     │
│  - OS Server (ローカルHTTPサーバー)                   │
├─────────────────────────────────────────────────────┤
│  Firefox Base (Gecko Engine)                        │
└─────────────────────────────────────────────────────┘
```

本記事で実装する LLM チャットは **React Pages** に置き、ブラウザ操作は **ESM Modules（親プロセス）** の Actor を経由して実行します。

---

## `browser-features/modules/actors/webscraper` を深く理解する（重要）

このディレクトリは、LLM ブラウザ実装における「ページ読み取り・DOM 操作の実体」に近い層です。`pages-llm-chat` 側から見ると「`client.scraperClick(...)` の先」でしかありませんが、実際にはここで **互換性（イベント列/セッター）** や **可観測性（ハイライト/情報パネル）** を含む難しさを吸収しています。

特に重要なのは次の 4 点です。

- Firefox の **親子プロセス境界**をまたぐメッセージ設計
- **XrayWrapper**（特権コンテキストとページコンテキスト差）への対処
- React/Vue 等が期待する **イベント列の再現**（click / input / change など）
- **可視化（ハイライト/情報パネル）**によるデバッグ性・安全性の向上

LLM ブラウザを作ると、モデル精度の問題と DOM 操作の問題が混ざって見えます。`webscraper` の設計を理解しておくと、「モデルの判断ミス」なのか「ブラウザ操作の再現性不足」なのかを切り分けやすくなります。

### 全体像（Parent / Child / webscraper modules）

```text
LLM Tool (pages-llm-chat)
   │
   │ Actor/RPC
   ▼
親プロセス Service（scraper instance 管理）
   │ sendQuery("WebScraper:*")
   ▼
NRWebScraperParent.sys.mts
   │  ├─ parentでしかできない処理（翻訳, ファイル読み込み, chrome側 context menu 制御）
   │  └─ それ以外は child へ転送
   ▼
NRWebScraperChild.sys.mts
   │  ├─ receiveMessage で WebScraper:* を dispatch
   │  ├─ DOMOperations (Facade)
   │  ├─ FormOperations
   │  └─ ScreenshotOperations
   ▼
webscraper/*.ts
   ├─ DOMRead / DOMWrite / DOMAction / DOMWait
   ├─ EventDispatcher / HighlightManager / TranslationHelper
   └─ constants / types / utils(Xray unwrap)
```

### 1. Parent / Child Actor の役割分担

#### `NRWebScraperChild.sys.mts`（コンテンツプロセス側の実行ハブ）

- 参照：`browser-features/modules/actors/NRWebScraperChild.sys.mts:27`
- `receiveMessage()` で `WebScraper:*` を `DOMOperations` / `FormOperations` / `ScreenshotOperations` に dispatch
- `actorCreated()` で `pagehide` を監視し、SPA 遷移時のハイライト/情報パネルを掃除
- `willDestroy()` でイベントリスナー解除 + `DOMOperations` 破棄

Actor 自体を巨大化させず、ライフサイクル管理とメッセージルーティングに責務を絞っているのが良い点です。実処理は `webscraper/` 配下へ委譲されているため、DOM 操作の改善が Actor 本体に波及しにくくなっています。

#### `NRWebScraperParent.sys.mts`（親プロセス側の特権処理）

- 参照：`browser-features/modules/actors/NRWebScraperParent.sys.mts:6`
- `WebScraper:Translate` → `handleTranslate()` で i18n provider を利用
- `WebScraper:ReadFile` → `handleReadFile()` で `IOUtils` を使ったファイル読み込み
- `WebScraper:BlockContextMenu` / `UnblockContextMenu` → chrome 側コンテキストメニュー抑止

基本は子プロセス側へのフォワーダーですが、親プロセスでしかできない処理だけを受け持つ構成になっています。`uploadFile` のようなケースでこの分離が有効に機能します。

### 2. `types.ts` / `utils.ts` の価値（XrayWrapper を前提とした設計）

- 型定義：`browser-features/modules/actors/webscraper/types.ts:12`
- unwrap helper：`browser-features/modules/actors/webscraper/utils.ts:15`

Firefox の特権コンテキストでは、ページ DOM が XrayWrapper 越しに見えるため、`instanceof` 判定・イベント生成・`File/Blob` 生成がそのままでは壊れやすいです。この実装では：

- `RawContentWindow` で content world の native constructor を型として明示
- `XrayElement` / `XrayWrapped<T>` で `wrappedJSObject` の存在を型に反映
- `unwrapWindow()` / `unwrapElement()` / `unwrapDocument()` で必要箇所だけ unwrap

という形で、Xray 関連の難しさを局所化しています。

### 3. `DOMOperations` は Facade + 依存共有の中核

- 参照：`browser-features/modules/actors/webscraper/DOMOperations.ts:24`
- 依存定義：`browser-features/modules/actors/webscraper/DOMDeps.ts:11`

`DOMOperations` は以下を 1 回生成して各サブモジュールに共有します。

- `HighlightManager`
- `EventDispatcher`
- `TranslationHelper`
- `context` / `getDocument()` / `getContentWindow()`

これにより、read/write/action/wait で「イベント dispatch の流儀」や「可視化の見た目」が揃います。DOM 操作が増えても品質を横断的に維持しやすい設計です。

### 4. 互換性の核：`EventDispatcher` + `DOMWriteOperations` + `DOMActionOperations`

#### `EventDispatcher`（イベント列と native setter の再現）

- 参照：`browser-features/modules/actors/webscraper/EventDispatcher.ts:24`
- `dispatchPointerClickSequence()` (`:34`) で `pointerdown → mousedown → ... → click`
- `getNativeValueSetter()` / `getNativeCheckedSetter()` / `getNativeSelectValueSetter()` (`:255` 以降)

`element.value = ...` だけでは React/Vue 系 UI が反応しないケースがあるため、この実装では native setter と input/change 系イベントを組み合わせて互換性を上げています。

#### `DOMWriteOperations`（入力・選択・チェック・ファイルアップロード）

- 参照：`browser-features/modules/actors/webscraper/DOMWriteOperations.ts:17`
- `inputElement()` (`:62`) は typing mode / instant mode を切替可能
- `setChecked()` (`:316`) は `checked` / `aria-checked` / `defaultChecked` を同期
- `uploadFile()` (`:399`) は親プロセス側の `WebScraper:ReadFile` と協調

`uploadFile()` は特に重要で、親プロセスで読んだバイト列を子プロセス側で `Cu.cloneInto` + content world の `Blob/File` として再構成し、`mozSetFileArray()` に渡しています。これは「ブラウザ内ネイティブ統合」の典型例です。

#### `DOMActionOperations`（click の実戦的実装）

- 参照：`browser-features/modules/actors/webscraper/DOMActionOperations.ts:16`
- `clickElement()` (`:27`) は scroll/focus/highlight 後に複数経路を試す
- `pressKey()` (`:445`), `dragAndDrop()` (`:502`) までカバー

`clickElement()` は `dispatchPointerClickSequence()`、`rawElement.click()`、synthetic `MouseEvent("click")` を段階的に試しており、ページ実装差異への耐性を上げています。

### 5. 安定性の核：`DOMWaitOperations` + `FormOperations`

#### `DOMWaitOperations`

- 参照：`browser-features/modules/actors/webscraper/DOMWaitOperations.ts:14`
- `waitForElement()` は `MutationObserver` ベース + timeout + `AbortSignal`
- `waitForReady()` は `readystatechange` / `DOMContentLoaded` を監視

単純なポーリングではなくイベントベースに寄せているため、不要な待機を減らしつつ、動的ページでの待機失敗を抑えています。`dead object` 系エラーの扱いも防御的です。

#### `FormOperations`

- 参照：`browser-features/modules/actors/webscraper/FormOperations.ts:20`
- `fillForm()` (`:45`) は複数フィールドの進捗表示・再試行・最終値再検証まで行う
- `submit()` (`:235`) は `requestSubmit()` を優先しつつ `submit()` にフォールバック

フォーム入力を 1 フィールドずつの薄いラッパーで済ませず、「複数フィールドを安全に完了させる」オーケストレーション層として設計されているのが実運用向けです。

### 6. 観測性の核：`HighlightManager` + `TranslationHelper` + `constants.ts`

#### `HighlightManager`

- 参照：`browser-features/modules/actors/webscraper/HighlightManager.ts:31`
- `applyHighlight()` (`:544`), `applyHighlightMultiple()` (`:743`)
- `showInfoPanel()` (`:253`) で操作内容/進捗を視覚化
- `showControlOverlay()` (`:939`) / `hideControlOverlay()` (`:991`) で操作中競合を抑止
- `highlightOperationId` による非同期競合対策

LLM ブラウザでは「どの要素を読んだ/触ったか」が見えないと、ユーザーにも開発者にも不安が残ります。この実装は読み取り操作も含めて可視化しており、デバッグ性は高い水準です。

#### `TranslationHelper` / `constants.ts`

- `TranslationHelper`: `browser-features/modules/actors/webscraper/TranslationHelper.ts:16`
- `constants.ts`: `browser-features/modules/actors/webscraper/constants.ts:15`

`TranslationHelper.translate()` は子プロセス側から親プロセス側へ翻訳問い合わせを行い、失敗時は `FALLBACK_TRANSLATIONS` にフォールバックします。`constants.ts` には `HIGHLIGHT_PRESETS` / `HIGHLIGHT_STYLES` もあり、ロジックと演出を分離しやすくなっています。

### 7. `webscraper` に新操作を追加する実践順序（おすすめ）

1. `webscraper/types.ts` に必要な message field を追加（必要な場合）
2. `DOMRead/Write/Action/Wait` のどこに属するか決めてメソッド実装
3. 必要なら `TranslationHelper` / `constants.ts` に文言追加
4. `DOMOperations` に facade メソッド追加
5. `NRWebScraperChild.receiveMessage()` に `WebScraper:*` 分岐追加
6. parent 専用処理が必要なら `NRWebScraperParent` に追加
7. 上位の scraper service / Actor client / LLM tool へ接続

---

# ガードレール（最小の脅威モデル）

LLM Agent ブラウザは便利な一方で、設計を誤ると事故りやすい仕組みです。実装に入る前に、最低限次の 3 つだけは意識しておきましょう。

ただし、今回はこのガードは実装しないことにします。

### 1. Prompt Injection

Web ページが「このツールを実行しろ」と誘導してくる攻撃です。

- 対策：ツール実行前のユーザー確認、ページコンテンツとユーザー指示の明確な分離、ドメインポリシーによる制限

### 2. 過剰権限

ツールが強すぎると、LLM の 1 回の判断ミスで破壊的な操作が起こり得ます。

| リスク | ツール例                                         | 推奨ポリシー         |
| ------ | ------------------------------------------------ | -------------------- |
| 低     | `list_tabs`, `read_page_content`, `read_element` | 自動実行可           |
| 中     | `create_tab`, `navigate_tab`, `click_element`    | ログ表示 + 経過観察  |
| 高     | `fill_form`, `submit_form`, `close_tab`          | **ユーザー確認必須** |

### 3. データ流出

ページ本文・履歴・フォーム内容をモデルに送ると、漏洩リスクが上がります。

- 対策：ページ内容のサニタイズ、機密情報のフィルタリング、ドメイン単位のアクセス制御

#### 最低限の禁止ルール（例）

- `about:`, `file:`, `resource:`, `chrome:`, `moz-extension:` は **読み取りも操作も禁止**
- `input[type=password]` は **読み取り対象外 / 入力は常に確認必須**

---

# LLM チャットの実装（Step by Step）

実装は **下から上に**（ブラウザ操作 → LLM 通信 → UI）の順序で進めます。

## 実装のロードマップ

```
Step 1  型定義を決める
Step 2  ツール定義を書く（Function Calling）
Step 3  Actor 通信を実装する
Step 4  エラー処理を整備する
Step 5  ツール実行ロジックを作る（executeToolCall）
Step 6  ストリーミングを実装する（SSE）+ tool_calls デルタ蓄積
Step 7  リトライロジックを入れる（指数バックオフ）
Step 8  プロバイダー設定を管理する（Prefs）
Step 9  Agentic Loop を組む
Step 10 チャット UI を作る
Step 11 メッセージ表示を磨く
```

### ファイル構成（目安）

| ファイル                                                         | 役割                                       |
| ---------------------------------------------------------------- | ------------------------------------------ |
| `browser-features/pages-llm-chat/src/types.ts`                   | 型定義（Message/ToolRun/Settings）         |
| `browser-features/pages-llm-chat/src/lib/tools.ts`               | ツール定義、Actor クライアント、ツール実行 |
| `browser-features/pages-llm-chat/src/lib/rpc.ts`                 | Preferences 用 RPC（設定）                 |
| `browser-features/pages-llm-chat/src/lib/llm.ts`                 | LLM 呼び出し、SSE、リトライ、Agentic Loop  |
| `browser-features/pages-llm-chat/src/components/Chat.tsx`        | UI 本体                                    |
| `browser-features/pages-llm-chat/src/components/ChatMessage.tsx` | Markdown + ツール表示                      |

この構成にしている理由は、責務の分離がはっきりしてデバッグしやすいからです。LLM ブラウザは「モデルの挙動」「ツール実行」「ブラウザ API」「UI 表示」のどこでも問題が起きるため、1 ファイルに寄せると原因切り分けが難しくなります。

特に `tools.ts` と `llm.ts` を分けると、前者を「ブラウザ操作ラッパー」、後者を「モデル対話制御（streaming/retry/loop）」として分業でき、失敗時の調査がしやすくなります。

---

## Step 1：型定義を決める（types.ts）

最初のステップは、プロジェクト全体で使うデータモデルの型定義です。後続のすべてのステップがこの型に依存するため、最初に固めます。

```ts
// browser-features/pages-llm-chat/src/types.ts

export type LLMProviderType =
  | "openai"
  | "anthropic"
  | "ollama"
  | "openai-compatible"
  | "anthropic-compatible";

export interface LLMProviderSettings {
  type: LLMProviderType;
  apiKey: string;
  baseUrl: string;
  defaultModel: string;
  enabled: boolean;
}

export type ToolRunStatus = "running" | "completed" | "error";

export interface ToolRun {
  id: string;
  name: string;
  args: unknown;
  status: ToolRunStatus;
  startedAt: number;
  finishedAt?: number;
  resultPreview?: string;
}

// OpenAI 互換 API の tool call（簡略化）
export type ToolCall = {
  id: string;
  type: "function";
  function: {
    name: string;
    arguments: string; // JSON 文字列（streaming では断片で届くことがある）
  };
};

export interface Message {
  id: string;
  role: "user" | "assistant" | "system" | "tool";
  content: string | null;
  timestamp: number;
  tool_calls?: ToolCall[];
  tool_call_id?: string;
  provider?: LLMProviderType;
  model?: string;
  toolRuns?: ToolRun[];
}

export interface ChatSession {
  id: string;
  messages: Message[];
  createdAt: number;
  updatedAt: number;
}

export interface AgenticState {
  isGenerating: boolean;
  isRunningTool: boolean;
  currentTool: string | null;
  iteration: number;
}

export interface LLMRequest {
  messages: Array<{
    role: "user" | "assistant" | "system" | "tool";
    content: string | null;
    tool_calls?: ToolCall[];
    tool_call_id?: string;
  }>;
  model: string;
  stream?: boolean;
  max_tokens?: number;
  temperature?: number;
}
```

**型設計のポイント**

- `tool_calls`（API 生の情報）と、UI の `ToolRun`（実行ログ）を分ける
- `assistant.content` はツール呼び出しだけのとき `null` になり得る

---

## Step 2：ツール定義を書く（BROWSER_TOOLS）

OpenAI の Function Calling 形式でツールを定義します。

```ts
// browser-features/pages-llm-chat/src/lib/tools.ts

export const BROWSER_TOOLS = [
  {
    type: "function" as const,
    function: {
      name: "list_tabs",
      description:
        "List all open browser tabs. Returns tab IDs, titles, and URLs.",
      parameters: { type: "object", properties: {}, required: [] },
    },
  },
  {
    type: "function" as const,
    function: {
      name: "create_tab",
      description:
        "Create a new browser tab and navigate to a URL. Returns a tab instance ID for further operations.",
      parameters: {
        type: "object",
        properties: {
          url: {
            type: "string",
            description: "The URL to open in the new tab",
          },
        },
        required: ["url"],
      },
    },
  },
  // ... navigate_tab, close_tab, search_web, read_page_content, click_element, fill_form, take_screenshot など
];
```

### ツール命名と実装側の対応

- **ツール名**は OpenAI 互換 API の慣習に合わせて `snake_case`（例：`create_tab`）にしています。
- **親プロセス側の RPC 関数**は実装都合で `camelCase`（例：`createTab`）になることが多いです。
- そのため `executeToolCall`（ツール実行ディスパッチ）が橋渡しします（`create_tab` → `client.createTab()`）。

## Step 3：Actor 通信を実装する（子→親）

Web ページ（子プロセス）から親プロセスのブラウザ API を叩くため、Noraneko の `NRSettingsSend/NRSettingsRegisterReceiveCallback` を transport とし、**birpc** で型安全な RPC を作ります。

```ts
// browser-features/pages-llm-chat/src/lib/tools.ts

import { createBirpc } from "birpc";

interface FloorpParentFunctions {
  listTabs: () => Promise<TabInfo[]>;
  createTab: (
    url: string,
  ) => Promise<{ success: boolean; id?: string; error?: string }>;
  // ... scraper 系もここに追加
}

class FloorpActorClient {
  private rpc: FloorpParentFunctions | null = null;
  private initPromise: Promise<void> | null = null;

  constructor() {
    this.initPromise = this.initialize();
  }

  private async initialize(): Promise<void> {
    const win = globalThis as Window & {
      NRSettingsRegisterReceiveCallback?: (cb: (data: string) => void) => void;
      NRSettingsSend?: (data: string) => void;
    };

    // Actor の準備待ち
    for (let i = 0; i < 50 && !win.NRSettingsRegisterReceiveCallback; i++) {
      await new Promise((r) => setTimeout(r, 100));
    }

    if (!win.NRSettingsRegisterReceiveCallback || !win.NRSettingsSend) {
      throw new Error("NRSettings Actor not available");
    }

    this.rpc = createBirpc<FloorpParentFunctions, Record<string, never>>(
      {},
      {
        post: (data) => win.NRSettingsSend!(data),
        on: (cb) => win.NRSettingsRegisterReceiveCallback!((data) => cb(data)),
        serialize: JSON.stringify,
        deserialize: JSON.parse,
      },
    );
  }

  private async ensureReady(): Promise<void> {
    if (this.initPromise) await this.initPromise;
    if (!this.rpc) throw new Error("Actor client not initialized");
  }

  async listTabs() {
    await this.ensureReady();
    return this.rpc!.listTabs();
  }
}
```

### 設計メモ：birpc を「一方向 RPC」として使う

birpc 自体は双方向 RPC ですが、この例では `Record<string, never>` にして **子プロセス側から関数を公開しません**（親→子の呼び出しを禁止）。

- 子プロセス：**親の API を呼ぶだけ**（最小権限）
- 親プロセス：ブラウザ操作の実体を持つ

これにより「UI 側が不用意にコールバックを受け取って状態が複雑化する」ことを避けつつ、攻撃面も狭められます。

---

## Step 4：エラー処理を整備する（formatToolError）

Actor（birpc）越しのエラーは形がバラバラです。LLM へ渡すのは最終的に文字列なので、**常に読めるエラー文字列**に正規化します。

```ts
// browser-features/pages-llm-chat/src/lib/tools.ts

function formatToolError(error: unknown, depth = 0): string {
  if (error instanceof Error) return error.message || error.name;
  if (typeof error === "string") return error.trim() || "Unknown error";
  if (error == null) return "Unknown error";
  if (typeof error !== "object") return String(error);

  if (depth > 2) return "Unknown error object";

  const value = error as Record<string, unknown>;
  const message = typeof value.message === "string" ? value.message : null;
  const code = typeof value.code === "string" ? value.code : null;
  if (message && code) return `${code}: ${message}`;
  if (message) return message;

  const errorField = value.error;
  if (typeof errorField === "string" && errorField.trim() !== "")
    return errorField;
  if (errorField !== undefined) return formatToolError(errorField, depth + 1);

  const cause = value.cause;
  if (cause !== undefined) return formatToolError(cause, depth + 1);

  try {
    const seen = new WeakSet<object>();
    const json = JSON.stringify(error, (_k, v) => {
      if (v && typeof v === "object") {
        if (seen.has(v)) return "[Circular]";
        seen.add(v);
      }
      return v;
    });
    if (!json || json === "{}" || json === "[]") return "Unknown RPC error";
    return json;
  } catch {
    return "Unknown RPC error";
  }
}

function getResultError(result: unknown): string | null {
  if (!result || typeof result !== "object") return null;
  if (!("error" in result)) return null;
  const err = (result as { error?: unknown }).error;
  if (err == null || err === "") return null;
  return formatToolError(err);
}
```

---

## Step 5：ツール実行ロジックを作る（executeToolCall）

> 方針：戻り値は **常に string**（成功・失敗を含む）。例外も文字列化して返し、Agentic Loop を落とさない。

### 代表例：副作用なし（list_tabs）

```ts
// browser-features/pages-llm-chat/src/lib/tools.ts

export async function executeToolCall(toolCall: ToolCall): Promise<string> {
  const client = getFloorpClient();
  const { name, arguments: argsStr } = toolCall.function;

  let args: Record<string, unknown>;
  try {
    args = JSON.parse(argsStr) as Record<string, unknown>;
  } catch (e) {
    return `Error executing ${name}: invalid JSON arguments (${formatToolError(e)})`;
  }

  try {
    switch (name) {
      case "list_tabs": {
        const tabs = await client.listTabs();
        return JSON.stringify(tabs, null, 2);
      }

      // 代表例：読み取り（read_page_content）
      case "read_page_content": {
        const url = typeof args.url === "string" ? args.url.trim() : "";
        if (!url)
          return "Error executing read_page_content: url must be a non-empty string";

        // 読み取りはセッション管理で包む（下の details）
        return withNavigatedScraperSession(client, url, async (id) => {
          const content = await client.scraperGetText(id);
          const err = getResultError(content);
          if (err) return `Failed to read page content: ${err}`;

          // ※ 実運用では長文をトリム/要約するとコンテキスト節約になる
          const text = (content as { text: string }).text;
          const maxChars = 20_000;
          return text.length > maxChars
            ? text.slice(0, maxChars) + "\n\n[TRUNCATED]"
            : text;
        });
      }

      // それ以外（create_tab / click_element / fill_form など）は同様に dispatch
      default:
        return `Error executing ${name}: unknown tool`;
    }
  } catch (e) {
    return `Error executing ${name}: ${formatToolError(e)}`;
  }
}
```

### 設計メモ：高リスクツールは二重ガード

ガードレール表で「高」としたツール（例：`fill_form`, `submit_form`, `close_tab`）は、**UI 側で必ず確認**を挟むのが基本です。

さらに防御を厚くするなら、実行側でも「確認されていない高リスクツールは拒否する」ようにしておくと安全です（"UI バグ" や "Prompt Injection" に対する保険）。

- UI：確認ダイアログ → OK のときだけ実行
- 実行側：拒否する場合は `tool` で `UserDenied: ...` のような結果を返す

（この記事では UI 側の確認を前提にし、実行側の二重ガードは推奨として扱います）

```ts
async function withNavigatedScraperSession(
  client: FloorpActorClient,
  url: string,
  run: (scraperId: string) => Promise<string>,
): Promise<string> {
  if (typeof url !== "string" || url.trim() === "") {
    return "Failed to navigate: url must be a non-empty string";
  }

  const createResult = await client.createScraperInstance();
  if (!createResult.success || !createResult.id) {
    return `Failed to create scraper instance: ${formatToolError(createResult.error)}`;
  }

  try {
    const navResult = await client.scraperNavigate(createResult.id, url);
    if (!navResult.success) {
      return `Failed to navigate to ${url}: ${formatToolError(navResult.error)}`;
    }

    await waitForScraperActorReady(client, createResult.id);
    return await run(createResult.id);
  } finally {
    try {
      await client.destroyScraperInstance(createResult.id);
    } catch (e) {
      console.debug?.(
        "[pages-llm-chat] failed to destroy scraper instance",
        formatToolError(e),
      );
    }
  }
}

async function waitForScraperActorReady(
  client: FloorpActorClient,
  scraperId: string,
): Promise<void> {
  for (let attempt = 0; attempt < 12; attempt++) {
    try {
      const probe = await client.scraperGetHtml(scraperId);
      const probeError = getResultError(probe);
      if (!probeError) return;
      if (!isLikelyTransientScraperError(probeError)) return;
    } catch (e) {
      if (!isLikelyTransientScraperError(e)) return;
    }
    await sleep(250);
  }
}

async function waitForSelectorBeforeOperation(
  client: FloorpActorClient,
  scraperId: string,
  selector: string,
  timeout = 8000,
): Promise<string | null> {
  if (typeof selector !== "string" || selector.trim() === "") {
    return "selector must be a non-empty string";
  }

  for (let attempt = 0; attempt < 4; attempt++) {
    try {
      const result = await client.scraperWaitForElement(
        scraperId,
        selector,
        timeout,
      );
      if (result.success) return null;
      if (!isLikelyTransientScraperError(result.error)) {
        return formatToolError(result.error);
      }
    } catch (e) {
      if (!isLikelyTransientScraperError(e)) return formatToolError(e);
    }
    await sleep(300);
  }

  return `Element "${selector}" did not appear within ${timeout}ms`;
}
```

---

## Step 6：ストリーミングを実装する（Server-Sent Events）

LLM の応答を「全部生成されてから表示」するのではなく、「生成されたトークンから順次表示」します。

OpenAI 互換 API のストリーミングは **Server-Sent Events (SSE)** が一般的です。

- 1 イベントは **空行（`\n\n` または `\r\n\r\n`）** で区切られる
- 本文は `data:` 行に載る（複数行の `data:` があり得る）
- 終端は `data: [DONE]`

> ここを `\n` で単純 split すると、**チャンク境界**や**複数 data 行**で壊れやすいので、イベント境界で処理します。

### SSE のイベント境界でパースする

```ts
// browser-features/pages-llm-chat/src/lib/llm.ts

function splitSseEvents(buffer: string) {
  // CRLF を LF に正規化
  const normalized = buffer.replaceAll("\r\n", "\n");
  // SSE は空行でイベント境界
  const parts = normalized.split("\n\n");
  return { complete: parts.slice(0, -1), rest: parts.at(-1) ?? "" };
}

function sseDataLines(eventBlock: string) {
  return eventBlock
    .split("\n")
    .filter((l) => l.startsWith("data:"))
    .map((l) => l.slice(5).trimStart());
}
```

### tool_calls（デルタ）の安全な蓄積

`tool_calls.function.arguments` は断片で届くため、`index` をキーに **Map** で蓄積してから確定させます。

```ts
type ToolCallAcc = {
  id?: string;
  type?: "function";
  function: { name?: string; arguments: string };
};

function accumulateToolCalls(
  acc: Map<number, ToolCallAcc>,
  deltas: Array<{
    index?: number;
    id?: string;
    type?: "function";
    function?: { name?: string; arguments?: string };
  }>,
) {
  for (const d of deltas) {
    const idx = d.index ?? 0;
    const cur = acc.get(idx) ?? { function: { arguments: "" } };

    if (d.id) cur.id = d.id;
    if (d.type) cur.type = d.type;

    // name は通常 1 回で十分（+= しない）
    if (d.function?.name) cur.function.name = d.function.name;

    // arguments は断片で届くので連結
    if (typeof d.function?.arguments === "string") {
      cur.function.arguments += d.function.arguments;
    }

    acc.set(idx, cur);
  }
}

function finalizeToolCalls(acc: Map<number, ToolCallAcc>): ToolCall[] {
  return [...acc.entries()]
    .sort((a, b) => a[0] - b[0])
    .map(([, v]) => ({
      id: v.id ?? crypto.randomUUID(),
      type: "function",
      function: {
        name: v.function.name ?? "unknown_tool",
        arguments: v.function.arguments,
      },
    }));
}
```

### streamMessagesWithTools（抜粋）

```ts
async function streamMessagesWithTools(
  settings: LLMProviderSettings,
  messages: Message[],
  callbacks: StreamMessagesWithToolsCallbacks = {},
): Promise<{ content: string; toolCalls: ToolCall[] }> {
  const response = await fetch(/* ... */);
  const reader = response.body?.getReader();
  if (!reader) throw new Error("No response body");

  const decoder = new TextDecoder();
  let buffer = "";
  let accumulatedContent = "";
  const toolAcc = new Map<number, ToolCallAcc>();

  while (true) {
    const { done, value } = await reader.read();
    if (done) break;

    buffer += decoder.decode(value, { stream: true });
    const { complete, rest } = splitSseEvents(buffer);
    buffer = rest;

    for (const ev of complete) {
      const data = sseDataLines(ev).join("\n");
      if (!data || data === "[DONE]") continue;

      let json: any;
      try {
        json = JSON.parse(data);
      } catch {
        continue;
      }

      const delta = json.choices?.[0]?.delta;
      if (!delta) continue;

      if (typeof delta.content === "string" && delta.content) {
        accumulatedContent += delta.content;
        callbacks.onChunk?.(delta.content);
      }

      if (delta.tool_calls) {
        accumulateToolCalls(toolAcc, delta.tool_calls);
      }
    }
  }

  // 末尾の rest がイベント境界を含まず残る場合がある
  // 必要ならここで flush する（例：splitSseEvents(buffer + "\n\n")）

  return { content: accumulatedContent, toolCalls: finalizeToolCalls(toolAcc) };
}
```

---

## Step 7：リトライロジックを入れる（Exponential Backoff）

API リクエストはネットワークエラーやレート制限（429）で失敗することがあります。一時的なエラーを自動リトライする仕組みを実装します。

（この節は現状の実装で OK。可能なら `Retry-After` を読めるとさらに良いです）

---

## Step 8：プロバイダー設定を管理する（Preferences）

- 設定画面で入力した provider 情報を Prefs に JSON で保存
- チャット側はそれを読み出して正規化
- Ollama の baseUrl は `/v1` を自動補正

> 注：Prefs 変更を UI 側でリアルタイム反映したい場合、`StorageEvent`（localStorage）ではなく、Prefs 側の変更通知を Actor 経由で届ける仕組み（イベント or polling）が必要です。

---

## Step 9：Agentic Loop（Agent 実行ループ）

LLM がツールを使いながらタスクを完遂するまでループします。

- `maxIterations` を必ず設ける
- ツール結果が長すぎる場合はトリム/要約する（コンテキスト節約）

### Confirm を入れる具体的な流れ（推奨）

高リスクツール（`fill_form`, `submit_form`, `close_tab` など）は **UI で確認 → 結果を tool メッセージで返す**のが扱いやすいです。

- ユーザーが **OK** → ツールを実行して結果を `tool` で返す
- ユーザーが **拒否** → 実行せず `tool` で `UserDenied: ...` を返す

これにより、LLM は「拒否された」という事実を会話履歴として受け取り、次の手（代替案の提示など）に進めます。

```ts
// 擬似コード（UI 側）
if (isHighRisk(toolCall) && !(await confirmWithUser(toolCall))) {
  pushToolResult({
    tool_call_id: toolCall.id,
    content: `UserDenied: ${toolCall.function.name}`,
  });
  continue;
}

const result = await executeToolCall(toolCall);
pushToolResult({ tool_call_id: toolCall.id, content: result });
```

---

## Step 10：チャット UI（Chat.tsx）

- `isGenerating`（生成中）と `isRunningTool`（ツール実行中）を分けて UX を改善
- tool 実行ログ（ToolRun）をメッセージに紐づけて表示
- localStorage が死ぬ（プライベート等）ケースは in-memory にフォールバック

---

## Step 11：メッセージ表示（ChatMessage.tsx）

- Markdown（コードブロック/テーブル）をリッチに表示
- ツール実行状態（running/completed/error）を視覚化
- 実行時間と結果プレビューを見せる

---

# 具体例：「東京の天気を調べて」

1. LLM が `search_web("東京 天気")` を提案 → 実行
2. 必要なら `read_page_content(...)` でページ本文を読んで要約
3. ツール呼び出しが無くなったら終了

---

# まとめ

拡張機能や Selenium に頼らない LLM ネイティブなブラウザ開発について、Floorp の実装例をもとに解説しました。

## 参考リンク

- Floorp: [https://github.com/Floorp-Projects/Floorp](https://github.com/Floorp-Projects/Floorp)
- Noraneko: [https://github.com/f3liz-dev/noraneko](https://github.com/f3liz-dev/noraneko)
- ビルド手順: [https://docs.floorp.app/docs/building/](https://docs.floorp.app/docs/building/)
