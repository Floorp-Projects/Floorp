# Colocated Test Runner

This directory documents the colocated test style only.

## Scope

- Tests are placed next to source files or under a sibling `test/` folder.
- Default execution is browser integration testing.
- Browser test discovery follows the browser loader patterns:
- `browser-features/chrome/**/test/**/*.test.{ts,mts}`
- `browser-features/modules/**/*.test.{ts,mts}`

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
- Smoke includes lightweight Floorp-side scan via `deno task test --list --layer chrome|esm`.
- Smoke runs `deno check` and `deno lint` across Floorp source directories (`tools`, `bridge`, `browser-features`, `i18n`, `libs`).
- Smoke does not stop on first failure; it executes all steps and returns a summarized failure at the end.

## Conventions

- Place browser tests as `*.test.ts` or `*.test.mts`.
- Chrome layer browser tests should be under a `test/` directory in `browser-features/chrome/**`.
- ESM layer browser tests can be colocated under `browser-features/modules/**`.
- If a test module exports `runAllTests`, the runner calls it.
- Otherwise, the module is imported and treated as pass when import succeeds.
- Layer mapping (`--layer`):
- `chrome` -> `browser-features/chrome/**`
- `esm` -> `browser-features/modules/**`, `bridge/loader-features/**`
- `pages` -> `browser-features/pages-*/**`
