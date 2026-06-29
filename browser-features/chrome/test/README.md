<!-- SPDX-License-Identifier: MPL-2.0 -->

# Browser Chrome Tests

Floorp chrome tests run inside the Firefox/Floorp browser window through the
colocated test runner in `bridge/loader-features/loader/test/index.ts`.

## Test Styles

- `runAllTests` modules: Floorp's original colocated format. A test file exports
  `runAllTests()` and usually imports helpers from
  `browser-features/chrome/test/utils/test_harness.ts`.
- Mozilla-style task modules: compatibility format for small Firefox
  browser-chrome tests. A test file calls `add_task(...)` at module load time
  and uses globals such as `ok`, `is`, `isnot`, `info`, and
  `registerCleanupFunction`.

Mozilla-style task modules are discovered only when they match the existing
colocated naming rules, such as `*.test.js` or `*.test.ts`. Raw upstream Firefox
names such as `browser_example.js` are not discovered unless they are renamed or
wrapped.

## Borrowing Firefox Tests

Prefer downloaded raw Firefox tests before copying code into this repository.
Small tests are good candidates when they:

- use browser-window APIs such as `gBrowser` and `Services`;
- avoid `head.js`, `SpecialPowers`, manifest-only support files, and broad
  mochitest harness features;
- can run as a module after being renamed to `*.test.js` or `*.test.ts`.

If the raw upstream file can run unchanged, add it to
`firefox-downloaded/allowlist.json`. CI collects the raw files into
`_dist/firefox-tests/files/`, generates ignored wrappers under
`firefox-downloaded/generated/`, and runs those wrappers through the normal
Floorp test runner.

When copying or closely adapting upstream Firefox test code under
`firefox-imported/`:

- preserve the MPL-2.0 license header;
- record the upstream source path and revision or retrieval date;
- document meaningful Floorp-specific changes;
- keep cleanup explicit with `registerCleanupFunction`.

Use [Firefox import workflow](./firefox-import-workflow.md) when collecting
candidate Firefox tests or deciding whether a runner shim is worth adding.

Imported Firefox tests that are not stable in the Floorp runner should stay in
the source tree with a non-discovered suffix such as `.quarantined.js`, plus a
file-local note explaining the blocker. The remaining stable adapted imports
live under `firefox-imported/*.test.js`; `browserBug484315.quarantined.js` is
kept as evidence for a popup-window case that still needs a safer Floorp harness
path.

## Current Compatibility Surface

The compatibility layer intentionally supports only a small browser-chrome
subset:

- `add_task(fn)`
- `registerCleanupFunction(fn)`
- `ok(condition, message?)`
- `is(actual, expected, message?)`
- `isnot(actual, unexpected, message?)`
- `info(message)`
- `todo(condition, message?)`
- `gBrowser`, `gBrowserInit`, `BrowserCommands`, and `gURLBar` for
  borrowed-style `.test.js`, `.test.mjs`, and `.test.jsx` modules. These are
  resolved from the most recent browser window; `gBrowser` is exposed through a
  small Xray-waiving compatibility proxy.

Cleanups run in last-in, first-out order after tasks complete. They also run
when a task fails, and failures are reported together for the test file.
Before Mozilla-style tasks run, the compatibility layer normalizes the browser
window to a single selected `about:blank` tab, matching the startup assumption
used by many Firefox browser-chrome tests.

## Verification

Use the colocated runner as the primary proof path and let it auto-start the
test browser when possible:

```bash
deno task test --near browser-features/chrome/test/mochitest-compat --layer chrome
```

For iteration, `--near` narrows both host-side discovery and browser-side
execution:

```bash
deno task test --near browser-features/chrome/test/mochitest-compat/mochitestCompat.test.js --list
deno task test --near browser-features/chrome/test/mochitest-compat --layer chrome
```

For manual startup, start the collector first and launch the test browser in
another shell. Scoped runs cannot attach filter/run-id control prefs to a test
browser that is already running.

```bash
deno task test --no-autostart --near browser-features/chrome/test/mochitest-compat --layer chrome
deno task feles-build test
```
