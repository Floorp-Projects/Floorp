# 新しいMac imageのSDKを限定取得する診断

2026-10-31 JST 00時台。M1のSDK前提を進める作業単位。

## 確認した失敗

[image-bound run41031469840 / attempt1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/41031469840)、
head `7b1d90302a668c96022055894340cd7616be2451`、job123981954108はfailure。
最初の失敗stepはSelect the exact image profile and verify every archive identity、
例外は固定語彙 `image-not-pinned`。実際のImageVersionは `20261026.0455.2`。
既存selectorが知る20261013/20261021のどちらでもなく、profile選択とtar作成より前に拒否した。
SDK内容の不一致を観測した結果ではない。今回のSDK内容・raw/headerは未確認である。

report uploadとowned cleanupはsuccess、SDK uploadはskipped、SDK artifactは0。
report artifact12965757652はproducer.jsonだけを含む795 bytesのZIP。
認証APIの前後metadata、期限、全ZIP bytes/digest、同run/head/attempt/repo、producer内の5入力hashを照合した。
archive report/selectionは存在せず、stdoutとのarchive一致は該当しない。
元の失敗をsuccessへ変更しない。

通常Floorp CI41031453407/head564afdeaのsmokeと4環境jobは全success。
docs41031453439はverify success/生成job skipped、Runtime shadow41031476952/head7b1d90302もsuccessだった。
これらをMac42件や最終配布製品の合格へ換算しない。

## 今回の限定取得

新しい `qa-sdk-image-snapshot-20261031.yml` だけを追加。
自分のhandoff branch、固定parent7b1d90302、initial push/created=false/attempt1に限定する。
1 job、macos-26/arm64、最大20分、初回1回。Firefox build/上流42件は実行しない。
今回の目的は、まだ取得していない新imageの完全SDK archiveを検査可能な形で保存すること。
同条件のrerunや既存workflowの書換えはしない。

Xcode26.6/build17F113/SDK26.5のパスとversionの検証は前workflowと同一。
ImageOS macos26、ImageVersion20261026.0455.2、macOS26.6.2、Python3.9.6、arm64に事前固定。
環境が違えば、観測contextと拒否理由をproducer.jsonに保存してtar作成前に止める。
未知imageを既存selectorへ追加したり、候補に合わせてprofileを選ぶ処理は行わない。

既存reader2本のSHA256を固定し、source/FD identity、所有者、link、bytes/members/time上限を維持する。
外側gzip headerのmtime4 bytesだけを0にする既存encodingを使い、元gzipの全bytes/digestも記録する。
raw tar内のmtime/PAX/内容/順序は変えず、全gzip/raw/tree/metadata/order/11 header-field digestを保存する。
元の49701メンバー/704719765 logical bytesのcanonical treeとSDK26.5が違えば候補uploadを止める。

これは**別の診断snapshot**であり、常に `NOT_ADMITTED / loaderAdmitted=false / productQualification=false`。
既存profileのraw一致・image選択条件を緩めたSDK producerとしては扱わない。
producer context・固定reader pins・lineageを検査前に再結合し、producerSHA256をreportへ記録する。
成功時だけ正規化archiveを別名 `sdk-image-snapshot-NOT_ADMITTED-<run>-1` へ保存する。
producer/reportは失敗時も保存、保存期間は両artifactとも14日。
cleanupは専用root内の4 regular filesだけ。未知file、symlink、hardlinkは消さない。
秘密情報、任意ファイル、SDK bytesを本人向け証拠ZIPへ含めない。

## 次の採用条件

同run/head/attempt/jobの終端と全step、first failure、cleanupを照合する。
成功したsnapshotも、前後認証API・期限・repo実ID・producer・全wire ZIP bytes/digest、
全展開内容とraw/header/profileを独立して再検査するまで入力にしない。
その後、取得した**1つのarchive profileとartifact実ID**を明示固定するproducer/source/loaderを準備し、
差替え・失効・lineageの負例と新sourceに必要な全catalog/pins検証へ結ぶ。
もしimageが再び変わった場合は、今回の拒否記録を保持し、同じ取得処理を盲目的に反復しない。

Floorpの148 helper hashes/754-ID catalog、production loader/schema1〜3/plan/fresh45無効候補は不変。
既存の全749成功・同じ実Darwin専用5skip・公開verifier7 controlsは再実行/加算しない。
SDK producer commitとコンパイル元7dd619742b25567e479eeda97ae071d10542ad0bは区別する。
旧Intel2runは消費済み失敗、45行は42 UNVERIFIED + 3 MANIFEST_DISABLED、実行/成功件数は不明。
新fresh45 ref/POST、Mac42件、完全scoped log、最終製品/全OS qualificationは未達。

## 隔離検証結果

構文・起動・context差替え・reader/lineage差替え・失敗時保存・cleanupの29 controlsが成功。
そのうち1件は保存済みSDK全49701メンバーを新snapshot経路で検査し、全archive identityが既存完全baselineと一致。
これは新imageの実行結果ではない。初回の小型content負例はテスト用gzip header設定で止まり、
その初回ログを保存してfixtureを修正後に29件の終端successを確認した。
148 source hashesと754-ID catalogは不変。旧全回帰の再実行・加算は行っていない。
