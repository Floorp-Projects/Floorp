# Colocated Test Runner

This directory documents the colocated test style only.

## Scope

- Tests are placed next to source files or under a sibling `test/` folder.
- Default execution is browser integration testing.
- Browser test discovery follows the browser loader patterns:
- `browser-features/chrome/**/test/**/*.test.{ts,mts,tsx,js,mjs,jsx}`
- `browser-features/modules/**/*.test.{ts,mts,tsx,js,mjs,jsx}`
- `browser-features/pages-*/**/*.test.{ts,mts,tsx,js,mjs,jsx}`

## Tasks

- `deno task test`
- `deno task test:smoke`

## How to run

1. Start browser in test mode:

```bash
deno task feles-build test
```

1. Run all browser integration tests:

```bash
deno task test
```

1. Run only tests near a directory:

```bash
deno task test browser-features/chrome/common/split-view
```

1. Run only tests near a specific source file:

```bash
deno task test browser-features/modules/modules/chrome-web-store/polyfills/documentId/DocumentIdPolyfill.sys.mts
```

1. Run only a specific layer:

```bash
deno task test --layer chrome
deno task test --layer esm
deno task test --layer pages
```

1. Combine folder/file and layer filters:

```bash
deno task test browser-features/chrome --layer chrome
```

1. List targets without running tests:

```bash
deno task test --list
```

1. Run with explicit (generous) timeouts:

```bash
deno task test --timeout-ms 1800000 --startup-timeout-ms 1800000
```

1. Run smoke suite:

```bash
deno task test:smoke
```

1. Run smoke suite via dev-tool:

```bash
deno task dev-tool smoke
deno task dev-tool smoke --mode runtime
deno task dev-tool smoke --mode unit
```

Smoke runner policy:

- Smoke checks run only on Deno runtime commands (`deno test`, `deno check`, `deno lint`).
- Browser auto-launch and Marionette-based integration execution are excluded from smoke.
- Smoke includes lightweight Floorp-side scan via `deno task test --list --layer chrome|esm|pages`.
- Smoke runs `deno check` and `deno lint` across Floorp source directories (`tools`, `bridge`, `browser-features`, `i18n`, `libs`).
- Smoke does not stop on first failure; it executes all steps and returns a summarized failure at the end.

Runner timeout policy:

- `--timeout-ms` controls host-side browser result collection timeout (default/max: `1800000`).
- `--startup-timeout-ms` controls how long auto-start waits for a test browser (default/max: `1800000`).
- Runner enforces an overall execution cap of `1800000`ms (`30` minutes) across startup and collection stages.
- Invalid timeout values (non-integer, <= `0`, or > `1800000`) fail fast with an input error.

## Conventions

- Place browser tests as `*.test.ts` or `*.test.mts`.
- Chrome layer browser tests should be under a `test/` directory in `browser-features/chrome/**`.
- ESM layer browser tests can be colocated under `browser-features/modules/**`.
- Pages layer browser tests can be colocated under `browser-features/pages-*/**`.
- If a test module exports `runAllTests`, the runner calls it.
- Otherwise, the module is imported and treated as pass when import succeeds.
- Layer mapping (`--layer`):
- `chrome` -> `browser-features/chrome/**`
- `esm` -> `browser-features/modules/**`, `bridge/loader-features/**`
- `pages` -> `browser-features/pages-*/**`
