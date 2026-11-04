# Floorp QA #2513 実用化ロードマップ

更新日：2026-10-29 JST。担当：Floorp 開発 B。

本人の「この資料に従って、ロードマップを作成し、ゆっくりでいいのでどんどん進められる？」に基づく作業計画。
基準資料は添付 `floorp-qa-session-goals-20261026 -1-.md`（25768 bytes、SHA256
`cee4f66e37b11ba62543db3bdc66fce91ffdc49a9c748e256d98934d3abeb799`）。

## 目標と現在地

Firefox上流テスト、Floorp固有E2E、最終配布製品の検証をつなぎ、変更が既存機能を壊していないかを証拠付きで判断できる状態を作る。
土台のテスト追加は、次の製品検証を妨げる問題を解消する場合に絞る。

現在の直接確認済み基点は Floorp `155335dada61ca314c2fad369f55778188173a36`、
Runtime `8f9b7aa2f4a56d731f72c8e756014766cc7f7fec`。
[Floorp Draft #2985](https://github.com/Floorp-Projects/Floorp/pull/2985) と
[Runtime Draft #97](https://github.com/Floorp-Projects/Floorp-Runtime/pull/97) は2026-10-29 JSTの再照合でもopen・Draft・未merge、同headだった。

[Intel run 40562022078 / attempt 1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/40562022078) はfresh buildと3 smoke controlsまで成立。
登録45件のうち有効42件は完全な終了ログを確認できず、全件UNVERIFIED。上流manifest無効3件は別記する。
ログは存在を示す32467 bytesのreceiptがあるが、内容・拒否理由は公開射影から回収できていない。
artifactは0件。根本原因は未確定。製品全体のqualificationはない。

731件の隔離回帰（726成功・実Darwin専用5skip）と過去のnative preflightは、この固定sourceに結び付く既存の証拠として保存する。
以後の変更にその成功を流用せず、新しいsourceと必要な回帰を結ぶ。

## 段階と完了条件

| 段階 | 作業と成果物 | 完了条件 | QA3との対応 | 現在の状態 |
| --- | --- | --- | --- | --- |
| M0 現状の凍結と棚卸し | source・差分・担当・既存PR・最終artifactの由来を整理 | 各成果を現在確認済み／資料に基づく過去実証／未確認に分け、重複適用と取りこぼしを説明できる | 01・04・11 | 自分の2案件・両Draft・今回の終端は照合済み。旧QA3と本人PCの最新差分は未再照合 |
| M1 失敗を調査できるMac上流試験 | 不完全ログの拒否理由、harness診断、個別結果、失敗時の保存経路を復旧 | 固定45-ID inventory、有効42件の実結果と無効3件、完全なscoped log、source/SDK/run/attempt、first failure、資源・cleanupを回収。インフラ障害と製品不具合を分ける | 02・03・11 | 最優先。ローカルの診断欠落修正から再開 |
| M2 Debugと製品のsource対応 | upstream full revision、Runtime差分、順序付きpatch、共通変換、Debug差分、config/toolchainを記録 | 同じ表示versionではなく、DebugとPGO/optの共通ソースと差分を検証できる。primaryとtest supportは同じOBJDIRに由来し、入替え負例を拒否 | 01・02・08 | 資料に限定実証あり。最終製品との対応は未確認。M1と並行して静的棚卸し可能 |
| M3 まず1OSの最終製品E2E | Mac Intelを第一候補に、実配布bytesで起動・閲覧・Workspace/タブ・保存・再起動・更新を確認 | exact final artifactのhash、ビルド来歴、scenario別結果、独立oracle、失敗資料・cleanupが残る。dev-loaderや別binaryへのfallbackを拒否。配布経路ごとに判定 | 05・06・08・11 | 未達。既存Linux縦断・Workspace実証を棚卸しし、再利用できる実装を先に採用 |
| M4 継続運用と5 native target | Windows、Linux x64/ARM、Mac Intel/ARMへ同じ受入契約を展開 | 各targetで実GUI・OS input・通常起動・cleanupを実測。Mac universalは同じ最終bytesをIntelとARM双方で確認。coverage、quarantine責任者/期限、費用・p50/p95・取消復旧を運用 | 07・08・11 | 未達。WSL/POSIX成功やmatrix設定生成をnative製品合格に換算しない |
| M5 公開判断へ接続 | 最終bundle→QA→本人承認→artifact/sidecar/履歴/失効の再照合をshadowで実装 | source・Debug・製品の判定を区別し、差替え・期限切れ・承認後変更の負例でpublic mutationが0。required化は証拠と別の本人承認後 | 04・08・09 | 設計と既存実装を照合する段階。実公開は本計画の作業許可に含めない |
| M6 Agentic QAと上流coverage拡大 | 登録action＋独立oracle、固定replay、最小再現、known-bad/fixed比較をadvisoryで導入 | controllerの認証情報とworkerを分離。OSレベル資源制限と取消/cleanup、finding→再現→回帰化を実証。モデル障害をPASSにしない。suite別の分母・除外理由を管理 | 03・06・10・11 | synthetic基盤の過去記録あり。実Agent経路は未確認。最初の製品E2E成立後に進める |

主な依存は M0 → M1 → M3 → M4 → M5。M2はM1の横で調査し、M3の認定前に成立させる。
M6はM3の独立oracleを使う。保存・初回履歴・資源と費用の記録（QA3-11）は全段階で行う。

## 最初の作業単位

1. **失敗情報の欠落を補う。** `scope_receipt()` が拒否した理由と、scoped harnessの診断を安全な固定語彙で残す。不完全ログの観測情報を合否へ昇格させない。
2. **失敗時の資料を保存する。** 検証器がexit 1でも診断artifactを保存する候補を作る。raw logとSDK・認証情報を公開artifactへ混入させない。完全なprivate scoped logの回収先・利用者・保存期間はnative開始前に確定する。
3. **ローカルで異常経路を確認する。** 不完全ログ、未知event、harness例外、資料書込失敗の負例を確認。旧失敗を残し、関連回帰、全実ID catalogとsource pins、公開verifier制御を新しいsourceへ結ぶ。
4. **次のnative実行を具体化する。** 有効なSDK来歴、固定source/fixture/toolchain、実行上限、保存先、初回branch1回を準備する。前の2実行枠を再使用しない。旧期限2026-10-28T12:34:38Zと枠は終了済みなので、新しい実行範囲の指定を受けてから起動する。
5. **42件の実結果を評価する。** 原因が基盤なら最小修正。本物の不具合なら再現と回帰を残す。test本体・assertion・manifestを弱めず、必要な失敗をquarantineへ移す場合は責任者・期限・理由を記録する。
6. **製品へ戻る。** 42件を評価できるようになったら、土台の際限ない拡張を止め、M2/M3の最終artifactと主要シナリオへ進む。

## 1OS製品E2Eの初期catalog

最終的な機能名・oracle・前提は既存実装の棚卸しで確定する。以下は計画であり、実行済みのケースではない。

| シナリオ群 | 独立した確認内容 | 必要な証拠 |
| --- | --- | --- |
| インストール・起動・閲覧 | 配布package由来の実行ファイルで起動し、固定したページ内容を読める | final bytes/hash、process/binary identity、表示結果、終了 |
| Workspace・タブ | 作成・切替・移動・保持が仕様どおり | 操作前後の独立状態読取、固定replay、失敗資料 |
| 保存・再起動 | 設定とテストデータが実再起動後も一致する | 専用profile、終了確認、再起動後読取、元データ非接触 |
| 更新・移行 | 更新前後で同じ主要機能とデータが利用できる | 更新元/先artifactと経路、適用結果、移行前後の照合 |
| Fluent／Flasco | 既存の定義・対象機能・既知の不具合を先に照合し、固定ケース化 | sourceとの対応、期待値、実resource、既知不良/修正比較 |
| 資源回帰・終了 | 固定負荷でCPU/メモリ/プロセスと終了の基準を満たす | baseline、sample数、上限、cleanup。基準未確定の測定はadvisory |

## 進め方・報告・期間

一度に1つの関門を進める。独立した静的調査は並行可能だが、今回の担当Bからの再委任は行わない。
既存のWindows/WSL SDK不要作業、翻訳継承、引継ぎ一覧の完了を再実装しない。
本人PCの未公開19ファイル差分は古い資料のhashを最新と断言せず、アクセスできる正本との照合が必要になった時点で一度だけ確認する。

報告は「確認できた動作・そのsourceと証拠・未確認範囲・次の1手」を短く示す。
無変化の通知を繰り返さず、長い外部実行はOpenBotのwaitで引き継ぐ。
停止済みの旧10分routineをそのまま再開しない。自動継続の頻度と新しい外部実行枠を確定して、今回のロードマップ専用の指示にする。

M1は基盤原因に収まれば再開後2〜4作業日、M3は数週間規模、全体は月単位という従来の粗い目安を維持する。
いずれも納期保証ではない。M0の棚卸しとM1の原因判定後に積算を更新する。費用の大きい検証を同条件で繰り返さない。

main変更・merge・release・第三者送信、他担当や他routineの変更、認証情報抽出、NAS/Samba/私用バックアップ操作は行わない。
必要なローカル実行は既存の秘密情報なし・ネットワークなし隔離環境で行う。

## 今回進めた最初の作業

- 不完全ログを拒否した理由、固定語彙のevent別観測数・例外種類、scoped console logの安全な診断を残す修正候補を作成した。
- 不完全・未知event・重複JSON・途中切断・診断収集失敗の6つの回帰ケースを追加。完全な終了ログがない場合のcountsは不明、合否は未認定のまま維持する。
- 新しい全sourceを19の隔離batchで検証し、737件中732成功・従来と同じ実Darwin専用5skip・0fail/0errorを確認。旧731 IDsを全保持し、145 source hashesは全batchと無効化したworkflow候補で一致した。関連100テストはこの全体検証と重複するため合算しない。
- 公開verifierの7 synthetic controlsで、失敗時の45行・初回失敗・未確認状態と安全な診断artifactを保持し、最終stdoutと保存内容が一致することを確認した。初回候補の不一致も失敗記録として保存した。
- Runtime側の候補は `docs/qa/mac-native-firefox45-diagnostics-20261029.yml.disabled` に置いた。既存の実行済みworkflowは変更していない。新しいnative branch/origin・SDK・fixtureの凍結はまだ行っておらず、この候補は起動可能な状態ではない。
- 新しいIntel実行・push・Draft本文変更は行っていない。42件のMac実結果と完全なprivate scoped logの回収は引き続き未達である。次は回収経路を確定し、起動前の来歴と入力を揃える。

## 参照

- 本人添付資料の第3節（QA3-01〜11）、第6節（判定と停止）、第7節（再開順と全体完了条件）。
- [固定QA308レビュー](https://github.com/Floorp-Projects/Floorp/blob/c6df42ba68074ea47b70be55980b426020a2641f/docs/qa/kit-v0.3.0-review-fixes/COMPREHENSIVE_REVIEW_QA308.md)。内容は本人添付の整理に基づき、最新実装状態は別途棚卸しする。
- 今回の終端監査：`Shared/FloorpAgents/jobs/qa-2513-20261026-b/native-result-20261027/Floorp-QA2513-Intel40562022078-20261028-JA.md`。
- 保存済みnative ledger、SDK来歴、source pins、初回失敗、portable回帰は上記終端監査の参照先を保持する。

## 2026-10-29 JST 06時台の継続

- 詳細ログを限定8パスからsnapshotし、OpenSSL 3 CMS（AES-256-GCM/RSA-OAEP-SHA256）で暗号化する回収候補を実装した。公開鍵だけをsourceへ固定し、復号鍵は担当Bの私有領域で保持する。SDK、任意ファイル、環境変数を収集せず、raw tarはメモリ内だけに置く。
- 失敗時の公開診断と暗号化artifactの2経路を無効workflow候補へ組み込み、SDK取得・build前のtransport probeも追加した。新branch専用のorigin selectorは旧route・rerun・非初回pushを拒否し、元の345分／360分とclockを維持する。
- 全754 IDsを19の隔離batchで検証し、749成功・従来と同じ実Darwin専用5skip・0fail/0error。旧737 IDsを全保持し、148 source hashesは全batchで一致した。追加17件は暗号化回収13件と新origin4件。関連47件は全体検証に含まれ、別途合算しない。
- 新sourceを使う公開verifierの7 synthetic controlsでも、失敗時の45行・初回失敗・未確認状態、stdoutと保存artifactの一致を確認した。最初の暗号化test setupエラーは別記して保存した。
- 新native実行は開始していない。Mac上のOpenSSL 3の実在・version・来歴と、最新SDK metadata/取得bytes、最終fixture/head・branch不存在・run0の直前照合が残る。既存42件はUNVERIFIED・上流無効3件のままである。

回収仕様と限界は `docs/qa/qa-diagnostic-transport-2026-10-29.ja.md` を参照。

## 2026-10-29 JST 18時台の継続

- 前回候補の通常CIはsmokeと4環境jobの終端successを確認した。
- SDK12739697223を正規経路から再取得し、前後のAPI metadataとproducer、ZIP digest/size、全49701メンバーのcanonical tree、760518144 bytesのuncompressed tarの同一性を再確認した。
- Macの公式image資料だけではOpenSSL 3の実在・versionを確定できないため、長時間nativeの前に最大10分の実機依存確認と、最大20分の同一内容SDK再取得を直列で行う候補を用意した。SDKは今回22:57 JSTに期限を迎えるため、次回に備える。
- 新workflowは自分の既存handoff branchへの今回のpush、固定parent、attempt1だけを許す。実行済みworkflowとfresh45の無効候補は変更せず、新しいfresh45 branchもまだ作成しない。
- 既存の754-ID catalogと148 source hashesは不変。QA helperを変えていないため、既済の全回帰を繰り返し・加算しない。新workflowの構文・起動の負例・時間とSDK同一性条件を検証する。

仕様、参照、実機結果の採用条件は `docs/qa/qa-native-input-probe-2026-10-29.ja.md` に記載した。

## 2026-10-30 JST 00時台の継続

- 前回のMac依存13件は成功、SDK再取得はraw tar hash不一致で拒否した終端記録を保持する。旧SDKは保存期限を経過し、APIでも404。新build入力に使わない。
- SDK内容・格納順序・header metadataを分けてhashする限定診断を用意した。新しい初回pushに固定したMac26の1 job・最大20分。元identity gateは維持し、SDK候補は別名の調査資料として保存、常にNOT_ADMITTEDとする。
- 秘密情報なし隔離環境の診断器12回帰は成功。元の保存SDK全49701メンバーとraw/canonical hashesを新診断器でも一致確認し、比較baselineを固定した。変更のない754-ID/148 hashesの全回帰を繰り返し・加算しない。
- 前回doc head b2b70afの通常CI40848313173はsmokeと4環境jobの終端successを確認した。新fresh45はまだ未開始、42件はUNVERIFIEDのまま。

今回の条件と採用境界は `docs/qa/qa-sdk-archive-diagnostic-2026-10-30.ja.md` を参照。

## 2026-10-30 JST 06時台の継続

- SDK診断40898949074/attempt1を終端照合。元raw同一性ゲートでfailure、report/候補保存とowned cleanupはsuccess。通常Floorp CI40898945379の5jobsとRuntime shadow40898963223はすべてsuccess。
- API前後metadataと正規ZIP bytesを照合し、全49701メンバーのcontent tree・SDK26.5・格納順序が旧baselineと一致。全mtimeと22417件のPAX mtimeの差を検出し、独立した全raw監査で他のheader・file/link内容・paddingの差分0を確認した。runner image更新が原因という因果は未実証。
- 候補SDKのrawは前回probeと同じ32dd16b5で、元57aeb200とは不一致。旧profile/失敗は保持し、候補はNOT_ADMITTED、fresh45は未開始。次は現在の全archive identityをversion付き別sourceへ明示固定するproducer候補を準備し、success・期限・全bytesを照合してから採用を判断する。具体条件は `qa-sdk-archive-terminal-2026-10-30.ja.md` に記載した。

## 2026-10-30 JST 12時台の継続

- 現在のraw32dd16b5・全49701内容・順序・全header11 digest・Xcodeを別profileへ固定する、最大20分/初回push1回のSDK producerを準備した。外側gzip時刻4 bytesだけを0に固定し、完全な圧縮bytesも事前固定する。raw内の時刻や内容は変えない。
- 新検証器18件と起動/CLI/cleanup13 controlsは秘密情報なし隔離環境で成功。保存済み全SDKの再検査で別profileと完全一致。旧148 hashes/754 IDsと既済全回帰は不変で、再実行・加算していない。
- 前回Floorp CI40955108259の5jobsとRuntime shadow40955135690は終端success。新producerの終端とAPI/全wire ZIP照合は次に必要。loader/schema1〜3と元失敗は保持し、schema4は無効な設計候補だけ。Mac42件・製品資格は未確認。詳細は `qa-sdk-exact-profile-2026-10-30.ja.md` を参照。

## 2026-10-30 JST 18時台の継続

- 前回producer40996012481は固定32dd profileに対しraw57aとなりfailure。report保存/owned cleanup成功、SDK artifact0。report12951441466の前後API/期限/ZIP/producer/最終stdout一致を確認し、保存旧SDKの全49701メンバーから全archive identityの一致も確認した。
- 2つのrunner imageを観測したため、image/OS/Python/Xcode/SDKから先に1つの完全profileを選ぶ限定producerを追加した。未知imageや値の混在/差替えを拒否し、任意の時刻差を許容しない。旧workflow・verifier・profile・source schema1〜3は不変。
- 新selector9テストと起動/cleanup/report結合12 controlsは秘密情報なし隔離環境で成功。2つの完全offline baselineを結合し、148 helper hashes/754 IDsの不変を確認。既済回帰は再実行・加算していない。
- 前回通常CI40996000828の5jobsとRuntime shadow40996017301は終端success。新producerは最大20分/初回push1回で別artifactを作り、同run/head/attempt/jobのAPI/全bytes/全内容照合後に採用を判断する。Mac42件は未実行。具体条件は `qa-sdk-image-bound-2026-10-30.ja.md` を参照。

## 2026-10-31 JST 00時台の継続

- 前回image-bound41031469840/attempt1/head7b1d90302/job123981954108は、新しいimage20261026.0455.2を `image-not-pinned` でtar作成前に拒否。SDK0、producer report/cleanup成功。前後API/期限/ZIP全795bytes/digest/lineage/固定5入力hashを照合した。新imageのSDK内容とraw/headerはまだ未確認である。
- その観測imageへ事前固定した別の限定snapshot診断を用意した。既存selector/profile/verifier/workflowを保ち、全archive identityを保存する。取得成功でもNOT_ADMITTED。元tree/SDKが違えば候補uploadを拒否し、環境が違えばproducerに拒否理由を保存してtar前に止める。
- 前回通常Floorp CI41031453407の5jobs、docs41031453439のverify、Runtime shadow41031476952はsuccess。Floorpの148 helper hashes/754 IDsは不変で既済全回帰を再実行/加算しない。新診断の構文/起動/差替え/失敗保存/owned cleanupと完全offline baselineを含む29 controlsが秘密情報なし隔離環境で成功した。
- 新診断は最大20分/初回push1回。Mac42件は未実行、旧45ledger/失敗と旧practicalは不変。具体条件は `qa-sdk-image-snapshot-2026-10-31.ja.md` を参照。

## 2026-10-31 JST 06時台の継続

- Snapshot41078301968/attempt1/head2fe74b81/job124140325890は全step/保存/cleanupがsuccess。report12988147877とSDK診断12987718264の前後認証API/期限/全ZIP bytes/digest/lineage、producer pins/最終stdoutを照合した。
- 全49701メンバーと全gzip/raw/tree/order/metadata/11 header digestsを別読取処理でも照合。canonical treeとSDK26.5/順序は旧baselineと同じ、新rawf0d4937dを別の完全profileへ固定した。診断snapshotはNOT_ADMITTED、旧57a/32dd profileと失敗は保持する。
- 単一image20261026.0455.2と新完全profileに事前固定する別producerを追加。最大20分/初回push1回、異なる環境や全identity差は拒否、失敗reportとowned cleanupを保持。次に同run終端/全API/全bytes照合後の実artifact/source/schema4/loaderと負例・新source回帰へ接続する。
- 通常CI41078286356全5jobs/docs41078286207のverify/shadow41078313206はsuccess。148 helper hashes/754 IDs/loader/旧workflow/45ledgerは不変、既済回帰は再実行/加算なし。Mac42件は未実行。具体条件は `qa-sdk-single-image-2026-10-31.ja.md` を参照。

## 2026-10-31 JST 12時台の継続

- 単一image producer41128679337/attempt1/head032085d9/job124313694921は全step/保存/cleanup success。SDK13011079529とreport13011134290の前後API/期限/全ZIP、producer/profile/最終stdoutを照合し、全49701メンバー・全archive identityを独立readerでも一致確認した。
- 実artifact/producer/期限/新rawを別source/schema4へ明示固定。旧source1〜3・旧raw/失敗を保持し、取得前後のAPI再照合と失効/差替え拒否、profile付きreceiptを追加。正規実bytesと保存APIによる隔離fetch→receipt→全展開/tree検証が成功した。
- 新sourceの全765 IDsを20batchで760成功・同じ実Darwin専用5skip・0fail/0error。旧754 IDs全保持、全150 source hashesが一致。公開診断7制御とSDK proof5制御も成功。単独11件は全765件に含み、重複加算しない。
- 前回通常CI41128672556は4jobs成功/Linux x64ブラウザー試験exit1。詳細artifact13012153420の取得はHTTP503で原因未確定として保持。docs41128672561 verifyとRuntime shadow41128687783はsuccess。変更後CIと無効候補の最終fixtureを次に照合し、native入力凍結へ進む。
- fresh45 ref不存在/push0/POST未送信、Mac42未実行。45ledger/旧practicalは不変。実行済みworkflowは変更せず、SDK/秘密情報なしの証拠を保存する。詳細は `qa-sdk-v4-admission-2026-10-31.ja.md`。

## 2026-11-01 JST 00時台：初回Mac終端・NSSとstack-fixerの切分け

初回41187440235/attempt1/head3fae099d/job124497118586はfailure。全artifact/API/SDK/元150pinsと公開verifier最終stdoutを照合し、限定8パスの認証復号を完了した。scope91recordsは開始1件/終了0件で正式42件のcountsはunknown、45ledgerは42 UNVERIFIED+3無効。NSSのlibrary load crash(-5977)の後、取得済みfix-stacksの参照先漏れでharnessが中断していた。MOZ_FETCHES_DIRを検証済みtoolsへ固定する限定修正は、stock wrapperで旧例外を再現後、関連186件中185成功/同じDarwin専用1skip/失敗0。新767-ID catalogを全発見し旧765IDs保持、151hashesを固定。全767回帰やMacでの修正成功は未確認。次はNSSの同OBJDIR library/build/install closureと実load診断の設計。新native/manual rerunなし。詳細は `qa-native-terminal-2026-11-01.ja.md`。

## 2026-11-01 JST 06時台：NSS動的libraryの生成・配置を追跡

前回Floorp CI41213621232の全5jobsとRuntime shadow41213637548/contracts312件は終端success、全artifact/API/bytes/logsを照合。固定コンパイルsource13ファイルと適用30patchを読み、NSS→softokn3→freebl3の実行時load経路と、現在のQAが両DSOの存在・配置を検証しない点を確認した。直接失敗したlibraryは未確定。

固定generated graphと同OBJDIRのdist/bin・app内6libraryだけをFD同一性/上限付きで読む準備候補を追加。欠落・差替え・不正Mach-O・symlink/hardlink/FIFO拒否等の新15件＋既存target12件は隔離環境で全27成功。旧767IDs全保持、新782-ID catalog/153hashes。全782回帰とMac実loadは未実行。観測器はpipeline未接続で、今回は新native/manual rerunなし。次は失敗時保存・限定DSO targetと実NSS load probeを結合し、入力と必要回帰を凍結する。42UNVERIFIED/3無効と旧first failure/practicalは保持。詳細は `qa-nss-dynamic-closure-2026-11-01.ja.md`。

## 2026-11-01 JST 12時台：NSS観測を失敗時のsummaryへ接続

前回CI41242323807の全5jobs、shadow41242338846/contracts312件は終端success。artifact/API/完全ZIP/logsとSDK最新API・保存正規全ZIPを照合した。NSS観測を同OBJDIRのtarget build前後・child resources再配置後・終了時へ接続し、fixed config/graph/6libraryをFD同一性/総量64MiB/30秒と既存期限の残りへ結んだ。collector例外やcleanup未確認で元first failure・ledgerを上書きしない。

新sourceの全795 IDsを照合し、790成功・従来と同じ実Darwin専用5skip・0fail/0error。旧782 IDs全保持、全153source hashesは各実行と一致。最終batchの120秒driver timeoutは保存し、成功済み23件を再利用して未完了12件だけ4件ずつ分割し全成功した。795件は重複加算しない。設定FD/失敗時保存など新13件はこの全件に含む。公開診断7制御と、既存8パス暗号化回収で新summary field/first failureがbyte完全一致して復号される追加1制御も成功。Mac実観測・実loadは未実行。

新nativeは未開始。次は固定generated graph宣言でguardする限定DSO build候補と、同xpcshellのNSS実load/実Mac fix-stacks probeを準備し、新入力を凍結する。Mac42件・製品資格は未認定のまま。詳細は `qa-nss-evidence-integration-2026-11-01.ja.md`。

## 2026-11-01 JST 18時台：限定NSS buildと実probeを準備

前回Floorp CI41268271182/attempt1/headc0e5f303は全5jobs終端success。API・全artifact/ZIP・logsとSDK最新APIを照合した。前回shadow41268280073/contracts312の監査は同じbytesの証拠を再利用する。

opt-inのfresh45経路へ、同OBJDIRのsoftoken/freebl targetを実graph宣言/完全hashでguardするbuildと、同xpcshellのNSS internal-slot乱数生成/Rust SHA-256既知解probe、実stock wrapper/stack-fixerの短時間probeを接続した。固定sourceの確認でSHA-256単独はRust実装と判明したため、NSSConstructor→EnsureNSSInitialized→PK11_GetInternalSlot/GenerateRandomOnSlotへ結ぶ。既存42件の期待は不変で、probeは別件として扱う。default8パス回収を保ち、明示flagのみ固定3probe logsを追加。summary最終proofも追加receiptの欠落を拒否する。

実Macでのbuild/load/stack-fixer動作はまだ未確認。次は新しい未使用origin/branch、fixture/head/pins/catalog、SDKの直前照合、workflowと公開verifier・固有証拠を凍結し、必要な初回nativeへ進む。42 UNVERIFIED+3無効/counts unknown、旧first failure/practical/ledgerと既存workflowを保持する。詳細は `qa-nss-runtime-probes-2026-11-01.ja.md`。

検証結果：全816 exact IDsの811成功・従来と同じ実Darwin専用5skip・0fail/0error。旧795 IDs全保持、全157source hashesが41batchで一致。関連62件と新21件は全816件に含む。公開診断7制御と追加probe logs/summary/first failureの認証暗号化・byte完全一致復号1制御も成功。初回公開制御のfresh45 helper pin不一致は拒否として保存し、隔離verifier候補の厳密157-file listとpipeline/fresh45 pinだけを新bytesへ固定した。実行済みworkflowのverifierは不変。

## 2026-11-02 JST 00時台：NSS新origin・workflowの入力を結合

前回Floorp CI41293611976の全5jobsとartifact13078881939/API/全ZIP/logsを終端照合しました。新branch b/qa-fresh45-nss-20261102の初回だけを許すoriginと、対応する暗号化回収originを明示flagで追加。pipelineのNSS opt-inと固定11パス回収、新workflowの独立公開NSS predicateを結びました。旧origin/workflow・scope期待・345/360分は保持します。

関連58テスト全成功、新824 exact IDs全発見/旧816保持、157hashes固定。変更5ファイル以外の152hashesは不変です。旧全816回帰は過去証拠として保持し、不変766件を再実行・合算しません。新nativeでは全824件/skip0を要求します。独立NSS正例1＋26負例/秘匿制御、公開射影7制御、起動正例1＋10負例と全bash/埋込Python構文・flag結合は成功しました。

次は専用handoffの固定fixture通常CIとRuntime shadowを終端照合し、branch不存在/push0・SDK/API/期限/bytes・担当/両Draft・固有意図を直前確認して必要な初回nativeへ進めます。実Macのbuild/load/toolと42件は未判定、元45ledger/first failure/cleanup限定/旧practicalを保持。詳細は `qa-nss-input-freeze-2026-11-02.ja.md`。

## 2026-11-02 JST 12時台：NSS初回のpreflight終端と失敗ログ回収

Native41350068009/attempt1/head4b365e79/job124978125641はSDK取得前のcheckpoint-tests/nonzero-exitでfailure。全API/2artifactの全ZIP/認証復号/最終stdout保存一致と157pinsを確認した。固定11パス中2件回収、テスト詳細ログが旧allowlist外のため個別失敗・完了数・cleanupは未確定。今回の公開45ledgerはinventory未確認の45UNVERIFIEDで、旧42UNVERIFIED+3無効と旧firstfailure/practicalは保持する。NSS build/load/tool・上流42件は未実行。

失敗時と同じfixtureab661c0a/824IDs/157pinsを用い、専用handoffの固定parentから1回だけ起動する別のpreflight証拠診断を用意した。20分job/15分work/10分preflight、SDK/本体buildなし、固定10パスを暗号化回収する。新回収器/起動/暗号認証等の隔離12件は成功。既済Linux全回帰は再実行・合算しない。終端の個別失敗を読んでから必要修正とNSS実機再開条件を判断する。詳細は `qa-preflight-evidence-2026-11-02.ja.md`。

## 2026-11-02 JST 18時台：診断器の時間契約を修正

前回の短時間診断41382285951/attempt1/head7c7d52d3/job125075486438の全API/完全ZIP/CMS認証復号を照合。driverの `platform-budget-binding` は診断器がplan360分を15分へ置換し、ABIの360分呼出しとの一致検査でspawn前に拒否されたことによる。824件/14controlsは実行0、元NSS41350068009の個別失敗は引き続き不明。SDK/NSS/42件未実行、旧firstfailure/practical/45ledgerを保持する。

実行済みworkflow/scriptを保ち、元plan360分の引数契約とAPI開始基準のphase15分を併存させる別診断を追加。job20分/step10分/同fixtureab661c0a/824IDs/157pins/固定10パス暗号化回収、専用handoff固定parent7c7d52d3から初回push1回に限定。旧不整合の実Runner再現、compile境界到達、15分超過時spawn拒否、firstfailure保存を含む隔離16件が全成功。実Macのcompile以降は未検証で、次は新診断の同run/head/attempt/jobを照合し、元の個別failureを回収する。詳細は `qa-preflight-budget-2026-11-02.ja.md`。

## 2026-11-03 JST 00時台：個別3errorを回収し入力祖先の束縛を修正

修正版診断41418651939/attempt1/headd35a4f53/job125188586689の全API/完全ZIP/CMS8ファイル/157pins/824 exact IDsを照合。824件は821成功・3error・skip/failure0で完走し、3件ともstock stack wrapper後のdirectory-bindingだった。firstfailure checkpoint-tests/nonzero-exit/566.68秒と保存summaryは一致。消費3stageのleader reap/group消滅を確認、session/全子孫・全task cleanupは未認定。後続14制御・NSS/42件未実行、元NSS41350068009の個別失敗は未回収のまま保持する。

親へ出力entryを追加すると同じ失敗が再現したため、ancestorのdev/inode/owner/modeとFD/名前束縛を維持しつつnlinkを可変metadataへ移した。読取root/内側directory/fileの全属性、link/FIFO拒否は維持。置換・保護root変更等を含む関連105 unique隔離テスト成功、旧824IDs全保持の830catalog/157pins。全830回帰・不変725件は再実行しない。Macの実変化属性と修正成否は未確認なので、別の最大20分診断で固定105件＋未到達14制御を先に検証する。前回通常CI全5jobs/shadow312は終端照合success。SDK/API/期限/保存全ZIP同一、旧失敗/45ledger/practical/実行済みworkflowを保持。詳細は `qa-stack-binding-2026-11-03.ja.md`。

## 2026-11-03 JST 06時台：Mac束縛修正を確認しNSS実機再開を準備

限定Mac41470665560/attempt1/head8068a6435/job125363063696のAPI/完全ZIP/CMS9ファイル/157pins/105 exact IDsを終端照合。関連105件と14記録制御は全成功・skip0、以前の3errorも成功した。制御内の親nlink4→5とsize/mtime/ctime変化、inode/owner/modeおよび保護tool全属性の不変を実Macで観測。元NSS41350068009の未回収個別失敗は不明のまま保持する。5stageのleader reap/owned group消滅を確認、全task/session子孫cleanupは未認定。SDK/NSS/42未実行、旧45ledger/firstfailure/practicalは保持。

同head通常CI全5jobsとshadow312件は全API/artifact/ZIP/log監査success。SDK13011079529は最新API/保存全ZIP同一・期限11/13、同bytesの49701-member監査を再利用する。新しい初回専用NSS-binding origin/branchを追加し、新回収経路だけ固定preflight5パスを追加して全16パス暗号化。work345/job360・3秒query/3samples・30秒profile・FD/source identity・owned cleanupと元の上流期待を維持する。新sourceの全839 IDs発見/旧830保持、関連67件は隔離全成功・skip0、5変更以外152hash不変。全839/変更なし772件は再実行・加算しない。新workflow/公開verifier/固有入力と同head CIを凍結後、別native初回1回へ進む。詳細は `qa-nss-binding-resume-2026-11-03.ja.md`。

## 2026-11-04 JST：Mac終端と検証器の順序候補

Mac run 41630387790/attempt1/head e3689d0a/job125898143523 は build/test success 後の独立検証器で failure。認証済みログと45行台帳の照合では有効42 PASS・manifest無効3 SKIP。839事前検査・14記録制御、限定NSS/stack probeも成立したが、製品認定ではなく、task/session全子孫cleanupは未認定。

検証器は inventory を7 target build後と要求するが、実pipelineは helpers install後・target build前に実行する。消費済みworkflowを保持した参照差分で、inventoryを前に含む正確な順序へ修正する候補を作成。認証済みsummaryと157 source pinsを隔離再評価し、元 target proof false・候補 true、ほかの target graph/assets/smoke・upstream・全stage・summary最終判定条件 true を確認。順序・欠落・同一性・cleanup等の14負例とsummary最終判定の6負例は拒否、公開診断7合成失敗制御も成功した。これは元workflowを成功に書き換えず、実workflow全体や現在のSDK有効期限を再検証したものではない。

初回拡張検査は定数読取方法によるセットアップerrorで中止し、修正後に正常完了。対象helperは不変のため839件は再実行・加算しない。次は候補を消費済みworkflowではなく新workflowへ接続し、origin・SDK・fixture・期限・pins・負例を凍結してから、必要な別native初回を判断する。
