# FDH Gate 4 — Task 4-A (AC-03) — Agent 2 Report

## Identity

- Agent: **2**
- Worktree: `/tmp/fdh-gate2/pr2b100`
- Base commit: `bec27040382280c17a9e4a573359f017dca89036`
- Port offset: **100**
- Settings port: **5283**
- Marionette port: **5280**
- Task ID: `gate4-a100`
- Marker: `QA4-A-AGENT2`
- Output directory: `/tmp/fdh-gate4/acceptance/4a-agent2`

## Exact commands and exit codes

| # | Command | Exit |
|---|---------|------|
| 1 | `mkdir -p /tmp/fdh-gate4/acceptance/4a-agent2` | 0 |
| 1 | `cd /tmp/fdh-gate2/pr2b100 && git status --short > .../pre-status.txt` | 0 |
| 1 | `git diff > .../pre-diff.txt` | 0 |
| 2 | `deno run -A tools/fdh.ts task start --task-id gate4-a100 --worktree "$(pwd)" --port-offset 100` | 0 |
| 3 | edit `browser-features/pages-settings/src/components/app-sidebar.tsx` line 133 → `title: t("sidebar.firefoxSettings") + " QA4-A-AGENT2",` | 0 |
| 3 | `git diff -- browser-features/pages-settings/src/components/app-sidebar.tsx > .../marker-diff.txt`; `grep -n 'QA4-A-AGENT2'` | 0 |
| 4 | `deno task feles-build build --phase before-mach` | **0** |
| 5 | `FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev up --task-id gate4-a100` | **0** |
| 6 | `deno run -A tools/dev-tool.ts navigate "http://localhost:5283/" --context content` | **0** |
| 7 | `deno run -A tools/fdh.ts verify browser --task-id gate4-a100 --eval "$(cat .../eval.js)" --expect QA4-A-AGENT2` | **0** |
| 8 | `deno task test:host` | **1** (one pre-existing failure) |
| 9 | `deno run -A tools/fdh.ts evidence --task-id gate4-a100 --out _dist/fdh/evidence/gate4-a100.json` | **0** |
| 9 | `deno run -A .../validate-run.ts` (`validateManifest(value, {role:"runner"})`) | **0** |
| 10 | `lsof -nP -iTCP:5283 -sTCP:LISTEN > .../lsof-settings-open.txt` | 0 |
| 10 | `lsof -nP -iTCP:5280 -sTCP:LISTEN > .../lsof-marionette-open.txt` | 0 |
| 11 | `git status --short > .../post-status.txt`; `git diff > .../post-diff.txt` | 0 |
| 12 | write `.../report.md` | 0 |

## Step details

### 1. Pre-state

`pre-status.txt`:
```
 M browser-features/pages-settings/src/components/app-sidebar.tsx
 M tools/feles-build.ts
```
`pre-diff.txt` contains the local uncommitted patcher guard in `tools/feles-build.ts` and the older `QA3-T02-MARKER` suffix on line 133 of `app-sidebar.tsx` (count: 1 occurrence).

### 2. Task start

```
{"taskId":"gate4-a100","worktree":"/private/tmp/fdh-gate2/pr2b100","baseCommit":"bec27040382280c17a9e4a573359f017dca89036","portOffset":100,"status":"running","statePath":"/private/tmp/fdh-gate2/pr2b100/_dist/fdh/tasks/gate4-a100.json"}
```
Exit 0.

### 3. Marker edit

Replaced the older `QA3-T02-MARKER` suffix on line 133 with `QA4-A-AGENT2`:
```
133:      title: t("sidebar.firefoxSettings") + " QA4-A-AGENT2",
```
`marker-diff.txt`:
```
-      title: t("sidebar.firefoxSettings"),
+      title: t("sidebar.firefoxSettings") + " QA4-A-AGENT2",
```
(The committed baseline has no marker; the prior QA3 marker was an uncommitted local edit, now replaced.)

### 4. Build

`deno task feles-build build --phase before-mach` — exit **0**. Log ends:
```
[builder] SUCCESS: Build complete.
```

### 5. Dev up

```
[dev-tool] Using port-offset=100 marionette=5280 pref=nora.dev.port_offset
Started (PID 19673). Waiting for Marionette...
Browser ready! Marionette on port 5280.
```
Exit **0**. Dev server (PID 19673) and browser (PID 21959) left RUNNING.

### 6. Navigate

