// SPDX-License-Identifier: MPL-2.0
//
// Baseline for css-variables.test.ts, taken on Floorp 12.21 / Gecko 159.0.1
// (Runtime daily-1179). One entry per custom property with a one-line
// reason. The test fails only on names that are not listed here, and warns
// about entries that are no longer needed; delete an entry when it is fixed.
//
// Entries starting with "Lepton bug" are real breakage in vendored Lepton
// that Floorp does not fix (no Gecko 152 rename to alias, the rule is opt-in
// or the fix would change Lepton's look); they are for upstream. Gecko 152
// renames that Lepton still reads are aliased in
// utils/gecko-152-var-aliases.css.ts instead of being listed here.

// Shared reasons (used by several entries).
const PRE_151_RENAME =
  "Lepton bug: renamed by Gecko before 151, Lepton never followed";
const GONE_GECKO =
  "Gecko token Lepton overrides; Gecko 159 no longer reads it (removed)";
const GONE_PAGE =
  "old in-content page palette Lepton overrides; no Gecko 159 page reads it";
const PHOTON_COLOR =
  "Photon-era palette color Lepton declares; nothing reads it on Gecko 159";
const USER_HOOK =
  "Lepton user-customization hook, read with a fallback and meant to be set by users";
const PWA_IDENTITY =
  "PWA container indicator reads Gecko's pre-152 --identity-color-*; the hard-coded fallback color applies";

/** Read with var() by Floorp/Lepton CSS, defined by nothing. */
export const ALLOWED_UNDEFINED_READS: Record<string, string> = {
  // --- Lepton (vendored) -------------------------------------------------
  "--urlbar-box-hover-text-color":
    `${PRE_151_RENAME} (--urlbar-box-text-color-hover); opt-in combined.urlbar.reload_button only`,
  "--urlbarView-item-inline-padding":
    "Lepton bug: urlbar view padding calc() is invalid; opt-in urlView.as_commandbar / full_width_padding only",
  "--urlbar-min-height":
    "Gecko 152 renamed it to --urlbar-height; no visible effect: #urlbar stays 32px tall either way (measured)",
  "--urlbar-width":
    "Lepton bug: Gecko no longer sets --urlbar-width; opt-in urlView_expanding / as_commandbar only",
  "--arrowpanel-menuitemblank-padding-block":
    "Lepton bug: typo for --arrowpanel-menuitemblank-padding; off by default (needs icon.panel_full and panel_photon off). Floorp's icon-disabled.css copy is removed (guarded in css-variables.test.ts)",
  "--urlbar-container-padding":
    "removed from Gecko; 2 of 4 Lepton reads fall back to 0px",
  "--tab-shadow-max-size":
    "removed from Gecko; Lepton's negative margin calc() is invalid",
  "--checkbox-checked-border-color-hover":
    "removed from Gecko; every Lepton read has a fallback",
  "--lwt-tab-loading-fill-inactive":
    "never defined by Gecko; Lepton reads it with a fallback",
  "--uc-light-bkgnd-color": USER_HOOK,
  "--uc-newTab-wallpaper": USER_HOOK,
  "--newtab-icon-secondary-color":
    "removed from the Gecko 159 new tab page; Lepton content CSS still reads it",
  "--color-marketing-gray-10":
    "old about: page palette color; no Gecko 159 page defines it",
  "--in-content-primary-button-border-color":
    "no longer defined by Gecko; no visible effect: Lepton's about:support #reset-box-button rule does not apply on 159 (measured)",
  "--in-content-primary-button-border-hover":
    "no longer defined by Gecko; same about:support rule as --in-content-primary-button-border-color",
  "--in-content-primary-button-border-active":
    "no longer defined by Gecko; same about:support rule as --in-content-primary-button-border-color",
  "--in-content-primary-button-text-color-active":
    "no longer defined by Gecko; Lepton's active primary button text color is empty",

  // --- Floorp chrome-extras (extracted from Lepton) ------------------------
  "--toolbarbutton-icon-fill-opacity":
    "removed from Gecko; without a fallback fill-opacity falls back to its initial 1 (harmless)",
  "--zoom-controls-bgcolor": "removed from Gecko; every read has a fallback",
  "--panel-separator-margin":
    "Gecko 159 itself reads it without defining it; Lepton follows",
  "--uc-oneliner-urlbar-base-width": USER_HOOK,

  // --- Floorp features ---------------------------------------------------
  "--toolbarbutton-border-color":
    "PWA window style reads a token Gecko never defined; the border color is empty",
  "--identity-color-blue": PWA_IDENTITY,
  "--identity-color-turquoise": PWA_IDENTITY,
  "--identity-color-green": PWA_IDENTITY,
  "--identity-color-yellow": PWA_IDENTITY,
  "--identity-color-orange": PWA_IDENTITY,
  "--identity-color-red": PWA_IDENTITY,
  "--identity-color-pink": PWA_IDENTITY,
  "--identity-color-purple": PWA_IDENTITY,
  "--identity-color-toolbar": PWA_IDENTITY,
  "--identity-color-gray": PWA_IDENTITY,
  "--bookmark-bar-height":
    "bookmarkbar_autohide option reads a token nothing sets (no fallback)",
  "--toolbar-bgimage":
    "removed from Gecko; bookmarkbar_focus_expand reads it with a fallback",
  "--ease-basic":
    "fluerial reads an easing token nothing defines (no fallback)",
  "--tab-transition-duration":
    "fluerial reads a duration token nothing defines (no fallback)",
  "--tab-opacity": "fluerial reads a token nothing defines (no fallback)",
};

