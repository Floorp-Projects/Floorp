# Independent Runtime protocol and workflow review: c902 / 8bbe

Reviewed immutable local commit: `c9027879e053c9031c7f3e27495f6d415d36c10f`, on integrated predecessor `cc83e6fea6499d6ec89c76ae9837c280556e9d53`. Source `.github/workflows/scripts/qa3_native_io.py` SHA-256 `8bbea4abfc5d8cf1a3b70a275287c7fe6725b96fa17afb4c74ef51814b7d142e`; tests SHA-256 `0dae5c088abd51a84338a62c977a7a43e5765dfb1fd8a0647090b10fcab625a6`. Both match the author's frozen candidate. The author committed the same bytes during the read-only review; no reviewer source changes or CI actions occurred. Initial cc83 provenance and initial snapshot are retained separately from the final committed preservation record.

**Scoped result: Go for the typed private protocol repair, existing Draft review and ordinary public PR/shadow CI. No new P0/P1/P2 finding is confirmed in this protocol/workflow scope. New private native execution and signing/publication remain No-Go under the existing prerequisites and authorized scope.** This is not a native qualification or publication approval.

## Independent validation

`workflow-runtime-final-8bbea4ab-protocol-fixture.py` imports the immutable source snapshot and injects synthetic serialization only into its trusted fork producer. It runs under 256 MiB address-space, 12-second CPU, 16 MiB file-write and zero-core limits, without native/browser execution, network, credentials or repository mutation. Seventy private-wire cases and one public `run_owned` policy-forwarding case pass: **71/71**, with no typed-guard mismatches.

The original 25 cases are preserved in name and expectation. All 25 now match expectations; all nine previously malformed-but-accepted variants now reject: false/zero/empty-string/empty-object errors, extra envelope key, null/scalar success result, incomplete cleanup and boolean exit. The old f800 RED results remain immutable and must not be reported as successful evidence.

Additional cases independently confirm:

- `_supervisor_reply`, source lines 568–643: exactly three envelope keys; error is precisely None for a successful reply; successful result has exactly nine required fields; cleanup is precisely true and stop precisely None; name/argv/data-role match the parent's expected command. Missing/extra fields, changed command and changed stdout role fail.
- Success exit must be a genuine integer. Default integer nonzero fails; integer 1 and signal-style -9 pass only with explicit boolean `allow_nonzero=True`. Boolean, float, string or null exits fail even with the allowance. String/integer allowances fail before fork.
- Duration is nonnegative, numeric and finite; both stream hashes are lowercase 64-character hexadecimal strings. The fixture uses fake hashes and therefore proves protocol shape validation, not log-content authenticity. Actual `_run_owned` still computes both hashes from closed log files; the unchanged build caller separately compares tool-version stdout SHA-256 with the pinned expected hash.
- Failure requires null result plus an exact typed error object. Empty/false errors, mixed success-and-error, missing/extra error keys and untyped errno/filename/args/message fail. Ordinary OSError errno/filename survive; `SystemExit(0)`, `BaseException` and `KeyboardInterrupt` payloads become ordinary failure exceptions rather than process-success control flow.
- Budget usage must be a genuine integer between previous usage and maximum, inclusive. Valid success and OSError replies preserve monotonic accounting. Bool/float/string/null/regressed/oversized budgets fail. A caller without a budget accepts only null usage. A malformed budget cannot be bypassed by an error payload.
- Public `run_owned`, source lines 816–852, forwards exactly name, argv, stdout-is-data and the explicit nonzero policy to the validator. Its original public signature is unchanged.

Results and full log: `workflow-runtime-final-8bbea4ab-protocol-results.json`, `workflow-runtime-final-8bbea4ab-protocol.log`. The fixture's final successful exit and all 71 case records were checked independently; no suite-count inference is used.

## Parent initialization and cancellation boundary

Static review of `_supervise`, source lines 646–813, confirms that default SIGCHLD and a single caller thread are required before fork. Parent setup/reader/close failures enter a common finally block. It retains/retries closure of both control descriptors, attempts TERM then KILL and bounded reaping, and fails on cleanup errors. A pidfd identifies the own supervisor where supported. If unavailable or signaling fails, `waitid(...WNOWAIT)` retains the caller's unreaped fork-child lifetime before signaling that child; it no longer requires a successful initial `/proc` birth snapshot to perform cleanup. No parent-wide subreaper adoption or global process kill is introduced.

Child setup and serialization are inside its own finally/exit boundary. Nonzero supervisor status, absent/invalid wire, pipe overrun or cleanup failure cannot return a successful result. The private writer is close-on-exec; actual target Popen retains its default close-fds behavior. SIGTERM/SIGINT received by the supervisor set cancellation, the controlled command checks it, and cancellation after command cleanup also produces a failure. Parent exceptions during wait are recorded while remaining bounded cleanup phases continue.

The actual owned-process parent-initialization regressions are assigned to the separate portability reviewer; this report does not substitute synthetic protocol cases or static reasoning for their real-process evidence. The existing `_run_owned` cleanup implementation is AST-identical to cc83. Hard supervisor SIGKILL/STOP, caller death or an uninterruptible kernel stop still require external dedicated job/cgroup/VM teardown; logical PID ownership is not universal containment. This known prerequisite remains a reason to withhold new private execution, and is independent of allowed hosted PR contracts.

