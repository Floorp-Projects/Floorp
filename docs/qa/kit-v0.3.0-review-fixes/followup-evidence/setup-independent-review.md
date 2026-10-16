# QA309 setup 修正の独立 post-fix review

2026-10-09。レビュー担当 `/root/review_release_contracts` はこの setup 修正の作者ではない。対象は `/workspace/qa-review-v030/worktrees/Runtime-shadow-fixes`、base `81c92fc4bbbc13b4fd4c2a2adaf6a137e342bd53` 上の未commit差分。repository source を変更していない。

**この修正範囲に新規 P0/P1/P2 は確認せず、Draft 提出へ Go。** 作者による lint 最終結果および主担当の統合検証と合わせて利用する。この判定は native/build/資格認定/公開を許可するものではない。

## 固定 byte と検証

| ファイル | SHA256 |
|---|---|
| `qa3_build_profile.py` | `e7f84f39d36355074e15e3d192249cd5adde954110fc5f820b2bda6c47dbc00a` |
| `setup-floorp.sh` | `9bf55ac325397d76d3d162e681ba2bc47facc8b2ab61e68c2646e2d1b95f651c` |
| `test_qa3_build_profile.py` | `5b8452881d0107ea3f19643e1d36d380fd718753b9b9e51e440dc5642ded9c96` |

[初期 snapshot と immutable copies](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-initial-snapshot.json)、[最終 SHA/旧 test AST 照合](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-final-snapshot.json)。各 run の直後・最終確認で3ファイルの SHA 不変。

* 作者の専用 unittest を独立 bounded subprocess で再実行: **12 methods / 3.379 seconds / exit 0 / skip 0 / OK**。[完全な unittest stderr](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-unit-01.stderr)、[argv/上限/結果](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-unit-01.json)。unittest は stderr に出力するため同名 `.log` は空であり、証拠欠落ではない。
* 別の独立 fixture で base の実 `setup-floorp.sh` と修正版を同じ stub 環境で実行し、**26 組の generated mozconfig 全文 byte 一致**: canonical Debug 5、production-opt 5 + production PGO generate/use 8 = production 13、legacy Debug+PGO 8。[全 mozconfig/effects の raw JSON](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-fixtures-03.stdout)。値を比較する helper のみのテストではなく、双方の実 shell script が実行されている。
* 同じ fixture の追加負例は raw CLI 8 組と strict Python API 7 組。全41診断（26比較＋8 CLI拒否＋7型拒否）を保存した。[fixture code](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-fixtures.py)、[最終 run receipt](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-fixtures-03.json)。追加の2つの初期失敗も別名で保持し、成功へ上書きしていない。
* scoped `git diff --check`: exit 0。[raw](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-diff-check.log)。旧7 methods のうち6件は AST 同一。旧 legacy-use の4 targetケースは維持し、generate/use 8ケース、explicit legacy flag、非資格化の判定へ拡張された。5 methods追加、削除や期待値緩和は無い。

## 確認した契約

`setup-floorp.sh` は argument count 4–7 を検査し、platform/arch/debug/pgo/mode/artifact の同じ `PROFILE_ARGS` を preflight と生成後検査へ渡す。preflight は script 内の `cd`、Debug patches、mozconfig/branding copy、APT/Rust/bootstrap より前。Windows ARM、不明 target、Linux ARM PGO generate/use、non-PGO の profile指定、use の profile欠落、generate の artifact指定、未指定/不正mode、raw不正boolean を拒否する。追加 raw CLI 負例も config未作成・effects無し・branding未copy・update marker不変だった。

`qa3_build_profile.py::profile` は Python bool を `type(...) is bool` で限定し、legacy policy flag も同様。mode/artifact は文字列を要求する。`main` は argparse の boolean converter と `profile(...)` を通してから `enable_debug_tests` / `verify_config` を実行する。`--validate-inputs` と `--config` は相互排他的。configを書き直すのは期待値検証成功後だけで、target/Debug/profile/tests の不一致を例外で停止する。

最終 config 検査は **生成・patch/copyの後、APT/Rust/bootstrapの前**。wrong target、余分な Debug/profile、ambiguous tests の実 shell 負例はこの地点で非成功になり、sudo/rustup/bootstrapを呼ばない。入力自体の拒否と、生成物検査の拒否の位置を混同しない。

