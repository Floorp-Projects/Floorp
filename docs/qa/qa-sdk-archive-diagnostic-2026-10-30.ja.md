# SDK archive差分の限定診断

担当B。2026-10-30 JST。M1のMac上流42件は未確認。

前回の入力probe run40848328700 / attempt1は、Xcode26.6/build17F113/SDK26.5の
選択には成功したが、tar全体のSHA256が`32dd16b5…`となり、固定値`57aeb200…`を
満たさず失敗した。サイズはどちらも760518144 bytes。前回の失敗とSDK artifact0件の
記録は保持する。旧SDK12739697223は期限2026-10-29T13:57:37Zを経過し、今回の
APIでは404だった。有効なbuild入力として利用しない。

## 今回の変更条件と上限

Runtime `.github/workflows/qa-sdk-archive-diagnostic-20261030.yml` は、前回失敗runの
rerunではない。新しい診断器で内容・格納順序・ヘッダーの差分を取得するための候補。
自分のhandoff branchの固定parent `e0765995c3d6ad4d122bd9b2eed07734b0a635ef` からの
今回のpush、created=false、attempt1、変更workflowパスだけを許す。
`macos-26` 1 job・最大20分、tar作成と診断step最大8分、診断器内部120秒。
package install・Firefox build・42件の実行は含まない。

元と同じXcodeとSDKを選択する。診断器は実headからsparse checkoutし、SHA256を
workflow内で照合する。既存実行済みworkflow・fresh45無効候補・production SDK loader・
上流assertion/manifest/planは変更しない。新fresh45 branchも作成しない。

## 取得する証拠と採用境界

全regular file内容とlink targetをhashし、既存と同じcanonical treeを計算する。
格納順序、名前順に並べたメタデータ、mode/uid/gid/uname/gname/mtime/type/linkname/
device番号/PAX headersそれぞれのdigestを別記する。完全なgzip footerとtar末尾も
読み、compressed/raw両方のsizeとSHA256を保存する。展開や任意ファイル収集はしない。
source/FD identity、path/link検査、件数・展開量・時間の上限を維持する。

hashだけのreportと限定producer情報は固定2パスから14日保存する。
SDK候補は別名 `sdk-forensic-candidate-<run>-1` として7日保存し、ローカル隔離環境で
全bytesを再検証する資料に限定する。build用artifact名`macos-sdk`は生成しない。
reportと候補を保存した後も、元のraw tar・canonical tree・SDK versionの一致条件を
すべて要求し、不一致ならworkflowは失敗する。すべて一致しても`NOT_ADMITTED`のまま。
実API来歴・期限・bytesと明示pinの最終照合なしにSDKへ採用しない。

元SDKの保存bytesは今回の比較baselineに限定する。新診断器で全49701メンバー、
704719765 logical bytes、raw tar760518144 bytes、canonical tree
`c3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f`を確認した。
新SDKの内容との一致は、実機の資料取得後に判断する。

## 検証と次の判定

秘密情報・ネットワークなし、source read-onlyの隔離環境で内容変更・順序変更・
メタデータ変更の区別、危険なpath/link/device、重複、gzip切断、末尾データ、入力symlink、
資源上限と採用不可の維持を検証する。workflowの構文・起動負例・元identity gateも検証する。
Floorp QA helperの148 source hashesと754-ID catalogは不変で、前回749成功・
実Darwin専用5skipの全回帰を再実行・加算しない。今回のRuntime診断器12件は別記する。

前回doc head b2b70afの通常CI40848313173 / attempt1は終端successで、smokeと
4環境jobすべてのsuccessを照合した。これはMac42件や製品の合格を示さない。

実機終端後に同run/head/attempt/job、API metadata/digest/expiry、SDK候補bytes、
公開reportとjobログの一致を照合する。内容tree・順序・各header digestを元SDKと比較し、
原因が内容か表現かを判断する。元identityと異なる候補の自動採用はしない。
旧Intel2runは消費済み失敗、有効42件はUNVERIFIED、無効3件はMANIFEST_DISABLEDを維持する。
