# Six-finding follow-up

The comprehensive review found one P1 and five P2 defects in the separate
Floorp/Runtime QA3 foundation. This follow-up repairs those code paths while
retaining the original review, first failing regressions, the v0.3.0 input and
the separate review-fixes.1 kit archive.

The [frozen comprehensive review](COMPREHENSIVE_REVIEW_QA308.md) remains 24502
bytes, SHA256
`26b8b073b2cdf3793b57f858634ecb150b6c4150c033b4f514ff2170a0bcc84f`. Its relative
evidence links refer to the complete
[427-entry review bundle](COMPREHENSIVE_REVIEW_QA308_Evidence.zip), 2238033
bytes, SHA256
`60b5386d5363edafc4fac738e32c7dd59336db2d23fffb2ba8940a81f65c91d2`. Expand it
into a new directory to read the original report with its fixtures, logs and
hash index. This historical bundle describes the pre-fix commits.

| Review finding | Change and acceptance check                                                                                                                                                                                                                                                                                        |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| P1-NEW-01      | Runtime command cleanup remains mandatory after post-spawn initialization or log-I/O exceptions. TERM/KILL/reap must finish without invoking the failing log sink. The initial exception and cleanup result are retained when the receipt destination remains writable.                                            |
| P2-NEW-01      | The real Runtime setup entry validates target, boolean inputs and complete PGO mode/profile combinations before setup actions, then verifies the generated mozconfig before package installation/bootstrap. Production PGO generation/use, Linux ARM opt and explicit noncanonical legacy Debug+PGO are preserved. |
| P2-NEW-02      | Runtime `test_status` requires a present `subtest` that is JSON null or a string, matching the upstream writer's post-conversion type. Empty/whitespace strings remain valid; raw boolean, number and container values are rejected.                                                                               |
| P2-NEW-03      | Runtime mozlog uses a bounded byte `readline`, caps each event at 1 MiB and rejects oversized or unterminated events before JSON parsing.                                                                                                                                                                          |
| P2-NEW-04      | Floorp JUnit accepts UTF-8 with an optional UTF-8 BOM, rejects NULs/non-UTF-8 declarations and DTD/entity declarations before parsing decoded text. UTF-16/32 DTD inputs cannot bypass the declaration check.                                                                                                      |
| P2-NEW-05      | Floorp commands run under an isolated subreaper supervisor that owns only the command's descendants. Detached children and forks during termination are reaped, with process birth/pidfd checks. The caller's unrelated children and subreaper state remain unchanged.                                             |

The command-supervisor boundary also addresses a safety gap exposed while
implementing P1: an unrelated prior process can fork a detached child and exit
during the command. Adopting every new direct child in the calling process would
incorrectly claim that unrelated orphan. Cleanup ownership therefore belongs to
the dedicated command supervisor, rather than the caller.

## Scope and dependency

