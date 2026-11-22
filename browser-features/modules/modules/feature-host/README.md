# Experimental local Hotfix host (M0)

This is a local experiment, not the production loader and not a security
sandbox. The parent ESM host evaluates one pinned, self-contained classic script
fixture in a fresh system-principal `Cu.Sandbox` per activation. The fixture ABI
(`activate`, `attachWindow`, `health`, `deactivate`) and JSON-string broker are
experimental and are not the future `FeatureContext`. M0 fixtures only create a
generation-owned diagnostic marker on eligible browser windows. Stage 1 adds a
parent-only HUB registry and pinned HUB fixtures; Stage 2 adds restricted
parent-Actor page discovery and the Settings React transport. This remains
separate from the legacy loader and existing feature-data, preference, or
arbitrary network integrations.

Experimental FeatureHost activation does not start automatically.
`getM0FeatureHostExperimental(true)` is the explicit profile-root singleton
entrypoint and is restricted to `dev` or `test` mode. Browser tests use
`createM0FeatureHostForTests(true)`, which creates a UUID-named root below
`floorp-hotfix-m0-tests` and removes only that new root after clean shutdown. It
never deletes the persistent `floorp-hotfix-m0` experiment root. Both
entrypoints require opt-in. Fixture bytes and SHA-256 pins are in
`M0Fixtures.sys.mts`; separate HUB fixture bytes and pins are in
`HubFixtures.sys.mts`. Arbitrary package source and remote imports are not
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

## M1 HUB contract (Stage 1 + Stage 2)

`HubRegistry.sys.mts` owns bounded JSON descriptors, generation leases,
host-epoch claims, detached snapshots, monotonically increasing HUB revisions,
and direct acknowledgement validation. Fixture requests contain only a
`hub.register` discriminator and a strict descriptor; pages and actors receive
only serialized `HubSnapshotMessage` values and return
`HubSnapshotAcknowledgement` values. Descriptors permit canonical
`/extensions/{featureId}/{localId}` routes, localization keys, optional
navigation metadata, and the fixed `message` view with bounded scalar
interpolation parameters. They cannot provide markup, renderers, callbacks,
privileged objects, external URLs, or feature-data writes.

`BrowserGlue.sys.mts` initializes shared HUB page discovery and registers the
`NRFeatureHub` JSWindowActor. Actor registration and adapter setup are
automatic, but discovery and connection are accepted only in `dev` or `test`
mode, including for `about:hub` and chrome Settings pages. This does not install
or activate the experimental FeatureHost or any experimental fixture.
`HubPageDiscovery.sys.mts` installs the shared `HubClientDiscoveryAdapter` used
by `HubRegistry.sys.mts` and discovers only parent-visible current top-level
documents in the finite captured W0. A document must be in an eligible normal
browser window (`navigator:browser`, visible toolbar, non-private, and not a Web
Panel), and the parent rechecks its current WindowGlobal and URI before
accepting a client. In `dev`/`test`, authorized documents are `about:hub` and
`chrome://noraneko-settings/...`; HTTP `localhost` or `127.0.0.1` additionally
requires `nora.dev.allow_http_loader` and the trusted Settings port resolved by
`launchDevPorts()`. Actor registration or a same-origin URL alone does not grant
parent authority.

The page sends a fresh `requestId` on connect/resync. The parent creates a
connection nonce; snapshots and page acknowledgements carry both values, and the
parent validates them against the current Actor connection and latest snapshot
before accepting an acknowledgement. The Settings `FeatureHubProvider`
acknowledges only after React has committed the matching view. Dynamic sidebar
items match the exact canonical pathname; built-in entries retain their existing
prefix matching. If a descriptor disappears while its URL remains open, the page
renders the localized unavailable section at that same URL rather than
redirecting or presenting stale content.

Native tests that inject fake parent clients into the packaged system ESM remain
core registry/lifecycle coverage only. They are separate from the real
Actor/React page-transport acceptance tests below and do not establish visual or
assistive-technology behavior.

### Verified Stage 2 packaged acceptance

