// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import fluerial from "@nora/skin/fluerial/css/fluerial.css?raw";
import {
  FLUERIAL_TAB_CORNER_CSS,
  TAB_COLOR_LIKE_TOOLBAR_CSS,
} from "../utils/tab-color-like-toolbar.css.ts";
import { GECKO_152_VAR_ALIASES_CSS } from "../utils/gecko-152-var-aliases.css.ts";
import {
  assert,
  assertApprox,
  runTests,
} from "../../../test/utils/test_harness.ts";
import type { AppearanceTestBrowser } from "./types.ts";

async function testNativeGroupLines(checkHover: boolean): Promise<void> {
  const browser = gBrowser as AppearanceTestBrowser;
  const originalTab = browser.selectedTab;
  const style = document.createElement("style");
  const tabs: XULElement[] = [];
  let group: XULElement | undefined;
  const verticalPref = "sidebar.verticalTabs";
  const hadVerticalPref = Services.prefs.prefHasUserValue(verticalPref);
  const wasVertical = Services.prefs.getBoolPref(verticalPref, false);
  const designPref = "floorp.design.configs";
  const hadDesignPref = Services.prefs.prefHasUserValue(designPref);
  const savedDesign = Services.prefs.getStringPref(designPref, "{}");
  // Load the Fluerial tab styles over native Firefox tabs: a detached DOM
  // cannot expose the paint-order bug.
  style.textContent = GECKO_152_VAR_ALIASES_CSS + fluerial +
    TAB_COLOR_LIKE_TOOLBAR_CSS + FLUERIAL_TAB_CORNER_CSS;
  try {
    const design: unknown = JSON.parse(savedDesign);
    assert(
      typeof design === "object" && design !== null,
      "Design config must be an object",
    );
    const globalConfigs = "globalConfigs" in design &&
        typeof design.globalConfigs === "object" &&
        design.globalConfigs !== null
      ? design.globalConfigs
      : {};
    // The full test runner has a live design renderer. Layering Fluerial over
    // Lepton leaves Lepton's selected-tab drop-shadow filter in place, which
    // creates a stacking context and hides the line regardless of our fix.
    Services.prefs.setStringPref(
      designPref,
      JSON.stringify({
        ...design,
        globalConfigs: { ...globalConfigs, userInterface: "fluerial" },
      }),
    );
    Services.prefs.setBoolPref(verticalPref, false);
    document.head!.append(style);
    for (let i = 0; i < 3; i++) {
      tabs.push(browser.addTab("about:blank", {
        triggeringPrincipal: Services.scriptSecurityManager
          .getSystemPrincipal(),
        skipAnimation: true,
      }));
    }
    group = browser.addTabGroup(tabs, { label: "Fluerial regression" });
    browser.selectedTab = tabs[0];
    await new Promise<void>((resolve) =>
      requestAnimationFrame(() => resolve())
    );

    assert(
      document.getElementById("tabbrowser-tabs")?.getAttribute("orient") ===
        "horizontal",
      "The native tab strip must be horizontal for this test",
    );
    const line = (tab: XULElement): Element => {
      const element = tab.querySelector(".tab-group-line");
      assert(element, "Native tabs must contain a group line");
      return element;
    };
    assert(
      getComputedStyle(tabs[0].querySelector(".tab-background")!)!.filter ===
        "none",
      "The Fluerial fixture must not inherit another design's drop-shadow filter",
    );
    if (!checkHover) {
      // The selected .tab-content previously painted over the group's line.
      // Hit testing checks actual stacking, rather than a z-index declaration.
      const selectedLine = line(tabs[0]);
      const rect = selectedLine.getBoundingClientRect();
      assert(
        rect.width > 0 && rect.height > 0,
        "Selected group line is rendered",
      );
      const stack = document.elementsFromPoint(
        rect.x + rect.width / 2,
        rect.y + rect.height / 2,
      );
      const content = tabs[0].querySelector(".tab-content")!;
      assert(
        stack.includes(selectedLine) &&
          (!stack.includes(content) ||
            stack.indexOf(selectedLine) < stack.indexOf(content)),
        "Selected group line must paint above the tab content fill",
      );
      return;
    }

    const idle = line(tabs[1]).getBoundingClientRect();
    const neighbor = line(tabs[2]).getBoundingClientRect();
    InspectorUtils.addPseudoClassLock(tabs[1], ":hover");
    const hovered = line(tabs[1]).getBoundingClientRect();
    assert(tabs[1].matches(":hover"), "The native hover selector must match");
    assertApprox(hovered.y, idle.y, 0.1, "Hover must not move the group line");
    assertApprox(
      hovered.x,
      idle.x,
      0.1,
      "Hover must not indent the group line",
    );
    assertApprox(
      hovered.width,
      idle.width,
      0.1,
      "Hover must not shorten the line",
    );
    assertApprox(
      hovered.y,
      neighbor.y,
      0.1,
      "Adjacent group lines stay aligned",
    );
  } finally {
    for (const tab of tabs) InspectorUtils.removePseudoClassLock(tab, ":hover");
    browser.selectedTab = originalTab;
    for (const tab of tabs) browser.removeTab(tab);
    // Native groups remove themselves through a MutationObserver. Let that
    // finish before restoring the design, which reparents the tab toolbar
    // and disconnects the group's observer, discarding pending mutations.
    await new Promise<void>((resolve) =>
      requestAnimationFrame(() => resolve())
    );
    style.remove();
    if (hadVerticalPref) Services.prefs.setBoolPref(verticalPref, wasVertical);
    else Services.prefs.clearUserPref(verticalPref);
    if (hadDesignPref) Services.prefs.setStringPref(designPref, savedDesign);
    else Services.prefs.clearUserPref(designPref);
    await new Promise<void>((resolve) =>
      requestAnimationFrame(() => resolve())
    );
    assert(!group?.isConnected, "The fixture must leave no native tab group");
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("fluerial-groups.test.ts", [
    {
      name: "native selected Fluerial group line paints above the tab fill",
      fn: () => testNativeGroupLines(false),
    },
    {
      name: "native Fluerial group lines stay aligned on hover",
      fn: () => testNativeGroupLines(true),
    },
  ]);
}
