---
name: fdh-dev-loop
description: Run the FDH Gate 3 local dev loop in an existing Floorp worktree with the fdh CLI (task start, prepare, edit, build verification, dev up, test/browser verification, EvidenceManifest v0, dev down). Use when an agent must verify a local change and produce schema-valid 2-D evidence without a conversation loop or shared services.
---

# FDH local dev loop (`fdh`)

`fdh` is a thin, non-interactive CLI. It does **not** run an agent loop, call an
LLM, push a branch, or open a PR. An already-running agent (DSH/Codex/Claude)
drives the commands below and does the editing itself. The local development
loop needs no shared services. Optional commands such as `ext poll`, `evidence
trace` and `qa collect` read GitHub through `gh` under their separate contracts.
The optional platform QA step (`job start --step platform`) reaches the FDH QA
targets over SSH.

## When to use

Use this skill when:

- you are in a Floorp git worktree and need the Gate 3 local loop: request →
  edit → build → test → browser confirmation → evidence;
- you need an `EvidenceManifest v0` that passes
  `tools/qa3/evidence/evidence-manifest.schema.json` and
  `validateManifest(value, { role: "runner" })`;
- you want the build/test/browser assertions recorded automatically instead of
  hand-written in a report.

Do **not** treat this local loop or its optional read-only evidence commands as
packaging, release qualification, PR creation or publication approval. Optional
shared-service operations below require their own explicit operator intent.

## Prerequisites

