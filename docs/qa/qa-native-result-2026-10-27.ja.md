# Intel Mac実行結果と補助実行ファイルの配置修正

[Runtime run 40373209304 / attempt 1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/40373209304)
は2026年10月27日07:12 JSTにfailureで終了した。
実行headはRuntime a8364771a826fd39bbe5adfe2a6fc124cb680b6d、
fixtureはFloorp 9fc6d3f58b94a2286ba6342dc7316c0dc472d6ae。

## 観測した結果

公開verifierはsource pins・事前検証・SDK由来・元の345分予算を照合した。
7つのtarget buildと5つのupstream準備コマンドはexit 0。
公開されたconfigure／exports等を含む15ステージのleader回収・process group cleanupを確認し、
全58ステージのcomplete判定もtrueだった。session全体・すべてのdescendantの閉包は未確認のまま。

停止位置は補助実行ファイルの前提検査で、plugin-containerの次のTestArgumentsがfile条件を満たさなかった。
inventoryは有効42件・上流manifest無効3件を確認済み。
有効42件はすべてNOT_EXECUTED、無効3件はMANIFEST_DISABLEDとして45行を保存した。
smokeも上流42件も未実行で、製品qualificationはfalse。
元のrun 39461757999のmonitor-unavailableの原因まで解明したという結果ではない。

## 修正

固定Runtime 7dd619742b25567e479eeda97ae071d10542ad0bの
xpcom/tests/moz.buildは5つのSimpleProgramsをTEST_HARNESS_FILESへ登録する。
python/mozbuild/mozbuild/backend/recursivemake.pyの
_process_final_target_filesは、生成済みのobjdirファイルをmisc tierで配置する。
既存経路にはxpcom/tests/targetしかなく、その配置処理が欠けていた。

helpers build直後にmach build -j3 -C xpcom/tests miscを追加。
元のshared deadlineを渡し、配置ステージの失敗・欠落が合格にならない検証を追加した。
上流45テスト、assertion、manifest、plan、Runtimeコンパイル元は変更しない。

Runtime公開verifierは、固定sourceのfailure code一覧を読み込む前に永続化した失敗を投影し、
実際のmozilla-upstream-current-native-assetがrunner-failureへ一般化されていた。
sourceを照合してから投影する順へ変更。
source照合が失敗しても、固定された失敗メタデータの取得はfinallyで行う。

事前検証で、build profileを同一サイズで書き換え、metadata identityが変わらない場合の
既存の見逃しも再現した。解析前後に同じdescriptorからSHA-256を計算して照合する。
2 GiB上限・30秒予算・1 MiBの読み取り単位・既存のinode／owner／freshness条件を維持した。

## 検証

- 修正前のmetadata固定対照はJSONLとwhole-profileの両方で誤受理を再現した。
- 修正後の関連76件と、helper／fresh45／pipelineの114件は成功。
- 最終sourceの固定699 IDを18バッチで実行。694成功・実Darwin専用5 skip・失敗0。
  全IDの一意性・完全一致と、変更した7 sourceのhashをバッチごとに確認した。
- 公開verifierの実コードを使う7つのsynthetic対照も成功した。
  summary欠落・harness前停止・不完全ログ・未認定buildに加え、
  永続化した既知code・未知code・source pin不一致を検査する。
- 途中の単一プロセス実行はexit 143で終端し未完了。原因は確定しておらず、成功件数に含めない。
  分割検証では一度tmpfsの空き容量が実disk reserve条件を満たさず失敗した。
  TMPDIRを専用ディスクへ替え、guardを変更せず同じバッチを成功させた。

## 再実行の条件

SDK artifact 12624490543の現在のAPI取得はHTTP 404。
macos-sdkという名前の現存artifact一覧も0件だった。
記録上の期限は同日11:31 JST。消失の操作原因までは確認していない。

Runtime Draftには、安定版SDK 26.5の完全なuncompressed tarの
760518144 bytes／SHA-256
57aeb200890ba97c382bf450f23f9dab3e0d4696fdf02323a705f82210b087e3
が一致した場合だけ2日保持する、20分上限の取得用workflow候補を置く。
専用branchの初回push・attempt 1だけを受け付ける。候補配置だけでは実行されない。

新しいSDKの作成と、修正版Intel Macの追加1回実行には別の実行承認が必要。
前回の「1回実行する」は完了したrunに使った。
現在の承認範囲内で追加のnative実行、merge、releaseは行っていない。
Windows現head・最終配布物・全OSのqualificationも引き続き未確認。
