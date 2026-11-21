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
`disposeAll()` retains the original cleanup result and aggregates
errors/deadlines. The colocated browser test covers delayed/manual disposal,
manual failure aggregation, nested resource counts, and timer/resource bounds.

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

Browser tests may reopen the same UUID-named test root after stopping its host.
The test-only write-fault adapter injects failures before or after native
`IOUtils.writeJSON`; reads still use native `IOUtils`. Recovery-boundary tests
seed real journal records after shutdown, so they are synthetic crash-boundary
tests, not evidence from killing a Floorp process mid-write. The production
singleton has no fault-injection option. No window observers/admission gate, HUB
publication, signing, migration, or production loader is included.

## Design acceptance cases

The case names and numbers below match §16 of `floorp-hotfix-design-v0.2.md`.
Statuses describe this local prototype and its colocated tests, not full M0
acceptance.

| #  | Design case                                       | Slice status                                                                                                      |
| -- | ------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------- |
| 1  | import top-level failure                          | Partial: broken fixture rejection exists; legacy ownership is out of scope.                                       |
| 2  | install                                           | Pinned fixture installation is pointer-only and performs zero evaluation.                                         |
| 3  | disabled update crash                             | Journal restores the before pointer without evaluation; synthetic native case added.                              |
| 4  | enable failure / disable crash intent             | Failed enable restores disabled state; durable disable wins on reopen.                                            |
| 5  | v1→v2→v1 and re-enable                            | Existing native lifecycle test exercises fresh sandbox generations.                                               |
| 6  | old callback/disposer delay                       | Partial: scope revocation and stale-wrapper checks exist; arbitrary callback termination is not proven.           |
| 7  | hook needs task release                           | Partial: deactivate begins alongside disposal; task-drain coordination is absent.                                 |
| 8  | concurrent dispose / hook throw / timeout         | Partial: in-flight/error drains, bounded cleanup, and shared stop are tested; full fault matrix absent.           |
| 9  | W0 capture / window churn                         | Partial: W0 filters hidden-toolbar, private, and web-panel windows; observers/close handling are absent.          |
| 10 | no HUB tab / ack / live disconnect                | Deferred: HUB publication is outside this slice.                                                                  |
| 11 | precommit / revision update                       | Atomic record revisions and before/after-replace write faults are covered; full request matrix is partial.        |
| 12 | postcommit publication / attach failure           | Deferred: no HUB readiness protocol.                                                                              |
| 13 | PREPARED crash recovery                           | Schema-2 recovery covers all three actual replaced phases and disabled before state.                              |
| 14 | interrupted / repeated attempts                   | Durable two-digest episodes survive reopen; exhaustion quarantines without automatic retry.                       |
| 15 | rejected selected package / corrupt record        | Preflight rejection may use one proof-eligible fallback; malformed/incompatible records evaluate nothing.         |
| 16 | candidate failure / old restore / cleanup failure | Candidate failure restores only a current-health before generation within the two-candidate budget.               |
| 17 | private-mode existing feature migration           | Deferred to M2.                                                                                                   |
| 18 | fifty-update session                              | Existing native test tracks resources, wrappers, and timers over 50 updates.                                      |
| 19 | profile/data compatibility                        | Deferred to M2; no existing feature data is touched.                                                              |
| 20 | terminal write after publish                      | Before/after-replace fault paths preserve durable success when present and block the current runtime.             |
| 21 | new request / lost response / txId reuse / expiry | Partial: retry/query and descriptor conflict are supported; retention expiry remains untested.                    |
| 22 | host/runtime build tuple change                   | Package compatibility metadata and rollback health proof are separately exact-tuple gated.                        |
| 23 | startup fallback / crash boundary                 | Startup selected/fallback recovery and synthetic interrupted episodes are covered; killed-process faults are not. |

The colocated browser test is `M0FeatureHost.test.mts`; run it with
`deno task test --near browser-features/modules/modules/feature-host/M0FeatureHost.test.mts --layer esm`.
The 50-update lifecycle case remains part of `runAllTests`. Passing these tests
does not validate window churn observers, HUB readiness, real process-kill
recovery, or complete M0 acceptance.
