# Experiments Client — 使い方と詳細

このドキュメントは `browser-features/modules/modules/experiments/Experiments.sys.mts`（`ExperimentsClient` クラス）を使う開発者向けのリファレンスです。

## 概要

- クラス名: `ExperimentsClient`
- デフォルトのシングルトン: `Experiments`（ファイル末尾で `new ExperimentsClient()` が export されています）
- 目的: installId に基づく確定的割当、variant の `configUrl` フェッチと prefs キャッシュ、実験の有効期間管理。

## 主要な pref キー

- `floorp.experiments.installId` — 自動生成される installId（profile 単位）
- `floorp.experiments.assignments.v1` — 割当情報を JSON 文字列で保存（{ [experimentId]: { installId, variantId, assignedAt } }）
- `floorp.experiments.config.<experimentId>:<variantId>` — variant の JSON キャッシュ（{ fetchedAt, config }）

## API（使用例）

- 初期化

```ts
// singleton を使う場合
await Experiments.init();

// For local testing or when you want to point to a different manifest,
// set the manifest URL on the profile prefs and then call init().
// Example (in a privileged chrome/test context):

// Services.prefs.setStringPref("floorp.experiments.manifestUrl", "http://localhost:8000/experiments.sample.json");
// Experiments.init({ installId: "install-12345" }).then((instance) => { /* instance is the ExperimentsClient */ });

// Note: init() returns the initialized instance (Promise<ExperimentsClient>),
// so you can await it and use the returned object for testing.
```

- 割当・設定の取得

```ts
// 割当された variant id を得る
const variantId = Experiments.getVariant("example_new_ui");

// variant に紐づく config を取得（キャッシュ/非同期）
const cfg = await Experiments.getConfig("example_new_ui");

// すべての割当を確認
const all = Experiments.getAllAssignments();
```

- テスト用ヘルパー（追加された getter）

```ts
// 実験の定義を取得
const exp = Experiments.getExperimentById("example_new_ui");

// variant オブジェクト取得（variantId を明示しても良い）
const variant = Experiments.getVariantById("example_new_ui");

// 保存済みの割当を取得
const assignment = Experiments.getAssignment("example_new_ui");

// prefs にキャッシュされている config を直接取得（ネットワークは行わない）
const cached = Experiments.getCachedConfig("example_new_ui");
```

## 実験マニフェスト（manifest）フォーマット

トップレベルは `{ "experiments": [ ... ] }`。
各 experiment の例:

```json
{
  "id": "example_new_ui",
  "salt": "example_new_ui_salt_v1",
  "rollout": 50,
  "start": "2025-10-01",
  "end": "2025-11-01",
  "variants": [
    { "id": "control", "weight": 50 },
    {
      "id": "variantA",
      "weight": 50,
      "configUrl": "configs/new_ui_variantA.json"
    }
  ]
}
```

- `start` / `end` は `YYYY-MM-DD`（UTC 日付のみ）または ISO タイムスタンプを許容。
- `rollout` (0-100) で母集団比を制御。
- `variants[].weight` が相対的な配分を決定。省略時は均等扱い。
- `configUrl` は絶対/相対どちらもOK（相対時は manifest の URL を基準に解決）。

## 振る舞いのポイント

- 割当は `installId` と `salt` を結合してハッシュし、百分率で評価します（FNV-1a 32ビット）。
- `rollout` により割当対象の上限を決めます。対象外のユーザーは `control` バリアントに割り当てられるか、`control` がなければ実験に参加しません。
- `getConfig` はまず prefs のキャッシュを確認し、なければ `configUrl` をフェッチして prefs に保存します。
- モジュール内の `DEFAULT_EXPERIMENTS_URL` を変更することでデプロイ先 manifest を指定します（ビルド時に置換する運用を推奨）。

## 統合手順（簡易）

1. `Experiments.init()` をプロファイル起動時（BrowserGlue の適切なフック）で呼ぶ。
2. UI では `getVariant` を同期的に参照して振る舞いを切り替える。追加設定が必要な場合は `getConfig` を await して適用。
3. テストでは `new ExperimentsClient(url)` してローカル manifest を指せるようにし、`init()` → `getVariant` で分布を確認する。

## ローカル検証手順

1. `experiments.sample.json` と `configs/` をホストする簡易サーバを起動（プロジェクトルートで）:

```pwsh
# PowerShell 例
python -m http.server 8000
```

2. `ExperimentsClient` をローカル URL でインスタンス化して `init()` を実行（またはファイル内の `DEFAULT_EXPERIMENTS_URL` を一時変更してテスト）。
3. `getAllAssignments()` や `getCachedConfig()` を使い、期待どおりに割当が保存されるか確認。

## 運用上の注意

- 本番では manifest と config を HTTPS で配信してください。
- 重要な設定は manifest 側で速やかに削除または `end` 日付を過去にすることでロールバックできます。
- config に秘密を含めないでください。フェッチ失敗や不正 JSON に備えた堅牢な消費側チェックを行ってください。

---

最終更新: 2025/11
