# 新しいタブ・Speed Dial・RSS の QA 記録

検証日: 2026-10-28〜29

対象ブランチ: `codex/newtab-speed-dial-rss`

比較元: `0c5b265d73c23f3811b4c2199eb61186e8949003`

2026-10-29 の Windows 本体 UI 改善は [表示・操作改善と Windows QA](windows-visual-qa.md)に分けて記録する。以下の先行 Linux 検証結果はそのまま保持する。

## 環境と範囲

- Linux x86_64、Xvfb、実 Floorp / Firefox **159.0**、build **20261026023026**。
- `floorp-runtime.lock.json` の Runtime を使用。source commit: `0ac255739cdd64e100cd6913329247421b0ec2ea`、配布物 SHA-256: `8b130d9f8c0e1c032064f9e1163db1d6000f955c29ac16036ac19138f0c445f5`。
- 現行 Runtime patch の適用後、作業専用プロファイルで実行。既存の `_dist/profile/test` と利用者プロファイルは使用していない。
- colocated tests は Gecko 内の公式ハーネスで実行。HTTP 検証はローカルの RSS/Atom・画像・エラー応答 fixture を使用し、実 actor、Places、SQLite、ネイティブ通信を通した。
- プロトタイプの localStorage や、Chromium の画面確認を本体 QA の代わりにはしていない。

## 実ブラウザーの操作・通信

| 対象 | 確認結果 |
| --- | --- |
| 入れ子のブックマーク | 2段階のフォルダー作成、サイト追加、編集ダイアログからの移動・改名が実 Places に保存された。メニューの Escape とフォーカス復帰も成功。 |
| ワークスペース・通知 | 2つのネイティブウィンドウで別の現在ワークスペースを保持し、それぞれの割当グループを自動選択。他ウィンドウからの Places 更新と Hub の時計設定変更も表示へ反映した。 |
| プライベート | 明示的な新しいタブへのアクセスでも、通常プロファイルの背景・ブックマーク・RSS・広告・ワークスペースを返さず、購読操作を `READ_ONLY` で拒否した。 |
| 既存の背景と検索 | ローカル PNG のアップロードとファイル名の保存、Hub の背景・スライドショー・透明化設定、呼び出し元ウィンドウの URL バーへの検索フォーカスが成功。 |
| 狭幅 | 320 CSS px / 200% ズームでホームの横方向オーバーフローなし。 |
| RSS / Atom | 実 UI からの購読、配信された本文・抜粋、HTML alternate 候補の明示選択、既読・スター、解除後のスター・既読・出典保持が成功。 |
| 本文・画像 | 危険な要素・属性・URL を許可 AST へ変換。表示だけでは元記事・追跡画像を取得せず、明示操作後だけ匿名で取得した PNG の data URL を表示。SVG は拒否。 |
| HTTP | ETag / Last-Modified と 304、通常 / 異なるオリジンへのリダイレクト、Cookie / Authorization の不送信を確認。 |
| 失敗時 | 20秒のタイムアウト、429、リダイレクトループ・危険 URL・展開後5MiB超過・DOCTYPE/外部 entity・壊れた XML の拒否を確認。外部 entity の参照先への要求なし。 |
| 同時更新 | 同じフィードへの20並列 refresh 呼び出しを HTTP 1要求へ集約。20並列 snapshot も成功。これは20タブの性能保証ではない。 |

## 最終チェック

実 Firefox 内の最終 suite は **15モジュール / 122ケース / 失敗0**。新しいタブ関連の14モジュール・71ケースに加え、今回変更した既存 Hub integration の51ケースを実行した。

| テスト | ケース数 |
| --- | ---: |
| SettingsService / BookmarkService | 7 / 6 |
| FeedParser / FeedScheduler / FeedStore | 5 / 3 / 5 |
| NRNewTab actor・広告・割当 | 9 |
| Hub 新しいタブ設定 | 4 |
| 新しいタブの本文 / 背景タイマー / フォルダー復帰 / 既存 utility | 4 / 3 / 4 / 21 |
| 既存 Hub integration | 51 |

`deno task test:smoke` は全6工程成功。runner unit tests 41件、chrome / ESM / pages のテスト発見、CI 対象ソースの型検査、全1,280ファイルの lint を含む。新しいページ・サービス・テストの個別型検査も成功した。

