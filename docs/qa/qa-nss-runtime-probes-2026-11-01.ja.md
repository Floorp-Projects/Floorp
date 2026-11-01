# M1：限定NSS buildと実行時probe（2026-11-01 18時台 JST）

初回Mac41187440235/attempt1/job124497118586のNSSライブラリ読込み失敗は未解決です。今回の変更は次の実機検証の候補で、Mac42件の結果や修復成功を示しません。

## 変更した実行経路

新しい `--nss-runtime-closure` は `--fresh-firefox45` のときだけ有効です。省略時は既存経路を維持します。旧workflowと消費済み実行を変更・再実行しません。

1. 検証済みtools展開後に、固定stock `fix_stacks.py` wrapperと実 `fix-stacks` を最大30秒で起動します。固定frameを1つ処理し、実toolのEOF/exit0・wrapper marker・owned stage cleanupと実行前後FD同一性を確認します。symbolicationの品質はUNVERIFIEDです。失敗ならコンパイルへ進みません。
2. 既存のgenerated root.mk/root-deps.mkをFD付きで読み、元の完全hashと固定parserの実宣言を照合します。softoken_softokn3とfreebl_freebl3の宣言が両方ある場合だけ、同じsource/OBJDIRで `mach build -j3 -C . <target>` を直列実行します。graphの欠落・変更・link・不明構文やcleanup不明を拒否し、各stage後もgraphを再照合します。上限は元の345分work/360分jobのままです。
3. 元のshell/browser/faster buildとchild resources配置を終えた後、同じ現行xpcshell/XULで独立した最大120秒のstock harness probeを1件実行します。既存45 IDs・assertions・manifest・planは変更しません。probe自体を42件の実結果へ加算しません。
4. probeはNSS経由の32-byte乱数生成と、独立したRust SHA-256の既知解を確認します。成功には正確な単一test lifecycle、2つ以上のPASS assertion、正常終了・reap・group cleanup、実行前後の同binary identityが必要です。実DSOのmap先、full-session cleanup、製品資格は認定しません。probe後に既存smokeと42件scopeへ進みます。
5. opt-in時は保存summaryの最終proofでも追加build/stack/probe receiptを要求します。追加証拠の欠落を既存scopeの成功で補いません。first failureと実行/未実行/不明の扱いを維持します。

## Oracleをsourceへ結ぶ

コンパイル元 `7dd619742b25567e479eeda97ae071d10542ad0b` の登録・IDL・実装を正規APIから読取照合しました。SHA-256単独はNSS loadの証拠になりません。`crypto_hash/src/lib.rs` はRustのsha2実装です。

`components.conf` のrandom-generator登録は `NSSConstructor<nsRandomGenerator>` を使います。`nsNSSModule.cpp` はconstructorで `EnsureNSSInitializedChromeOrContent()` を要求し、`nsRandomGenerator.cpp` は `PK11_GetInternalSlot` と `PK11_GenerateRandomOnSlot` の成功を要求します。乱数の内容・暗号品質は期待値にせず、成功と長さ32だけを確認します。Rust SHA-256には固定入力abcの独立した既知解を使います。

## 失敗資料と互換性

暗号化回収の既存8パスはdefaultで不変です。次workflowが `--nss-probes` を明示したときだけ、stack probe log、NSS probe raw harness log、NSS probe console logの固定3パスを追加します。各1MiB、既存の総量64MiB/読取30秒、所有者/FD/link拒否とCMS認証暗号化を維持します。任意ファイルやSDK、鍵・環境変数を追加しません。元scopeの2ログの回収有無と追加probeの回収有無を分けます。

Linux隔離環境のstock wrapper検証では制御用実行ファイルを使います。flow model・合成receipt・既知解fixtureの確認を実Mac成功として扱いません。関連検証中の初回失敗は、inventoryより前の停止をNOT_EXECUTEDと期待したtest側の誤りでした。既存ledgerのinventory未確認→UNVERIFIEDを保持し、期待を修正しました。失敗記録は保存します。

## 次nativeに必要な凍結

新しい未使用branch/origin（候補 `b/qa-fresh45-nss-20261101`）、今回の実行flagと証拠回収flag、fixture/head・全source hashes/実ID catalog、SDK13011079529の直前期限/同一性、345/360分と固有失敗資料を結ぶ必要があります。現在のorigin selectorは旧実行専用のままです。新workflowの公開verifierにも追加proofと固定語彙を結び、完全な回帰・公開制御・通常CIを確認してから、厳密ref作成1回で初回nativeを起動します。同runのmanual rerunや同条件の盲目的反復は行いません。

今回新native/manual rerun/SDK producerは起動しません。元nativeの42 UNVERIFIED+3 MANIFEST_DISABLED、executed/passed unknown、first failure mozilla-upstream-proof-incomplete、group epoch限定cleanup、旧practical/45ledgerを保持します。

検証結果：全816 exact IDsの811成功・従来と同じ実Darwin専用5skip・0fail/0error。旧795 IDs全保持、全157source hashesが41batchで一致。関連62件と新21件は全816件に含む。公開診断7制御と追加probe logs/summary/first failureの認証暗号化・byte完全一致復号1制御も成功。初回公開制御のfresh45 helper pin不一致は拒否として保存し、隔離verifier候補の厳密157-file listとpipeline/fresh45 pinだけを新bytesへ固定した。実行済みworkflowのverifierは不変。
