# Final package QA / publish gate design (KAN-133)

Status: PARTIAL. This change lands **Alpha environment wiring only** for KAN-133 D-21.
The stable release final-package-QA publish dependency is **not implemented** here and
KAN-133 must not be recorded as completed from this document or the accompanying
workflow change.

## Landed in this change

- `.github/workflows/publish_alpha.yml`: the `main` job now declares
  `environment: Deploy-to-installer-alpha`. GitHub evaluates environment protection
  rules on the job that references the environment, so the declaration belongs on the
  job inside the called workflow; a caller job cannot set `environment`
  (https://docs.github.com/en/actions/how-tos/reuse-automations/reuse-workflows).
  Creating the environment and its required reviewers is repository-admin (human)
  work, not part of this code change.
- `tools/src/fdh_publish_workflow.test.ts`: static text-level regression tests for the
  Alpha gate/wiring, the unchanged Alpha publication contract, the still-dispatch-only
  final package QA scope (Windows shadow preserved), and the current stable publisher
  boundaries.
- This design note.

## Observed evidence (read-only REST, 2026-11-13; no workflow was triggered)

Production package run: https://github.com/Floorp-Projects/Floorp/actions/runs/43359316284
(workflow `package_and_publish_release.yml`, `main`, head
`9e6b24e5fd561250ab04a011b70ee0b1280b1adc`). Observed UTC timeline:

| time | job |
| --- | --- |
| 04:02:11 | Assemble verified release bundle completed |
| 04:03:36 | Publish verified release / Validate immutable bundle and create draft release completed |
| 04:14:36 | Publish verified release / Manual approval and public release gate started |
| 04:15:03 | same gate completed (about 11 minutes waiting at `Deploy-to-installer-release`) |
| 04:16:40 | Publish and verify stable updater metadata v2 completed |
| 04:20:32 | Build the exact public Floorp Portable release completed |
| 04:26:58 | Publish the exact source-run DEB to PPA completed |

Final package QA run: https://github.com/Floorp-Projects/Floorp/actions/runs/43363023598
(`workflow_dispatch`, `main`, same head). It started 04:03:51 UTC, 15 seconds after the
draft release job completed and while the release gate job was waiting for approval, and
completed 04:07:45 UTC with `conclusion: success`; all four matrix jobs succeeded
(`windows/x86_64`, `linux/x86_64`, `mac/x86_64`, `mac/aarch64`). Its four evidence
artifacts (`final-package-qa-<platform>-<arch>-<qa-run-id>`) are small (about
250-330 KB each).

The QA evidence `input.json` observed in that run binds the QA result to the package
run: `runId = 43359316284`, `floorpHead = 9e6b24e5...`, `unsigned = false`,
`sourceEligibility.phase = awaiting-publication-approval`,
`publicationApprovalPerformed = false`, plus the exact package and native-record
artifact id/name/digest and the immutable bundle id/digest. No evidence artifact is
committed or referenced by local path in this repository; it was read read-only via the
GitHub REST API and contains no secrets.

The workflow contract that makes this ordering necessary is in
https://github.com/Floorp-Projects/Floorp/blob/9e6b24e5fd561250ab04a011b70ee0b1280b1adc/tools/final-package-qa-source.mjs
(`validateFinalPackageSource` with `prePublication: true`): QA requires one pending
deployment on `Deploy-to-installer-release` with required reviewers, the draft release
bound to the same head, the immutable bundle from the same run, and no publication job
executed yet.

## Current publisher gap

`.github/workflows/publish_release.yml` has no dependency on the final package QA run:
it validates the immutable bundle, creates the draft, requires the owner to have made the
draft public, and only then publishes updater metadata, Portable, and PPA.
`validate-final-package-qa.yml` remains an independent `workflow_dispatch` entry
(`pre_publication` input), so a successful automation path does not currently prove that
QA passed.

## Unresolved cyclic dependency (why a dispatch-time run id input is not enough)

1. The signed package run must be dispatched first, because the QA intake needs the
   draft release, the assembled immutable bundle, and the pending protected installer
   approval from that exact run.
2. The QA run can therefore only be created after `prepare_release` and after the
   release gate job is queued and waiting.
3. The operator cannot supply the QA run id at package-run dispatch time, and a second
   publisher invocation cannot bind to a still-waiting (not completed) source run:
   `prepare_release` accepts a prior source run only when it completed successfully, and
   the PPA path requires the exact current source run.
4. GitHub's workflow-run REST object does not expose `workflow_dispatch` inputs
   (`GET /repos/{owner}/{repo}/actions/runs/{run_id}` keys checked read-only: no
   `inputs` field), so the publisher cannot verify `package_run_id`,
   `pre_publication`, or `validation_only` from the run metadata alone
   (https://docs.github.com/en/rest/actions/workflow-runs). `display_title` is only the
   workflow name here.

## Candidate production-graph designs (not activated)

- (a) Reusable workflow call from the same run: add a `workflow_call` interface to the QA
  workflow and a race-safe await job before it. This gives an exact same-run binding but
  edits the QA workflow and must be validated on GitHub.
- (b) Publisher self-dispatch: a gate job in `publish_release.yml` dispatches the QA
  workflow, holds the returned immutable run id, polls it, and validates run identity,
  job set, and evidence. Needs `actions: write` on the gate job and live validation.
- (c) Discovery gate (recommended): keep the current manual QA dispatch, and have a
  `publish_release.yml` gate job find candidate QA runs for the same head/workflow,
  require the full four-job set with no skipped jobs (Windows may be failed-but-present
  shadow), download the small evidence artifacts, and verify the `input.json` binding
  (run id, head, `unsigned: false`, phase, package/record/bundle digests) against the
  package run's actual artifacts. Pure selection/binding logic can live in a
  host-tested `tools/src` helper with fixture-based positive and negative cases.

Each option changes the production publisher graph and needs live GitHub verification
before landing. The later read-only discovery library/CLI is preparation for option (c),
not activation of a production dependency. Synthetic fixtures exercise full archive
composition but do not establish the real signed pending-source positive case. A
validation_only unsigned run cannot satisfy that production-source contract.

The cycle-safe production graph would be prepare_release -> {QA gate, release_check},
with both gates required by every automated distribution path. Making release_check
depend on QA would suppress the very pending deployment the collector requires. The
current collector cannot qualify historic/prior-source resume: that needs a separate
immutable historical-receipt design, never a QA bypass. Manual public-release UI
actions remain outside the graph's control. No production graph patch is activated here.

## Residual limitations

- Windows final package QA remains `continue-on-error` shadow mode; do not claim
  mandatory all-OS release qualification, and do not remove the shadow by fiat.
- Manual publication in the GitHub UI is outside workflow control; the current
  `release_check` deliberately verifies an already-public release. A future gate can
  block automated distribution but cannot prevent a human public-release mutation.
- Alpha cannot be bound to the final package QA run in its current form: the QA intake's
  `validateSourceIdentity` accepts only `package_and_publish_release.yml` runs with
  signed stable-style package/record artifacts, while Alpha packages are built by
  `package.yml` from a runtime artifact run id.
- Environment creation and approver/reviewer configuration (`Deploy-to-installer-alpha`,
  and the existing release environments) are repository-admin human settings; declaring
  the environment in source does not configure reviewers.
- The new host tests read workflow YAML as text; they assert indentation-scoped strings
  and do not parse YAML or evaluate GitHub expressions. They are static regression
  guards, not proof of runtime behavior.
- No workflow was triggered, nothing was published, and no repository settings or
  environments were changed in this round.

## Suggested acceptance for the follow-up

- Gate wiring plus host unit tests for the binding logic (positive and negative
  fixtures: wrong package run id, wrong head, failed/skipped mandatory job, Windows
  shadow, missing/expired evidence, digest mismatch).
- Static workflow tests for dependency ordering and preserved publisher contracts.
- A live, non-destructive observation that the gate fails closed when QA evidence is
  absent and passes only with a valid QA run; document any manual step that remains.
