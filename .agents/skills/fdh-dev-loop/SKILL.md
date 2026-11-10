---
name: fdh-dev-loop
description: Run the FDH Gate 3 local dev loop in an existing Floorp worktree with the fdh CLI (task start, prepare, edit, build verification, dev up, test/browser verification, EvidenceManifest v0, dev down). Use when an agent must verify a local change and produce schema-valid 2-D evidence without a conversation loop or shared services.
---

# FDH local dev loop (`fdh`)

`fdh` is a thin, non-interactive CLI. It does **not** run an agent loop, call an
LLM, push a branch, open a PR, or contact GitHub. An already-running agent
(DSH/Codex/Claude) drives the commands below and does the editing itself.

## When to use

Use this skill when:

- you are in a Floorp git worktree and need the Gate 3 local loop: request →
  edit → build → test → browser confirmation → evidence;
- you need an `EvidenceManifest v0` that passes
  `tools/qa3/evidence/evidence-manifest.schema.json` and
  `validateManifest(value, { role: "runner" })`;
- you want the build/test/browser assertions recorded automatically instead of
  hand-written in a report.

Do **not** use it for packaging, release qualification, PR creation, or any
operation that needs GitHub or another shared service.

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

# 9. Build + validate EvidenceManifest v0. --out must stay under
#    <worktree>/_dist/fdh/.
deno run -A tools/fdh.ts evidence --task-id <task-id> \
  --out _dist/fdh/evidence/<task-id>.json

# 10. Always tear down.
deno run -A tools/fdh.ts dev down --task-id <task-id>
```

`deno task fdh <command>` is equivalent to `deno run -A tools/fdh.ts <command>`.

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
posts that body with `gh pr comment`, and the `fdh-review-gate` workflow
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
`<!-- fdh-review-receipt:v1 {...} -->`) to the PR. The gate counts receipts only
from comment authors whose GitHub association is OWNER, MEMBER or COLLABORATOR;
other authors' receipts are logged and ignored. A receipt for another commit is
stale and ignored (it neither counts nor blocks). A head receipt with a
`request_changes` verdict or `blocking=true` fails the gate.

Posting a trusted receipt does not run the gate itself: the `rerun-on-receipt`
job waits for the PR head's latest `review-gate` run and reruns it when it did
not pass, so the required check updates on the PR head. The reviewer identities
are still declared by the poster; the gate cannot tell whether two receipts from
one trusted account came from two independent reviews.

Limits to know:

- `MEMBER` means any member of the organisation, not write access to this
  repository. An org member with private membership may appear as `CONTRIBUTOR`;
  their receipts are then ignored (make the membership public or use a
  collaborator account).
- Only newly created comments trigger the rerun; after editing a receipt, rerun
  the PR's `review-gate` run by hand.
- A PR that changes `fdh-review-gate.yml` still needs one manual rerun after its
  receipts are posted, because `issue_comment` workflows are read from the
  default branch.
- The gate code runs from the PR's own checkout, so a PR can change what the
  gate checks; review edits to `.github/workflows/fdh-review-gate.yml` and
  `tools/src/fdh_review.ts` before merging.

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
the lost attempt's step process tree (identity-checked) and start the next epoch
from the checkpoint (build commands already completed are not rerun); fail the
job when its inputs (HEAD, tracked diff, untracked file contents, guard env)
changed or `--max-attempts` (default 3) attempts were lost. Each terminal result
is copied into the task state once (operations, assertions, a `job result <id>`
event). A build that still has to compile source is not respawned while this
worktree's dev server runs (the W-04 guard): run `dev down`, then `job wait`.

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
- Images/screenshots are outside the CLI contract; use existing tools directly
  if a human review needs them.
- Shared services (GitHub, PRs, CI dispatch) are not required and are never
  called by the CLI.
- `fdh job` is one-machine fault tolerance only (no central scheduler, no
  cross-machine lease). It does not claim exactly-once: a safe step can run
  again after its executor is lost, and an old executor's local writes are not
  undone. Intents and confirms are self-reported by the agent.
- Cleanup is reported honestly: `dev up` leaves cleanup `partial` until
  `dev down` succeeds.
- `reproduction` is `inconclusive` unless a defect was actually reproduced; the
  CLI does not infer reproduction from command output.
