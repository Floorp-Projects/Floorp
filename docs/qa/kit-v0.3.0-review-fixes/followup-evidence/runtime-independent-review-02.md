# Runtime QA309 independent post-fix review — c9027879 (scoped GO)

Observed 2026-10-09 UTC. This reviewer authored neither the Runtime native helper/test, setup/profile nor mozlog fixes. The review and tests were read-only for source. No commit/push/dispatch/cancel, browser/full build, paid provider access, permission, signing or publication operation was performed.

## Fixed input and decision

Commit **c9027879e053c9031c7f3e27495f6d415d36c10f**, clean at the captured beginning and end. Native source SHA256 **8bbea4abfc5d8cf1a3b70a275287c7fe6725b96fa17afb4c74ef51814b7d142e**; native test SHA256 **0dae5c088abd51a84338a62c977a7a43e5765dfb1fd8a0647090b10fcab625a6**. All seven selected file hashes remained unchanged during independent tests. [Start pin](runtime-independent-input-02.json), [end pin, limits and artifact hashes](runtime-independent-input-end-02.json), [committed repair diff from cc83](runtime-independent-native-candidate-8bbe-02.patch).

**GO for the reviewed source repairs, reviewable Draft and existing ordinary public PR CI.** The previously independently confirmed parent-init P1 is fixed in both actual-owned reproductions. No remaining P0/P1/P2 defect was confirmed in the changed helper, setup/profile or parser scope. New private native qualification, Blacksmith migration/paid execution, product readiness and publication remain **UNVERIFIED / No-Go for claiming proof**. Operator launch/isolation and unconditional worker-loss containment still need their separate execution evidence. Ordinary public hosted CI authorization is unaffected by those later stages.

The earlier [cc83 RED report](runtime-independent-review-01.md) is retained as historical evidence; its P1 conclusion is superseded for this fixed hash. Its references to a parent full191 run were an incorrect provisional count: root later confirmed the prior cc83 whole suite raw count was **190**. This report claims only this reviewer's observed native47 and the earlier unchanged configuration/parser33 results; it does not claim an independently executed whole-runtime suite.

## Independent observations

| Check | Result and evidence |
| --- | --- |
| Full native I/O unit suite | Exit0; **47 methods, 46.688s**. [Complete log](runtime-independent-native-suite-8bbe-01.log). This is independently executed author-supplied regression coverage, supplemented below with independently written fixtures. |
| Parent initial snapshot raises EIO | Real supervisor and TERM-ignoring command had started. Original EIO returned in **2.225s**; both were absent before external fixture recovery; command receipt already recorded cleanup complete/cancelled; parent writer closed. [Result](runtime-independent-parent-init-raise-03.json). |
| Initial snapshot absent, then reader initialization raises EIO | Original reader EIO returned in **2.160s**; same process absence, receipt and FD observations before fixture recovery. [Result](runtime-independent-parent-init-none-03.json). |
| Parent reader close raises EIO after actually closing | Initial snapshot EIO retained; separate CLOSE_READER EIO note retained; both processes absent and all own parent control FDs closed before fixture recovery, **2.210s**. [Result](runtime-independent-parent-init-close-reader-04.json). |
| Parent PIDFD close raises EIO after actually closing | Initial snapshot EIO retained; separate CLOSE_PIDFD EIO note retained; same process/FD checks, **2.150s**. [Result](runtime-independent-parent-init-close-pidfd-04.json). |
| No PIDFD and missing initial snapshot, then reader EIO | Held-child WNOWAIT path demonstrably sent TERM; no /proc birth prerequisite. Both processes absent, completed cancellation receipt, all own parent FDs closed before recovery, **2.201s**. [Result](runtime-independent-parent-init-fallback-04.json). |
| Real ignored SIGCHLD and real second OS thread | Both refused before allocating a control pipe or calling fork; signal state restored and thread joined. [Result](runtime-independent-prefork-8bbe-01.json). |
| Fork EAGAIN with transient close-after-close EIO | Original fork exception object preserved; both real pipe descriptors closed before fixture recovery. [Same result](runtime-independent-prefork-8bbe-01.json). |
| Setup/profile and mozlog | Earlier independent **33 methods, 3.469s**, exit0. [Full log](runtime-independent-config-logs-suite-01.log). Their source/tests hashes are unchanged at c902; no rerun for counts. |

Actual-owned fixtures are [initialization](runtime-independent-parent-init-03.py) and [additional control/fallback](runtime-independent-parent-init-04.py); guard/FD fixture is [prefork](runtime-independent-prefork-8bbe-01.py). Corresponding full logs are retained with the same prefixes. All five actual-owned cases distinguish observations before recovery from their outer safety cleanup. None required the fixture to kill a surviving target to manufacture a passing cleanup observation. The outer recovery only ever considers fixture-owned PID/birth identities, reaps them, closes still-matching own FDs and restores the fixture caller's original subreaper state.

The earlier actual-owned -02 RED results are retained. The new fixture waits until its known command writes its identity before injecting the parent fault, so a pre-launch cancellation cannot falsely satisfy the regression.

## Code review tied to this commit

All positions below are in .github/workflows/scripts/qa3_native_io.py at c9027879; named functions are the stable anchors.

