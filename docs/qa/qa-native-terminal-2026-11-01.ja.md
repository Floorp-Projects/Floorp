# Mac初回診断の終端と次の修正（2026-11-01 JST）

初回 [run41187440235/attempt1](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/41187440235)、job124497118586 は failure。実行head `3fae099d01b92fcc6bdadf9755e68256f7e67936`、fixture `e9c88fbcbfa98da41989fa8656701cfb380cfd19`、コンパイル元 `7dd619742b25567e479eeda97ae071d10542ad0b`。2026-10-31 09:08:26〜13:33:10 UTC、macos-15-intel、work345分/job360分以内で終端した。再実行はしていない。

## 回収と判定

- 公開artifact13043543646（ZIP18563 bytes、SHA256 `ce576548562e57064e608eb4a55b60f87e56edda11c702c226e40a1d0b30412b`）、暗号化artifact13043848313（ZIP1660210 bytes、SHA256 `b037ac94d1468659e5db151ccfd72effc8205575463b14f4b410724068e04a15`）の取得前後API・実bytes・期限・run/head/attempt/jobを照合した。
- CMS/GCM認証復号に成功し、限定8パスすべてのmanifest・bytes/hashを照合した。復号鍵とraw logは担当Bの私有領域に保持し、共有証拠・repositoryへ入れていない。回収成功はharnessの完全終了を意味しない。
- 公開verifierの最後のstdoutと保存JSONは一致。first failure は `mozilla-upstream-xpcom / mozilla-upstream-proof-incomplete`、elapsed15833.27秒。summary・固定first-failure・公開射影で一致し、そのまま保持した。
- 全45 IDを保持し、正式ledgerは **42 UNVERIFIED + 3 MANIFEST_DISABLED**、executed/passedはunknown。raw scoped streamは91 records、suite_start1/test_start1/test_status3（PASS3）/test_end0/suite_end0。開始したのは `test_KeyedUUIDMapper.js` 1件で、完了・合格には換算しない。
- Debug・意図した失敗・上流version comparatorのsmoke3件は、完全ログと期待exitを確認。59 build stageのexit0/owned group終了と、scoped harnessのexit1/exact reap/group終了は確認。全session/descendant closureとnative-suite cleanupは未認定で、製品認定もfalse。

## 観測した2つの障害

1. **NSS初期化のnative crash**：3つ目のsubtest後に `MOZ_CRASH(InitializeNSSWithFallbacks failed: -5977)`。固定sourceの `nsprpub/pr/include/prerr.h` では `PR_LOAD_LIBRARY_ERROR`。具体的にどのlibrary・依存・配置が原因かは回収範囲だけでは未確定である。アプリ全体の不具合判定へ昇格させない。
2. **診断toolの参照先漏れ**：そのstack出力を処理中、stock `fix_stacks.py` が `cannot find fix-stacks` でharnessを中断。tool自体は既に固定payload（SHA256 `f6039e2b679c7785323dba067f2c08b1d26367480dcd942a09c010ecb5427cc8`）から取得されていた。従来の環境はtoolsをPATHへ入れるだけで、stock wrapperが参照する `MOZ_FETCHES_DIR/fix-stacks/fix-stacks` を指定していなかった。

参照：[stock wrapper](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/tools/rb/fix_stacks.py)、[NSS fallback](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/security/manager/ssl/nsNSSComponent.cpp)、[NSPR error](https://github.com/Floorp-Projects/Floorp-Runtime/blob/7dd619742b25567e479eeda97ae071d10542ad0b/nsprpub/pr/include/prerr.h)。

## 今回の限定修正と検証

`clean_environment(task, tools)` で `MOZ_FETCHES_DIR` を検証済みtoolsへ固定する。stock stack-fixerを無効化せず、tool欠落は引き続き失敗とする。tests/assertions/manifest/plan・NSS実装・SDK・3秒query/3samples・30秒profile・source/FD identity・owned cleanup・345/360分は変更しない。実行済みworkflowと無効prototypeは不変。

固定sourceから原文のstock wrapperをfixture（SHA256 `cfdca169762f2b6c82b7d73566069251ad4da3a260e434b53cefcf0eb5b07071`）として取り込み、子processでlookupを検証した。修正前の同じ例外を再現し、修正後のlookup成功、tool欠落時の拒否、外部MOZ_FETCHES_DIR/GH_TOKENの非継承を確認。portable testの実行ファイルはcat stubであり、Macの実fix-stacks実行やNSS修復の証明ではない。

関連4moduleの隔離回帰は **186件中185成功・従来と同じDarwin専用1skip・失敗0/error0**。新767-ID catalogを全件発見し、旧765 IDsすべてを保持。新sourceは151 hashes（追加fixture1、変更support/test/catalog3、既存147不変）。全767テストは実行しておらず、以前の765件回帰を新sourceの全件成功として数えない。

終端runの元150 source hashesはimmutable fixtureから独立再照合し、以前の765-ID回帰・公開7制御・SDK5制御への結合を保持。SDK13011079529の最新API/producer/job/期限（2026-11-13T21:08:48Z）と保存全ZIPは一致し、同じbytesに対する全49701メンバー監査を再利用した。SDKの新producerは起動していない。

## 次の作業単位

NSSの動的libraryと依存のbuild/install/package closureを固定sourceで照合する。次候補では同OBJDIR由来のlibrary identity・実load診断と、実Mac fix-stacksの動作確認をharness前に置く。完全scoped logが揃う前に42件の正式countsは確定しない。

新nativeは未起動。NSS原因候補と検証する変更・新origin/branch・fixture/pins/catalog・SDK期限・上限・固有証拠を先に記録してから、必要な修正検証を1回ずつ進める。旧runのmanual rerunや同条件の反復はしない。M2/M3と製品の完了条件は未達のまま維持する。