- Deno is available (the repo's `deno.json` tasks apply).
- The worktree is a real git work tree; run the commands from inside it (or pass
  `--worktree` to `task start` / `task resume`).
- `_dist/` is already git-ignored; fdh writes only under
  `<worktree>/_dist/fdh/`.
- Run from the worktree root so `dev`/`verify`/`evidence` can find the state
  through `git rev-parse --show-toplevel`.

## Exact command order

```sh
# 0. Enter the worktree (all later commands resolve the state from here).
cd /path/to/floorp-worktree

# 1. Create the task state. --port-offset is optional: <n|auto>, and an
#    invalid value (bare, negative, non-numeric) exits 1 before spawning.
deno run -A tools/fdh.ts task start \
  --task-id <task-id> \
  --worktree "$(pwd)" \
  --port-offset auto

# 2. Prepare the worktree in one idempotent command: deno install when fflate
#    is missing, copy the Debug runtime into _dist/bin (--runtime-source is a
#    registry tree such as /private/tmp/fdh-gate2/pr2b100/_dist/bin), relink
#    the devdir mounts to this worktree (relative symlinks on POSIX, absolute
#    symlinks or junctions on Windows), wipe the copied sessionstore and
#    node_modules/.vite, and record setup.guardEnv. Re-running it is a near
#    no-op. In a fresh worktree it runs `deno install` first and finishes in a
#    new process.
deno run -A tools/fdh.ts task prepare --task-id <task-id> \
  [--runtime-source <abs-registry-bin-dir>]

# 3. Edit the source in this worktree (agent work; fdh does not edit).

# 4. Re-open the state after editing/session gaps. This is local-only: it
#    verifies worktree, base commit, port offset and probes ports; it never
#    reruns push/PR or other non-idempotent external operations.
deno run -A tools/fdh.ts task resume --task-id <task-id>

# 5. Record the build assertion BEFORE starting the dev environment. The
#    default phase `all` compiles the source first (before-mach) and then
#    rewrites the Runtime XHTML (after-mach), so a broken TypeScript source
#    fails here. --phase after-mach is the legacy inject-only fast path;
#    --phase before-mach skips the Runtime-dependent inject (useful in a fresh
#    worktree before the Runtime is installed). all/before-mach are refused
#    while this worktree's dev server runs: a source build under a live dev
#    browser leaves Marionette unable to start new sessions until restart. To
#    rebuild after a later edit: `dev down`, `verify build`, `dev up`.
deno run -A tools/fdh.ts verify build --task-id <task-id> [--phase <all|before-mach|after-mach>]

# 6. Start the dev environment with the recorded offset (never re-resolves
#    "auto"). setup.guardEnv (FLOORP_SKIP_PATCHER=1) is passed to the child,
#    so no manual export is needed after task prepare. Edits made while it runs
#    reach the browser through Vite HMR without a rebuild.
deno run -A tools/fdh.ts dev up --task-id <task-id>

# 7. Record the host test assertion (deno task test:host).
deno run -A tools/fdh.ts verify test --task-id <task-id>

# 8. Confirm the browser. Status must report a connected browser. --eval is
#    optional and its output is recorded verbatim; add --expect <text> to turn
#    that output into an honest contains-check assertion (use it for the
#    change-confirmation text, e.g. the edited label).
deno run -A tools/fdh.ts verify browser --task-id <task-id> \
  --eval 'document.title' --expect 'Floorp'

# 8b. Cross-platform Agentic QA, when the change needs it (see "Cross-platform
#     Agentic QA" below for when, and for the payload and target files). One
#     durable job per platform; the two targets are separate machines, so
#     start both, then wait for each. Each exits 1 unless that run passed.
deno run -A tools/fdh.ts job start --task-id <task-id> --step platform \
  --platform macos --target <macos-target.json> --payload <Floorp.dmg> \
  [--eval '<chrome-context js with return>' --expect '<text>']
deno run -A tools/fdh.ts job start --task-id <task-id> --step platform \
  --platform linux --target <linux-target.json> \
  --payload <floorp-linux-x86_64.tar.xz> [--eval ... --expect ...]
deno run -A tools/fdh.ts job wait --task-id <task-id> --job-id <job-id>

# 9. Build + validate EvidenceManifest v0. --out must stay under
#    <worktree>/_dist/fdh/.
deno run -A tools/fdh.ts evidence --task-id <task-id> \
  --out _dist/fdh/evidence/<task-id>.json

# 10. Always tear down.
deno run -A tools/fdh.ts dev down --task-id <task-id>
```

`deno task fdh <command>` is equivalent to `deno run -A tools/fdh.ts <command>`.

## Prepared Runtime startup

`task prepare` installs or verifies only the existing BrowserGlue startup patch.
The setup result includes local patch/target byte hashes; the persisted task
setup is a summary, not the full bootstrap receipt. These hashes are not native
compiler, signature or authorization evidence. The unrelated full
patcher guard remains `FLOORP_SKIP_PATCHER=1`. Incompatible, partial or conflicting
hooks are refused; prepare does not change the supplied registry source.

Host contracts retain no run/env permission using filesystem-only Git DI. The
separate `tools/integration/prepared-runtime-bootstrap.test.ts` exercises real
Git with `--allow-read --allow-write --allow-run=git` on existing Native Runtime
CI runners. Browser qualification must observe automatic startup before any
manual server-module import; an import-triggered result is a narrower claim.

## OS API authentication (separate from browser verification)

The TypeScript OS API verifier accepts an explicit token-file path using
`--token-file` or `FLOORP_OS_TOKEN_FILE` (flag wins). It never discovers Home or
profile credentials and never accepts a raw-token argument. An authenticated target
must be a literal HTTP loopback root origin; redirects are refused and responses
are redacted before reporting. HTTP 401 in the wrapper proves reachability only,
not successful authentication. The Python client has token-file transport parity
and synthetic host coverage; confirm the checked-out wrapper flags before use.

Only use the verifier against an explicitly owned test browser: its main routine
creates fixture pages and mutates browser state. Do not run it merely to typecheck
code, or against the user's installed browser without authorization. For code-only
validation, use the synthetic host/integration tests below instead.

Dev/test HTTP-loader profiles opt into a separate token advertisement. On supported
POSIX hosts its fixed location is `ProfD/floorp-os-server/os-server-token`.
Windows dev-profile advertisement requires the ABI 1 native private-profile
bridge `@floorp.app/private-profile-token;1` in the selected Debug Runtime.
Runtimes without that capability deliberately refuse; there is no Node/Home
fallback. A prepared-runtime receipt is selection metadata, not native behavior
or default-publication proof. Initial refusal with no prior ownership
performs no dirsvc/IO and never falls back to Home. If this lifecycle already owns
a legacy advertisement or has a queued/in-flight write, refusal first awaits its
generation-bumping cleanup using the captured owned path; this withdrawal may do
IO and must settle before rejection. Live MCP token preference is not cleared.
Product-default opt-out profiles retain the legacy location. POSIX preexisting-link checks are not atomic no-follow guards;
same-user replacement races are not prevented; native packaged/default delivery remains separate.

Synthetic checks (no credential files or browser are used):

```sh
deno test --allow-read --allow-write tools/src/os_server_token.test.ts tools/src/os_test_client.test.ts
deno run --allow-net=127.0.0.1 tools/integration/os-test-client.ts
# Only AFTER building bridge/loader-modules:
deno test --allow-read tools/integration/os-server-token.test.ts
```

## Offline final-package evidence helpers (not qualification)

`validateFinalPackageQaConsistency` checks caller-observed metadata/attestations
against trusted expectations; it does not authenticate the API or archive bytes.
`verifyFinalPackageQaArchive` separately hashes supplied bytes, validates a narrow
bounded ZIP32 profile and verifies every entry. Its default signed-prepublication
policy requires both canonical input and post-QA JSON. Explicit completed-source
policy may return a null post-QA observation when the workflow did not create one;
no observation or timestamp is synthesized. Its successful integrity result still
refuses publication,
API-authenticity, native-QA and live-approval claims. Do not override the older
consistency result's false proof fields or wire either helper alone into publishing.
`verifyFinalPackageQaEvidence` now composes them offline: bounded immutable JSON
snapshots and exactly four native byte inputs are privately captured before awaiting;
only actually verified archive JSON can supply consistency evidence. Its safe
summaries still refuse transport/API/native-QA/live-approval/publication claims.

```sh
deno test --cached-only --frozen tools/src/fdh_final_package_qa_gate.test.ts tools/src/fdh_final_package_qa_archive.test.ts tools/src/fdh_final_package_qa_evidence.test.ts
# Explicit historical compatibility only, after separately obtaining verified samples:
deno run --cached-only --frozen --allow-read tools/integration/fdh-qa-archive-samples.ts
```

The historical integration never downloads data; missing samples fail rather than
skip. It is not a new browser or QA execution. Runtime zlib output-cap support,
strict archive limits, current-attempt API collection and protected approval remain
separate review boundaries; no helper result is release authorization.

## Read-only final-package QA collection (not publication)

`fdh qa collect` lazily invokes the bounded canonical GET-only collector. Its
eight options are mandatory; source/probe heads are independent, run IDs must
differ, and freshness is explicit (positive, at most 86400000 ms). Unknown or
duplicate options reject before collector import/I/O. There are no repository,
host, URL, header, token, executable or clock overrides.

The default is `--mode signed-prepublication`, preserving protected pending-source
conditions. Explicit `--mode published-release` collects completed canonical
package QA, not approval of an unfinished source. Unsigned collection is
unsupported. The completed workflow may omit its prepublication-only post-QA
entry; byte/CRC/path/size/digest verification still applies and the collector
performs its real final API readback. All authority flags remain false.

```sh
deno task fdh qa collect --source-run-id N --source-attempt N --source-head SOURCE_SHA \
  --qa-run-id N --qa-attempt N --qa-head PROBE_SHA \
  --windows-policy require-success --max-age-ms 86400000
# Least-privilege direct main invocation also needs --allow-read for existing FDH initialization:
# deno run --cached-only --frozen --allow-read --allow-run=gh tools/fdh.ts qa collect ...
```

Exit 0 means read-only prerequisites matched, **not** publication permission; exit
1 is not-ready/collection-failed and exit 2 is invalid arguments. Inspect the safe
JSON stage/code, not exit 1 alone: a startup permission error is not a verified
sourceNotReady result. All transport/API/native-QA/live-approval/publication flags
remain false. Host-installed gh/auth/config/TLS is the TCB; no origin attestation
or atomic collection lock is claimed. The live completed-source negative is
verified; a real pending positive and protected workflow activation remain open.

## Bounded read-only QA discovery (not a publisher gate)

`fdh qa discover` derives a QA run index window from the pinned source run's
creation time. Source and QA probe commits stay independent. Its five required
options do not include a caller timestamp, QA head, URL, repository or transport.

```sh
deno run -A tools/fdh.ts qa discover --source-run-id N --source-attempt N --source-head SOURCE_SHA \
  --windows-policy shadow-observation --max-age-ms 600000
```

The hard limits are three pages, 300 indexed runs and eight candidates. Every
candidate must pass the existing collector; an incomplete, pending, ambiguous,
unverifiable or changed window fails closed. A selected match is recollected and
the full window re-listed; this is still a point-in-time observation, not an
atomic GitHub transaction. Exit 0 and matches=true remain nonauthorizing.
Production publisher wiring and real signed pending-positive proof are separate.

Also run changed test groups directly with type checking enabled. A broad host
graph can pass while a focused graph exposes Node/DOM WebCrypto type differences;
fixture keys should use their cryptographic library returned types, not casts.

## Recording commands that ran outside fdh (IMP-2)

`fdh verify build|test` only records what fdh itself spawned. When a build or
test was run outside the CLI (for example a raw `--phase before-mach` run, or a
failed host test in another shell), record it with the observation file so the
evidence manifest sees the failure. The exit code and operator are
operator-attested; fdh pins the observation bytes by SHA-256 and re-hashes them
when building evidence. `--attested-at` must be an ISO 8601 date-time with a
zone and must not be in the future (5 minutes of clock skew are tolerated).

```sh
# Move the raw log under _dist/fdh/observations/ first (the file must stay
# there; 256 KiB limit).
deno run -A tools/fdh.ts record --task-id <task-id> \
  --command "deno task feles-build build --phase before-mach" \
  --exit-code 1 \
  --observation-file _dist/fdh/observations/build-broken.log \
  --operator <agent-name> \
  --cwd "$(pwd)" \
  --verification build
```

With `--verification`, the CLI records `external-<kind>-exit-code` (passed only
when exit is 0), so a failed external build/test makes the manifest state
`failed`. Without it, the operation is recorded but the state stays
assertion-driven. Evidence limitations disclose that fdh did not observe the
execution.

## Review receipts for the D-14 gate

An independent LLM review is recorded as a machine-readable receipt bound to the
PR head commit. `fdh review record` is still local-only: it writes the receipt
under `<worktree>/_dist/fdh/reviews/` and prints the comment body. The agent
posts that body with `gh pr comment`, and the `fdh-trusted-review-gate` workflow
requires at least two distinct reviewer identities at the PR head.

```sh
deno run -A tools/fdh.ts review record --task-id <task-id> \
  --reviewer <identity> --model <model> \
  --commit <pr-head-sha> \
  --verdict approve_with_notes \
  --scope "changed files + evidence" \
  --tier full_read \
  --blocking false
```

Then post the printed `commentBody` (marker
`<!-- fdh-review-receipt:v1 {...} -->`) to the PR. The gate counts receipts from
authors whose GitHub association is OWNER, MEMBER or COLLABORATOR and whose
effective repository permission is write or admin (GitHub maps maintain to
write). A receipt for another commit is stale. A head receipt with a
`request_changes` verdict or `blocking=true` fails the gate.

## Trusted receipt-check boundary

GitHub loads the `pull_request_target` workflow from the PR's base on `main`.
The `trusted-receipt-publication` job checks out that same workflow commit with
`github.workflow_sha` and evaluates receipt comments against the event's PR
head. It verifies the PR still targets `main` at that head before and after
validation. The custom `fdh-trusted-receipts` Check Run carries the diagnostic
result. The separate `pull_request` job `review-gate` runs the candidate host
tests and build with read-only contents permission.

The default-branch `issue_comment` job reruns trusted validation for the current
PR head when an eligible author's receipt marker is added, edited, or removed.
For a PR already open when the rerun workflow ships, reopen it or update it to
start a run the comment job can find. Use the same recovery when the original
run is outside GitHub's 30-day rerun window. Revalidation reads the full comment
set. Reviewer identities remain declarations by the poster.

Before moving all receipt enforcement to this workflow, the repo admin must
protect the trusted job from impersonation. The binding to GitHub Actions App
15368 identifies the App and job name. Require the trusted workflow by identity
from a protected source in an organization ruleset where supported.
Alternatively, enforce maintainer code-owner approvals with stale approvals
dismissed for all workflow files, local actions, the trusted receipt checker and
its imports, and CODEOWNERS itself. Keep the candidate test/build check required
through rollout.

## Evidence on the PR (D-20, KAN-132)

A manifest lives only in this worktree's `_dist/fdh/`. To keep it with the
change, post it on the PR as a comment.

Post it after the last commit, from a clean worktree, so that it is bound to
the PR's final head.

```sh
deno task fdh evidence publish --task-id <id> --pr <n> --out _dist/fdh/evidence-<n>.md
# Read _dist/fdh/evidence-<n>.md.manifest.json (the same manifest, in plain
# JSON) before posting. Do not post it if it holds anything sensitive.
# Neither file may exist yet: a new head needs a new --out name.
gh pr comment <n> --body-file _dist/fdh/evidence-<n>.md
deno task fdh evidence trace --pr <n> [--require-merged true]   # exit 1 if incomplete
```

The trace evaluates receipts using the poster's current repository permission
and the same eligibility policy as the trusted gate. If GitHub cannot verify
permissions, the trace reports an incomplete receipt set.

The comment is public. The manifest carries command output and local paths,
and fdh's secret check only refuses known secret references; it does not find
arbitrary credentials or personal data. That is why the plain copy has to be
read first.

What the comment holds:

- The whole manifest, as gzip+base64. It is never embedded as JSON, because
  `gh api` rewrites JSON-escaped control characters, such as the ANSI colours in
  command output, when it prints a comment. A manifest that does not fit in one
  comment is refused.
- A marker that binds it to the repository, the task, the PR, the HEAD commit,
  and whether the worktree had uncommitted changes. Uncommitted changes are not
  hashed; the manifest's limitations say so.
- The SHA-256 and the size of the manifest text.

Evidence counts for `evidence trace` only if all of these hold:

- **Author.** It is by a trusted author: OWNER, MEMBER or COLLABORATOR, as for
  receipts.
- **Not edited.** It has not been edited since it was posted.
- **Intact.** It decodes and matches its size and SHA-256, the manifest schema
  accepts it, and the manifest is for the bound task.
- **Binding.** It is bound to this repository (names compared
  case-insensitively), this PR and the PR's final head.
- **Clean build.** It was built without uncommitted changes.
- **Verdict.** The manifest records at least one assertion, and its own state
  is `succeeded`. The state takes the latest assertion of each name, so a
  failure fixed later still succeeds; a failed or cancelled task never does.

The trace is complete when such evidence exists and the D-14 receipts are valid
at that head. With `--require-merged true`, the PR must also be merged.
Comments that do not count are listed as notes; they never make a trace
incomplete.

How far this can be trusted:

- The binding is what the trusted author's comment says.
- The SHA-256 is not a signature. It catches a manifest that no longer matches
  its marker, not an author who rewrites both. That is why an edited comment
  does not count.
- A PR comment stays with the PR. CI artifacts expire, so they are not the
  record.
- A central append-only store, with authenticated principals, waits for D-18.

## Quota and approval ledger (Gate 4 4-D / 4-C)

Use the local ledger (`<worktree>/_dist/fdh/quota/<task-id>.json`) when work can
stop for quota, infrastructure, or manual reasons, and when an unknown
non-idempotent external operation must wait for approval.

```sh
# Assign the task to one agent. A second active holder is rejected, so the same
# work is never assigned twice while an agent holds it.
deno run -A tools/fdh.ts quota claim --task-id <task-id> --agent <name>

# Stop for a reason; only the active holder may stop.
deno run -A tools/fdh.ts quota stop --task-id <task-id> --agent <name> \
  --reason quota --note "LLM quota exhausted"

# Resume the stopped task and re-assign it.
deno run -A tools/fdh.ts quota resume --task-id <task-id> --agent <name>

# Record an unknown non-idempotent external op awaiting approval.
deno run -A tools/fdh.ts quota pending --task-id <task-id> --op push \
  --note "remote state unknown; do not retry until approved"

# Approve and clear pending external ops before retrying them.
deno run -A tools/fdh.ts quota approve --task-id <task-id> --note "human approved"

# Release the holder without stopping; print the ledger.
deno run -A tools/fdh.ts quota release --task-id <task-id> --agent <name>
deno run -A tools/fdh.ts quota status --task-id <task-id>
```

`task resume` reports `approvalPending` from this ledger and still never reruns
push/PR/release; the agent must wait for `quota approve`. Stop reasons are
`quota|infra|manual`; pending ops are `push|pr|release|external`.

## Durable jobs and external intents (B-02, D-23)

Use `fdh job` instead of `verify build|test` when the step is long and must
survive this agent session, its controller, or the executor being killed. The
contract is the frozen B-02 ADR (Confluence 8716289): run/attempt/epoch and an
execution digest are recorded before anything runs, a detached executor holds a
heartbeat lease, an older epoch's writes are rejected, and the result is written
to an outbox and applied under one journal lock.

```sh
# Start a durable step (same commands as verify build|test); returns at once.
deno run -A tools/fdh.ts job start --task-id <task-id> --step test
deno run -A tools/fdh.ts job start --task-id <task-id> --step build [--phase all]

# Wait (reconciles every second). Exits 1 unless the job succeeded. Safe to
# rerun from a new session: it reconnects to a live executor.
deno run -A tools/fdh.ts job wait --task-id <task-id> --job-id <job-id>

# Inspect or reconcile without waiting.
deno run -A tools/fdh.ts job status --task-id <task-id> [--job-id <job-id>]
deno run -A tools/fdh.ts job cancel --task-id <task-id> --job-id <job-id>
```

Reconcile (`job wait|status|reconcile`, `task resume`) does exactly one of:
reconnect to a live executor; apply a result left in the outbox (no rerun); stop
the lost attempt's step process tree (identity-checked; on Linux/macOS also the
members of its process group, so children that outlived the root are found)
and start the next epoch from the checkpoint (build commands already completed
are not rerun); fail the
job when its inputs (HEAD, tracked diff, untracked file contents, guard env)
changed or `--max-attempts` (default 3) attempts were lost. Each terminal result
is copied into the task state once (operations, assertions, a `job result <id>`
event). A build that still has to compile source is held, not respawned, while
this worktree's dev server runs (the W-04 guard; reconcile reports `held`, and
no attempt is used up): run `dev down`, then `job wait`.

