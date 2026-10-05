/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  CHROME_EXTRAS_DEFAULTS,
  CHROME_EXTRAS_KEYS,
  type ChromeExtrasKey,
  type ChromeExtrasSettings,
  isLeptonFamilyDesign,
} from "../chrome-extras.ts";

/**
 * The `userChrome.*` prefs the chrome-extras toggles used to be stored in,
 * keyed by their chrome-extras key. Seeding from these is what makes an
 * upgrade keep the user's choices.
 *
 * Lepton's user.js writes `userChrome.icon.menu = true` to the default branch.
 * Config migration can run before that user.js is applied, so the iconMenu
 * fallback follows the selected design unless the user set the pref directly.
 * Every other key defaults to `false` in the bundled user.js.
 */
export const LEGACY_CHROME_EXTRAS_PREFS: Partial<
  Record<ChromeExtrasKey, string>
> = {
  autohideTab: "userChrome.autohide.tab",
  autohideNavbar: "userChrome.autohide.navbar",
  autohideSidebar: "userChrome.autohide.sidebar",
  autohideBackButton: "userChrome.autohide.back_button",
  autohideForwardButton: "userChrome.autohide.forward_button",
  autohidePageAction: "userChrome.autohide.page_action",
  hiddenTabIcon: "userChrome.hidden.tab_icon",
  hiddenTabbar: "userChrome.hidden.tabbar",
  hiddenNavbar: "userChrome.hidden.navbar",
  hiddenSidebarHeader: "userChrome.hidden.sidebar_header",
  hiddenUrlbarIconbox: "userChrome.hidden.urlbar_iconbox",
  hiddenBookmarkbarIcon: "userChrome.hidden.bookmarkbar_icon",
  hiddenBookmarkbarLabel: "userChrome.hidden.bookmarkbar_label",
  hiddenDisabledMenu: "userChrome.hidden.disabled_menu",
  iconDisabled: "userChrome.icon.disabled",
  iconMenu: "userChrome.icon.menu",
  centeredTab: "userChrome.centered.tab",
  centeredUrlbar: "userChrome.centered.urlbar",
  centeredBookmarkbar: "userChrome.centered.bookmarkbar",
  urlViewMoveIconToLeft: "userChrome.urlView.move_icon_to_left",
  urlViewGoButtonWhenTyping: "userChrome.urlView.go_button_when_typing",
  // Lepton's own name for this pref is `userChrome.urlbar.*`; the settings page
  // used to write `userChrome.urlView.*`, which Lepton never read. Accept both
  // so nobody's saved choice is dropped on upgrade.
  urlViewAlwaysShowPageActions: "userChrome.urlView.always_show_page_actions",
  sidebarOverlap: "userChrome.sidebar.overlap",
  tabbarAsTitlebar: "userChrome.tabbar.as_titlebar",
  tabbarOneLiner: "userChrome.tabbar.one_liner",
};

/** Extra legacy pref names accepted for keys whose name was wrong upstream. */
export const LEGACY_ALIASES: Partial<Record<ChromeExtrasKey, string[]>> = {
  urlViewAlwaysShowPageActions: ["userChrome.urlbar.always_show_page_actions"],
};

/**
 * Seed the chrome-extras settings from the old `userChrome.*` prefs.
 *
 * Called while building the default config; `deepMerge` then lets the value
 * already stored in `floorp.design.configs` win, so this only has an effect on
 * the first run after the upgrade.
 */
function getMigrationDesign(): string {
  try {
    const raw = Services.prefs.getStringPref("floorp.design.configs", "");
    if (raw) {
      const parsed: unknown = JSON.parse(raw);
      if (
        typeof parsed === "object" && parsed !== null &&
        "globalConfigs" in parsed
      ) {
        const globalConfigs = parsed.globalConfigs;
        if (
          typeof globalConfigs === "object" && globalConfigs !== null &&
          "userInterface" in globalConfigs &&
          typeof globalConfigs.userInterface === "string"
        ) {
          return globalConfigs.userInterface;
        }
      }
    }
  } catch {
    // The normal config loader reports invalid stored JSON. Use legacy prefs.
  }
  return getOldInterfaceConfig();
}

export function getOldChromeExtrasConfig(
  design = getMigrationDesign(),
): ChromeExtrasSettings {
  const result: ChromeExtrasSettings = { ...CHROME_EXTRAS_DEFAULTS };
  for (const key of Object.keys(result) as ChromeExtrasKey[]) {
    const pref = LEGACY_CHROME_EXTRAS_PREFS[key];
    // Floorp-only toggles have no legacy pref to seed from.
    if (!pref) continue;
    if (key === "iconMenu" && !Services.prefs.prefHasUserValue(pref)) {
      result.iconMenu = isLeptonFamilyDesign(design);
      continue;
    }
    if (Services.prefs.getBoolPref(pref, CHROME_EXTRAS_DEFAULTS[key])) {
      result[key] = true;
      continue;
    }
    for (const alias of LEGACY_ALIASES[key] ?? []) {
      if (Services.prefs.getBoolPref(alias, false)) {
        result[key] = true;
        break;
      }
    }
  }
  return result;
}

/**
 * Keep the vendored Lepton stylesheet in step with the new config. Lepton,
 * Photon, and ProtonFix still load that stylesheet, whose old `userChrome.*`
 * media queries can otherwise keep a rule active after its new toggle is
 * switched off. The new config remains the source of truth; these prefs are
 * only a compatibility bridge until the vendored rules can be removed.
 *
 * Write a user value even when the current default already matches. Applying
 * Lepton's user.js again can change the default branch without changing the
 * design config, and a user value must keep the two in sync.
 */
export function syncLegacyChromeExtrasPrefs(
  settings: ChromeExtrasSettings,
  verticalTabs = false,
): void {
  for (const key of CHROME_EXTRAS_KEYS) {
    const legacyPref = LEGACY_CHROME_EXTRAS_PREFS[key];
    if (!legacyPref) continue;
    // Keep Lepton's horizontal layout combinations, but never hide vertical tabs.
    const value = settings[key] &&
      !(key === "tabbarAsTitlebar" && verticalTabs);
    const prefs = [legacyPref, ...(LEGACY_ALIASES[key] ?? [])];
    for (const pref of prefs) {
      if (
        !Services.prefs.prefHasUserValue(pref) ||
        Services.prefs.getBoolPref(pref, !value) !== value
      ) {
        Services.prefs.setBoolPref(pref, value);
      }
    }
  }
}

export const getOldInterfaceConfig = () => {
  switch (Services.prefs.getIntPref("floorp.browser.user.interface", 0)) {
    case 3:
      switch (Services.prefs.getIntPref("floorp.lepton.interface", 0)) {
        case 1:
          return "photon";
        case 3:
          return "protonfix";
        default:
          return "lepton";
      }
    case 8:
      return "fluerial";
  }

  return "lepton";
};

export const getOldTabbarStyleConfig = () => {
  switch (Services.prefs.getIntPref("floorp.tabbar.style", 0)) {
    case 1:
      return "multirow";
    case 2:
      return "vertical";
    default:
      return "horizontal";
  }
};

export const getOldTabbarPositionConfig = () => {
  switch (Services.prefs.getIntPref("floorp.browser.tabbar.settings", 0)) {
    case 1:
      return "hide-horizontal-tabbar";
    case 2:
      return "optimise-to-vertical-tabbar";
    case 3:
      return "bottom-of-navigation-toolbar";
    case 4:
      return "bottom-of-window";
    default:
      return "default";
  }
};
