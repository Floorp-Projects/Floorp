# Floorp QA Harness — 検証ティアと操作リファレンス

このドキュメントは Floorp QA skills すべてが共有する自動化基盤の知識です。
QA 項目を実行する前に必ず読み、項目ごとに適切なティアを選んでください。

## 検証ティア

| ティア | 手段 | 強み | 対象ビルド |
|---|---|---|---|
| L0 単体/静的 | `deno task test` (colocated), `--list` | 決定的・CI 済み | dev ソース |
| L1 状態検証 | Marionette chrome eval、prefs.js/プロファイルファイル直接検査 | 高速・内部状態に到達 | dev/stage (Marionette)、全ビルド (file) |
| L2 機能操作 | os-server HTTP API でタブ/UI を実操作 | 本番バイナリを駆動可能 | 全ビルド |
| L3 実機統合 | Computer Use (macOS AX) でネイティブ UI 操作 | OS 統合・ジェスチャ・署名系 | パッケージ版 |
| L4 非機能 | 性能計測・a11y スキャン・i18n キースキャン | 大規模/網羅 | 全ビルド |

**原則: 項目は最も低い(確実な)ティアで検証する。** L1 で内部状態を直接確認できるものを
L2 の UI 操作でぼんやり確認しない。逆に UI 統合が本質の項目は L2/L3 で検証する。

---

## 1. qa-drive — ハーネス CLI

場所: `.devin/qa/tools/qa-drive.ts`。すべての出力は stdout の JSON。エラーは stderr + 非ゼロ終了。

```bash
QA="deno run -A .devin/qa/tools/qa-drive.ts"   # 以下の例はこのエイリアス前提
```

### launch — 検証用インスタンスの起動

```bash
$QA launch --profile /tmp/floorp-qa-<run-id>/profile \
           --binary /Applications/Floorp.app/Contents/MacOS/floorp \
           --os-port 58263 --marionette-port 28293 \
           --pref floorp.workspaces.enabled=true \
           --pref some.int.pref=42 --pref some.bool.pref=true
```

- 使い捨てプロファイルに `user.js` を書き、detached で起動する。
- `floorp.os.enabled=true`・固定 bearer token・marionette を自動設定。
  **token は戻り値 JSON と `<profile>/prefs.js` の両方から取得できる。**
- `--pref name=value` は繰り返し可。`true/false`/整数は型付き、他は文字列になる。
- `osServerReady:true` なら os-server が実際の HTTP 応答を返した確認済み。
- macOS で `/tmp` は `/private/tmp` に正規化される。stop/status には launch が返した
  profile パスをそのまま使うこと。

### os — os-server API 呼び出し

```bash
$QA os GET  /tabs/list --profile <profile> --os-port 58263
$QA os POST /tabs/instances --profile <profile> --os-port 58263 \
     --data '{"url":"https://example.com","waitForLoad":true}'
$QA os POST /tabs/instances/<iid>/click --data '{"selector":"button.save"}' ...
```

- `--token` 未指定なら `--profile` の prefs.js から自動読み出し。
- リクエストには 30 秒タイムアウトがある。**タイムアウトした場合はサーバーが
  wedge している可能性があるため、インスタンス再起動を検討すること**(後述)。

### eval / shot — Marionette (dev/stage ビルドのみ)

```bash
$QA eval 'Services.prefs.getStringPref("floorp.workspaces.v4.store","")' \
     --marionette-port 28293 --context chrome
$QA eval 'document.title' --context content
$QA shot --out evidence/shot-01.png --marionette-port 28293
```

- chrome context は `Services`/`gBrowser`/XUL DOM に完全アクセス。L1 検証の主力。
- **本番パッケージ 12.21.2 ではポートは開くがハンドシェイクが完了しない
  (2026-11-28 検証済み)。production では Marionette に頼らず、
  os-server + ファイルシステム検査 + Computer Use で検証する。**
  dev/stage ビルドではチームの開発フロー通り利用可能。
  必要なら `--remote-allow-system-access` + `MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1` が
  Firefox 159+ で必須(qa-drive launch は `--system-access` で付与)。

