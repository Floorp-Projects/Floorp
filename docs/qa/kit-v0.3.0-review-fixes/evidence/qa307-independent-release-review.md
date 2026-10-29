# QA307: four minimal fixes, independent re-review

2026-10-09 UTC。独立 reviewer `/root/review_release_contracts`。本報告は修正コピーのコードと synthetic fixture に限定する。

**結論: 4 指摘の修正を確認。今回の差分に新たな P0/P1/P2 はない。docs-only Draft PR と Runtime の当該修正のレビュー提出は Go。native/build/dispatch/merge/publish は本レビューの対象外で、実 launch は引き続き No-Go。**

対象と固定点:

- 原本: `/workspace/qa-review-v030/inputs/github-87311566/extracted/Floorp_QA_Implementation_Kit_v0.3.0`。取得元 Floorp commit `87311566a225936fbe7948db142bce3f1d3379d8`、Git blob `e688e2725ce502f36c600cfec88060e7954b30e2`、原 ZIP 150471 bytes / SHA256 `a4af60380399050f82465864734391227ab34024d26bdf16c31559e2824a8d50`。原 ZIP を置換・install していない。
- 修正コピー: `/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0`。コード 5 files の SHA256 と実行前後の一致を [qa307-independent-boundaries.json](/workspace/qa-review-v030/evidence/qa307-independent-boundaries.json) に固定。
- Runtime: `/workspace/qa-review-v030/repos/Floorp-Runtime` の HEAD `120f443af5c257cbdd4712f23b28f9c6fa8c4396` + 未commitの `qa3_mozlog.py` / `test_qa3_mozlog.py`。独立チェック実行前後で同一 bytes。現在の git SHA だけを修正コードの identity として扱っていない。
- 本 reviewer が追加・実行したものは `evidence/` 内の独立テストと証拠のみ。共有 repo やキットのコードは編集していない。network、browser、model、installer CLI、native/build、paid run は実行していない。

## 修正確認

| 旧指摘 | 修正と根拠 | 独立確認 | 判定 |
|---|---|---|---|
| P1: 巨大 attempt が `list(range(...))` の巨大確保を誘発 | `result()` / `evaluate()` が attempt を 1..100 に制限。評価前に `records <= expectedCases * 100`。履歴は実際の番号を sort し `enumerate(..., 1)` と照合する。最大値比例の配列を作らない。[core.py](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/core.py:270) / [schema](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/qa/schemas/result.schema.json:54) | `2**53-1` は AS256MiB の child で BLOCK。attempt100 と全ケース100履歴は PASS、101は拒否。総record上限ちょうどは PASS、+1は QAError。bool/float/文字列も拒否。FAIL→PASS、SKIP、duplicate、history hole は BLOCK。 | 修正済み |
| P1: mozlog のゼロ assertion / retry / child failure / unknown action を PASS | `import_mozlog()` は非空かつ重複なしの `suite_start.tests` を resolved plan と照合し、各 required test に非空文字列 subtest と assertion を要求。retry/rerun/TEST-UNEXPECTED、非zero child exit、未対応 action、leak/assertion diagnostic は非PASS。child exit は exact `int` が必須。[core.py](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/core.py:326) | 4 original bad trace と typed-exit負例は全て拒否。suite selection 欠損/空/重複/foreign/extra、assertion欠損/空白/非stringも非PASS。grouped selection、正しい int0 + assertion を含む trace は PASS。入力 bytes / line / event の境界も下記確認。 | 修正済み |
| P1: Agent の partial stdin が seconds を超えて blocking `readline()` | `ActionReader.read(deadline)` は nonblocking fd、bounded `os.read`、単一 absolute deadline と最大65536-byte frameを使用する。`execute_campaign()` の finally で fd blocking mode を復元し、Driver.close を実行する。[agent.py](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/agent.py:81) / [campaign](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/agent.py:234) | fake Driver + `'{', EOFなし, seconds=1` は `budget-exhausted`、Driver.close/元blocking復元。2 pipelined frame を両方処理。clean EOF/partial EOF と slow partial byte stream を確認。65536 bytesは受理、65537は拒否。実browserを起動していない。 | 修正済み |
| P2: PR84 child exit 欠損 / False / 0.0 が int0 と同じ扱い | `verify_mozlog()` は `type(entry.get("exitcode")) is not int or entry["exitcode"] != 0` で拒否。[qa3_mozlog.py](/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts/qa3_mozlog.py:136) | 欠損、False/True、0.0/1.0、None、"0"、1を拒否。int0正常traceは PASS。既存 expected canary と unrelated failure の分離も維持。 | 修正済み |

## 回帰固定化の実効性

主担当の修正前実行ログを確認した。kit 13 methods の修正前実行は failures16、Runtime 1 method は欠損/False/0.0の3 subtestsが失敗している。kitには巨大attemptの MemoryError、partial stdinの外側timeout/SIGKILL、pipelined frameの取りこぼし、4 bad mozlogの誤PASSが記録されている。単なる setup failure を修正実証として扱っていない。

- [qa307-kit-regressions-first-failure.log](/workspace/qa-review-v030/evidence/qa307-kit-regressions-first-failure.log)
- [qa307-runtime-regressions-first-failure.log](/workspace/qa-review-v030/evidence/qa307-runtime-regressions-first-failure.log)
- [qa307-first-failure-verification.json](/workspace/qa-review-v030/evidence/qa307-first-failure-verification.json)

元114 test methods の AST は全て同一だった (`test_core.py`61 + `test_design.py`39 + `test_extensions.py`14)。`test_core.happy_events()` の変更は suite_start に正しい selection を加えた1行だけで、既存テストの削除・skip・assertion緩和はない。現在の新規回帰は16 methodsで、attempt100の正常側、grouped selectionの正常側、サイズguardも固定する。[test_review_regressions.py](/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0/tools/qa/tests/test_review_regressions.py:71)

