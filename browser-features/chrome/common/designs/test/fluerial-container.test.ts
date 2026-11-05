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
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

function styleOf(element: Element) {
  const style = getComputedStyle(element);
  assert(style, "native element has computed style");
  return style;
}

async function nextFrame(): Promise<void> {
  await new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));
}

async function testContainerIndicators(vertical: boolean): Promise<void> {
  const originalTab = gBrowser.selectedTab;
  const designPref = "floorp.design.configs";
  const verticalPref = "sidebar.verticalTabs";
  const hadDesign = Services.prefs.prefHasUserValue(designPref);
  const savedDesign = Services.prefs.getStringPref(designPref, "{}");
  const hadVertical = Services.prefs.prefHasUserValue(verticalPref);
  const savedVertical = Services.prefs.getBoolPref(verticalPref, false);
  const { ContextualIdentityService } = ChromeUtils.importESModule(
    "moz-src:///toolkit/components/contextualidentity/ContextualIdentityService.sys.mjs",
  );
  ContextualIdentityService.ensureDataReady();
  const identities = [
    ContextualIdentityService.create(
      "Fluerial blue regression",
      "fingerprint",
      "blue",
    ),
    ContextualIdentityService.create(
      "Fluerial orange regression",
      "briefcase",
      "orange",
    ),
  ];
  const tabs: XULElement[] = [];
  const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
    Ci.nsIStyleSheetService,
  );
  const agentSheet = sss.AGENT_SHEET;
  assert(typeof agentSheet === "number", "agent sheet origin is available");
  const sheet = Services.io.newURI(
    "data:text/css;charset=utf-8," + encodeURIComponent(
      GECKO_152_VAR_ALIASES_CSS + fluerial +
        TAB_COLOR_LIKE_TOOLBAR_CSS + FLUERIAL_TAB_CORNER_CSS,
    ),
  );
  let registered = false;
  // Native tabs expose the stacking and flex layout failures. A detached line
  // with an arbitrary z-index would not test the production paint order.
  try {
    const design: unknown = JSON.parse(savedDesign);
    assert(
      typeof design === "object" && design !== null,
      "design config exists",
    );
    const globals = "globalConfigs" in design &&
        typeof design.globalConfigs === "object" &&
        design.globalConfigs !== null
      ? design.globalConfigs
      : {};
    Services.prefs.setStringPref(
      designPref,
      JSON.stringify({
        ...design,
        globalConfigs: { ...globals, userInterface: "fluerial" },
      }),
    );
    Services.prefs.setBoolPref(verticalPref, vertical);
    await nextFrame();
    await nextFrame();
    // Match production origin: an author sheet cannot override the vendored
    // AGENT !important background positioning rule.
    sss.loadAndRegisterSheet(sheet, agentSheet);
    registered = true;
    for (const identity of identities) {
      tabs.push(gBrowser.addTrustedTab("about:blank", {
        userContextId: identity.userContextId,
        skipAnimation: true,
      }));
    }
    const normal = gBrowser.addTrustedTab("about:blank", {
      skipAnimation: true,
    });
    tabs.push(normal);
    gBrowser.selectedTab = normal;
    await nextFrame();
    await nextFrame();
    assertEquals(
      document.getElementById("tabbrowser-tabs")?.getAttribute("orient"),
      vertical ? "vertical" : "horizontal",
      "native tab orientation matches the case",
    );
    const line = (tab: XULElement): Element => {
      const result = tab.querySelector(".tab-context-line");
      assert(result, "native tabs have a container indicator");
      return result;
    };
    const normalLine = line(normal);
    const normalBefore = styleOf(normalLine).backgroundColor;
    const identityColors = new Set<string>();
    for (const tab of tabs.slice(0, 2)) {
      gBrowser.selectedTab = tab;
      await nextFrame();
      const indicator = line(tab);
      const rect = indicator.getBoundingClientRect();
      assert(rect.width > 0 && rect.height > 0, "selected indicator has area");
      const colorProbe = document.createElement("span");
      colorProbe.style.color = styleOf(tab).getPropertyValue(
        "--identity-icon-color",
      );
      document.head!.append(colorProbe);
      const expectedColor = styleOf(colorProbe).color;
      colorProbe.remove();
      identityColors.add(expectedColor);
      assertEquals(
        styleOf(indicator).backgroundColor,
        expectedColor,
        "each real container retains its own identity color",
      );
      if (!vertical) {
        const content = tab.querySelector(".tab-content");
        assert(content, "native tab content exists");
        const point = document.elementsFromPoint(
          rect.x + rect.width / 2,
          rect.y + rect.height / 2,
        );
        assert(
          point.includes(indicator) &&
            (!point.includes(content) ||
              point.indexOf(indicator) < point.indexOf(content)),
          "selected indicator paints above the opaque tab content",
        );
        // The CSS contract also applies to native multiselection attributes.
        gBrowser.selectedTab = normal;
        tab.setAttribute("multiselected", "true");
        await nextFrame();
        const multiRect = indicator.getBoundingClientRect();
        const multiPoint = document.elementsFromPoint(
          multiRect.x + multiRect.width / 2,
          multiRect.y + multiRect.height / 2,
        );
        assert(
          multiPoint.includes(indicator),
          "multiselected indicator is hit-test visible",
        );
        tab.removeAttribute("multiselected");
      } else {
        gBrowser.selectedTab = normal;
        await nextFrame();
        InspectorUtils.addPseudoClassLock(tab, ":hover");
        await nextFrame();
        const hover = indicator.getBoundingClientRect();
        const bounds = tab.getBoundingClientRect();
        assert(tab.matches(":hover"), "native hover selector matches");
        assert(
          hover.width > 0 && hover.height > 0,
          "hover retains the vertical line area",
        );
        assert(
          hover.x >= bounds.x - 0.5 && hover.right <= bounds.right + 0.5 &&
            hover.y >= bounds.y - 0.5 && hover.bottom <= bounds.bottom + 0.5,
          "hover indicator remains inside its native tab",
        );
        InspectorUtils.removePseudoClassLock(tab, ":hover");
      }
    }
    assertEquals(
      identityColors.size,
      2,
      "the two container colors remain distinct",
    );
    assertEquals(
      styleOf(normalLine).backgroundColor,
      normalBefore,
      "the non-container indicator is unchanged",
    );
  } finally {
    for (const tab of tabs) {
      InspectorUtils.removePseudoClassLock(tab, ":hover");
      tab.removeAttribute("multiselected");
    }
    gBrowser.selectedTab = originalTab;
    for (const tab of tabs) gBrowser.removeTab(tab, { animate: false });
    for (const identity of identities) {
      ContextualIdentityService.remove(identity.userContextId);
    }
    if (registered) sss.unregisterSheet(sheet, agentSheet);
    if (hadVertical) Services.prefs.setBoolPref(verticalPref, savedVertical);
    else Services.prefs.clearUserPref(verticalPref);
    if (hadDesign) Services.prefs.setStringPref(designPref, savedDesign);
    else Services.prefs.clearUserPref(designPref);
    await nextFrame();
  }
}

export async function runAllTests(): Promise<void> {
  await runTests("fluerial-container.test.ts", [
    {
      name:
        "native horizontal container indicators paint over selected and multiselected fills",
      fn: () => testContainerIndicators(false),
    },
    {
      name:
        "native vertical container indicators stay visible and inside tabs on hover",
      fn: () => testContainerIndicators(true),
    },
  ]);
}
