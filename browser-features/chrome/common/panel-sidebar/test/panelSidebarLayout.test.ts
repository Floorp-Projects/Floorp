// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  isFloating,
  panelSidebarConfig,
  panelSidebarData,
  selectedPanelId,
  setIsFloating,
  setPanelSidebarConfig,
  setPanelSidebarData,
  setSelectedPanelId,
} from "../data/data.ts";
import { PanelNavigator } from "../panel-navigator.ts";
import { gFlexOrder } from "../../flex-order/flex-order.tsx";
import {
  assert,
  assertApprox,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const testPanelId = "sidebar-layout-regression";
const nextFrame = () =>
  new Promise<void>((resolve) => {
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()));
  });

function element(id: string): HTMLElement {
  const result = document.getElementById(id);
  assert(result, `#${id} should exist`);
  return result as unknown as HTMLElement;
}

async function withPanel(test: () => Promise<void>): Promise<void> {
  const controller = PanelNavigator.gPanelSidebar;
  assert(controller, "panel sidebar should be initialized");
  const config = panelSidebarConfig();
  const panels = panelSidebarData();
  const selected = selectedPanelId();
  const floating = isFloating();
  try {
    setIsFloating(false);
    setPanelSidebarData([...panels, {
      id: testPanelId,
      type: "web",
      url: "about:blank",
      width: 400,
      icon: undefined,
      userContextId: undefined,
      zoomLevel: undefined,
      userAgent: undefined,
      extensionId: undefined,
    }]);
    controller.changePanel(testPanelId);
    await nextFrame();
    await test();
  } finally {
    controller.unloadPanel(testPanelId);
    setPanelSidebarData(panels);
    setPanelSidebarConfig(config);
    gFlexOrder.applyFlexOrder(config.position_start);
    if (selected) controller.changePanel(selected);
    setIsFloating(floating);
  }
}

async function testWebPanelViewport(): Promise<void> {
  await withPanel(async () => {
    const browser = element(`sidebar-panel-${testPanelId}`) as unknown as {
      browsingContext: { associatedWindow: Window & typeof globalThis };
      getBoundingClientRect(): DOMRect;
    };
    let child: (Window & typeof globalThis) | undefined;
    for (let attempt = 0; attempt < 200; attempt++) {
      child = browser.browsingContext?.associatedWindow;
      if (child?.document.getElementById("floorp-webpanel-styles")) break;
      await new Promise((resolve) => setTimeout(resolve, 50));
    }
    assert(
      child && child.document.getElementById("floorp-webpanel-styles"),
      "web panel should initialize",
    );
    for (const width of [225, 400, 600]) {
      element("panel-sidebar-box").style.width = `${width}px`;
      await nextFrame();
      assertApprox(
        child.document.documentElement.getBoundingClientRect().width,
        browser.getBoundingClientRect().width,
        1,
        `child viewport should fit its ${width}px panel instead of the taskbar window minimum`,
      );
    }
  });
}

async function testCompletedResizePersists(): Promise<void> {
  await withPanel(async () => {
    const controller = PanelNavigator.gPanelSidebar!;
    for (const atEnd of [false, true]) {
      setPanelSidebarConfig((config) => ({ ...config, position_start: atEnd }));
      await nextFrame();
      const box = element("panel-sidebar-box");
      box.style.width = "475px";
      // Gecko dispatches command after committing mouse or keyboard resizing.
      element("panel-sidebar-splitter").dispatchEvent(new Event("command"));
      const savedWidth = Math.round(box.getBoundingClientRect().width);
      assertEquals(
        controller.getPanelData(testPanelId)?.width,
        savedWidth,
        "completed resize should update panel data",
      );
      const saved = JSON.parse(
        Services.prefs.getStringPref("floorp.panelSidebar.data"),
      ) as { data: { id: string; width: number }[] };
      assertEquals(
        saved.data.find((panel) => panel.id === testPanelId)?.width,
        savedWidth,
        "completed width should be persisted in preferences",
      );
      controller.changePanel(testPanelId);
      await nextFrame();
      controller.saveCurrentSidebarWidth();
      assertEquals(
        controller.getPanelData(testPanelId)?.width,
        savedWidth,
        "hidden panel must not save zero width",
      );
      controller.changePanel(testPanelId);
      await nextFrame();
      assertApprox(
        box.getBoundingClientRect().width,
        savedWidth,
        1,
        "reopening should restore width",
      );
      controller.unloadPanel(testPanelId);
      controller.changePanel(testPanelId);
      await nextFrame();
      assertApprox(
        box.getBoundingClientRect().width,
        savedWidth,
        1,
        "recreating a panel should restore width",
      );
    }
  });
}