## Integration and unchanged workflow gates

`workflow-runtime-final-8bbea4ab-committed-preservation.json` records all preservation checks as true; the full initial comparisons are in `workflow-runtime-final-8bbea4ab-snapshot.json`.

- Final #83 `91e94b73617730d54ffee3d4ea42ff3a91463c7d` remains an ancestor of c902. Setup, profile and profile-test files are byte-identical #83 → #84. All eight mozconfigs remain unchanged from the frozen earlier #83. Other five integrated repair files remain at the previously reviewed hashes. Their already independently successful 12-method safe-stub setup suite is retained; it was not repeated just to enlarge counts.
- Runtime #83's 22, Runtime #84's 23 and Floorp #2889's 21 workflow YAML files are byte-identical to their frozen pre-QA309 sources. Ten native caller, source/transport/qualification scripts remain byte-identical. Original `run_owned` public arguments, sixteen nonprocess functions, `_run_owned`, `_wait_supervisor`, IOBudget and ExpansionBudget remain unchanged in AST.
- Source preparation still selects canonical Linux x86_64 Debug without PGO. Five canonical Debug profiles retain explicit tests; optimized production/PGO stays on its prior route, and explicitly allowed legacy Debug+PGO remains noncanonical and ineligible. No workflow/schema/qualification/publication policy change is introduced by c902.
- Runtime `qa3-shadow.yml` is the unchanged read-only Ubuntu PR/manual contract job with job-level continue-on-error. It records the actual unit-test exit, then runs policy preflight and uploads evidence with `if: always()`. Final native/product qualification remains UNVERIFIED and publicationAuthorized false. A green workflow alone is insufficient; the raw contract result and expected step conclusions must also pass.
- `qa3-linux-debug-proof.yml` remains reusable-only with no workflow caller in these fixed sources. Private prerequisite precedes self-hosted producer, then consumer; inputs bind R/W, plan, both toolchains, Python environment and launcher. Artifact transport binds exact IDs, digests, run and attempt. No new PR auto-trigger or publication edge is added. Its run-id/attempt concurrency group remains per-run and does not serialize independent runs.
- Existing stable package → five artifact verification jobs → immutable assembled bundle → publisher dependencies and protected environment declarations remain unchanged. The intended QA3 source-once → final QA → approval → fresh revalidation → publish ordering is **not implemented as a new required publisher dependency**. Final-package validation remains a separate manual workflow. The existing release-check inspects a release already made public. Actual protected-environment reviewer/settings enforcement was not queried, so source declarations are not claimed as verified repository settings. Alpha's existing direct-public route is unchanged. Prior review `review-qa308/evidence/workflow-review.md` contains the detailed fixed-source tracing.

## Public CI evidence and final-head observation

Already independently observed evidence remains valid for the heads it records:

- Runtime #83 final 91e94: PR shadow run `37949737610`, attempt 1, expected workflow path/head/PR association, required contract/preflight/upload steps success, raw 26 tests/OK. The checkout merge was `0caeb02c0b1fe94f85d1d8271188064bd00256f6`, merging that PR head into c74fe1. Evidence prefix `workflow-runtime-final-pr83-`.
- Floorp #2889 final 7747: ordinary colocated run `37949019418`, attempt 1, all five expected hosted jobs success. Linux locked-prebuilt browser, execution coverage and runner unit steps succeed; the other three matrix jobs' artifact checks succeed. PR-only PAT metadata and matrix-specific skips were checked as conditional skips, not counted as missing tests. Evidence prefix `workflow-runtime-final-floorp-colocated-`.

Floorp's colocated workflow is ordinary hosted CI and has no QA3 private-launch/sign/publish edge. Its PAT metadata step excludes pull_request. This statement is deliberately confined to that workflow: the separate unchanged moderation workflow has its own PR event, model secrets and write permissions. A private proof No-Go does not justify canceling existing public PR QA. No CI was dispatched, restarted or canceled by this reviewer.

c902 was local and unpushed when this review was bound; there is no claim that a GitHub run tested it. After an authorized push, root should observe the normal autostart run by **that exact PR head**, PR association and `.github/workflows/qa3-shadow.yml` path, then inspect the specific attempt's jobs and raw contract log/exit evidence. Earlier #83, cc83 or a canceled/skipped run cannot substitute for final #84-head PASS. The exact harmless command plan is retained in `workflow-observation-plan.md`. Raw count for the predecessor whole-Runtime suite was 190, not 191; the new 47 native test methods are an AST count and are not represented here as this reviewer's full-suite execution.

| Scope | Decision | Basis / remaining limit |
| --- | --- | --- |
| Typed protocol repair at c902 | Go | Independent 71/71; former nine malformed acceptances reject |
| Existing Drafts, ordinary hosted PR CI and shadow contracts | Go | Guardrails/callers/profiles preserved; final #84 autostart evidence must be bound after push |
| New private native execution | No-Go | External launcher, dedicated teardown, firewall/ephemeral worker, resource/free-space prerequisites remain unproven; actual parent-init review is separate |
| Signing, publication or product/native qualification | No-Go | Outside current authorization; publication false/qualification UNVERIFIED; desired QA3 publication gate not wired |

Review mutations were confined to uniquely prefixed evidence/fixture files. No credentials were read and no source, commit, push, workflow setting, dispatch/cancel, native build, signature or release operation was performed.
