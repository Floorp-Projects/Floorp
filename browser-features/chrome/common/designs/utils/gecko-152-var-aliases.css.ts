/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * # Gecko 152 (Project Nova) CSS variable aliases — Floorp-wide
 *
 * ## Why this exists
 *
 * Firefox 152 ("Project Nova", 2026-06-16) renamed a large set of CSS custom
 * properties by dropping the legacy `bgcolor` / `textcolor` / `background`
 * suffixes in favor of the more regular `background-color` / `text-color`
 * forms, and folded the `arrowpanel-*` family into the `panel-*` family. Both
 * the definitions and the internal references in mozilla-central moved to the
 * new names.
 *
 * Floorp's own chrome components (statusbar, panel-sidebar, workspaces, PWA,
 * split-view, downloadbar, fluerial, command-palette, ui-custom options, ...)
 * still reference the pre-152 names in dozens of places — and the LWT runtime
 * sets `--panel-sidebar-background-color` via `var(--toolbar-bgcolor)` at
 * runtime. With the old names gone on 152, all of those resolve to nothing,
 * which surfaces as transparent / unstyled surfaces (e.g. the status bar
 * turning transparent — the symptom that prompted this file).
 *
 * ## Approach
 *
 * Re-expose every renamed token under its legacy name as an alias of the new
 * name. This is a single, Floorp-wide shim rather than touching 50+ call
 * sites, because:
 *   - the rename is mechanical (same semantics, different spelling),
 *   - it keeps the vendored Lepton CSS untouched (update_lepton.yml safe),
 *   - it lets Floorp migrate component by component over time.
 *
 * The aliases are declared WITHOUT !important and only set a value when the
 * legacy name is otherwise undefined, so any code that legitimately sets the
 * old name still wins. **Gecko < 152 is no longer supported**, so there is no
 * `@supports` / self-referential fallback here — every alias points directly
 * at the 152 name.
 *
 * ## Where the mapping comes from
 *
 * The 151 → 152 runtime diff (Floorp-Runtime PR #45), cross-checked against
 * the 152 source of every defining file:
 *   - browser/themes/shared/browser-colors.css   (toolbox-* , tab-background-*)
 *   - browser/themes/shared/tabbrowser/tab.tokens.css  (tab-* tokens)
 *   - browser/themes/{windows,osx,linux}/browser.css   (toolbar-bgcolor)
 *   - browser/themes/shared/toolbarbuttons.css  (toolbarbutton-*)
 *   - browser/themes/shared/browser-shared.css  (toolbar-start-end-padding)
 *   - browser/themes/shared/urlbar-searchbar.css (toolbar-field-focus-*)
 *   - toolkit/themes/shared/popup.css           (panel-* / arrowpanel-*)
 *
 * ## What is NOT a rename (deliberately absent from the table)
 *
 *   - `--toolbar-color` — still defined in 152 (global-shared.css). It is NOT
 *     `--toolbar-text-color`. Floorp/Lepton components that read
 *     `--toolbar-color` keep working unaided.
 *   - `--panel-text-color` — NEW in 152 (popup.css: `--panel-text-color:
 *     MenuText`). It is not the rename of any 151 token; it is the 152 form
 *     that the legacy `--arrowpanel-color` now aliases to.
 *
 * ## Injection point
 *
 * Applied to ALL designs via `browser-design-element.tsx`, not just the
 * Lepton family — because Floorp's own components (not only Lepton) read
 * these names. See Issue #2489.
 */

/**
 * Each entry maps `[legacyName, newName]` where, on Gecko 152, `legacyName`
 * was removed and `newName` carries the value. The emitted alias is
 * `legacyName: var(newName)` (no !important, no self-fallback — see module
 * doc). Custom themes / LWTs that set the *new* name therefore flow through,
 * and any code that still sets the *old* name wins by source order.
 */
export const GECKO_152_RENAMED_VARS: ReadonlyArray<
  readonly [string, string]
> = [
  // Toolbar / toolbox surface colors
  // browser/themes/{windows,osx,linux}/browser.css
  ["--toolbar-bgcolor", "--toolbar-background-color"],
  // browser/themes/shared/browser-colors.css:9-16
  ["--toolbox-bgcolor", "--toolbox-background-color"],
  ["--toolbox-textcolor", "--toolbox-text-color"],
  ["--toolbox-bgcolor-inactive", "--toolbox-background-color-inactive"],
  ["--toolbox-textcolor-inactive", "--toolbox-text-color-inactive"],

  // Tab tokens
  // browser-colors.css:70->75, tab.tokens.css:32->15
  ["--tab-selected-bgcolor", "--tab-background-color-selected"],
  // tab.tokens.css:18->14
  ["--tab-hover-background-color", "--tab-background-color-hover"],
  // tab.tokens.css:21,22,24 -> 83,79,74 (same values on :root). Vendored
  // Lepton still reads the legacy names: without these its container-tab
  // underline (`.tab-content::before`, userChrome.tab.container, on by
  // default) has an invalid width on the selected tab and is not drawn.
  // The first two are also set per element; see GECKO_152_TAB_SCOPED_VARS.
  ["--tab-inline-padding", "--tab-padding-inline"],
  ["--tab-inner-inline-margin", "--tab-margin-inline-inner"],
  ["--tab-loading-fill", "--tab-icon-fill-loading"],

  // Toolbar / toolbar field tokens that vendored Lepton still reads.
  // browser-shared.css:418 and toolbarbuttons.css:258 (start/end padding of
  // the toolbars) now read --toolbar-padding-inline; urlbar-searchbar.css:234
  // (focused field outline) now reads --toolbar-field-border-color-focus.
  ["--toolbar-start-end-padding", "--toolbar-padding-inline"],
  [
    "--toolbar-field-focus-border-color",
    "--toolbar-field-border-color-focus",
  ],

  // Toolbar buttons
  // toolbarbuttons.css:768-769 -> 775-776
  ["--toolbarbutton-hover-background", "--toolbarbutton-background-color-hover"],
  [
    "--toolbarbutton-active-background",
    "--toolbarbutton-background-color-active",
  ],
  // toolbarbuttons.css:13,17 — the padding tokens were renamed too. Vendored
  // Lepton still reads the legacy names inside `calc()` chains (e.g. its
  // `--uc-toolbarbutton-hide-size`), so without these aliases those `calc()`
  // expressions are invalid and the autohide back/forward button rules
  // silently do nothing.
  ["--toolbarbutton-outer-padding", "--toolbarbutton-padding-outer"],
  ["--toolbarbutton-inner-padding", "--toolbarbutton-padding-inner"],

  // Panels / arrow panels (arrowpanel-* folded into panel-* in 152)
  // toolkit/themes/shared/popup.css:10 -> --panel-background-color
  ["--arrowpanel-background", "--panel-background-color"],
  ["--arrowpanel-color", "--panel-text-color"],
  ["--arrowpanel-border-color", "--panel-border-color"],
  // The short `--panel-background` (no -color suffix) was also renamed.
  ["--panel-background", "--panel-background-color"],
];

/**
 * Tokens that are referenced on 152 but NOT defined by it, so they must be
 * synthesized by Floorp. Each entry is `[name, chain]` and the alias is
 * emitted as `name: chain` without !important.
 *
 * `--toolbar-text-color` is the important one: `tab.tokens.css` on 152 reads
 * `--tab-selected-textcolor: var(--toolbar-text-color)`, but Mozilla defines
 * `--toolbar-text-color` nowhere — it is expected to come from the active
 * theme / LWT / Lepton. On a plain build it is therefore empty and selected
 * tab text goes unstyled. Alias it to `--toolbar-color` (the 152-surviving
 * token that actually carries the toolbar text color), letting LWTs that set
 * `--lwt-text-color` win first.
 */
export const GECKO_152_SYNTHESIZED_VARS: ReadonlyArray<
  readonly [string, string]
> = [
  // Prefer an LWT-provided text color, then the surviving toolbar token.
  ["--toolbar-text-color", "var(--lwt-text-color, var(--toolbar-color))"],
];

/**
 * Legacy names from `GECKO_152_RENAMED_VARS` that are re-declared on
 * `.tabbrowser-tab` as well as on `:root`.
 *
 * A custom property that references another one is resolved on the element
 * that declares it, so the `:root` alias carries the root value of the new
 * name into every tab. Gecko sets these two per element: tabs.css (159)
 * `#tabbrowser-tabs[orient="vertical"]` (2878) and
 * `#pinned-tabs-container[orient="vertical"]` (2782) set
 * `--tab-padding-inline`, and the compact `tab-split-view-wrapper` (1551) sets
 * `--tab-margin-inline-inner: 13px` (151 set `--tab-inline-padding` on the
 * same vertical-tab elements, tabs.css 2303 / 2220). Declaring the alias on
 * the tab resolves it against the value the tab inherits there. Gecko sets
 * `--tab-icon-fill-loading` only on `:root` (tab.tokens.css,
 * ThemeVariableMap), so `--tab-loading-fill` stays root-only.
 */
export const GECKO_152_TAB_SCOPED_VARS: ReadonlyArray<string> = [
  "--tab-inline-padding",
  "--tab-inner-inline-margin",
];

function buildTabScopedDeclarations(): string {
  return GECKO_152_TAB_SCOPED_VARS.map((legacy) => {
    const entry = GECKO_152_RENAMED_VARS.find(([name]) => name === legacy);
    if (!entry) throw new Error(`${legacy} is not in GECKO_152_RENAMED_VARS`);
    return `  ${legacy}: var(${entry[1]});`;
  }).join("\n");
}

function buildAliasDeclarations(): string {
  const renamed = GECKO_152_RENAMED_VARS.map(([legacy, renamed]) =>
    `  ${legacy}: var(${renamed});`
  );
  const synthesized = GECKO_152_SYNTHESIZED_VARS.map(([name, chain]) =>
    `  ${name}: ${chain};`
  );
  return [...renamed, ...synthesized].join("\n");
}

/**
 * The variable alias stylesheet. Injected for every Floorp design so all
 * Floorp chrome components (and the vendored Lepton CSS) keep resolving the
 * pre-152 variable names.
 */
export const GECKO_152_VAR_ALIASES_CSS = `
/* =========================================================================
 * Floorp Gecko 152 variable aliases (Floorp-wide)
 * Re-exposes pre-152 CSS custom property names as aliases of the 152 names.
 * See utils/gecko-152-var-aliases.css.ts.
 * ======================================================================= */
:root {
${buildAliasDeclarations()}
}
.tabbrowser-tab {
${buildTabScopedDeclarations()}
}
`;
