# Porting the noraneko architecture into Floorp

Floorp is being incrementally refactored to adopt the architecture of the
`noraneko` testbed. This document tracks progress and the remaining work.

Working branch: `port/noraneko-refactor`.

## Done

| Step | Change | Commit |
|---|---|---|
| 1 | Remove the NMA (Noraneko Module Archive) subsystem | `5b46dcb2` |
| 2 | Restructure the module loader into a DOP layout | `565e0923` |
| 3 | Migrate `custom-shortcut-key` off io-ts/fp-ts to valibot | `5fbddaba` |
| 5 | Port the `lib/core` module framework from noraneko | `8a0f1651` |

Step 4 (rename the inter-module dispatcher to `ModuleEventBus`) was **N/A**:
Floorp has no equivalent of noraneko's `lib/core` event dispatcher. The
`EventDispatcher` under `modules/actors/webscraper/` is unrelated, and the
`birpc` usage in `pages-*/src/lib/rpc/` is page↔chrome RPC — a separate
mechanism, left as-is.

After these steps Floorp has an NMA-free DOP module loader and the `lib/core`
module framework (`registerModule`, `module_factory`, `registry`,
`module-event-bus`). All of this is **additive** — the existing
`chrome/common/` features are untouched and keep using their current loader.

## Remaining

### Step 6 — `features/` framework + migrate `chrome/common/` features

Introduce `browser-features/chrome/features/` and migrate the features in
`chrome/common/` to the DOP feature structure — **incrementally, not
big-bang.**

Each feature is a directory with a `mod.ts` entry point; it grows into DOP
layers only as needed:

```
feature-name/
├── mod.ts      # entry point — registerModule({ ... }, import.meta)
├── types/      # type definitions & valibot schemas
├── data/       # constants, defaults, pref names
├── ops/        # pure functions (no side effects)
├── io/         # side effects (DOM, prefs, network)
├── state/      # @preact/signals reactive state
└── ui/         # Preact components
```

**Progressive disclosure** — a small feature stays a single `mod.ts`; add the
sub-directories only once the feature is large enough that the separation
earns its keep.

A per-feature migration couples three changes:

1. **DOP restructure** — the directory layout above.
2. **Drop fp-ts/io-ts** — replace that feature's `fp-ts` with `@mobily/ts-belt`
   and `io-ts` with `valibot`. (Feature-level io-ts/fp-ts was intentionally
   left in place in step 3; it is removed per-feature here.)
3. **Solid → Preact** for the feature's UI — this is owned by the
   `preact-xul-migration` branch; coordinate, do not duplicate.

**Ordering** (lowest risk first): pilot the smallest independent leaf
features, then the shared hubs (`designs`, `modal-parent`), then medium
features, then the large ones (`workspaces`, `panel-sidebar`, `split-view`,
`mouse-gesture`, `pwa`) — each large feature as its own project. One feature
per PR. The loader supports `index.ts` and `mod.ts` side by side, so migrated
and unmigrated features coexist with no flag-day cutover.

### Step 7 — hotfix delivery as a privileged add-on

Replace NMA-style hotfixing with **add-on based delivery**: a privileged
"hotfix host" add-on shipped builtin in `omni.ja`, updated out-of-band via the
train-hop pattern with a Floorp-signed XPI.

Requires ~2 patches in the Gecko runtime:

- `security/manager/ssl/addons-public.pem` — add Floorp's signing root.
- `toolkit/mozapps/extensions/internal/XPIInstall.sys.mjs` `getSignedStatus`
  — recognise Floorp's certificate OU as a privileged signing state.

Two tiers: **data hotfixes** (CSS / Fluent / config — an unprivileged data
add-on, frequent and safe) and **logic hotfixes** (the privileged host — rare,
reviewed, sigstore-attested). This is a separate, larger track.

## Notes

- `@mobily/ts-belt` and `valibot` are now dependencies (added in steps 3 / 5).
- `fp-ts` / `io-ts` remain only inside `chrome/common/` features; they are
  removed as each feature is migrated in step 6.
- `lib/core` currently has no consumers — it is infrastructure placed ahead of
  step 6.
