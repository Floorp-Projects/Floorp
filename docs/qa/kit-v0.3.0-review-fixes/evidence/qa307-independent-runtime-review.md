# QA307 限定修正の独立再レビュー

承認済みの kit 3 件と Runtime PR84 1 件について、**Draft 提出は Go** と判断する。この差分内に新しい P0/P1 blocker は確認していない。部分 stdin の期限、mozlog の誤成功、巨大 attempt に比例する割り当て、Runtime の child exitcode 型という 4 件は、修正前の反例と修正後の回帰に結び付いている。native qualification は引き続き **UNVERIFIED / No-Go**、`publicationAuthorized=false` である。先行レビューの source-cohort 規範・認証契約は既知の残件であり、今回閉じた指摘に含めない。

対象は元 kit `/workspace/qa-review-v030/inputs/github-87311566/extracted/Floorp_QA_Implementation_Kit_v0.3.0`、修正版 `/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0`、Runtime `codex/qa3-native-proof-contracts`。再レビューは HEAD `120f443af5c257cbdd4712f23b28f9c6fa8c4396` 上の未 commit 2-file 差分で行った。主担当の local follow-up commit `460106115bc76b026c8bf8dad24df7fcb07f97c2` へ、レビュー済み 2 file の SHA256・bytes が完全一致することを追加確認した。現在の Runtime worktree は clean。[commit 結合記録](/workspace/qa-review-v030/evidence/qa307-independent-runtime-commit-binding.json) の SHA256 は `2dbacc8f1f57ad9c3ccac7ad3180dc38dbebe89a321488249a7e48120b20ad92`。

元 ZIP は Floorp commit `87311566a225936fbe7948db142bce3f1d3379d8`、SHA256 `a4af60380399050f82465864734391227ab34024d26bdf16c31559e2824a8d50`。読取可能ファイルと証拠ログを [review 時点 snapshot](/workspace/qa-review-v030/evidence/qa307-independent-review-snapshot.json) に固定した。snapshot 自体の SHA256 は `ac7471648bbc44ee56c57ca16607e5a6292a128e85580aec48b51d3d54c4ce0b`。主担当が組み立てた修正版 ZIP は 158581 bytes、SHA256 `60ad9a63c9fa7e328cc2da41187c6b725a64ee6144aaac8f66c2ebc3260710f2`。safe fresh expansion、60 entries / 59 manifest entries、patch roundtrip、fresh expansion 上の 130 tests 成功は [主担当 archive 検証](/workspace/qa-review-v030/evidence/qa307-archive-validation.json) による。この担当は展開済み入力の 59 manifest entries と修正コードを独立照合した。

## 修正判定

以下の指摘 ID は今回の修正版 `REVIEW_FIXES.md` と主担当の管理 ID に合わせた。

|指摘|確認した実装・検出力|判定|
|---|---|---|
|KIT-P1-01 巨大 attempt とメモリ|[core.py:31](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/core.py:31) の上限 100 を生成・受入れに共通使用。`evaluate` は [288 行](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/core.py:288) で record 数を case 数×100 に制限し、299 行で各 attempt を制限、313 行で実際の履歴を列挙する。最大 attempt までの `list(range(...))` は除去された。schema の maximum も 100。2^53−1 は 256 MiB の専用子プロセスで BLOCK、全履歴 100 は PASS、101 は BLOCK。|修正済み|
|KIT-P1-02 mozlog 誤成功|[core.py:356](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/core.py:356) で非空・重複なし selection と resolved plan を比較。383–384 行の subtest 検査・計数と 413 行の positive assertion 要求により zero-assertion trace を成功にしない。393–396 行で整数の child exitcode 0 を要求し、402–408 行で retry/rerun/TEST-UNEXPECTED・未対応 event を成功にしない。128 MiB file、4 MiB line、100 万 event の読取上限を追加。正しい単一 suite、grouped selection、typed zero の正例は通過する。|修正済み|
|KIT-P1-03 Agent 部分入力と期限|[agent.py:81](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/agent.py:81) の `ActionReader` は fd の元 blocking flag を保存し、nonblocking `os.read`、最大 65,537 bytes の収集 buffer、LF を含め最大 65,536 bytes の frame を使う。毎 loop が同じ absolute deadline を参照する。buffer 内の後続 LF を保持し、部分 EOF は error、空 EOF は input-ended。294 行からの finally で fd flag を復元し、299 行から Driver を cleanup。独立した実 pipe/FakeDriver 6 ケースで期限と cleanup を確認した。|修正済み|
|PR84-P2-01 child exitcode 型|[qa3_mozlog.py:136](/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts/qa3_mozlog.py:136) は `type(entry.get("exitcode")) is int` と `exitcode == 0` を同時に要求する。missing、False、True、0.0、1.0、None、"0"、1 の 8 負例と整数 0 の正例を [test_qa3_mozlog.py:132](/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts/test_qa3_mozlog.py:132) が実 parser/file trace で検査する。実装は 1 行の変更、test は 1 method の追加だけ。|修正済み|

