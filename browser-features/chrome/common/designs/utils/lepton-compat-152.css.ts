/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * # Gecko 152 (Project Nova) compatibility layers
 *
 * Gecko 152 renamed a large set of chrome CSS variables (see
 * `gecko-152-var-aliases.css.ts`) AND made the built-in-theme signals that
 * user style sheets historically relied on unreliable. The alias file covers
 * the renames; this file covers the rest:
 *
 *   1. `GECKO_152_COLOR_FIX_CSS` — theme/LWT color stabilization. Applied to
 *      EVERY design (including `fluerial`), because dialogs going black and
 *      the chrome collapsing to a single color are not Lepton-specific
 *      symptoms — they hit any chrome component that reads `--lwt-accent-color`
 *      / `--in-content-page-background` / `--arrowpanel-background` once the
 *      built-in-theme block stops matching.
 *   2. `LEPTON_COMPAT_152_CSS` — Lepton-only LWT handling and
 *      native-sidebar positioning compatibility.
 *      Applied only to the `lepton` / `photon` / `protonfix` designs.
 *
 * The Floorp-only icon rules that used to live here as `FLOORP_ICON_PATCHES`
 * moved to the chrome-extras stylesheets
 * (`ui-custom/styles/css/options/chrome-extras/{icon-disabled,icon-menu}.css`),
 * which are design-agnostic and driven by the `uiCustomization.chromeExtras`
 * settings instead of Lepton prefs. See `designs/chrome-extras.ts`.
 *
 * ## Design rule: never clobber a theme-provided value
 *
 * The previous version of this file force-overrode `--lwt-accent-color`,
 * `--toolbar-bgcolor` and `--arrowpanel-background` with `!important` under
 * broad selectors. That is what broke third-party LWTs ("Windows XP modern"
 * etc.): the theme author's own palette was discarded and the whole chrome
 * collapsed to one color. Every rule below now either:
 *   - sets a value ONLY when none is provided (a guarded alias, no
 *     `!important`), or
 *   - puts an LWT fallback in a low-priority cascade layer so a theme-provided
 *     value wins, or
 *   - narrows the selector to the no-theme case (`:root:not([lwtheme])`)
 *     so a loaded LWT is never touched.
 *
 * Reference: https://github.com/Floorp-Projects/Floorp/issues/2489
 */

/**
 * Theme/LWT color stabilization — applied to every design.
 *
 * What this fixes:
 *  - **Black dialogs.** Lepton and several Floorp components derive
 *    `--in-content-page-background` from `--lwt-accent-color`. On 152 the
 *    accent can resolve to nothing in-content, so dialogs paint black. Anchor
 *    in-content surfaces to a stable per-scheme value, and let an LWT that
 *    actually provides an accent override it.
 *  - **Transparent panels.** `--arrowpanel-background` (the legacy name the
 *    components still read) is now an alias of `--panel-background-color`,
 *    but when neither is set the panel goes transparent. Provide a safe
 *    default for the no-theme case only.
 *
 * What this deliberately does NOT do:
 *  - override `--lwt-accent-color` / `--toolbar-bgcolor` globally. Those are
 *    the LWT author's own palette and must flow through untouched. The
 *    broad `!important` overrides that used to live here were the root cause
 *    of custom-theme breakage.
 */
export const GECKO_152_COLOR_FIX_CSS = `
/* =========================================================================
 * Floorp Gecko 152 color fix (applied to every design)
 * See utils/lepton-compat-152.css.ts.
 * ======================================================================= */

/*= Dialog background (the "black dialog" symptom) ==========================
 * Anchor in-content surfaces to a stable per-scheme value. Only the no-theme
 * case is pinned; a loaded LWT may still override via its own accent. */
@media (prefers-color-scheme: light) {
  :root:not([lwtheme]),
  :root:not([lwtheme]) dialog {
    --in-content-page-background: rgb(255, 255, 255);
  }
}
@media (prefers-color-scheme: dark) {
  :root:not([lwtheme]),
  :root:not([lwtheme]) dialog {
    --in-content-page-background: rgb(31, 30, 38);
  }
}
/* LWT in-content surface — use a safe, content-appropriate token.
 * The previous revision read --lwt-accent-color directly, which is the
 * *frame* color (XP Modern: rgb(9,96,224) blue). Painting that onto
 * in-content surfaces made Customize Floorp solid blue (#2503) and, when the
 * accent was missing/transparent, left dialogs black (#2403/#2502).
 * Prefer a content canvas / toolbar value instead. The fallback is in a
 * low-priority layer so a theme rule that legitimately sets its own
 * --in-content-page-background still wins, even though this sheet is loaded
 * after the theme's sheets. */
/* Capture the effective root value unlayered so a theme-provided value is
 * carried onto dialogs; the dialog override below then references it. */
:root:is(:-moz-lwtheme, [lwtheme]) {
  --floorp-lwt-in-content-page-background: var(--in-content-page-background);
}
@layer floorp-compat {
  :root:is(:-moz-lwtheme, [lwtheme]) dialog {
    --in-content-page-background: var(
      --floorp-lwt-in-content-page-background,
      var(--background-color-canvas, var(--toolbar-background-color, Canvas))
    );
  }
}

/*= Panel background default (the "transparent panel" symptom) ==============
 * Only fill in a default for the no-theme case. A loaded LWT or a custom
 * theme that sets --arrowpanel-background / --panel-background-color must
 * keep its value (the alias in gecko-152-var-aliases already chains them). */
:root:not([lwtheme]) {
  --arrowpanel-background: var(--panel-background-color, -moz-dialog);
  --arrowpanel-color: var(--panel-text-color, -moz-dialogtext);
  --arrowpanel-border-color: var(--panel-border-color, rgba(0, 0, 0, 0.1));
}
`;