### stop / status / wait

```bash
$QA stop --profile <profile>     # プロファイルパスで QA インスタンスのみ kill
$QA status --profile <profile> --os-port 58263
```

### 重要な運用注意

- **os-server に裸の TCP connect/close プローブを打たない**(nc -z 等)。
  不完全な HTTP リクエストとしてサーバー側に socket エラーと
  滞留ハンドラを発生させる。readiness 確認は必ず実 HTTP GET で行う
  (qa-drive は内部でそうしている)。
- **os-server が wedge した兆候**: それまで応答していた全エンドポイントが
  タイムアウトする。12.21.2 実測では `/evaluate` 実行後に wedge する事象を確認。
  対処: ブラウザを `stop` → 新プロファイルまたは同プロファイルで `launch` し直し。
  wedge 自体は QA の**発見**として記録すること(再現手順・直前の API 列)。
- 1 回の QA セッションではテスト項目ごとに使い捨てインスタンス(tab instance)を
  作り、終わったら `POST /tabs/instances/<iid>/close` で閉じる。
  ブラウザ自体の再起動が前提条件の項目は項目ごとに launch/stop を行う。

---

## 2. os-server API サマリ

ベース: `http://127.0.0.1:<os-port>`、認証 `Authorization: Bearer <token>`。
詳細なリクエスト形状の正典はソース:
`browser-features/modules/modules/os-server/{tabs,browser,workspaces,scraper,shared}/routes.sys.mts`
(対象ビルドのブランチで読むこと。API は進化している)。

主要エンドポイント:

```
GET  /tabs/list                              全タブ (browserId, windowId, title, url, selected, pinned)
POST /tabs/instances                         新規タブ作成 {url, inBackground?, waitForLoad?} → {instanceId}
POST /tabs/attach                            既存タブに接続 {browserId} → {instanceId}
GET  /tabs/instances/:id                     インスタンス情報
POST /tabs/instances/:id/close               タブを閉じる
GET  /tabs/instances/:id/exists              軽量生存確認

POST /tabs/instances/:id/navigate            {url}
POST /tabs/instances/:id/click               {selector または fingerprint, button?, clickCount?, force?}
POST /tabs/instances/:id/doubleClick|rightClick|hover
POST /tabs/instances/:id/input               {selector, text|value, typingMode?, typingDelayMs?}
POST /tabs/instances/:id/clearInput          {selector}
POST /tabs/instances/:id/fillForm            {formData: {...}}
POST /tabs/instances/:id/pressKey            {key}
POST /tabs/instances/:id/selectOption        {selector, value}
POST /tabs/instances/:id/setChecked          {selector, checked}
POST /tabs/instances/:id/evaluate            {script}  ※パラメータ名は script (expression ではない)
POST /tabs/instances/:id/waitForElement      {selector|fingerprint, timeout?, state?}
POST /tabs/instances/:id/waitForNetworkIdle  {idleDuration?, timeout?, maxInflight?}
POST /tabs/instances/:id/waitForReady
GET  /tabs/instances/:id/screenshot          → {image: data:image/png;base64,...}
GET  /tabs/instances/:id/fullPageScreenshot
POST /tabs/instances/:id/regionScreenshot    {rect}
GET  /tabs/instances/:id/elementScreenshot
GET  /tabs/instances/:id/ax-tree             アクセシビリティツリー (UI 検証に強力)
GET  /tabs/instances/:id/html|text|title|uri
POST /tabs/instances/:id/elements            {selector} → 要素列
POST /tabs/instances/:id/elementByText
GET  /tabs/instances/:id/cookies             cookie 検証
POST /tabs/instances/:id/scrollTo            {selector または座標}
POST /tabs/instances/:id/uploadFile          {selector, filePath}

GET  /workspaces                             ワークスペース一覧 (API 経由の workspace 検証)
GET  /browser                                ブラウザ情報
GET  /downloads /history                     ダウンロード/履歴
GET  /events                                 イベントストリーム
```

