# QA309 portability and independent-test guardrails

Prepared 2026-10-09 UTC, read-only, before the six fixes are frozen. This is a test plan, not an independent fix verification result. No author files, commits, pushes, workflow dispatches or cancellations were changed. Existing ordinary public GitHub-hosted autostart CI is allowed; new paid Blacksmith/private Debug runs, permissions, signing and publication stay outside this task.

Current inputs: Runtime setup worktree /workspace/qa-review-v030/worktrees/Runtime-shadow-fixes; fixed Runtime #84 head 460106115bc76b026c8bf8dad24df7fcb07f97c2; fixed Floorp #2889 head 718e688fbd7ad88a776534e7778ad8e4a7f67364. Read worktree AGENTS and the custom .github scripts; no upstream discovery was needed. Searchfox remains unavailable, so no broad upstream claims.

## Setup/configuration matrix

| Input class | Targets | Expected result |
|---|---|---|
| debug=true, pgo=false, empty mode/profile | Linux x64/ARM, Windows x64, Mac Intel/ARM | canonical-debug, exactly one active --enable-tests, no active --disable-tests, no profiling flags, qualification eligible in profile metadata only |
| debug=false, pgo=false, empty mode/profile | All five | production-opt, preserve each template's active tests options; no new profiling inputs |
| debug=false, pgo=true, generate, no profile | Four non-Linux-ARM targets | profile-generate=cross; preserve template tests; no profile-use/profdata/jarlog inputs |
| debug=false, pgo=true, use, nonempty profile | Four non-Linux-ARM targets | profile-use=cross, MOZ_LTO=cross, one profdata and one jarlog option; preserve template tests |
| debug=true plus each supported PGO mode | Four non-Linux-ARM targets | legacy-debug-profile-generate/use, preserve existing tests/profiling behavior, qualificationEligible=false |
| unsupported target/mode/type/profile combinations | real setup script | fail before patch application, mozconfig copy, package installation, rustup, application metadata rewrite and mach bootstrap |

Important fixed-template detail: linux-aarch64.mozconfig has a commented --disable-tests and no active tests option. Its production-opt fixture must preserve that empty set, rather than add --disable-tests uniformly.

Suggested real-entry invalid matrix: Windows ARM; unknown platform/architecture; invalid debug/pgo boolean; Linux ARM PGO generate/use; non-PGO mode/profile; unknown PGO mode; use without profile; generate with profile. Side-effect stubs should log calls and reject any invocation in these invalid cases. A guard marker/mozconfig/application.ini should remain byte-identical or absent as appropriate.

Do not treat resolver-only profile() rejection as real setup input validation. Final generated config should be checked against the same resolved expectation. For production and PGO, compare against fixed-template behavior, not just "script exited zero." The generated matrix has configureExecuted=false/nativeTestsExecuted=false; it is not five OS builds.

## Process cleanup corpus (Runtime and Floorp)

- Keep success with complete stdout/stderr, burst/cumulative log caps, separate decoded-data budget, closed stdout/stderr with a still-live process, timeout/disk-stop, setsid daemon, and an unrelated pre-existing child.
- Inject EIO, EDQUOT and ENOSPC in the normal drain and again during cleanup drain. A TERM-ignoring own child must still receive KILL, be reaped, and leave no own descendants. Keep original failure cause and non-success receipt if its destination remains writable.
- Exercise failure immediately after successful Popen: nonblocking fd configuration, selector construction/registration and sink open/setup. An initialization exception must not bypass ownership cleanup.
- Check cancellation/exception paths separately from exit-zero paths. Assert bounded termination and owned PID plus birth identity; do not kill/reap unrelated pre-existing children.
- If the fix uses Linux subreaper state, check whether it is restored, or isolate tests in a dedicated process and document this requirement. Repeat sequential commands to catch stale ownership state.
- For source execute(), verify a setsid grandchild on leader exit zero and failure/timeout. Preserve complete host output and exit semantics.
- Fixture supervisor must use a short hard wall deadline, low process/resource bounds, and an independent finally that reaps only fixture-owned processes. Capture failures as well as successes.

These synthetic tests do not prove an external launcher, cgroup escape resistance, whole machine cancellation, Windows/macOS cleanup, or native browser behavior.

## Mozlog field and bounded-reader regressions

- Reject missing subtest key and non-Unicode-writer JSON types (list, dict, number, bool).
- Preserve formally writer-emittable subtest=None, empty string and whitespace string for ordinary PASS events. A stricter nonempty-name rule would be a separate policy change.
- Canary still needs exactly one FAIL/expected PASS with the exact canary reason in test_status, exit integer 1, complete suite/test lifecycle and positive assertion evidence. Invalid field/type cannot become an expected rejection.
- Keep process_exit typed-integer zero checks (reject False, 0.0, missing, strings); reject skip, empty discovery, unknown IDs, retries/recovered first failure, duplicate execution, crash/leak, incomplete lifecycle, unrecognized event and trailing error.
- Use a small fake binary reader that fails if unbounded iteration/read is attempted. Assert requested readline bound no greater than line limit+1. No multi-GiB allocation is needed.
- Boundary cases: legal LF-terminated line at maximum byte size; one byte over; final unterminated/partial JSON; invalid UTF-8, CRLF as supported by current JSON contract; event-count overflow via a generated reader rather than giant file. Preserve exact byte/event accounting.
- Whole raw-file cap and per-line bound are distinct. Do not weaken the existing 2-GiB total or 1,000,000-event policy while fixing allocation order.

For source JUnit, retain exact identities, aggregate contradictions, duplicate/skip/failure rejection. Test UTF-8 and UTF-16 DOCTYPE/ENTITY inputs and valid ordinary JUnit. An explicit UTF-8-only contract may reject valid UTF-16, but should be documented; DTD rejection must not depend on ASCII raw bytes only.

## Independent review handoff

After root provides frozen candidate commit/diff and author results:
1. Verify immutable bytes/commit and read actual changed code/test paths. Author selftests are context, not independent evidence.
2. Rerun focused suites in a separate bounded command, then run independently constructed regression inputs for the root-cause paths. Preserve exact command, stdout/stderr, exit code, fixture hashes and any failed attempt under fix-qa309/evidence/portability-*.
3. Recheck intended valid setup profiles and existing command behavior, plus fail-closed receipt/cleanup/mozlog and source JUnit identity behavior.
4. Report confirmed severity findings separately from intentional native/OS/provider gaps. Do not count synthetic tests as native/configure/package/publish evidence.
5. Mark Draft review, ordinary existing public CI, new private native launch and qualification/public readiness separately. No provider or signing/publishing mutation is part of this review.

