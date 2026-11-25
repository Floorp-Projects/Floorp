# Packaged browser QA runner

This opt-in runner loads scoped browser tests from packaged `chrome:` resources
after the normal production core initializes. It uses the existing host collector,
expiring control file, run identifier, result preferences and browser test harness.
It does not start a development server or modify browser protection preferences.

The normal production Vite configuration does not import this configuration.
Compiling startup with `MODE=production` removes the QA entry entirely. A QA
artifact must be kept separate from a release artifact.

## Build a QA package through the normal pipeline

The package workflow's manual `packaged_qa_modules` input accepts a nonempty JSON
array of exact repository-relative test paths. It is available only for unsigned
`Linux-x64` runs with `provenance_mode=stable-v2-validation`, `skip_signing=true`
and `signed_validation=false`. The first workflow step rejects signed, release,
other-platform and reusable-workflow routes. The default empty input preserves
normal production builds.

For a local pre-mach build from the repository root:

```sh
FLOORP_PACKAGED_QA=1 \
FLOORP_QA_TEST_MODULES='["browser-features/modules/modules/i18n/test/LegacyBranding.test.mts"]' \
  deno task feles-build build --phase before-mach
```

This builds every normal production asset, selects the QA startup mode, then
builds scoped test resources and registers them in the generated content
`jar.mn`. The normal mach build and package stages consume those resources.
No finished product archive is edited. Clear both QA environment variables for
a regular production build; its clean asset build removes prior QA output.

## Build the two QA resources independently

From this directory, set `FLOORP_QA_TEST_MODULES` to a nonempty JSON array of exact
repository-relative browser test paths, then run:

```sh
FLOORP_QA_TEST_MODULES='["browser-features/modules/modules/i18n/test/LegacyBranding.test.mts"]' \
  deno run -A npm:vite build --config vite.packaged-test.config.ts
```

The output is `_dist/qa/`. Its manifest records the selected test source
hashes. This initial configuration was validated with the plain TypeScript KAN23
module; it does not promise the development build's JSX, decorators, alias or page
test transformations for arbitrary modules.

From `bridge/startup`, build startup with:

```sh
deno task build --env.MODE=test-packaged
```

In a dedicated QA package, register the runner output under
`chrome://noraneko/content/qa/` and use that QA `chrome_root.js`. All relative chunk
paths must remain intact. Prefer the integrated pipeline above for complete QA
artifacts. Manually substituting resources into a product copy
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
