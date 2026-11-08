# FDH Gate 4 Task 4-A — Agent 3 Isolation Re-verify (after Agent 1 stop)

- **Agent:** 3
- **Worktree:** `/tmp/fdh-gate4/pr2b200` (realpath `/private/tmp/fdh-gate4/pr2b200`)
- **Port offset:** 200
- **Settings port:** 5383
- **Marionette port:** 5380
- **Task ID:** `gate4-a200`
- **OUT:** `/tmp/fdh-gate4/acceptance/4a-agent3` (realpath `/private/tmp/fdh-gate4/acceptance/4a-agent3`)
- **Marker:** `QA4-A-AGENT3`
- **Role:** re-verify step after Agent 1 (`/tmp/fdh-gate2/pr2b2`, offset 0, ports 5183/2828) was stopped by the previous step.

## 1. Survival proof — Agent 1's stop did not kill Agent 3's dev/browser

Captured while the Agent 3 dev server and browser were still running from the parallel stage, after Agent 1 had been stopped.

```
$ lsof -nP -iTCP:5383 -sTCP:LISTEN > "$OUT/lsof-settings-after-a-stop.txt"
COMMAND   PID USER   FD   TYPE   DEVICE SIZE/OFF NODE NAME
deno    23173 user   21u  IPv4 0xefa73eafff2b0cb1      0t0  TCP 127.0.0.1:5383 (LISTEN)

$ lsof -nP -iTCP:5380 -sTCP:LISTEN > "$OUT/lsof-marionette-after-a-stop.txt"
COMMAND   PID USER   FD   TYPE   DEVICE SIZE/OFF NODE NAME
floorp  23242 user   32u  IPv4 0xf9d45e78917af283      0t0  TCP 127.0.0.1:5380 (LISTEN)

$ lsof -nP -iTCP:5183 -iTCP:2828 -sTCP:LISTEN > "$OUT/lsof-a-closed.txt"
(empty; 0 bytes; lsof exit 1)
```

- **portsOpenBeforeStop:** true — listeners on 5383 (deno PID 23173) and 5380 (floorp PID 23242) survived Agent 1's stop.
- Agent 1's ports 5183/2828 were confirmed closed (empty output), so the two checks are discriminating: A stopped, Agent 3 alive.

## 2. Browser re-verify after A's stop

Command (exit 0):

```
$ deno run -A tools/fdh.ts verify browser --task-id "gate4-a200" \
    --eval "$(cat "$OUT/eval.js")" --expect "QA4-A-AGENT3"
```

Verbatim output (tail):

```
[marionette] Connecting to port 5380
[marionette] Connected (protocol v3)
[marionette] Session: fb49422b-c7bf-4f31-a852-451de1a03f26
Connected to browser.
  Title: Floorp Hub
  URL: chrome://browser/content/browser.xhtml
  Tabs: 1
  Active tab: Floorp Hub
[marionette] Connecting to port 5380
[marionette] Connected (protocol v3)
[marionette] Session: 1798b970-0c8e-408e-9a8f-8ece80d3ba9b
QA4-A-AGENT3 @ http://localhost:5383/#/overview/home
```

- **markerConfirmed:** true
- **markerActual:** `QA4-A-AGENT3 @ http://localhost:5383/#/overview/home`
- No NewSession timeout occurred; no dev down/up or navigate retry was needed. Two Marionette connections are expected: the `verify browser` connectivity probe followed by the content-actor eval.

## 3. Evidence after A's stop (dev/browser still up)

```
$ deno run -A tools/fdh.ts evidence --task-id "gate4-a200" \
    --out _dist/fdh/evidence/gate4-a200.json
{"taskId":"gate4-a200","out":"...","bytes":10686,"state":"succeeded","assertions":4,"limitations":13}
```

- Copied to `$OUT/evidence-after-a-stop.json`.
- Validation with `validateManifest(value, {role:"runner"})`:

```
$ deno run -A "$OUT/validate-after-stop.ts"
{
  "errorCount": 0,
  "errors": []
}
VALIDATE_EXIT=0
```

- Assertions (4): two `browser-connected` (expected/actual `Connected to browser.`) and two `browser-eval-contains` (actual `QA4-A-AGENT3 @ http://localhost:5383/#/overview/home`) — the second pair is this re-verify run appended to the task record.
- Cleanup at this point: `partial` (dev intentionally still running).

## 4. Own dev stopped

```
$ FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev down --task-id "gate4-a200"
[fdh] deno run -A tools/dev-tool.ts stop
Stopping dev processes for this checkout...
Stopped (32 processes).
DEVDOWN_EXIT=0
```

Port check after stop:

```
$ lsof -nP -iTCP:5383 -sTCP:LISTEN > "$OUT/lsof-settings-after-dev-down.txt"   # exit 1, 0 bytes
$ lsof -nP -iTCP:5380 -sTCP:LISTEN > "$OUT/lsof-marionette-after-dev-down.txt" # exit 1, 0 bytes
$ lsof -nP -iTCP:5383 -iTCP:5380 -sTCP:LISTEN                                  # exit 1, no output
```

- **portsClosedAfterStop:** true — both 5383 and 5380 are closed; no deno/floorp listener remains on Agent 3's ports.
- Cleanup recorded in task state: `cleanup.result = "completed"`, note `Dev server stopped by 'fdh dev down'.`, detail `dev-tool stop`.