/** Defined by Floorp/Lepton CSS, read by nothing (Gecko or Floorp). */
export const ALLOWED_UNREAD_DEFS: Record<string, string> = {
  // --- Lepton overrides of renamed Gecko tokens (the #3093 class) ----------
  "--urlbar-box-focus-bgcolor":
    `${PRE_151_RENAME} (--urlbar-box-background-color-focus); only the focus state of urlbar boxes`,
  "--urlbar-box-active-bgcolor":
    `${PRE_151_RENAME} (--urlbar-box-background-color-active); only the pressed state of urlbar boxes`,
  "--tab-selected-color":
    "Lepton bug: not a Gecko token (151 has none); Lepton puts a background color in it (protonfix, Windows)",
  "--panel-color":
    "Gecko 152 renamed it to --panel-text-color; no visible effect: menus use MenuText rgb(251,251,254) / rgb(21,20,26) instead of Lepton's #fff / #000 (measured)",
  "--arrowpanel-menuitem-padding":
    `${PRE_151_RENAME} (--panel-menuitem-padding); padding.panel's 5px menu items are 8px`,
  "--arrowpanel-padding":
    `${PRE_151_RENAME} (--panel-padding, now a two-value shorthand set per panel)`,

  // --- Lepton overrides of Gecko tokens that are gone ----------------------
  "--lwt-tabs-border-color": GONE_GECKO,
  "--button-primary-color": GONE_GECKO,
  "--win-sidebar-hover-bgcolor": GONE_GECKO,
  "--win-red-border-color": GONE_GECKO,
  "--input-color": GONE_GECKO,
  "--toolbar-non-lwt-textcolor": GONE_GECKO,
  "--urlbar-popup-action-color": GONE_GECKO,
  "--urlbar-popup-url-color": GONE_GECKO,
  "--autocomplete-popup-background": GONE_GECKO,
  "--autocomplete-popup-color": GONE_GECKO,
  "--autocomplete-popup-highlight-background": GONE_GECKO,
  "--autocomplete-popup-highlight-color": GONE_GECKO,
  "--autocomplete-popup-hover-background": GONE_GECKO,
  "--panel-disabled-color": GONE_GECKO,
  "--panel-shortcut-color": GONE_GECKO,
  "--panel-description-color": GONE_GECKO,
  "--panel-separator-margin-horizontal": GONE_GECKO,
  "--menuitem-disabled-hover-background-color": GONE_GECKO,
  "--error-text-color": GONE_GECKO,
  "--input-error-border-color": GONE_GECKO,
  "--arrowpanel-dimmed-even-further": GONE_GECKO,
  "--panelview-toolbarbutton-focus-box-shadow": GONE_GECKO,
  "--lwt-toolbarbutton-icon-fill-attention": GONE_GECKO,
  "--lwt-brighttext-url-color": GONE_GECKO,
  "--urlbar-icon-border-radius": GONE_GECKO,
  "--subviewbutton-border-radius": GONE_GECKO,
  "--tab-selected-bgimage":
    "removed from Gecko; Lepton and tab-color-like-toolbar only reset it",
  "--in-content-icon-color": GONE_GECKO,
  "--in-content-accent-color": GONE_GECKO,
  "--in-content-accent-color-active": GONE_GECKO,
  "--in-content-table-border-dark-color": GONE_GECKO,
  "--in-content-sidebar-width": GONE_GECKO,
  "--card-shadow-hover": GONE_GECKO,
  "--grey-90-a60": PHOTON_COLOR,
  "--green-90": PHOTON_COLOR,
  "--red-80": PHOTON_COLOR,
  "--red-90": PHOTON_COLOR,
  "--yellow-60-a30": PHOTON_COLOR,
  "--yellow-90": PHOTON_COLOR,
  "--shadow-30": PHOTON_COLOR,

  // --- Lepton content CSS (about: pages, PDF viewer) ------------------------
  "--newtab-focus-border-selected": GONE_PAGE,
  "--newtab-search-dropdown-header-color": GONE_PAGE,
  "--newtab-search-dropdown-color": GONE_PAGE,
  "--color-blue-06": GONE_PAGE,
  "--color-blue-07": GONE_PAGE,
  "--color-blue-09": GONE_PAGE,
  "--page-bg": GONE_PAGE,
  "--color-shade-bg": GONE_PAGE,
  "--color-marketing-gray-02": GONE_PAGE,
  "--color-inverse-bg": GONE_PAGE,
  "--color-inverse": GONE_PAGE,
  "--color-text": GONE_PAGE,
  "--color-moz-text": GONE_PAGE,
  "--color-moz-heading": GONE_PAGE,
  "--color-text-light": GONE_PAGE,
  "--color-success": GONE_PAGE,
  "--color-warning": GONE_PAGE,
  "--color-error": GONE_PAGE,
  "--color-error-hover": GONE_PAGE,
  "--color-moz-inverse-bg": GONE_PAGE,
  "--focus-shadow": GONE_PAGE,
  "--color-dark-gray-10": GONE_PAGE,
  "--sidebaritem-bg-color":
    "PDF.js sidebar token; also inside the dead unobservable-document() block",

  // --- Lepton internal -----------------------------------------------------
  "--uc-tab-corner-half-size-reverse": "Lepton-internal variable nothing reads",
  "--uc-menubar-inner-height": "Lepton-internal variable nothing reads",

  // --- Floorp --------------------------------------------------------------
  "--arrowpanel-menuimageblank-padding":
    "icon-disabled.css copy of a Lepton variable that nothing reads",
  "--panel-touch-icon-size":
    "panel-sidebar declares it but never reads it (dead declaration)",
};
