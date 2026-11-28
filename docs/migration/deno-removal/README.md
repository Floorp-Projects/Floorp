# Deno removal migration Phase 0

Phase 0 captures the pre-migration inventory and regression baseline for the project direction: remove Deno entirely, use Ruby only as a thin orchestrator, and use Node.js/pnpm for TypeScript builds and complex typed tooling.

Start here:

1. `migration-inventory.md` — scale, migration areas, dependency/import-map surface, CI surface, and sequencing implications.
2. `baseline-report.md` — validated pre-migration build/test baseline and comparison rules.
3. `deno-api-matrix.csv` — file-level `Deno.*` API inventory.
4. `deno-json-inventory.csv` — package/workspace configuration inventory.
5. `ci-deno-inventory.csv` — workflow line inventory.
6. `baseline-artifact-manifest.csv` — production feature artifact file list, sizes, and hashes.

These files are snapshots of baseline commit `4a33a7a693ae`. Regenerate or refresh them if the migration is paused long enough for `main` to change materially.
