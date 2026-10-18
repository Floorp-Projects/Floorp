この文書と `test_checkpoint_prototype.py` はローカル設計実験です。workflow、製品コード、既存の検証条件は変更していません。upload、CI起動、cache、公開、runner/権限/SDK保持期間の変更は行いません。

実行時間の境界は次の固定案とします。job1は最初のstepで正規APIから得たjob開始時刻を検証し、経過時間をmonotonic clockへ一度だけ対応付けます。開始時刻が取れない、時計差を説明できない、既に期限を過ぎている場合は実行しません。新しいjob2では新しい原点を作ります。

| job1開始から | 上限までに終える処理 |
| --- | --- |
| 240分 | buildへの予定SIGINT送信。完了済みなら中断せずその実exitを使う |
| 245分 | 既存の所有確認を用いたcleanup、exact child reap、全writer終了確認 |
| 260分 | 全compiler log走査、source/obj manifest、容量/機密情報/リンク検査 |
| 305分 | archive作成、上限付き書込、hash、完全な読戻し検査 |
| 335分 | 1回のupload。再upload/overwrite/cacheなし |
| 345分 | artifact ID/digest/サイズ/保持期限の照合、結果の記録、終了 |
| 360分 | プラットフォーム上限。最後の15分を使用計画へ入れない |

各処理は残時間を使い切る前に期限で失敗終了します。archive/hash/uploadがこの窓に収まる実測証拠はまだありません。10GiBから転送速度や圧縮所要時間を推定しません。既存のRSS 10240MiB、disk reserve 8GiB、log 256MiB、stall/取得不能の停止条件を維持します。archive作成も同じ資源監視の対象です。API/ネットワークの個別timeoutに加えてphase全体のabsolute deadlineが必要です。

安全な保存対象を選ぶ契約は、自然に成功した対象と予定中断を分けます。

1. 最小の自然成功境界は、同じconfigureの後に `mach build -j 3 export` がexit 0になり、その所有writerがすべて終了した時点です。pinned Runtimeの `config/recurse.mk` にexport tierがあります。これは有効な準備状態ですが、job2の6時間問題を解決する進捗量かは未測定です。
2. より進捗を残す具体候補は、その後の `mach build -j 3 --allow-subdirectory-build toolkit/library/rust` の成功境界です。pinned `toolkit/library/rust/moz.build` はgkrustを宣言しています。コマンドの依存関係充足、実行時間、目的のRust成果物が完成することは実Macで未確認なので、成功や時間内適合を仮定しません。未完了ならSIGINTで止め、保存しません。
3. 通常の `mach build -j 3` を240分で止める案もローカルで検討しました。ただし、そのobjdirを即座にupload可能とはしません。予定中断の分類が成立しても、それは「再開候補」でありbuild成功ではありません。全in-flight recipeの主生成物・副生成物・Cargo build-script OUT_DIRの不完全状態が無効化されることを証明する別のgateが必要です。現時点ではこのgateは未成立です。

SIGINT境界では、まずresource/monitor/既存firstFailureとexact childの生存を確認します。signal送信前にstop intentを新規ファイルへwrite/fsyncし、phase、monotonic時刻、現在のlog byte offsetとfile identity、実行cohort、所有generationを記録します。signal送信そのものの成否とraw exit codeも残します。SIGINT後90秒以内に正常な終了処理が完了しなければ、既存TERM/KILL手順で安全に回収しますが、そのobjdirは保存対象から外します。group消滅とleader reapだけで脱離writerの不存在までは証明できません。source/obj/Cargo等の保存rootへ書く所有descendantの追跡が不完全、daemonが残存、権限/状態が不明なら保存不可です。ファイルのサイズが2回一致するだけではwriter消滅の証拠にしません。

cleanup後、閉じたprivate logを先頭からEOFまでstream scanします。stop前offsetまでと全体のbytes/hash、診断の種類と数、完全な安全化済み診断記録、before/after cleanupの既存excerptを残します。raw compiler text、環境全体、token、署名付きURLはcheckpoint/public evidenceへ含めません。raw logはjob内で走査完了まで保持します。公開に残るのは安全化した記録とraw bytesのhashで、raw logの永続保存を約束しません。

途中/末尾の不完全行、走査中の変更、読取り/encoding失敗、未知fatal/error、SIGINTと競合したcompiler診断はUNKNOWNまたは失敗として保存を拒否します。SIGINT前から存在した非zero/firstFailureを予定中断で上書きしません。SIGINT後のmake `Error 1/2`は包括的に許容しません。local prototypeは、送信後offset以降の厳密なmake `Error 130`形と独立したinterrupt記録だけを候補にできる例を試しています。この許容形はGecko/Cargoへの一般化を未承認です。markerなしのexit 2、単なるtimeout、cleanup成功のみから予定中断成功を推定しません。

checkpointの範囲はcompiler/source状態だけです。最終格納サイズ10GiB以下、artifact 1個、`retention-days: 1`、直列の標準 `macos-15-intel` 2 jobsを1回という案を維持します。wrapper/manifest込みの最終上限に収まる格納方式をSHA固定でレビューします。容量超過はupload前に失敗させます。圧縮前logical bytesは別途30GiB、entriesは200000を試行上限案とし、実測値とは呼びません。producerはarchive作成中も8GiB reserveを維持し、consumerは取得/展開に必要なcompressed bytes + logical bytes + 8GiBを確保します。現在の40GiB preflightも緩めません。どちらの上限に収まるかは未知です。

