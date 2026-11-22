# Gate 4 task 4-A — isolation stop (Agent 1)

MARKER: QA4-A-AGENT1
WORKTREE: /tmp/fdh-gate2/pr2b2 (agent 1), OFFSET=0
SETTINGS_PORT: 5183, MARIONETTE_PORT: 2828
TASK_ID: gate4-a0
STAGE: stop step of the parallel isolation test

## Result: STOPPED — Agent 1 only

Agent 1's dev server and browser were left RUNNING by the parallel stage. This step
stopped only this worktree's dev via `fdh dev down`, and verified both of its ports
are closed.

### Pre-stop listeners (confirmed before stop)

| Port | Listener | PID |
|---|---|---|
| 5183 | deno (settings/dev server) | 21910 |
| 2828 | floorp (marionette browser) | 21970 |

### Stop command and outcome

```
cd /tmp/fdh-gate2/pr2b2
FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev down --task-id gate4-a0
```

- exit code: **0**
- stopped process count: **33** (`Stopped (33 processes).`)
- log: `dev-down.log` — verbatim:
  ```
  [fdh] deno run -A tools/dev-tool.ts stop
  Stopping dev processes for this checkout...
  Stopped (33 processes).
  ```

### Post-stop port verification (both closed)

`lsof -nP -iTCP:5183 -sTCP:LISTEN` → exit **1**, empty (0 bytes)
`lsof -nP -iTCP:2828 -sTCP:LISTEN` → exit **1**, empty (0 bytes)

Evidence files:
- `lsof-settings-closed.txt` (0 bytes)
- `lsof-marionette-closed.txt` (0 bytes)

Both must be empty; both are empty. `portsClosed: true`.

### Post-down evidence regeneration

```
deno run -A tools/fdh.ts evidence --task-id gate4-a0 --out _dist/fdh/evidence/gate4-a0.json
```

- exit code: **0**
- state: **succeeded**, assertions: 2, limitations: 12, bytes: 9013
- log: `evidence-postdown.log` — verbatim:
  ```
  {"taskId":"gate4-a0","out":"/private/tmp/fdh-gate2/pr2b2/_dist/fdh/evidence/gate4-a0.json","bytes":9013,"state":"succeeded","assertions":2,"limitations":12}
  ```
- copy: `evidence-postdown.json`
- evidence cleanup section records `result: "completed"` and
  `notes: "Dev server stopped by 'fdh dev down'."`

### Runner-role validation of post-down evidence

`validate-postdown.ts` (role `runner`) produced:
```json
{
  "role": "runner",
  "errorCount": 0,
  "errors": []
}
```
`validate-postdown.log` confirms `errorCount: 0`, `errors: []`.

The two assertions remain passed (browser-connected; browser-eval-contains
`QA4-A-AGENT1`). They describe the pre-stop recorded run, which is unchanged by
the stop — the post-down evidence reflects the same run plus cleanup completed.

## Isolation scope

- Stopped: **Agent 1 only** (`/tmp/fdh-gate2/pr2b2`, ports 5183/2828).
- Not touched: `/tmp/fdh-gate2/pr2b100` (Agent B) and `/tmp/fdh-gate4/pr2b200`
  (Agent C).
- Read-only cross-check after stop: sibling listener groups remain live —
  `127.0.0.1:5274–5292` (10 listeners incl. floorp 21959 on 5280) and
  `127.0.0.1:5374–5392` (10 listeners incl. floorp 23242 on 5380). No sibling
  deno/floorp process was stopped by this step.

## Remaining verification for the next stage

B and C remain RUNNING and are **to be re-verified by the next stage**
(`4-B` / `4-C` stop steps): confirm their pre-stop listeners, stop each with its
own `dev down --task-id`, verify their ports closed, regenerate their post-down
evidence, and validate runner-role -> `[]`. This stage does not assert B/C
closed ports.

Evidence state summary: `evidenceState = succeeded`; runner validation
`errors = []`; `stopped = true`; `stoppedCount = 33`; `exit = 0`;
`portsClosed = true`.
