// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { CHROME_EXTRAS_DEFAULTS } from "../chrome-extras.ts";
import {
  config,
  deepMerge,
  getChromeExtrasSettings,
  setConfig,
  updateChromeExtrasSetting,
} from "../configs.ts";
import {
  getOldChromeExtrasConfig,
  LEGACY_ALIASES,
  LEGACY_CHROME_EXTRAS_PREFS,
} from "../utils/old-config-migrator.ts";
import { zFloorpDesignConfigs } from "../type.ts";
import type { TitlebarTestBrowser } from "./types.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const VERTICAL_PREF = "sidebar.verticalTabs";
const LEGACY_PREF = "userChrome.tabbar.as_titlebar";
const LEGACY_PREF_NAMES = [
  ...new Set([
    ...Object.values(LEGACY_CHROME_EXTRAS_PREFS),
    ...Object.values(LEGACY_ALIASES).flat(),
  ]),
];
const DESIGNS = [
  "proton",
  "lepton",
  "photon",
  "protonfix",
  "fluerial",
] as const;
const browser = gBrowser as TitlebarTestBrowser;

function snapshotPref(name: string) {
  return {
    name,
    hadUserValue: Services.prefs.prefHasUserValue(name),
    value: Services.prefs.getBoolPref(name, false),
  };
}

function restorePref(pref: ReturnType<typeof snapshotPref>): void {
  if (pref.hadUserValue) {
    Services.prefs.setBoolPref(pref.name, pref.value);
  } else {
    Services.prefs.clearUserPref(pref.name);
    // Switching designs can apply user.js defaults even when the starting
    // Proton/Fluerial design does not mirror legacy prefs.
    if (Services.prefs.getBoolPref(pref.name, false) !== pref.value) {
      Services.prefs.getDefaultBranch("").setBoolPref(pref.name, pref.value);
    }
  }
}

function assertRestoredPref(pref: ReturnType<typeof snapshotPref>): void {
  assertEquals(
    Services.prefs.prefHasUserValue(pref.name),
    pref.hadUserValue,
    `${pref.name} must restore its user-value state`,
  );
  assertEquals(
    Services.prefs.getBoolPref(pref.name, false),
    pref.value,
    `${pref.name} must restore its effective value`,
  );
}

async function waitFor(
  condition: () => boolean,
  message: string,
): Promise<void> {
  const deadline = Date.now() + 10_000;
  while (!condition()) {
    assert(Date.now() < deadline, message);
    await new Promise((resolve) => setTimeout(resolve, 25));
  }
  await new Promise((resolve) => setTimeout(resolve, 25));
}

function display(element: Element): string {
  const style = getComputedStyle(element);
  assert(style, "a connected tab must have computed styles");
  return style.display;
}

function assertRendered(tab: Element): void {
  assert(display(tab) !== "none", "the selected tab must not be hidden");
  const rect = tab.getBoundingClientRect();
  assert(rect.width > 0 && rect.height > 0, "the tab must have a visible box");
}

async function withTitlebarTabs(
  test: (tabs: XULElement[]) => Promise<void>,
): Promise<void> {
  const before = config();
  const selected = browser.selectedTab;
  const root = document.documentElement;
  const attributes = ["customtitlebar", "tabsintitlebar"].map((name) => ({
    name,
    value: root.getAttribute(name),
  }));
  const browserPrefs = [VERTICAL_PREF, "sidebar.revamp"].map(snapshotPref);
  const legacyPrefs = LEGACY_PREF_NAMES.map(snapshotPref);
  const tabs: XULElement[] = [];
  try {
    Services.prefs.setBoolPref("sidebar.revamp", true);
    Services.prefs.setBoolPref(VERTICAL_PREF, false);
    setConfig((previous) => ({
      ...previous,
      tabbar: {
        ...previous.tabbar,
        tabbarStyle: "horizontal",
        tabbarPosition: "default",
      },
      uiCustomization: {
        ...previous.uiCustomization,
        chromeExtras: { ...CHROME_EXTRAS_DEFAULTS, tabbarAsTitlebar: true },
      },
    }));
    for (let i = 0; i < 2; i++) {
      tabs.push(browser.addTab("about:blank", {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
        skipAnimation: true,
      }));
    }
    browser.selectedTab = tabs[0];
    await waitFor(
      () => browser.tabContainer.getAttribute("orient") === "horizontal",
      "horizontal tabs must be ready",
    );
    await test(tabs);
  } finally {
    setConfig(before);
    for (const pref of browserPrefs) restorePref(pref);
    // Settle the mounted design/vertical-pref effects before restoring the
    // exact legacy values, including values outside the saved design config.
    await new Promise((resolve) => setTimeout(resolve, 25));
    for (const pref of legacyPrefs) restorePref(pref);
    for (const attribute of attributes) {
      if (attribute.value === null) {
        root.removeAttribute(attribute.name);
      } else {
        root.setAttribute(attribute.name, attribute.value);
      }
    }
    browser.selectedTab = selected;
    for (const tab of tabs) browser.removeTab(tab, { animate: false });
    await new Promise((resolve) => setTimeout(resolve, 25));
    for (const pref of [...browserPrefs, ...legacyPrefs]) {
      assertRestoredPref(pref);
    }
  }
}

