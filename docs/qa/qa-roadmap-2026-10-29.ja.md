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
