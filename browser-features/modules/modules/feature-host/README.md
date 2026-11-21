# Experimental local Hotfix host (M0)

This is a local experiment, not the production loader and not a security
sandbox. The parent ESM host evaluates one pinned, self-contained classic script
fixture in a fresh system-principal `Cu.Sandbox` per activation. The fixture ABI
(`activate`, `attachWindow`, `health`, `deactivate`) and JSON-string broker are
experimental and are not the future `FeatureContext`. The only effect is a
generation-owned diagnostic marker on eligible browser windows. No HUB, legacy
loader, BrowserGlue, Actors, existing feature, network, preference, or
feature-data integration is added.

Nothing starts automatically. `getM0FeatureHostExperimental(true)` is the
explicit profile-root singleton entrypoint and is restricted to `dev` or `test`
mode. Browser tests use `createM0FeatureHostForTests(true)`, which creates a
UUID-named root below `floorp-hotfix-m0-tests` and removes only that new root
after clean shutdown. It never deletes the persistent `floorp-hotfix-m0`
experiment root. Both entrypoints require opt-in. Fixture bytes and SHA-256 pins
are in `M0Fixtures.sys.mts`; arbitrary package source and remote imports are not
accepted. No package GC is needed for this finite fixture set.

`ResourceScope` revokes synchronously. A handle's cleanup reference and
parent-scope reference are cleared before cleanup begins; repeated or reentrant
disposal while that cleanup is running is a non-awaiting no-op, while
`disposeAll()` retains the original cleanup result and aggregates bounded
errors/deadlines. Completed child-disposal reports stay with the root, and
pending nested scopes remain counted. The colocated browser test covers
delayed/manual disposal, child throw/timeout aggregation, nested resource
counts, and timer/resource bounds.

`ResourceScope.runTask(factory)` synchronously registers host-managed work
before calling the factory and supplies that scope's `AbortSignal`. Revocation
marks the whole child tree revoked before delivering abort notifications; task
creation and broker effects are then rejected while issued cleanup handles
remain usable. Task counts, pending tasks, observed rejections, and task
failures are tracked separately from resources. Stop invokes `deactivate` and
starts scope cleanup concurrently under one total two-second deadline; only
rejection with the exact scope `signal.reason` is treated as expected
cancellation. Lifecycle hook promises remain separately bounded rather than
becoming managed tasks, so activation failure/retry behavior is unchanged. The
signal and runner are host-internal and are not exposed through the JSON fixture
broker or as a new network/timer capability.

The experimental journal is schema 2. It records a strict before snapshot and
`PREPARED`/`OLD_STOPPED`/`CANDIDATE_READY` phase, plus one durable attempt
episode shared by startup and interrupted operations. Episodes allow at most two
distinct candidate digests; an evaluation is recorded before sandbox execution,
and a preflight rejection is recorded before fallback. Only `succeeded` and
`failed_restored` clear an episode. Recovery preserves the origin transaction,
refuses stale build proofs, treats durable disable intent as authoritative, and
leaves durable quarantine blocked without automatic repair. Schema-1 or
malformed records fail closed before fixture evaluation. Each pinned fixture
also carries host-schema/build, locked runtime BuildID, and capability metadata.
The store checks this tuple and the host's explicit capability allowlist before
returning bytes to the activation path; compatible package metadata is separate
from the health proof used for rollback.

The parent host is tested through its packaged
`resource://noraneko/modules/feature-host/FeatureHost.sys.mjs` system ESM URI.
It owns stable eligible-window IDs and observes `domwindowopened`, browser
delayed-startup completion, and `domwindowclosed`. Each activation snapshots a
finite W0 before evaluation; later windows are admitted only after terminal
publication and a bounded core `attachWindow` succeeds. Closing a window
detaches only its generation child scope without entering the management queue.
Initial markers stay staged until commit; staged-close cleanup failure
quarantines the candidate before commit. A post-commit attach/cleanup failure
stops and durably blocks the active generation while preserving desired intent.
Shutdown removes observers, load callbacks, readiness waiters, and tracked
window references. Native tests close a test-owned first attached window while
leaving the runner's original window open.

Browser tests may reopen the same UUID-named test root after stopping its host.
The test-only write-fault adapter injects failures before or after native
`IOUtils.writeJSON`; reads still use native `IOUtils`. The test-only durable
write observer runs after the flushed atomic journal replacement and the
in-memory record update, and receives a detached snapshot. The production
singleton rejects this test option. These barriers do not simulate power loss,
failure during rename, or failure during flush.

