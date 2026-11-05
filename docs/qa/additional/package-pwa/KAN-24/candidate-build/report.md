# KAN-24 existing PR2950 candidate assets

Independent worktree `/workspace/qa-kan24-candidate`, branch `codex/qa-kan24-pr2950`, latest-main base `29a3eb23145da90706d8a94441373278521cc737`. Applied only the eight-file diff from canonical PR2950 base `7b72dbf35d9b551aebf2d8bdada943660880b910` to head `69d306b11c9e52da9e6270870740d49b1d561659` using `git apply --3way`. All paths applied cleanly, no unresolved textual conflicts. Exact patch SHA256 and per-source hashes: `source-subject.json`.

`deno install --frozen --allow-scripts` completed with exit0; standard `deno task feles-build build --phase before-mach` completed with exit0, including startup, browser chrome, all pages (settings/Hub included), and ESM modules. Logs: `frozen-install.log`, `before-mach.log`. Generated assets and SHA256 are in `generated-assets-inventory.json`; mounts are `_dist/noraneko/`. The tracked dependency lock is unchanged.

This is generated production frontend output, not a full native/source product build. No product was materialized, no push/new PR/Jira update performed. A clean application/build does not establish semantic compatibility: initial synchronization and stale form behavior remain for actual product comparison and the parent review. Preserve canonical PR2950; no duplicate PR.
