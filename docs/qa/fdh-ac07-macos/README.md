# FDH v1.0 AC-07 on macOS

**Result: the dev loop passed with a locally prepared Runtime.** A fresh
checkout could not launch with either the default Debug download or the
unmodified locked Release Runtime. The staging fallback and a subsequent full
dev launch both worked after unpacking the Release Runtime and applying the
existing BrowserGlue startup patch.

| Run                                       | Result                                                                                                                                                       |
| ----------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| Default Debug launch                      | Runtime index returned HTTP 403 after five attempts. Marionette never started.                                                                               |
| Locked Release launch                     | Dev assets built. XHTML injection failed with `ENOENT` because `browser.xhtml` was inside `browser/omni.ja`. Marionette never started.                       |
| Staging fallback                          | Stage build and XHTML injection completed. The staged Floorp binary launched with Marionette on `5280`. The packaged Hub shell and native Settings rendered. |
| Full dev launch with the prepared Runtime | The HTTP loader reached `loaded`. Floorp Hub rendered on `5283` and its Workspaces link opened the Workspaces settings.                                      |

Both successful launches used offset `+100`. The Floorp process owned the
loopback OS-Server listener on `127.0.0.1:5275`, and authenticated `GET /health`
returned `200 {"status":"ok"}`. The installed-browser port `58261` had no
listener. Checkout-scoped `dev-tool stop` removed 29 processes after staging and
31 after dev. Post-stop checks found no surviving tracked processes, bundle
processes, launcher state files, or listeners in the shifted port range or on
`58261`.

## Environment and setup

- Run date: 2026-11-21 UTC. macOS 26.6.2 (25G83), Darwin 25.6.0, Apple Silicon.
- Fresh clone of `Floorp-Projects/Floorp` at
  [`6f621741aeabe173572d8a5abb4618eb81997ea5`](https://github.com/Floorp-Projects/Floorp/commit/6f621741aeabe173572d8a5abb4618eb81997ea5),
  which matched `refs/heads/main` at clone time. Deno 2.9.8, Node.js 24.21.0.
  Dependency setup completed with `deno install --frozen`. FDH stored its state
  and the browser launch profile inside the clone's ignored `_dist/` tree.
- The locked macOS universal artifact was `daily-1180` at Runtime source
  `741c4537e89b550b0297ed7ea493026506930d4d`, version `160.0.1`, build ID
  `20261112140843`. The checkout's existing initializer installed the artifact.

## Run sequence

1. Ran
   `deno task fdh task start --task-id fdh-ac07-macos --worktree "$PWD" --port-offset 100`
   and `deno task fdh task prepare --task-id fdh-ac07-macos`. Preparation found
   no Debug Runtime and recorded the packaged-Runtime guard
   `FLOORP_SKIP_PATCHER=1`. Ran `deno task fdh dev up --task-id fdh-ac07-macos`,
   then repeated with `FLOORP_DEV_RUNTIME_DISTRIBUTION=release`. Ran
   `fdh verify browser` and `fdh dev down` after each attempt.
2. In the clone's `_dist/bin/floorp/Floorp.app/Contents/Resources`, extracted
   `omni.ja` into `Resources` and `browser/omni.ja` into `Resources/browser`.
   Preserved the archive's top-level `chrome.manifest` entries together with the
   generated manifest entry and moved the two archives out of the app bundle.
   Applied the repository's `prepareRuntimeBootstrap(BIN_DIR)` helper to the
   extracted `moz-src/browser/components/BrowserGlue.sys.mjs`. All of these
   changes were local to the test bundle.
3. Ran
   `FLOORP_SKIP_PATCHER=1 deno task feles-build stage --marionette --port-offset 100`.
   The command built production assets and launched the staged app in the
   isolated profile with `nora.dev.allow_http_loader=false`. Used
   `dev-tool status`, `navigate`, `eval`, `screenshot`, and `console`. Recorded
   the staging task wrapper with the existing `recordDevProcess` helper before
   running `deno task dev-tool stop`.
4. Created and prepared FDH task `fdh-ac07-macos-prepared` at the same offset,
   then ran
   `FLOORP_DEV_RUNTIME_DISTRIBUTION=release deno task fdh dev up --task-id fdh-ac07-macos-prepared`.
   Ran `fdh verify browser` with a chrome eval assertion for
   `"loader":"loaded"`, then used content eval to click Workspaces and verify
   its route and rendered text. Captured screenshots and the browser console.
   Ran `fdh dev down`, which invokes checkout-scoped `dev-tool stop`.

For each successful launch the OS-Server was already listening before the API
check. No server-module import was performed through Marionette. The health
probe read the bearer from the checkout profile in memory. The cleanup probe
compared PID and start time for the recorded processes and their descendants,
checked for processes launched from the app bundle, and checked control files
and TCP listeners.

## Evidence

- [Default dev manifest](default-dev.json) and
  [prepared dev manifest](prepared-dev.json) were generated by `fdh evidence`
  after teardown. The former retains both failed browser assertions. The latter
  records the successful connection and loader assertion.
- [Logs summary](logs-summary.md) includes launch, console, port, and shutdown
  excerpts.
- [Dev Floorp Hub](screenshots/dev-hub.png) and
  [dev Workspaces](screenshots/dev-workspaces.png) show the live dev UI before
  and after navigation.
- [Packaged Hub shell](screenshots/stage-hub.png) and
  [native Settings](screenshots/stage-preferences.png) are from the staging
  fallback. The Hub frame was captured while its main pane displayed “Loading…”.

This run covered browser startup, HTTP loader initialization, UI navigation,
OS-Server port isolation, and process teardown. A live source edit and HMR
update were outside the run. Host tests and package qualification were also
outside the run.
