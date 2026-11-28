---
name: floorp-qa
description: Floorp 機能 QA の統括 — 領域別 subagent QA をオーケストレーションし Discord にレポートする
argument-hint: "<area|all> [scope] 例: /floorp-qa workspaces-backup P0"
allowed-tools:
  - read
  - exec
  - grep
  - glob
triggers:
  - user
---

# Floorp QA オーケストレーター

Floorp の包括的 QA を統括する。領域ごとの QA は subagent skill に委譲し、
結果を集約して Discord にレポートする。

## 領域と skill の対応

| area | skill | matrix |
|---|---|---|
| workspaces-backup | floorp-qa-workspaces-backup | .agents/qa/matrix/workspaces-backup.yaml |

(matrix にあって skill がない領域は未実装としてユーザーに伝える)

## 実行手順

1. 引数を解釈する: `<area>` と任意の scope(`P0`/`P1`/`all`/ID 列)。
   引数がなければユーザーに対象を確認する
2. `.agents/qa/harness.md` を読み、実行環境の前提(qa-drive が動くか、
   対象ビルドは何か)を把握する
3. 対象領域の subagent skill を `/floorp-qa-<area> <scope>` で呼び出す。
   複数領域なら直列で1つずつ実行する(ブラウザインスタンスの競合を避けるため並列にしない)
4. 各 subagent の報告を集約する:
   - `_dist/qa-results/<run>/` の verdicts.jsonl / report.md / summary.json を確認
   - 報告と実ファイルの整合をスポットチェックする(pass 主張の証跡が存在するか)
5. 統合レポートを作り Discord に投稿する:

```bash
python3 .agents/qa/tools/discord-post.py \
  --message "<統合要約>" \
  --file <run_dir>/report.md \
  --file <run_dir>/verdicts.jsonl \
  --thread-name "Floorp QA <area> <date>"
```

   - 要約は **fail/blocked を先頭に**、項目 ID と1行理由を列挙し、
     pass は集計数のみ。release-blocker 関連の発見を最上位に書く
   - 認証情報未設定(終了コード3)なら: ローカルの report.md パスを報告し、
     `.agents/qa/local.env` に `DISCORD_BOT_TOKEN`/`DISCORD_CHANNEL_ID` が必要なことを
     ユーザーに伝える(秘密はリポジトリに書かない)
6. 最終報告をユーザーに返す: 集計、fail/blocked の一覧、証跡パス、Discord 投稿状況

## カバレッジ追跡

- matrix の項目数に対する実行・pass 率を報告に含める
- `manual-required` が多い領域は、自動化できない理由を分類しユーザーに提示する

## 禁止事項

- subagent の報告を検証なしに鵜呑みにして Discord に流さない(証跡の存在確認をする)
- fail を隠さない。pass 率を盛らない
- 並列で複数の QA subagent を走らせない(ブラウザインスタンス・ポート競合)
