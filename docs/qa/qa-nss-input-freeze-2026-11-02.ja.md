# M1：NSS修正検証の初回入力（2026-11-02 JST）

次の実機検証は、消費済みrun41187440235のNSS初期化失敗に対する限定DSO buildとstock stack-fixer参照修正を確認します。旧実行の42 UNVERIFIED＋上流無効3件、executed/passed unknown、first failureとcleanupの限界を保持します。

## 実行と回収の結合

新しいoriginは `b/qa-fresh45-nss-20261102` 専用です。`fresh_job_origin.py --nss-20261102` は作成push・before全0・attempt1と、認証APIの同run/head/branch・唯一の `fresh-firefox45` job・macos-15-intelを照合します。既存originの選択は維持し、旧branch、rerun、dispatch、selector混在を拒否します。job開始時刻とmonotonic originを一度だけ保存し、345分work／360分jobを維持します。

回収器にも `--nss-origin-20261102` を追加しました。旧originのままでは新branchを拒否するため、新workflowでは実行前のtransport probeと失敗時の最終回収の両方へ明示します。追加3固定ログは独立した `--nss-probes` で選択します。defaultの8パス、64MiB総量、FD/owner/link検証、OpenSSL 3の認証暗号化は維持します。新originと旧profileの交差は拒否します。

Runtimeの新workflow `mac-native-firefox45-nss-20261102.yml` は、固定fixtureの `--fresh-firefox45 --nss-runtime-closure` と上記回収flagを結びます。compile sourceは `7dd619742b25567e479eeda97ae071d10542ad0b`、harnessは `cbe45a7d7480d713b2c40a9ed348420126e6ccb0`。実行済みworkflowと無効prototypeは変更しません。

## 公開verifierの追加条件

新workflowの独立predicateは、固定graphのhash・softoken/freebl宣言、追加stageの順序・重複なし・正常終了・reap・group cleanup、実stock wrapper/toolのreceipt、同xpcshell/XULと固定probe、NSS PK11乱数操作＋Rust SHA-256既知解、単一test lifecycle・2件以上のPASS assertionを要求します。pipelineのNSS証拠が欠けた場合、既存42件の証拠が揃っていても最終proofを拒否します。runtime側の `nss_runtime.proof_complete()` はこの追加predicateのoracleとして呼びません。

NSS関連stageと、固定sourceから抽出する `nss-*` の失敗語彙を公開射影へ追加しました。graph/library/probeはhash・件数・固定状態だけを公開し、raw logや自由文を含めません。元の45行ledger・first failure・stdoutと保存artifactの一致も維持します。実DSOのmap先、symbolication品質、full-session cleanup、製品qualificationは認定しません。

## 現在の検証

- 新originと暗号化回収の関連58件は、秘密情報なし・networkなし・source read-onlyの隔離環境で全成功。新規8件はこの58件に含みます。新branchでsummary・first failureと追加3ログの認証復号をbyte一致確認しました。
- 824 exact IDsを全発見し、旧816 IDsを保持。157 source hashesを固定しました。今回変更はorigin・回収器とそのtests・catalogの5ファイルだけで、他152 hashesは前回と一致します。前回の全816件（811成功・同Darwin専用5skip）は過去証拠として保持し、現在sourceの全824件実行と合算しません。未変更766件は今回再実行せず、新native preflightでは824件すべての成功・skip0を要求します。
- 公開射影7制御は失敗時の45行・first failure・stdout/artifact一致を確認。独立NSS predicateの合成正例1件と負例26件、壊れたsummaryの秘匿確認が成功しました。SHA-only、graph/fixture/binary差替え、欠落・重複・reorder・reap不明・bool件数等を拒否します。合成結果はMac成功に換算しません。
- workflowの正例と起動負例10件、全bash/埋込Python構文、catalog/modules/pins・明示flag・失敗時回収を確認しました。
- 前回Floorp CI41293611976/attempt1/head19546317の5jobsは終端success。artifact13078881939の前後API・全ZIP/hash/期限/lineage・logsを照合しました。Runtime shadow41292739802の312contractsと全artifact監査は、同じbytesの既存証拠を保持します。

SDK13011079529のproducerは41128679337/attempt1/head032085d9/job124313694921、期限は2026-11-13T21:08:48Zです。保存正規ZIPと最新APIを照合し、同じbytesの49,701-member独立監査を再利用します。compile sourceとは別の来歴です。

## 起動前と次の一手

新fixture commit、全157 pins、824-ID catalogと新workflowを専用handoffへ固定して通常CIを照合します。起動直前に新branch不存在・push run0、両Draft/open/未merge/担当、SDKのAPI/bytes/期限、操作意図・排他・固有証拠を再確認します。揃った場合だけ新branchを厳密なPOST git/refsで初回1回作成し、必要なnativeを1実行ずつ進めます。返却不明ならref/run/attempt/jobを照合し、POSTを再送しません。新branch作成前のworkflow公開だけではnativeを起動しません。

実MacのDSO build/load/stack-fixerと42件は未判定です。既存scope tests/assertions/manifest/plan、3秒query/3samples・30秒profile、source/FD identity、owned cleanupは維持します。