Non-idempotent operations (push, PR, release, other external writes) are never
run by fdh. Record the intent before you perform it, and the outcome after:

```sh
deno run -A tools/fdh.ts job intent --task-id <task-id> --op push \
  --target "origin <branch>" --agent <name>          # prints jobId, epoch 1
# ... perform the operation yourself ...
deno run -A tools/fdh.ts job confirm --task-id <task-id> --job-id <job-id> \
  --epoch 1 --agent <name> --outcome succeeded
```

If the session ends between the two, `task resume` puts the intent into
`awaiting-approval` and the quota ledger's pending list. Do not retry it: a
human checks the external state and runs
`job resolve --resolution succeeded|failed|retry [--agent <name>]`, which also
approves the pending entry. If the ledger update fails, run the same
`job resolve` again: it only repeats the ledger step. Reconcile puts a missing
pending entry back while the job still awaits approval, unless the ledger
records an approval for it (or a `quota approve`) since then. A confirm that
arrives after the holder was declared lost is refused and recorded
(`late-report`).

## Cross-platform Agentic QA (KAN-142, KAN-145)

`job start --step platform` runs a **packaged** Floorp build of the change on a
QA target and records assertion `platform-qa-<platform>`:

- `macos`: the stability tart VM on the Mac Studio (GUI session, real macOS);
  payload `.dmg` or `.tar.gz`.