async function testHorizontalBooleanAttributes(): Promise<void> {
  await withTitlebarTabs(async (tabs) => {
    for (const design of DESIGNS) {
      setConfig((previous) => ({
        ...previous,
        globalConfigs: { ...previous.globalConfigs, userInterface: design },
      }));
      await new Promise((resolve) => setTimeout(resolve, 25));
      for (const attribute of ["customtitlebar", "tabsintitlebar"]) {
        for (const value of ["", "true"]) {
          document.documentElement.removeAttribute("customtitlebar");
          document.documentElement.removeAttribute("tabsintitlebar");
          document.documentElement.setAttribute(attribute, value);
          for (const tab of tabs) {
            browser.selectedTab = tab;
            await new Promise((resolve) => setTimeout(resolve, 25));
            assertEquals(
              tab.getAttribute("selected"),
              "",
              "Firefox boolean attribute",
            );
            assertRendered(browser.tabContainer);
            assertRendered(tab);
            const other = tabs.find((candidate) => candidate !== tab);
            assert(other, "the fixture must include an unselected tab");
            assertEquals(
              display(other),
              "none",
              "titlebar mode shows only the current tab",
            );
          }
        }
      }
      browser.pinTab(tabs[0]);
      browser.selectedTab = tabs[0];
      await new Promise((resolve) => setTimeout(resolve, 25));
      for (const selected of ["", "true"]) {
        for (const pinned of ["", "true"]) {
          tabs[0].setAttribute("selected", selected);
          tabs[0].setAttribute("pinned", pinned);
          assertRendered(tabs[0]);
        }
      }
      tabs[0].setAttribute("selected", "");
      browser.unpinTab(tabs[0]);

      document.documentElement.removeAttribute("customtitlebar");
      document.documentElement.removeAttribute("tabsintitlebar");
      assertEquals(
        display(browser.tabContainer),
        "none",
        "native titlebar keeps the legacy behavior",
      );
      updateChromeExtrasSetting("tabbarAsTitlebar", false);
      await new Promise((resolve) => setTimeout(resolve, 25));
      for (const tab of tabs) assertRendered(tab);
      updateChromeExtrasSetting("tabbarAsTitlebar", true);
    }
  });
}

async function testVerticalTabsAcrossDesigns(): Promise<void> {
  await withTitlebarTabs(async (tabs) => {
    Services.prefs.setBoolPref(VERTICAL_PREF, true);
    await waitFor(
      () => browser.tabContainer.getAttribute("orient") === "vertical",
      "vertical tabs must be ready",
    );
    for (const design of DESIGNS) {
      setConfig((previous) => ({
        ...previous,
        globalConfigs: { ...previous.globalConfigs, userInterface: design },
      }));
      await new Promise((resolve) => setTimeout(resolve, 25));
      for (const customTitlebar of [false, true]) {
        document.documentElement.removeAttribute("tabsintitlebar");
        document.documentElement.toggleAttribute(
          "customtitlebar",
          customTitlebar,
        );
        assertRendered(browser.tabContainer);
        for (const tab of tabs) assertRendered(tab);
      }
    }
  });
}

async function testVerticalRoundTripPreservesHorizontalLegacyLayout(): Promise<
  void
> {
  await withTitlebarTabs(async (tabs) => {
    setConfig((previous) => ({
      ...previous,
      globalConfigs: { ...previous.globalConfigs, userInterface: "lepton" },
    }));
    updateChromeExtrasSetting("tabbarOneLiner", true);
    await waitFor(
      () => Services.prefs.getBoolPref(LEGACY_PREF, false),
      "horizontal Lepton layout must retain its legacy titlebar preference",
    );
    assertEquals(
      Services.prefs.getBoolPref("userChrome.tabbar.one_liner"),
      true,
      "the one-liner preference must remain enabled",
    );
    for (let i = 0; i < 2; i++) {
      Services.prefs.setBoolPref(VERTICAL_PREF, true);
      await waitFor(
        () =>
          browser.tabContainer.getAttribute("orient") === "vertical" &&
          !Services.prefs.getBoolPref(LEGACY_PREF, true),
        "vertical tabs must disable the legacy titlebar presentation",
      );
      assertEquals(
        getChromeExtrasSettings().tabbarAsTitlebar,
        true,
        "the saved choice survives",
      );
      assertEquals(
        Services.prefs.getBoolPref("userChrome.tabbar.one_liner"),
        true,
        "unrelated legacy preferences remain enabled",
      );
      assertRendered(browser.tabContainer);
      for (const tab of tabs) assertRendered(tab);
      Services.prefs.setBoolPref(VERTICAL_PREF, false);
      await waitFor(
        () =>
          browser.tabContainer.getAttribute("orient") === "horizontal" &&
          Services.prefs.getBoolPref(LEGACY_PREF, false),
        "returning to horizontal tabs must restore the legacy layout preference",
      );
      document.documentElement.setAttribute("customtitlebar", "");
      assertRendered(browser.selectedTab);
    }
  });
}

