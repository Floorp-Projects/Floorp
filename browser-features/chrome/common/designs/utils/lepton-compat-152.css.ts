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
 *   2. `LEPTON_COMPAT_152_CSS` — Lepton-only tab/toolbox variable aliases,
 *      palette restoration, and native-sidebar positioning compatibility.
 *      Applied only to the `lepton` / `photon` / `protonfix` designs.
 *   3. `FLOORP_ICON_PATCHES` — Floorp-only icon rules extracted from the
 *      vendored leptonChrome.css so the daily upstream sync cannot delete
 *      them. Lepton-family only (the IDs are Lepton-scoped).
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
  :root:not([lwtheme]):not(:-moz-lwtheme),
  :root:not([lwtheme]):not(:-moz-lwtheme) dialog {
    --in-content-page-background: rgb(255, 255, 255);
  }
}
@media (prefers-color-scheme: dark) {
  :root:not([lwtheme]):not(:-moz-lwtheme),
  :root:not([lwtheme]):not(:-moz-lwtheme) dialog {
    --in-content-page-background: rgb(31, 30, 38);
  }
}
/* An LWT-provided accent wins for in-content surfaces — but ONLY when the
 * theme actually sets one. var(..., <keep>) via @property is not available
 * cross-version, so guard by scoping to the LWT selector and reading the
 * accent directly. No !important: this is equal-specificity source-order. */
:root:is(:-moz-lwtheme, [lwtheme]),
:root:is(:-moz-lwtheme, [lwtheme]) dialog {
  --in-content-page-background: var(--lwt-accent-color);
}

/*= Panel background default (the "transparent panel" symptom) ==============
 * Only fill in a default for the no-theme case. A loaded LWT or a custom
 * theme that sets --arrowpanel-background / --panel-background-color must
 * keep its value (the alias in gecko-152-var-aliases already chains them). */
