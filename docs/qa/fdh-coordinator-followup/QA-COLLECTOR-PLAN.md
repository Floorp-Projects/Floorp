# Read-only QA collector — historical audited plan

## Scope and authority
This preserves task-23's preimplementation API/CLI audit. Transport, collector and the lazy FDH route were subsequently implemented and independently verified; see [QA-READONLY-COLLECTOR.md](QA-READONLY-COLLECTOR.md) for current results and limitations. The trust/budget requirements below still apply; no live gate activation is implied. Canonical target: Floorp-Projects/Floorp, repository id437505831. Observed workflow IDs: package163541311, QA377351757; cross-check canonical paths and current state when implementing rather than treating a display name as authority.

No public executable/repository/host/URL/header/token/extra-gh-argument selector is approved. A trusted installed gh binary, owner-managed authentication/configuration/TLS/redirect behavior are explicit host TCB. Absolute path alone does not establish trust; test-only injected dependencies must not become model-controlled CLI capabilities. Use constructed args, no shell, stdin null, explicit hostname github.com and GET-only fixed endpoints. Never print raw stderr/API bodies, signed artifact URLs, environment or credentials.

## Existing live negative
Task-23 observed source43359316284 attempt1 is completed/success, pending deployments empty, and v12.21.2 is already public. The existing source validator rejected that actual snapshot for prepublication as expected. It must NEVER be relabeled as a pending positive. QA43363023598 attempt1 and its four artifact samples are historical compatibility evidence only. Positive pending integration remains synthetic until an authorized real pending run exists.

At pinned source head9e6b24e5fd561250ab04a011b70ee0b1280b1adc, read-only version audit found Floorp12.21.2, runtime lock schema1, Firefox160.0.1 build20261112140843 for relevant platforms. Future expectations must fetch versions at the pinned source head, not use checkout files or decoded QA payloads.

## Canonical GET endpoint families
All under api.github.com/repos/Floorp-Projects/Floorp:

```text
/actions/workflows/package_and_publish_release.yml
/actions/workflows/validate-final-package-qa.yml
/actions/runs/{id}
/actions/runs/{id}/attempts/{attempt}
/actions/runs/{id}/attempts/{attempt}/jobs?per_page=100&page=N
/actions/runs/{id}/artifacts?per_page=100&page=N
/actions/artifacts/{selectedId}
/actions/artifacts/{selectedId}/zip
/contents/package.json?ref={pinnedSourceHead}
/contents/floorp-runtime.lock.json?ref={pinnedSourceHead}
/actions/runs/{sourceId}/pending_deployments
/releases/tags/v{validatedPinnedVersion}
```

Use exact-attempt job endpoints. Run artifacts have no attempt field: keep their real source IDs/heads/repository/branch/timestamps and preserve declared precision. Complete inventory/pagination is required; no prefilter that hides expired or duplicate matches. Actual source inventory32 metadata entries fits composition1..64; selected required identities are not the total inventory. QA job/artifact count4 matches current workflow; future format changes deliberately require review.

## Proposed finite collection sequence
1. Validate explicit safe integer source/QA IDs and attempts, distinct run IDs, separate40lowerhex source/probe heads, explicit Windows policy and bounded freshness policy. Pin repo/workflow/path identities.
2. Read both current and exact-attempt runs, complete attempt jobs and artifact inventories. Reject changed attempt/head/workflow/repository, malformed/unstable/truncated pagination or duplicate IDs; do not deduplicate silently.
3. Read package/runtime versions at source head; real pending deployment and release state. Invoke unchanged authoritative source eligibility helper with prePublication:true,unsigned:false, all source jobs and all named bundle matches. Only a release-tag404 may become null (existing draft visibility limitation); other failures are not evidence.
4. Select/recheck exact four QA artifact IDs/names/digests from full metadata. Download ONLY those four ZIPs, never packages/build outputs. Validate actual streamed bytes, not just size claims.
5. Construct bounded canonical JSON snapshots after projecting needed fields, preserving all array items, expiration/duplicates and original timestamp strings. Invoke the real offline composition, no decoded-evidence or proof-flag injection.
6. Re-read current attempts/heads, selected identities and protected source state; re-run authoritative eligibility and freshness. Reject changes. This is a point-in-time observation, not a lock or human approval.

## Working hard safety ceilings (not SLA)
- Metadata command30s; ZIP command60s; collection300s total, sequential one owned child, no retries/background polling.
- Metadata stdout2MiB/response and16MiB aggregate; project observation JSON<=1MiB, policy<=1MiB. Stderr16KiB, drained concurrently and never forwarded.
- Lists100items/page, at most10pages/1000items transport ceiling, then stricter composition source64/QA4 bounds. Validate total_count/termination/identity stability.
- ZIP8MiB each, **16MiB aggregate aligned with composition**, enforced while reading across four downloads. Earlier32MiB proposal is superseded.
- On limit/abort, cancel readers, terminate only the owned child and await settlement. Windows killed-process exit1 without signal is termination when the abort/limit flag says so, not a fresh command failure.
- Output summary<=256KiB, fixed sanitized codes/stages and public identity summaries only.

The current ext-poll runner has a60s timeout but unbounded output buffering and raw stderr errors; it is not approved unchanged for this collector. Preserve existing commands and lazily load a new route so unrelated local FDH operation/help does not gain new I/O/dependency requirements.

## Implemented command surface (see current collector record)
```text
fdh qa collect --source-run-id N --source-attempt N --source-head SHA \
  --qa-run-id N --qa-attempt N --qa-head SHA \
  --windows-policy require-success|shadow-observation --max-age-ms N
```

Reject unknown/duplicate arguments, overflow, arbitrary host/repository/URL or binary selection. Internal clock only; no caller-supplied now. Initial route should be stdout-only, no ledger/file persistence, browser execution, deployment approval, workflow dispatch or publication.

## Required independent tests
Synthetic endpoint/child injection: fixed methods/host/path, pagination ambiguity, changed attempts/heads/artifacts, version binding, missing protected review state,401/403/404 distinctions, bounded stdout/stderr/ZIP streams, timeouts/reader cancellation/owned-child settlement, byte-budget/race preservation, exact four downloads, redaction and no invocation on invalid CLI args. Real historical completed-source negative is safe; live pending positive remains separate.

No repository settings, authorization server, Runner Protocol, resident service, credentials, publisher workflows or Jira completion state are changed by this plan.