独立実行は **39 tests / 2.188s / exit0 / failure0 / error0 / skip0**。

| 内訳 | 件数 | 内容 |
|---|---:|---|
| evidence-only独立 boundary fixtures | 9 | limitsの正常側と異常側、FAIL/SKIP/hole維持、assertion/selection拒否、slow partial deadline |
| kit新回帰 | 16 | 巨大attempt、4 mozlog bad traces、typed exit、selection、Agent期限/EOF/2frame |
| Runtime mozlog fixture | 14 | typed exit修正、exact case、retry/skip/leak、canary、raw completeness |

実行証拠: [qa307-independent-boundaries.log](/workspace/qa-review-v030/evidence/qa307-independent-boundaries.log)、[JSON](/workspace/qa-review-v030/evidence/qa307-independent-boundaries.json)、[独立テスト](/workspace/qa-review-v030/evidence/qa307_independent_release_tests.py)。allowlist環境、AS2GiB、CPU20s、単一file16MiB、wall40sで実行。partial-input / huge-attempt fixtureのchildは追加のAS256MiB / CPU8s / file8MiB / own-group外側3s制限を使用する。Agentはfake Driver、stdinのみ。

主担当の broader runs も raw log / exitを照合: kit最新130 (114+16) PASS、元114 PASS、Runtime162 PASS。独立39と足し合わせてtest数を水増ししない。

- [qa307-kit-all-after-boundary-tests-2.json](/workspace/qa-review-v030/evidence/qa307-kit-all-after-boundary-tests-2.json): 130 / 2.292s / exit0。
- [qa307-kit-original114-after-fix-1.json](/workspace/qa-review-v030/evidence/qa307-kit-original114-after-fix-1.json): 114 / exit0。
- [qa307-runtime-all-corrected-outer-both-limits-3.json](/workspace/qa-review-v030/evidence/qa307-runtime-all-corrected-outer-both-limits-3.json): 162 / exit0。初回と第2回の失敗logも保持。初回はouter hard AS2GiB、第2回もouter hard FSIZE32MiBが、既存 `run_owned` child要求のAS28GiB / FSIZE50GiBと衝突。診断の `prlimit: Operation not permitted` と、そのenvelopeのみを修正した成功を照合した。コードの不具合やfirst FAIL消去として扱わない。

## 制限値と設計への影響

| 制限 | 確認方法と結果 |
|---|---|
| attempt 100 / 総record `expectedCases*100` | 本物の上限値で inclusive PASS と +1拒否を確認。上限はreadinessやretry許可ではなく、入力拒否の上限。初回非PASSを後のPASSで消せない。 |
| mozlog 128MiB / 1,000,000 events | 定数を静的確認。小型fixtureでcapを実サイズ・5eventに束縛し、exact-boundary PASS / +1相当拒否を確認。実128MiBや100万eventのstress実行はしていない。 |
| mozlog line 4MiB | 実4MiBの1行をPASS、+1 byteをdecode前に拒否。`readline(limit+1)` は全行を無制限に確保しない。 |
| Agent frame 65536 bytes | newlineを含め実境界値を確認。deadlineはfragmentごとに延長せず、slow inputでも期限で戻る。 |

追加guardはいずれも4指摘を閉じる入力/記録契約であり、三層分担、source/native coverage、policy/profile、最終artifact binding、publication gateの変更はない。`evaluate()` の `publicationAuthorized=false`、campaignの `releaseEvidence=false` は保持される。新規コードは publisher、credentials、workflow trigger、OS worker隔離を追加していない。キットをFloorpへinstallしない提出方法と整合する。

これは単一ローカルplan内の履歴検証であり、cross-run first FAIL、資格のissuer/trust/expiry/revocation、manual UI publish前の機械的gate、MAR/DEB/Portable coverage、Debug/native実証の旧課題を解決したという判定ではない。Agent起動/action自体をpreemptする全体watchdogやOS隔離も今回のstdin修正だけでは実装されない。実native前の別契約/検証を維持する。

## コードidentity

| ファイル | SHA256 |
|---|---|
| kit `tools/qa/core.py` | `28767090836801f51e18cfef59434a9ae0cedc44c15cfc21f0da7160d43d3bdc` |
| kit `tools/qa/agent.py` | `90fc2e506263ed2004496411caaf8c0bccc960a9a46004df8382d8af9c3b695b` |
| kit `tools/qa/tests/test_core.py` | `5bb6a9a510108b1c13eb4588278c98bc9e9139a34fd7758e1ea4bcad8985f790` |
| kit `tools/qa/tests/test_review_regressions.py` | `31a77bad47938f862f981ca5f530c382d5e3bd8682a78a697bd4c7a881842910` |
| kit `qa/schemas/result.schema.json` | `c2df35c7f2eb93103328264468fdee7e669aa69b0b69d1ba387f92989571c4d4` |
| Runtime `qa3_mozlog.py` | `bea848caac52c0d1025b2ef586c145a2fb89ede65b77767764eac724eddb4e1c` |
| Runtime `test_qa3_mozlog.py` | `1ed4356fa3734058825499d49f537ad6ed25fa4f0f53d0b10ee01a79765d0ec8` |

このidentityは提出ZIP/patch組立前のコード判定。README repair note / metadata / MANIFEST / 修正版ZIPの完成検証は主担当が別証拠として記録する範囲。生成された `.ruff_cache` はZIPに含めず、原本ZIPと修正版ZIPを区別する必要があることを主担当へ通知済み。本レビューで原本を修正版と再ラベルしていない。
