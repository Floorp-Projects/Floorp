# FDH Gate 3 T-02 Acceptance Report (AC-02, D-06c)

- **Model**: mimo-v2.6-flash (one of two DSH same-harness models on the same Skill/Tool contract)
- **Worktree**: /tmp/fdh-gate2/pr2b100 (git resolves to /private/tmp/fdh-gate2/pr2b100)
- **Base commit**: 4bbfe29fb0b8e48d52fcae5a45aa8268d73c1741 (unchanged before and after)
- **Port offset**: 100 (Marionette 5280, settings 5283)
- **Settings port**: 5283
- **Task id**: gate3-t02
- **OUT**: /tmp/fdh-gate3/acceptance/t02-mimo
- **Skill read**: `.agents/skills/fdh-dev-loop/SKILL.md` read FIRST (156 lines) and saved to `skill-read.log` (exit 0) — recorded before any fdh command.
- **Deliberate error**: none in T-02 (T-01 covers the error/fix loop).

## Exact ordered command list and exit codes

| # | Command (run from worktree root) | Exit | Key output |
|---|---|---|---|
| 1a | `git status --short > $OUT/pre-status.txt` | 0 | ` M tools/feles-build.ts` only (local patcher guard) |
| 1b | `git diff > $OUT/pre-diff.txt` | 0 | only tools/feles-build.ts FLOORP_SKIP_PATCHER guard |
| 2 | `sed -n '1,200p' .agents/skills/fdh-dev-loop/SKILL.md > $OUT/skill-read.log` | 0 | 156 lines; Skill contract read before any fdh command |
| 3 | `deno run -A tools/fdh.ts task start --task-id gate3-t02 --worktree "$(pwd)" --port-offset 100 2>&1 \| tee $OUT/task-start.log` | 0 | `{"portOffset":100,"status":"running","baseCommit":"4bbfe29..."}` |
| 4 | Edit `browser-features/pages-settings/src/components/app-sidebar.tsx` line 133 → `title: t("sidebar.firefoxSettings") + " QA3-T02-MARKER",` then `git diff -- <file> > $OUT/marker-diff.txt`; `grep -n QA3-T02-MARKER <file>` | 0 / 0 | diff shows exactly one line changed; grep: `133:      title: t("sidebar.firefoxSettings") + " QA3-T02-MARKER",` |
| 5 | `deno task feles-build build --phase before-mach 2>&1 \| tee $OUT/build.log` | **0** | `[builder] SUCCESS: Build complete.` |
| 6 | `FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev up --task-id gate3-t02 2>&1 \| tee $OUT/dev-up.log` | **0** | `Using port-offset=100 marionette=5280`; **`Browser ready! Marionette on port 5280.`** |
| 7 | `deno run -A tools/dev-tool.ts navigate "http://localhost:5283/" --context content 2>&1 \| tee $OUT/navigate.log` | **0** | `Navigated to http://localhost:5283/`; `Title: Floorp Hub` |
| 8a | Write adapted eval to `$OUT/t02-eval.js` (MarionetteCommandsParent actor proxy, chrome → content doc via selected browser's browsingContext, `document.body.innerText.includes("QA3-T02-MARKER")`) | 0 | file written, content verified |
| 8b | `deno run -A tools/fdh.ts verify browser --task-id gate3-t02 --eval "$(cat $OUT/t02-eval.js)" --expect "QA3-T02-MARKER" 2>&1 \| tee $OUT/browser-verify.log` | **0** | `Connected to browser.` … **`QA3-T02-MARKER @ http://localhost:5283/#/overview/home`** |
| 8c (optional) | `deno run -A tools/dev-tool.ts eval 'document.body.innerText.includes("QA3-T02-MARKER") ? "QA3-T02-MARKER" : "ABSENT"' -c content > $OUT/browser-verify-content-context.log` | **0** | `QA3-T02-MARKER` |
| 9 | `deno run -A tools/fdh.ts verify build --task-id gate3-t02 2>&1 \| tee $OUT/fdh-verify-build.log` | **0** | `[injector] SUCCESS: XHTML injection complete.` |
| 10a | `deno run -A tools/fdh.ts evidence --task-id gate3-t02 --out _dist/fdh/evidence/gate3-t02.json 2>&1 \| tee $OUT/evidence.log` | **0** | `{"state":"succeeded","assertions":3,"limitations":12}` (9320 bytes) |
| 10b | `cp` manifest + state to `$OUT/evidence-gate3-t02.json`, `$OUT/state-gate3-t02.json` | 0 | copies ok |
| 10c | Independent `validateManifest(value,{role:"runner"})` (import `tools/qa3/evidence/validate.ts`) → `$OUT/manifest-validate.log` | 0 | **`VALIDATION_ERRORS=[]`** |
| 11a | `git status --short > $OUT/post-status.txt`; `git diff > $OUT/post-diff.txt` | 0 / 0 | ` M browser-features/pages-settings/src/components/app-sidebar.tsx` + ` M tools/feles-build.ts`; nothing staged, HEAD unchanged |
| 11b | `grep -n QA3-T02-MARKER <file>` | 0 | `133: ... + " QA3-T02-MARKER",` still present, uncommitted |
| 12a | `FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev down --task-id gate3-t02 2>&1 \| tee $OUT/dev-down.log` | **0** | `Stopped (33 processes).` |
| 12b | `lsof -nP -iTCP:5283 -sTCP:LISTEN` (and port 5280) | **1** (no matches) | no output → no listener on 5283 (or 5280) after dev down; saved `port-check.log` |
| 13a | Re-run `fdh evidence …` → `$OUT/evidence-postdown.log` | **0** | `{"state":"succeeded","assertions":3,"limitations":11}` (9448 bytes) |
| 13b | `cp` → `$OUT/evidence-gate3-t02-postdown.json`; independent validate again | 0 | **`VALIDATION_ERRORS_POSTDOWN=[]`** |
| 14 | This report (`$OUT/report.md`) | 0 | written |

## Marker confirmation

- Source: `browser-features/pages-settings/src/components/app-sidebar.tsx:133`
  `title: t("sidebar.firefoxSettings") + " QA3-T02-MARKER",` (diff in `marker-diff.txt`, one line changed).
- Browser (adapted chrome→content eval via `fdh verify browser`, exit 0):
  **`QA3-T02-MARKER @ http://localhost:5283/#/overview/home`**
- Corroboration (content context): `QA3-T02-MARKER` (exit 0).
- Build: `[injector] SUCCESS: XHTML injection complete.` (exit 0), i.e. the built settings bundle contains the marker.

## Evidence (both generations)

- Pre-down (`evidence-gate3-t02.json`): state **succeeded**, assertions **3**, limitations **12**, cleanup `partial` (expected until dev down).
- Post-down (`evidence-gate3-t02-postdown.json`): state **succeeded**, assertions **3**, limitations **11**, cleanup **`completed`** ("Dev server stopped by 'fdh dev down'.", details: `dev-tool stop`).
- `taskStatus.state = "succeeded"`, `stateTransition: running → succeeded`.
- Assertions (all `passed: true`):
  1. `browser-connected` — expected/actual `Connected to browser.`
  2. `browser-eval-contains` — expected `eval output contains "QA3-T02-MARKER"`, actual `QA3-T02-MARKER @ http://localhost:5283/#/overview/home`
  3. `build-exit-code` — expected/actual `exit code 0`
- Operations recorded (5, all exit 0): dev-tool start (port-offset 100, marionette 5280, Browser ready!), dev-tool status (Connected to browser., Title: Floorp Hub), dev-tool eval (marker), `feles-build build --phase after-mach` ([injector] SUCCESS), dev-tool stop (Stopped 33 processes).
- `visualJudge: null`.
- Independent validation: `validateManifest(value, {role:"runner"})` → **`[]`** (pre-down and post-down both).

## Limitations (from manifest, 11 post-down)

1. Covers only the local fdh loop for gate3-t02; not a Gate 3 qualification or release approval.
2. No visual judgment (visualJudge is null).
3. DSH toolchain identity recorded UNVERIFIED.
4. Assertions reflect recorded exit codes/verbatim outputs only.
5. Local dev worktree is not the qualification target environment (matchesTarget false).
6. No GitHub/shared-service behavior exercised.
7. **No host test verification recorded (`deno task test:host` not run in T-02); host test state UNVERIFIED.**
8. Browser eval output verbatim; only `--expect` makes it a contains-check; does not prove behavior beyond the checked text.
9. No approval requested/recorded (approval not-required).
10. Build/toolchain commit is the start base; uncommitted working-tree diff is not hashed.
11. No defect reproduced; reproduction inconclusive by design.

Additional environment limitations for this run:
- Packaged Debug Runtime `.app` without moz-src (hand-prepared: unpacked browser.xhtml, dev CSP, noraneko-devdir mounts); marker was confirmed in the running packaged browser, not from a full source build tree.
- Local uncommitted guard skips `Patcher.run` when `FLOORP_SKIP_PATCHER=1` (local-only workaround, not part of any PR; visible in pre/post diff as `tools/feles-build.ts`).
- macOS only; no host test; no visual/look-and-feel judgment.

## Git status before / after

- Before: ` M tools/feles-build.ts` (local patcher guard only).
- After: ` M browser-features/pages-settings/src/components/app-sidebar.tsx` + ` M tools/feles-build.ts`; `git diff --cached --name-only` empty (nothing staged); HEAD still `4bbfe29fb0b8e48d52fcae5a45aa8268d73c1741` (`Merge pull request #3071 …`). Marker exists only as an uncommitted working-tree change (grep exit 0 at line 133).

## Cleanup proof

- `fdh dev down` exit 0, `Stopped (33 processes).`
- `lsof -nP -iTCP:5283 -sTCP:LISTEN` → exit 1, empty output (no listener).
- `lsof -nP -iTCP:5280 -sTCP:LISTEN` → exit 1, empty (Marionette released).
- Post-down evidence regenerated: cleanup `completed`, state `succeeded`, validation `[]`.

## Deviations / re-verify timeouts

- **No deviations from the prescribed command order.** No browser re-verify or verify-build timeout ("NewSession timed out — browser may be busy or BiDi conflict") occurred; no dev down/up recovery cycle was needed. All first attempts passed on the first try.
- Only environment-imposed adaptations already specified by the prompt: content context for navigation/eval (`--context content` / `-c content`) because the settings tab is a remote (Fission) content tab, and the adapted chrome→content eval for `fdh verify browser` (whose `dev-tool eval` default context is chrome).

This is T-02 evidence for the Lead/Gate record, not a Gate 3 pass by itself.
