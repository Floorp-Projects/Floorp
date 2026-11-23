# FDH Gate 3 — T-01 (AC-01) acceptance rerun with merged PR #3071

Task: `gate3-t01-final`
Worktree: `/tmp/fdh-gate2/pr2b2`
Base commit: `4bbfe29fb0b8e48d52fcae5a45aa8268d73c1741` (`main`, merge of PR #3071)
Port offset: `0` (recorded integer, never auto)
Task state: `_dist/fdh/tasks/gate3-t01-final.json`
Evidence manifest: `_dist/fdh/evidence/gate3-t01-final.json`
Run window: 2026-11-08 11:03–11:08 JST (`2026-11-08T02:03:05Z` – `2026-11-08T02:08:10Z`)

**This is the T-01 acceptance rerun after PR #3071 merged. It replaces the pre-merge
`gate3-t01` evidence (`state: failed`), which was blocked by the hardcoded `isCI: true`
after-mach path. It is not a claim that Gate 3 passed.**
No commit, push, or PR was made. Only `/tmp/fdh-gate2/pr2b2` was modified.

---

## 1. Pre-state (environment)

`pre-status.txt`:
```
 M tools/feles-build.ts
```
`pre-diff.txt` (917 bytes) is the **local-only patcher guard** in `tools/feles-build.ts`
(`Patcher.run` skipped when `FLOORP_SKIP_PATCHER=1`; 3 call sites). It is an environment
workaround for the packaged runtime without `moz-src`, **not** part of merged PR #3071 and not
part of this task's source change.

Worktree HEAD was the merged main (`4bbfe29f`, PR #3071 merge commit); the after-mach
`isCI: Deno.env.get("CI") === "true"` fix and the evidence honesty fixes were present from
`origin/main`, not from local copies.

## 2. Commands, exit codes, key outputs

| Step | Command | Exit | Key output |
|---|---|---|---|
| task start | `deno run -A tools/fdh.ts task start --task-id gate3-t01-final --worktree "$(pwd)" --port-offset 0` | **0** | `{"taskId":"gate3-t01-final","baseCommit":"4bbfe29f…","portOffset":0,"status":"running"}` (`task-start.log`) |
| marker edit | edit `browser-features/pages-settings/src/components/app-sidebar.tsx:133` → `title: t("sidebar.firefoxSettings") + " QA3-T01-MARKER",` | — | `marker-diff.txt` |
| build initial | `deno task feles-build build --phase before-mach` | **0** | `[builder] SUCCESS: Build complete.` (`build-initial.log`) |
| dev up | `FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev up --task-id gate3-t01-final` | **0** | **`Browser ready! Marionette on port 2828.`** (`dev-up.log`) |
| navigate | `deno run -A tools/dev-tool.ts navigate http://localhost:5183/ --context content` | **0** | `Navigated to http://localhost:5183/` / `Title: Floorp Hub` (`navigate.log`) |
| browser verify (adapted content-DOM eval) | `deno run -A tools/fdh.ts verify browser --task-id gate3-t01-final --eval '<actor eval>' --expect "QA3-T01-MARKER"` | **0** | **`QA3-T01-MARKER @ http://localhost:5183/#/overview/home`** (`browser-verify.log` final) |
| deliberate error | inserted `const qa3Broken = ;` after `static-panels.ts:9` | — | `broken-error-diff.txt` |
| broken build | `deno task feles-build build --phase before-mach` | **1** | `[swc] x Expression expected` at `…/link-features-chrome/common/panel-sidebar/data/static-panels.ts:10:1` (`build-broken.log`) |
| fix | removed the `const qa3Broken = ;` line | — | `fix-diff.txt` is **0 bytes** |
| fixed build | `deno task feles-build build --phase before-mach` | **0** | `[builder] SUCCESS: Build complete.` (`build-fixed.log`) |
| fdh verify build (merged code) | `deno run -A tools/fdh.ts verify build --task-id gate3-t01-final` | **0** | **`[injector] SUCCESS: XHTML injection complete.`** (`fdh-verify-build.log`) — the merged `isCI` fix selects local `BIN_DIR` |
| browser re-verify (attempt 1) | same adapted command | **1** | `NewSession timed out — browser may be busy or BiDi conflict` → recorded UNVERIFIED (`browser-verify-final.log`) |
| dev down/restart | `fdh dev down` (31 processes) → `fdh dev up` | **0/0** | `Browser ready! Marionette on port 2828.` (`dev-down-restart.log`, `dev-up-restart.log`) |
| navigate (restart) | `dev-tool navigate --context content` | **0** | `Title: Floorp Hub` (`navigate-restart.log`) |
| content eval corroboration | `dev-tool eval '…includes("QA3-T01-MARKER")…' -c content` | **0** | `QA3-T01-MARKER` (`browser-verify-content-context.log`) |
| browser re-verify (final) | same adapted command | **0** | **`QA3-T01-MARKER @ http://localhost:5183/#/overview/home`** (`browser-verify.log`) |
| evidence | `deno run -A tools/fdh.ts evidence --task-id gate3-t01-final --out _dist/fdh/evidence/gate3-t01-final.json` | **0** | `{"bytes":13189,"state":"succeeded","assertions":7,"limitations":12}` (`evidence.log`) |
| dev down | `FLOORP_SKIP_PATCHER=1 deno run -A tools/fdh.ts dev down --task-id gate3-t01-final` | **0** | `Stopped (33 processes).` (`dev-down.log`) |
| evidence (post-cleanup) | same evidence command | **0** | `{"bytes":13316,"state":"succeeded","assertions":7,"limitations":11}` (`evidence-post-down.log`) |

## 3. Marker confirmation (browser)

Final `fdh` browser assertion (`browser-verify.log`):
```
Connected to browser.
  Title: Floorp Hub
  URL: chrome://browser/content/browser.xhtml
  Tabs: 6
  Active tab: Floorp Hub
QA3-T01-MARKER @ http://localhost:5183/#/overview/home
```
Recorded assertion `browser-eval-contains`, expected `eval output contains "QA3-T01-MARKER"`,
actual `QA3-T01-MARKER @ http://localhost:5183/#/overview/home`, **passed: true**.

Context adaptation (as in the pre-merge run): the Floorp Hub settings tab is a remote
(Fission) content tab, so chrome-context `document.body.innerText` is the browser chrome;
the adapted eval uses the browser's own Marionette content-process actor proxy (same
evaluation path WebDriver uses for content) while keeping the marker text. Independently
corroborated by `dev-tool eval -c content` returning `QA3-T01-MARKER`. No screenshot or
visual judgment.

## 4. Deliberate error → log → fix

`build-broken.log` exit **1**, exact error lines:
```
[swc]   x Expression expected
    ,-[/private/tmp/fdh-gate2/pr2b2/bridge/loader-features/link-features-chrome/common/panel-sidebar/data/static-panels.ts:10:1]
 10 | const qa3Broken = ;
    :                   ^
```
(`link-features-chrome` is the symlinked chrome source path for
`browser-features/chrome/common/panel-sidebar/data/static-panels.ts`.)

Fix: removed exactly that line. `fix-diff.txt` = 0 bytes; `grep qa3Broken` finds nothing.
`build-fixed.log` exit **0**, `[builder] SUCCESS: Build complete.`
`fdh verify build` exit **0** with `[injector] SUCCESS` (merged-code after-mach on the local
`BIN_DIR`; the pre-merge run failed here with `ENOENT … obj-artifact-build-output/dist/bin/…`).

Note: the `deno task feles-build` before-mach builds are run outside the fdh CLI, so the
manifest's `operations` list records only fdh-driven commands (dev start/status/eval,
after-mach builds, stop). The error/fix proof is in the captured logs and diffs above.

