// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { LEPTON_COMPAT_CSS } from "../utils/lepton-compat-152.css.ts";
import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function styleOf(element: Element): CSSStyleDeclaration {
  const style = getComputedStyle(element);
  assert(style, "element has computed style");
  return style;
}

async function settle(): Promise<void> {
  await new Promise<void>((resolve) => setTimeout(resolve, 250));
  await new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));
}

async function withNativeTabs(check: (tab: XULElement) => Promise<void>) {
  const designPref = "floorp.design.configs";
  const savedDesign = Services.prefs.getStringPref(designPref);
  const hadDesign = Services.prefs.prefHasUserValue(designPref);
  const prefs = ["sidebar.verticalTabs", "browser.uidensity"];
  const saved = prefs.map((name) => ({
    name,
    had: Services.prefs.prefHasUserValue(name),
    value: name === "browser.uidensity"
      ? Services.prefs.getIntPref(name, 0)
      : Services.prefs.getBoolPref(name, false),
  }));
  const original = gBrowser.selectedTab;
  const tab = gBrowser.addTrustedTab("about:blank", { skipAnimation: true });
  const sheet = document.createElementNS(
    "http://www.w3.org/1999/xhtml",
    "style",
  );
  // Production chromeStylesRaw uses a DOM style, not a global AGENT sheet.
  sheet.textContent = LEPTON_COMPAT_CSS;
  try {
    const config = JSON.parse(savedDesign) as {
      globalConfigs: { userInterface: string };
    };
    config.globalConfigs.userInterface = "lepton";
    Services.prefs.setStringPref(designPref, JSON.stringify(config));
    Services.prefs.setBoolPref("sidebar.verticalTabs", false);
    gBrowser.selectedTab = tab;
    await settle();
    document.documentElement.append(sheet);
    await settle();
    assert(
      Services.prefs.getBoolPref("userChrome.tab.lepton_like_padding"),
      "Lepton padding is enabled",
    );
    await check(tab);
  } finally {
    sheet.remove();
    gBrowser.selectedTab = original;
    gBrowser.removeTab(tab, { animate: false });
    if (hadDesign) Services.prefs.setStringPref(designPref, savedDesign);
    else Services.prefs.clearUserPref(designPref);
    for (const pref of saved) {
      if (!pref.had) Services.prefs.clearUserPref(pref.name);
      else if (typeof pref.value === "boolean") {
        Services.prefs.setBoolPref(pref.name, pref.value);
      } else Services.prefs.setIntPref(pref.name, pref.value);
    }
    await settle();
  }
}

async function testHorizontalDensity(): Promise<void> {
  await withNativeTabs(async (tab) => {
    const background = tab.querySelector(".tab-background");
    assert(background, "real native tab background exists");
    assertEquals(
      document.getElementById("tabbrowser-tabs")?.getAttribute("orient"),
      "horizontal",
      "real horizontal tabs",
    );
    for (
      const [density, expected] of [[0, "2px"], [1, "2px"], [2, "4px"]] as const
    ) {
      Services.prefs.setIntPref("browser.uidensity", density);
      await settle();
      assertEquals(
        styleOf(document.documentElement).getPropertyValue(
          "--tab-block-margin",
        ).trim(),
        expected,
        "vendor density value is available",
      );
      assertEquals(
        styleOf(background).marginBlockStart,
        expected,
        `native background consumes Lepton density ${density}`,
      );
      assert(
        background.getBoundingClientRect().height > 0,
        "native background retains area",
      );
    }
  });
}

async function testVerticalAndThemeOverrides(): Promise<void> {
  await withNativeTabs(async (tab) => {
    const root = document.documentElement;
    const background = tab.querySelector(".tab-background");
    assert(background, "real native tab background exists");
    const originalStyle = root.getAttribute("style");
    try {
      Services.prefs.setBoolPref("sidebar.verticalTabs", true);
      await settle();
      assertEquals(
        document.getElementById("tabbrowser-tabs")?.getAttribute("orient"),
        "vertical",
        "real vertical tabs",
      );
      const before = styleOf(background).marginBlockStart;
      root.style.setProperty("--tab-block-margin", "11px", "important");
      await settle();
      assertEquals(
        styleOf(background).marginBlockStart,
        before,
        "vertical native margin is independent of the legacy horizontal value",
      );
      Services.prefs.setBoolPref("sidebar.verticalTabs", false);
      await settle();
      assertEquals(
        styleOf(background).marginBlockStart,
        "11px",
        "horizontal alias follows a custom legacy value rather than hardcoding 2px",
      );
      root.style.setProperty("--tab-margin-block", "13px", "important");
      await settle();
      assertEquals(
        styleOf(background).marginBlockStart,
        "13px",
        "explicit theme margin remains authoritative",
      );
    } finally {
      if (originalStyle === null) root.removeAttribute("style");
      else root.setAttribute("style", originalStyle);
    }
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("lepton-tab-margin", [
    {
      name: "native horizontal tabs retain Lepton density margins",
      fn: testHorizontalDensity,
    },
    {
      name: "vertical spacing and explicit theme margins remain independent",
      fn: testVerticalAndThemeOverrides,
    },
  ]);
}
