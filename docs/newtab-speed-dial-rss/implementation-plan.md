# 新しいタブ・Speed Dial・RSS の実装計画書

更新日: 2026-10-29
状態: 比較元 `0c5b265d73c23f3811b4c2199eb61186e8949003` からの本体実装に対応する工程・成果物・配布前確認。実行済みの QA と残る制約は [QA レポート](qa-report.md)を正とする。

[文書一覧](README.md) · [製品仕様](../newtab-speed-dial-rss-spec.md) · [設計書](design.md)

## 1. 到達点

既存の背景・検索・時計・履歴候補を保ち、新しいタブへ Places 連動の Speed Dial・入れ子フォルダーと RSS リーダーを追加した。設定は既存 Floorp Hub に集約し、共有サービスと専用 actor で両画面を接続する。

工程の依存は **P0 → P1 → P2/P3 → P4/P5 → P6**。P0〜P5 の本体コードを追加し、P6 の検証を実施する構成である。下表の「実装」はコードの存在を表し、全 OS・全受入条件の合格を表さない。検証結果は重複転記せず QA レポートへ集約する。

## 2. 工程と実装成果物

| 工程 | 実装した成果物 | 受入条件・証拠の確認先 |
| --- | --- | --- |
| P0 契約・保存所有者 | `libs/newtab` の要求/応答・入力検証・設定 schema 1、正規文書判定、親 actor。Places・設定・RSS の正本を分離 | 型検査、設定の未知キー・将来版・競合、actor の URI/private/通知・要求記録のテスト |
| P1 設定・Hub | `SettingsService` の比較更新、`/features/newtab`、4分類・背景5方式・スライドショー・透明効果・検索・ja/en、既存 Start 無効化の移設 | 設定検索先/フォーカス、CAS 競合、他画面への反映、フォルダーピッカーの取消し |
| P2 Places・WS・移行 | 専用 root/common、実 GUID の階層操作と observer、呼び出し元 window の WS、journal 付き全 pinned 移行 | 範囲・循環・並べ替え・削除確認、外部編集、WS 削除/割当先消失、中断/再開/明示修復 |
| P3 RSS | `FeedService/Store/Scheduler/Network/Parser`、schema 2 SQLite、RSS2/Atom1/HTML候補、匿名取得、AST、既読・スター・保持 | 取得/redirect/timeout/Retry-After、再配信、容量と解除、parser と表示の fixture、private からの拒否 |
| P4 新タブ・リーダー | ホーム・入れ子・履歴候補・専用リーダー、空/失敗/保存状態、既存背景資産・旧配置・表示選択 | 実データ操作、キーボード・フォーカス・狭幅・言語・低減表示、本体のスクリーンショット |
| P5 スポンサー | 既存 Cubesoft 1件同梱、pref カタログの最大2枠、表示/非表示/復元、表示内容を照合した通常ブックマーク保存 | 同時非表示の保持、掲載変更競合、未配信/無効値/期限、同 URL 重複抑止、保存後の独立 |
| P6 回帰・配布確認 | 実ブラウザーハーネスと専用プロファイルでの統合確認、ビルド・smoke、実装レビュー | [QA レポート](qa-report.md)の実行ログ・成功範囲・未検証事項。公開・マージ判断とは分ける |

## 3. 設計案からの変更・採用判断

- 予定していた API 名ではなく、`libs/newtab/types.ts` の `snapshot/folder/bookmark/articles/articleState` 等を使用する。両ページが `newTabClient.call()` と `subscribe()` を共有する。
- RSS 保存は `<profile>/floorp/feeds.sqlite` の schema 2 とした。別の ArticleState テーブルを作らず、記事に状態を持ち、本文整理後は tombstone へ既読を残す。
- RSS 数値は既定15分、並列4、開始 URL の origin ごと2、全リダイレクト合計20秒、feed5MiB、最大5回を採用した。リダイレクト先 origin ごと2件までの厳密な枠は保証しない。
- 非スターは既定90日・500件/フィード、DB 本体100MiB、tombstone365日・10万件とした。これらはコードの上限であり、性能の実測結果ではない。
- private では親 actor が通常データの取得・永続変更・移行をサービス開始前に拒否する。標準 `about:privatebrowsing` を変更しない。
- 本番スポンサーの新しい配信 endpoint は作らず、既存 Cubesoft 1件と pref カタログに限定する。2枠は最大掲載数であり、空枠や架空の広告を表示しない。追加の掲載契約・素材・計測は別の作業とする。
- 旧 Floorp Start UI への feature gate は実装していない。既存 Start 無効化は再起動後に Firefox 標準へ戻す。旧 UI へ戻す場合は対応コミットの revert または対応版への変更を使い、保存データを自動削除しない。

全要件の受入条件とこれらの採用判断は別である。未実施の性能評価、全 OS、旧版との互換確認を、実装した値だけで合格にしない。

## 4. 継続テストと確認対象

要件の対応は UI-01〜08 → P1/P4/P6、SD-01〜10 → P2/P4/P6、RSS-01〜10 → P3/P4/P6、SP-01〜05 → P5/P6、HUB-01〜04 → P1/P6、MIG-01〜05 → P0/P2/P6、NFR-01〜06 → 全工程。AC-01〜12 は結果を記録するための受入条件であり、項目があるだけで合格とはしない。

