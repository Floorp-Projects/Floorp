/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { createRootHMR, render } from "@nora/solid-xul";
import { createEffect, createSignal } from "solid-js";
import { panelSidebarConfig } from "../panel-sidebar/data/data";

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
    `}
      </style>
    ), document?.head);
  }
}