`fingerprint` は要素の安定識別子。`elements` や `ax-tree` で取得し、
`resolveFingerprint` で解決できる。動的 UI では selector より fingerprint が堅い。

### evaluate と CSP(重要)

`/evaluate` はページ内で Function/eval を使うため、**CSP でブロックされるページでは
使えない**(about:hub 等の特権ページでは `EvalError: call to Function() blocked by CSP`
になる。12.21.2 実測)。したがって特権ページの操作は:

- **クリック/入力は `click`/`input`/`pressKey`** — 合成 trusted event なので CSP と無関係に動く
- **読み取りは `ax-tree`/`html`/`text`/`title`/`uri`/screenshot** — eval 不要で全文取れる
- `evaluate` は通常の web コンテンツでのみ使う

backup bridge(NRWorkspacesBackupNow 等)を production で直接呼ぶことはできないが、
設定 UI を click 操作すれば同等の操作はすべて実行でき、検証はファイルシステムで行える。

---

## 3. ファイルシステム検査 (全ビルドで使える最強の L1)

プロファイルディレクトリは完全に読める。UI 操作不要で多くの項目を検証できる:

```
<profile>/prefs.js                          user_pref 全量 (grep で検証)
<profile>/workspaces/backup/backup-*.jsonlz4   backup 本体 (mozlz4 圧縮)
<profile>/workspaces/backup/*.tmp              orphan tmp はクリーンアップ対象
<profile>/sessionstore.jsonlz4               セッション
<profile>/containers.json                    コンテナ定義
```

### mozlz4 展開

```bash
python3 .devin/qa/tools/mozlz4.py <backup.jsonlz4> --keys    # 構造サマリ
python3 .devin/qa/tools/mozlz4.py <backup.jsonlz4>           # JSON 全文
```

backup JSON の検証観点例: プライベートタブ混入(A-20〜22)、formdata/session storage/
postdata 非含有(A-24)、URL 平文リスク(A-25)、icon は inline data image のみ(A-26)、
復元不可 URL scheme の除外(A-28/29)。

### 秘密情報の平文漏れ検査 (C-10 型)

```bash
grep -c 'private-container-url-pATTERN' <profile>/prefs.js   # 0 であること
```

**実プロファイルの秘密がレポートに混入しないよう、QA は必ず使い捨てプロファイルで行い、
fixture にはダミー値のみ使う。**

---

## 4. dev/stage ビルドでの検証

```bash
cd <Floorp repo>
deno task dev-tool start          # dev サーバー + ブラウザ (Marionette 有効)
deno task dev-tool eval '...'     # chrome context
deno task dev-tool console --level error
deno task test                    # colocated テスト (L0)
deno task test -- --near browser-features/chrome/common/workspaces/
```

- dev-tool の Marionette ポートは `<repo>/_dist/marionette-port.txt`。
  qa-drive eval にそのポートを渡せば qa-drive からも chrome eval できる。
- release 候補の QA は対象ブランチ (例: `devin/release-12.21.3`) の worktree で行う。
- colocated テストで既にカバーされている項目 (例: workspaces backup の G1〜G8 テスト) は
  L0 済みとして記録し、QA skill は統合/実機観点に集中する。

### FDH / EvidenceManifest との関係

チームには FDH (`tools/fdh.ts`) と EvidenceManifest v0
(`tools/qa3/evidence/evidence-manifest.schema.json`) という変更検証の証跡系がある。
役割分担:

- **FDH/qa3**: 特定の変更・タスクの検証 (build → dev up → 操作 → schema-valid な evidence)
- **本 QA 系**: 機能カタログ (matrix) に対する包括的 QA。verdicts.jsonl/report.md を产出

QA 実施中に dev worktree を扱う場合、fdh の dev-loop
(`.agents/skills/fdh-dev-loop` が worktree にあれば参照) を使ってもよい。
ただし QA verdict の記録は本 harness の verdicts.jsonl 形式で行う。
両者は矛盾しない: verdicts.jsonl はカバレッジ追跡用、EvidenceManifest は変更検証の正式証跡用。

