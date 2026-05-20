<!--
SPDX-License-Identifier: MPL-2.0
-->

# NMA — Noraneko Module Archive

This directory contains the **Noraneko Module Archive (NMA)** subsystem,
ported from [noraneko](https://github.com/f3liz-dev/noraneko). NMA is a
dynamic module loading system that uses [Sigstore](https://www.sigstore.dev/)
to verify the authenticity of module archives before loading them into the
browser.

Think of it as a secure, content-addressed module delivery pipeline: the
browser downloads a signed archive, verifies the signer's identity against a
trusted list, then swaps in updated modules at runtime — no full reinstall
required.

---

## Status: foundation only (types + state)

The Floorp port is **not yet complete**. Two of the five NMA files are
intentionally absent: they depend on `@freedomofpress/sigstore-browser`, which
is not yet a Floorp dependency.

### What's here

| File | Layer | What it does |
|------|-------|--------------|
| `types.ts` | N0 | All NMA interfaces and enums. No imports, no side effects — pure declarations. |
| `state.ts` | N1 | Singleton state container. Depends only on `types.ts`. Contains `TODO(Floorp)` comments marking what needs to change. |
| `mod.ts` | N3 (stub) | Public API re-export. Currently exports types and state only; fuller exports will be wired in once `core.ts` and `loader.ts` are ported. |

### What's missing

| File | Layer | Blocked by |
|------|-------|------------|
| `core.ts` | N2 | `@freedomofpress/sigstore-browser` — handles FS/network/crypto IO and Sigstore verification |
| `loader.ts` | N3 | `core.ts` — the orchestrator that drives init, verify, and hotswap |

---

## Architecture

NMA is a **self-contained subsystem**: it does not depend on the main loader's
`types/` or `state/` directories. The internal dependency graph is a clean
four-level DAG:

```
N0: types.ts       pure type definitions, no imports
       ↑
N1: state.ts       singleton state + event bus, imports types.ts only
       ↑
N2: core.ts        FS / network / crypto IO, Sigstore verify   ← needs sigstore-browser
       ↑
N3: loader.ts      orchestrator: initializeNMALoader, verifyNMA, hotswap…
    mod.ts         public re-export surface
```

The main loader connects to NMA at its own L2/L3
(`io/loading.ts` and `mod.ts`), using `initNMASystem()`, `isNMAActive()`,
`hasNMAModule()`, and `loadNMAModule()`. When NMA is inactive, `isNMAActive()`
returns `false` and the existing module path is taken unchanged — no
disruption to what works today.

---

## Next steps

Complete the port in this order:

- [ ] **Add `@freedomofpress/sigstore-browser` to Floorp's `deno.json` imports**

- [ ] **Port `nma/core.ts`** — FS/network/crypto IO and Sigstore verification  
  Reference: `bridge/loader-features/loader/nma/core.ts` in noraneko `rftr-bridge`

- [ ] **Port `nma/loader.ts`** — the orchestrator (`initializeNMALoader`,
  `verifyNMA`, `hotswapModule`, etc.)  
  Reference: `bridge/loader-features/loader/nma/loader.ts` in noraneko

- [ ] **Wire `initNMASystem()` into `bridge/loader-features/loader/index.ts`**  
  Call it early in the startup sequence, before `loadEnabledModules()`. NMA is
  self-silencing when no archive is present, so this won't break anything on
  its own.

- [ ] **Resolve the `TODO(Floorp)` markers in `state.ts`**:
  - Add Floorp's repository (e.g. `"nyanrus/Floorp"`) to
    `DEFAULT_NMA_TRUSTED_CONFIG.allowedRepositories`
  - Update `NMA_PATHS.FILE_PATTERN` to match Floorp's archive naming convention

- [ ] **Rename `PREF_HASH_STATE` in `core.ts`** from `"noraneko.nma.hash_state"`
  to `"floorp.nma.hash_state"` — see critical notes below

---

## ⚠️ Critical notes

Three things that must be correct for NMA to work safely:

1. **`registerNMAResource` ordering** — must be called *before*
   `loadEnabledModules()`. If it runs after, `jar:` URL resolution fails
   silently and modules don't load.

2. **`state.loader.isActive` atomicity** — the sequence
   `verifyNMA → readManifest → isActive = true` must not be interrupted or
   reordered. Setting `isActive` before verification completes would allow
   unverified modules to be loaded.

3. **Pref key collision** — if `PREF_HASH_STATE` stays as
   `"noraneko.nma.hash_state"`, Floorp and noraneko share the same preference
   on any system where both are installed. Rename to `"floorp.nma.hash_state"`
   before enabling NMA in production.

---

## Pointers

- **Upstream NMA source**: `bridge/loader-features/loader/nma/` in
  [`f3liz-dev/noraneko`](https://github.com/f3liz-dev/noraneko), branch
  `rftr-bridge`
- **Architecture analysis** (DAG levels, category-theoretic formalization,
  "what would break" notes):
  `~/.mio/notes/noraneko-nma-structure.md`
- **Merge strategy and full roadmap**:
  `~/.myu/notes/noraneko-floorp-merge.md`
- **Session resume notes** (Phase A context):
  `~/.myu/notes/noraneko-floorp-resume.md`
