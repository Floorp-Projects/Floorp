# QA309 mozlog independent post-fix review

**Result:** No confirmed residual P0, P1 or P2 in the two reviewed corrections. Independent execution of all 21 test methods passed. A separately authored 36-case fixture produced all 14 expected acceptances and 22 expected rejections. This is a scoped contract review, not native qualification or publish authorization.

Reviewed files in `/workspace/qa-review-v030/repos/Floorp-Runtime` are `.github/workflows/scripts/qa3_mozlog.py` and `test_qa3_mozlog.py`. Both matched `mozlog-final-verification.json` at the beginning and end:

| File | SHA256 |
|---|---|
| qa3_mozlog.py | `670a7ea7cf69db66801ec98d1932136ae8dbe6f1b2ae0bed34063a437eb078ff` |
| test_qa3_mozlog.py | `5625812d4eab52274efa040100155fa376dfe21672bedfc67dc033d3f4e0e063` |

[Independent snapshot](mozlog-independent-static.json) compares all 14 original test methods to fixed #84 `460106115bc76b026c8bf8dad24df7fcb07f97c2`: every AST is unchanged. Seven methods were added. It also independently rereads the exact fixed upstream `structuredlog.py` and `logtypes.py` and matches both QA308 formal-evidence hashes. The author's setup freeze remained unchanged at this check. [End verification](mozlog-independent-end-verification.json) confirms the reviewed two-file freeze remained unchanged during this review.

## Subtest contract

`verify_mozlog` now requires the `subtest` key and accepts only JSON null or strings. Independent actual-file cases accepted null, empty, space, escaped whitespace, Unicode and a NUL-containing string; missing/integer/negative integer/float/boolean/list/dict cases were rejected. This agrees with fixed upstream `test_status`'s required `Unicode("subtest")`: its converter accepts None/string and converts other Python inputs to strings before emission. Rejecting a raw nonstring field therefore does not reject those ordinary converted writer outputs. Empty/whitespace/null were not reclassified as malformed.

The fixed upstream [JSONFormatter evidence](mozlog-independent-writer-contract.json) shows `formatters/__init__.py`'s `JSONFormatter` returning `json.dumps(x) + "\n"`. The earlier exact `formatters/raw.py` read returned path-missing; no broad upstream search or bootstrap was used. Requiring a terminator agrees with this writer, while CRLF remains accepted.

## Bounded reading and budgets

The new loop calls binary `readline(MAX_EVENT_BYTES + 1)` with the actual 1 MiB limit, rejects over-limit bytes before JSON parsing, and requires a trailing LF. Independent files tested LF and CRLF at limit−1/limit/limit+1, UTF-8 at byte boundaries, complete CRLF logs, valid JSON without final LF, partial JSON, CR-only EOF and truncated UTF-8. Outcomes were all expected.

A real 3 MiB event was instrumented through a wrapper around the real binary file. All six reads requested 1,048,577 bytes. The oversized sixth read returned only 1,048,577 bytes and was rejected; only the five preceding complete events reached JSON parsing. This separately verifies the bounded read, rather than merely observing a final size error.

An independently generated lazy stream verified the unchanged 1,000,000-event threshold without writing a large artifact. Exactly 1,000,000 events passed; event 1,000,001 was rejected before parsing, with exactly 1,000,000 parse calls. All reads remained bounded. File-size gate cases 0 / 2 GiB / 2 GiB+1 used synthetic stat metadata; rejected sizes were never opened. This verifies the gate predicate, not physical 2 GiB file processing.

## Evidence and scope

- [Independent suite stderr](mozlog-independent-suite.stderr.log) and [run limits](mozlog-independent-suite.json): 21/21 PASS.
- [Separate fixture](mozlog-independent-fixture.py), [complete observations](mozlog-independent-fixture-results.json), [run output](mozlog-independent-extra.stdout.log), [run limits](mozlog-independent-extra.json): 36 cases, zero mismatches.
- Both executions used own subprocesses, 256 MiB address space, CPU 20 seconds, FSIZE 8 MiB, core dumps disabled, wall timeout 40 seconds and a fresh environment without inherited credentials.

This agent made no source edits, commits, pushes, CI dispatches, native/full builds, browser launches, paid/private runs or publish/signing operations. Only uniquely prefixed `mozlog-independent-*` evidence and temporary fixture trees were written. Production harness emission, other native/log-module changes, issuer authentication and full qualification remain outside this two-file review. No whole-Firefox local search, Searchfox bootstrap or upstream code execution occurred.