---

## 5. Computer Use (L3・macOS パッケージ版)

Floorp は macOS Accessibility 経由で操作可能 (`app.floorp.Floorp`)。
AGENTS.md の実績メモを参照:

- AX ツリーは巨大でライブ更新中は STALE_SNAPSHOT → 毎回 getAXState を再読込
- バックグラウンド時は WINDOW_CHANGED → activate()
- AX インデックスは毎回変わる。verified:false 前提で AX 再読込して確認
- 新規タブ: タブバーの AXButton "新しいタブ"。URL: ⌘L → 入力 → Return

用途: PWA インストールの Finder 統合、updater、macOS ジェスチャ、
ネイティブメニュー、タスクバー/Dock identity、印刷/保存シート。

---

## 6. 時間圧縮と前提条件の作り方

scheduler/retention 系は実時間を待たず pref で圧縮する:

- backup 間隔 15 分 → 設定 UI/pref で最小値に変更し capture を観測
- retention 期限切れ → backup ファイル名のタイムスタンプを過去に改変して再現
  (`backup-YYYYMMDD-HHMMSS-<id>.jsonlz4` は UTC 名)
- system sleep → `wake_notification` は直接オブザーバ通知を chrome eval で発火可能
  (dev/stage のみ)
- 時計操作は macOS 全体に影響するため**禁止**。代替としてファイル名/pref で再現する

---

## 7. verdict と証跡の記録

結果は `_dist/qa-results/<YYYYMMDD-HHMMSS>-<area>/` に保存する (`_dist` は gitignore 済み):

```
verdicts.jsonl        1行1項目 (下記スキーマ)
evidence/             スクリーンショット・ダンプ・レスポンス JSON
report.md             人間向け日本語レポート
summary.json          集計 (pass/fail/blocked/manual 数、対象ビルド、実施時刻)
```

verdicts.jsonl の1行:

```json
{"id":"A-36","verdict":"pass","tier":"L2","evidence":["evidence/a36-backup-list.json"],
 "notes":"pre-restore backup が生成され list で確認","durationSec":42,"ts":"2026-11-28T21:30:00+09:00"}
```

- verdict: `pass` / `fail` / `blocked` / `manual-required` / `skip`
- `fail` には必ず再現手順と期待/実際を notes に入れる
- `blocked` は環境理由 (例: production で Marionette 不可) と対象外を区別する
- 推測で pass にしない。検証不可能なら manual-required

### Discord レポート

```bash
python3 .devin/qa/tools/discord-post.py \
  --message "<要約 markdown>" \
  --file _dist/qa-results/<run>/report.md \
  --file _dist/qa-results/<run>/verdicts.jsonl \
  --thread-name "QA <area> <date>"
```

- 認証情報は環境変数 `DISCORD_BOT_TOKEN`/`DISCORD_CHANNEL_ID` または
  `.devin/qa/local.env` (gitignore・未コミット) に置く。**レポジトリに secret を書かない**
- 未設定時は終了コード 3。QA 結果はローカルに残るので後から投稿できる
- 要約は fail/blocked を先頭に、pass 数は集計のみ。問題点がすぐ分かる形にする

---

## 8. QA 実施の標準フロー

1. matrix YAML を読み、対象項目と tier を確認
2. 対象ビルドを決定 (dev/stage/packaged)。backup など 12.21.3 機能は
   production 12.21.2 に存在しない — 必ず対象ブランチのビルドを使う
3. 使い捨てプロファイルで launch。前提条件 (workspaces 有効等) を pref/操作で構築
4. 項目を実行し都度 verdict を記録。証跡は evidence/ に保存
5. 全項目後に report.md を生成し、orchestrator に報告 (または Discord 投稿)
6. インスタンスを stop し、プロファイルを残すか削除するか判断
   (fail 再現用に残すのがデフォルト)
