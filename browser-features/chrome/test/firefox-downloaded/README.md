<!-- SPDX-License-Identifier: MPL-2.0 -->

# Downloaded Firefox Browser Tests

This directory keeps candidate-quarantine metadata and ignored generated
wrappers for selected raw Firefox browser-chrome tests. The repository-root
`floorp-runtime.lock.json` is the sole authority for the executable selection,
source commit, manifests, preferences, expected task counts, and locked support
material.

Raw upstream files are not committed here. CI collects them into
`_dist/firefox-tests/files/`, then
`tools/firefox-tests/prepare_firefox_browser_tests.ts` generates ignored
`generated/*.test.js` wrappers. Each wrapper exports
`__NORA_DOWNLOADED_FIREFOX_TEST__`; its `load` function lazily imports exactly
one locked raw file through the `#firefox-tests/` Vite alias. The runner applies
the typed locked preferences before loading the raw test.

Upstream `head.js` and other support files are integrity-locked but are not
executed or loaded. The marker records `headPolicy: "harness-replaced"` and
`supportPolicy: "locked-not-loaded"` to make that boundary explicit.

Add only tests that pass in the Floorp runner without editing the upstream raw
file. If a test needs Floorp-specific changes, keep it under
`../firefox-imported/` instead.

There is no separate allowlist. Update the canonical Runtime lock through its
reviewed lock-refresh flow when promoting or removing an executable raw test.
Use `quarantine.json` only for moving-ref candidates that should not run yet but
need a recorded blocker.

Normal collection is fail-closed: it requires the exact locked Runtime commit
and tree and verifies every material path, Git mode, Git blob ID, byte length,
and SHA-256 before generating wrappers. Candidate inspection must opt in with
`--candidate`, must explicitly name the lock's `trackingRef`, and verifies that
the checkout's `HEAD` resolves to that ref. It is a static-only projection of
the source closure and test set recorded in the Runtime lock; it reports drift
in that reviewed selection and does not claim to discover new upstream tests. An
optional `--path-prefix` can narrow this already bounded projection. The prepare
step rejects every candidate collection. Candidate manifests label this scope as
`locked-closure`, not as a full browser-chrome inventory.

After collecting candidate tests, run:

```bash
deno task firefox-tests:triage-browser
```

The triage step writes `_dist/firefox-tests/triage.json` and
`_dist/firefox-tests/TRIAGE.md` so candidates can be promoted in small batches.