Packaged-asset acceptance in `dev` mode with the HTTP loader explicitly disabled
passed M0, authorization, and history. The positive M0 path used a top-level
`about:hub` page and chrome Settings companion and completed the handshake and
publication. Authorization used live controlled HTTP HTML documents returning
200 from the Settings origin, wrong port, and dotted-host wrong origin. Wrong
origins and the HTTP-loader-off Settings document were rejected at the
registration boundary before parent discovery, with no HUB admission. A
private-window content document obtained an actual Actor but was rejected by the
parent as ineligible. These are development-mode packaged results, not
production-loader qualification.

The Back result was ordinary history: `pageshow.persisted` was false and both
WindowGlobal and inner-window identities changed. BFCache behavior is
unverified. The uninstrumented native feature-host group passed 8/8, and the
Settings provider acceptance passed 1/1. Native checks passed for all 24 changed
M1 module files. The scoped Pages check reports 15 Settings diagnostics (13
TS7017 and 2 TS2345); identical signatures reproduce on an isolated M0-base
tree, so these remain a baseline typecheck limitation rather than new M1
diagnostics.

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
The eight crash-boundary scenarios run with W0 empty and test windows excluded;
existing UI window lifecycle coverage remains in the native FeatureHost tests,
separately. A distinct ninth first-window continuity scenario opens normal W1
and W2 with window exclusion disabled, closes W1 from Marionette-controlled W2,
then updates, disables, enables, and shuts down the host. This continuity
evidence is Linux x86_64 only; it makes no heap-leak, isolation, or other-OS
claim. The supervisor records the durable journal snapshots, process identities,
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
been verified. HUB UI readiness, signing, migration, and production loader
integration are not included.

## Design acceptance cases

The case names and numbers below match §16 of `floorp-hotfix-design-v0.2.md`.
Statuses describe this local prototype and its colocated tests, not full M0
acceptance.

| #  | Design case                                       | Slice status                                                                                                                                                                                                        |
| -- | ------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1  | import top-level failure                          | Partial: broken fixture rejection exists; legacy ownership is out of scope.                                                                                                                                         |
| 2  | install                                           | Pinned fixture installation is pointer-only and performs zero evaluation.                                                                                                                                           |
| 3  | disabled update crash                             | Native synthetic case and Linux process-kill `PREPARED` case restore the before pointer without evaluation.                                                                                                         |
| 4  | enable failure / disable crash intent             | Native recovery and Linux process kills cover interrupted enable and durable disable intent without evaluation.                                                                                                     |
| 5  | v1→v2→v1 and re-enable                            | Existing native lifecycle test exercises fresh sandbox generations.                                                                                                                                                 |
| 6  | old callback/disposer delay                       | Partial: stale wrappers and broker callbacks are measured; no sandbox security or heap-leak claim.                                                                                                                  |
| 7  | hook needs task release                           | Native: abort precedes deactivate; released tasks drain concurrently with global/window disposal under one deadline.                                                                                                |
| 8  | concurrent dispose / hook throw / timeout         | Native: task, hook, child/window, resource, deadline, and sandbox teardown outcomes are aggregated.                                                                                                                 |
| 9  | W0 capture / window churn                         | Native: W0/window churn tests; Linux x86_64 process case proves real W1-to-W2 continuity.                                                                                                                           |
| 10 | no HUB tab / ack / live disconnect                | Core: empty W0 and live/closed/stale acks. Actor: allowed Settings/loopback, wrong port/origin/private/iframe exclusion, and genuine page ack; packaged HTTP-off rejects before parent discovery with no admission. |
| 11 | precommit / revision update                       | Native tests cover pending replay, descriptor conflict ordering, dequeue revision checks, and bounded-queue busy.                                                                                                   |
| 12 | postcommit publication / attach failure           | Core: ack-gated commit and post-commit failures. Actor/React: second-window admission and ordinary history; no BFCache proof.                                                                                       |
| 13 | PREPARED crash recovery                           | Schema-2 recovery has Linux process-kill coverage at PREPARED, OLD_STOPPED, CANDIDATE_READY, and disabled update.                                                                                                   |
| 14 | interrupted / repeated attempts                   | Linux process kills repeat fallback-before-mark and verify bounded digest exhaustion without retry.                                                                                                                 |
| 15 | rejected selected package / corrupt record        | Preflight rejection may use one proof-eligible fallback; malformed/incompatible records evaluate nothing.                                                                                                           |
| 16 | candidate failure / old restore / cleanup failure | Candidate failure restores only a current-health before generation within the two-candidate budget.                                                                                                                 |
| 17 | private-mode existing feature migration           | Deferred to M2.                                                                                                                                                                                                     |
| 18 | fifty-update session                              | Native test compares resource, window-scope, task, and timer baselines and fresh wrappers across 50 updates.                                                                                                        |
| 19 | profile/data compatibility                        | Deferred to M2; no existing feature data is touched.                                                                                                                                                                |
| 20 | terminal write after publish                      | Before/after-replace fault paths preserve durable success when present and block the current runtime.                                                                                                               |
| 21 | new request / lost response / txId reuse / expiry | Native: detached replay; 32 archived + last persist on reopen; retry unknown/no effects; current-revision tx works.                                                                                                 |
| 22 | host/runtime build tuple change                   | Package compatibility metadata and rollback health proof are separately exact-tuple gated.                                                                                                                          |
| 23 | startup fallback / crash boundary                 | Linux process kills cover committed update publication and fallback recovery; other crash models remain open.                                                                                                       |

