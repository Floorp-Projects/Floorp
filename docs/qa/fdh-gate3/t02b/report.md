# FDH Gate 3 — T-02b (shared-service outage) acceptance report

Task: `gate3-t02b`
Worktree: `/tmp/fdh-gate2/pr2b2`
Base commit: `4bbfe29fb0b8e48d52fcae5a45aa8268d73c1741` (merged main: PR #3070 fdh CLI + PR #3071 fixes)
Port offset: `0` (Marionette 2828, settings 5183)
Evidence: `_dist/fdh/evidence/gate3-t02b.json` (`evidence-gate3-t02b.json`, post-down copy)
Run window: 2026-11-08 11:20–11:26 JST

**T-02b acceptance evidence: the T-01-like local loop completes while GitHub (shared service)
is unreachable. It is not a Gate 3 pass claim by itself.**

No commit, push, or PR was made. Only `/tmp/fdh-gate2/pr2b2` was modified (marker + pre-existing
local patcher guard).

---

## 1. Outage proof (recorded)

`outage-proof.log` — dead proxy for all external HTTP (`HTTPS_PROXY=http://127.0.0.1:9`,
`HTTP_PROXY=http://127.0.0.1:9`, `NO_PROXY=""`):

```
=== gh auth status (dead proxy) ===
github.com
  X Failed to log in to github.com account Ryosuke-Asano (keyring)
  - The token in keyring is invalid.
GH_EXIT:1
=== curl https://github.com/ (dead proxy, -m 5) ===
curl: (7) Failed to connect to 127.0.0.1 port 9 after 0 ms: Couldn't connect to server
CURL_EXIT:7
```

Baseline before the outage: `gh auth status` exit 0 (`baseline-gh.log`).

**Finding from the broad outage (honest):** with *all* external HTTP blocked, `fdh dev up`
fails before Marionette is ready:
```
[initializer] WARN: Runtime index fetch failed (attempt 1/5): fetch failed. ...
[feles-build] ERROR: Unhandled error: fetch failed
[dev-tool] Dev server exited before Marionette was ready.
```
(`dev-up.log`, exit 1). `Initializer.run({distribution:"debug"})` unconditionally fetches
`https://dev-assets.floorp.app/floorp-runtime-builds/.ftp-deploy-sync-state.json` before
checking whether the Debug Runtime is already installed. So the local loop has a runtime-asset
host dependency (not GitHub).

**Documented exemption for the loop (`outage-exemption-proof.log`):** the runtime-asset host is
exempted from the dead proxy (`NO_PROXY="dev-assets.floorp.app"`) because it is the build/runtime
distribution host, while GitHub stays blocked through the proxy:
```
=== exemption: dev-assets.floorp.app reachable directly while GitHub remains blocked ===
dev-assets HTTP 200
DEV_ASSETS_EXIT:0
=== GitHub still unreachable ===
GH_EXIT:1
CURL_EXIT:7
```
All loop commands below ran with `HTTPS_PROXY/HTTP_PROXY=127.0.0.1:9` and
`NO_PROXY=dev-assets.floorp.app`; GitHub was unreachable for the whole loop.

## 2. Commands, exit codes, key outputs

| Step | Command (proxy env active) | Exit | Key output |
|---|---|---|---|
| pre-state | `git status --short`, `git diff` | 0/0 | guard only (`pre-status.txt`) |
| outage proof | `gh auth status`; `curl -sS -m 5 https://github.com/` | **1 / 7** | GitHub unreachable (`outage-proof.log`) |
| skill read | `sed -n '1,200p' .agents/skills/fdh-dev-loop/SKILL.md` | 0 | `skill-read.log` |
| task start | `deno run -A tools/fdh.ts task start --task-id gate3-t02b --worktree "$(pwd)" --port-offset 0` | **0** | base `4bbfe29f…`, offset 0 (`task-start.log`) |
| marker edit | `app-sidebar.tsx:133` → `title: t("sidebar.firefoxSettings") + " QA3-T02B-MARKER",` | — | `marker-diff.txt` |
| build | `deno task feles-build build --phase before-mach` | **0** | `[builder] SUCCESS: Build complete.` (`build.log`) |
| dev up (broad outage) | `FLOORP_SKIP_PATCHER=1 fdh dev up` with all-external proxy | **1** | runtime index fetch failed (`dev-up.log`) |
| dev up (retry, asset host exempt) | same, `NO_PROXY=dev-assets.floorp.app` | **0** | `Browser ready! Marionette on port 2828.` (`dev-up-retry.log`) |
| navigate | `deno run -A tools/dev-tool.ts navigate http://localhost:5183/ --context content` | **0** | `Title: Floorp Hub` (`navigate.log`) |
| browser verify | `fdh verify browser --task-id gate3-t02b --eval '<actor eval>' --expect "QA3-T02B-MARKER"` | **0** | **`QA3-T02B-MARKER @ http://localhost:5183/#/overview/home`** (`browser-verify.log`) |
| content corroboration | `dev-tool eval '…includes("QA3-T02B-MARKER")…' -c content` | **0** | `QA3-T02B-MARKER` (`browser-verify-content-context.log`) |
| build verify | `fdh verify build --task-id gate3-t02b` | **0** | `[injector] SUCCESS: XHTML injection complete.` (`fdh-verify-build.log`) |
| evidence | `fdh evidence --task-id gate3-t02b --out _dist/fdh/evidence/gate3-t02b.json` | **0** | `{"state":"succeeded","assertions":3,"limitations":12}` (`evidence.log`) |
| post-state | `git status --short`, `git diff` | 0/0 | marker + guard (`post-status.txt`) |
| dev down | `FLOORP_SKIP_PATCHER=1 fdh dev down --task-id gate3-t02b` | **0** | `Stopped (33 processes).` (`dev-down.log`) |
| lsof 5183 / 2828 | `lsof -nP -iTCP:5183 -sTCP:LISTEN` / 2828 | **1 / 1** | no listeners (`lsof-5183-after-down.txt`, `lsof-2828-after-down.txt`) |
| evidence (post-cleanup) | same evidence command | **0** | `{"state":"succeeded","assertions":3,"limitations":11}` (`evidence-postdown.log`) |

## 3. Marker confirmation

`browser-verify.log`:
```
Connected to browser.
  Title: Floorp Hub
  URL: chrome://browser/content/browser.xhtml
  Active tab: Floorp Hub
QA3-T02B-MARKER @ http://localhost:5183/#/overview/home
```
Recorded assertion `browser-eval-contains`, expected `eval output contains "QA3-T02B-MARKER"`,
actual `QA3-T02B-MARKER @ http://localhost:5183/#/overview/home`, **passed: true**.
Content-context eval corroborated `QA3-T02B-MARKER` (the settings tab is a Fission content tab;
the adapted actor eval reaches the real tab document, same as T-01/T-02).

## 4. Evidence summary (2-D)

`evidence-gate3-t02b.json` (post-down copy):
- `envelope.taskStatus.state` = **`succeeded`**; `cancelRequested: false`.
- Assertions (3): browser-connected true, browser-eval-contains true, build-exit-code 0.
- Approval entity `decision: "not-required"`.
- `cleanup.result` = `completed` ("Dev server stopped by 'fdh dev down'").
- Operations include the failed broad-outage `dev up` (exit 1) and the successful retry (exit 0);
  the failure is recorded, not hidden.
- Limitations (11 post-down): local-loop-only; no visual judge; DSH pin UNVERIFIED; assertions
  only reflect recorded exit codes; matchesTarget false; **no shared services are exercised by
  the loop itself**; no host test; eval verbatim/only `--expect`; approval not-required;
  build/toolchain commit = task start base and uncommitted diff not hashed; reproduction
  inconclusive.
- `validateManifest(m,{role:"runner"})` → **`[]` errors** for both pre- and post-down manifests
  (`validate.log`, exit 0).

## 5. Uncommitted-diff preservation and cleanup

`post-status.txt`:
```
 M browser-features/pages-settings/src/components/app-sidebar.tsx
 M tools/feles-build.ts
```
Marker remains uncommitted at `app-sidebar.tsx:133`; guard is the pre-existing local-only
`FLOORP_SKIP_PATCHER` workaround (not part of any PR).
`dev down` stopped 33 processes; `lsof` on 5183 and 2828 shows no listener; `cleanup.result`
completed.

## 6. Limitations / deviations (honest)

- Broad dead-proxy outage (all external HTTP) does **not** let `fdh dev up` complete: the
  Debug Runtime index fetch from `dev-assets.floorp.app` fails (exit 1, recorded).
- For the completion run, `dev-assets.floorp.app` is exempted via `NO_PROXY` (runtime-asset
  host); GitHub stays unreachable (`gh` exit 1, `curl` exit 7) for the whole loop.
- Local packaged Floorp runtime without `moz-src`; `FLOORP_SKIP_PATCHER=1` local-only guard;
  `omni.ja`, dev CSP and `noraneko-devdir` hand-prepared; macOS local only; `matchesTarget:
  false`; no host test; visual judge null; DSH pin UNVERIFIED; uncommitted diff not hashed.
- `environmentFidelity.network` may read `enabled` while GitHub was blocked; the manifest
  limitation "no shared services are exercised" reflects the fdh loop, not the outage simulation.
- The T-02b loop is the T-01-like contract without a deliberate error; T-01 covers error/fix.

## 7. Conclusion

With GitHub unreachable (proved by `gh auth status` and `curl`), the T-01-like fdh loop
completed end to end: marker confirmed in the browser, build assertion passed, 2-D evidence
validated, uncommitted diff preserved, dev stopped. The one shared-service dependency found is
the runtime-asset host (`dev-assets.floorp.app`) used by `fdh dev up`; it was exempted and the
finding is recorded.
