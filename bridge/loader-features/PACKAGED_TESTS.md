# Packaged browser QA runner

This opt-in runner loads scoped browser tests from packaged `chrome:` resources
after the normal production core initializes. It uses the existing host collector,
expiring control file, run identifier, result preferences and browser test harness.
It does not start a development server or modify browser protection preferences.

The normal production Vite configuration does not import this configuration.
Compiling startup with `MODE=production` removes the QA entry entirely. A QA
artifact must be kept separate from a release artifact.

## Build the two QA resources

From this directory, set `FLOORP_QA_TEST_MODULES` to a nonempty JSON array of exact
repository-relative browser test paths, then run:

```sh
FLOORP_QA_TEST_MODULES='["browser-features/modules/modules/i18n/test/LegacyBranding.test.mts"]' \
  deno run -A npm:vite build --config vite.packaged-test.config.ts
```

The output is `_dist/packaged-test/`. Its manifest records the selected test source
hashes. This initial configuration was validated with the plain TypeScript KAN23
module; it does not promise the development build's JSX, decorators, alias or page
test transformations for arbitrary modules.

From `bridge/startup`, build startup with:

```sh
deno task build --env.MODE=test-packaged
```

In a dedicated QA package, register the runner output under
`chrome://noraneko/content/qa/` and use that QA `chrome_root.js`. All relative chunk
paths must remain intact. This change does not yet wire these outputs into the
complete package workflow. Manually substituting resources into a product copy
is a diagnostic run, not acceptance of an immutable complete product.

## Collect a scoped run

Prepare the dedicated `_dist/profile/test` profile, including any normally
installed signed language pack, and close its browser. Before launching the QA
package with that profile, start the existing collector from the repository root:

```sh
deno task test --near browser-features/modules/modules/i18n/test/LegacyBranding.test.mts \
  --no-autostart --timeout-ms 120000
```

Launch the QA browser only after the collector writes `nora-tests-control.json`.
The requested modules must all be present in the QA bundle. Missing, empty,
expired or unavailable selections fail closed. A process-wide claim is made
before asynchronous core initialization; subsequent windows load the production
features but cannot consume the request again or overwrite the owner's result.

Use the original protection settings or stronger settings in the dedicated
profile. Do not use the development preference helper for this route. Close only
the browser belonging to this dedicated run after collecting results.
