# 完全一致SDKを実artifactの読み込み経路へ接続する

2026-10-31 JST 12時台。M1のSDK入力確定と新source回帰の作業単位。

## 確認した実artifact

[producer41128679337 / attempt1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/41128679337)、
head `032085d9c0176b68d405fb7d3297629df6c1a971`、job124313694921は全step・保存・owned cleanupがsuccess、first failureなし。
SDK artifact13011079529は105157519 bytes、ZIP SHA256 `8d1fc67bc1b130e61c1fd334dcfdadd525a6bf095ad2d50d76a5626950d93aca`、期限2026-11-13T21:08:48Z。
report13011134290は2040 bytes、ZIP SHA256 `e4fd60789b9ace6e9d758e8ad2510cf2f9969f642b1e7a4abb1314d24798f9c6`。
正規取得の前後API、repo914860957/run/head/attempt、全wire ZIPとproducer/profile/最終stdoutを照合した。
全49701メンバー、704719765 logical bytes、全gzip/raw/tree/order/metadata/11 header digestは独立readerでも固定profileと一致。
profile SHA256は `ce50e72c5aa2d163a39ba6de283c58f1e2930fb5e261a22ddb6160f54edaaf42`。

## source/schema4

`tools/qa/native-nonpgo/sdk_source-20261031.json` を明示指定した場合だけ、新しい完全profileと実artifactを使う。
資格語彙は `official-artifact-exact-profile-v2`。全pin、producer、profile、image、Xcode、期限を固定し、未知/混在/型違いを拒否する。
compressed105599920 bytes/SHA256 `15c6c4ba110a5ff7c231f85a352c5e226f5407243c961d5a04a548d73f3886de`、
raw760518144 bytes/SHA256 `f0d4937d3793e5bd0d17ec899df993fb3b68267dab88ddaa9bdc14ccab7c4a58`。
canonical treeは従来の `c3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f`、SDK26.5を維持する。
source1〜3と旧raw57a/32ddの期待値、旧失敗・期限は保存。SDK producerとコンパイル元 `7dd619742b25567e479eeda97ae071d10542ad0b` は別revision。

fetchは取得前と全payload/raw検査後に認証APIのartifact/run/jobを照合する。run成功・workflow/ref/event・repository/head/attempt/job、固定期限を要求。
取得後の失効・変更・期限到来はreceipt作成前に拒否する。receipt/SDK proofにはexact profileを残し、入替えや削除を拒否する。
保存API応答と正規実bytesを使う秘密情報なし・networkなし隔離環境でもfetch→receipt→全SDK展開/tree検証が成功。
これは新しい認証HTTP呼出しの実機試験ではなく、正規取得した実bytesと保存metadataを使うoffline結合検証である。

## 検証と次の関門

全765実IDを20の秘密情報なし/networkなし/source read-only隔離batchで検証。760成功・従来と同じ実Darwin専用5skip・0fail/0error。
旧754 IDsを全保持し、新規11件はこの765件に含む。全batchと無効候補の150 source hashesが一致。関連11件の単独実行は合算しない。
公開診断7制御で失敗時の45行/不明/first failure/stdoutと保存artifact一致を保持し、SDK proofの正例と差替え負例5制御も成功。
無効fresh45候補を新source・catalog/pinsへ結合する。実行済みworkflowは不変。345分work/360分job、3秒query/3samples、30秒profile、source/FD identity、owned cleanupは維持。

前回通常Floorp CI41128672556は4jobs成功、Linux x64の `Run combined browser test suite against locked Runtime` がexit1。
Marionette readyは観測済み。詳細artifact13012153420取得はHTTP503で失敗し、原因は未確定。成功へ換算せずmanual rerunしない。
docs41128672561 verifyとRuntime shadow41128687783はsuccess。今回変更後のCI終端を確認して次のnative入力凍結へ進む。

Mac42件は未開始、fresh45 ref不存在/push0/POST未送信。旧45ledgerは42 UNVERIFIED+3 MANIFEST_DISABLED、実行/成功数は不明。
旧practicalContinuationを保持し、SDK bytesや秘密情報を報告・証拠ZIPへ入れない。最終製品/全OS qualificationは未達。
初回ローカル監査が取得途中のafter metadata不足で停止した記録も保持し、取得完了後に全照合した。