| テスト・対象 | 主な確認 |
| --- | --- |
| [`newtab/test/`](../../browser-features/modules/modules/newtab/test/) | 設定の比較更新・未知キー・旧値、Places 管理範囲・移動・削除、移行の中断と利用者編集 |
| [`actors/test/NRNewTab.test.mts`](../../browser-features/modules/actors/test/NRNewTab.test.mts) | URI/入力/private、初回通知、要求記録、スポンサー・割当の競合 |
| [`feeds/test/`](../../browser-features/modules/modules/feeds/test/) | parser、scheduler、実 DB の再取得・状態・保持・解除・schema |
| [`pages-settings/test/app/newtab-settings.test.ts`](../../browser-features/pages-settings/test/app/newtab-settings.test.ts) と既存統合テスト | 検索カテゴリ/項目 ID、Start 無効化の同値・隣接設定保護 |
| [`pages-newtab/test/features/newtab/`](../../browser-features/pages-newtab/test/features/newtab/) | 許可 AST の描画、画像の明示取得、背景 timer とフォルダー復帰 |
| 実ブラウザー操作 | 実 Places/SQLite/actor と表示・保存・再起動、ネイティブ操作、window 別 WS |

変更レビューで、同時スポンサー非表示の更新欠落、初回 snapshot 中の通知取りこぼし、削除済み割当による保存失敗、掲載変更と保存の競合、応答 cache の画像保持を修正した。関連するテストと再検証の結果は QA レポートへ記録する。

### 再実行コマンド

本体テストは [`AGENTS.md`](../../AGENTS.md) の実 Firefox 内ハーネスを使用する。ただし現在の runner では `--near` を task へ直接渡す。`deno task test -- --near ...` は使わない。

```bash
deno task test --near browser-features/modules/modules/newtab
deno task test --near browser-features/modules/modules/feeds
deno task test --near browser-features/modules/actors/test/NRNewTab.test.mts
deno task test --near browser-features/pages-settings/test/app/newtab-settings.test.ts
deno task test --near browser-features/pages-newtab
deno task test:smoke
```

page の production build は各ページの Vite 設定で行える。

```bash
node_modules/.bin/vite build --config browser-features/pages-newtab/vite.config.ts browser-features/pages-newtab
node_modules/.bin/vite build --config browser-features/pages-settings/vite.config.ts browser-features/pages-settings
```

配布全体のビルドは既存の `deno task feles-build build` の before/after-mach 工程を使う。上記ページ build の成功を配布全体の成功へ読み替えない。実際に実行したコマンド・実行物・環境は QA レポートに記録する。

`bridge/loader-features/vite.config.ts` の React 対象に pages-newtab を含める。本体ページのテストを Solid/XUL として変換しないための変更であり、新しいページの実装自体をテストから除外するものではない。

プロトタイプを変更するときだけそのディレクトリーの `npm run check` / `npm run build` を使う。mock transport や Chromium の画面確認は UI の補助検証であり、Gecko の権限・IPC・移行の証拠は実 Floorp で得る。

## 5. 残る計測・配布前確認

次の表は継続して確認する項目。QA レポートに個別の結果がないものを成功扱いにしない。

| ケース | 確認すること |
| --- | --- |
| 対応 OS と配布 | Windows/macOS/Linux のネイティブフォルダー/画像選択、packaged/dev 両経路、全配布工程 |
| 初回・再表示 | 同じ実行物/機器で、旧版との差、操作可能になる時刻、JS/画像量、キャッシュ有無 |
| 多数の新しいタブ | 20タブの scheduler 数・要求数・通知・メモリ。タブ数に比例した重複取得をしないこと |
| 大きいデータ | 1万ブックマーク、深い階層、100購読・1万記事で直下取得・ページ送り・検索を測定 |
| 背景・透明化 | ぼかし有効/無効、長時間スライドショー、低性能端末の CPU・メモリ・描画遅延 |
| 操作と表示 | 320〜1536px、200%、長い日本語名、ja/en、RTL、キーボード、低減設定、強制カラー |
| 復帰・互換 | 既存 profile、中断/再開、外部 Places 編集/Sync、対応する旧版へ戻すときの表示と保存値 |

採否を判断できる性能予算は別途実測して記録する。サンプル規模や DB 上限の採用、単体デモの容量、実行していない OS の推測で性能合格を宣言しない。

## 6. 移行失敗と復帰手順

1. 元の設定 raw と移行 journal を保持する。破損値や未知 schema を初期値で上書きしない。予定 GUID を記録してから Places を更新する。
2. 中断後は GUID と親子関係・receipt を照合する。移行済みの利用者編集・移動・削除は巻き戻さず、commit の有無が不明なら自動再生成せず明示修復へ案内する。
3. 完了済み root/common が失われた場合の修復は空の入口を作る。旧 pinned を再投入して消したサイトを復活させない。
4. Firefox 標準へ戻す場合は Hub の既存 Start 無効化と再起動を使う。旧 Floorp Start UI は feature gate では選べず、対応コミットの revert または対応版への変更が必要になる。
5. どの復帰でも新しい Places、RSS DB、journal を自動削除しない。旧 pinned と新しい階層を双方向同期する保証や、任意の旧版が新しい保存形式を編集できる保証はない。profile を保存した上で対応版の互換範囲を確認する。

実装 PR の QA レポートは、再現手順・証拠・残る制約とともに更新する。この計画書を更新したこと自体を、リリース・マージ・全項目検証の完了とはしない。
