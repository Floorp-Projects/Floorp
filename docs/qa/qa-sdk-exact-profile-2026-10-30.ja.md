# 別profileによるSDKの厳密固定

2026-10-30 JST 12時台。担当B。Mac42件は未実行、SDK loaderへの採用は保留。

## producerの変更条件

前回SDK診断で全49701メンバーの内容・格納順序が一致し、raw tarの違いは時刻metadataに限定できた。
元raw57aeb200、source schema1〜3、期限切れSDK12739697223、診断runの失敗は保持する。
今回の別profileはraw32dd16b5を明示固定し、内容一致を理由にraw検証を省かない。

Runtime `.github/workflows/qa-sdk-exact-profile-20261030.yml` は、自分のhandoff branchへの
parent `e9a3dfd5273889178711b596d6d95955bd0e32c8`、created=false、push、attempt1だけを許す。
Mac26/arm64の1 job・最大20分、archive作成と検証step最大8分。Firefox buildは含まない。
以前の実行済みworkflowとfresh45無効候補は変更しない。

外側gzipには作成時刻が入っていたため、10-byteの既知header形式を厳密に検査し、
外側headerのmtime4 bytesだけを0へ変えた別ファイルを作る。残りのgzip bytesはそのままコピーする。
raw tar内のmtime、PAX、file/link内容、header、paddingは一切変更しない。
元gzipのsize/hashも記録する。任意のraw時刻差や内容差を許す処理ではない。

## 事前に固定したidentity

profileはRuntime `.github/workflows/scripts/qa-sdk-exact-20261030.json`、SHA256
`184d797d742f64c0beeb65dfe605b881f578a30a0b6b7a017c43cccac5fd2d1b`。
workflow内でprofile・完全読取器・新検証器のSHA256を照合する。

| 対象 | 固定値 |
| --- | --- |
| Xcode / build / SDK | 26.6 / 17F113 / 26.5 |
| 正規化gzip | 105611310 bytes / `67adc9681c53b25d4a41ef92e7fa1330da22ff330db072e1f83eefcc3713fe47` |
| 完全raw tar | 760518144 bytes / `32dd16b5188f7a2d49b461dfaf6d937c8f50181e71ff5284c9fa91ff3b3d7f64` |
| 全member数 / logical bytes | 49701 / 704719765 |
| canonical content tree | `c3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f` |
| 格納順序 | `203dada4fc5a0eda8884f5f52f91bfc17f9692dbc2dd926282b171c7c541f061` |
| 全metadata | `939c939fc71d82bd6da6a4eabacc7ad3a390f64ac9909723dae1a74ea62ae74e` |

さらにmode/uid/gid/uname/gname/mtime/type/linkname/devmajor/devminor/pax_headersの11 digestを
profile内に個別固定する。source/FD identity、入力symlink/重複/危険path/link拒否、
全gzip footerとtar末尾、件数・展開量・時間上限も保持する。

## 検証と採用境界

秘密情報・networkなし、source read-only、writeは/evidenceと/scratchだけの隔離環境で、
新検証器18テスト、起動/CLI/cleanupの13 controlsが成功した。
内容・順序・raw時刻・外側gzip・全header各fieldの差し替え、bool件数、別Xcode、
JSON重複、symlink、既存出力、異常reportを拒否した。CLI4 controlsはstdoutと保存reportの一致も確認した。
保存済みSDK全49701メンバーを正規化gzipから再検査し、上記固定値の完全一致を確認した。
これはoffline baselineの検証であり、新producerの実行結果ではない。

一致時だけ別名artifact `macos-sdk-exact-v2` を14日保存する。hash-only reportと限定producer情報も
14日保存し、失敗時もreportを回収する。SDKを本人向け証拠ZIPには入れない。
新profileが一致しても `PENDING_API_RECONCILIATION` / loaderAdmitted=falseを維持する。
元profileや通常loaderのfallbackには使わない。

新source schema4は `sdk-source-v4-proposal-20261030.json.disabled` に未解決項目を明記した設計候補。
終端success、run/head/attempt/job、前後API metadata・期限・ZIP全bytes/digest、全展開内容とreportを照合した後、
実ID・wire ZIPを明示固定する。loader実装時には別raw/tree/metadata、別producer、期限切れ、入替えの負例と
全source catalog/pinsを新sourceへ再結合する。それまでschema4の実行入力は存在しない。
producer commitとコンパイル元 `7dd619742b25567e479eeda97ae071d10542ad0b` を区別する。

148 helper hashes/754-ID catalogと既存749成功・実Darwin専用5skip・公開verifier7 controlsは
元sourceの証拠として維持し、不変の全回帰を再実行・加算しない。
前回doc headのFloorp CI40955108259/attempt1はsmokeと4環境jobがすべてsuccess、
Runtime shadow40955135690/attempt1/job123733155203もsuccessを確認した。

旧45行の42 UNVERIFIED + 3 MANIFEST_DISABLED、実行/成功件数不明、旧practicalContinuationを維持する。
fresh45のfixture/SDK/source・全pins凍結と有効42件の実評価は引き続き未達である。
