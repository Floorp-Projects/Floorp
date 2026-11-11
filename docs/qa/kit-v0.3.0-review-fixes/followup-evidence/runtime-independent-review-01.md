# Runtime QA309 independent integrated review — cc83e6f (RED)

2026-10-09 UTC. Read-only independent review of Runtime HEAD cc83e6fea6499d6ec89c76ae9837c280556e9d53, clean at start. This reviewer did not author native helper/tests, setup/profile or mozlog changes. No source changes, commit/push/dispatch/cancel, native browser/full build, paid provider access, permission or publication changes.

## Decision

**Blocking P1 remains in the new parent _supervise initialization/cleanup path.** Ordinary existing public PR CI and reviewable Draft scope are separate. New private native proof remains No-Go until this helper defect is repaired and the external worker boundary is verified. No QA3 native/product/public readiness is certified.

Independent setup/mozlog verification is green (33 methods). The parent-root full191 run is author/integration context, not this reviewer's independent evidence.

[Seven-file input hashes](runtime-independent-input-01.json). qa3_native_io.py SHA256 f8005ff48f51d857304b1001e2dfc3477f34edb753dcb3abd3a2ff0663438a55. Exact code positions below refer to cc83e6f, even if a later repair is in the working tree.

## P1 — missing initial supervisor birth disables mandatory cancellation

Code: .github/workflows/scripts/qa3_native_io.py, _supervise lines617/624/625 and finally650–659. birth starts as None; _proc(pid) happens before parent writer close; finally requires a truthy matching birth before either TERM or KILL.

Case A: a one-shot parent-only initial _proc EIO. After12.025s, run_owned re-raises the original EIO while both actual own supervisor and its TERM-ignoring Python command remain alive. No command process receipt exists yet. The parent's private control writer also remains open.

Case B: parent initial _proc returns None, then reader os.set_blocking raises EIO. After12.032s, the original reader EIO returns while both own supervisor/command remain alive and no process receipt exists. Writer is closed in this variant.

These are ordinary recoverable parent initialization faults; the supervisor itself was not SIGKILLed, stopped, or kernel-stalled. FAIL remains closed, but owned work and resource use continue without the promised mandatory recovery/receipt. This is distinct from the documented last-resort external-job boundary for an unconditionally lost supervisor.

Evidence: [raised snapshot](runtime-independent-parent-init-raise-02.json), [missing birth + reader init](runtime-independent-parent-init-none-02.json), [actual-owned fixture](runtime-independent-parent-init-02.py), complete -02 logs. Fixture independently checks PID birth, sends the own supervisor cancellation, reaps it, verifies the command absent, closes its own leaked writer and restores caller subreaper. No foreign process kill or resource exhaustion.

Fix direction: unconditionally protect both parent pipe ends; establish an ownership handle that remains usable on parent snapshot/init failure, or prevent command launch until parent startup validation. A newly forked unreaped own child must not skip cancellation only because /proc birth was unavailable. Continue bounded TERM→KILL→wait; retain the original error and cleanup evidence. Add actual-process regressions for both above cases. Parent was notified immediately.

## Fixture limits and retained first attempt

wall40s / AS512MiB / CPU15s / file8MiB; actual command address-space128MiB, one command, short sleep, tiny stdout/identity file. Only the helper's in-memory regular-file cap was reduced to8MiB to match the outer fixture hard cap. No production source or operational disk/memory limit was changed.

The initial attempt used the outer8MiB cap with the helper's50GiB prlimit request; prlimit refused to raise that hard limit and the command never started. [First-error record](runtime-independent-parent-init-first-error-01.json), initial script/log/process receipt are retained. That first failure is fixture coverage, not evidence of a surviving command. The corrected -02 attempts reached and observed the real process lifecycle.

## Integration configuration and mozlog

Independent command:
timeout --signal=TERM --kill-after=5s 35s /usr/bin/prlimit --as=536870912 --cpu=20 --fsize=8388608 -- python3 -B -m unittest test_qa3_build_profile test_qa3_mozlog -v

[Full log](runtime-independent-config-logs-suite-01.log). Exit 0, 33 methods, 3.469s.

Setup/profile: actual setup entry validates target/booleans/mode/profile before patch/copy/install/bootstrap; generated config is checked against the same expectation. Five canonical Debug mappings have explicit tests, four supported PGO generate/use mappings preserve profiling/LTO/profdata/jarlog, production-opt keeps target-specific template flags (Linux ARM has no active tests flag), legacy Debug+PGO remains explicitly noncanonical. These are stubbed setup/generated configuration tests, not five native configure/builds.

Mozlog: missing/nonstring-nonnull subtest rejected; formal None/empty/whitespace retained. Reader requests MAX_EVENT_BYTES+1 before allocation, LF termination and byte/event boundaries checked. Retry, skip, crash/leak, incomplete lifecycle, wrong IDs and child exit types remain fail-closed. Valid cases and negative canary tests passed. No real packaged harness or full eligible manifest coverage is claimed.

## Limits / further review

The new _run_owned inner body has broader protected initialization and cleanup-call wrappers, log-I/O fault tests, own forest and fork-transfer budget checks. Those author tests and source inspection do not clear the confirmed parent initialization P1. This reviewer will re-run focused native helper tests and independent parent faults against the next immutable repaired candidate rather than certify cc83e6f from the full191 integration result.

Persistent kernel failures and unconditional supervisor kill/stop require an external job/cgroup/VM teardown boundary. Linux-only pidfd/subreaper mechanics here do not establish Windows/macOS native cleanup. Operator launcher, actual build/test archives, all five native targets, performance/cost and qualification/publisher enforcement remain unverified.

