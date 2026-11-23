# Read-only QA collector — implemented and verified (rounds 6–7), not live-gate activation

## Scope and authority
This record documents the completed read-only QA collector chain — canonical GET-only gh transport, collector, and lazy `fdh qa collect` route — prepared in rounds 6–7 of [CHECKPOINT.md](CHECKPOINT.md). It supersedes the unimplemented-proposal status of [QA-COLLECTOR-PLAN.md](QA-COLLECTOR-PLAN.md) for implementation shape only; that plan's trust boundaries (host TCB, fixed endpoints, budgets, no selector surface) remain binding and unchanged.

Nothing here authorizes publication, approval, or release qualification. Every collected report permanently carries `transportAuthenticityVerified:false`, `apiAuthenticityVerified:false`, `nativeQaVerified:false`, `liveApprovalVerified:false`, `publicationAuthorized:false`. No live pending-positive, native QA rerun, publish, settings change, push or PR has been performed. The operator subsequently authorized local split commits; see [LOCAL-COMMIT-PLAN.md](LOCAL-COMMIT-PLAN.md).

## Implemented files (feature branch `fdh-coordinator-followup`, baseline 9e6b24e)
- Transport: [fdh_qa_gh_transport.ts](../../../tools/src/fdh_qa_gh_transport.ts), [types](../../../tools/src/fdh_qa_gh_transport_types.ts), [tests](../../../tools/src/fdh_qa_gh_transport.test.ts)
- Collector: [fdh_final_package_qa_collector.ts](../../../tools/src/fdh_final_package_qa_collector.ts), [types](../../../tools/src/fdh_final_package_qa_collector_types.ts), [tests](../../../tools/src/fdh_final_package_qa_collector.test.ts)
- CLI route: [fdh_qa_cli.ts](../../../tools/src/fdh_qa_cli.ts), [types](../../../tools/src/fdh_qa_cli_types.ts), [tests](../../../tools/src/fdh_qa_cli.test.ts), plus the raw-argv `qa` intercept in [tools/fdh.ts](../../../tools/fdh.ts)

## Transport (canonical capabilities only)
- Operation kinds are fixed by type: repository, workflow (source/qa), currentRun, attemptRun, jobsPage, artifactsPage, artifactMetadata, selectedZip, pinnedContents (package/runtimeLock at a pinned head), pendingDeployments, releaseTag. Paths are built under `repos/Floorp-Projects/Floorp` only; no repository, host, URL, executable, header, or extra-gh-argument selector exists.
- Child invocation is exactly `gh api --hostname github.com --method GET --include` with two fixed Accept/version headers and the fixed path; no shell, stdin null, one owned child at a time.
- Wire ceilings (`QA_GH_LIMITS`): metadata 2 MiB per command / 16 MiB per session, ZIP 8 MiB per command / 16 MiB aggregate session, stderr 16 KiB (drained, never forwarded), response header block 16 KiB / 128 lines, 30 s metadata / 60 s ZIP / 300 s session, 10 pages, JSON depth 64. These are wire-byte budgets, not total heap bounds.
- Only the final response header block is parsed; redirect/multi-response output fails closed and `Location` is never chased. Status classes: 2xx success, 401 unauthorized, 403 forbidden, 404 notFound, other client/server error.
- Bytes are accepted only after native ArrayBuffer/Uint8Array guards (kind, exact backing buffer, non-resizable, cached intrinsic getters) that reject forwarding/size-spoofing Proxies, extending round 4's byte boundary.
- On limit/abort: readers cancel, the owned child receives SIGKILL, status and both drains settle before return. Windows killed-process exit 1 without a signal is treated as termination only when the abort/limit flag says so. Internal clock only; never caller-supplied time.
- `QaGhTestDependencies`/`QaGhTestChild` are test seams only and are not reachable from any CLI argument.

## Collector (point-in-time, recheck-confirmed)
- Exactly eight options (source/QA run id, attempt, 40-lowercase-hex head, Windows policy, max age ≤ 86400000 ms); distinct run IDs; internal clock; invalid options fail closed before any transport use.
- Pinned identities: repository 437505831, workflows package 163541311 (`.github/workflows/package_and_publish_release.yml`) and QA 377351757 (`.github/workflows/validate-final-package-qa.yml`), targets windows/x86_64, linux/x86_64, mac/x86_64, mac/aarch64. Stage flow: options → identity → inventory → versions → eligibility → archives → evidence → recheck.
- Eligibility projects real pending-deployment and release shapes and invokes the unchanged authoritative validator `validateFinalPackageSource` ([tools/final-package-qa-source.mjs](../../../tools/final-package-qa-source.mjs)) with `prePublication:true`, `unsigned:false`. Unreadable protected-state shapes and authentication failures are `collectionFailed`, never a negative predicate observation; empty reviewer lists are valid metadata.
- Release handling fix (task-29): an HTTP 200 response with a null body is now a failure; only a release-tag 404 may become null (existing draft-visibility limitation). No other failure is treated as absence evidence.
- Only the four selected per-target QA evidence ZIPs are downloaded (8 MiB each, 16 MiB aggregate), streamed, hashed, and fed through the existing composition helper `verifyFinalPackageQaEvidence`; no decoded caller evidence or proof-flag injection. Artifact expiry is checked after the final hash await: required artifacts must have `expired:false` and `expires_at` later than a timestamp taken after that await.
- The recheck stage re-reads current runs, attempt jobs, and full artifact inventories and requires byte-equal snapshots, then re-runs eligibility before the final composition; this is a point-in-time observation, not a lock or human approval.

