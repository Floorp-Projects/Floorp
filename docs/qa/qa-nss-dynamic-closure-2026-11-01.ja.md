# M1：NSS動的libraryの生成・配置診断（2026-11-01 JST 06時台）

初回Mac41187440235/attempt1/job124497118586のNSS -5977は未解決。42 UNVERIFIED+3 MANIFEST_DISABLED、正式executed/passed unknown、first failure `mozilla-upstream-proof-incomplete`を保持する。今回の観測器は次のnative条件を具体化する準備で、Macでの実読込みや修復成功の証拠はない。

## 終端照合

前回Floorp CI41213621232/attempt1/head7c2a463dは全5jobs success。Runtime shadow41213637548/attempt1/head0f186067/job124573553999はsuccess、contracts312テスト/exit0、preflight PASS。shadowのnativeQualificationはUNVERIFIED、publicationAuthorizedはfalse。両artifact13047323736/13047606421の取得前後API・完全ZIP digest/size・run/head/期限とlogsを照合した。過去Linux41128672556の失敗と原因未確定の記録は保持する。

SDK13011079529の最新API metadataと保存完全ZIPが一致。producer41128679337/attempt1/head032085d9/job124313694921、期限2026-11-13T21:08:48Z。既存49701member独立監査は同じbytesへ結合して再利用する。新SDK producerは起動していない。

## 固定sourceの観測

対象はコンパイル元Runtime `7dd619742b25567e479eeda97ae071d10542ad0b`。13ファイルのbytes/hashを保存し、30枚の適用patchのhashと変更先を読み、13ファイルと交差しないことを確認した。別のfirst-cause patchは保存summaryの適用hashとplanが一致し、宣言先はMacWebAppService.mm。今回raw patch自体は再取得していない。

- `security/nss/lib/pk11wrap/pk11load.c` の `softoken_LoadDSO` は `libnss3` の場所を基準に `softokn3` を実行時ロードする。
- `security/nss/lib/freebl/loader.c` は `softokn3` を基準に暗号実装DSOをロードする。`secload.c` は同directory、symlink解決後のdirectory、最後に通常の検索経路を試す。
- `security/nss/nss.gyp` の `nss_libs` は `softokn3` と `freebl3` を依存に持つ。一方、linkに使うstatic library集合と実行時ロード対象は同じではない。
- `browser/installer/package-manifest.in` はMac向けにもnss3/softokn3/freebl3を含める。現在のQAの `mozilla_target_build.verify_assets` はこれらを検査していない。
- GYP readerのobjdir規則から `security/nss/lib/softoken/softoken_softokn3/target` と `security/nss/lib/freebl/freebl_freebl3/target` を導出した。実際の固定generated graph内での宣言とshellからの到達性は未観測であり、存在・欠落を先取りしない。

不足DSO、依存先、architecture、signature、探索先のどれが元runの直接原因かは未確定。元runには6箇所のlibrary bytesもdyldの失敗先も保存されておらず、`-5977`だけで具体的なlibraryを確定しない。

## 追加した診断器

`nss_dynamic_closure.observe(task, bundle)` はread-only・単体の準備候補。既存pipeline/公開verifier/実行済みworkflowへは未接続である。

- 同じtask内の固定root.mk/root-deps.mkと、dist/binおよびapp/Contents/MacOSのnss3/softokn3/freebl3、計8箇所だけを読む。
- 既存FD readerの所有者・regular inode・hardlink/symlink拒否・親directoryとpath/FD同一性・最終再照合、30秒/総量64MiB、library各16MiBの上限を継承する。
- bytes/hashとx86_64 dylib header/UUID、欠落/読取不能理由、2配置間の一致を残す。raw bytes、絶対path、環境変数を返さない。symlink配置はmissingと区別して未確認扱いにする。
- generated graphは既存parserで検査してからliteral edgesだけを観測する。make構文を実行せず、期待hashとの一致を別記する。
- `realLoadPerformed/buildPerformed/productQualification/native42ResultQualification` は常にfalse。library存在やhash一致からNSS起動成功を推定しない。

新15件＋既存target12件、計27件を秘密情報なし/networkなし/QA source read-onlyのbwrap環境で全成功。旧767IDs全保持を独立発見し、新catalog782IDs・153source hashesを記録。旧151ファイル中150は不変、変更はcatalogのみ、2sourceを追加。全782テストとMac実観測は未実行。未変更の公開verifier7制御/SDK5制御/既済回帰は再実行・加算していない。

## 次の1単位の条件

新nativeを送る前に観測器を失敗時の保存経路へ接続し、既存source/FD identity、同OBJDIR、345分work/360分job、3秒query/3samples、30秒profile、owned cleanupを維持する。追加資料の失敗で元first failureを上書きしない。

1. 同じjob/OBJDIRでtarget build前後・再配置後のlibrary観測を残す。固定generated graphに動的targetが宣言されているか先に検査する。
2. libraryがbuild closureから漏れている場合の限定target追加案を準備する。宣言のないnode、別OBJDIR・別SDK・prebuilt libraryへの差替えは採用しない。
3. 同じxpcshellでNSS初期化を実際に通るbounded probeと、検証済み実Mac fix-stacksの動作を確認する。hash/headerの観測だけでは実loadに代えない。
4. 新origin/branch・変更条件・fixture/head・全pins/実ID catalog・SDK期限・失敗保存・実行上限・固有証拠を凍結し、必要な全回帰/公開制御が通った場合だけ新初回実行を直列で送る。

今回は新native、manual rerun、workflow変更、build target変更なし。上流tests/assertions/manifest/plan、旧45ledgerとpracticalContinuation、既存workflowを保持する。main/merge/release/required化なし。両既存Draftと共有担当/M1を更新して、同routineの既存wait35f7c47bで次へ引き継ぐ。
