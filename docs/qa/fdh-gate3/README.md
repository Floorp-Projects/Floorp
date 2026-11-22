# FDH Gate 3 exit evidence (v0.1 前半)

**状態:** Gate 3 出口通過（2026-11-08） ／ Base: `4bbfe29fb0b8e48d52fcae5a45aa8268d73c1741`
（PR #3070 fdh CLI/Skill + PR #3071 Gate 3 fixes マージ後の main）
**Jira:** [KAN-113](https://floorp-projects.atlassian.net/browse/KAN-113)、[KAN-124](https://floorp-projects.atlassian.net/browse/KAN-124)、[KAN-104](https://floorp-projects.atlassian.net/browse/KAN-104)
**計画:** [FDH Gate 2/3/4 試験計画](https://floorp-projects.atlassian.net/wiki/spaces/FI/pages/8061156)（Confluence 8061156）

Gate 3 出口条件「T-01・T-02・T-02bが通り、証拠が残る」を満たした。証拠は本 PR 内に置く
（計画 §4「証拠置き場: 当面PR内」）。本 README は Gate 3 の出口記録であり、各 report.md と
2-D EvidenceManifest が一次証拠。

## 対象と結果

| 項目 | 結果 | 証拠 |
|---|---|---|
| T-01（AC-01） | **PASS** — 実コード変更→build→起動→ブラウザー確認、意図的エラーをログから発見して修正、未commit差分保全 | [`t01/report.md`](t01/report.md)、`t01/evidence-gate3-t01-final.json`、`t01/state-gate3-t01-final.json`、`t01/logs/` |
| T-02（AC-02、D-06c） GLM 5.3 Flash | **PASS** — offset 0 / settings 5183、`QA3-T02-MARKER`、manifest state `succeeded`、assertions 3、validation `[]` | [`t02/glm/report.md`](t02/glm/report.md)、`t02/glm/evidence-gate3-t02-postdown.json`、`t02/glm/logs/` |
| T-02（AC-02、D-06c） MiMo 2.6 Flash | **PASS** — offset 100 / settings 5283、同一 `QA3-T02-MARKER`、manifest state `succeeded`、assertions 3、validation `[]` | [`t02/mimo/report.md`](t02/mimo/report.md)、`t02/mimo/evidence-gate3-t02-postdown.json`、`t02/mimo/logs/` |
| T-02b（共有サービス停止） | **PASS（限定）** — GitHub 到達不能（`gh` exit 1、`curl` exit 7）のまま T-01 相当ループ完走、manifest state `succeeded`、assertions 3、validation `[]` | [`t02b/report.md`](t02b/report.md)、`t02b/evidence-gate3-t02b.json`、`t02b/evidence-gate3-t02b-postdown.json`、`t02b/logs/` |

## T-01 の要点

- Task `gate3-t01-final`、worktree `/tmp/fdh-gate2/pr2b2`、offset 0、base `4bbfe29f…`。
- `app-sidebar.tsx:133` に `+ " QA3-T01-MARKER"` を未commitで追加。build 後、Fission content tab
  の実 document を Marionette actor proxy 経由で評価し `QA3-T01-MARKER @ http://localhost:5183/#/overview/home`。
- `static-panels.ts` に `const qa3Broken = ;` を意図的に挿入 → build exit 1（`[swc] Expression
  expected`）→ 除去 → build exit 0。`fdh verify build` は PR #3071 修正後の merged code で exit 0
  （`[injector] SUCCESS`）。修正前は `isCI: true` 固定で local `BIN_DIR` を見ず ENOENT だった。
- 未commit差分（marker + ローカル patcher guard）は実行前後で保全。manifest `state: succeeded`、
  7 assertions、11 limitations、`validateManifest(role:"runner")` `[]`。7 assertions には
  browser re-verify の一時的 `NewSession timed out`（browser-connected false）を含み、dev 再起動後の
  最終 pass で succeeded になっている（report §2/§6）。
- committed task-state JSON は `status: "running"` のまま（fdh CLI は final status を state ファイルへ
  遷移させない）。PASS の「succeeded」は **evidence manifest の `envelope.taskStatus.state`** を指す。

## T-02 の要点（DSH 同一ハーネス2モデル）

- 同一 Skill（`.agents/skills/fdh-dev-loop/SKILL.md` を両モデルが読了）と同一 `fdh` CLI 契約、
  同一依頼・同一マーカー `QA3-T02-MARKER`、同一コマンド順。
- GLM 5.3 Flash = offset 0（Marionette 2828 / settings 5183）、MiMo 2.6 Flash = offset 100
  （Marionette 5280 / settings 5283）。両方 `state: succeeded`、assertions 3（browser-connected /
  browser-eval-contains / build-exit-code）、limitations 11（post-down）、validation `[]`。
- 両 report のコマンド列・exit code・マーカー出力・evidence を相互比較可能な形で保存。

## T-02b の要点と発見

- GitHub 停止 proof: 全外部HTTP dead proxy で `gh auth status` exit 1、`curl https://github.com/`
  exit 7。baseline は `gh auth status` exit 0。
- **発見（非blocking、限定付き）:** 全外部HTTP停止では `fdh dev up` が
  `dev-assets.floorp.app` の Runtime index fetch で exit 1（`Initializer.run({distribution:"debug"})`
  がインストール済みでも index を取得）。T-01相当ループは GitHub 非依存だが、runtime-asset host
  には依存する。
- 完走は `NO_PROXY="dev-assets.floorp.app"` で runtime-asset host のみ免除し、GitHub は proxy 経由で
  到達不能のまま実施。`QA3-T02B-MARKER @ http://localhost:5183/#/overview/home`、build assert exit 0、
  manifest state `succeeded`、assertions 3、limitations 11、validation `[]`。failed dev up も operations に記録。
- 注意: 2-D manifest の boilerplate limitation「No GitHub or other shared-service behavior is
  exercised; the local loop does not need shared services」は `fdh evidence` が全タスクに付ける定型文で、
  T-02b の runtime-asset host 依存には正確ではない。一次記録は report §1/§6 と本 README の上の発見。
- 全外部HTTP停止時の `dev-up` 失敗の WARN 行は、失敗時の `_dist/dev-server.log` が retry で上書きされた
  ため、`t02b/logs/dev-broad-outage-repro.log`（同条件の再現、exit 1）に保存している。

## 環境注記（全項目共通）

- ローカル Debug Runtime は `moz-src` を含まない packaged `.app`。`Patcher.run` abort のため
  `FLOORP_SKIP_PATCHER=1` で skip（PR外ローカル guard）。`omni.ja`、dev CSP、`noraneko-devdir`
  mounts は手作業で用意。macOS local のみ、`matchesTarget: false`、host test なし、visual judge なし、
  DSH pin UNVERIFIED、未commit差分は manifest で hash されない。
- T-02/T-02b は DSH 限定 spike（Confluence 8028321）を入口証拠とし、D-03 のフェーズ5採用とは別。
- D-13 により 2-C は shadow 継続（#2917 を main 上で作り直す）。Gate 3 は 2-C を待たない。

## 次の作業

Gate 4（4-A 並列Agent / 4-B 回帰テスト集 / 4-D 使用量上限、4-C 最低限resume）へ。
