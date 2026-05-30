# PR #2447 — Deferred CodeRabbit Review Items

## Background

PR #2447 (`port/noraneko-refactor`) resolved the CodeRabbit review's 🔴 Bugs and
🟡 Design issues across `browser-features/`, `bridge/`, and `libs/` (commits
`d2fa8a96`, `48d554d0`, `a5614002`).

Three items were deferred because each is a larger change than the surrounding
fixes — a cross-cutting refactor, a multi-interpretation scope decision, or a
structural redesign. This document records each so it can be picked up as its
own task.

All paths are relative to the repository root. State verified against
`port/noraneko-refactor` @ `a5614002`.

---

## 1. Actor filename typo: `NRExperimemmt` → `NRExperiment`

### Summary

The JSWindowActor pair `NRExperimemmtChild` / `NRExperimemmtParent` carries a
typo — `Experimemmt` (doubled `m`) instead of `Experiment`.

### Current state

The typo is **consistent** across every reference, so the browser works as-is:
the registration key, the file names, the exported window globals, and the
consumer all agree on the misspelling. It is a cosmetic correctness issue, not
a functional bug.

### Files affected (5)

| File | Reference |
|---|---|
| `browser-features/modules/actors/NRExperimemmtChild.sys.mts` | class `NRExperimemmtChild`; methods `NRExperimemmtPing/Send/RegisterReceiveCallback`; `Cu.exportFunction(..., { defineAs: "NRExperimemmt*" })` |
| `browser-features/modules/actors/NRExperimemmtParent.sys.mts` | class `NRExperimemmtParent` |
| `browser-features/modules/modules/BrowserGlue.sys.mts` (~L53) | actor registration key `NRExperimemmt:` and the two `../actors/NRExperimemmt*.sys.mts` import paths |
| `browser-features/modules/common/defines.ts` (~L51) | `interface NRExperimemmtParentFunctions` |
| `browser-features/pages-settings/src/lib/rpc/experiments.ts` | type import; `globalThis.NRExperimemmtSend` / `globalThis.NRExperimemmtRegisterReceiveCallback` reads |

### Why deferred

The rename must change in lockstep:

- The two source files (rename on disk).
- The `defineAs` strings in the child actor — these define globals on the
  content window, and the `pages-settings` consumer reads exactly those names.
  Renaming one side without the other breaks the page↔actor bridge silently.
- The actor registration key and import paths in `BrowserGlue.sys.mts`.

It is contained, but it touches a runtime contract (window globals) and an
actor registration, so it warrants a build + launch verification rather than a
blind rename.

### Approach

1. Rename both files: `NRExperimemmt{Child,Parent}.sys.mts` →
   `NRExperiment{Child,Parent}.sys.mts`.
2. Replace `Experimemmt` → `Experiment` in all 5 files: class names, method
   names, `defineAs` strings, the registration key, the `import` paths, the
   interface name, and the `globalThis.*` reads.
3. Grep the whole tree again for any residual `Experimemmt`. No matches were
   found in `.ts/.mts/.tsx/.json`, but `moz.build` / packaging files were not
   searched and should be checked.
4. Verify with `deno task feles-build stage`: confirm `NRExperimentChild
   created!` appears in the log and the experiments page in `about:settings`
   still functions.

### Risk

Low. Self-contained, no external API. The only trap is renaming one half of the
window-global contract; step 2 must change both sides together.

---

## 2. FP library consolidation (`@mobily/ts-belt` vs `fp-ts` / `io-ts`)

### Summary

CodeRabbit flagged that `browser-features/chrome` depends on **two**
functional-programming libraries: `fp-ts`/`io-ts` and `@mobily/ts-belt`.

### Current state

- `browser-features/chrome/deno.json` declares `@mobily/ts-belt`, `fp-ts`,
  `fp-ts/Either`, `fp-ts/function`, and `io-ts`.
- `@mobily/ts-belt` is used in exactly **one** file:
  `browser-features/chrome/lib/core/module-event-bus.ts`. It imports `R` and
  builds a small `Result` shim (`Result`, `ok`, `err`, `isOk`, `isError`,
  `mapResult`, …) on top of `R.Result`.
- That shim is the **only** public surface — `lib/core/mod.ts` re-exports it;
  downstream callers use the helpers, not `ts-belt` directly.
- `fp-ts`/`io-ts` is used in **19 files** (`designs/`, `panel-sidebar/`,
  `workspaces/`, `keyboard-shortcut/`, `mouse-gesture/`, `pwa/`,
  `pages-settings/types/`).

### Important constraint

`io-ts` and `ts-belt` are **not interchangeable**. `io-ts` is a runtime
*decoder/validator*; `ts-belt` is an FP *utility* library with no schema
system. "Standardise on ts-belt" is not possible. The repo's established
decoder replacement is **`valibot`** (already used by
`libs/shared/custom-shortcut-key/defines.ts`, declared in
`libs/shared/deno.json`).