The original test files are at the `feature-host/` directory root:
`HubRegistry.test.mts`, `M0HubLifecycle.test.mts`, and `M0FeatureHost.test.mts`.
`HubRegistry.test.mts` and `M0FeatureHost.test.mts` cover core/unit behavior;
`M0HubLifecycle.test.mts` also contains real Actor/React HUB cases. Compile
production modules before starting the browser with
`deno task feles-build build --phase before-mach`, then run each suite with:

```sh
deno task test --near browser-features/modules/modules/feature-host/HubRegistry.test.mts --layer esm
deno task test --near browser-features/modules/modules/feature-host/M0HubLifecycle.test.mts --layer esm
deno task test --near browser-features/modules/modules/feature-host/M0FeatureHost.test.mts --layer esm
```

The 50-update lifecycle case is in `M0FeatureHost.test.mts`'s `runAllTests`. The
grouped `feature-host` directory target runs these suites and the M1 Actor
acceptance files below:
`deno task test --near browser-features/modules/modules/feature-host --layer esm`.
The separate Linux M0 process-death matrix runs with
`deno task test:hotfix-process`; Deno harness tests run with
`deno task test:host`.

The real Actor/React acceptance targets are
`M1HubAuthorizationAcceptance.test.mts`, `M1HubResyncAcceptance.test.mts`,
`M1HubAckFencingAcceptance.test.mts`, `M1HubWindowAdmissionAcceptance.test.mts`,
and `M1HubHistoryAcceptance.test.mts`. Run them individually with:

```sh
deno task test --near browser-features/modules/modules/feature-host/test/M1HubAuthorizationAcceptance.test.mts --layer esm
deno task test --near browser-features/modules/modules/feature-host/test/M1HubResyncAcceptance.test.mts --layer esm
deno task test --near browser-features/modules/modules/feature-host/test/M1HubAckFencingAcceptance.test.mts --layer esm
deno task test --near browser-features/modules/modules/feature-host/test/M1HubWindowAdmissionAcceptance.test.mts --layer esm
deno task test --near browser-features/modules/modules/feature-host/test/M1HubHistoryAcceptance.test.mts --layer esm
```

The Settings React provider/navigation test is run with
`deno task test --near browser-features/pages-settings/test/lib/feature-hub-provider.test.tsx --layer pages`.

Remaining validation gaps: the current Back test was ordinary session history
(`pageshow.persisted` was false and restored WindowGlobal/inner-window identity
changed), so BFCache behavior is unverified. Cross-platform screen-reader/AT and
visual HCM checks, signing/distribution, existing-feature/private migration, and
production-loader integration remain unverified. This development-mode packaged
run does not establish production qualification or source/installed runtime
equivalence. Core-only tests and development-mode Actor tests do not close those
gaps or establish complete M0/M1 acceptance.
