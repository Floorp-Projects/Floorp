# Pre-migration baseline report (Phase 0)

Baseline commit: `4a33a7a693ae` (`main` at inventory time)  
Captured: 2026-11-28 on Linux x86_64 with Deno 2.9.8

This report freezes the pre-migration behavior used to compare the Ruby/Node migration. It covers feature asset generation and host-tool tests. It does **not** claim to baseline installer/package artifacts, signed release artifacts, browser integration tests, or OS-specific runtime launch behavior yet.

## Production feature asset baseline

The baseline was produced in a clean temporary worktree with:

```sh
deno install
deno eval 'import { run } from "./tools/src/symlinker.ts"; run();'
deno eval 'import { run } from "./tools/src/builder.ts"; await run("production", "019b23c1-2278-7000-8000-000000000000");'
```

The direct `Builder.run` invocation intentionally isolates feature compilation from runtime download, patching, XHTML injection, packaging, and signing. The build completed successfully.

Aggregate artifact result:

| Metric | Value |
|---|---:|
| Files | 958 |
| Total bytes | 45,359,795 |
| Manifest | `baseline-artifact-manifest.csv` |

Per-package artifact summary:

| Package | Files | Bytes |
|---|---:|---:|
| `bridge/startup` | 6 | 39,639 |
| `bridge/loader-features` | 215 | 15,092,871 |
| `bridge/loader-modules` | 243 | 1,608,109 |
| `browser-features/pages-aboutDialog` | 6 | 219,961 |
| `browser-features/pages-modal-child` | 5 | 274,021 |
| `browser-features/pages-newtab` | 109 | 12,043,618 |
| `browser-features/pages-notes` | 5 | 794,310 |
| `browser-features/pages-profile-manager` | 5 | 328,160 |
| `browser-features/pages-settings` | 203 | 8,324,127 |
| `browser-features/pages-welcome` | 161 | 6,634,979 |

`baseline-artifact-manifest.csv` records one row per generated file:

- `package`: source package whose `_dist` output contains the artifact
- `path`: repository-relative artifact path in the baseline worktree
- `size_bytes`: exact byte size
- `sha256`: exact content hash

The manifest is suitable for file-list comparison. Hash comparison is useful, but hash equality is not the acceptance criterion for every migration step: dependency-version updates, chunk naming, sourcemap paths, build IDs, and intentional bundler-configuration changes can alter hashes without changing product behavior. Differences must be explained rather than ignored.

## Host-tool test baseline

Command run in the same clean worktree:

```sh
deno task test:host
```

Result:

| Metric | Value |
|---|---:|
| Passed | 2,311 |
| Failed | 0 |
| Ignored | 11 |
| Reported duration | 32s |

This is the primary regression baseline for Phase 3 host-tool migration. Browser-integrated colocated tests are not included because they require a prepared browser runtime and are part of a later runtime baseline.

## Frozen dev/test port topology

The authoritative port table is `libs/shared/dev-ports/defines.ts`. Current base values:

| Name | Port | Role |
|---|---:|---|
| `updateDevServer` | 5173 | Dev-only update fixture server |
| `designs` | 5174 | Designs/icon dev server |
| `profileManager` | 5179 | Profile manager page |
| `browserRemote` | 5180 | Marionette automation port |
| `loader` | 5181 | Feature loader used by bridge startup |
| `settings` | 5183 | Settings/Hub page |
| `modalChild` | 5185 | Modal child page |
| `newtab` | 5186 | New tab page |
| `welcome` | 5187 | Welcome page |
| `notes` | 5188 | Notes page |
| `workflowProgress` | 5192 | Workflow progress page |

The port offset contract is part of the runtime behavior baseline: one nonzero offset shifts the complete `5173–5192` range so parallel checkouts do not overlap. Ruby orchestration must preserve this contract exactly.

## Baseline gaps still to capture before final removal

| Gap | Why it matters | Suggested phase |
|---|---|---|
| Browser-integrated colocated test result | Validates features inside Firefox, not just host tooling | Phase 2/3 |
| Runtime download/patch/XHTML injection baseline | Covers the main Ruby orchestration responsibility | Phase 2 |
| Package matrix artifacts | Required before deleting CI `setup-deno` | Phase 4 |
| Windows/macOS host behavior | Process, junction/symlink, path, and installer behavior differ | Phase 2/4 |
| FDH/evidence compatibility baseline | FDH has schema and provenance contracts beyond ordinary tests | Phase 3 |

## Regression comparison rule

For each migration PR, compare at the narrowest useful level:

1. command exit status and failure text class;
2. generated file list;
3. manifest entries that are expected to be byte-stable;
4. behavioral tests for process/runtime contracts;
5. artifact hashes only after accounting for intentional dependency or chunking changes.

A migration is not complete when output merely exists. It is complete when the output contract is either identical or every difference is intentional, reviewed, and documented.
