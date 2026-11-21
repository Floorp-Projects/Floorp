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

## Design acceptance cases

The case names and numbers below match §16 of `floorp-hotfix-design-v0.2.md`.
Statuses describe source coverage only; none is claimed to have passed native
verification.

| #  | Design case                                       | Slice status                                                                                                   |
| -- | ------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| 1  | import top-level failure                          | Partial: broken fixture rejection exists; legacy ownership is out of scope.                                    |
| 2  | install                                           | Source implemented, unrun: pinned package only, zero evaluation.                                               |
| 3  | disabled update crash                             | Partial: disabled update is pointer-only; crash recovery is absent.                                            |
| 4  | enable failure / disable crash intent             | Partial: failed enable preserves disabled selection; reopen blocks unresolved records.                         |
| 5  | v1→v2→v1 and re-enable                            | Source/test implemented, unrun: each activation uses a fresh sandbox.                                          |
| 6  | old callback/disposer delay                       | Partial: scopes revoke references; native tests check stale wrappers after disable/update.                     |
| 7  | hook needs task release                           | Partial: deactivate begins alongside disposal; task-drain coordination is absent.                              |
| 8  | concurrent dispose / hook throw / timeout         | Partial: manual in-flight/error drains, bounded cleanup, and shared stop are tested; full fault matrix absent. |
| 9  | W0 capture / window churn                         | Partial: W0 excludes hidden-toolbar, private, and web-panel windows; observer/close handling are absent.       |
| 10 | no HUB tab / ack / live disconnect                | Deferred: HUB publication is outside this slice.                                                               |
| 11 | precommit / revision update                       | Partial: atomic revision writes and stale-revision rejection exist; fault injection is absent.                 |
| 12 | postcommit publication / attach failure           | Deferred: no HUB readiness protocol.                                                                           |
| 13 | PREPARED crash recovery                           | Deferred: unresolved reopen fails closed; journal recovery is absent.                                          |
| 14 | interrupted / repeated attempts                   | Deferred: durable attempt episodes and retry budgets are absent.                                               |
| 15 | rejected selected package / corrupt record        | Partial: pinned bytes are revalidated and malformed records fail closed; fallback recovery is absent.          |
| 16 | candidate failure / old restore / cleanup failure | Partial: clean active restore and disabled failed-enable path exist; injected cleanup faults are absent.       |
| 17 | private-mode existing feature migration           | Deferred to M2.                                                                                                |
| 18 | fifty-update session                              | Source/test implemented, unrun: native test tracks resources, wrappers, and timers over 50 updates.            |
| 19 | profile/data compatibility                        | Deferred to M2; no existing feature data is touched.                                                           |
| 20 | terminal write after publish                      | Partial: terminal outcomes are preserved by a guard; end-to-end storage-fault injection is absent.             |
| 21 | new request / lost response / txId reuse / expiry | Partial: queued duplicate checks and detached outcomes exist; expiry cases remain untested.                    |
| 22 | host/runtime build tuple change                   | Partial: rollback requires an exact current tuple; migration behavior is untested.                             |
| 23 | startup fallback / crash boundary                 | Deferred: startup recovery is absent.                                                                          |

The colocated browser test is `M0FeatureHost.test.mts`; the documented command
is
`deno task test --near browser-features/modules/modules/feature-host/M0FeatureHost.test.mts --layer esm`.
Do not claim native loader behavior, repeat-update bounds, or complete M0
acceptance until that test and the remaining gates run against the pinned Floorp
runtime. This source-coverage map does not claim complete M0 acceptance.