/**
 * Lepton-specific compatibility — applied only to `lepton` / `photon` /
 * `protonfix` (they share the vendored Lepton CSS).
 *
 * Why this exists (Floorp Issue #2489): the vendored Lepton CSS
 * (`skin/lepton/css/leptonChrome.css`) detects built-in light/dark themes
 * through three brittle mechanisms that Gecko 152 no longer populates
 * reliably:
 *
 *   1. `[lwtheme-mozlightdark]` attribute
 *   2. `[builtintheme][devtoolstheme="..."]`
 *   3. inline `[style*="--lwt-accent-color: rgb(240, 240, 244); …"]`
 *      exact-string RGB matching
 *
 * When the detection misses, Lepton's `!important` color overrides on
 * `--lwt-accent-color`, `--toolbar-bgcolor`, `--arrowpanel-background`, etc.
 * never apply. `fluerial` and the built-in `proton` design are unaffected
 * because they do not use Lepton CSS.
 *
 * We do **not** patch the vendored Lepton files: the `update_lepton.yml`
 * workflow re-syncs them from upstream daily and would clobber any local
 * edit. Instead this is injected AFTER Lepton's own sheets, so
 * equal-specificity rules here win by source order. This keeps the upstream
 * sync path clean and lets us adapt to Gecko 153+ by editing only this file.
 *
 * The selectors below deliberately rely on the stable `[lwtheme]`
 * signal rather than hardcoded RGB
 * triples or attributes that Mozilla renames between versions. And, unlike
 * the previous revision, they do NOT clobber values a loaded LWT provides.
 */
