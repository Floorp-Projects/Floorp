# Mac検証器の順序修正を別workflowへ接続する

2026-11-04 JST。M1の選定上流試験を最後まで検証できるようにする作業単位。

消費済みrun41630387790/attempt1/job125898143523はfailureのまま保持する。有効42 PASS・manifest無効3 SKIPは認証済みログと台帳で確認済みだが、実workflowの最終検証器で順序不一致があった。

新workflow `mac-native-firefox45-verifier-order-20261104.yml` はinventoryをhelpers install直後、7 target buildより前に要求する。前回隔離評価した厳密な順序候補をそのまま組み込み、欠落・重複・並べ替えを許さない。消費済みworkflowは変更しない。

専用branchは `b/qa-fresh45-verifier-order-20261104`。明示flagの新originと暗号化回収profileを追加し、旧branch・曖昧なselector・attempt2・dispatch・既存taskを拒否する。認証APIの同run/head/job/開始時刻へ結び、clockの作り直しを認めない。

上限はmacos-15-intel、fresh-firefox45、attempt1、job360分/work345分。3秒query/3samples・30秒profile、source/FD identity、owned cleanup、元の上流tests/assertions/manifest/planは維持する。回収は既存と同じ16固定パスだけをCMS認証暗号化する。SDK・任意ファイル・環境変数を列挙しない。

SDK13011079529は現行APIのbefore/after、producer41128679337/job124313694921/head032085d9、保存済み正規ZIP105157519bytes/SHA256 `8d1fc67bc1b130e61c1fd334dcfdadd525a6bf095ad2d50d76a5626950d93aca` と一致した。期限は2026-11-13T21:08:48Z。全49701-member監査は同一bytesに対する既存証拠を再利用する。コンパイル元7dd61974とSDK producerを混同しない。起動直前のAPI再照合は別途必要。

## 検証

- 新旧sourceを結び、旧839実IDを全保持した848-ID catalogを全発見。157source hashesのうち変更はorigin・archive・両テスト・catalogの5件だけ、残り152件は不変。
- 関連76 uniqueテスト成功・skip/failure/error0。初回は75成功と既存CLI mockの期待引数漏れ1失敗。期待値を修正し、その1件だけ再実行。成功済み75 methodのAST不変を確認して再利用した。新9件を重複加算しない。変更のない772件と全848のローカル実行は行わない。
- 公開射影の7合成失敗制御で45行、未実行/不明/記録済み、first failure、stdout最終結果とartifact一致を確認。
- 独立NSS/最終summaryの正例1・負例26、起動正例1・負例10、全bash/埋込Python構文、pipeline/transport flag結合が成功。
- 比較検査で全workflow stepと全判定関数ASTが、明示したorigin/ref・848件・順序の変更以外は一致。前回の14 target負例と6最終summary負例は前回source/同一判定の証拠として保持し、今回再実行・加算しない。比較検査の初回は期待側のorigin固定語彙置換が不足して拒否され、その検査設定を直してから成功した。

新workflow全体の実Mac成功は未確認。まず同じ公開fixtureの通常CI・shadowを終端照合し、新branch不存在/push0・SDK期限・両Draft/担当を再確認する。その後だけ固有の意図を保存し、新branch初回のPOST git/refsを1回送る。消費済みrun/refの再利用・manual rerunはしない。

旧失敗/45ledger/practicalを保持し、製品qualificationはfalse。task/session全子孫cleanup、最終製品、全OSは未認定。M1の再検証が進んだらM2のDebug/PGO source対応とM3のexact final artifactへ移り、土台の件数増加を目的にしない。7日経過の見直し基準時刻は2026-11-05 01:53 JST。