```
[marionette] Connecting to port 5280
[marionette] Connected (protocol v3)
Navigated to http://localhost:5283/
Title: Floorp Hub
```
Exit **0** (content-context navigate).

### 7. Browser marker verification

`eval.js` used the content-process actor eval:
```js
(async () => {
  const mod = ChromeUtils.importESModule("chrome://remote/content/marionette/actors/MarionetteCommandsParent.sys.mjs");
  const bc = gBrowser.selectedBrowser.browsingContext;
  const actor = mod.getMarionetteCommandsActorProxy(() => bc);
  const raw = await actor.executeScript("return JSON.stringify({url: location.href, hit: document.body.innerText.includes(\"QA4-A-AGENT2\")})", [], {});
  const parsed = JSON.parse(raw);
  return parsed.hit ? "QA4-A-AGENT2 @ " + parsed.url : "ABSENT @ " + parsed.url;
})()
```
`verify browser` output:
```
QA4-A-AGENT2 @ http://localhost:5283/#/overview/home
```
Exit **0**. **MarkerConfirmed = true**, markerActual = `QA4-A-AGENT2 @ http://localhost:5283/#/overview/home`.

### 8. Host test

`deno task test:host` — exit **1**.

Final summary from the log:
```
FAILED | 425 passed | 1 failed | 1 ignored (5s)
```
The single failure is the known pre-existing `initializer.test.ts` failure:
```
locked Runtime install validates then swaps and retains recovery state => ./tools/src/initializer.test.ts:757:6
error: AssertionError: Values are not equal.
  [Diff] Actual / Expected
-   000
+   002
at file:///private/tmp/fdh-gate2/pr2b100/tools/src/initializer.test.ts:788:5
```
Recorded honestly; not hidden.

### 9. Evidence

`fdh evidence` — exit 0:
```
{"taskId":"gate4-a100","out":".../_dist/fdh/evidence/gate4-a100.json","bytes":8957,"state":"succeeded","assertions":2,"limitations":13}
```
Copied to `evidence-manifest.json` and `task-state.json` in OUT.

Manifest state (envelope): `taskStatus.state = "succeeded"`, `stateTransition: running → succeeded` at `2026-11-08T04:18:27.324Z`.
Assertions (2, both passed):
- `browser-connected` — expected `Connected to browser.` / actual `Connected to browser.`
- `browser-eval-contains` — expected eval output contains `QA4-A-AGENT2` / actual `QA4-A-AGENT2 @ http://localhost:5283/#/overview/home`

Limitations: 13, including build state UNVERIFIED and host test state UNVERIFIED (those commands were run via `deno task`, not recorded as fdh operations), cleanup partial because dev was intentionally left running, no visual judgment, local worktree not qualification target.

Validation with `validateManifest(manifest, { role: "runner" })` from `tools/qa3/evidence/validate.ts`:
```
{ "errorCount": 0, "errors": [] }
```
Exit 0. Expected [] — met.

### 10. Ports / dev left running

`lsof -nP -iTCP:5283 -sTCP:LISTEN`:
```
COMMAND   PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME
deno    20781 user   84u  IPv4 0xc38027e43b2056e4      0t0  TCP 127.0.0.1:5283 (LISTEN)
```
`lsof -nP -iTCP:5280 -sTCP:LISTEN`:
```
COMMAND   PID USER   FD   TYPE             DEVICE SIZE/OFF NODE NAME
floorp  21959 user   29u  IPv4 0xafbbd882d0cedefd      0t0  TCP 127.0.0.1:5280 (LISTEN)
```
Both listeners present. `devLeftRunning = true`. **No `fdh dev down` was run**; dev server and browser remain running for the isolation stage.

### 11. Post-state

`post-status.txt`:
```
 M browser-features/pages-settings/src/components/app-sidebar.tsx
 M tools/feles-build.ts
```
`post-diff.txt` shows 2 files changed (app-sidebar.tsx marker, tools/feles-build.ts patcher guard). Marker remains **uncommitted**. No commit/push/PR performed; other worktrees untouched.

## Summary for the isolation stage

Agent 2 has completed edit → build → dev up → content navigate → browser marker confirmation → host test → evidence → port checks, at offset 100, with dev server and browser still RUNNING. The marker `QA4-A-AGENT2` is visible at `http://localhost:5283/#/overview/home`, settings listener 5283 and Marionette listener 5280 are both open, and no `dev down` was issued.