Runtime の型検査は pinned 一次ソースと整合する。R `120f443af5c257cbdd4712f23b28f9c6fa8c4396` の [structuredlog.py:644](https://github.com/Floorp-Projects/Floorp-Runtime/blob/120f443af5c257cbdd4712f23b28f9c6fa8c4396/testing/mozbase/mozlog/mozlog/structuredlog.py#L644) は `process_exit` に default のない `Int("exitcode")` を要求し、[logtypes.py:264](https://github.com/Floorp-Projects/Floorp-Runtime/blob/120f443af5c257cbdd4712f23b28f9c6fa8c4396/testing/mozbase/mozlog/mozlog/logtypes.py#L264) は writer 側で `int(data)` に正規化する。JSON event 消費側が欠落を 0 と補う根拠はない。これは R の exact file/blob 確認であり、ローカルにない U オブジェクトまで独立照合したという意味ではない。[一次ソース hash・blob 記録](/workspace/qa-review-v030/evidence/qa306-mozlog-process-exit-static.json) を保持している。

attempt 上限は資源 ceiling であり retry を許可するものではない。[core.py:315](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/core.py:315) は履歴中の非 PASS を保持している。suite selection、positive assertion、typed zero を満たすだけで信頼できる実行者を証明できるとは主張しない。mozlog は未対応診断や repeat/verify を引き続き拒否するので、上流 native harness との実互換性には別の検証が必要である。

## 独立 fixture とテストの意味

この担当で実行した追加検証は [独立 FakeDriver fixture](/workspace/qa-review-v030/evidence/qa307-independent-reader-fixtures.py) の 6 ケースだけ。実 Driver を生成する前に FakeDriver に置換し、実 browser・native harness・network・model は実行していない。自分の専用子プロセス group のみを管理し、AS 256 MiB / CPU 8 秒 / file 8 MiB / outer 3 秒、資格情報を含まない環境を使用した。

|入力|観測結果|
|---|---|
|`{` を pipe に書き、writer を開いたままにする|1.002 秒で budget-exhausted、outer timeout なし、completedSteps=0|
|空 EOF|input-ended、completedSteps=0|
|`{` の部分 EOF|finding-or-environment-error、Unterminated action frame at EOF|
|observe 2 行を一度に pipe に書く|completedSteps=2、取りこぼしなし|
|元から nonblocking の fd に observe 2 行|completedSteps=2、元の False を復元|
|65,536 spaces + LF|Action input too large、completedSteps=0|

全 6 ケースで FakeDriver.close が呼ばれ、fd blocking flag は元の値へ戻った。全ケースの `releaseEvidence=false` を確認した。ここでの fixture PASS は期待した入力終了・拒否を検出した意味であり、campaign の製品資格成功を意味しない。[結果 JSON と各 raw log](/workspace/qa-review-v030/evidence/qa307-independent-reader-results.json) に保存した。

追加の [test_review_regressions.py:31](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/tests/test_review_regressions.py:31) も静的に確認した。TempDirectory 内の合成データと FakeDriver を使い、own child のみを timeout/kill する。巨大 attempt の fixture は 256 MiB の子プロセスへ隔離され、実 process/binary の探索や browser、install、network、credential への新しい呼出しはない。既存 114 test method の AST は変更されていない。`test_core.py:73` の happy trace に suite_start selection を付けた fixture 修正は、新しい要求に対して正しい正例を維持する変更である。この点は別担当の [39 境界検査・AST 比較](/workspace/qa-review-v030/evidence/qa307-independent-boundaries.json) とも一致する。

修正前の [kit 初回失敗ログ](/workspace/qa-review-v030/evidence/qa307-kit-regressions-first-failure.log) は、partial stdin の own child が outer timeout で −9、連続 2 行が 1 step、zero assertions/retry warning/child exit 1/unknown action が PASS、巨大 attempt が MemoryError になることを記録している。修正前でも typed zero の正例は成功している。Runtime の [初回失敗ログ](/workspace/qa-review-v030/evidence/qa307-runtime-regressions-first-failure.log) は 8 負例中 missing/False/0.0 の 3 subcase を検出した。したがって回帰は修正後の分岐を単に写したものではなく、元の誤受理・停止を検出し、正常例を残している。

主担当の最終ログ・実行 receipt を読取り照合した。以下の全体検査はこの担当で再実行していない。

|検査|最終結果|根拠|
|---|---|---|
|修正版で元 114 tests|114 / 0.535 秒 / exit 0 / skip 0|[log](/workspace/qa-review-v030/evidence/qa307-kit-original114-after-fix-1.log)|
|修正版 kit 全体|130（追加 16）/ 2.292 秒 / exit 0 / skip 0|[log](/workspace/qa-review-v030/evidence/qa307-kit-all-after-boundary-tests-2.log)、[receipt](/workspace/qa-review-v030/evidence/qa307-kit-all-after-boundary-tests-2.json)|
|Runtime 全体|162（追加 1 method / 8 bad-value subcases）/ 7.346 秒 / exit 0 / skip 0|[log](/workspace/qa-review-v030/evidence/qa307-runtime-all-corrected-outer-both-limits-3.log)、[receipt](/workspace/qa-review-v030/evidence/qa307-runtime-all-corrected-outer-both-limits-3.json)|
|Runtime targeted mach Ruff|exit 0 / 0 problems|[log](/workspace/qa-review-v030/evidence/qa307-runtime-mach-lint.log)|
|Runtime 全 modified-file mach lint|BLOCKED。sparse checkout の `third_party/node/pnpm-lock.yaml` 欠落で pnpm-audit 登録が失敗|[log](/workspace/qa-review-v030/evidence/qa307-runtime-mach-lint-all-modified.log)|

Runtime 全体検査の初回は 1 failure + 3 errors、その後の AS 補正だけの実行も失敗した。未変更 `run_owned` が子プロセスへ設定する AS 28 GiB / FSIZE 50 GiB に対し、外側 hard AS 2 GiB / hard FSIZE 32 MiB が低く、`prlimit` が Python 子プロセス開始前に Operation not permitted となっていた。[AS 独立診断](/workspace/qa-review-v030/evidence/qa307-runtime-limit-diagnostic.json) と [FSIZE 独立診断](/workspace/qa-review-v030/evidence/qa307-runtime-fsize-diagnostic.json) が同じ原因を示す。最終実行は outer soft AS 2 GiB / hard AS 32 GiB、soft FSIZE 32 MiB / hard FSIZE 64 GiB へ補正し、既存 worker の physical memory 制限を併用した。対象コードやテスト期待値は変えていない。初回 raw log は残っているため、この検査 envelope の訂正を native 初回失敗の成功置換として扱わない。

## 差分と残る条件

修正版 kit の全 59 manifest entries を実ファイルに再照合した。差分は `agent.py`、`core.py`、result schema、happy fixture、追加 regression test、および修正版 README/REVIEW_FIXES/validation/MANIFEST。元 DESIGN と 14 文書は byte 単位で同じである。Runtime の review 時点 status は `qa3_mozlog.py` と `test_qa3_mozlog.py` の 2 modified files だけで、follow-up commit も同じ 2 path だけを含む。workflow・producer・公開 gate は変更されていない。ソースの変更や commit はこの担当では行っていない。

[REVIEW_FIXES.md](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/REVIEW_FIXES.md) は、同一 U だけで実行証拠を再利用できないこと、元 `docs/12` が修正済み規範文書ではないこと、native qualification と publish 権限がないことを明記している。この注記は残件を明確にするもので、U/R/P/transforms/B/D/F、build/target/run/attempt、producer/controller の認証や first-failure history の結合を実装した証拠ではない。

実 Driver の startup、observe/action、shutdown は campaign 入力とは別の timeout と処理を残している。stdout の消費や browser/helper の停止まで含む whole-campaign hard deadline、OOM/disk/log 上限、全 child の cleanup を今回の ActionReader だけで保証しない。専用 worker、外部 supervisor、秘密情報のない HOME、UID/cgroup/通信権限分離の配備と実測が必要である。Windows Agent は未対応のまま。機種や runner label の選択だけではこの契約を満たさない。

cold CLI/dependencies/offline wheels、実 Mozilla harness の canary・skip・初回失敗の保存、controller-owned source/material/receipt の結合、全 OS/native target と Linux ARM production-opt、Debug と最終署名製品 artifact の分離・再照合は未実証である。`bundle → final QA → manual approval → 証拠再照合 → publish` と既存 gate を保持する必要がある。今回の限定修正は Draft として提出できるが、native 実行・資格 gate の有効化・公開の根拠にはならない。

重い build、native/browser 実行、network/model 呼出し、CI dispatch、署名・公証・公開、インフラ・資格情報変更はこの担当で行っていない。
