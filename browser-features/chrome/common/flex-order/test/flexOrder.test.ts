// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import { gFlexOrder } from "../flex-order.tsx";
import { createRoot } from "solid-js";

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";

const flexOrderStyleId = "floorp-flex-order-style";
let disposeStyle: (() => void) | undefined;

function cleanupFlexOrderStyle(): void {
  disposeStyle?.();
  disposeStyle = undefined;
  document?.getElementById(flexOrderStyleId)?.remove();
}

function findFlexOrderStyleText(): string | undefined {
  return document?.getElementById(flexOrderStyleId)?.textContent ?? undefined;
}

function getRenderedOrder(styleText: string, selector: string): number {
  const escapedSelector = selector.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  const match = styleText.match(
    new RegExp(`${escapedSelector}\\s*\\{[^}]*order:\\s*(-?\\d+)`),
  );
  assert(match, `style should include an order for ${selector}`);
  return Number(match[1]);
}

function getComputedOrder(id: string): number {
  const element = document?.getElementById(id) ?? null;
  assert(element !== null, `browser should contain #${id}`);
  const style = getComputedStyle(element);
  assert(style !== null, `browser should compute styles for #${id}`);
  return Number(style.order);
}

function assertStrictlyIncreasing(ids: string[], message: string): void {
  const orderValues = ids.map(getComputedOrder);
  for (let index = 1; index < orderValues.length; index++) {
    assert(
      orderValues[index - 1] < orderValues[index],
      `${message}: ${ids[index - 1]} (${
        orderValues[index - 1]
      }) should precede ${ids[index]} (${orderValues[index]})`,
    );
  }
}

function initializeFlexOrder(): string {
  cleanupFlexOrderStyle();
  createRoot((dispose) => {
    disposeStyle = dispose;
    gFlexOrder.init();
  });
  const styleText = findFlexOrderStyleText();
  assert(styleText !== undefined, "gFlexOrder.init should inject its style");
  return styleText;
}

function testGFlexOrderExports(): void {
  assert(
    typeof gFlexOrder.init === "function",
    "gFlexOrder.init should be a function",
  );
  assert(
    typeof gFlexOrder.applyFlexOrder === "function",
    "gFlexOrder.applyFlexOrder should be a function",
  );
}

function testInitRendersFlexOrderStyle(): void {
  const styleText = initializeFlexOrder();
  assert(
    styleText.includes("#panel-sidebar-box"),
    "style should include the Floorp sidebar selector",
  );
  cleanupFlexOrderStyle();
}

function testFloorpSidebarAtRight(): void {
  initializeFlexOrder();
  gFlexOrder.applyFlexOrder(true);

  const styleText = findFlexOrderStyleText();
  assert(styleText !== undefined, "flex-order style should remain rendered");
  assertEquals(
    getRenderedOrder(styleText, "#panel-sidebar-splitter"),
    1000,
    "right-side splitter should follow all Firefox-owned browser children",
  );
  assertEquals(
    getRenderedOrder(styleText, "#panel-sidebar-box"),
    1001,
    "right-side panel should follow its splitter",
  );
  assertEquals(
    getRenderedOrder(styleText, "#panel-sidebar-select-box"),
    1002,
    "right-side selector should be the outermost Floorp element",
  );
  cleanupFlexOrderStyle();
}

function testFloorpSidebarAtLeft(): void {
  initializeFlexOrder();
  gFlexOrder.applyFlexOrder(false);

  const styleText = findFlexOrderStyleText();
  assert(styleText !== undefined, "flex-order style should remain rendered");
  assertEquals(
    getRenderedOrder(styleText, "#panel-sidebar-select-box"),
    -3,
    "left-side selector should be the outermost Floorp element",
  );
  assertEquals(
    getRenderedOrder(styleText, "#panel-sidebar-box"),
    -2,
    "left-side panel should follow its selector",
  );
  assertEquals(
    getRenderedOrder(styleText, "#panel-sidebar-splitter"),
    -1,
    "left-side splitter should precede all Firefox-owned browser children",
  );
  cleanupFlexOrderStyle();
}

function testFirefoxOrderingIsNotOverridden(): void {
  const styleText = initializeFlexOrder();
  const firefoxOwnedSelectors = [
    "#sidebar-container",
    "#sidebar-launcher-splitter",
    "#sidebar-box",
    "#sidebar-splitter",
    "#tabbrowser-tabbox",
    "#ai-window-splitter",
    "#ai-window-box",
  ];

  for (const selector of firefoxOwnedSelectors) {
    const escapedSelector = selector.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
    assert(
      !new RegExp(`${escapedSelector}\\s*\\{[^}]*\\border\\s*:`).test(
        styleText,
      ),
      `Floorp should not override Firefox-owned order for ${selector}`,
    );
  }
  cleanupFlexOrderStyle();
}

