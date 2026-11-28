# Deno removal migration inventory (Phase 0)

Baseline commit: `4a33a7a693ae` (`main` at inventory time)  
Generated: 2026-11-28

This inventory is the migration input for removing Deno while converging on Ruby as the thin orchestrator and Node.js/pnpm as the JavaScript/TypeScript toolchain. It is a snapshot of the pre-migration tree, not a statement that every listed file must be rewritten by hand.

## Generated machine-readable inventory

| File | Rows | Contents |
|---|---:|---|
| `deno-api-matrix.csv` | 243 | Every tracked TypeScript file with a `Deno.*` reference, API counts, owning area, and migration priority |
| `deno-json-inventory.csv` | 22 | Every tracked `deno.json`, its role, task count, import count, workspace/export flags |
| `ci-deno-inventory.csv` | 169 | Every workflow line that references Deno, classified by use |
| `baseline-artifact-manifest.csv` | 958 | Pre-migration production artifact paths, sizes, and SHA-256 hashes |

The area and priority columns in `deno-api-matrix.csv` are mechanical classifications. They are intended for sequencing and risk assessment; they are not ownership assignments.

## Scale summary

| Surface | Count |
|---|---:|
| Tracked `deno.json` files | 22 |
| TypeScript files containing `Deno.*` | 243 |
| Total `Deno.*` API references | 3,846 |
| Files containing `Deno.test` | 133 |
| `Deno.test` references | 1,601 |
| Root `deno.json` tasks | 35 |
| Workflows with Deno references | 15 |
| Classified workflow references | 169 |
| Import-map path aliases | 7 |
| `@std/*` import-map entries | 7 |
| npm-backed import-map entries | 42 |

Most `Deno.*` references are concentrated in tests and host tooling, not browser feature runtime code. Only 9 files classified as feature/library code contain `Deno.*`, and 8 of those 9 are tests. The direct runtime migration surface is therefore much smaller than the raw 3,846-reference count suggests.

## `deno.json` roles

| Role | Files | Migration meaning |
|---|---:|---|
| Root workspace, command routing, shared import map | 1 | Replace with pnpm workspace plus explicit Ruby/Node entry points |
| Bridge build package | 3 | Convert build tasks to direct `vite`/`tsdown` execution |
| Browser feature package boundary/dependencies | 2 | Convert to package metadata/exports where still needed after bundling |
| Page package dependency/import configuration | 8 | Convert direct dependencies to npm package metadata |
| Library package boundary/dependencies | 6 | Convert only packages that remain runtime dependencies; type-only packages may disappear |
| Localization package boundary | 1 | Convert if imported as a package; otherwise plain source assets need no package boundary |

Important import-map aliases that must be removed or replaced:

- `#chrome/`
- `#features-chrome/`
- `#i18n/`
- `#libs/`
- `#modules/`
- `#themes/`
- `#ui/`

`@std` imports currently used through the import map:

- `@std/assert`
- `@std/cli`
- `@std/encoding`
- `@std/fs`
- `@std/path`
- `@std/streams`
- `@std/toml`

These should not be moved wholesale into runtime dependencies. Most tooling uses can be replaced with Node standard modules or small npm dependencies; browser-facing code should avoid depending on runtime-specific path/fs modules.

## Deno API concentration

Top API families by reference count:

| API | References | Main migration implication |
|---|---:|---|
| `Deno.test` | 1,601 | Largest mechanical surface; migrate by owning subsystem, not as one rewrite |
| `Deno.remove` | 242 | Mostly test temporary-file cleanup |
| `Deno.readTextFile` | 236 | Usually direct `node:fs/promises` replacement |
| `Deno.makeTempDir` | 201 | Mostly test isolation; use Node tempdir helpers |
| `Deno.writeTextFile` / `writeTextFileSync` | 287 | Direct `node:fs` replacement |
| `Deno.build` | 102 | Platform/arch detection; centralize in one runtime-neutral module |
| `Deno.mkdir` / `mkdirSync` | 173 | Direct `node:fs` replacement |
| `Deno.exit` | 84 | CLI entrypoints need explicit process exit handling |
| `Deno.Command` | 69 | Highest-risk surface; process spawning behavior differs by platform |
| `Deno.env` | 45 | Usually direct `process.env`, but shared configs must be runtime-neutral during transition |

## Migration areas

| Area | Files with `Deno.*` | Priority | Notes |
|---|---:|---|---|
| Build orchestration | 5 | P1 | Small surface, but central to the migration: `feles-build`, builder, patcher, injector, update |
| Runtime/process control | 22 | P1 | Highest-risk area: initializer, runtime archives, dev server, browser launcher, prepared runtime |
| Build config | 0 direct files in the mechanical classifier | P1 | Vite/tsdown configs still matter because they execute under Deno today even when they use few `Deno.*` APIs |
| Feature/library | 9 | P1 | Mostly tests; product feature code is already largely runtime-neutral |
| dev-tool | 1 direct file in the classifier | P2 | The CLI is small, but it depends on the runtime/process-control layer |
| Host tooling | 101 | P2 | Large but mostly mechanical fs/test migration |
| FDH/evidence | 76 | P3 | Large and semantically sensitive; migrate after the build/runtime boundary is stable |
| QA/release/packaging | 9 | P3 | Must preserve provenance and artifact contracts |
| Docs pipeline | 7 | P3 | Independent tooling; can migrate late or be replaced |
| Other | 13 | P3 | Mostly historical QA validation helpers plus a few native/static utilities |

The `build config` row is zero because the classifier keys on direct `Deno.*` API use. That undercounts runtime dependence: several Vite configs are loaded by Deno and can use Deno-only globals or import-map resolution even when they mostly use `node:*` modules.

## CI dependency shape

15 workflows contain Deno references. Of 169 classified references:

| Kind | Count |
|---|---:|
| `setup-deno` | 21 |
| `deno install` | 12 |
| `deno task` | 28 |
| `deno run` | 24 |
| `deno test` | 8 |
| Other `deno` references | 76 |

The high “other” count is mostly environment variables, paths, comments, cache keys, and conditional shell fragments rather than direct command invocations. CI migration therefore needs both command replacement and cleanup of assumptions around Deno cache/layout.

## Initial migration sequence implied by the inventory

1. Build assets and build configuration first (`P1`): feature code is already close to runtime-neutral.
2. Runtime/process control next (`P1`): this is the correctness boundary for Ruby orchestration.
3. dev-tool and general host tooling (`P2`): large but mostly mechanical.
4. FDH, QA evidence, release/packaging (`P3`): high semantic risk and tightly coupled to CI/evidence contracts.
5. Delete compatibility shims and `deno.json` files last.

## Dead or legacy candidates

No file was mechanically proven dead in Phase 0. The clearest deletion candidates are historical validation scripts under `docs/qa/fdh-gate*`: they have few production consumers and may be evidence artifacts rather than living tools. They should not be deleted solely from this inventory; confirm with FDH owners before removal.

## Known inventory limitations

- API counts are lexical counts, not runtime complexity scores.
- `Deno.test` count includes test registration density, which inflates the apparent cost of otherwise mechanical test files.
- Files that use Deno indirectly through `deno task`, import maps, or workspace resolution may have zero direct `Deno.*` references.
- The inventory does not yet classify npm versus JSR replacement difficulty package-by-package; that belongs to the Phase 1 dependency work.