async function testHoverAnchor(): Promise<void> {
  await withPanel(async () => {
    const root = document.documentElement;
    const launcher = element("sidebar-container");
    const savedRoot = root.hasAttribute("sidebar-expand-on-hover");
    const savedEnd = launcher.hasAttribute("sidebar-positionend");
    const savedExpanded = launcher.hasAttribute("sidebar-launcher-expanded");
    const savedAnimating = launcher.hasAttribute("sidebar-ongoing-animations");
    const pref = "sidebar.position_start";
    const hadPref = Services.prefs.prefHasUserValue(pref);
    const savedPref = Services.prefs.getBoolPref(pref, true);
    const verticalPref = "sidebar.verticalTabs";
    const hadVerticalPref = Services.prefs.prefHasUserValue(verticalPref);
    const savedVerticalPref = Services.prefs.getBoolPref(verticalPref, false);
    const sidebar =
      (globalThis as unknown as { SidebarController: { setPosition(): void } })
        .SidebarController;
    try {
      Services.prefs.setBoolPref(verticalPref, true);
      await nextFrame();
      root.setAttribute("sidebar-expand-on-hover", "");
      for (const firefoxAtStart of [true, false]) {
        Services.prefs.setBoolPref(pref, firefoxAtStart);
        sidebar.setPosition();
        for (const floorpAtEnd of [false, true]) {
          setPanelSidebarConfig((config) => ({
            ...config,
            position_start: floorpAtEnd,
          }));
          gFlexOrder.applyFlexOrder(floorpAtEnd);
          for (const open of [true, false]) {
            setSelectedPanelId(open ? testPanelId : null);
            launcher.removeAttribute("sidebar-launcher-expanded");
            launcher.removeAttribute("sidebar-ongoing-animations");
            await nextFrame();
            const collapsed = launcher.getBoundingClientRect();
            for (
              const attr of [
                "sidebar-launcher-expanded",
                "sidebar-ongoing-animations",
              ]
            ) {
              launcher.setAttribute(attr, "");
              await nextFrame();
              const expanded = launcher.getBoundingClientRect();
              assertApprox(
                firefoxAtStart ? expanded.left : expanded.right,
                firefoxAtStart ? collapsed.left : collapsed.right,
                1,
                `hover anchor should stay put: Firefox start=${firefoxAtStart}, Floorp end=${floorpAtEnd}, open=${open}, ${attr}`,
              );
              launcher.removeAttribute(attr);
            }
          }
        }
      }
    } finally {
      if (hadPref) Services.prefs.setBoolPref(pref, savedPref);
      else Services.prefs.clearUserPref(pref);
      if (hadVerticalPref) {
        Services.prefs.setBoolPref(verticalPref, savedVerticalPref);
      } else Services.prefs.clearUserPref(verticalPref);
      sidebar.setPosition();
      root.toggleAttribute("sidebar-expand-on-hover", savedRoot);
      launcher.toggleAttribute("sidebar-positionend", savedEnd);
      launcher.toggleAttribute("sidebar-launcher-expanded", savedExpanded);
      launcher.toggleAttribute("sidebar-ongoing-animations", savedAnimating);
    }
  });
}

async function testFloatingResizePersists(): Promise<void> {
  await withPanel(async () => {
    setIsFloating(true);
    await nextFrame();
    const box = element("panel-sidebar-box");
    const before = box.getBoundingClientRect();
    element("floating-splitter-right").dispatchEvent(
      new MouseEvent("mousedown", {
        bubbles: true,
        clientX: before.right,
        clientY: before.top + 100,
      }),
    );
    // Finish before the queued animation frame. The final pending width must
    // be applied before saving, including very short drags.
    document.dispatchEvent(
      new MouseEvent("mousemove", {
        clientX: before.right + 60,
        clientY: before.top + 100,
      }),
    );
    document.dispatchEvent(new MouseEvent("mouseup"));
    const width = box.getBoundingClientRect().width;
    assertApprox(
      width,
      before.width + 60,
      1,
      "floating resize should flush the final frame",
    );
    assertEquals(
      PanelNavigator.gPanelSidebar!.getPanelData(testPanelId)?.width,
      Math.round(width),
      "floating resize should persist its final width",
    );
    await nextFrame();
    const browser = element("browser");
    const property = panelSidebarConfig().position_start
      ? "--floorp-panel-end-width"
      : "--floorp-panel-start-width";
    assertApprox(
      parseFloat(browser.style.getPropertyValue(property)),
      element("panel-sidebar-select-box").getBoundingClientRect().width,
      1,
      "floating panel should not reserve space for the hover launcher",
    );
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("panelSidebarLayout.test.ts", [
    {
      name: "embedded web panel viewport fits its container",
      fn: testWebPanelViewport,
    },
    {
      name: "completed resize persists across closing and recreating panels",
      fn: testCompletedResizePersists,
    },
    {
      name:
        "hover expansion preserves the anchor on either side with panels open or closed",
      fn: testHoverAnchor,
    },
    {
      name: "floating resize flushes and persists the final animation frame",
      fn: testFloatingResizePersists,
    },
  ]);
}
