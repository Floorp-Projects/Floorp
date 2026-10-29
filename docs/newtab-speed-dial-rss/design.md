# 新しいタブ・Speed Dial・RSS 設計書

更新日: 2026-10-29
状態: 比較元 `0c5b265d73c23f3811b4c2199eb61186e8949003` へ追加した本体実装の設計。採用値は[仕様書 §10](../newtab-speed-dial-rss-spec.md#10-採用した判断と残る確認)、実行済みの検証は [QA レポート](qa-report.md)を参照する。

[文書一覧](README.md) · [仕様書](../newtab-speed-dial-rss-spec.md) · [実装計画書](implementation-plan.md)

## 1. 画面と実装の配置

[承認済みプロトタイプ](../design/newtab-prototype/DESIGN.md)を基準に、既存 Hub の共通部品・シェルへ接続した。ホームは中央検索・時計・要約、Speed Dial は混合タイルとパンくず、RSS は購読先・記事一覧・本文を配置する。背景・透明効果はホームと Speed Dial に適用し、専用リーダーと Hub は可読性を優先する。プロトタイプの画像は [home](figures/home.png) / [dial](figures/dial.png) / [reader](figures/reader.png) / [Hub](figures/hub.png)。本体のスクリーンショットや検証の証拠とは区別する。

| 実装 | 責務 |
| --- | --- |
| [`libs/newtab`](../../libs/newtab/) の `types.ts` / `client.ts` / `client-types.ts` | ページと親側の DTO・要求型、ID 付き要求と変更通知 |
| 同 `origin.ts` / `validation.ts` / `settings.ts` / `assignments.ts` | 正規文書判定、入力検証、設定 schema・patch、失効した割り当ての整理 |
| 同 `background-assets.ts` | 既存 Floorp 背景6枚の参照 |
| [`NRNewTabChild`](../../browser-features/modules/actors/NRNewTabChild.sys.mts) / [`Parent`](../../browser-features/modules/actors/NRNewTabParent.sys.mts) | 制限した gateway、要求元・ウィンドウ・private の確認 |
| [`NewTabService`](../../browser-features/modules/modules/newtab/NewTabService.sys.mts) | snapshot、各サービスへの振り分け、変更通知の集約 |
| [`SettingsService`](../../browser-features/modules/modules/newtab/SettingsService.sys.mts) | 既存設定の唯一の新規書き込み経路、revision 比較更新 |
| [`BookmarkService`](../../browser-features/modules/modules/newtab/BookmarkService.sys.mts) / [`MigrationService`](../../browser-features/modules/modules/newtab/MigrationService.sys.mts) | Places の階層・操作・観測と旧 pinned の journal 付き移行 |
| [`BrowserIntegration`](../../browser-features/modules/modules/newtab/BrowserIntegration.sys.mts) | 呼び出し元 WS、履歴候補、URL バー・Hub・フォルダーピッカー・画像読み込み |
| [`feeds/`](../../browser-features/modules/modules/feeds/) の `FeedService` / `FeedStore` / `FeedScheduler` / `FeedNetwork` / `FeedParser` | プロファイル単位の購読・取得・解析・保存・状態更新 |
| [`SponsorService`](../../browser-features/modules/modules/newtab/SponsorService.sys.mts) | カタログ選択、最大2枠、個別非表示・復元・ブックマーク保存 |
| [`pages-newtab/src/features/newtab/`](../../browser-features/pages-newtab/src/features/newtab/) | 本体 UI、文書内の選択・スクロール、型付きサービス呼び出し |
| [`pages-settings/src/app/newtab/`](../../browser-features/pages-settings/src/app/newtab/) | Hub の設定ページ・検索メタデータ・既存 Start 無効化設定の移設 |

既存の `NRStartPage` や汎用 prefs RPC に新機能の要求を追加せず、新しいタブと Hub が同じ `newTabClient` を使う。既存 actor 自体をすべて除去した変更ではない。actor の登録は [`BrowserGlue.sys.mts`](../../browser-features/modules/modules/BrowserGlue.sys.mts) にある。

## 2. データの正本と保存先

| データ | 正本・単位 |
| --- | --- |
| サイト・フォルダー・名前・混合順序 | Places。GUID / parentGuid / index を使用し、別のブックマーク DB は作らない |
| 表示・背景・割り当て・RSS 更新間隔・スポンサー表示 | 既存 pref `floorp.newtab.configs`。設定 schema 1 |
| Floorp Start 無効化 | 既存 pref `floorp.design.configs.uiCustomization.disableFloorpStart` |
| 選択中ワークスペース | 呼び出し元 actor の `browsingContext.topChromeWindow.workspacesFuncs` |
| WS 一覧 | `floorp.workspaces.v4.store` と `floorp.workspaces.enabled` |
| フォルダー・画面・記事選択・スクロール | 文書の hash と React 状態。WS の割り当てを自動変更しない |
| RSS 購読・本文・既読・スター・取得状態 | `<profile>/floorp/feeds.sqlite`、DB schema 2 |
| 移行元の raw 設定・GUID・進捗 | `<profile>/floorp/newtab-migration-v1.json`、journal version 1 |
| 掲載内容 | `SponsorService` の同梱1件、または pref `floorp.newtab.sponsorCatalog` |

### 設定

`floorp.newtab.configs` の `components/background/searchBar/topSites` を残し、`schemaVersion/revision/speedDial/home/rss/sponsors` を補う。旧 `components.topSites=false` は新規の `speedDial.show` と `home.showHistory` の既定値へ反映する。旧 `components.firefoxLayout` と新 `home.layout` は別の値として保ち、一律変換しない。

`SettingsService.patch(expectedRevision, changes)` は親側で直列化する。最新 raw を読み、許可した領域の指定項目だけを浅く merge して検証し、revision を増やす。未知キーは保持する。古い revision、外部 writer の変更、保存ロック、壊れた値、未知の将来 schema では成功扱いにせず、元データを初期値で上書きしない。UI は競合を通知し最新値を取り直す。

背景の画像・ファイル名・パス・選択画像・スライドショー値を保持する。フォルダー画像取得には要求側からパスを渡さず、保存済みの選択だけを使う。25MiB上限・拡張子と magic bytes の照合を行い、SVG を返さない。自分の画像は既存圧縮処理を再利用する。

Start 無効化は Hub の既存 CAS RPC を使い、外観設定の古いフォームが隣接項目を保存してもこの値を巻き戻さない。無効化は再起動後に Firefox 標準の新しいタブへ戻す機能である。

### RSS DB

Gecko `Sqlite.sys.mjs` の単一接続で読み書きを直列化し、更新・解除を transaction にする。schema 1 から2では `feeds.retry_after` を追加する。未知の版や破損 DB は削除・再作成しない。

| テーブル | 内容 |
| --- | --- |
| `feeds` | URL 一意の購読、enabled、ETag / Last-Modified、最終成功、失敗回数・次回時刻・Retry-After |
| `articles` | feed ごとの記事 ID、出典、本文 AST・抜粋、配信/初回検出時刻、既読・スター |
| `tombstones` | 整理した記事の識別・既読状態。再配信による未読復活を抑える |

別の ArticleState / Metadata テーブルや FTS は設けていない。一覧検索は title / excerpt を対象にし、既定50件・最大100件ずつ返す。配信時刻、なければ初回検出時刻を基準に安定した順序で取得し、本文は記事を開く際に読む。

非スターは既定90日・500件/フィードの両方を適用する。スターは自動整理から除外。解除は購読を無効化して通信を停止し、非スターの本文を削除、スターと出典を保持する。再購読は同じ feed ID を使う。解除済み記事の最後のスターを外すと本文を削除する。全保存データの一括削除 UI は追加していない。

DB 本体は `max_page_count` による100MiB上限、tombstone は365日・10万件上限、購読は1000件上限を持つ。WAL を含むディスク総量の厳密な上限や、無制限のスター保存を保証しない。容量不足で既存スターを自動削除しない。

## 3. Gateway の契約と権限

本番では正規の `about:newtab/about:home/about:hub` と `chrome://noraneko-newtab/content/index.html` / `index.xhtml`、対応する `noraneko-settings` の入口だけを許可する。開発・テスト mode の HTTP 例外は `localhost` / `127.0.0.1` の5183・5186、パス `/` に限定する。

子側の判定に加えて親側で毎回、現在の文書 URI、非 null principal、トップレベル文書、呼び出し元 chrome window と private を確認する。成功結果を返す前も文脈を確認する。任意の `about:`、iframe、遷移済み文書、外部 Web ページを許可しない。RSS 本文から任意 prefs 名・SQL・ファイルパスを指定できる API はない。

契約の正本は [`types.ts`](../../libs/newtab/types.ts)。実際の応答は次の形で、通知 revision と設定 revision は別である。

```ts
type NewTabResponse<T = unknown> =
  | { ok: true; value: T }
  | { ok: false; error: { code: string; message: string } };
interface NewTabChangedEvent {
  topic: "settings" | "bookmarks" | "feeds" | "workspace";
  revision: number;
}
```

| `newTabClient.call(method, params)` | 内容 |
| --- | --- |
| `snapshot`, `patchSettings` | 設定・グループ・WS・スポンサー・購読の取得、`expectedRevision` と `changes` による保存 |
| `folder`, `deleteSummary`, `bookmark`, `repairBookmarks` | 直下項目と祖先、削除件数と revision、create/update/move/reorder/delete、明示修復 |
| `feeds`, `articles`, `article` | 購読一覧、条件と offset/limit による記事一覧、本文 |
| `subscribe`, `unsubscribe`, `refresh` | URL 検証と候補選択、解除、更新ジョブ開始 |
| `articleState`, `articleImage` | 既読・スター更新、登録済み `articleId/imageId` だけの明示画像取得 |
| `dismissSponsor`, `restoreSponsors`, `saveSponsor` | 非表示・復元、`id/parentGuid/expectedTitle/expectedUrl` を照合した保存 |
| `history`, `focusUrlBar`, `openHub`, `pickFolder`, `folderImage` | 履歴候補とブラウザー固有の操作 |

クライアントは要求 ID を付ける。接続待ちは15秒、要求は通常45秒・フォルダーピッカー120秒で timeout し、自動的な変更の再送はしない。親は未完了要求を最大128件保持し、同じ ID の未完了要求を共有する。完了した変更は最大200件の ID だけを記録し、再要求に `CONFLICT` を返して状態確認を促す。画像や設定の応答本体を receipt cache に保持しない。これは profile 再起動をまたぐ exactly-once 保証ではない。

親は最初の非同期処理前に変更通知を購読する。通知は短い間隔でまとめ、UI は snapshot を再取得する。全記事本文・背景画像を broadcast しない。actor 破棄時に購読と要求記録を解放する。

private はサービス import・DB open・移行より前に拒否する。`snapshot` だけは背景・pinned・RSS・WS・広告を含まない `private:true/readOnly:true` の値を返し、他の要求は `READ_ONLY`。private ページを開いて通常プロファイルの scheduler を起動しない。

## 4. Places とワークスペース

専用 root は初回に「その他のブックマーク」配下へ作成し、直下の「共通」を既定グループにする。root/common は管理 GUID で判定し、名前が一致する任意のフォルダーを採用しない。各階層は必要な直下だけを読む。サイト・フォルダーの順序は Places の index で、未対応ノードを削除・変換しない。

親側で管理範囲・フォルダー型・祖先を確認する。循環移動を拒否し、子をコピーせず同じ GUID を移す。削除確認は子サイト/フォルダー数と revision を含み、件数取得中や確認後に変化したら再確認する。ただし外部 Places 操作と最終 remove を単一の compare-and-delete transaction にする保証はない。

Places observer は外部編集・Sync・復元も無効化通知へ変換する。現在フォルダーが消えたら UI が既知の祖先から入口までを再確認する。割当グループが消えたら共通へ戻し、共通/root もない場合は明示修復の入口を示す。

WS 一覧は profile の共有情報、現在の ID は actor のウィンドウから得る。他ウィンドウの選択を流用しない。WS 変更は `floorp.workspaces.changed` などを観測し、割当先の入口へ戻る。削除済み WS/group の古い対応は次の割当保存時に除去し、新しく失効した保存先を選んだ要求は競合にする。WS 削除で Places を削除しない。

## 5. RSS 取得と本文

`FeedService` の lazy start で profile に一つの scheduler を起動する。タブごとに更新 timer を作らない。30秒 tick で due を判定し、更新間隔は0/15/30/60分、既定15分。全体4件・開始 URL の origin ごと2件の pool と feed ID ごとの job を共有する。全体4件は各 hop を通じて維持する。異なる開始 origin が同じリダイレクト先へ合流する場合、最終 origin の同時2件までは保証しない。

ETag / Last-Modified / 304 を処理する。失敗は15分を基準に指数バックオフと jitter、最大6時間。Retry-After は秒・HTTP 日時を受け、手動更新でも尊重する。offline・復帰・設定変更で予定を調整し、終了時は AsyncShutdown で取得・検査・画像要求を abort して DB を閉じる。

[`FeedNetwork.sys.mts`](../../browser-features/modules/modules/feeds/FeedNetwork.sys.mts) は HTTP(S) URL と各リダイレクトを検証する。各 hop の URL 自身から content principal を作り、新しい Gecko channel を開く。Cookie・認証情報を省く匿名取得とし、ブラウザーの証明書・proxy 処理を使う。native redirect を止めて最大5回まで手動検証し、別 origin へ条件付き header を転送しない。20秒 deadline は全 hop の合計に適用し、feed は展開後5MiB、画像は8MiBで打ち切る。queue 待ち時間はこの20秒に含まない。

RSS 2.0 / Atom 1.0 に対応し、HTML は `rel=alternate` の候補を最大20件提示する。複数候補を自動購読しない。HTTP charset、BOM・XML 宣言・HTML meta 等から decode し、未対応の文字コードを黙って成功にしない。DOCTYPE/ENTITY/stylesheet 等を拒否し、XML は6万要素・深さ64・記事2000件、本文は2万 node・深さ64で制限する。

記事 ID は feed ID と配信側 ID、なければリンク、さらに欠落すればタイトル・配信日時・著者等の hash から作る。本文変更で ID を変えず、既読・スターを upsert で失わない。RSS は `content:encoded` → `description`、Atom は `content` → `summary` を使い、Atom の外部 `src` を本文取得に使わない。

本文を live DOM の HTML parser へ渡さず、閉じた許可 AST にして React で描画する。相対 URL / `xml:base` を解決し HTTP(S)・認証情報なしを確認、危険な要素・属性・style を除く。DB からの AST も検証する。script/CSS/font/iframe/video 等を外部取得しない。

画像は placeholder とし、画像ごとの操作で DB 内の image ID を親が取得する。PNG/JPEG/GIF/WebP/AVIF の MIME、decode 後32M pixel・各辺16384以下を検証して data URL を返す。本文・出典のリンクは有効な記事 URL、なければサイト URL を使い、両方ない場合は理由を示して無効にする。元サイトの追加全文抽出は実装しない。

## 6. スポンサー

同梱掲載は既存の Cubesoft 1件だけで、最大2件を描画する。`floorp.newtab.sponsorCatalog` の JSON 配列で掲載を指定でき、ID・名前・HTTP(S) URL・期限を検証する。不正・期限切れ・非表示の掲載は除外し、空のダミータイルを残さない。新しい remote endpoint、素材配信、表示/クリック計測は追加していない。

表示設定と dismissed ID はプロファイル共有。非表示・復元・保存を共通 queue で処理し、同時の非表示操作で選択を失わない。保存要求の `expectedTitle/expectedUrl` が最新掲載と異なれば `CONFLICT`。一致した名前・URL を閲覧中 folder の通常ブックマークとして保存し、同じ folder の同 URL は既存 GUID を返す。後の掲載変更・非表示は保存済みブックマークに作用しない。

## 7. 移行・修復・復帰

journal は旧 `floorp.newtab.configs` の raw と全 pinned、予定 GUID・元 index、root/common と各項目の `planned/created/user-changed` を持つ。原子的な JSON 書き込みを先行してから Places を更新する。pinned の GUID は元 index・名前・URL から安定して生成し、意図的な重複も保持する。URL が広告に見えることを理由に除去しない。

初期 root/common の予約 GUID を検証し、既存の同名フォルダーへ書き込まない。処理中の同時起動は同じ promise に合流する。確認できない途中状態は自動再作成せず停止して修復操作を促し、作成済み項目の利用者による移動・編集・削除を尊重する。完了後の root/common 消失は新しい空の入口だけを明示作成し、完了済みの pinned を再移行しない。

Places と prefs/journal 全体が単一 transaction になるとは保証しない。journal を使うことを、任意の外部編集下で完全な rollback ができる意味にしない。未知の設定/DB 版や壊れた元値を初期化しない。

旧 UI を選択する feature gate はない。Start 無効化は Firefox 標準への切り替え、旧 Floorp Start UI への復帰は対応コミットの revert または対応版への変更である。どちらも新しい Places・RSS DB・journal を自動削除する処理は持たない。旧 pinned と新しい階層の双方向同期や、任意の旧版での新 schema 書き込み互換は保証しない。

## 8. 検証の境界

実 Firefox の colocated harness、実 Places/DB、親 actor とページ UI を合わせて検証する。本文 fixture・mock transport・Chromium だけの成功を Gecko の権限や配布物の証拠にしない。実際に実行した環境・結果は [QA レポート](qa-report.md)、再実行手順は[実装計画書](implementation-plan.md)に記録する。

Windows/macOS のネイティブ選択、配布全工程、20タブ・1万ブックマーク・大量購読での長時間メモリ、起動・スクロール・blur の性能予算は、未実測を成功扱いにしない。採用した件数・容量制限は性能実測の代用ではない。


## 履歴候補の統合（2026-10-29の追加指定）

独立したHomeのHistorySites領域を廃止。BookmarkGridがグループ入口で既存の`history`要求を使い、登録済み項目の後へ候補を表示する。親側は既存のAboutNewTab / Activity Stream APIと既存候補のfallbackを引き継ぐ。別の履歴DBやブックマーク自動生成は追加しない。候補はGUIDを持つ保存済み項目として扱わず、通常の並べ替え・編集・削除メニューを与えない。保存操作だけが`bookmark/create`で実Placesへ書き込む。

表示の既存キー`home.showHistory`と非表示URL`topSites.blocked`を維持し、HubではSpeed Dialカテゴリへ移す。private snapshotでは候補APIを呼ばず、入れ子フォルダーにも表示しない。現在グループ直下の登録URL、表示中スポンサー、非表示URLと候補間の重複を除く。登録済みサイトの名前・順序・ワークスペース割当は変更しない。
