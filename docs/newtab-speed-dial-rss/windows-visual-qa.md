# Windows 実機での表示・操作改善と QA

検証日: 2026-10-29（日本時間）

対象: [PR #3003](https://github.com/Floorp-Projects/Floorp/pull/3003)、`codex/newtab-speed-dial-rss`

改善前: `b8951372881cbf6c2ed847fca1bc344564beac6c`

状態: 本体の修正、関連テスト、本番Windowsでの両テーマ・背景条件・操作確認、元プロファイル復帰を完了。

既存の[製品仕様](../newtab-speed-dial-rss-spec.md)と[設計](design.md)を維持した本体 UI の改善記録。先行する [Linux QA](qa-report.md) の結果と区別し、この記録では今回 Windows で実行した範囲だけを扱う。

## 環境とデータ保護

- Windows ネイティブ PowerShell、作業ディレクトリ `E:\floorp-dev\Floorp-pr3003`。
- `floorp-runtime.lock.json` に対応する Floorp / Firefox **159.0**、build **20261026023026**。Gecko 全体の再ビルドは行わず、`deno task feles-build stage --marionette` で本番 assets と CSP を使う。
- 開始時の `git status --short` は空。ローカル HEAD と PR の head は上記改善前コミットで一致し、PR は open・未マージだった。
- 既存の `_dist/profile/test` は `_dist/profile/pre-visual-20261029` へ退避し、その複製を `test` として使用した。検証後、QAブラウザーを正常終了し、QA側を `_dist/profile/visual-qa-20261029` に保存して元のデータを `test` に復帰した。`prefs.js`・`places.sqlite`・`favicons.sqlite`のSHA-256が退避元と一致。普段使いのプロファイルは使用・初期化していない。
- Browser plugin はこのセッションにない。実際の Windows 版 Floorp で確認する依頼に従い、リポジトリの Marionette client と Windows Computer Use（`@oai/sky`）を使用した。Chromium やプロトタイプの画面を本体 QA の代替にしていない。
- 日本語の長い記事名・配信本文・画像・空応答・エラー応答は、`127.0.0.1` の一時 QA fixture を使用。ブックマークや記事の確認用データは検証プロファイルだけに置き、本番コードへデモ記事・広告・ワークスペースを追加していない。
- 作業用ログ・スクリプト・未選別画像は `_dist/visual-qa/`。採用した画像だけを `qa/` へ保存する。

## 改善前の課題と変更理由

現在の Floorp Hub の書体・紫の操作色・共通部品・余白を基準にした。[Web-v5 のデザインシステム](https://github.com/Floorp-Projects/web-v5/blob/master/docs/design-system/README.md)も参照し、ホーム中央の検索、右上の時計、Speed Dial と RSS の既存構成を保った。

| 対象 | 改善前に確認した課題 | 本体の変更 |
| --- | --- | --- |
| 背景と半透明パネル | 写真の模様が本文領域へ強く透ける。暗いテーマの操作色と明るいパネルが混在し、操作部品の見分けが弱い | 壁紙があるときの白固定の上書きを除去し、ホーム・Speed Dial・Reader・Hub をブラウザーの明暗テーマへ揃える。`data-theme` も共通トークンへ伝え、パネル90%・ホームRSS96%・タイル76%の面と境界を使用。時計にもテーマに合った下地を設ける。Geckoで重複した prefixed 宣言が標準宣言を打ち消していたぼかしを修正 |
| Speed Dial | タイルが大きく、長い名前が一行で切れる。フォルダーとサイトの区別、長いグループ名の扱いが弱い | タイル高と余白を抑え、名前は二行まで表示。完全な名前を title に保持。フォルダーへ淡い紫の面と境界を付け、狭幅でも項目数を残す。グループ名の幅を制限し、アイコンが潰れないようにする |
| パンくず・メニュー・編集 | パンくずが周囲の背景に埋もれやすく、画面下端や拡大時にメニュー末尾へ届きにくい。ダイアログ内余白のクリックでも閉じる | パンくずへ区切られた面を追加し、親へ戻る操作の参照先を修正。メニューは実際の高さで上・下を選び、内部スクロールで閉じないようにする。移動先・ボタンを折り返し、ダイアログは外側のクリックだけで閉じる |
| RSS 一覧と本文 | 配信元・日付の文字が小さく、長文タイトルや本文の読み進め方、狭幅で一覧に戻る位置が分かりにくい | 補助文字を 13px に揃え、一覧の行間・折り返し・フォーカスを調整。本文の最大幅を 76ch に制限。記事を閉じても一覧の選択を保ち、元の項目へフォーカスを戻す |
| RSS の状態表示 | 取得失敗と空の一覧が重なって見える。記事切り替え中に前の記事が残り、画像取得中の操作も分かりにくい | 空・エラーを分け、一覧・本文に再試行を用意。記事読み込み時の旧本文を解除し、画像取得中と購読処理中のラベル・busy 状態を示す |
| Hub | カテゴリと最初の項目の間の余白が大きい。設定と説明の関係、透明化を選べない理由、狭い本文領域での入力幅が分かりにくい | カテゴリと保存状態をまとめ、共通部品の重複余白を整理。背景の方式・画像・スライドショーに続けて透明化を配置し、背景なしの場合の理由を日英で明記。ホームのフィード表示を明確にし、狭いコンテナでは選択欄を縦配置する |

機能仕様の変更はない。背景の各方式・既存画像・スライドショー・時計・履歴候補・既存の配置設定、実 Places の階層とウィンドウ別ワークスペース連動、RSS の既読・スター、スポンサー最大2枠の別管理を維持する。RSS は引き続き配信された本文だけを表示し、画像は明示操作で取得する。元記事の追加全文取得は行わない。

## 画面比較

比較はすべて本体の Windows 画面。ウィンドウは1440×1020、ページ領域は **1382×938 CSS px、100%ズーム**。ホームと Speed Dial は内蔵背景 `1100-1600.png`・透明化あり。Reader と Hub は読みやすさを優先した通常の面で表示する。

改善前は暗いブラウザーテーマでも壁紙上の面が白固定だった。改善後の暗いテーマ列はこの不整合の解消を含む比較であり、明るいテーマ列でも同じ画面を記録した。最終比較の撮影前に fixture の3記事を未読・スターなしへ戻し、Readerでは先頭記事を開いた。時計と更新時刻の違いはデザイン変更ではない。

| 画面 | 改善前 | 改善後・暗いテーマ | 改善後・明るいテーマ |
| --- | --- | --- | --- |
| ホーム | [before](qa/windows-before-home.png) | [dark](qa/windows-after-home.png) | [light](qa/windows-light-home.png) |
| Speed Dial | [before](qa/windows-before-dial.png) | [dark](qa/windows-after-dial.png) | [light](qa/windows-light-dial.png) |
| RSS リーダー | [before](qa/windows-before-reader.png) | [dark](qa/windows-after-reader.png) | [light](qa/windows-light-reader.png) |
| Hub | [before](qa/windows-before-hub.png) | [dark](qa/windows-after-hub.png) | [light](qa/windows-light-hub.png) |

最終の8画面は `_dist/visual-qa/final-comparison-result.json` で URL、テーマ、ページ幅、`scrollWidth`、本番 script URI を照合。いずれもページ幅と `scrollWidth` は1382pxで一致した。[測定・操作結果の抜粋](qa/windows-checks.json)も保存した。

200%の実画面: [ホーム](qa/windows-dark-home-200.png)・[Reader](qa/windows-dark-reader-200.png)・[メニュー末尾のフォーカス](qa/windows-light-menu-200.png)・[Hub](qa/windows-light-hub-200.png)・[暗いダイアログ](qa/windows-dark-dialog-200.png)・[明るいダイアログ](qa/windows-light-dialog-200.png)。いずれもページ領域321×359 CSS px。

背景条件の例: [暗いテーマ＋白背景＋透明化](qa/windows-dark-white-glass.png)・[明るいテーマ＋白背景＋透明化](qa/windows-light-white-glass.png)・[暗いテーマ＋背景なし](qa/windows-dark-none-opaque.png)・[明るいテーマ＋背景なし](qa/windows-light-none-opaque.png)。[購読処理中の状態](qa/windows-feed-loading.png)も記録した。

## 実行したテストとビルド

| 検証 | 結果 | 証拠・範囲 |
| --- | --- | --- |
| `deno task test --near browser-features/pages-newtab` | **10 モジュール成功、失敗0** | 実 Firefox 内。本文の安全な描画・画像の明示取得、Reader 2ケース、Modal 1ケース（標準取消とフォーカス復帰を3回）、背景タイマー、フォルダー復帰、既存 utility。最終再実行は `_dist/visual-qa/modal-newtab-tests.log` |
| `deno task test --near browser-features/pages-settings/test/app/newtab-settings.test.ts` | **1 モジュール・4ケース成功、失敗0** | 実 Firefox 内の Hub 新しいタブ設定テスト。`_dist/visual-qa/hub-tests.log` |
| `deno task test --near browser-features/pages-settings/test/integration/settings.test.ts` | **1 モジュール・51ケース成功、失敗0** | 既存 Hub integration。`_dist/visual-qa/hub-integration.log` |
| `deno task test:smoke` | **6 工程成功** | runner unit **41 ケース成功・失敗0**、chrome / ESM / pages のテスト発見、CI 対象ソースの型検査、**1,282 ファイル**の lint。`_dist/visual-qa/modal-smoke.log` |
| 改善後の `deno task feles-build stage --marionette` | **ビルド・起動成功** | 最終のModal修正を含む本番 bundle・locked Runtime・CSP を使う実機確認。`_dist/visual-qa/modal-stage.log`。テスト用 HTTP ページの成功とは分ける |
| 最終テーマ/CSS の newtab production build | **成功** | `_dist/visual-qa/final-theme-build.log`。最終8画面で更新後の `chrome://noraneko-newtab/content/assets/` を確認 |

モジュール数をケース数として数えない。ページのビルド成功を署名済み配布物全体の QA 成功として扱わない。

## Windows 本体の受け入れ確認

以下は実際の操作と保存状態の確認結果。テーマ・ぼかしの最終調整前に行った背景と狭幅の結果は、その後の最終組み合わせ確認と分けて記載する。

| 確認項目 | 手順・確認点 | 結果 |
| --- | --- | --- |
| 本番ページの成立 | `about:newtab` と `about:hub#/features/newtab` の URL・内容・本番 script URI・CSP | 本番の chrome URI と CSP を確認。最終両テーマ8画面も本番 asset を読み込み、空画面・framework overlay なし |
| 明暗の背景・透明化 | 白・黒の QA 画像、透明化あり/なし、背景なしの両テーマ | 最終12通りが成功。両テーマで画像あり・透明化onは `blur(24px)`、off/背景なしは `none`。面色と文字色もテーマへ追従し、横 overflow なし（`final-matrix-result.json`） |
| 両テーマ・広幅 | ホーム・Speed Dial・Reader・Hub を暗いテーマ/明るいテーマで表示 | 最終8画面でテーマに沿う面と文字色を確認、1382×938 CSS pxで横 overflow なし |
| 狭幅・200%ズーム | 742 CSS px/100%、321×359 CSS px/200%。ホーム・Speed Dial・Reader・Hub・編集ダイアログ | 先行確認で横 overflow なし。本文領域と操作バー、幅287pxのダイアログが画面内。Hubの4カテゴリを保持。Readerの操作バーを見失うレイアウトも修正して再確認 |
| キーボード | Enter、End、Escape、Shift+Tab、フォーカス表示と復帰 | メニュー最終項目へEndで到達、Escapeで元のメニューボタンへ復帰。ReaderをEnterで開くと本文へ移動し、戻ると元記事を再選択。Shift+Tabで元サイト操作へ到達。Modal修正後にEscapeを10回とWindows実キーで確認し、毎回閉じて作成ボタンへ復帰 |
| フォルダー整理 | 入れ子の作成・移動・改名・削除確認、実 Places への保存と画面更新 | 入れ子を作成し、親フォルダーを移動・改名。GUIDと子階層を保持。削除確認に子フォルダー1件を表示し、削除後に実データから消えた。パンくずで共通グループへ戻った |
| RSS 閲覧 | 記事選択、既読/未読、スター、一覧へ戻る位置とフォーカス | 選択により本文と既読状態が更新。スターと未読への変更を保存値で確認。狭幅で戻ると同じ記事ボタンへフォーカスし、選択表示も保持 |
| RSS の状態 | 空、読み込み中、通信失敗、再操作 | 空の検索結果、HTTP503の理由と再操作可能なフォーム、5秒の遅延中のbusy状態と完了を確認。取得中の購読ボタンを「読み込み中」に修正 |
| RSS 本文・画像 | 長文・長い連続文字、明示画像、元記事への通信 | 初期表示は画像placeholderで外部imgなし。明示操作後に1px画像を読み込み。fixtureログの `/image.png` は1要求、`/article/` 要求なし |
| Hub の反映 | 時計・背景・透明化の設定と開いた新しいタブの表示 | 時計offを新しいタブで確認後、onが開いたページへ即時反映。背景・透明化の設定値と画面を先行マトリクスで照合 |
| スポンサー | メニューから保存・非表示、Hubから復元 | 実Placesへ通常ブックマークとして保存。非表示後の保存設定、Hubの復元後に既存1枠が再表示されることを確認。確認用ブックマークは削除 |
| コンソール | stageログと本体画面 | Windows JumpListsのテストURL用favicon警告、既存CustomizableUI・ロケール警告を記録。これらをページの成功判定へ混ぜない。別記の native crash は除外せず制約として残す |
| 最終の背景・幅の組み合わせ | テーマ・ぼかし修正後に明暗背景、透明化、狭幅/200%を再確認 | 背景12通りと両テーマのホーム・Speed Dial・Reader・Hub各200%で成功。全ページの `scrollWidth` は321px。メニュー末尾が画面内、ダイアログも画面内。Reader操作バーは上端112px・下端184pxで本文と独立して表示 |
| 元プロファイルの復帰 | QA ブラウザーを終了し、元の確認データを `test` へ戻す | 正常終了とcloneプロセス消滅を確認後に復帰。主要3ファイルのSHA-256一致、QA側データも別ディレクトリで保持 |

操作の記録は `_dist/visual-qa/` の `folder-move-result.json`、`reader-actions-result.json`、`reader-state-result.json`、`feed-errors-result.json`、`narrow-result.json`、`final-layout-result.json` を参照。初回のフォルダースクリプトは画面遷移後の古いセレクターで停止したが、画面を再取得して継続し、移動・改名・削除を保存状態まで確認した。途中停止したログを全体成功として数えていない。

## QAで見つけたダイアログ終了時のクラッシュ

10:30:55と10:42:54に、700×800・200%のダイアログをEscapeで閉じる際に親プロセスが終了した。初回のWER Event 1000/1001 は `xul.dll 159.0.0.3`、`c0000005`、offset `0x1c4bb78`、report ID `4f220b50-e613-432b-ad14-06ce28bb8f1d`。dumpがないためネイティブ内部の根因は特定できていない。DOM clickで開く再確認では成功したが、起点へフォーカスしてEnterで開く手順で再現した。

従来の`onCancel`は標準取消を止めてReact stateを閉じ、native cancelイベント中にアンマウント・`dialog.close()`・フォーカス復帰を行っていた。標準取消を許可し、非同期の`close`イベントでReact stateを同期する形へ変更した。native終了中にダイアログを削除しない回帰テストを実Firefox内に追加した。

修正後の本番stageで、両テーマの200%確認2回、追加反復8回（うち3回は開いた後に100%→200%・ウィンドウ縮小）、Windows実キー1回が成功した。すべて閉じて起点へフォーカス復帰。通常の取消、Enterでの作成・保存、購読完了による終了も成功した。証拠は`modal-narrow-result.json`、`modal-repeat-result.json`、`final-actions-result.json`、`final-loading-result.json`。再現したUI経路への修正と確認であり、Gecko内部の障害原因まで解明したとは扱わない。

## 残る制約

- ダイアログ終了経路の修正後に再現手順は成功したが、native dumpがなくGecko内部の根因は未特定。長時間・全環境での再発がないことまでは保証していない。
- 署名済みインストーラー、macOS、実 Firefox Sync の複数端末往復、大量ブックマーク・購読の長時間性能、低性能端末での blur 負荷は、この視覚改善の検証で合格にしていない。
- ローカル HTTP fixture は表示・操作の再現用であり、実在するすべての配信元への互換性を保証するものではない。
- 機能仕様の変更を必要とする提案は今回の修正へ含めていない。

## 全画面レビュー後の配置調整

同日、`7e1e622`の実画面について「上に偏っている」「よく使うサイトが無機質」という追加レビューを受け、2560×1440の全画面で調整した。以下は先行QAとは別の追加検証。

- 中央配置では、検索・Speed Dial/RSS・履歴候補を一つの領域として、余った高さを上下へ分配する。時計はその外の右上に保つ。高さを固定しないため、狭幅や内容が多い場合は上から通常どおりスクロールできる。「サイトを先に表示」は既存の順序と上寄せを維持する。
- よく使うサイトは、常時表示の長方形の枠を減らし、56pxの柔らかいアイコン背景・2行の名前・ドメイン表示へ変更。保存・非表示の操作領域は各44×44px。既存ライブラリのYouTube・Reddit・Firefoxロゴを使い、アイコン取得のための通信や新たな初期サイトは加えていない。
- 「読む中心」＋RSSのみ表示でも1列になるよう、単独パネルのCSS優先順位を修正。表示設定の意味や保存形式は変えていない。

| 確認 | 結果 |
| --- | --- |
| 全画面2560×1440、時計off | 検索上端120px→282.6px。ヘッダー下80pxから検索まで202.6px、履歴候補下端1237.4pxから画面下まで202.6px。上下の空きが一致 |
| 暗いテーマ、時計on | 時計上端120pxの右上配置を保持。本文と操作部品も暗いテーマへ追従 |
| RSSのみ・読む中心 | 幅1360pxの1列。空の2列目なし |
| 検索off / サイト優先 | 非表示にした検索の余白を残さず配置。サイト優先ではコンテンツ→履歴→検索の順序を維持 |
| 200%、321×359 CSS px | ページの横overflowなし。検索上端140pxで上側の欠けなし。履歴の保存・非表示へTabで移動し、両ボタン44×44pxを確認 |
| ビルド・ソース検査 | 本番newtab Vite build、Home/UIの`deno check`・`deno lint`、`git diff --check`成功。表示中心の追加変更に新しい単体テストは設けず、実画面の配置・操作を検証 |

比較: [変更前の全画面](qa/windows-layout-before-fullscreen.png) / [変更後の全画面](qa/windows-layout-after-fullscreen.png)。追加画面: [暗いテーマと時計](qa/windows-layout-clock-dark.png)、[200%の履歴候補とフォーカス](qa/windows-layout-history-200.png)、[RSSのみ](qa/windows-layout-feed-only.png)、[サイト優先](qa/windows-layout-sites-first.png)。[測定結果](qa/windows-layout-checks.json)。

検証には保存済みの専用QAプロファイルを使用し、レビュー時の設定（時計off・中央配置・ライトテーマなど）へ復帰した。新しいタブを全画面で起動したままレビューへ引き渡す。元の`_dist/profile/test`は引き続き使用していない。