- `_supervise` lines648–655 rejects nondefault SIGCHLD or more than one OS thread before control allocation/fork. Under that supported single-owner contract, a live unreaped fork child cannot be recycled to an unrelated PID. Fork failure closes both controls with bounded attempts while preserving the initial exception (lines657–666).
- Parent ownership/cancellation no longer depends on obtaining `/proc` birth. `_supervise` opens an own PIDFD where supported (lines762–770), otherwise `signal_supervisor` inspects the unreaped child with `waitid(...WNOWAIT)` before signalling (lines741–759). It never falls back from an already recycled arbitrary PID. The parent retains and reaps this child through bounded TERM then KILL waits (lines788–803).
- All parent control ends and PIDFD receive bounded cleanup, with failure retained and named secondary notes rather than a skipped later phase (lines718–739,788–808). The independently tested close-after-close case also reaches the EBADF retry path. Successful recovery of a close fault still fails the command; it is not silently promoted to PASS.
- Inner `_run_owned` protects post-Popen initialization, remembers the first exception, continues cleanup independently of a broken log drain, and records ownership uncertainty (lines339–511). TERM/KILL discovery and reap remain separate cleanup operations. The independent47 run exercised sink ENOSPC/EDQUOT, initial snapshot/selector/pipe faults, signal/close secondary failures, detached and TERM-late-fork children, unrelated late adoption, and caller subreaper preservation. These assertions inspect target absence and failure receipts, not only test totals.
- `_run_owned` cancellation during cleanup and after a successful leader exit cannot return PASS (lines419–434,524–546); its supervisor also checks cancellation after the command returns (lines680–682). Worker hard SIGKILL/STOP remains a separately documented boundary, not a successful local cleanup claim.
- `_supervisor_reply` lines568–643 validates exact envelope/result/error shapes, explicit bool versus int distinctions, clean stop/completion, finite duration, lowercase SHA256 syntax, command/data-role identity, monotone typed staging usage, and Exception-only reconstruction. A candidate-controlled private reply is still shadow evidence, not a controller qualification receipt. `SystemExit(0)` and false/empty error variants are rejected by the independently run regressions. Live stdout budget use crosses the fork boundary (test_data_budget_accounting_crosses_supervisor_boundary), so the next command cannot reuse the initial budget merely because accounting happened in the child.

Setup/profile .github/workflows/scripts/qa3_build_profile.py `profile` lines26–67, `enable_debug_tests` lines79–99 and `verify_config` lines102–141 keep canonical Debug separate from opt, profile-generate/use and explicitly noncanonical legacy Debug+PGO. setup-floorp.sh lines14–35 validates the actual entry before patch/copy/install/bootstrap; lines136–151 preserve profiling/use flags and verify generated config. Generated five-target mappings, Linux ARM opt template behavior, four supported PGO targets and legacy flags passed in the unchanged33 test run. These are stubbed generated-config tests; no five-target native configure/build was executed.

Mozlog .github/workflows/scripts/qa3_mozlog.py `verify_mozlog` lines59–65 bounds each binary readline to MAX_EVENT_BYTES+1, counts events and requires LF before parsing; lines94–102 accepts present null/string subtest, rejects missing/list/number/bool. The unchanged21 parser methods also cover byte/newline/event limits, valid writer fields, bad lifecycle, skips/retries/crash/leak and canary distinctions. Synthetic logs do not establish packaged harness execution or manifest eligibility.

## Test bounds and retained first errors

The complete native unit command used wall90s, CPU40s, AS32GiB and regular-file50GiB hard limits. The latter two leave the helper's existing28GiB/50GiB child prlimit requests valid; they are process limits, not allocations or measurements. Actual unit inputs/outputs are small. No disk was filled, native binary built or browser started.

Each actual-owned supplementary fixture used wall40s, CPU15s, AS512MiB and file8MiB, with its command AS128MiB. Only the imported helper's in-memory MAX_EXPANDED was reduced to8MiB to agree with the outer fixture cap; source bytes and native operational budgets were unchanged. The pre-fork/FD fixture used wall10s/CPU5s/AS512MiB/file8MiB.

All initial failures remain. The original -01 parent fixture was blocked because its outer8MiB file hard cap conflicted with the helper50GiB request; it did not prove a live target. The new pin's first input-capture attempt used a wrong setup path before launching any test; [record](runtime-independent-input-02-first-error.json). An initial empty working-tree patch was retained because root committed identical source while pinning; the actual committed diff is the separate -02 patch. These are fixture/setup limits, not new candidate defects.

## Remaining coverage boundaries

This evidence is from the selected Linux environment. PIDFD/subreaper/procfs/waitid mechanics do not establish Windows/macOS cleanup. A permanently unresponsive or independently SIGKILLed/stopped supervisor, kernel refusing all signalling/reaping, and loss of the entire caller require the external job/cgroup/VM boundary; local fail-closed is not universal containment. Those conditions were not converted into successful receipts.

No actual private native producer/consumer, cross-machine material transfer, native browser/xpcshell suites, all five OS targets, upstream full eligible manifest, external operator permissions/cgroup setup, Blacksmith capacity/free-disk measurements, performance/cost or qualification/publisher activation was executed. Original immutable upstream object U remains absent in this shallow checkout; no assertion of a missing upstream tree path is made. Searchfox tooling remains unavailable and no broad upstream search or bootstrap was undertaken.

The separate [Floorp source repair independent review](floorp-independent-review-02.md) remains pinned to its own source/test hashes. No author self-test or earlier operations report is counted as this reviewer's independent execution.
