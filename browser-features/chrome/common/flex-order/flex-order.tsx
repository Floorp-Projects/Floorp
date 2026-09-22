/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { createEffect, createSignal, onCleanup, untrack } from "solid-js";
import { panelSidebarConfig } from "../panel-sidebar/data/data";
import hoverStyle from "./hover-offset.css?inline";

type Orders = {
  floorpSidebarSplitter: number;
  floorpSidebar: number;
  floorpSidebarSelectBox: number;
};

// deno-lint-ignore no-namespace
export namespace gFlexOrder {
  const floorpSidebarId = "panel-sidebar-box";
  const floorpSidebarSplitterId = "panel-sidebar-splitter";
  const floorpSidebarSelectBoxId = "panel-sidebar-select-box";
  let hoverOffsetFrame: number | undefined;

  const [orders, setOrders] = createRootHMR(
    () =>
      createSignal<Orders>({
        floorpSidebarSplitter: -1,
        floorpSidebar: -1,
        floorpSidebarSelectBox: -1,
      }),
    import.meta.hot,
  );

  export function init() {
    renderOrderStyle();
    observePanelWidths();

    createEffect(() => {
      const floorpSidebarPositionPref = panelSidebarConfig().position_start;
      applyFlexOrder(floorpSidebarPositionPref);
    });
  }

  export function applyFlexOrder(floorpSidebarPositionPref: boolean) {
    if (floorpSidebarPositionPref) {
      // Keep Floorp's sidebar on the far right without overriding Firefox's
      // ordering for its sidebar launcher, content, or AI window.
      setOrders({
        floorpSidebarSplitter: 1000,
        floorpSidebar: 1001,
        floorpSidebarSelectBox: 1002,
      });
    } else {
      // Negative orders keep Floorp's sidebar on the far left while Firefox
      // remains the single owner of all upstream browser child ordering.
      setOrders({
        floorpSidebarSelectBox: -3,
        floorpSidebar: -2,
        floorpSidebarSplitter: -1,
      });
    }
    updateHoverOffset();
    scheduleHoverOffsetUpdate();
  }

  // Flex order and child insertion can change the rendered panel width only
  // after the current style/layout pass. Measure once more on the next frame
  // so Firefox's hover launcher uses the post-layout width in narrow windows.
  function scheduleHoverOffsetUpdate() {
    if (hoverOffsetFrame !== undefined) {
      cancelAnimationFrame(hoverOffsetFrame);
    }
    hoverOffsetFrame = requestAnimationFrame(() => {
      hoverOffsetFrame = undefined;
      updateHoverOffset();
    });
  }

  // Firefox anchors its absolute hover launcher to #browser's edge. Floorp's
  // in-flow panels can occupy that edge, so reserve their actual rendered size.
  // Do not observe the launcher itself: its animation must not feed back into
  // the offset or move the hover target out from underneath the pointer.
  function updateHoverOffset() {
    const browser = document?.getElementById("browser");
    if (!browser) return;
    const launcher = document.getElementById("sidebar-container");
    if (
      document.documentElement.hasAttribute("sidebar-expand-on-hover") &&
      (launcher?.hasAttribute("sidebar-launcher-expanded") ||
        launcher?.hasAttribute("sidebar-ongoing-animations"))
    ) {
      // Expanding Firefox's launcher can let a flex-shrunk Floorp panel grow.
      // Keep the collapsed measurement so the hover target does not move.
      return;
    }
    let width = 0;
    for (
      const id of [
        floorpSidebarSelectBoxId,
        floorpSidebarId,
        floorpSidebarSplitterId,
      ]
    ) {
      const element = document.getElementById(id);
      if (!element) continue;
      const style = getComputedStyle(element);
      if (
        !style || style.display === "none" || style.position === "absolute" ||
        style.position === "fixed"
      ) continue;
      width += element.getBoundingClientRect().width +
        (parseFloat(style.marginInlineStart) || 0) +
        (parseFloat(style.marginInlineEnd) || 0);
    }
    const atEnd = untrack(orders).floorpSidebar > 0;
    browser.style.setProperty(
      "--floorp-panel-start-width",
      `${atEnd ? 0 : width}px`,
    );
    browser.style.setProperty(
      "--floorp-panel-end-width",
      `${atEnd ? width : 0}px`,
    );
  }

  function observePanelWidths() {
    const browser = document?.getElementById("browser");
    if (!browser) return;
    const resizeObserver = new ResizeObserver(updateHoverOffset);
    const panelObserver = new MutationObserver(updateHoverOffset);
    const observe = () => {
      resizeObserver.disconnect();
      panelObserver.disconnect();
      for (
        const id of [
          floorpSidebarSelectBoxId,
          floorpSidebarId,
          floorpSidebarSplitterId,
        ]
      ) {
        const element = document.getElementById(id);
        if (element) {
          resizeObserver.observe(element);
          panelObserver.observe(element, {
            attributes: true,
            attributeFilter: ["data-floating", "hidden"],
          });
        }
      }
      updateHoverOffset();
      scheduleHoverOffsetUpdate();
    };
    const childrenObserver = new MutationObserver(observe);
    childrenObserver.observe(browser, { childList: true });
    observe();
    onCleanup(() => {
      resizeObserver.disconnect();
      panelObserver.disconnect();
      childrenObserver.disconnect();
      if (hoverOffsetFrame !== undefined) {
        cancelAnimationFrame(hoverOffsetFrame);
        hoverOffsetFrame = undefined;
      }
      browser.style.removeProperty("--floorp-panel-start-width");
      browser.style.removeProperty("--floorp-panel-end-width");
    });
  }

  function renderOrderStyle() {
    render(() => (
      <style id="floorp-flex-order-style" jsx>
        {`
      #${floorpSidebarId} {
        order: ${orders().floorpSidebar} !important;
      }
      #${floorpSidebarSelectBoxId} {
        order: ${orders().floorpSidebarSelectBox} !important;
      }
      #${floorpSidebarSplitterId} {
        order: ${orders().floorpSidebarSplitter} !important;
      }
      ${hoverStyle}
    `}
      </style>
    ), document?.head);
  }
}