export const LEPTON_COMPAT_152_CSS = `
/* =========================================================================
 * Floorp Lepton compat — Gecko 152 (Project Nova)
 * Loaded AFTER leptonChrome.css; equal specificity wins by source order.
 * ======================================================================= */

/* No built-in light/dark palette override here. An earlier no-theme rule
 * set only the legacy --lwt-accent-color / --toolbar-bgcolor tokens. Gecko
 * 159 paints the selected tab, #browser and (for protonfix) #nav-bar from
 * --toolbar-background-color, which follows the GTK theme on Linux, so
 * overriding only the legacy token split the bars (e.g. protonfix dark:
 * nav-bar rgb(68,67,75) vs panel sidebar rgb(43,42,51)). On Windows,
 * Lepton's own !important system colors win anyway. The rule's
 * selector was invalid on 159, so it never applied; it was removed rather
 * than revived. */

/*= Third-party LWT support (Issue #2489 — "Windows XP modern" etc.) ========
 * Lepton writes fixed-color !important overrides keyed to built-in-theme
 * detection. When an LWT is active that detection can partially match and
 * paint the whole chrome a single accent color.
 *
 * Let the LWT's own values flow by giving the chrome surface a guarded
 * alias: read the LWT accent first, and only fall back to the toolbar color
 * when the theme did not provide one. No !important — the LWT author wins. */
:root:is(:-moz-lwtheme, [lwtheme]) {
  --lwt-accent-color: var(--lwt-accent-color, var(--toolbar-background-color));
  --toolbar-bgcolor: var(--toolbar-background-color);
}
/* The navigator toolbox background follows --lwt-accent-color in Lepton; make
 * sure it reads the live (possibly LWT-provided) value.
 *
 * IMPORTANT: only pin --lwt-accent-color as a TOKEN here. Do NOT set
 * background-color on #navigator-toolbox under LWT. Firefox draws a
 * Lightweight Theme's additional_backgrounds on <body> and relies on
 * #navigator-toolbox (and its translucent .browser-toolbar children) being
 * transparent so that artwork shows through (see browser-shared.css). Giving
 * #navigator-toolbox an opaque accent fill hides the entire theme background,
 * which is the bug we are fixing. The toolbox itself stays transparent; the
 * token alias above is enough for any Lepton rule that reads the accent.
 * The one exception is the overlapping F11 toolbox below. */

/*= LWT toolbox in overlapping F11 fullscreen (Issue #2759 / #2419) =========
 * Lepton's userChrome.fullscreen.overlap (on by default in the Lepton,
 * Photon and ProtonFix user.js) makes the F11 toolbox position: fixed above
 * the web content. Under an LWT the toolbox is transparent and most of the
 * theme is painted on <body> behind it, so once the toolbox is lifted over
 * the content the page shows through the tab strip and nav bar. Lepton only
 * gives the toolbox an opaque fill for the no-theme case. DOM (video)
 * fullscreen hides the toolbox and is excluded, the same way Lepton excludes
 * it from the overlap rules.
 *
 * Outside fullscreen the toolbox area shows two layers, and both are
 * restored here:
 *
 *   1. <body>'s background: the frame color and, under Lepton's !important
 *      body overrides, the header image (background-size: contain on wide
 *      windows). Repaint it on the toolbox with background: inherit. Fixed
 *      attachment keeps it positioned and sized against the window, the same
 *      box <body> uses.
 *   2. The toolbox's own images, for themes where LightweightThemeConsumer
 *      sets [theme-image-in-toolbox] (backgrounds_area: top_toolbars, or a
 *      center/bottom additional_backgrounds_alignment). Gecko positions
 *      those against the toolbox (browser-shared.css), and layer 1 replaces
 *      the toolbox background, so draw them again on a ::before that covers
 *      the toolbox, below its toolbars. They stay attached to the toolbox. */
@media -moz-pref("userChrome.fullscreen.overlap") {
  @media -moz-pref("browser.fullscreen.autohide") {
    :root:is(:-moz-lwtheme, [lwtheme])[sizemode="fullscreen"]:not([inDOMFullscreen="true"]) > body > #navigator-toolbox {
      background: inherit;
      background-attachment: fixed;
    }
    :root:is(:-moz-lwtheme, [lwtheme])[theme-image-in-toolbox][sizemode="fullscreen"]:not([inDOMFullscreen="true"]) > body > #navigator-toolbox::before {
      content: "";
      position: absolute;
      inset: 0;
      z-index: -1;
      pointer-events: none;
      background-image: var(--toolbox-background-image);
      background-repeat: var(--toolbox-background-repeat);
      background-position: var(--toolbox-background-position);
      background-size: var(--toolbox-background-size);
    }
  }
}

/* Customize Floorp is a content surface, not the theme frame. Gecko paints
 * LWT header images on the chrome document body, while Lepton leaves the
 * customization container transparent. With a theme such as Windows XP
 * Modern that exposes a blue frame accent, that transparency lets the frame
 * color fill the entire customization page. Keep the toolbox transparent so
 * the theme still paints its tabs and toolbar, but give the customization
 * surface the theme's content-safe toolbar/panel color. */
:root:is(:-moz-lwtheme, [lwtheme])[customizing] #customization-container {
  background-color: var(
    --toolbar-background-color,
    var(--panel-background-color, -moz-dialog)
  ) !important;
  background-image: none !important;
}

/* No primary-button override here. Lepton's own
 * userChrome.theme.proton_color block already sets the
 * --in-content-primary-button-* tokens per color scheme on Gecko 159. An
 * earlier no-theme override forced light-mode blue in dark mode and dark text
 * on blue in light mode; its selector was invalid on 159, so it never
 * applied, and it was removed rather than revived. */

/*= Right-positioned native sidebar (Issue #2556) ============================
 * Gecko 152 exposes the native sidebar's end position on #sidebar-box as the
 * presence-only [sidebar-positionend] attribute. Vendored Lepton still keys
 * its overlap layout's direction flip to the former [positionend] attribute,
 * so its inline-start offset is applied on the wrong side and pushes a right
 * sidebar partly outside the window.
 *
 * Keep the legacy attribute in the selector for older Gecko versions. These
 * are boolean presence attributes, so intentionally do not match a value. */
@media -moz-pref("userChrome.sidebar.overlap") {
  #sidebar-box:is([positionend], [sidebar-positionend]) {
    direction: rtl;
  }
}
`;

/**
 * Combined Lepton-family stylesheet: color fix + Lepton-specific compat.
 * Injected after Lepton's own sheets.
 *
 * The Floorp icon patches that used to be appended here now live in the
 * design-agnostic chrome-extras stylesheets.
 */
export const LEPTON_CONTAINER_INDICATOR_CSS = `
/* Photon uses selected="true", but current Gecko represents selection with
 * selected="". Its inactive animation therefore hides selected containers,
 * while its identity-color reset also replaces their individual colors.
 * A container's identity must remain visible independently of selection/hover.
 * Keep this after the vendored sheet, which is replaced by upstream syncs. */
@media -moz-pref("userChrome.tab.photon_like_contextline") {
  #tabbrowser-tabs .tabbrowser-tab[usercontextid] > .tab-stack > .tab-background > .tab-context-line {
    --identity-icon-color: inherit;
    background-color: var(--identity-icon-color) !important;
    opacity: 1 !important;
    transform: none !important;
  }
  /* Photon forces a 2px height for horizontal tabs. Restore the native
   * vertical indicator's height when the tab strip changes orientation. */
  #tabbrowser-tabs[orient="vertical"] .tabbrowser-tab[usercontextid] > .tab-stack > .tab-background > .tab-context-line {
    height: 24px !important;
    align-self: center;
  }
}
`;

export const LEPTON_COMPAT_CSS = GECKO_152_COLOR_FIX_CSS + "\n" +
  LEPTON_COMPAT_152_CSS + "\n" +
  LEPTON_CONTAINER_INDICATOR_CSS;
