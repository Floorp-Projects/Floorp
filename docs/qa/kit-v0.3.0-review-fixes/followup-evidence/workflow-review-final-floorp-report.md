# QA309 independent Floorp candidate-03 review

**Result: Go for the existing nonblocking public source shadow and its standard public PR CI autostart.** No new P0/P1 or unresolved candidate protocol blocker was found in this targeted workflow/control review. This is approval of the pinned candidate within the authorized repair scope, not evidence that a later committed head's CI has passed, and not native/product/publication qualification.

## Immutable reviewed candidate

Baseline commit: Floorp #2889 `718e688fbd7ad88a776534e7778ad8e4a7f67364`. Frozen author patch: `floorp-candidate-03.patch`, SHA-256 `30fa4bf57c9c557c6dced39abcfd7854a35b22eb79b9a4c4881da746690edcd4`.

| File | Reviewed SHA-256 |
|---|---|
| `tools/qa3/source.py` | `5cb0803d54c47197e0597cd93de436c60cb2f667340f5bcff55e6f0326b30d32` |
| `tools/qa3/test_source.py` | `a8e50bc710b7a2fce37269df43326d2683d4997d9a40772f5f286f6a7a9dde51` |
| `tools/qa3/README.md` | `c602857927aa6e401b9c647428506f9b568e0ca0e5d747f56fafe5b8bee73ec0` |

Live files matched the captured snapshots and the live three-file diff matched the saved author patch both at capture and after verification. Metadata, per-workflow hashes and AST comparisons are in `workflow-review-final-floorp-candidate-03-snapshot.json`; byte snapshots use the same filename prefix. The candidate is still an uncommitted diff at review time. Bind this review to the eventual commit containing those exact source bytes.

## Findings resolved and control invariants

The candidate-01 residual at `_execute_owned` is resolved: SIGTERM injected immediately before actual cleanup now produces exitCode 0 with `error="source supervisor interrupted"`, empty cleanupErrors and `ownedCleanupComplete=true`. Parent `execute` rejects it. A successful child exit plus successful descendant cleanup therefore cannot erase that interruption.

Candidate-03 `_cleanup_owned` at snapshot line 381 records up to four bounded distinct syscall errors, continues TERM/KILL/reap phases, and returns completion and errors separately. `_execute_owned` at line 467 retains them independently from the initial command error. `execute` at line 534 requires exactly four control keys, typed error/cleanup-error fields, a genuine bool completion field, completion true, no recorded errors and a genuine integer command exit code. It rejects a cleanup error even when completion is true. Missing fields now become a normalized ValueError rather than candidate-01's KeyError.

The supervisor receives the private pipe through its parent's pass_fds. Its target launch has no pass_fds and uses default close_fds=True. An actual controlled target enumerated only stdin `/dev/null` and the two log descriptors; the private pipe was absent. A fake credential variable and NODE_OPTIONS injected solely into the test process were absent from the target. No real credential values were read, recorded or transmitted. Bounded UTF-8 error formatting and `ensure_ascii=False` fit the maximum four-error control record in the parent limit; the independent long mixed Unicode/quote/control-character fixture serialized to 1573 bytes.

Independent fixture `workflow-review-final-floorp-fixture.py` passed **34/34 checks** against the frozen snapshot: cleanup-stage interruption; 27 malformed/typed/errorful control inputs rejected; three valid integer exit codes 0/124/-9 preserved; real control-FD separation; fake-environment filtering; bounded error serialization. Results and full log are `workflow-review-final-floorp-results.json` and `workflow-review-final-floorp-fixture.log`. These are synthetic control and own Python-process tests, not a native build or the source plan's 106 executed cases.

## Workflow and qualification gates preserved

All **21 workflow YAML files** at the candidate checkout remain byte-identical to fixed #2889. Existing package/signature verification needs, artifact ID/digest/subject bindings, protected environment names, updater/PPA dependency edges, public-release byte revalidation and publication concurrency declarations are therefore unchanged by this patch. The declared environment gate is preserved; actual GitHub reviewer/branch-protection settings were not queried here.

AST comparison confirms the five fixed source registry/catalog constants and 11 critical pre-existing functions remain identical, including plan/coverage/case validation, Deno lock binding, `python_tests`, `child_environment`, `run_source` and normal CLI `main`. The fixed 106 source cases and outer source-receipt schema remain unchanged. The supervisor stays in the same `source.py`, preserving runnerSha256's binding to the executed code. Source PASS still leaves product/Runtime/native qualification UNVERIFIED and publicationAuthorized false.

The ordinary Floorp colocated workflow still tests all PRs on the four standard hosted labels; PRs exclude its PAT metadata step. #2889 still path-matches both source shadow and docs PR verification because its cumulative diff contains `.github/workflows/**`. The separate opened/edited moderation workflow is unchanged and must not be described as secret-free colocated CI. No new private proof caller, signing secret, environment approval, workflow_run publication bridge, required QA promotion or publication action was added. Observe final-head runs using `workflow-observation-plan.md`; shadow workflow success alone remains insufficient because its job is continue-on-error.

## Scope decisions and remaining limits

| Scope | Decision | Evidence / remaining condition |
|---|---|---|
| Existing Draft repair and review | Go | Pinned three-file candidate; protocol/guard preservation checks pass |
| Existing public source shadow and standard PR CI autostart | Go | No workflow change or private/publisher edge; final-head runtime outcomes still must be observed |
| New private native proof execution | No-Go / outside this authorization | Runtime P1 repair and launcher/cgroup/worker prerequisites need their own final review; this source patch cannot qualify them |
| Required publication promotion or release | No-Go / outside this authorization | New pre-publication source/final-QA/approval/fresh sidecar chain is still absent; actual environment settings and publication inputs were not validated live |

The reproduced candidate-01 SIGKILL/SIGSTOP supervisor-loss fixture left a controlled child alive while the parent failed closed; the fixture then birth-verified, killed and reaped it. Candidate-03's parent hard-stop fallback is unchanged and README now explicitly documents external whole-worker/job teardown. This remains an honest containment boundary, not a repaired universal SIGKILL/kernel-stall guarantee. It must not be used to justify cancelling normal hosted PR CI; a future shared/self-hosted execution needs a dedicated externally enforced scope.

This reviewer independently checked control/protocol/gates. The portability review owns its additional normal/timeout/cleanup-syscall adversarial matrix; its results are separate evidence, not counted in these 34. No full source plan, hosted run, browser/native/full build, private/paid worker, credential operation, signing, publication, source authorship, commit, push, dispatch, cancellation or settings change was performed here. Runtime final setup propagation and combined final committed heads await the subsequent integration review.