- `linux`: a fresh Docker container on the Debian host (Xvfb + openbox, no
  network by default); payload `.tar.xz`, `.tar.bz2` or `.tar.gz`.

The job runs one command, `fdh platform-qa run`: preflight, lock, upload,
install, launch with a fresh profile, a fixed probe (app, OS, browser window,
content-sandbox level), the optional `--eval` (chrome context; the script must
`return` its value) with `--expect`, a screenshot, and cleanup, which always
runs once the target was taken. Results go to
`_dist/fdh/platform/<task-id>/<platform>-report.json` and `<platform>.png`;
the job log ends with a one-line summary. Host addresses are redacted from both.

Outcomes, in the report and the summary line:

- `passed`: every step ran, the probe and `--expect` matched, cleanup was clean.
- `failed`: the build or the run on the target went wrong (`reason` names the
  step), or cleanup left something behind.
- `blocked`: the target was unreachable or not ready (held by another task, low
  disk, no GUI session, a foreign Floorp running). The change was not tested;
  say so, and count it neither as a pass nor as a product failure.

Both `failed` and `blocked` make the job (and the assertion) fail; read the
report to tell them apart.

When to run it:

- Run it when the change touches browser chrome UI, pages, startup/bridge, ESM
  modules, prefs, or anything that differs per platform (paths, file pickers,
  native integration, fonts, shortcuts), and a packaged build of the change
  exists for that platform. Run it whenever the user asks.