`deno task test:hotfix-process` is a Linux-only process-death runner. It uses a
fresh private profile and M0 UUID root, a dedicated Marionette port, and a
verified browser session/process group. The scenario driver uses the packaged
FeatureHost system ESM and normal install/enable/update/disable operations; the
journal is not seeded and crash boundaries do not call graceful host shutdown.
The process matrix runs with W0 empty and test windows excluded; existing UI
window lifecycle coverage remains in the native FeatureHost tests, separately.
The supervisor records the durable journal snapshots, process identities,
BuildID, runtime-lock hashes, per-process payload evaluation counts, recovery
results, and assertions in `_dist/hotfix-process-test-runs/`; browser stdout and
stderr are retained there as well. Current Linux process-death coverage kills at
update `PREPARED`, `OLD_STOPPED`, `CANDIDATE_READY`, and
`committed_pending_publish`; repeats fallback-pending interruption before the
attempt marker; interrupts after the fallback attempt marker and verifies
exhaustion without re-evaluation; and covers disabled-update `PREPARED`, durable
disable intent, and interrupted enable. This is not evidence for other platforms
or for power-failure / in-rename / in-flush behavior. The recorded process
matrix evidence is Linux x86_64 only; Linux aarch64 process recovery has not
been verified. HUB publication/readiness, signing, migration, and production
loader integration are not included.

## Design acceptance cases

The case names and numbers below match §16 of `floorp-hotfix-design-v0.2.md`.
Statuses describe this local prototype and its colocated tests, not full M0
acceptance.

| #  | Design case                                       | Slice status                                                                                                         |
| -- | ------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| 1  | import top-level failure                          | Partial: broken fixture rejection exists; legacy ownership is out of scope.                                          |
| 2  | install                                           | Pinned fixture installation is pointer-only and performs zero evaluation.                                            |
| 3  | disabled update crash                             | Native synthetic case and Linux process-kill `PREPARED` case restore the before pointer without evaluation.          |
| 4  | enable failure / disable crash intent             | Native recovery and Linux process kills cover interrupted enable and durable disable intent without evaluation.      |
| 5  | v1→v2→v1 and re-enable                            | Existing native lifecycle test exercises fresh sandbox generations.                                                  |
| 6  | old callback/disposer delay                       | Partial: stale wrappers and broker callbacks are measured; no sandbox security or heap-leak claim.                   |
| 7  | hook needs task release                           | Native: abort precedes deactivate; released tasks drain concurrently with global/window disposal under one deadline. |
| 8  | concurrent dispose / hook throw / timeout         | Native: task, hook, child/window, resource, deadline, and sandbox teardown outcomes are aggregated.                  |
| 9  | W0 capture / window churn                         | Native: finite W0, delayed startup, close detach, admission, filtering, and shutdown teardown are tested.            |
| 10 | no HUB tab / ack / live disconnect                | Deferred: HUB publication is outside this slice.                                                                     |
| 11 | precommit / revision update                       | Atomic record revisions and before/after-replace write faults are covered; full request matrix is partial.           |
| 12 | postcommit publication / attach failure           | Partial: staged-close cleanup failure quarantines before commit; failed admission quarantines; HUB is deferred.      |
| 13 | PREPARED crash recovery                           | Schema-2 recovery has Linux process-kill coverage at PREPARED, OLD_STOPPED, CANDIDATE_READY, and disabled update.    |
| 14 | interrupted / repeated attempts                   | Linux process kills repeat fallback-before-mark and verify bounded digest exhaustion without retry.                  |
| 15 | rejected selected package / corrupt record        | Preflight rejection may use one proof-eligible fallback; malformed/incompatible records evaluate nothing.            |
| 16 | candidate failure / old restore / cleanup failure | Candidate failure restores only a current-health before generation within the two-candidate budget.                  |
| 17 | private-mode existing feature migration           | Deferred to M2.                                                                                                      |
| 18 | fifty-update session                              | Existing native test tracks resources, wrappers, and timers over 50 updates.                                         |
| 19 | profile/data compatibility                        | Deferred to M2; no existing feature data is touched.                                                                 |
| 20 | terminal write after publish                      | Before/after-replace fault paths preserve durable success when present and block the current runtime.                |
| 21 | new request / lost response / txId reuse / expiry | Partial: retry/query and descriptor conflict are supported; retention expiry remains untested.                       |
| 22 | host/runtime build tuple change                   | Package compatibility metadata and rollback health proof are separately exact-tuple gated.                           |
| 23 | startup fallback / crash boundary                 | Linux process kills cover committed update publication and fallback recovery; other crash models remain open.        |

The colocated browser test is `M0FeatureHost.test.mts`; compile the production
modules before starting the browser with
`deno task feles-build build --phase before-mach`, then run
`deno task test --near browser-features/modules/modules/feature-host/M0FeatureHost.test.mts --layer esm`.
The 50-update lifecycle case remains part of `runAllTests`. The Deno harness
checks run with `deno task test:host`; the separate Linux process matrix runs
with `deno task test:hotfix-process`. Passing these tests does not validate HUB
readiness, power-failure behavior, other-platform process recovery, or complete
M0 acceptance.
