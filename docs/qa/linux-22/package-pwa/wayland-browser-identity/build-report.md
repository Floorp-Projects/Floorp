# Ordinary-browser identity candidate: generated modules and isolated product copy

Prepared only in independent source worktree`/workspace/qa-linux-wayland-browser-fix`, based on3032 source`8249e234adfbb3e877e4a20cf5f57d7264d9a447`. Root owns the three source/test implementation changes. This helper task did not push, dispatch, modify another source worktree or change product protection/verification settings.

## Frozen preparation and checks

- Read this worktree's AGENTS.md and cloud-environment runtime skill; managed network is enforced/unrestricted and VPN is not configured. No secret values were inspected or logged.
- `deno install --frozen --allow-scripts` succeeded using the existing dependency cache and this worktree's own node_modules. Lockfile remained unchanged.
- Latest changed production/test files pass frozen Deno check, lint, format check and`git diff --check`. The only source write by this helper was root-authorized formatting of the added test; implementation edits remained root-owned.
- The intermediate direct`Services.appinfo.remotingName` use failed the repository's older Gecko type declaration in two places. The exact locked Runtime does expose this API (`nsIXULAppInfo.idl`68 / `nsAppRunner.cpp`1174). Root supplied a typed narrow optional field, after which checks passed. No `any` or disabled type validation was introduced.

## Minimal standard production module build

The existing full production builder invokes the loader-modules workspace task, whose definition is`deno run -A npm:tsdown --config-loader unrun`. The focused equivalent with frozen dependency enforcement is:

```sh
# In the independent worktree's bridge/loader-modules directory:
NODE_ENV=production deno run -A --frozen npm:tsdown --config-loader unrun \
  --env.__BUILDID2__=01a1ac9e-ba04-71bb-a115-9231694b2853 \
  --env.__VERSION2__=12.21.1
```

The module entry symlink`link-modules` points to this worktree's`browser-features/modules`, matching the normal symlinker mapping. BUILDID2 above is the original completed3032 CI product's existing`browser/buildid2`; it was not used as a new source-identity assertion. Exact source file/diff SHA256 values supply that identity. An earlier exploratory invocation used a different BUILDID2 argument; only the corrected final invocation's regenerated output was materialized or handed off.

Final standard tsdown build succeeded, generating184 files/approximately1.50MB. Source code's`Linux.sys.mjs` entry re-exports a content-hashed shared Linux chunk, so replacing only that entry would be insufficient.

## Controlled product copy

`materialize_candidate.py` copied the authenticated completed3032 CI product from`/workspace/qa-linux-full-build/products/3032/floorp` to:

**`/workspace/qa-linux-22/package-pwa/wayland-browser-identity/candidate-product/floorp`**

It compared every normal generated`.mjs` output to the base archive.177 generated modules were byte-identical. The exact generated differences in`omni.ja` are four changed resources:

- `modules/pwa/SsbCommandLineHandler.sys.mjs`
- `modules/pwa/supports/Linux.sys.mjs`
- `modules/pwa/test/SsbCommandLineHandler.test.mjs`
- `modules/pwa/supports/test/Linux.test.mjs` (its shared-chunk import changes)

The new`Linux.sys-gk7qUyeB.mjs` shared chunk was added and obsolete`Linux.sys-Boapfgj3.mjs` removed. Full member names and before/after SHA256 values are in`candidate-resource-provenance.json`.

The materializer independently compares both archive member contents and all product files: **only`omni.ja` changes; native binaries and every other product file/symlink remain unchanged**. It refuses a populated candidate output and checks that every changed archive resource is a generated ESM within the expected resource prefix. Original completed3032 product files were not edited.

The first materializer attempt failed locally because reusing a mutable ZipInfo object changed its in-memory header offset. No tests used that incomplete copy. The helper was corrected to copy ZipInfo metadata; the incomplete copy is separately named`candidate-product-first-attempt`. The final copy completed all independent archive/file invariance assertions. This was a local QA-tool failure, not a product failure.

**Scope: generated-resource substitution into an isolated actual product copy; NOT a complete new product/source/package build or signed release.** Root will use actual native regression evidence before committing/pushing its dependent Draft and starting the existing full package CI path.

## Hand-off and evidence

Candidate/provenance paths were sent to root and the actual PWA/native and colocated-test agents. The latter uses a separate fresh profile/display97 and the included actual`resource://noraneko/modules/pwa/test/SsbCommandLineHandler.test.mjs`. This helper did not claim those execution results in advance.

Logs:`frozen-deps.log`, `typecheck-final.log`, `lint-final.log`, `fmt-check-final.log`, `production-modules-build-final.log`, `materialize-candidate-final.log`. Source/output/native correspondence:`candidate-resource-provenance.json`. Do not commit the candidate products, node_modules or large archives; share the small report/scripts/logs/provenance only.