- Skip it for `tools/`, `docs/` and CI-only changes, and state in the final
  report that it was skipped and why.
- The payload is the agent's input: a packaged build of this change (for
  example a PR's CI package, or a local package build). fdh records its SHA-256
  and version, but cannot tell which commit it was built from.

Target configs are JSON files outside the repository (they name the hosts and
key files); never commit them. Their hashes and the payload hash are fixed when
the job starts: if either file changes, a rerun fails instead of testing
something else.

A lost executor is handled by the normal reconcile: the next attempt reruns the
whole command, which first removes what this task left on the target. If the
job gave up or was cancelled, free the target with:

```sh
deno run -A tools/fdh.ts platform-qa cleanup --platform <macos|linux> \
  --task-id <task-id> --target <target.json>
```

Boundary with QA3: this step is an early check during development, on one
machine per platform. It is not QA3, release qualification, final-package QA or
a visual judgment; the screenshot is kept for a human to look at. Windows is not
covered yet (KAN-147). The Linux container has no desktop session, no GPU and no
user-namespace sandbox; see `docs/qa/fdh-qa-linux/` and `docs/qa/fdh-qa-macos/`
for each target's limits.

The QA targets are FDH development machines reached over SSH, not self-hosted
GitHub Actions runners. D-17 (GUI self-hosted runners only when the user says
so) still applies to runners; within the dev loop an agent may start this step
under the rules above without asking each time.

