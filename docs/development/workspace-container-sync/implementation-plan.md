# ワークスペース同期のコンテナ対応 — 実装計画書

作成日: 2026-10-28（日本時間）

状態: 製品コード・Runtime patch・production 成果物を統合済み。
最新の検証結果と未実施範囲は [QA 記録](qa-report.md) を参照。

関連文書: [仕様書](specification.md) / [設計書](design.md)

## 1. 到達目標と前提

仕様書 FR-01〜FR-14 と AC-01〜AC-16 を満たす Desktop 向け機能を実装する。 P0 の
native 移行・削除フックの先行実証を反映し、残る統合・受け入れを進める。 Cookie
転送、サイト割り当てルールの同期、モバイル対応は今回の実装に含めない。

実装ベースは Floorp `c122afebbdbb4627a3d342c5b8d969686024c7e9`、 Runtime
`0ac255739cdd64e100cd6913329247421b0ec2ea`（Gecko 159）。 分離した作業 checkout
と完全な Linux Floorp アプリで作業している。
この文書は実装順序と残る公開条件を管理し、下のテスト行列を一括合格とは扱わない。

## 2. 分割と依存関係

| 段階                      | 主な成果物                                                           | 完了条件                                                                      |
| ------------------------- | -------------------------------------------------------------------- | ----------------------------------------------------------------------------- |
| P0 成立条件の実証         | 削除経路の一覧・実行順記録、移行 POC、タブ置換 POC、設計判断の更新   | 削除を他端末へ送らず、旧版による誤展開を隔離でき、native の提供方法が確定する |
| P1 同期モデル             | コンテナ定義・参照・wire 検証・マージ・未確定状態                    | 情報欠落を通常コンテナと解釈せず、再送・順序逆転・競合を決定的に処理できる    |
| P2 関連付けサービス       | account/profile 対応表、実体レジストリ、同名解決、作成ジャーナル     | 複数ウィンドウ・再起動でも重複作成せず、名前変更・削除を識別できる            |
| P3 受信と観測             | 解決後のタブ生成、既定設定、永続保留、capture の観測分類             | 最初のネットワーク要求から正しいコンテナを使い、未展開タブを削除しない        |
| P4 削除と明示適用         | 事前保護フック、復旧、既存タブの置換・再開処理                       | 削除由来の閉鎖だけを抑止し、通常閉鎖・remote 削除を維持する                   |
| P5 UI と翻訳              | 状態表示、初回確認、候補選択、削除復旧、明示適用、英日文言           | キーボードで操作でき、背景受信で確認が重複・連発しない                        |
| P6 移行・回帰・パッケージ | 新 engine 登録、移行状態機械、モデル／native 回帰、production 成果物 | 部分移行から復旧でき、旧新版の相互汚染がなく、配布物に変更が入る              |
| P7 二端末の受け入れ       | 実認証 Sync、異 OS、オフライン・再起動・旧版混在の記録               | 必須 AC と公開条件を満たし、結果を同じ成果物のハッシュへ結び付けられる        |

P0 → P1 → P2 → P3 を基本順序とする。P4 の native フックは P0 後に P2/P3
と並行できる。 P5 の文言・UI は状態モデル確定後に並行できる。P6/P7 は P1〜P5
の統合後に行う。 PR の提示・マージ・QA
は今回の依頼に含まれる。最終結果は成果物と証拠を添えて追記する。

## 3. P0 で先に解決する事項

### 3.1 削除・無効化の捕捉

- 設定 UI、拡張の
  `contextualIdentities.remove`、コンテナ機能全体の無効化を実ブラウザーで観測する。
- 「コンテナを削除」と「コンテナ内のタブをまとめて閉じる」を区別する。
- TabClose より前に、アカウント・実体・同期 tabId を記録できる共通点を決める。
- 永続化失敗・キャンセル・中断で、不完全な保護のまま削除を進めない経路を検証する。
- 起動時の実体照合、数値 ID の再利用、無効化後の再有効化も確認する。
- Runtime 変更は Floorp 側の patch で保持し、ロック先に対して適用を検証する。

成果物は「正式な操作経路 → フック → 最初の閉鎖 → 保存 →
復旧」の一覧と再現ケースとする。
削除通知だけを監視して合格としない。成立しない場合は FR-10
を削らず設計を修正する。

### 3.2 互換性と移行

専用 `floorp-workspaces-v2`（engine 1 / payload 2）と暗号化制御コレクション
`floorp-workspaces-migration` を実装した。native Collection / CryptoWrapper /
PostQueue と Service.uploadMetaGlobal を使う先行検証で、ページ取得・metadata
保全・CAS 412・再開を確認した。 次の項目は最終統合にも引き継ぐ。