## CLI (lazy route; no process/network/environment grants for help or invalid input)
- `deno run -A tools/fdh.ts` intercepts raw `qa` argv before the generic parser (which would collapse duplicate options) and lazily imports `fdh_qa_cli.ts`. Help and invalid arguments never import or invoke the collector.
- Usage: `fdh qa collect --source-run-id N --source-attempt N --source-head SHA --qa-run-id N --qa-attempt N --qa-head SHA --windows-policy require-success|shadow-observation --max-age-ms N` — all eight required.
- Exit enum: 0 matched, 1 not ready/collection failed, 2 invalid arguments. Duplicate or unknown options (any `--key=value` or space form) exit 2. No repository, host, URL, token, executable, clock, or transport override is accepted.
- Route-generated output is a single fixed-shape JSON summary on stdout, ≤ 256 KiB with a wire-byte guard; the handler does not forward raw stderr, API bodies, URLs or exception text. Runtime startup errors before the route initializes remain outside this handler contract. A matched result echoes only independently validated identities plus `archiveBytesVerified:true`.
- Matching is nonauthorizing; native QA, live approval, and publication remain unverified by design.

## Verification (original record plus root/Luna resumed integration)
- Transport dedicated suite: **94 tests, no grants**; independent task-26 **ACCEPT** plus 27 additional adversarial probes. Task-27 ran six Windows probes with real own Deno child processes (cancellation and pipe recovery); all settled.
- Collector dedicated suite **101** and CLI dedicated suite **164** tests, no grants. Combined accepted proof layers total **619 no-grant tests** (260 prior offline archive/composition layers + 94 + 101 + 164); independent task-29 **ACCEPT** with 41 collector and 1282 CLI independent assertions.
- Root reran the canonical FDH host suite on the resumed final source: **1365 passed / 0 failed / 3 ignored**. Current before-mach source build exited 0. OpenCode Go Luna separately reproduced 359 transport/collector/CLI tests and the actual sanitized completed-source CLI negative, with no source edits.
- Review-pinned SHA-256 (recomputed in this session against the worktree files and confirmed identical):
  - collector `6119F33417CC175CC4A7A4BBAD375F434A2947554C6459492F98317266C71C6E`
  - collector types `9A23099253CB6234FFCE19B809DAA563C4CCAAF4D3C3C3D850E35D3120CE8C32`
  - collector tests `226D23B675EFCE8A74B8780C2C807C8FBDDF6855129A76075AEAD718DEC30465`
- Session-computed reference digests for the transport/CLI files (not independently review-pinned): transport `D45A0359F003C2C7619020F9874B2F6A3884F7D814B2A6A6927F636EB237256C`, transport types `9C09D9B4012809FEB4BDEF331D02C0E75B1094E1BE627122C746FC82D9697422`, transport tests `4D88A98EB0E7708FFFFC762B983AA8891E477BFE4399CDBE5DB7F4A19463F09B`, CLI `29995A4A46EB36F3DD8C58500D42E7EC24511D8AB7BDDEEC9E2A5DC8AF1C0ACC`, CLI types `E42C6DE5F4B214BA31440F952D4E02F96EAE5F6819C9E0D38BE00F686F824AD6`, CLI tests `C8E5C26F133F78A53C78D00C2AD97CFC9FA2C2CA9EAA9481A7927FAA5F29752D`.
- Root restricted-permission CLI probes (actual process, `--allow-read` only): `qa collect --help` (exit 0) and an invalid duplicate `--source-run-id` (exit 2) both pass with no process, network, or environment grants.

## Verified live negative (not a pending positive)
With `--allow-read --allow-run=gh`, a fully valid invocation pinned to the actual historical runs — source run 43359316284 attempt 1 head `9e6b24e5fd561250ab04a011b70ee0b1280b1adc`, QA run 43363023598 attempt 1, same head, `--windows-policy require-success`, `--max-age-ms 86400000` — returned `matches:false`, stage `eligibility`, code `sourceNotReady`, all five authority flags false, exit 1. This is consistent with the task-23 audit: that source is completed and already public (v12.21.2 published, pending deployments empty), so prepublication intake is correctly rejected as not ready, and the QA run is historical compatibility evidence only.
The first attempt ran without `--allow-read`, failed at Deno `NotCapable` startup, and is explicitly **not counted** as proof of anything. No fabricated pending positive was constructed, and historical negatives are not relabeled.

## Evidence manifest status
Root generated [coordinator-collector-final.json](../../../_dist/fdh/evidence/coordinator-collector-final.json) after the resumed integration: schema and runner validation pass, state succeeded, 15 recorded assertions and 13 explicit limitations. Current source before-mach build exited 0. The asserted live negative is pinned as an operator-attested observation; its wrapper exit0 is test success while the actual child CLI exit1 means not ready. The first no-read-permission startup failure is excluded. This local manifest is not release qualification, does not hash the uncommitted diff and proves no native browser/macOS/live pending-positive/API-origin approval.

## Reproduction sketch
```bash
deno test --allow-read tools/src/fdh_qa_gh_transport.test.ts \
  tools/src/fdh_final_package_qa_collector.test.ts tools/src/fdh_qa_cli.test.ts
deno run -A tools/fdh.ts verify test --task-id coordinator-followup
deno run --allow-read tools/fdh.ts qa collect --help
```
A live repeat of the negative invocation additionally requires `--allow-run=gh` plus an installed, authenticated `gh` binary (host TCB); see [QA-COLLECTOR-PLAN.md](QA-COLLECTOR-PLAN.md) for the trust boundary and [QA-EVIDENCE-COMPOSITION.md](QA-EVIDENCE-COMPOSITION.md) for the offline evidence layer.
