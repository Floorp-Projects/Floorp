<!-- SPDX-License-Identifier: MPL-2.0 -->

# Firefox Test Import Workflow

Use this workflow when selecting upstream Firefox tests for Floorp's chrome
colocated runner. The goal is to turn Firefox tests into real Floorp runtime
coverage without committing a large raw upstream test dump or silently depending
on unsupported mochitest behavior.

## Scope

This workflow is for Firefox `browser-chrome` style tests that can plausibly run
inside the browser window through `deno task test --layer chrome`.

Defer other Firefox harness types until the Floorp runner explicitly supports
them:

- `xpcshell` tests
- plain mochitest content tests
- tests that require manifest-only support files
- tests that depend heavily on `SpecialPowers`, `ContentTask`, or complex
  cross-process mochitest helpers

## Candidate Record

Record one candidate entry before importing a file:

```text
Upstream path:
Firefox revision:
Retrieved:
Original test type:
Floorp target path:
Support files:
head.js dependencies:
Required harness APIs:
Expected Floorp behavior:
Compatibility decision:
Verification command:
Verification result:
Residual risk:
```

Use an exact revision when possible. If only a downloaded snapshot is available,
record the retrieval date and source URL.

At minimum, keep the upstream path and exact revision or retrieval date in the
imported file comment. Longer adaptation notes may live in the importing PR or
issue, but the file should retain enough provenance for later license review,
refreshes, and upstream comparison.

## Triage

Classify each candidate before copying code:

- `direct`: uses `add_task`, `registerCleanupFunction`, `ok`, `is`, `isnot`,
  `info`, `todo`, `Services`, and basic `gBrowser` access only.
- `small-adapter`: needs a narrow helper such as a small `head.js` extraction or
  a limited local helper that can be reviewed with the test.
- `runner-shim`: needs a reusable Firefox test helper such as a small subset of
  `BrowserTestUtils`, `TestUtils`, or `EventUtils`.
- `defer`: needs broad mochitest behavior, many support files, process/content
  task helpers, or behavior that Floorp cannot exercise yet.

Prefer importing `direct` and `small-adapter` tests first. Add runner shims only
when at least two real tests need the same helper and the helper can be
specified with focused behavior.

## Import Rules

Prefer downloaded raw tests first. Add the upstream path to
`firefox-downloaded/allowlist.json` when the raw file can run unchanged from the
downloaded `_dist/firefox-tests/files/` collection through a generated wrapper.
Generated wrappers live under `firefox-downloaded/generated/` and are ignored by
git.

Use `firefox-downloaded/quarantine.json` for downloaded raw candidates that are
known not to run yet. Keep the blocker, required APIs, source ref, and last
observed date in that metadata instead of adding a failing raw file to
`allowlist.json`.

When a test needs Floorp-specific changes, import or closely adapt Firefox test
code under `firefox-imported/` instead:

- Preserve the upstream MPL-2.0 license header and copyright notices.
- Add `// @colocated-env browser`.
- Add a file-local reference to `@types/mochitest-compat.d.ts` when TypeScript
  or `@ts-check` needs the Mozilla-style globals.
- Rename or wrap Firefox names such as `browser_example.js` as colocated files
  such as `example.test.js`; raw `browser_*.js` files are not discovered.
- Use `.test.js`, `.test.mjs`, or `.test.jsx` for borrowed-style tests that rely
  on injected `gBrowser` or `gBrowserInit`; the compatibility proxy is installed
  only for JavaScript module tests.
- Port `head.js` dependencies explicitly instead of assuming the upstream
  harness loaded them.
- Keep Floorp behavior deltas small and documented.
- Use `registerCleanupFunction` for tab, pref, window, listener, and DOM
  cleanup.
- Avoid changing the test so much that it stops checking the original Firefox
  behavior unless the Floorp-specific difference is intentional and recorded.
- Quarantine unstable imports instead of forcing weak assertions. Use a suffix
  that the colocated runner will not discover, such as `.quarantined.js`, and
  record why the test is not currently stable in Floorp.

## Shim Decision Rules

Before adding a new compatibility shim, write down:

- the Firefox API surface being emulated;
- the exact tests that require it;
- the subset Floorp will support;
- how failures will be reported;
- the cleanup behavior;
- the closest real Floorp command that proves it works.

Do not add broad placeholder helpers. A shim should fail loudly for unsupported
arguments or modes so imported tests do not become false positives.
Harness-shape compatibility, such as starting Mozilla-style tasks from one clean
selected `about:blank` tab, belongs in the shared runner layer instead of in
individual imported files.

## Verification

For downloaded raw tests, collect a browser-chrome snapshot, generate wrappers,
and run the generated directory:

```bash
deno task firefox-tests:collect --runtime-dir _dist/floorp-runtime --out _dist/firefox-tests --scope browser-chrome
deno task firefox-tests:triage-browser
deno task firefox-tests:prepare-browser
deno task test --near browser-features/chrome/test/firefox-downloaded/generated --layer chrome --list
deno task test --near browser-features/chrome/test/firefox-downloaded/generated --layer chrome
```

Review `_dist/firefox-tests/TRIAGE.md` before promoting more tests. Move only
small batches of stable candidates into `firefox-downloaded/allowlist.json`;
leave blocked candidates in `quarantine.json` or classify them as shim work.

Use the real Floorp test runner whenever practical. For repo-local adapted
tests, start with discovery, then run the imported test path:

```bash
deno task test --near browser-features/chrome/test/<area>/<file>.test.js --layer chrome --list
deno task test --near browser-features/chrome/test/<area>/<file>.test.js --layer chrome
```

For a group import, run the directory:

```bash
deno task test --near browser-features/chrome/test/<area> --layer chrome
```

When `--near` is used with an auto-started test browser, it narrows both
host-side discovery and browser-side execution. If an existing test browser is
already running, the browser-side filter and run id cannot be injected before
startup. Scoped runs fail fast in that state; stop the test browser and rerun,
or start the collector with `--no-autostart` before launching the browser.

If the import touches shared runner compatibility or adds a reusable shim, also
execute an existing borrowed-style suite in the real Floorp runner:

```bash
deno task test --near browser-features/chrome/test/mochitest-compat --layer chrome
```

Then run the broader smoke path as supplemental coverage:

```bash
deno task test:smoke --mode runtime
```

Record the observed command result next to the candidate record. If the real
Floorp runtime cannot be launched, record the blocker and the substitute check
instead of marking the import verified.

## Review Checklist

Before merging an imported test:

- The upstream path and revision or retrieval date are recorded.
- The file has the correct license header and `@colocated-env browser`.
- The test is discovered by `deno task test --near ... --list`.
- Required `head.js` or support-file behavior is ported or intentionally omitted
  with a reason.
- Every tab, pref, window, listener, and DOM mutation has cleanup.
- The test failed at least once during porting or has another concrete reason to
  believe it can catch the intended regression.
- Verification used real Floorp runner output, or the fidelity gap is recorded.
- Quarantined imports are not matched by `*.test.*`, and the quarantine reason
  is documented in the file.
