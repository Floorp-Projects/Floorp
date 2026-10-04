// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  isFloating,
  isPanelSidebarEnabled,
  isPanelSidebarHoverOpen,
  isPanelSidebarHoverPreview,
  panelSidebarConfig,
  panelSidebarData,
  selectedPanelId,
  setIsFloating,
  setIsPanelSidebarEnabled,
  setPanelSidebarConfig,
  setPanelSidebarData,
} from "../data/data.ts";
import {
  PANEL_HOVER_CLOSE_DELAY,
  PANEL_HOVER_OPEN_DELAY,
} from "../components/overlay-hover.ts";
import { PanelNavigator } from "../panel-navigator.ts";
import { gFlexOrder } from "../../flex-order/flex-order.tsx";
import {
  assert,
  assertApprox,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const panelId = "sidebar-overlay-regression";
const nextFrame = () =>
  new Promise<void>((resolve) => {
    requestAnimationFrame(() => requestAnimationFrame(() => resolve()));
  });
const waitForOpen = () =>
  new Promise<void>((resolve) =>
    setTimeout(resolve, PANEL_HOVER_OPEN_DELAY + 50)
  );
const waitForClose = () =>
  new Promise<void>((resolve) =>
    setTimeout(resolve, PANEL_HOVER_CLOSE_DELAY + 50)
  );

function element(id: string): HTMLElement {
  const result = document.getElementById(id);
  assert(result, `#${id} should exist`);
  return result as unknown as HTMLElement;
}

async function withOverlay(test: () => Promise<void>): Promise<void> {
  const controller = PanelNavigator.gPanelSidebar;
  assert(controller, "panel sidebar should be initialized");
  const config = panelSidebarConfig();
  const panels = panelSidebarData();
  const selected = selectedPanelId();
  const floating = isFloating();
  const enabled = isPanelSidebarEnabled();
  try {
    controller.closePanel();
    setIsFloating(false);
    setIsPanelSidebarEnabled(true);
    setPanelSidebarConfig({ ...config, overlay: true, openOnHover: true });
    setPanelSidebarData([...panels, {
      id: panelId,
      type: "web",
      url: "about:blank",
      width: 400,
      icon: undefined,
      userContextId: undefined,
      zoomLevel: undefined,
      userAgent: undefined,
      extensionId: undefined,
    }]);
    (globalThis.gBrowser.selectedBrowser as unknown as HTMLElement).focus();
    await nextFrame();
    await test();
  } finally {
    controller.closePanel();
    controller.unloadPanel(panelId);
    setPanelSidebarData(panels);
    setPanelSidebarConfig(config);
    gFlexOrder.applyFlexOrder(config.position_start);
    setIsPanelSidebarEnabled(enabled);
    if (selected) controller.openPanel(selected);
    setIsFloating(floating);
    await nextFrame();
  }
}

function enterPanel(): void {
  element(panelId).dispatchEvent(
    new MouseEvent("mouseover", {
      bubbles: true,
      relatedTarget: element("tabbrowser-tabbox"),
    }),
  );
}

function leaveSidebar(id = "panel-sidebar-box"): void {
  element(id).dispatchEvent(
    new MouseEvent("mouseout", {
      bubbles: true,
      relatedTarget: element("tabbrowser-tabbox"),
    }),
  );
}

async function testOverlayPreservesPageViewport(): Promise<void> {
  await withOverlay(async () => {
    const controller = PanelNavigator.gPanelSidebar!;
    for (const atRight of [false, true]) {
      controller.closePanel();
      setPanelSidebarConfig((config) => ({
        ...config,
        position_start: atRight,
      }));
      gFlexOrder.applyFlexOrder(atRight);
      await nextFrame();
      const before = element("tabbrowser-tabbox").getBoundingClientRect();
      controller.openPanel(panelId);
      await nextFrame();
      const after = element("tabbrowser-tabbox").getBoundingClientRect();
      assertApprox(
        after.width,
        before.width,
        1,
        "opening overlay preserves page width",
      );
      assertApprox(
        after.height,
        before.height,
        1,
        "opening overlay preserves page height",
      );
      assertApprox(
        after.left,
        before.left,
        1,
        "opening overlay preserves page position",
      );
      assertEquals(
        document.getElementById("panel-sidebar-splitter"),
        null,
        "overlay must not reserve an in-flow splitter",
      );
      const panel = element("panel-sidebar-box").getBoundingClientRect();
      const rail = element("panel-sidebar-select-box").getBoundingClientRect();
      assertApprox(
        atRight ? panel.right : panel.left,
        atRight ? rail.left : rail.right,
        1,
        "overlay anchors against the panel rail",
      );
      element("panel-sidebar-overlay-splitter").dispatchEvent(
        new KeyboardEvent("keydown", {
          key: atRight ? "ArrowLeft" : "ArrowRight",
          bubbles: true,
        }),
      );
      await nextFrame();
      assertApprox(
        element("panel-sidebar-box").getBoundingClientRect().width,
        panel.width + 10,
        1,
        "keyboard resizing moves the overlay's inner edge",
      );
      assertApprox(
        controller.getPanelData(panelId)?.width ?? 0,
        panel.width + 10,
        1,
        "keyboard resizing persists the width",
      );
      assertApprox(
        element("tabbrowser-tabbox").getBoundingClientRect().width,
        before.width,
        1,
        "overlay resizing preserves the page viewport",
      );
      const browser = element("browser");
      assertApprox(
        Number.parseFloat(browser.style.getPropertyValue(
          atRight ? "--floorp-panel-end-width" : "--floorp-panel-start-width",
        )),
        rail.width,
        1,
        "Firefox hover launcher reserves only the rail",
      );
      controller.closePanel();
      await nextFrame();
      assertApprox(
        element("tabbrowser-tabbox").getBoundingClientRect().width,
        before.width,
        1,
        "closing overlay preserves page width",
      );
    }
  });
}

async function testHoverPreviewRetainsBrowserAndCanPin(): Promise<void> {
  await withOverlay(async () => {
    // Native popup builders can remove their popup without a popuphidden event.
    // A prior menu must not prevent a subsequent preview from retracting.
    const removedPopup = document.createXULElement("menupopup");
    document.documentElement.append(removedPopup);
    removedPopup.dispatchEvent(new Event("popupshown", { bubbles: true }));
    removedPopup.dispatchEvent(new Event("popuphiding", { bubbles: true }));
    removedPopup.remove();
    const viewport = element("tabbrowser-tabbox").getBoundingClientRect().width;
    enterPanel();
    await waitForOpen();
    await nextFrame();
    assert(
      isPanelSidebarHoverPreview() && isPanelSidebarHoverOpen(),
      "hovering a panel opens a temporary preview",
    );
    const browser = element(`sidebar-panel-${panelId}`);
    assertApprox(
      element("tabbrowser-tabbox").getBoundingClientRect().width,
      viewport,
      1,
      "hover opening keeps the page viewport stable",
    );
    // Crossing from the icon rail into the panel should keep the preview open.
    element("panel-sidebar-select-box").dispatchEvent(
      new MouseEvent("mouseout", {
        bubbles: true,
        relatedTarget: element("panel-sidebar-box"),
      }),
    );
    await waitForClose();
    assert(isPanelSidebarHoverOpen(), "crossing into the panel keeps it open");
    leaveSidebar();
    await waitForClose();
    await nextFrame();
    assert(!isPanelSidebarHoverOpen(), "leaving retracts the hover preview");
    assertEquals(
      selectedPanelId(),
      panelId,
      "retraction retains panel selection",
    );
    assertEquals(
      document.getElementById(`sidebar-panel-${panelId}`),
      browser,
      "retraction retains the loaded browser",
    );
    assertEquals(
      getComputedStyle(element("panel-sidebar-box"))?.display,
      "none",
      "retracted preview is hidden",
    );
    enterPanel();
    await waitForOpen();
    element(panelId).click();
    leaveSidebar();
    await waitForClose();
    assert(!isPanelSidebarHoverPreview(), "clicking a preview pins it");
    assert(
      getComputedStyle(element("panel-sidebar-box"))?.display !== "none",
      "pinned panel stays open after pointer leave",
    );
    element(panelId).click();
    assertEquals(selectedPanelId(), null, "clicking a pinned panel closes it");
  });
}

async function testHoverCancellationAndInteractionGuards(): Promise<void> {
  await withOverlay(async () => {
    enterPanel();
    leaveSidebar("panel-sidebar-select-box");
    await waitForOpen();
    assertEquals(
      selectedPanelId(),
      null,
      "brief pointer crossings do not open a panel",
    );
    enterPanel();
    setPanelSidebarConfig((config) => ({ ...config, openOnHover: false }));
    await waitForOpen();
    assertEquals(
      selectedPanelId(),
      null,
      "disabling hover cancels a queued opening",
    );
    setPanelSidebarConfig((config) => ({ ...config, openOnHover: true }));
    enterPanel();
    await waitForOpen();
    const focused = element("panel-sidebar-overlay-splitter");
    focused.focus();
    assertEquals(
      document.activeElement,
      focused,
      "focusable overlay separator receives keyboard focus",
    );
    leaveSidebar();
    await waitForClose();
    assert(isPanelSidebarHoverOpen(), "keyboard focus keeps the preview open");
    const embeddedBrowser = element(`sidebar-panel-${panelId}`);
    embeddedBrowser.focus();
    assertEquals(
      document.activeElement,
      embeddedBrowser,
      "embedded panel browser is the chrome focus host",
    );
    leaveSidebar();
    await waitForClose();
    assert(
      isPanelSidebarHoverOpen(),
      "focused panel content keeps the preview open",
    );
    (globalThis.gBrowser.selectedBrowser as unknown as HTMLElement).focus();
    await waitForClose();
    assert(
      !isPanelSidebarHoverOpen(),
      "moving focus outside permits retraction",
    );
    enterPanel();
    await waitForOpen();
    const popup = document.createXULElement("menupopup");
    document.documentElement.append(popup);
    popup.dispatchEvent(new Event("popupshown", { bubbles: true }));
    try {
      leaveSidebar();
      await waitForClose();
      assert(isPanelSidebarHoverOpen(), "an open menu protects the preview");
      document.dispatchEvent(
        new KeyboardEvent("keydown", { key: "Escape", bubbles: true }),
      );
      assert(
        isPanelSidebarHoverOpen(),
        "Escape dismisses the menu before the preview",
      );
    } finally {
      popup.dispatchEvent(new Event("popuphidden", { bubbles: true }));
      popup.remove();
    }
    await waitForClose();
    assert(!isPanelSidebarHoverOpen(), "closing the menu permits retraction");
    enterPanel();
    await waitForOpen();
    document.dispatchEvent(new Event("dragstart"));
    leaveSidebar();
    await waitForClose();
    assert(isPanelSidebarHoverOpen(), "dragging protects the preview");
    document.dispatchEvent(new Event("dragend"));
    await waitForClose();
    assert(!isPanelSidebarHoverOpen(), "ending the drag permits retraction");
    element(panelId).dispatchEvent(
      new KeyboardEvent("keydown", {
        key: "Enter",
        bubbles: true,
      }),
    );
    assert(!isPanelSidebarHoverPreview(), "keyboard opening pins the panel");
    assertEquals(
      selectedPanelId(),
      panelId,
      "keyboard opening selects the panel",
    );
  });
}

async function testFullscreenSuppressesPendingHover(): Promise<void> {
  await withOverlay(async () => {
    const root = document.documentElement;
    const previous = root.getAttribute("inDOMFullscreen");
    try {
      enterPanel();
      root.setAttribute("inDOMFullscreen", "true");
      await waitForOpen();
      assertEquals(
        selectedPanelId(),
        null,
        "entering video fullscreen cancels queued hover opening",
      );
      root.removeAttribute("inDOMFullscreen");
      await nextFrame();
      PanelNavigator.gPanelSidebar!.openPanel(panelId);
      await nextFrame();
      root.setAttribute("inDOMFullscreen", "true");
      await nextFrame();
      assertEquals(
        getComputedStyle(element("panel-sidebar-box"))?.display,
        "none",
        "video fullscreen hides manually opened overlays",
      );
      assertEquals(
        selectedPanelId(),
        panelId,
        "video fullscreen keeps manually opened panel selection",
      );
      root.removeAttribute("inDOMFullscreen");
      await nextFrame();
      assert(
        getComputedStyle(element("panel-sidebar-box"))?.display !== "none",
        "leaving video fullscreen restores the pinned overlay",
      );
    } finally {
      if (previous === null) root.removeAttribute("inDOMFullscreen");
      else root.setAttribute("inDOMFullscreen", previous);
    }
  });
}

export async function runAllTests(): Promise<void> {
  await runTests("panelSidebarOverlay.test.ts", [
    {
      name: "anchored overlay preserves page viewport on both sides",
      fn: testOverlayPreservesPageViewport,
    },
    {
      name: "hover preview keeps its browser loaded and clicks pin it",
      fn: testHoverPreviewRetainsBrowserAndCanPin,
    },
    {
      name: "hover cancels pending work and protects focus, menus and dragging",
      fn: testHoverCancellationAndInteractionGuards,
    },
    {
      name: "video fullscreen cancels hover and suppresses pinned overlays",
      fn: testFullscreenSuppressesPendingHover,
    },
  ]);
}