## External GitHub / Jira operations (D-19, KAN-131)

Agents change GitHub (`gh`) and Jira (Atlassian MCP) themselves; fdh calls
neither API. fdh keeps a link table, the observations an agent polled, and
keyed operations, under `<worktree>/_dist/fdh/ext/<task-id>.json`. There is
no webhook: run `ext observe` for each value you poll, then `ext plan`.

```sh
deno task fdh ext link --task-id <id> --jira KAN-131 --pr 3200 --branch fdh/external-ops
# GitHub: read every linked PR (state, URL, the head's check runs) with gh.
# A PR that cannot be read is an error (exit 1), never a deletion.
deno task fdh ext poll --task-id <id> [--repo <owner/name>]
# Jira (Atlassian MCP), or any value you read yourself, with the object's
# API update time as --version:
deno task fdh ext observe --task-id <id> --ref jira:KAN-131 --field status \
  --value 進行中 --version <jira updated>
deno task fdh ext plan --task-id <id>      # actions, holds, inSync, keptEdits, fields
# For each action: start its key, perform the write, then record the outcome.
deno task fdh ext start --task-id <id> --key '<action.key>' --agent <name>
deno task fdh ext done --task-id <id> --key '<action.key>' --epoch <n> --agent <name> \
  --outcome succeeded
# Then poll the mirror again (ext observe) before the next plan.
```