## 5. Evidence after dev down

```
$ deno run -A tools/fdh.ts evidence --task-id "gate4-a200" \
    --out _dist/fdh/evidence/gate4-a200.json
{"taskId":"gate4-a200","out":"...","bytes":10814,"state":"succeeded","assertions":4,"limitations":12}
```

- Copied to `$OUT/evidence-post-down.json`.
- Validation:

```
$ deno run -A "$OUT/validate-post-down.ts"
{
  "errorCount": 0,
  "errors": []
}
VALIDATE_EXIT=0
```

- **evidenceState:** `succeeded` (both regenerations), **validationErrors:** `[]` (both validations, runner role).
- Manifest `envelope.taskStatus.state = "succeeded"`; `cleanup.result = "completed"`; limitations reduced 13 → 12 (the "dev still running / cleanup partial" limitation dropped).
- Assertions remain 4 (`browser-connected` ×2, `browser-eval-contains` ×2), all passed.
- Honest note: `_dist/fdh/tasks/gate4-a200.json` still reports `status:"running"` even though the dev-stop event and `cleanup down: completed` are recorded in its event list; the evidence envelope is `succeeded`. This is a task-record status quirk, not a failed stop — ports are verified closed.

## 6. Marker still uncommitted

```
$ git status --short > "$OUT/post-status.txt"
 M browser-features/pages-settings/src/components/app-sidebar.tsx
 M tools/feles-build.ts

$ git grep -n "QA4-A-AGENT3" HEAD -- browser-features/pages-settings/src/components/app-sidebar.tsx
(no match; exit 1)

$ git diff -- browser-features/pages-settings/src/components/app-sidebar.tsx | grep QA4
+      title: t("sidebar.firefoxSettings") + " QA4-A-AGENT3",
```

- Marker line present at `browser-features/pages-settings/src/components/app-sidebar.tsx:133` in the working tree, absent from HEAD → still **uncommitted**.
- `tools/feles-build.ts` remains modified from the earlier `FLOORP_SKIP_PATCHER` local guard (not part of any PR).
- No commit, push, or PR performed; no other worktree touched during this step.

## Commands and exit codes (this re-verify step)

| # | Command | Exit |
|---|---------|------|
| 1a | `lsof -nP -iTCP:5383 -sTCP:LISTEN > "$OUT/lsof-settings-after-a-stop.txt"` | 0 (listener present) |
| 1b | `lsof -nP -iTCP:5380 -sTCP:LISTEN > "$OUT/lsof-marionette-after-a-stop.txt"` | 0 (listener present) |
| 1c | `lsof -nP -iTCP:5183 -iTCP:2828 -sTCP:LISTEN > "$OUT/lsof-a-closed.txt"` | 1 (empty) |
| 2 | `deno run -A tools/fdh.ts verify browser --task-id gate4-a200 --eval "$(cat eval.js)" --expect QA4-A-AGENT3` | 0 (marker) |
| 3 | `deno run -A tools/fdh.ts evidence --task-id gate4-a200 --out _dist/fdh/evidence/gate4-a200.json` | 0 |
| 3b | `deno run -A "$OUT/validate-after-stop.ts"` | 0, errors `[]` |
| 4 | `FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev down --task-id gate4-a200` | 0 (`Stopped (32 processes).`) |
| 4b | `lsof -nP -iTCP:5383 -sTCP:LISTEN > lsof-settings-after-dev-down.txt` | 1 (empty) |
| 4c | `lsof -nP -iTCP:5380 -sTCP:LISTEN > lsof-marionette-after-dev-down.txt` | 1 (empty) |
| 5 | `deno run -A tools/fdh.ts evidence --task-id gate4-a200 --out _dist/fdh/evidence/gate4-a200.json` | 0 |
| 5b | `deno run -A "$OUT/validate-post-down.ts"` | 0, errors `[]` |
| 6 | `git status --short > "$OUT/post-status.txt"`; `git grep -n QA4-A-AGENT3 HEAD -- ...` | 0; 1 (uncommitted) |

## Summary

- **survived:** true — Agent 3's dev (5383) and browser (5380) remained listening after Agent 1 was stopped; Agent 1's ports 5183/2828 were empty.
- **markerConfirmed:** true — re-verify returned exit 0 with `QA4-A-AGENT3 @ http://localhost:5383/#/overview/home`.
- **portsOpenBeforeStop:** true; **portsClosedAfterStop:** true (own dev down exit 0, both own ports closed).
- **evidenceState:** succeeded; **validationErrors:** `[]` at both checkpoints.
- **Marker remains uncommitted.**

## Artifacts in OUT (this step)

- `lsof-settings-after-a-stop.txt`, `lsof-marionette-after-a-stop.txt`, `lsof-a-closed.txt`
- `browser-reverify-after-a-stop.log`
- `evidence-after-a-stop.log`, `evidence-after-a-stop.json`, `validate-after-stop.ts`, `validate-after-stop.log`
- `dev-down.log`
- `lsof-settings-after-dev-down.txt`, `lsof-marionette-after-dev-down.txt`
- `evidence-post-down.log`, `evidence-post-down.json`, `validate-post-down.ts`, `validate-post-down.log`
- `task-state-after-dev-down.json`
- `post-status.txt`
- `isolation-reverify.md`
- (from parallel stage, unchanged: `report.md`, `eval.js`, `evidence-manifest.json`, open-port lsofs, etc.)