canonical Debug 5 targets は testsOverride true、明示 `--enable-tests` が1つ。production-opt/PGO 13、legacy Debug+PGO 8 の全文が base と一致し、既存 tests flags は保持される。use の `export MOZ_LTO=cross`、`--enable-profile-use=cross`、profdata / jarlog paths も保持され、欠落負例は拒否される。Linux ARM production-opt は含まれ、Linux ARM PGO は不可。

legacy Debug+PGO は helper の既定では拒否し、shell の既存 Debug+PGO 組合せだけが明示 `--allow-legacy-debug-pgo` を渡す。role は `legacy-debug-profile-*`、testsOverride false、qualificationEligible false、preflight stderr に非canonical/資格不適格を示す。legacy を canonical Debug へ昇格していない。canonical の eligibility flag も実行証拠や発行済み資格ではない。

## 現行 caller との整合と限界

known paths のみを照合した。`common-build.yml` の setup 呼出は platform/arch、`toJSON` 済み debug/pgo、mode/artifact、resolved BuildDate の7 positional arguments を渡し、新たな platform/arch 必須 CLI と整合する。`qa3_build_profile.py` の config CLI は setup 内2地点でのみ使用され、旧 `--config --debug --pgo` 形式を残す repository caller はこの範囲では見つからなかった。

Windows wrapper は x86_64、Linux は ARM normal を pgo false へ固定し x64 generate/use を別jobへ分け、Mac は2archについて pgo false 時は空mode、true時はgenerate、その後artifact付きuseを渡す。既存13 production/8 legacyルートに必要な mode/artifact を新 guard が不正扱いする不整合は確認していない。

「早期」は **setup script 自身の入口**についての確認である。既存 common workflow は setup script より前に checkout、上流patch、Rust/swap/profile download 等を行う。新変更を workflow 全体の admission gate として認定しない。optional 第7引数 BuildDate の新しい standalone validator も今回追加されていない。現行 caller は別の context resolver と common-build の14digits検査を経てこれを渡す。この既存境界を新規P2として追加計上しない。

`verify_config` は generated mozconfig の文字列検査で、返す証拠にも `configureExecuted:false` / `nativeTestsExecuted:false` / `publicationAuthorized:false` が明示される。profdata/jarlog **実bytes**、toolchain、effective configure、実 compile/harness、Debug と製品の制限経路、全OS製品資格はこの修正・fixtureで実証していない。

## 初期失敗の保存と安全範囲

独立 fixture 初回は Git の大きい pack mmap が外側 AS512 MiB に抵触し exit128。第2回は私の `TRUE` 負例が作者 helper の `str(debug).lower()` で `true` に正規化され、誤った fixture expectation が失敗した。[初回 stderr](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-fixtures-01.stderr)、[第2回 stderr](/workspace/qa-review-v030/fix-qa309/evidence/setup-independent-fixtures-02.stderr)。setup defect として扱わない。

Git へ一回限りの `-c core.packedGitWindowSize=1m -c core.packedGitLimit=64m` を指定し、ASを緩めず読取りを実施した。Git configを変更していない。boolean負例は explicit raw arguments に変更し、第3回は41診断/4.044 seconds/exit0、stderr空。各 version の失敗/成功結果をそのまま保持した。

全fixtureは資格情報無し allowlist env、AS512 MiB / CPU20 s / FSIZE8 MiB / wall45 s、独立 process group。`git`/`sudo`/`rustup`/`mach` の **setupが呼ぶ副作用**は一時 synthetic tree 内の記録stubである。baseline sourceの取得に使う実 `git show` は固定 commitのread-onlyで、前述の上限を使う。実 apt/Rust/bootstrap/native/browser/build、ネット、workflow dispatch、paid run、source edit、commit/push は無い。AGENTS/mozlint を確認し、上流検索を他手段へ迂回していない。

この setup 修正の独立レビューは完了。主担当の統合 lint/tests と独立 native cleanup review を代替せず、それらの結果と合わせて Draft/通常CIの次段階へ進める。