The field table (I-04; the code is `FIELD_RULES` in
`tools/src/fdh_ext.ts`):

| Field (owner) | Mirror | Rule |
| --- | --- | --- |
| `pr.url` (GitHub) | Jira `pr-link-comment` | one fdh comment with the PR's URL, once per URL (Atlassian MCP cannot create remote links) |
| `pr.state` (GitHub) | Jira `status` | draft/open/merged → 進行中/レビュー中/完了, closed → nothing; never moves the status back |
| `ci.result` (GitHub), `<sha>:<success\|failure>` | Jira `ci-comment` | one fdh comment per head and result; pending is not posted |
| `pr.review-receipts` (GitHub) | none | the D-14 gate reads them on the PR |
| `jira.sprint`, `jira.assignee`, `jira.priority`, `jira.qa-state` (Jira) | none | not copied to GitHub |
| titles, descriptions, comments | none | no full-text copying |

Which version to pass:

- **PR fields:** the PR's `updated_at`.
- **`ci.result`:** the check run's `completed_at` (`started_at` while pending). A PR's `updated_at` does not move when checks finish.
- **Jira fields:** the issue's `updated`.
- **A deletion** (a 404): the poll time.

How observations behave:

- **Duplicates.** A repeated event id for the same field, or the same value at the same version, is a `duplicate`. It refreshes the poll time.
- **Stale values.** An older version than the stored one is `stale`; this is how an ordering inversion is caught.
- **Conflicts.** The same version with another value is a `conflict` and is not taken.
- **Deletions.** Observe the field `object` with `--deleted true` when an object is gone. Nothing more is written to it or because of it, and a later observation does not bring it back. A deletion older than the newest observation of the object is `stale`.

How the plan treats writes:

- **Concurrent edits.** fdh never overwrites a mirror that changed since fdh last wrote it (no compare-and-set is assumed).
  - If the owner changed too, the field is held (`concurrent-edit`). An approver settles it with `fdh ext resolve --link <KEY-n> --field <f> --keep <mirror|owner>`.
  - If only the mirror changed, the person's edit is kept (`keptEdits`).
- **After a write.** The field waits (`awaiting-poll`) for a poll made after `ext done`.
- **Before the first write.** A status is not written before Jira's current status has been polled (`mirror-unobserved`).

Keyed operations:

- **Key forms.** An action's key is `<KEY-n>:<field>:<value>#<generation>`; the generation moves each time a baseline is recorded or resolved. A comment's key is `<KEY-n>:ci.result:<sha>:<result>`. `ext start` refuses a field key that is not an action of the current plan; run `ext plan` again and use the key it gives.
- **An unreadable job journal** keeps its operation in flight, so its field stays held. Repair the journal under `_dist/fdh/jobs/<task-id>/`; if you know the operation never ran, remove the journal and the operation ends as failed.
- **What `ext start` records.** It stores what a planned action writes. When the operation succeeds, that becomes the field's baseline. This holds both after `ext done` and after an approver's `job resolve`, which `ext plan` picks up.
- **A key that succeeded** is `already-done`. Use one key per PR you create (`github:pr-create:<branch>`), so no second PR is made.
- **A key with no outcome yet** is refused.
- **A key that failed** may be started again. Report `failed` only when you know the write did not happen. If you do not know, do not confirm: once your lease runs out, `task resume` moves the operation to `awaiting-approval`. An approver then checks GitHub or Jira and resolves it.

## GUI desktop lease and secret-reference denial (Gate 4 4-A)

The shared desktop GUI is one resource. The lease is stored beside the git
common directory (`<git-common-dir>/fdh/gui-desktop-lease.json`), so all
worktrees of the same repository see the same holder, and it is never in the
tracked tree.

```sh
# Acquire the desktop lease. A second holder is rejected.
deno run -A tools/fdh.ts gui claim --task-id <task-id> --agent <name>

# End (normal completion) or cancel; only the current holder may release it.
deno run -A tools/fdh.ts gui end --task-id <task-id> --agent <name> \
  --reason ended --note "browser confirmation complete"

# Inspect the shared lease.
deno run -A tools/fdh.ts gui status --task-id <task-id>
```

Evidence records must not reference repository secrets. `fdh evidence` refuses a
task state that mentions `.secrets` or its token names, and the same check is
available directly. fdh scans only the task records; it never reads `.secrets`.

```sh
deno run -A tools/fdh.ts secrets check --task-id <task-id>
deno run -A tools/fdh.ts secrets status
```

## Contract

- Commands: `task start|resume|prepare`, `dev up|down`,
  `verify build|test|browser`, `evidence`, `record`, `review record`,
  `quota claim|stop|resume|pending|approve|release|status`,
  `job start|wait|status|reconcile|intent|confirm|resolve|cancel` (`job exec` is
  the detached executor and is not called directly), `gui claim|end|status`,
  `secrets check|status`, `help`.
