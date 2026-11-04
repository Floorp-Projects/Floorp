# M1：NSS診断を同OBJDIRの失敗時資料へ接続（2026-11-01 JST 12時台）

`pipeline.py` の上流XPCOM経路にNSS観測器を接続した。target build前・完了後・stock child libsとresourcesの再配置後・終了時に、`summary.json` の `nssDynamicClosure` に観測を残す。build、asset検査、harnessの失敗でも終了時の観測を試み、元のfirst failureを変更しない。実Macでの観測・NSS読込み成功はまだ未確認。

## 保存と判定の境界

`effective-config.private.log` の `substs.MOZ_MACBUNDLE_NAME` だけからbundle名を選ぶ。設定fileは2MiB以内、owned regular inode、link拒否、FD/pathと最終再照合を適用し、重複JSON keyや不正bundle名を拒否する。設定とlibraryを同一のFD reader/総量64MiBのbudgetへ結び、raw設定・library bytes・絶対pathを出力しない。設定hashと、既存の固定2graph/6libraryのreceiptだけを保存する。

各観測のbudgetは30秒と既存work期限の残りの小さい方。期限を過ぎた時は `work-deadline`、process groupの終了が未確認なら `process-cleanup-unconfirmed` としてfileを読まない。観測器の例外は固定語彙 `collector-exception` に落とし、例外本文・pathを残さない。既存の345分work/360分jobとclock起点は維持する。

観測はadvisory。欠落や異なるhashは調査資料であり、libraryの存在/header/UUID一致を実load成功や42件の合否へ変換しない。`realLoadPerformed/buildPerformed/productQualification/native42ResultQualification` はfalse。build targets、上流tests/assertions/manifest/plan、3秒query/3samples、30秒profile、source/FD identity、owned cleanupは今回変更していない。

新しいfieldは既存 `summary.json` に入るため、既存の固定8パスの暗号化回収で保存できる。公開verifierは自由な診断内容を公開projectionへ写さず、raw summaryを公開しない。collectorの収集先は拡大していない。

## 検証

新sourceの全795 IDsを照合し、790成功・従来と同じ実Darwin専用5skip・0fail/0error。旧782 IDs全保持、全153source hashesは各実行と一致。最終batchの120秒driver timeoutは保存し、成功済み23件を再利用して未完了12件だけ4件ずつ分割し全成功した。795件は重複加算しない。設定FD/失敗時保存など新13件はこの全件に含む。公開診断7制御と、既存8パス暗号化回収で新summary field/first failureがbyte完全一致して復号される追加1制御も成功。Mac実観測・実loadは未実行。

設定差替え、symlink、不正JSON・bundle、読取期限、失敗時の実FD観測と保存stdout一致、collector例外とfirst failure/未実行ledgerの独立性、未cleanup groupと期限到達の負例を確認する。合成試験はNSSやMac上流42件を実行した証拠にはしない。

公開診断の初回ローカル制御は、実行済みverifierが元150source固定のため新153sourceを `source-pin` で拒否した。この失敗は保存。実行済みworkflowは不変とし、隔離した次回候補のsource名集合153個とpipelineの厳密hashだけを更新して7制御を確認した。判定式は変更していない。この候補はまだworkflowへ組み込んでいない。

## 既存結果と次の実機条件

Floorp CI41242323807/attempt1/head60936aabの全5jobsとRuntime shadow41242338846/attempt1/headfaab8611/job124658620651は終端success。後者は312 contracts/exit0/preflight PASS、native UNVERIFIED、publicationAuthorized false。artifact13057713030/13057957400の前後API・完全ZIP digest/size/lineageとlogsを照合した。

SDK13011079529は最新APIと保存した正規取得ZIP105157519bytes/hash8d1fc67bが一致。期限2026-11-13T21:08:48Z、producer41128679337/attempt1/head032085d9/job124313694921。全49701memberの既存独立監査は同じbytesへの根拠として再利用する。今回新SDK取得・producerは開始していない。

元native41187440235は消費済みfailure、42 UNVERIFIED+3 MANIFEST_DISABLED、正式executed/passed unknown。first failure `mozilla-upstream-proof-incomplete` と59 build-stage/failed harnessのgroup epoch終了、全session/native cleanup未認定を維持。旧practicalContinuationと旧45ledgerも不変。

次は固定generated graph内のsoftoken/freebl target宣言を厳密に検査してから限定DSO buildを許す候補と、同xpcshellによるNSS実load・実Mac fix-stacks probeを準備する。今は実graph宣言も直接失敗libraryも未観測。build target追加・実load・新nativeは今回行っていない。入力、固有branch/origin、fixture/head、全pins/catalog、SDK期限、345/360分と失敗時保存を凍結してから次の必要な初回実行を1回ずつ進める。実行済みworkflow・旧prototypeを再利用して起動しない。