## 5. Uncommitted-diff preservation

`post-status.txt`:
```
 M browser-features/pages-settings/src/components/app-sidebar.tsx
 M tools/feles-build.ts
```
- `post-diff.txt` contains the marker hunk (`grep QA3-T01-MARKER` = 1) and the guard.
- `cmp pre-diff.txt post-guard-diff.txt` → **IDENTICAL** (guard untouched; exit 0).
- `static-panels.ts` diff is empty; `qa3Broken` absent from `post-diff.txt` (count 0).
- Marker still present and **uncommitted** at `app-sidebar.tsx:133`:
  `title: t("sidebar.firefoxSettings") + " QA3-T01-MARKER",`

Guard caveat: `tools/feles-build.ts` remains modified (local `FLOORP_SKIP_PATCHER` guard),
present before the task; environment workaround, not task source.

## 6. Evidence summary (2-D)

Final manifest: `evidence-gate3-t01-final.json`, state: `state-gate3-t01-final.json`.

- `schemaVersion` 1, `sourceFormat` `work-record`; `envelope.taskStatus.state` = **`succeeded`**,
  `cancelRequested: false`.
- Assertions (7, in order): browser-connected true, browser-eval-contains true, build-exit-code
  0, build-exit-code 0, browser-connected false (the timed-out attempt), browser-connected true,
  browser-eval-contains true.
- Approval entity: `decision: "not-required"` (host-authorization namespace) + limitation
  "No approval was requested or recorded".
- `cleanup.result` = `completed`, notes "Dev server stopped by 'fdh dev down'".
- `attempts[0].result` = `passed`; 11 commands, 7 assertions.
- Limitations (11, post-cleanup): local-loop-only / not Gate 3 qualification; no visual
  judgment; DSH pin UNVERIFIED; assertions only reflect recorded exit codes; matchesTarget false;
  no shared services; no host test; eval verbatim + only `--expect` contains-check; approval
  not-required; build/toolchain commit = task start base and uncommitted diff not hashed;
  reproduction inconclusive by design.
- `validateManifest(m, { role: "runner" })` → **`[]` errors** (`validate.ts`, exit 0).

## 7. Environment caveats (unchanged)

- Local packaged Floorp runtime without `moz-src`; `FLOORP_SKIP_PATCHER=1` local-only guard;
  `omni.ja`, dev CSP and `noraneko-devdir` hand-prepared.
- macOS local only; `matchesTarget: false`; no host test verification; no visual judge;
  DSH pin UNVERIFIED; uncommitted diff not hashed.
- The T-01 rerun is one task; T-02/T-02b were executed after it and Gate 3 exit passed 2026-11-08
  (this report was written before those runs; the Gate 3 exit record is in `../README.md`).

## 8. Conclusion

T-01's loop completed with the merged PR #3071 code: a real small edit built and confirmed in
the running browser, a deliberate error was found in build logs and fixed, the build passed,
`fdh verify build` passed (after-mach injection on the local bin), the uncommitted diff was
preserved, and the emitted 2-D manifest is valid with `state: succeeded`.