function testFirefoxSidebarGroupRemainsCoherent(): void {
  initializeFlexOrder();
  const sidebarController = (globalThis as unknown as {
    SidebarController?: { setPosition(): void };
  }).SidebarController;
  assert(sidebarController, "SidebarController should be initialized");

  const positionPref = "sidebar.position_start";
  const originalPosition = Services.prefs.getBoolPref(positionPref, true);
  const hadUserPosition = Services.prefs.prefHasUserValue(positionPref);
  const originalFloorpPosition = getComputedOrder("panel-sidebar-box") > 0;
  const firefoxOwnedIds = [
    "sidebar-container",
    "sidebar-launcher-splitter",
    "sidebar-box",
    "sidebar-splitter",
    "tabbrowser-tabbox",
    "ai-window-splitter",
    "ai-window-box",
  ];
  const floorpIds = [
    "panel-sidebar-select-box",
    "panel-sidebar-box",
    "panel-sidebar-splitter",
  ];

  try {
    for (const firefoxAtStart of [true, false]) {
      Services.prefs.setBoolPref(positionPref, firefoxAtStart);
      sidebarController.setPosition();

      const expectedNativeOrder = firefoxAtStart
        ? [
          "sidebar-container",
          "sidebar-launcher-splitter",
          "sidebar-box",
          "sidebar-splitter",
          "tabbrowser-tabbox",
        ]
        : [
          "tabbrowser-tabbox",
          "sidebar-splitter",
          "sidebar-box",
          "sidebar-launcher-splitter",
          "sidebar-container",
        ];
      assertStrictlyIncreasing(
        expectedNativeOrder,
        `Firefox sidebar group should stay coherent at ${
          firefoxAtStart ? "start" : "end"
        }`,
      );

      const firefoxOrders = firefoxOwnedIds.map(getComputedOrder);
      for (const floorpAtRight of [true, false]) {
        gFlexOrder.applyFlexOrder(floorpAtRight);
        const floorpOrders = floorpIds.map(getComputedOrder);
        if (floorpAtRight) {
          assert(
            Math.min(...floorpOrders) > Math.max(...firefoxOrders),
            "right-side Floorp elements should follow every Firefox-owned element",
          );
        } else {
          assert(
            Math.max(...floorpOrders) < Math.min(...firefoxOrders),
            "left-side Floorp elements should precede every Firefox-owned element",
          );
        }
      }
    }
  } finally {
    if (hadUserPosition) {
      Services.prefs.setBoolPref(positionPref, originalPosition);
    } else {
      Services.prefs.clearUserPref(positionPref);
    }
    sidebarController.setPosition();
    gFlexOrder.applyFlexOrder(originalFloorpPosition);
    cleanupFlexOrderStyle();
  }
}

function testPositionCanBeUpdatedWithoutRenderingAnotherStyle(): void {
  initializeFlexOrder();

  gFlexOrder.applyFlexOrder(true);
  gFlexOrder.applyFlexOrder(false);
  const styleText = findFlexOrderStyleText();
  assert(styleText !== undefined, "flex-order style should remain rendered");
  assertEquals(
    getRenderedOrder(styleText, "#panel-sidebar-box"),
    -2,
    "latest Floorp sidebar position should be reflected",
  );
  assertEquals(
    document?.querySelectorAll(`#${flexOrderStyleId}`).length ?? 0,
    1,
    "position updates should reuse the existing style element",
  );
  cleanupFlexOrderStyle();
}

export async function runAllTests(): Promise<void> {
  const originalStyle = document.getElementById(flexOrderStyleId);
  const originalFloorpAtRight = getComputedOrder("panel-sidebar-box") > 0;
  originalStyle?.remove();
  try {
    await runTests("flexOrder.test.ts", [
      { name: "gFlexOrder exports", fn: testGFlexOrderExports },
      {
        name: "init renders flex-order style",
        fn: testInitRendersFlexOrderStyle,
      },
      {
        name: "Floorp sidebar can be placed at right",
        fn: testFloorpSidebarAtRight,
      },
      {
        name: "Floorp sidebar can be placed at left",
        fn: testFloorpSidebarAtLeft,
      },
      {
        name: "Firefox-owned ordering is not overridden",
        fn: testFirefoxOrderingIsNotOverridden,
      },
      {
        name: "Firefox sidebar group remains coherent",
        fn: testFirefoxSidebarGroupRemainsCoherent,
      },
      {
        name: "boxless panels do not move the right hover anchor",
        fn: testEmptyPanelHoverOffset,
      },
      {
        name: "hover anchor follows logical placement in LTR and RTL",
        fn: testDirectionalHoverAnchor,
      },
      {
        name: "position updates reuse the rendered style",
        fn: testPositionCanBeUpdatedWithoutRenderingAnotherStyle,
      },
    ]);
  } finally {
    cleanupFlexOrderStyle();
    if (originalStyle) document.head.appendChild(originalStyle);
    gFlexOrder.applyFlexOrder(originalFloorpAtRight);
  }
}