- 新 engine の登録・暗号化・既存アカウントの有効化・再起動後の再調停。
- v1 を保全した一方向移行と、カットオフ後の v1 読書き停止。
- 未選択のワークスペースも含む wire
  の移行範囲と、各プロフィールの保留・選択・削除保護・未送信変更の移行。
  移行元の selected/excluded や関連付けを他端末へ配布しないこと。
- v1
  に存在しないコンテナ情報を推測せず、観測可能な所属と未確定状態を分ける方法。
- 移行途中のクラッシュ、部分アップロード、複数端末の同時開始、完了記録の競合。
- 移行後の旧版起動・進行中の v1 書き込みが v2 を変更しないこと。
- サインアウト・別アカウント・再サインイン時の移行権威の読み直し。

全アクティブ端末の更新を推奨する導入フロー、旧端末の変更の明示回収、
ダウングレード時に自動逆変換しない復旧方針を文書化する。 単純な engine version
引き上げ、dual-write、継続ブリッジで代替しない。
移行の確定契約は[設計書 9 章](design.md#9-互換性と移行)に記載する。
完了後の後続端末の v1 遅延編集は再投入せず、旧ファイルを保全する。
所有元ジャーナル紛失時は同期を停止し、所有権移管・旧版変更回収の UI
は未実装である。

### 3.3 作成と既存タブの置換

- native コンテナの作成とローカル関連付けの保存をまたぐクラッシュを再現する。
- 同名の無関係な実体を誤認せず、重複作成を防げるジャーナル／実体識別方法を決める。
- 既存タブのコンテナは DOM 属性だけで変更せず、正しい browsing context
  で置換する。
- 置換前後で一つの論理 tabId だけが capture の所有者となり、閉鎖 tombstone
  を生成しないことを示す。
- ロード失敗、アカウント切替、ウィンドウ終了の途中でも元タブを失わないことを確認する。

## 4. 実装項目と変更箇所

| 段階 | 既存箇所／追加候補                                                                                                                           | 実装する内容                                                          |
| ---- | -------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| P1   | `browser-features/modules/common/workspace-sync-types.ts`、`workspace-sync.ts`、`workspace-sync-record.ts`、`workspace-sync-record-types.ts` | 定義・参照、strict parser、size/Unicode 制約、merge、移行専用未確定値 |
| P2   | `browser-features/modules/modules/workspaces/` の新しい binding service                                                                      | 共通排他、同名候補、作成・実体世代・保存、account epoch               |
| P3   | `FloorpWorkspacesSync.sys.mts`、`workspacesSync.ts`、`workspacesSyncTypes.ts`                                                                | 保留の永続化、受信 ACK と UI の分離、所属観測、削除推論の抑止         |
| P4   | native common patch、`workspacesService.ts`、同期 adapter                                                                                    | 削除開始の保護、復旧、明示置換、起動時照合                            |
| P5   | workspace modal/context menu/toolbar/sidebar と `i18n/en-US/`、`i18n/ja-JP/`                                                                 | 事前確認、選択、状態・原因、復旧と再適用                              |
| P6   | engine 登録、production/development patch、同期 ledger                                                                                       | v2 起動、アカウント単位移行、再実行・rollback、設定再調停の制御       |

新規ファイルは
`WorkspaceContainerBindings.sys.mts`、`WorkspaceContainerMigration.sys.mts`、
共通の container / bindings / migration モデルと試験を追加した。 Runtime
lifecycle patch は production 用と開発用の両方に保持する。 private window
や内部ページ等の既存除外は維持し、一般の同期動作の変更を最小限にする。

## 5. 必須テスト行列

| 群               | 主なケース                                                                                                 | 合格の観測                                                  | 対応 AC    |
| ---------------- | ---------------------------------------------------------------------------------------------------------- | ----------------------------------------------------------- | ---------- |
| T01 関連付け     | 同名 0/1/複数、端末で異なる数値 ID、完全一致しない名前、ローカル名称変更                                   | 初回作成／確認／選択が仕様どおり、以後は同じ実体            | 01〜04     |
| T02 共有・重複   | 複数ワークスペース、複数ウィンドウ、再受信、同時作成、作成後保存前クラッシュ                               | 二重生成せず、対応と実体が一致                              | 03、04、13 |
| T03 所属         | workspace 既定、個別指定、明示的 none、既定変更、表示中とは別 workspace の受信                             | 最初の HTTP 要求の origin attributes が目的コンテナ         | 01、05、06 |
| T04 保留         | 未受信定義、未確定 legacy の既存／新規／none 選択と復旧、確認待ち、作成失敗、再起動、保留中の remote 変更  | 誤ロード・偽の削除なし。復旧時は最新希望状態のみ展開        | 02、11、13 |
| T05 削除         | 設定 UI、拡張 API、一括無効化、ロード中、二ウィンドウ、オフライン後再起動                                  | 保護 tabId の outgoing tombstone がなく、他端末が維持される | 08、09     |
| T06 閉鎖の区別   | 削除と同時の無関係な通常閉鎖、remote tombstone、タブのみ一括閉鎖                                           | 正当な閉鎖は伝わり、削除由来の閉鎖だけ抑止                  | 09〜11     |
| T07 復旧         | 既存へ再関連付け、明示再作成、同期解除、ID 再利用、同名実体を後から作成                                    | 削除後の自動復活なし。共有実体や Cookie を消さない          | 08、11、12 |
| T08 明示適用     | 論理参照変更、同一 UUID の関連付け先実体変更、削除後の残存タブ、成功・失敗・再起動、pin/order、重複論理 ID | 元タブの保持または成功置換。不要な tombstone なし           | 06、13     |
| T09 アカウント   | サインアウト、別アカウント、遅れた確認・作成、同期 off/on、機能 off/on                                     | 旧 epoch の作用がなく、再開後の関連付けが正しい             | 14         |
| T10 ログイン状態 | 二プロフィールの合成サイトに異なる Cookie、既存再利用と新規作成                                            | 受信側 Cookie を使用。wire に Cookie/認証情報なし           | 07         |
| T11 互換・移行   | 旧→新、新→旧、同時移行、部分成功、遅れた v1 書込、ダウングレード                                           | データ保全、v2 隔離、所属未確定を通常コンテナへ変換しない   | 15         |
| T12 UI           | 英日、長い名称、名前非表示、キーボード、画面読み上げ、背景受信、キャンセル                                 | 理由・対象・復旧が分かり、ウィンドウごとの確認重複なし      | 16         |

モデル試験だけで T03/T05/T08 を合格にしない。実際の gBrowser、SessionStore、
ContextualIdentityService、削除 UI/API、native Sync の出力を観測する。
ユーザーの Cookie を試験データに使わず、専用プロフィールの合成サイトで検証する。
重要ケースには修正前または保護を外した比較を置き、意図した問題を検出できることを確かめる。

## 6. 検証コマンドと実行環境

以下は参照版の
[workspace-sync README](https://github.com/Floorp-Projects/Floorp/blob/c122afebbdbb4627a3d342c5b8d969686024c7e9/tools/workspace-sync/README.md)
およびタスク定義に基づく検証コマンドである。実施済みと未実施は次節の記録で区別する。
新機能を含む checkout と対応する完全な Floorp アプリを使い、bare Runtime
で代用しない。

```sh
deno task test --near browser-features/modules/test/workspaces
deno task test --near browser-features/chrome/common/workspaces --layer chrome
deno task test:host
deno task test:smoke
NODE_ENV=production deno task feles-build build --phase before-mach
deno run -A tools/workspace-sync/verify-browser.ts --binary /absolute/path/to/floorp
deno run -A tools/workspace-sync/verify-container-migration.ts --binary /absolute/path/to/floorp
```

必要に応じて追加したテストディレクトリも対象にする。 変更ファイルの
format/lint/typecheck は所属 workspace に応じて実行し、 smoke
がすべての変更を型検査するとは仮定しない。 既存の native verifier は隔離した
workspace adapter を一部に使うため、 production UI
の経路と、認証済み二クライアントの通信を別途検証する。 README
の過去のケース数や成功記録を、今回の新機能の試験結果として引用しない。

### 6.1 実施済みの検証記録

2026-10-28、上記 Runtime に対応する完全な Linux Floorp
アプリと新規プロフィールで実施した。 以下の `_dist/`
は作業環境内の証拠であり、チェックイン対象の製品データではない。

| 検証             | 結果                                                                                                                                                                                                                                   | 証拠                                                                     |
| ---------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------ |
| native 移行      | 9 ケース合格。実 CryptoWrapper の暗号化、複数ページ取得、tombstone、native meta 条件付き保存、正確な ACK とサーバー照合、再起動相当の journal 再読込、別所有者・紛失の停止、CAS 412、空アカウント、不整合 v2、保存失敗後の再試行を確認 | `_dist/workspace-migration-native/run-cd6fa9bef1627875/results.json`     |
| 移行共通モデル   | 上記 native ブラウザー内で 8 ケース合格。安定 ID、未確定参照、ローカル既知所属、未送信追加、削除、孤立 tab、選択非転送、厳密な ACK・ID 検証                                                                                            | 同じ report と `workspaceContainerMigration.test.mts`                    |
| native lifecycle | 6 ケース合格。未準備 remove guard、永続 blocker 待機、閉鎖前準備、保存失敗の中止、キャンセルトークン再利用拒否、一括無効化の対象 ID 通知                                                                                               | `_dist/container-sync-qa/native-lifecycle-936ec39ccc17df5b/results.json` |
| TypeScript       | engine・migration module・migration tests・新 native migration driver の対象 check 合格                                                                                                                                                | 各所属 Deno config による対象 check                                      |

移行用 loopback Sync server は試験データだけを扱い、ブラウザー側は実 native
transport / crypto を使う。上表は移行・削除フックの先行検証である。 最終
production 成果物の native 統合・Chrome UI・実アカウント二クライアントの結果は
[QA 記録](qa-report.md) に記載する。 物理的な異 OS
間の往復、旧版アプリとの全操作行列、配布用インストーラーの受入は未実施であり、
上の部分合格を代用しない。

### 6.2 認証済み二クライアント

QA app preparation は Linux と macOS
を扱えるよう拡張した。専用アカウントと新規プロフィールを使う。次の driver に
同名関連付け・新規作成・明示再オープン・Cookie
分離・削除保護・明示復旧のケースを追加した。

```sh
python3 tools/workspace-sync/prepare-acceptance-app.py \
  --source-app /Applications/Floorp.app \
  --build-root /absolute/path/to/matching/build-checkout \
  --output-root /absolute/path/to/new/qa-directory
python3 tools/workspace-sync/launch-acceptance-apps.py \
  --setup /absolute/path/to/new/qa-directory/setup.json
deno run -A tools/workspace-sync/verify-live-two-client.ts \
  --setup /absolute/path/to/new/qa-directory/setup.json --phase preflight
```

専用の QA アカウントで両方のブラウザーにサインインした後に実行する。

```sh
deno run -A tools/workspace-sync/verify-live-two-client.ts \
  --setup /absolute/path/to/new/qa-directory/setup.json --phase status
deno run -A tools/workspace-sync/verify-live-two-client.ts \
  --setup /absolute/path/to/new/qa-directory/setup.json --phase acceptance
```

macOS 上の二プロセス合格だけで異 OS の確認済みとはしない。 Windows/macOS/Linux
の実ブラウザー回帰を確認し、少なくとも異 OS 二端末で関連付け・
削除保護・再起動復旧を往復確認する。実施できなかった組み合わせは結果に明記する。

## 7. 公開と障害時の扱い

1. P0 の実証結果を反映して、同期形式・移行方式・削除フックの設計を確定する。
2. 機能は実験設定下で統合し、T01〜T12 と既存同期回帰を満たす。
3. production の loader、ESM、pref、native patch、engine
   登録が成果物に含まれることを確認する。
4. 最終成果物で認証済み二端末と旧版混在を検証する。
5. 一般提供時には、初回ログインの必要性と旧版との同期境界を利用者向けに説明する。

機能停止時は対応・希望状態・保護ジャーナルを保存し、誤ったコンテナでの新規展開を止める。
既存のローカルタブ・コンテナ・Cookie は停止の副作用として削除しない。 v2
移行後に機能を停止しても、v1 の read/write を自動再開しない。
復旧は対応版で行う。戻し方の未検証なバージョンへ配布を切り戻しただけで復旧完了とは扱わない。

## 8. 完了の定義

- FR-01〜FR-14、AC-01〜AC-16
  の各条件に検証結果または明示した制限が対応している。
- P0
  の未実証事項が解消し、移行・削除・タブ置換の公開を妨げる不明点が残っていない。
- 必須ケースに不合格・未実施があれば一般提供を止め、完了扱いにしない。
- 変更コミット、Runtime、ビルド、パッケージの識別情報と試験結果を結び付けて保存している。
- ドキュメントは最終実装の形式・UI・互換性に合わせて更新されている。

実装・Linux での検証記録と、一般提供前に残る異
OS・旧版混在・配布物の確認を区別する。 復旧 UI の制限も [QA 記録](qa-report.md)
に明示する。 この PR の統合を、上記すべての配布条件の完了とは扱わない。