async function testLegacyMigrationKeepsTheSavedTitlebarChoice(): Promise<void> {
  await withTitlebarTabs(async () => {
    Services.prefs.setBoolPref(LEGACY_PREF, true);
    const migrated = getOldChromeExtrasConfig("lepton");
    assertEquals(migrated.tabbarAsTitlebar, true, "the legacy choice migrates");
    const saved = deepMerge({ chromeExtras: migrated }, {
      chromeExtras: { tabbarAsTitlebar: true },
    });
    setConfig((previous) => ({
      ...previous,
      globalConfigs: { ...previous.globalConfigs, userInterface: "lepton" },
      uiCustomization: { ...previous.uiCustomization, ...saved },
    }));
    Services.prefs.setBoolPref(VERTICAL_PREF, true);
    await waitFor(
      () => !Services.prefs.getBoolPref(LEGACY_PREF, true),
      "the migrated titlebar mode must not hide vertical tabs",
    );
    assertEquals(
      getChromeExtrasSettings().tabbarAsTitlebar,
      true,
      "the migrated choice remains enabled",
    );
    const persisted: unknown = JSON.parse(
      Services.prefs.getStringPref("floorp.design.configs"),
    );
    assert(
      zFloorpDesignConfigs.is(persisted),
      "persisted config must be valid",
    );
    assertEquals(
      persisted.uiCustomization.chromeExtras?.tabbarAsTitlebar,
      true,
      "vertical tabs must not erase the persisted choice",
    );
    Services.prefs.setBoolPref(VERTICAL_PREF, false);
    await waitFor(
      () => Services.prefs.getBoolPref(LEGACY_PREF, false),
      "the migrated titlebar choice must return in horizontal mode",
    );
  });
}

async function testFixtureRestoresNativeDesignLegacyPrefs(): Promise<void> {
  await withTitlebarTabs(async () => {
    for (const design of ["proton", "fluerial"] as const) {
      setConfig((previous) => ({
        ...previous,
        globalConfigs: { ...previous.globalConfigs, userInterface: design },
      }));
      Services.prefs.clearUserPref("userChrome.tabbar.one_liner");
      Services.prefs.setBoolPref(
        "userChrome.urlbar.always_show_page_actions",
        true,
      );
      const previous = [
        "userChrome.tabbar.one_liner",
        "userChrome.urlbar.always_show_page_actions",
      ].map(snapshotPref);
      await withTitlebarTabs(async () => {
        setConfig((settings) => ({
          ...settings,
          globalConfigs: { ...settings.globalConfigs, userInterface: "lepton" },
        }));
        updateChromeExtrasSetting("tabbarOneLiner", true);
        await waitFor(
          () =>
            Services.prefs.getBoolPref("userChrome.tabbar.one_liner", false) &&
            !Services.prefs.getBoolPref(
              "userChrome.urlbar.always_show_page_actions",
              true,
            ),
          "the mounted legacy mirror must change both fixture preferences",
        );
      });
      for (const pref of previous) assertRestoredPref(pref);
    }
  });
}

await runTests("tabbar-as-titlebar.test.ts", [
  {
    name: "titlebar tabs support boolean attributes and active vendor rules",
    fn: testHorizontalBooleanAttributes,
  },
  {
    name: "vertical tabs remain visible across every browser design",
    fn: testVerticalTabsAcrossDesigns,
  },
  {
    name: "vertical tabs preserve the horizontal legacy layout on round trips",
    fn: testVerticalRoundTripPreservesHorizontalLegacyLayout,
  },
  {
    name: "legacy migration retains the persisted titlebar choice",
    fn: testLegacyMigrationKeepsTheSavedTitlebarChoice,
  },
  {
    name: "fixture cleanup restores legacy preferences for Proton and Fluerial",
    fn: testFixtureRestoresNativeDesignLegacyPrefs,
  },
]);