:root:not([lwtheme]):not(:-moz-lwtheme) {
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
 * The selectors below deliberately rely on the stable `:-moz-lwtheme` /
 * `[lwtheme]` / `prefers-color-scheme` signals rather than hardcoded RGB
 * triples or attributes that Mozilla renames between versions. And, unlike
 * the previous revision, they do NOT clobber values a loaded LWT provides.
 */
export const LEPTON_COMPAT_152_CSS = `
/* =========================================================================
 * Floorp Lepton compat — Gecko 152 (Project Nova)
 * Loaded AFTER leptonChrome.css; equal specificity wins by source order.
 * ======================================================================= */

/*= Built-in light/dark theme palette restoration ==========================
 * Lepton keys off the built-in-theme attribute and inline-style exact-RGB
 * substring matchers on :root. On Gecko 152 those signals are unreliable,
 * so Lepton's !important overrides never apply and the chrome falls back to
 * raw defaults. Re-establish Lepton's intended palette using the stable
 * prefers-color-scheme signal instead — but ONLY for the no-theme case, so a
 * loaded LWT is never overridden. */
:root {
  /* Lepton "Original" intended accent colors (mirrors leptonChrome.css) */
  --lepton-compat-accent-light: rgb(229, 229, 235);
  --lepton-compat-toolbar-light: rgba(255, 255, 255, 1);
  --lepton-compat-accent-dark: rgb(28, 27, 34);
  --lepton-compat-toolbar-dark: rgba(43, 42, 51, 1);
}

/* Built-in theme, no LWT loaded: restore Lepton's light palette.
 * Chain both negations on :root (AND) so the rule only applies when NO theme
 * signal is present. */
:root:not([lwtheme]):not(:-moz-lwtheme) {
  --lwt-accent-color: var(--lepton-compat-accent-light);
  --toolbar-bgcolor: var(--lepton-compat-toolbar-light);
}
@media (prefers-color-scheme: dark) {
  :root:not([lwtheme]):not(:-moz-lwtheme) {
    --lwt-accent-color: var(--lepton-compat-accent-dark);
    --toolbar-bgcolor: var(--lepton-compat-toolbar-dark);
  }
}

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
 * token alias above is enough for any Lepton rule that reads the accent. */

/*= Primary button accent (Lepton blue) =====================================
 * Lepton declares these inside the built-in-theme block that no longer
 * matches on 152. Re-declare them for the no-theme case so primary buttons
 * keep their blue rather than inheriting a broken accent. An LWT that sets
 * its own button tokens wins by the alias layer + source order. */
:root:not([lwtheme]):not(:-moz-lwtheme),
:root:not([lwtheme]):not(:-moz-lwtheme):host,
:root:not([lwtheme]):not(:-moz-lwtheme) dialog {
  --in-content-primary-button-text-color: var(--in-content-page-color);
  --in-content-primary-button-background: var(--blue-60, #0060df);
  --in-content-primary-button-background-hover: var(--blue-50, #0a84ff);
  --in-content-primary-button-background-active: var(--blue-40, #4595ff);
}

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
 * Floorp-specific icon rules extracted from
 * `skin/lepton/css/leptonChrome.css` lines 14596–14672.
 *
 * Upstream Lepton does not know about these Floorp-only element IDs
 * (PWA/SSB, UserCSSLoader, webpanel, share mode, etc.). They were living
 * inside the vendored file, which means the daily `update_lepton.yml`
 * workflow would silently delete them whenever upstream restructured that
 * region. Hosting them here makes them independent of upstream syncs.
 *
 * The rules are duplicated verbatim from the vendored source (same `url()`
 * references resolve identically because this sheet is injected into the
 * same document and the relative `../icons` path is rewritten by
 * `replaceIconPaths()` at registration time). Duplicating intentionally
 * rather than deleting the vendored copy: the vendored copy disappears on
 * the next upstream sync, at which point this becomes the single source.
 */
export const FLOORP_ICON_PATCHES = `
/*= Floorp Browser (icon patches, extracted from leptonChrome.css) ==========*/
#ssbPageAction-image {
  list-style-image: url("../icons/pwa-install.svg");
}
#ssbPageAction-image[open-ssb="true"] {
  list-style-image: url("../icons/pwa-launch.svg");
}
@media -moz-pref("userChrome.icon.panel") {
  #rebootappmenu {
    list-style-image: url("../icons/refresh-cw.svg");
  }
  #openprofiledir {
    list-style-image: var(--uc-folder-icon);
  }
  #appMenu-ssb-button {
    list-style-image: url("../icons/pwa-manage.svg");
  }
  #appMenu-install-or-open-ssb-current-page-button {
    list-style-image: url("../icons/pwa-install.svg");
  }
  #appMenu-install-or-open-ssb-current-page-button[open-ssb="true"] {
    list-style-image: url("../icons/pwa-launch.svg");
  }
}
@media -moz-pref("userChrome.icon.menu") {
  #toggle_sharemode {
    --menuitem-image: url("chrome://branding/content/about-logo-private.png");
  }
  #usercssloader-menu {
    --menuitem-image: url("../icons/developer.svg");
  }
  #usercssloader-menupopup > menu[data-l10n-id="css-menu"] {
    --menuitem-image: url("../icons/document-css.svg");
  }
  #usercssloader-submenupopup > menuitem[data-l10n-id="rebuild-css"] {
    --menuitem-image: url("chrome://global/skin/icons/reload.svg");
  }
  #usercssloader-submenupopup > menuitem[data-l10n-id="make-browsercss-file"] {
    --menuitem-image: url("../icons/edit-active.svg");
  }
  #usercssloader-submenupopup > menuitem[data-l10n-id="open-css-folder"] {
    --menuitem-image: var(--uc-folder-icon);
  }
  #usercssloader-submenupopup > menuitem[data-l10n-id="edit-userChromeCss-editor"] {
    --menuitem-image: url("chrome://browser/skin/window.svg");
  }
  #usercssloader-submenupopup > menuitem[data-l10n-id="edit-userContentCss-editor"] {
    --menuitem-image: url("chrome://global/skin/icons/page-portrait.svg");
  }
  #context_toggleToPrivateContainer,
  #open_in_private_container {
    --menuitem-image: url("../icons/private-favicon.svg");
  }
  #toggle_statusBar {
    --menuitem-image: url("../icons/pulse-square.svg");
  }
  #muteMenu {
    --menuitem-image: url("chrome://browser/skin/tabbrowser/tab-audio-muted-small.svg");
    stroke: transparent !important;
  }
  #unloadWebpanelMenu {
    --menuitem-image: var(--uc-tab-unload-icon);
  }
  #changeUAWebpanelMenu {
    --menuitem-image: url("../icons/command-responsivemode.svg");
    fill-opacity: 0;
  }
  #deleteWebpanelMenu {
    --menuitem-image: url("chrome://global/skin/icons/delete.svg");
  }
  #run-ssb-contextmenu {
    --menuitem-image: url("../icons/pwa-launch.svg");
  }
  #uninstall-ssb-contextmenu {
    --menuitem-image: url("../icons/pwa-remove.svg");
  }
}
`;

/**
 * Combined Lepton-family stylesheet: color fix + Lepton-specific compat +
 * Floorp icon patches. Injected after Lepton's own sheets.
 */
export const LEPTON_COMPAT_CSS =
  GECKO_152_COLOR_FIX_CSS +
  "\n" +
  LEPTON_COMPAT_152_CSS +
  "\n" +
  FLOORP_ICON_PATCHES;