- State path: `<worktree>/_dist/fdh/tasks/<task-id>.json`. It holds `taskId`,
  `worktree`, `baseCommit`, `portOffset`, `createdAt`, `status`, `events[]`,
  `operations[]` (optional `external` attestation block), `assertions[]`,
  `cleanup`, `reproduction` and optional `setup` (prepare facts).
- `task start` and `task resume` print compact JSON. `evidence` prints a JSON
  summary. `dev`/`verify` stream child output and exit 1 when the child fails.
- The CLI records every build/test/browser command as an operation, an event and
  (for build/test/status) an assertion. Failed commands exit 1; assertions keep
  the actual output/exit code.
- `verify build` defaults to `--phase all`: before-mach (`build-exit-code`) then
  after-mach (`build-inject-exit-code`), stopping before the inject if the
  source compile fails. `--phase after-mach` records only the injector
  assertion, and the evidence manifest then adds a limitation that source
  compilation was not verified. `--phase before-mach` records only the source
  assertion.
- `verify build --phase all|before-mach` exits 1 without spawning anything when
  this worktree's recorded dev processes (`_dist/dev-processes.json`, matched by
  PID + start time + command line) are still running, and records a
  `verify build ... (refused)` event. When liveness cannot be determined (an
  unreadable or corrupt registry, a failed process listing) it also exits 1 and
  records `verify build ... (dev guard failed)`. `--phase after-mach` is always
  allowed. Either event newer than the last source build fdh ran itself adds the
  evidence limitation that the latest source edits were not compiled; an
  external `record` does not clear it. A corrupt registry is cleared by
  `fdh dev down`.
- `verify browser` succeeds only when `dev-tool status` prints
  `Connected to browser.` Without that line, do not claim browser verification.
- `--eval` output is always recorded verbatim. Without `--expect` it is never a
  pass; with `--expect <text>` the CLI records a `browser-eval-contains`
  assertion from the actual output and exits 1 when the text is absent.
- Evidence IDs are stable per task and use the required issuer namespaces
  (`scheduler:`, `executor:`, `host:`, `host-authorization:`). All seven
  entities and the five core relationships are always present.
- `taskStatus.state` maps the recorded outcome (`running` / `succeeded` /
  `failed`) from the LATEST assertion per name, and the listed
  `stateTransition.to` always equals it. A deliberately broken build/test fixed
  later therefore ends as `succeeded` while the failed intermediate assertion
  stays visible in `assertions[]`.
- `visualJudge` is always `null` and `limitations[]` is always non-empty. The
  DSH toolchain pin is `UNVERIFIED`; the matching limitation is included.
- The CLI is thin: no conversation loop, no LLM calls, no image/screenshot
  support, no GitHub calls.

## Evidence acceptance (2-D schema)

Evidence is accepted only when it passes **both**:

1. `tools/qa3/evidence/evidence-manifest.schema.json` (schema draft-07), and
2. `validateManifest(value, { role: "runner" })` from
   `tools/qa3/evidence/validate.ts`.

`fdh evidence` validates before writing and exits 1 with
`evidence-invalid
<code> <path>: <message>` lines when the manifest would be
rejected. Do not hand-edit the manifest to make it pass; fix the recorded state
or the command.

## Browser unavailable fallback

If the browser is unavailable (no Marionette connection, `dev up` fails,
headless limitation, etc.):

1. Leave the failed `browser-connected` assertion in the state; do not delete
   it.
2. Do not claim a pass. Run `fdh evidence` anyway: the manifest records the
   failure and the limitations state that browser behavior is UNVERIFIED.
3. Report the limitation explicitly in your final answer, including the exact
   command output that showed the browser was unavailable.

The same rule applies to build/test: never convert missing or failed output into
a pass by editing state.

## Limitations

- This local loop does not prove Gate 3 qualification or release readiness; it
  only records what the local commands did.
- `visualJudge` is `null`: no visual/look-and-feel judgment is performed.
- Images/screenshots are outside the CLI contract, except the platform QA
  screenshot, which is stored for human review and never judged; use existing
  tools directly if a human review needs more.
- Shared services (GitHub, PRs, CI dispatch) are not required and are never
  called by the local loop. Platform QA uses SSH to the FDH QA targets only.
- `fdh job` is one-machine fault tolerance only (no central scheduler, no
  cross-machine lease). It does not claim exactly-once: a safe step can run
  again after its executor is lost, and an old executor's local writes are not
  undone. Intents and confirms are self-reported by the agent.
- Cleanup is reported honestly: `dev up` leaves cleanup `partial` until
  `dev down` succeeds.
- `reproduction` is `inconclusive` unless a defect was actually reproduced; the
  CLI does not infer reproduction from command output.
