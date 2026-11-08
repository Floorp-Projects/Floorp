---
name: fdh-dev-loop
description: Run the FDH Gate 3 local dev loop in an existing Floorp worktree with the fdh CLI (task start, edit, dev up, build/test/browser verification, EvidenceManifest v0, dev down). Use when an agent must verify a local change and produce schema-valid 2-D evidence without a conversation loop or shared services.
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

# 2. Edit the source in this worktree (agent work; fdh does not edit).

# 3. Re-open the state after editing/session gaps. This is local-only: it
#    verifies worktree, base commit, port offset and probes ports; it never
#    reruns push/PR or other non-idempotent external operations.
deno run -A tools/fdh.ts task resume --task-id <task-id>

# 4. Start the dev environment with the recorded offset (never re-resolves
#    "auto").
deno run -A tools/fdh.ts dev up --task-id <task-id>

# 5. Record the build assertion (deno task feles-build build --phase after-mach).
deno run -A tools/fdh.ts verify build --task-id <task-id>

# 6. Record the host test assertion (deno task test:host).
deno run -A tools/fdh.ts verify test --task-id <task-id>

# 7. Confirm the browser. Status must report a connected browser. --eval is
#    optional and its output is recorded verbatim; add --expect <text> to turn
#    that output into an honest contains-check assertion (use it for the
#    change-confirmation text, e.g. the edited label).
deno run -A tools/fdh.ts verify browser --task-id <task-id> \
  --eval 'document.title' --expect 'Floorp'

# 8. Build + validate EvidenceManifest v0. --out must stay under
#    <worktree>/_dist/fdh/.
deno run -A tools/fdh.ts evidence --task-id <task-id> \
  --out _dist/fdh/evidence/<task-id>.json

# 9. Always tear down.
deno run -A tools/fdh.ts dev down --task-id <task-id>
```

`deno task fdh <command>` is equivalent to `deno run -A tools/fdh.ts <command>`.

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

## Contract

- Commands: `task start|resume`, `dev up|down`, `verify build|test|browser`,
  `evidence`, `quota claim|stop|resume|pending|approve|release|status`, `help`.
- State path: `<worktree>/_dist/fdh/tasks/<task-id>.json`. It holds `taskId`,
  `worktree`, `baseCommit`, `portOffset`, `createdAt`, `status`, `events[]`,
  `operations[]`, `assertions[]`, `cleanup` and `reproduction`.
- `task start` and `task resume` print compact JSON. `evidence` prints a JSON
  summary. `dev`/`verify` stream child output and exit 1 when the child fails.
- The CLI records every build/test/browser command as an operation, an event and
  (for build/test/status) an assertion. Failed commands exit 1; assertions keep
  the actual output/exit code.
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
- Cleanup is reported honestly: `dev up` leaves cleanup `partial` until
  `dev down` succeeds.
- `reproduction` is `inconclusive` unless a defect was actually reproduced; the
  CLI does not infer reproduction from command output.