### Two interpretations — pick one

**Option A — Surgical (resolves CodeRabbit's literal complaint)**

Migrate the single `ts-belt` file to `fp-ts`, so `chrome` depends on one FP
family (`fp-ts`/`io-ts`).

- Rewrite `module-event-bus.ts`'s internal `Result` shim to back onto `fp-ts`
  `Either` instead of `R.Result`. The exported shim API names stay identical.
- Remove `@mobily/ts-belt` from `browser-features/chrome/deno.json`.
- **Caveat:** the exported `Result` *type* changes shape (ts-belt tuple →
  fp-ts tagged union). Callers that only use the `isOk` / `mapResult` helpers
  are unaffected; any caller that destructures the raw value must be updated.
  `mod.ts` only re-exports the helpers, so the blast radius is small — but
  every consumer of `getModuleEventBus()` results must be audited.
- Scope: ~1 file rewrite + a caller audit. Low–medium effort.

**Option B — Project direction (larger)**

Migrate the 19 `io-ts` files to `valibot` and drop `fp-ts`/`io-ts` entirely,
matching the `custom-shortcut-key` migration already done on this branch.

- Each file's `io-ts` codecs (`t.type`, `t.array`, …) become `valibot` schemas
  (`v.object`, `v.array`, …); `isRight`/`fold` decode handling becomes
  `v.safeParse`.
- This is the repo's stated long-term direction (see `docs/noraneko-port.md`)
  but is a substantial, behaviour-sensitive refactor — runtime validation
  semantics differ subtly between `io-ts` and `valibot`.
- Scope: 19 files, each needing test-coverage review. High effort.

### Recommendation

Option A resolves the actual review comment with minimal risk. Option B is
worth doing eventually but is a separate, planned migration — it should not be
bundled into "address review feedback." Doing A now and tracking B in
`docs/noraneko-port.md` is the cleaner split.

### Risk

- Option A: low–medium — confined to one module, but the `Result` type-shape
  change needs a caller audit.
- Option B: medium–high — 19 files, validation-semantics differences; the
  existing `*.test.ts` decoder tests must be re-run.

---

## 3. `about:preferences` module-level singleton state (multi-tab safety)

### Summary

`bridge/startup/src/about-preferences.ts` (916 lines) keeps per-page state in
**module-level variables**. Because the module is evaluated once per JS
context, that state is shared across every `about:preferences` instance.

### Current state

Module-level mutable state:

| Line | Variable |
|---|---|
| 42 | `i18nextInstance` |
| 138 | `floorpHubWarningElements` |
| 139 | `warningI18nHandlersBound` |
| 140 | `localeObserverBound` |
| 141 | `i18nUtilsInstance` |
| 170 | `globalI18nInitializedObserverBound` |
| 694 | `floorpStartWarningElements` |

The boolean guard flags (`*Bound`) make initialisation idempotent for a single
page. But if a second `about:preferences` were opened, the guards are already
`true`, so the second page would never bind its own i18n handlers / locale
observer, and the element caches would point at the first page's DOM.

### Why deferred / real-world impact

Firefox de-duplicates `about:preferences` — opening it focuses the existing tab
instead of creating a second one. In normal use this code path is not reached,
so the practical impact is low. The concern is latent: it surfaces only if that
de-duplication is bypassed (e.g. a second browser window, or future code that
opens the page directly).

### Approach

Make the state per-document instead of per-module:

1. Collect the per-page state into a single `PageState` object.
2. Key it by `Document` via a module-level `WeakMap<Document, PageState>` (or
   hang it off a `window` property), and look it up at the start of each entry
   point.
3. Move the `addObserver` / `removeObserver` lifecycle onto that per-document
   state, cleaned up on the document's `pagehide`. The `pagehide` cleanup for
   the i18n observer added in `d2fa8a96` is the template.

### Risk

Medium. It is a structural change to a 916-line file that runs at browser
startup; a regression here affects the i18n warning banners on
`about:preferences`. It is mechanical (no behaviour change for the single-page
case) but broad, and must be verified with `deno task feles-build stage` plus
opening `about:preferences`.

### Recommendation

Lowest priority of the three — the bug is real but effectively unreachable
under Firefox's tab de-duplication. Worth doing for correctness, but only after
#1 and #2, and not under time pressure. If the redesign is judged not worth the
regression risk, an acceptable alternative is to leave the code and add a
comment documenting the single-instance assumption.

---

## Summary

| # | Item | Scope | Effort | Risk | Priority |
|---|---|---|---|---|---|
| 1 | `NRExperimemmt` → `NRExperiment` rename | 5 files | Low | Low | High (quick win) |
| 2 | FP library consolidation | A: ~1 file / B: 19 files | A: low–med / B: high | A: low–med / B: med–high | Option A: medium |
| 3 | `about:preferences` per-document state | 1 file (916 L) | Medium | Medium | Low |