The code changes belong to the existing Drafts
[Floorp #2889](https://github.com/Floorp-Projects/Floorp/pull/2889),
[Runtime #83](https://github.com/Floorp-Projects/Floorp-Runtime/pull/83) and
[Runtime #84](https://github.com/Floorp-Projects/Floorp-Runtime/pull/84).
Runtime #84 remains stacked on #83. This
[documentation Draft #2893](https://github.com/Floorp-Projects/Floorp/pull/2893)
retains supporting review evidence without installing the old kit into active
tools. The original design and 14 kit documents remain unchanged.

Existing release/build/PGO/signing/publisher workflows and their protected gates
remain unchanged. Source shadow is nonblocking, and its raw failure must be read
independently of an overall workflow success. No test retry is introduced.
Native/product qualification stays `UNVERIFIED` and
`publicationAuthorized=false`.

## Validation record

The [indexed follow-up evidence](followup-evidence-index.json) retains initial
red regressions, intermediate candidates, local checks and independent reports
as separate files. A later PASS does not overwrite a prior FAIL. Absolute
`/workspace` paths identify the original consumer environment; use the index's
relative paths to inspect this copy. Local process/log/configuration fixtures do
not establish native build, harness, product artifact or cross-platform
qualification.

The [follow-up ZIP](FOLLOWUP_QA309_Evidence.zip) contains this report and the
indexed evidence as regular files. Its external
[archive validation](followup-archive-validation.json) records byte length,
SHA256, member count, CRC, path/type/encryption checks and the public-content
scan. Private download descriptors and build caches are excluded. Original local
transport errors containing temporary URLs remain locally retained; separately
named redacted copies preserve their failure messages here.

Floorp #2889 is fixed at `7747cf0e711937e056a34594d8d39fcc8dfc54f0`: 22 contract
methods and the unchanged 106-case source plan pass with zero skips. The source
plan comprises 60 Deno, 28 Node and 18 Python cases; all three command exits are
zero. Its clean-tree receipt records runner SHA256
`5cb0803d54c47197e0597cd93de436c60cb2f667340f5bcff55e6f0326b30d32`, source PASS,
native/product `UNVERIFIED` and `publicationAuthorized=false`. Two independent
reviewers exercised XML encodings, malformed supervisor responses, actual
detached/late-fork children, transient cleanup failures, cancellation and
preservation of unrelated children. The final reports are
[portability review](followup-evidence/floorp-independent-review-02.md) and
[workflow review](followup-evidence/workflow-review-final-floorp-report.md).

Runtime #83 is fixed at `91e94b73617730d54ffee3d4ea42ff3a91463c7d`. Twelve
build-profile/setup methods and 14 adjacent source-cohort methods pass. The
independent real-entry stub fixtures reject invalid inputs before any setup
action. All 26 complete generated configs match the prior bytes for five Debug,
thirteen production and eight explicit legacy Debug+PGO combinations. The
[independent setup review](followup-evidence/setup-independent-review.md) also
records Linux ARM production-opt and both PGO generate/use preservation. The
canonical Debug test-enabling change belongs to the existing #83 foundation;
this follow-up preserves that behavior.

Runtime #84 is fixed at `c9027879e053c9031c7f3e27495f6d415d36c10f`, with #83's
final head as an ancestor and its setup files inherited byte-for-byte. The
complete Runtime suite passes **203 methods / 56.217s / zero skips** at this
clean committed head. The prior broad run at `cc83e6fea6` passed **190**
methods, but two independent parent-init fixtures still exposed a blocking P1;
broad success alone did not close that defect. A provisional commentary count of
191 is corrected in the retained
[count reconciliation](followup-evidence/runtime-count-reconciliation-01.json).

The final supervisor source SHA256 is
`8bbea4abfc5d8cf1a3b70a275287c7fe6725b96fa17afb4c74ef51814b7d142e`; its test
SHA256 is `0dae5c088abd51a84338a62c977a7a43e5765dfb1fd8a0647090b10fcab625a6`.
The supervisor retains its own fork-child lifetime using pidfd or a non-reaping
`waitid(WNOWAIT)` fallback. Missing `/proc` identity does not suppress cleanup.
Single-thread/default-SIGCHLD preconditions reject before forking, parent
control FDs are closed, and secondary cleanup faults cannot replace the first
error. The internal reply requires exact typed fields, successful cleanup,
command/data-role matching and a monotone bounded staging ledger. Explicit
boolean `allow_nonzero=True` is required to accept an integer nonzero exit.
Malformed or unsafe exception payloads, including `SystemExit(0)`, fail closed.
This strengthens a trusted private producer protocol; it is not evidence of an
untrusted target credential or qualification bypass.

The
[independent native review](followup-evidence/runtime-independent-review-02.md)
reruns all **47 native-helper methods** and five independently written actual
process faults. Both formerly failing initializations now return the original
EIO after command/supervisor recovery, with closed control FDs and completed
cancellation receipts **before** fixture safety cleanup. Real ignored SIGCHLD, a
second OS thread, fork failure, missing pidfd and secondary close errors are
also checked. A second
[independent protocol/gate review](followup-evidence/workflow-runtime-final-8bbea4ab-report.md)
passes **71 bounded cases**, rejects all nine previously accepted malformed
variants, and confirms unchanged workflows/callers/configs and #83 inheritance.
The source, setup, mozlog, native and documentation reviews give scoped Go for
these fixes and existing ordinary public CI, with no remaining confirmed
P0/P1/P2 in that changed scope.

The mozlog changes have 21 passing contract methods and an independent 36-case
writer/line-boundary review: 14 accepted fixtures and 22 rejected fixtures. The
review checks exact upstream JSONFormatter newline framing and Unicode/null
conversion, byte and multibyte boundaries, LF/CRLF, a real 3-MiB overlong line,
and a lazy one-million-event budget. The 2-GiB whole-file cap is tested through
a mocked stat result; no 2-GiB fixture is allocated. See the
[independent mozlog review](followup-evidence/mozlog-independent-review.md).

During implementation, independent reviewers retained additional red cases:
Floorp cleanup interrupted after a successful leader exit, a transient EMFILE
while closing children, and Runtime parent initialization failure before a
usable process-birth snapshot. They also checked strict types on the private
Runtime supervisor reply. These cases are part of the final cleanup acceptance,
not native qualification or trusted product attestations.

Existing standard public GitHub-hosted PR CI is authorized to run automatically
on these updates. Failed, skipped, cancelled and missing checks are not accepted
as PASS; a result for an older PR head does not validate a newer commit. The
private full Debug proof and new paid Blacksmith runs remain unexecuted.

At the Floorp code head above, ordinary public CI completed successfully on
attempt 1:

| Workflow                                                                                              | Required result and evidence                                                                                                                                                                                             |
| ----------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| [Source shadow 37949019450](https://github.com/Floorp-Projects/Floorp/actions/runs/37949019450)       | Raw job log confirms 22 contract methods, 106 source tests and three zero exits. Actual PR merge checkout is `dd09b0ae176394fc3cc14eef579018d3ebb5ba43`, merging the reviewed code head into `main@013bb6c9`.            |
| [Docs 37949019544](https://github.com/Floorp-Projects/Floorp/actions/runs/37949019544)                | Required verification succeeds. The conditional artifact-generation job is skipped and is excluded from PASS claims.                                                                                                     |
| [Browser integration 37949019418](https://github.com/Floorp-Projects/Floorp/actions/runs/37949019418) | Smoke and all four existing matrix jobs complete successfully. Linux x64's combined browser and coverage-verification steps succeed. Other targets' conditional browser skips establish no native browser qualification. |

Runtime #83's final-head
[shadow run 37949737610](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/37949737610)
also completes on attempt 1. Raw logs confirm 26 methods and required
preflight/evidence-upload steps. Actual PR merge checkout
`0caeb02c0b1fe94f85d1d8271188064bd00256f6` joins the reviewed #83 head to
`nora-0.2.0@c74fe1a`. These counts are distinct from #84's complete Runtime
suite.

At Runtime #84's final head,
[shadow run 37954821322](https://github.com/Floorp-Projects/Floorp-Runtime/actions/runs/37954821322)
also completes on attempt 1. Raw logs confirm **134 QA3 methods / 49.212s /
OK**, with every required contracts/preflight/upload step successful. The actual
PR merge checkout is `7e4b7ce818aadc5cfd41f58a79b82ed3487d4b6f`, joining the
final #84 head to #83's `91e94b7361`. This is the `test_qa3_*.py` subset,
separate from the complete 203-method local suite. The
[independent CI addendum](followup-evidence/workflow-runtime-final-8bbea4ab-ci-report.md)
checks head, PR/base, attempt, workflow, required steps and decoded log. Overall
shadow workflow success alone would not establish the contracts' success.

This documentation Draft's final-head ordinary CI is checked after this record
is committed and pushed. The final run/head/attempt and job results will be
recorded in its PR description; no older/cancelled head is accepted in its
place.

Decoded CI logs and run/job/attempt metadata are verified through the GitHub
connector. The source/integration artifact download action returned service
references, but reading the exact provided URLs in this consumer environment
failed HTTP 403. No URL/authentication workaround was attempted. The ZIP bytes,
full CI receipt payloads and exact Linux browser assertion count remain
`UNVERIFIED`. GitHub artifact digest metadata is retained without claiming a
local-byte verification. See the
[materialization failure](followup-evidence/ci-artifact-materialization.json).

Targeted Runtime `mach lint --linter ruff`, Ruff check/format, Bash syntax and
diff checks pass. Full modified-file `mach lint` remains blocked during linter
registration by the sparse checkout's missing `third_party/node/pnpm-lock.yaml`;
the full setup worktree also lacks pinned offline lint-venv dependencies. Those
failures remain in the evidence. They are not relabeled PASS. Shellcheck is
unavailable.

## Remaining blockers

These fixes do not complete the authenticated source-cohort/build/target/attempt
contract, trusted first-failure history, issuer/expiry/revocation, provisioning
of private isolated workers, all five native targets or final signed/transformed
artifact qualification. The required future qualification order is
`bundle -> final QA -> manual approval -> fresh evidence revalidation -> publish`.
That QA3 chain is not wired into the current publisher. Its existing release
checks, including the requirement for an already manually public release, remain
unchanged by these Drafts. Merge, release, signing, notarization, publication,
credential creation and production setting changes are outside this follow-up.
