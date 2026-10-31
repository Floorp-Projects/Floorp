# Mac SDKのrunner image固定と完全archive profile

2026-10-30 JST 18時台。M1のSDK前提条件だけを扱う。

## 前回の終端

[producer40996012481 / attempt1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/40996012481)、
Runtime head57a5825a2c72f80b32e4fa783af2a85ee66dd8b2、job123869190930はfailure。
最初の失敗はCreate and verify every fixed archive identity。
report保存とowned cleanupはsuccess、SDK uploadはskipped、SDK artifactは0件だった。
report artifact12951441466の前後API metadata、期限、ZIP1966 bytes/digest、producer lineage、
ログの最終JSONと保存reportの一致を確認した。

この実機はImageVersion20261013.0424.1、前のforensic40898949074は20261021.0445.8。
前者のrawは元の57aeb200へ戻り、後者の32dd16b5を要求したv2 profileから拒否された。
保存済み旧SDK全49701メンバーを再検査し、外側gzip時刻だけ0にした後の**全archive identityが今回の実機reportと一致**した。
内容・格納順序・SDK26.5も同じ。image差が時刻差の原因という因果までは実証していない。
元の32dd要求・失敗・実行済みworkflow・失効済みSDKの記録は保持する。

前回Floorp CI40996000828/head211e2a2はsmokeと4環境job全success、
Runtime shadow40996017301/head57a5825a2/attempt1/job123869206502もsuccessを確認した。

## 今回の変更と上限

新しいworkflow `qa-sdk-image-bound-20261030.yml` を追加した。
自分のhandoff ref、固定parent57a5825a2、初回push、created=false、attempt1だけを許す。
macos-26/arm64の1 job・最大20分、1回だけ。Firefox build/42件を含まない。
未知のimageはtar作成前に拒否する。同じimageでも全archive identityが違えばSDK uploadを拒否する。

| 観測image | 選ぶ固定profile | 正規化gzip bytes / SHA256 | raw SHA256 |
| --- | --- | --- | --- |
| 20261013.0424.1 | qa-sdk-exact-legacy-image-20261030.json / 660193da5a1dc9b3ae7c52feb234e58881b2dedeea50ea0bdb0648e8d32cbd04 | 105567985 / 6efe256ea134764ac20d31b5b80e4d1e7a5d6bdd1d7662b57119dee5f0e9e3dc | 57aeb200890ba97c382bf450f23f9dab3e0d4696fdf02323a705f82210b087e3 |
| 20261021.0445.8 | 既存qa-sdk-exact-20261030.json / 184d797d742f64c0beeb65dfe605b881f578a30a0b6b7a017c43cccac5fd2d1b | 105611310 / 67adc9681c53b25d4a41ef92e7fa1330da22ff330db072e1f83eefcc3713fe47 | 32dd16b5188f7a2d49b461dfaf6d937c8f50181e71ff5284c9fa91ff3b3d7f64 |

profile選択はarchiveを見て都合のよい方を選ぶ処理ではない。
ImageOS、ImageVersion、macOS26.6.2、Python3.9.6、arm64、Xcode26.6/build17F113、SDK26.5から先に1つだけ固定する。
選択後のprofile差替え、profile値の混在、fallbackは許さない。
各profileはgzip全bytes、raw全760518144 bytes、tree全49701メンバー/704719765 logical bytes、
格納順序、全metadata、11 header-field digestを固定する。
全内容treeはc3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f。
元gzipのdigestも記録し、変更は外側gzip header時刻4 bytesだけ。raw時刻/PAX/内容を正規化・許容しない。

元のreader/verifier/v2 profileは不変。新selectorと両profileのSHA256をworkflowへ固定し、
選択profileと検証reportのSHA256を再照合してから別名 `macos-sdk-image-bound-v3` をuploadする。
hash-only producer/selection/reportは失敗時も14日保存し、SDKも別artifactで14日保存。
秘密情報・任意パスを収集せず、専用の5 regular filesだけをowned cleanupする。

## 検証と未達範囲

既存の秘密情報なし/networkなし/source read-only bwrapで、新selector9テストと起動/cleanup/report結合12 controlsが成功した。
未知image、環境各項目、欠損/余剰/bool、profile bytes差替え、別image profile、symlink、
profile値混在・raw時刻差を拒否する。初回push以外、foreign cleanup、report選択違いを拒否する。
2つの完全offline baselineへprofileを結合した。今回57aは全内容再検査、32ddは不変の前回全内容検証を再利用した。

Floorp SDK loader、schema1〜3、plan、fresh45無効候補、148 helper hashes/754-ID catalogは不変。
既済749成功/実Darwin専用5skip・公開verifier7 controlsを再実行・加算しない。
SDK source schema4は引き続き無効な設計資料であり、今回のselectorはloaderへの採用を行わない。
producer commitとコンパイル元7dd619742b25567e479eeda97ae071d10542ad0bは区別する。

新producerがexact-matchでもPENDING_API_RECONCILIATION/loaderAdmitted=false。
同run/head/attempt/jobの終端success、全step/first failure/cleanup、前後認証API/期限、
取得ZIP全bytes/digest、全gzip/raw/tree/順序/metadata/header、実機とローカルreport/selectionの一致が必要。
その後だけ、**採用した1つのprofileとartifact実ID**を明示するloader/sourceを追加し、差替え/失効/lineage負例と全catalog/pins回帰を行う。
新SDK bytesが取得・照合できるまでは新規native42件を開始しない。

旧Intel2runは消費済み失敗、45ledgerは42 UNVERIFIED + 3 MANIFEST_DISABLED、実行/成功件数不明のまま。
new fresh45 branch/POSTとMac42件、完全scoped log、最終製品/全OS qualificationは未達。
main/merge/release/第三者送信/他担当・routine変更/再委任は行わない。
