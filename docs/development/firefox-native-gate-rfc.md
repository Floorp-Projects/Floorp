# RFC: original Firefox tabs and session test gate

Status: documentation-only proposal. No workflow or required check is enabled.
Source baseline: Floorp `7b89a91d51534eeec9d591740f808dd49bb6133b`.
Evidence snapshot: 2026-11-19. Counts below describe selected cases, not complete
upstream directories or whole-product qualification.

## Goal and constraints

Run original Firefox tabs and session tests against the exact Floorp product
under review, then require their qualified result on pull requests. Preserve
upstream assertions, expectations, timeouts, harness and support semantics.
Do not weaken CSP or other product security checks to obtain a passing result.
DEV means dev-tools launch; STG means `feles-build stage`; production means the
compiled product. Passing one environment does not establish the other two.

## Evidence and status matrix

| Evidence | Current result | What it does not establish |
| --- | --- | --- |
| Local original tabs cases | 20 passing cases reported by the validation lane | Hosted producer provenance or the complete tabs directory |
| Local clean SessionStore cases | 64 distinct cases passing across four isolated batches (9, 16, 20, and 19) | Full SessionStore coverage or integrated product correctness |
| Isolated restart chunks | 8 methods passed in two completed green batches; a separate batch had 7 passes and one driver error | A complete restart suite, normal onboarding, or an integrated hosted pass |
| Hidden-tab restore | Reproduction established; integrated fix not qualified | A clean product-wide result |
| Historical Actions artifact transport | Actual ZIP hash matches independent API digest | Original producer-attempt proof or original Firefox test semantics |
| Historical wrapper evidence | Rejected as native qualification: replaced harness, support not loaded, no original raw-log envelope | Cannot be promoted to native evidence because its workflow was green |
| Inactive source validators and wiring | Host checks and negative replay exist | Deployed isolation, trusted policy configuration or hosted native success |
| PR gate | Not enabled or mandatory | No new protection or passing status is supplied by this RFC |

The local counts are a validation-lane status summary, not an independent replay
performed by this documentation change. Retain exact per-run manifests, product
identities and raw records before using those cases as the hosted starting set.
Do not sum different scopes or replace missing results with host-unit-test counts.

## Proposed trust boundary

An independently reviewed immutable manual workflow owns a single static
producer job and a separate read-only collector. Candidate code must not control
the observer, upload credentials, authoritative outputs or frozen evidence.
OS/guest isolation and a trusted supervisor are implementation prerequisites,
not properties conferred by a workflow label or `permissions: {}`. Protecting
the receipt channel does not, by itself, authenticate observations made inside
a browser process controlled by the candidate.

The producer records its original run ID and attempt and exports the pinned
upload action's actual artifact ID and digest. The collector receives actual
direct `needs` outputs and independently resolves the numeric producer job ID
from complete, exact-attempt jobs API pages. A job name is usable only when the
reviewed workflow proves it unique and static, without matrix/reusable expansion.

The collector compares the actual downloaded ZIP hash with upload output and
REST metadata, then validates original raw test terminals and independently
observed process exit. Artifact metadata does not establish original upload
attempt; API job records do not supply job outputs. Never reconstruct either
from names or log text. Consumer-only reruns with stale producer outputs fail.

Keep PR head, base, actual tested merge/product, workflow commit, Runtime and
corpus identities distinct. Recheck the candidate before accepting a result.
A separate approved policy revision binds the immutable workflow; policy cannot
prove its own authorization merely by repeating these values.

## Remaining implementation versus configuration

| Owner | Required work |
| --- | --- |
| Code and tests | Resolve hidden restore with regression tests; qualify integrated product; preserve original harness/support and exact case manifests |
| Producer implementation | Isolate candidate execution from supervisor/uploader; observe real process status; freeze raw bytes and bind executed product identities |
| Collector implementation | Complete authenticated artifact transport and live producer/output binding; reject incomplete, stale, skipped, cancelled or ambiguous evidence |
| Repository/infrastructure owner | Review actual runner isolation, independent policy approval and configuration ACLs; prevent unapproved variable fallback; approve the usable workflow |
| Independent reviewer | Verify genuine complete hosted positives and operational negatives, without inferring them from local or synthetic results |
| Later reporting change | Publish only to the exact rechecked PR head with the expected App identity; review required-check configuration separately |

## Rollout and acceptance

1. Keep existing required checks unchanged. Do not activate an always-blocked
   placeholder merely to create a check name.
2. Finish and review the usable producer/collector and actual policy/runner
   configuration. Keep upload disabled until those boundaries are demonstrated.
3. Run a nonrequired, nonreporting hosted shadow using the unchanged selected
   tabs/session cases. Local logs may inform review, but must not be uploaded as
   though produced by this hosted execution.
4. Obtain a genuine whole-run positive with exact product/corpus identities,
   terminal original raw events and independently observed zero exit. Confirm
   real cancellation, failure, timeout, missing-artifact and consumer-only-rerun
   cases do not qualify. Do not manufacture these by editing saved API JSON.
5. Independently review the results and environment differences. Then add a
   separate exact-head reporting path; no candidate execution with write tokens.
6. Only after reproducible qualification, require a distinct proposed check such
   as `Firefox original tests / tabs-session`, bound to its expected App.

Only qualified original evidence may produce success. Missing or unsupported
evidence is not success; skip, neutral, workflow-level green and
`continue-on-error` cannot substitute. The target is a genuinely passing gate,
not a permanently failing skeleton or a renamed existing check.

## References

- [GitHub needs and run contexts](https://docs.github.com/en/actions/reference/workflows-and-actions/contexts#needs-context)
- [Exact-attempt jobs API](https://docs.github.com/en/rest/actions/workflow-jobs#list-jobs-for-a-workflow-run-attempt)
- [Artifact metadata API](https://docs.github.com/en/rest/actions/artifacts#get-an-artifact)
- [Pinned upload action outputs](https://github.com/actions/upload-artifact/blob/ea165f8d65b6e75b540449e92b4886f43607fa02/action.yml)
- [Existing Linux pilot proposal](https://github.com/Floorp-Projects/Floorp/pull/3138)
- [Strict Runtime proposal](https://github.com/Floorp-Projects/Floorp-Runtime/pull/117)

This RFC neither supersedes those proposals nor claims their implementation.