manifestは少なくとも次を固定します。

- Runtime commit/tree、upstream、全30 patches、QA firstCause patch、最終source treeのファイル内容/mode/mtime、fixture/harness、生成branding/configure入力。
- effective configure/substsとconfig.status/生成headerの内容、debug/tests/non-PGO/target/minimum OS、producerのMOZ_BUILD_DATE。
- SDKの正規取得receipt、ZIP/TAR/canonical tree/version、全toolchain archive/binary/version、Python/venv、runner image/system tools。runner labelの一致だけで代用しません。
- source/task/obj/tools/SDK/Cargo/mozbuild/Pythonの実absolute root、全objファイルの相対path/kind/size/mode/mtimeNS/hash、typed external-root references。
- producer run/job/attempt/head/workflow、境界の実exit/outcome、cleanup/全writer終了、log走査receipt、archive bytes/hash、manifest hash、最終artifact ID/service digest。

source/toolchain/SDK/venvは同じ固定内容を同じrootへ再構築します。SDKをcheckpointへ混入して保持期間を延長せず、job2も正規APIで未失効時に取得します。取得後のbuild継続はSDK失効時刻と結合しません。同じcanonical SDKであっても、transport pin変更は別の内容検証/レビューを要します。

consumerはmanifestの自己申告だけを信用せず、レビュー済みexpected pinsとproducer実行の来歴を照合します。artifact ID/service digest/inner archive hashから結び、path traversal、duplicate、device/FIFO、suid、未知hardlinkを展開前に拒否します。外部source/tool/SDK symlinkは、manifestにあるtyped rootとその相対pathだけを、対応rootの内容検証後に作り直します。任意のabsolute linkやsymlink chainを許可しません。hidden .deps/.fingerprint/Cargo stateを含めるか、意図的に無効化するかを明示し、欠落を黙認しません。

空のcurrent-owner staging rootへ展開し、全file内容/modeを照合してからproducerのmtimeNSを復元し、再照合後に期待する空のobj pathへ移します。sourceも内容検証後に元のmtimeを復元します。opaque depfile/DWARF/fingerprintのpathをsedで書換えたり、objectのmtimeを新job時刻へ進めて鮮度を装ったりしません。絶対rootやmtime精度を再現できなければ停止します。archive digest一致だけでは中断された生成物の意味的完全性を証明しません。

job2のstarted/started-monotonic/first-failure/summary/monitor records/所有PID/native registry/profileはすべて新規作成し、古いQA実行状態を展開しません。MOZ_BUILD_DATEはproducer cohort値として別管理します。現pipelineの `objdir-not-fresh` と `.o mtime >= started` を黙って通さず、resume専用のprovenance判定を設けます。成功後の表記は「検証済み状態から2 jobsで再開したfull build」であり、「fresh objdirから同一jobで完了したbuild」とは区別します。

job2では復元とsetupの上限を開始60分、通常full `mach build -j 3` の上限を開始300分、packageを315分、native suiteを335分、cleanup/結果記録を345分とする試行案です。各段階の時間内完了は未測定です。残時間不足やnative未完了をPASSにせず、job2から追加checkpoint/job3を自動生成しません。通常full build、host object/XUL検証、package、許可済みdisposable copyのad-hoc検証、native authentication/cause suite、post-test content bindingをすべて要求します。

将来の最小実装差分は、既存 `support.py` のfull-build限定・明示opt-inの予定停止outcome、`pipeline.py` のproducer/consumer分岐とQA state初期化、固定manifest/archive検査用の小さなhelper、直列2 jobsのworkflowに限定します。既定のsingle-job経路は保持します。新しいgeneric cache、探索fallback key、upload retry、scheduler/frameworkは作りません。予定停止を既存Failureへ流してfirstFailureを書いた後で消す実装も行いません。

ローカルでは `python3 -m unittest -v test_checkpoint_prototype` を実行できます。実makeとC compilerを使い、完成したstable.o→別targetへの部分書込→予定SIGINT→partial target削除→所有group消滅/reap→小archive→空stagingへ検証復元→通常makeと実行成功、stable.oのhash/mtime不変を検証します。early compiler errorが従来tail外にある場合、signal競合error、unknown make error、強制終了、cleanup未完了、log不完全/変更、容量/entry/deadline超過、改変payload/cohort/rootも拒否します。fixture以外のsource/SDK/toolchain取得やcloud mutationは不要です。

この試験はLinux上のGNU make機構と限定fixtureの再開契約を検証します。実Mac/libproc、Rust/Cargo、Geckoの全生成物、absolute rootの別VM再現、30GiB/10GiB実容量、archive/upload速度、2 jobsでの完了は未証明です。実checkpoint実行へのGOは出していません。自然成功したexport/Rust境界の実測、または全中断出力の無効化保証が成立するまでは、任意SIGINT後の実objdirを保存・再開しません。

根拠: [Runtime rules.mk](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/config/rules.mk)、[recurse.mk](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/config/recurse.mk)、[rust.mk](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/config/makefiles/rust.mk)、[Cargo 1.95 fingerprint](https://raw.githubusercontent.com/rust-lang/cargo/rust-1.95.0/src/cargo/core/compiler/fingerprint/mod.rs)、[Cargo build-script OUT_DIR](https://doc.rust-lang.org/cargo/reference/build-scripts.html#outputs-of-the-build-script)、[Cargo paths](https://doc.rust-lang.org/cargo/reference/build-cache.html)。
