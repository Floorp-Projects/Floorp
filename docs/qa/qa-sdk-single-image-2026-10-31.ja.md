# 単一image・完全archiveを固定するSDK producer

2026-10-31 JST 06時台。M1のMac42件に必要なSDK入力を確定する作業単位。

## 終端と独立照合

[Snapshot41078301968 / attempt1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/41078301968)、
head `2fe74b81ac3329a66f0b1d9079beab3154205b86`、job124140325890はsuccess。全stepとowned cleanupが成功、first failureなし。
report artifact12988147877（1916 bytes）とSDK診断artifact12987718264（105157519 bytes）を正規経路で取得。
前後の認証API metadata・repo914860957/run/head/attempt・期限（2026-11-13）・全ZIP bytes/digestを照合した。
producerSHA256・固定reader pins・全contextと、producer/reportの最終stdout一致を確認。

全49701メンバー／704719765 logical bytesを既存readerと独立した読取処理の両方で検査し、
全gzip/raw/tree/order/metadata/11 header digestsがreportと一致。
canonical tree `c3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f` とSDK26.5、格納順序は旧baselineと同じ。
新raw `f0d4937d3793e5bd0d17ec899df993fb3b68267dab88ddaa9bdc14ccab7c4a58`、760518144 bytesを明示固定する。
旧raw57a/32ddとは異なる。旧profileを変更して成功へ書き換えない。
正規化gzip `15c6c4ba110a5ff7c231f85a352c5e226f5407243c961d5a04a548d73f3886de`、105599920 bytes。
旧baselineとのheader差は mtime, pax_headers。image更新が差の原因という因果は未実証。
今回のSDK診断artifactは引き続き **NOT_ADMITTED**、製品入力にしない。
初回監査のstdout抽出は先頭keyの想定で停止し、その失敗を保存した。抽出を修正して再照合した。

## 新しい限定producer

`qa-sdk-single-image-20261031.yml` と `qa-sdk-exact-image-20261026.json` を追加する。
selectorや旧profileは使わず、独立照合した1つの全archive profileだけを事前固定。
image20261026.0455.2/macOS26.6.2/Python3.9.6/arm64/Xcode26.6/build17F113/SDK26.5をtar前に確認する。
reader2本と新profileのSHA256を固定し、producer/report/profileを結合。
全gzip/raw/tree/order/metadata/header digestのどれか1つでも違えばSDK uploadを止める。
外側gzip mtime4 bytesのみ従来のencodingで固定、raw/PAX/内容の変換なし。
未知image、別profileへのfallback、実行結果に合わせた期待値の変更はしない。

自分のhandoff branchの固定parent `2fe74b81ac3329a66f0b1d9079beab3154205b86`、initial push/created=false/attempt1のみ。
macos-26の1 job、最大20分、初回1回。reportは失敗時も保存、SDKは完全一致時だけ別artifact
`macos-sdk-single-image-v4` として14日保存し、専用4 regular filesをcleanupする。
SDK成功も `PENDING_API_RECONCILIATION / loaderAdmitted=false / productQualification=false`。
その後の前後API/全ZIP/producer/profile照合で得た実artifact/run/job/head/expiryを固定してから、
別source/schema4とloaderを実装し、差替え・失効・lineage負例、新sourceの全catalog/pinsへ進む。
今回SDK producerとコンパイル元7dd619742b25567e479eeda97ae071d10542ad0bは別のcommit。

Floorp QA helper148 hashes/754 IDs、loader/schema1〜3、旧workflow、plan/assertion/manifest、
fresh45無効候補、旧practical/45ledgerは不変。既済全回帰は再実行・加算しない。
通常Floorp CI41078286356の5jobs、docs41078286207のverify、Runtime shadow41078313206は終端success。
新fresh45 ref/POST、Mac42件、完全scoped log、最終製品/全OS qualificationは未達。

## 隔離検証

秘密情報なし・networkなし・source read-onlyの隔離環境で32 controlsが成功。
起動/rerun/context/reader/lineage/Xcode/attempt/採用状態の差替え、破損archive、全49701メンバーの完全profile一致、
内容が同じ旧SDKをraw/metadataの違いで拒否、失敗stdoutと保存report一致、owned/foreign/link/hardlink cleanupを確認した。
148 helper hashesと754-ID catalogは不変。旧全回帰と公開verifier7件は再実行・加算しない。
