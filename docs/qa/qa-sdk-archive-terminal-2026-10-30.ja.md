# SDK archive診断の終端と次の受入条件

2026-10-30 JST 06時台。担当B。M1は継続、Mac42件は未実行。

[診断run40898949074 / attempt1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/40898949074)はhead `ba77282a2ee5711e3a38ab379704b7ae46e1637f`、job123535644768で終端failure。
初回失敗は`Keep the original raw identity gate`。失敗前のreport・候補SDKの保存と、所有一時ファイルだけのcleanupはsuccess。
再送・rerunは行っていない。

## 確認した差分

- 候補artifact12908252491の105136306 bytesとSHA256 `62330d0847ef689a28197f0ad57e8a0031634d8fe5b05576aef38f5b5ba4f6b7`、report12908038229の1595 bytesとSHA256 `a5e25322ee904b89d89f43b538bdeaa36dab66367ccf97b88e6dc9234306f34d`を正規取得bytesで確認。前後API metadata、repo914860957、run/head/attempt/job、expired=false、期限を照合した。候補の期限は2026-11-05T15:12:30Z、reportは2026-11-12T15:12:28Z。
- 秘密情報・networkなし/source read-onlyの隔離環境で、候補全49701メンバー/704719765 logical bytesと全gzip footer/tar末尾を検査。実機reportとローカル再計算は完全一致。SDK26.5、canonical tree `c3c01387c82e9ce1436fd517ec19308416663cc526c39be15f66f6bab22c3a5f`と格納順序は保存旧SDKと一致した。
- 全49701メンバーのmtimeが変わり、22417件ではPAXのmtimeも変わっていた。その他9 header field digestは一致。別のraw構造監査でも両760518144 bytesを末尾まで読取り、全相違をmtime・checksum・PAX mtimeの長さ/値と対応header sizeに限定できた。file内容・link・padding・それ以外のheaderに差分0。
- 旧raw SHA256 `57aeb200890ba97c382bf450f23f9dab3e0d4696fdf02323a705f82210b087e3`と今回 `32dd16b5188f7a2d49b461dfaf6d937c8f50181e71ff5284c9fa91ff3b3d7f64`の不一致は時刻metadataで説明できる。前回probeで得た32dd16b5と今回rawが一致したことも確認した。runner image更新との因果までは実証していない。

## 採用境界と次の1手

今回の候補は調査資料であり`NOT_ADMITTED`を維持する。元profileのraw identity gate、schema1〜3、plan、上流assertion/manifestを変更していない。旧SDK12739697223は期限切れ/API404の比較baselineだけに使用した。
内容一致を理由にraw hash検証を省かず、元の失敗を成功へ書き換えない。

次の作業単位は、現在の全archive identityを明示固定する独立したSDK producerとversion付きsource候補の準備。
元profileを保持し、新profileだけにrepo/run/attempt/job/producer commit、Xcode26.6/build17F113/SDK26.5、wire ZIP/gzipのsize/hash、raw32dd16b5のsize/hash、全content treeと格納順序・各header digest、API期限を固定する。
新producerは初回の別条件1回、最大20分、Firefox buildやMac42件は含めない。producer終端successと最新API・全bytesの再照合を終えるまでloaderへ採用しない。source schema追加には改変・入替え・期限切れ・別raw/別treeの負例と、全source catalog/pinsの再結合が必要。
時刻差を任意に許容する一般化や、旧期限の延長は行わない。新しい固定SDK sourceが成立してからfresh45のfixture・全pins・branch/run0・345分work/360分jobを凍結する。

## 維持した検証と未達

Floorp QA helper148 hashesと754-ID catalogは不変。既済749成功/実Darwin専用5skip、公開verifier7 controls、実機依存13 controls、診断器12/起動制御17は元sourceの証拠として保持し、再実行・加算していない。
通常Floorp CI40898945379 / headb79f7fb / attempt1はsmoke+4環境jobすべてsuccess。Runtime shadow40898963223 / headba77282a / attempt1もsuccess。
これらはMac上流42件や最終製品合格を示さない。
旧Intel2runの失敗、45行の42 UNVERIFIED + 3 MANIFEST_DISABLED、実行/成功件数不明、旧practicalContinuationを維持する。
新fresh45 branchは未作成・POST未送信、完全scoped log・Mac42・最終製品・全OSは未確認。
両Draftを維持し、main/merge/release/第三者送信/再委任/routine変更は行わない。