async function nextLayout(): Promise<void> {
  await new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));
  await new Promise<void>((resolve) => requestAnimationFrame(() => resolve()));
}

async function testEmptyPanelHoverOffset(): Promise<void> {
  initializeFlexOrder();
  const style = document.createElement("style");
  style.textContent = `
    #panel-sidebar-box, #panel-sidebar-splitter { display: none !important; }
    #panel-sidebar-select-box { display: contents !important; }
  `;
  document.head.appendChild(style);
  try {
    gFlexOrder.applyFlexOrder(true);
    await nextLayout();
    const rail = document.getElementById("panel-sidebar-select-box");
    assert(rail, "Floorp rail should exist");
    assertEquals(
      rail.getBoundingClientRect().width,
      0,
      "display:contents has no box",
    );
    assertEquals(
      document.getElementById("browser")?.style.getPropertyValue(
        "--floorp-panel-end-width",
      ),
      "0px",
      "empty rect must not reserve the whole browser width",
    );
  } finally {
    style.remove();
    cleanupFlexOrderStyle();
  }
}

async function testDirectionalHoverAnchor(): Promise<void> {
  initializeFlexOrder();
  const root = document.documentElement;
  const browser = document.getElementById("browser");
  const launcher = document.getElementById("sidebar-container");
  assert(browser && launcher, "native browser and launcher should exist");
  const savedDirection = root.getAttribute("dir");
  const attributes = [
    "sidebar-positionend",
    "sidebar-launcher-expanded",
    "sidebar-ongoing-animations",
  ];
  const saved = attributes.map((name) => launcher.getAttribute(name));
  const savedHover = root.getAttribute("sidebar-expand-on-hover");
  const style = document.createElement("style");
  style.textContent = `
    #sidebar-container { display: flex !important; width: 58px !important; min-width: 58px !important; max-width: 58px !important; }
    #panel-sidebar-box, #panel-sidebar-splitter { display: none !important; }
  `;
  document.head.appendChild(style);
  try {
    root.setAttribute("sidebar-expand-on-hover", "");
    for (const direction of ["ltr", "rtl"]) {
      root.setAttribute("dir", direction);
      for (const floorpAtEnd of [false, true]) {
        gFlexOrder.applyFlexOrder(floorpAtEnd);
        for (const firefoxAtEnd of [false, true]) {
          launcher.toggleAttribute("sidebar-positionend", firefoxAtEnd);
          // Keep native flex ordering consistent with its position attribute.
          const savedOrder = launcher.style.order;
          launcher.style.order = firefoxAtEnd ? "999" : "0";
          try {
            launcher.removeAttribute("sidebar-launcher-expanded");
            launcher.removeAttribute("sidebar-ongoing-animations");
            await nextLayout();
            const onRight = firefoxAtEnd !== (direction === "rtl");
            const edge = () => {
              const rect = launcher.getBoundingClientRect();
              return onRight ? rect.right : rect.left;
            };
            const collapsed = edge();
            const property = floorpAtEnd
              ? "--floorp-panel-end-width"
              : "--floorp-panel-start-width";
            const collapsedOffset = browser.style.getPropertyValue(property);
            for (
              const state of [
                "sidebar-launcher-expanded",
                "sidebar-ongoing-animations",
              ]
            ) {
              launcher.setAttribute(state, "");
              await nextLayout();
              assert(
                Math.abs(edge() - collapsed) <= 1,
                `stable anchor: ${direction}, Floorp end=${floorpAtEnd}, Firefox end=${firefoxAtEnd}, ${state}`,
              );
              const rail = document.getElementById("panel-sidebar-select-box");
              assert(rail, "Floorp rail should exist");
              const railStyle = rail.style.cssText;
              try {
                for (const dimension of ["width", "min-width", "max-width"]) {
                  rail.style.setProperty(dimension, "84px", "important");
                }
                await nextLayout();
                assertEquals(
                  browser.style.getPropertyValue(property),
                  collapsedOffset,
                  "ResizeObserver must retain the collapsed offset during hover",
                );
              } finally {
                rail.style.cssText = railStyle;
                await nextLayout();
              }
              launcher.removeAttribute(state);
              await nextLayout();
              assert(
                Math.abs(edge() - collapsed) <= 1,
                "collapse restores anchor",
              );
            }
          } finally {
            launcher.style.order = savedOrder;
          }
        }
      }
    }
  } finally {
    style.remove();
    if (savedDirection === null) root.removeAttribute("dir");
    else root.setAttribute("dir", savedDirection);
    if (savedHover === null) root.removeAttribute("sidebar-expand-on-hover");
    else root.setAttribute("sidebar-expand-on-hover", savedHover);
    attributes.forEach((name, index) => {
      const value = saved[index];
      if (value === null) launcher.removeAttribute(name);
      else launcher.setAttribute(name, value);
    });
    cleanupFlexOrderStyle();
  }
}
