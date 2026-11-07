# Floorp QA309 independent final review — candidate-03

2026-10-09 UTC. This reviewer did not author the fix. No author source/test/README edits, commits, pushes, dispatches, cancellations, native builds or publication actions.

**Scoped Go:** candidate-03's JUnit encoding fix, isolated source supervisor and failure/cleanup protocol are acceptable within the reviewed Linux source-shadow scope. No remaining P0/P1/P2 was confirmed in these changed paths. This is not qualification or required promotion approval; new private native runs and provider/publication changes remain outside scope.

## Immutable evidence

At review start, base/head 718e688fbd7ad88a776534e7778ad8e4a7f67364 with three-file uncommitted diff matched floorp-candidate-03.patch byte-for-byte. Parent subsequently applied deno fmt to README only. End verification proves source/test hashes unchanged and README words unchanged after independently reconstructing the reviewed README from the fixed patch. The full later diff is not claimed equal to candidate-03.

- Patch SHA256: 30fa4bf57c9c557c6dced39abcfd7854a35b22eb79b9a4c4881da746690edcd4.
- source.py: 5cb0803d54c47197e0597cd93de436c60cb2f667340f5bcff55e6f0326b30d32.
- test_source.py: a8e50bc710b7a2fce37269df43326d2683d4997d9a40772f5f286f6a7a9dde51.
- README.md at review start: c602857927aa6e401b9c647428506f9b568e0ca0e5d747f56fafe5b8bee73ec0; after the independently verified word-preserving reflow: 91a09ecc0eaaa9136dfca829fcb44554b53b58ba4776e3430f93e3e5e7d424a6.

[Input pin](floorp-independent-input-02.json), [end pin](floorp-independent-input-end-02.json). End HEAD is 7747cf0e711937e056a34594d8d39fcc8dfc54f0, committed by the parent; this reviewer did not commit. The initial end assertion expecting an unchanged three-file diff failed on the README reflow; [metadata failure](floorp-independent-input-end-02-first-error.json) is retained. Candidate-01 RED report, fixture, raw results and logs remain preserved under the -01 names. Candidate-02 was an intermediate author revision, not independently certified here.

## Root-cause fixes verified

JUnit read_junit (source.py 227–243): explicit UTF-8/optional UTF-8 BOM, NUL rejection and encoding-declaration check precede DTD/entity rejection and ET parsing. Independent 19 inputs verify UTF-16/32 BOM/no-BOM LE/BE and conflicting declarations cannot reach ET.fromstring; ordinary Unicode UTF-8/BOM still works. Identity, aggregate, skip/failure behavior remains unchanged.

_cleanup_owned (381–458): bounded cleanupErrors collection now continues TERM/KILL/reap after transient snapshot, pidfd open, signal/poll/wait/close OSError. PID birth checks and pidfds protect descendant signalling. _execute_owned (467–531) preserves initial failure separately, emits typed ownedCleanupComplete, and converts cleanup-time interruption to failure. Parent execute (577–602) validates exact fields/types, and rejects recorded cleanup errors even after successful recovery.

The original candidate-01 single-EMFILE real-child fixture was rerun independently. It now returns the original timeout plus cleanupErrors and ownedCleanupComplete=true. The owned child is absent before the fixture's finally; external kill was not required. Two additional actual-child cases inject one close-EIO after the fd was closed and one cleanup process-snapshot EIO. Both retain the fault, complete termination/reap, and preserve the timeout. These are synthetic syscall faults at real helper boundaries, not actual fd/resource exhaustion or persistent-kernel-permission recovery claims.

## Independent runs

1. Focused suite executed by this reviewer: 22 methods, exit 0, 8.573s. Wall45s/AS512MiB/CPU20s/file16MiB. Full output [floorp-independent-suite-02.log](floorp-independent-suite-02.log). Includes detached descendants, TERM-late-fork, unrelated pre-existing child preservation, parent subreaper preservation, first-log immutability, and two actual isolated cleanup-fault/interruption regressions. Author-green results are not substituted for this execution.

2. [Independent negative script](floorp-independent-negative-02.py) exit 0 under wall45s/AS512MiB/CPU15s/file16MiB. [Results](floorp-independent-negative-02.json), [log](floorp-independent-negative-02.log):
   - XML 19: 17 rejected before parser, two valid Unicode inputs accepted.
   - Control protocol 21: typed exit zero accepted; exit124 preserved; bool/float/string exit, bool-like cleanup completion, missing/extra fields, untyped/oversized cleanup errors, recovered fault, incomplete cleanup, malformed/null/list/overlength control never returned zero.
   - Three PID race models: birth mismatch cannot signal wrong process; ESRCH before open/signal tolerated. Real PID reuse was not forced.
   - Real command receives only fds0/1/2 and no fixture GITHUB_TOKEN/NODE_OPTIONS/PYTHONPATH; caller subreaper setting unchanged.
   - Three actual-child cleanup fault cases: zero owned leftovers before fallback finally; no external kill; one retained error per case; caller subreaper restored.

[pidfd observation](floorp-independent-fixtures-02/pidfd-fault-observation.json), [close observation](floorp-independent-fixtures-02/close-fault-observation.json), [snapshot observation](floorp-independent-fixtures-02/snapshot-fault-observation.json).

## Boundaries and residual known gaps

Ordinary public GitHub-hosted PR CI and source shadow remain allowed. The original source setsid-child and UTF-16 DTD gaps are fixed for the tested conditions. The new source transient cleanup exception and cleanup-time interruption paths are also corrected.

A SIGKILL/SIGSTOP/stalled supervisor still needs external worker/job teardown; README states this and absent control remains non-success. Same-UID Python supervision is not a replacement for cgroup/VM containment. Persistent unavailable syscalls may make cleanup incomplete and FAIL; universal recovery is not claimed.

Linux pidfd/subreaper support was exercised here. Windows/macOS supervisors, native browser/configure/build/support, the full 106-case Deno/Node/Python source plan, private launcher/cgroups, provider costs and publication gates were not executed or qualified. The unrelated pre-existing-child and owned late-fork tests passed; arbitrary real PID reuse or every unrelated late-adoption timing was not forced.

README now records historical Library403 and the later #2893 kit review. The broader frozen coverage catalog and other PR documentation are outside this three-file candidate; earlier historical-document P2 entries are not treated as new cleanup/code findings.

**Go distinctions:** Go for this repair's reviewability and existing source-shadow/ordinary public CI scope. No-Go for declaring QA3 native/product qualification, required promotion or QA3 public readiness complete. A new private native launch still requires its separate Runtime fix verification and operator-owned boundary evidence/authorization.