GitHub の文書チェックで検出された生成カタログ9件も、新しい Hub ルートと actor に合わせて更新した。docs inventory の収集・検証と docs pipeline の53テストは成功。

本番用 bundle を locked Runtime に組み込み、開発用 HTTP loader を使わずに再起動した検証も **9項目 / 失敗0**。`about:newtab` が `chrome://noraneko-newtab/content/assets/` の script を読み込み、強化 CSP が有効であること、内蔵 AVIF 背景と半透明表示、明示画像の data URL 許可・直接 HTTP 画像の遮断、ブックマーク階層・記事の既読/スター/出典の保持を確認した。外部 Places 操作で現在フォルダーを削除した際の直近の親への復帰と、`about:hub` の接続も成功。

本体の画面: [ホーム](qa/home.png) · [RSS リーダー](qa/reader.png) · [Hub 設定](qa/hub.png)。画面内の QA 名・ローカル URL は専用プロファイルの fixture であり、新規プロファイルに配布する初期データではない。

[項目別の実行結果](qa/results.json)には HTTP/Reader 25項目、ワークスペース/通知6項目、複数ウィンドウ/private5項目、背景/狭幅2項目、本番ページ9項目の成功記録を保存した。これらは上記の操作を分解した確認であり、71ケースへ加算したテスト総数とは扱わない。

既存 Hub integration の初回追加実行では、復元された複数ウィンドウのため fixture 側が背面となり、メモリ設定の blur commit が発火しなかった。専用プロファイルのセッション復元を退避し、前面の1ウィンドウで再実行すると51ケースすべて成功した。ソース修正は不要で、利用者プロファイルのセッションは変更していない。

再実行にはリポジトリーの開発環境と locked Runtime が必要。現在の runner では `--near` を直接渡す。

```bash
deno task test --near browser-features/modules/modules/newtab
deno task test --near browser-features/modules/modules/feeds
deno task test --near browser-features/modules/actors/test/NRNewTab.test.mts
deno task test --near browser-features/pages-newtab
deno task test --near browser-features/pages-settings/test/app/newtab-settings.test.ts
deno task test --near browser-features/pages-settings/test/integration/settings.test.ts
deno task test:smoke
```

## QA で修正した問題

- Gecko ESM の timer / base64 API と、匿名フィード取得の opaque response 制限を実 Runtime に合わせた。各 HTTP hop を検証し、その URL の content principal で新しい匿名 channel を開く。全 hop の合計20秒と最大5回のリダイレクト制限を保つ。
- actor の初期化タイミングによる未接続、最初の snapshot 中の変更通知の取りこぼしを修正。
- 同時スポンサー非表示による更新欠落、表示後の広告差し替えと保存の競合を修正。
- actor の完了応答キャッシュが画像を保持し続ける問題を解消。処理中の要求だけを保持し、完了した変更操作は小さな ID のみを記録する。
- 削除済みグループ / ワークスペースの古い割当を整理し、新しい有効な割当を保存できるようにした。
- 削除された現在フォルダーから、同じグループ内の最も近い残存祖先へ戻るようにした。
- 旧スライドショーの `0`・小数秒・長い待機時間を維持し、timeout の桁あふれによる連続画像要求を防止。
- Hub の別画面が Floorp Start の最新設定を古い値で上書きしないよう、design 設定も CAS で保存。

## 出荷時に別途確認する範囲

Windows / macOS のネイティブファイル・フォルダー選択、署名済みインストーラー、実 Firefox Sync の複数端末往復は、この Linux 環境では検証していない。1万ブックマーク / 多数購読での性能予算と低性能端末の blur 比較も未計測。

広告は既存の Cubesoft の掲載情報のみを同梱。最大2枠、非表示・復元・通常ブックマークへの保存は実装しているが、新しい広告配信 endpoint・素材・計測は提供されていないため追加していない。

更新要求は全体4件・開始 URL の origin ごと2件まで。異なる開始 origin が同じリダイレクト先へ合流する場合、最終 origin の同時2件までは保証せず、全体4件の制限を保つ。

詳細ログと作業用画面は `/workspace/artifacts/newtab-production-qa/` に保存。プロファイル、Runtime、生成 bundle、失敗調査中の古い画面はソースへ含めない。本書の最終結果を受入記録とする。
