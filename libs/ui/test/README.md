# Settings and welcome regression tests

Run with the repository's browser-integrated runner (close an already running
development/test browser before a scoped run):

```sh
deno task test --near browser-features/pages-settings
deno task test --near browser-features/pages-welcome
```

The host runner starts the required fixture servers automatically and stops the
servers it owns afterward. It reuses an already running fixture server. To
inspect a fixture manually, run these commands from the respective page
directory:

```sh
deno run -A vite --config test/integration/vite.config.ts
```

Then open one of these URLs in the development Floorp browser:

- `http://localhost:5196/test/integration/index.html` — settings preferences,
  reloads, failures and preservation of unexposed values.
- `http://localhost:5196/test/integration/index.html?suite=advanced` — Lepton,
  gestures, shortcut editor, update preferences and account actions.
- `http://localhost:5196/test/integration/index.html?suite=management` — panel
  creation/editing/deletion, installed web apps and Floorp OS controls.
- `http://localhost:5197/test/integration/index.html` — all welcome steps,
  theme, search engine, language, support choice and completion.
- `http://localhost:5197/test/integration/index.html?releaseNotes=1` — existing
  users' release notes prompt: initial choice, draft selection, dismissal,
  save failure and retry. Add `&confirmed=1` or `&readFailure=1` to check a
  saved choice or a failed preference load.

Tests render the real React pages and operate their controls in Gecko.
Preference storage and external browser/OS actions are replaced with in-memory
doubles. Failure cases inject rejected reads/writes and actor responses. These
tests cover the UI-to-storage contract and reload behavior; they do not install
language packs, change the OS default browser, remove real apps or reset a real
profile. They complement the existing pure configuration/migration tests.

**Do not serve these fixtures on native actor ports (5183/5187).** Fixture setup
rejects those origins so native actors cannot replace the doubles. The standard
runner hosts a focused remote content browser and reads its report through its
own message manager, matching Gecko's process isolation.

Each fixture exposes `data-ui-test-result` (JSON) and `data-ui-test-current` on
its document element. A finished report has `status: "done"`; every result must
have `ok: true`. The colocated `runAllTests()` wrappers fail if the report
contains a failure or does not finish.
