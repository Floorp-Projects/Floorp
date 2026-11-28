# Floorp QA システム

Floorp の全機能を包括的に QA するための skill + matrix + harness 群。

## 構成

```
.agents/
├── qa/
│   ├── README.md            このファイル
│   ├── harness.md           検証ティア・qa-drive・証跡ルールの全知識(必読)
│   ├── local.env.example    Discord 認証情報のテンプレート
│   ├── matrix/              機能領域ごとの機械可読 QA 項目カタログ
│   │   └── workspaces-backup.yaml   (85項目: A-01〜A-80 + R-01〜R-05)
│   └── tools/
│       ├── qa-drive.ts      harness CLI (launch/os/eval/shot/stop/status)
│       ├── mozlz4.py        .jsonlz4 展開(依存なし)
│       └── discord-post.py  Discord レポート投稿
└── skills/
    ├── floorp-qa/                     オーケストレーター(ユーザー向け入口)
    └── floorp-qa-workspaces-backup/   領域 QA エグゼキュータ(subagent)
```

## 使い方

```
/floorp-qa workspaces-backup P0     # release 判断用の最小セット
/floorp-qa workspaces-backup all    # 全項目
/floorp-qa workspaces-backup A-39,A-64   # 個別項目
```

結果は `_dist/qa-results/<timestamp>-<area>/` に verdicts.jsonl + evidence + report.md
として残り、要約が Discord に投稿される(認証情報は `.agents/qa/local.env`)。

## 新しい領域を追加するには(横展開の手順)

1. **matrix を書く**: `.agents/qa/matrix/<area>.yaml`。
   `workspaces-backup.yaml` をテンプレートに、項目ごとに
   `id / spec / tier / automation / approach / priority` を定義する。
   spec は QA 候補ドキュメント(例: floorp-12.21.3-agentic-qa-candidates.md の B〜K 領域)や
   機能の実装仕様から転記し、approach は harness のティアでどう検証するかを具体化する
2. **skill を書く**: `.agents/skills/floorp-qa-<area>/SKILL.md`。
   `floorp-qa-workspaces-backup` をコピーし、matrix パスと領域固有の前提確認を差し替える
3. **orchestrator の対応表に追加**: `.agents/skills/floorp-qa/SKILL.md` の領域テーブル
4. 小さい項目セット(P0 相当)で試験実行し、approach の実現性を磨く

## 領域の候補(QA 候補ドキュメントの分類より)

| 領域 | matrix | 主なティア |
|---|---|---|
| workspaces-backup | ✅ 作成済み | L1/L2 |
| tab-unload (B: Auto Unload/memory) | 未 | L1 中心 |
| pwa-ssb (C: PWA/SSB/OS integration) | 未 | L2/L3 |
| runtime-os (D: Firefox 161/OS 個別) | 未 | L3 中心 |
| design-tabbar (E: デザイン/vertical tabs) | 未 | L2/L4 |
| split-view (F) | 未 | L2 |
| sidebar-panel (G: sidebar/web panel/PiP) | 未 | L2 |
| input (H: gesture/bookmarks/palette/context menu) | 未 | L2 |
| os-api-security (I: OS server/自動化セキュリティ) | 未 | L1 |
| settings-i18n (J: welcome/settings/i18n) | 未 | L2/L4 |
| packaging (K: release pipeline) | 未 | 手順中心 |

## 設計原則

- **推測で pass にしない**。検証できない項目は blocked/manual-required
- **最も低いティアで検証する**。L1 (状態検査) で確実に分かるものを L2 UI 操作でぼかさない
- **fail を隠さない**。Discord 要約は fail/blocked を先頭に
- **使い捨てプロファイル**。実データ・実プロファイルで検証しない
- **証跡ファースト**。verdict には必ず evidence パス。途中クラッシュに備え即時追記
