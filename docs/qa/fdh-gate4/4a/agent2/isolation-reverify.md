# Gate 4 task 4-A — isolation re-verify, Agent 2

Task: `gate4-a100` · Worktree: `/tmp/fdh-gate2/pr2b100` · Offset: 100
Own ports: settings 5283, marionette 5280 · Marker: `QA4-A-AGENT2`
Agent 1 (`/tmp/fdh-gate2/pr2b2`, offset 0, ports 5183/2828) was stopped by the
previous step before this re-verify.

## 1. Survival proof (A's stop did not kill Agent 2's processes)

| Port | Before A-stop (parallel stage) | After A-stop (this re-verify) | PID |
|---|---|---|---|
| 5283 settings | LISTEN (deno) | LISTEN (deno) | 20781 |
| 5280 marionette | LISTEN (floorp) | LISTEN (floorp) | 21959 |

Same PIDs before and after Agent 1's stop, so Agent 1's `dev down` left Agent 2's
dev server and browser untouched. Agent 1's ports are confirmed closed:

- `lsof -nP -iTCP:5283 -sTCP:LISTEN` → 1 listener line (`lsof-settings-after-a-stop.txt`)
- `lsof -nP -iTCP:5280 -sTCP:LISTEN` → 1 listener line (`lsof-marionette-after-a-stop.txt`)
- `lsof -nP -iTCP:5183 -iTCP:2828 -sTCP:LISTEN` → empty (`lsof-a-closed.txt`, 0 lines)

## 2. Browser re-verify after A's stop

```
deno run -A tools/fdh.ts verify browser --task-id gate4-a100 \
  --eval "$(cat "$OUT/eval.js")" --expect "QA4-A-AGENT2"
```
Exit 0. New Marionette sessions opened (4b9bfad3…, 64219f8b…); no NewSession timeout.
Result: `QA4-A-AGENT2 @ http://localhost:5283/#/overview/home`.
Marker still present in the live settings page served by Agent 2's dev server on its
own port 5283. Log: `browser-reverify-after-a-stop.log`.

## 3. Evidence after A's stop

```
deno run -A tools/fdh.ts evidence --task-id gate4-a100 \
  --out _dist/fdh/evidence/gate4-a100.json
```
Exit 0, state `succeeded`, 4 assertions, 13 limitations, 10644 bytes.
Copied to `evidence-after-a-stop.json`.
Validation with `validateManifest(manifest, { role: "runner" })`:
`errorCount: 0`, `errors: []` (log `validate-after-a-stop.log`).

## 4. Own dev stopped

```
FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev down --task-id gate4-a100
```
Exit 0: "Stopped (33 processes)." Log: `dev-down.log`.

Ports after stop:

- 5283 → empty (`lsof-settings-closed.txt`, 0 lines)
- 5280 → empty (`lsof-marionette-closed.txt`, 0 lines)
- 5183/2828 (Agent 1) → still empty (0 lines)

## 5. Post-down evidence

Regenerated `gate4-a100.json` → state `succeeded`, 4 assertions, 12 limitations,
10772 bytes; copied to `evidence-post-down.json`. Runner-role validation:
`errorCount: 0`, `errors: []` (log `validate-post-down.log`).

## 6. Git state — marker still uncommitted

`git status --short` (`post-status.txt`):

```
 M browser-features/pages-settings/src/components/app-sidebar.tsx
 M tools/feles-build.ts
```

The marker hunk remains an uncommitted working-tree modification:

```
+      title: t("sidebar.firefoxSettings") + " QA4-A-AGENT2",
```

HEAD is still the pre-task merge commit `bec270403822`; the marker is not in any
commit.

## Cleanup summary

- Agent 2 dev server (5283) and browser (5280): stopped by this step; both ports closed.
- Agent 1 ports (5183/2828): closed by previous step and re-confirmed closed here.
- No listener remains on any of the four ports.

## Verdict

Agent 2 survived Agent 1's stop (identical PIDs, ports stayed open), marker
`QA4-A-AGENT2` was confirmed live after the stop, evidence regenerated and
validated clean both before and after own shutdown, and the marker is still
uncommitted. Isolation holds.
