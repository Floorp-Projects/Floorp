/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/preact-xul";
import { signal } from "@preact/signals";
import { addDisposer, createRootHMR, rootEffect } from "@nora/preact-xul/lifetime";
import { panelSidebarConfig } from "../panel-sidebar/data/data";

type Orders = {
  fxSidebar: number;
  fxSidebarSplitter: number;
  browserBox: number;
  floorpSidebarSplitter: number;
  floorpSidebar: number;
  floorpSidebarSelectBox: number;
  verticaltabbarSplitter?: number;
  verticaltabbar?: number;
};

// deno-lint-ignore no-namespace
export namespace gFlexOrder {
  const fxSidebarPosition = "sidebar.position_start";
  const fxSidebarId = "sidebar-box";
  const fxSidebarSplitterId = "sidebar-splitter";

  const floorpSidebarId = "panel-sidebar-box";
  const floorpSidebarSplitterId = "panel-sidebar-splitter";
  const floorpSidebarSelectBoxId = "panel-sidebar-select-box";
  const browserBoxId = "tabbrowser-tabbox";

  const orders = createRootHMR(
    () =>
      signal<Orders>({
        fxSidebar: -1,
        fxSidebarSplitter: -1,
        browserBox: -1,
        floorpSidebarSplitter: -1,
        floorpSidebar: -1,
        floorpSidebarSelectBox: -1,
      }),
    import.meta.hot,
  );

  export function init() {
    const fxSidebarPositionPref = Services.prefs.getBoolPref(fxSidebarPosition);
    const floorpSidebarPositionPref = panelSidebarConfig.value.position_start;

    applyFlexOrder(fxSidebarPositionPref, floorpSidebarPositionPref);
    renderOrderStyle();

    const onPrefChange = () => {
      const fxPref = Services.prefs.getBoolPref(fxSidebarPosition);
      const floorpPref = panelSidebarConfig.value.position_start;
      applyFlexOrder(fxPref, floorpPref);
      renderOrderStyle();
    };

    Services.prefs.addObserver(fxSidebarPosition, onPrefChange);
    addDisposer(() => {
      Services.prefs.removeObserver(fxSidebarPosition, onPrefChange);
    });

    rootEffect(() => {
      const fxPref = Services.prefs.getBoolPref(fxSidebarPosition);
      const floorpPref = panelSidebarConfig.value.position_start;
      applyFlexOrder(fxPref, floorpPref);
      renderOrderStyle();
    });
  }

  export function applyFlexOrder(
    fxSidebarPositionPref: boolean,
    floorpSidebarPositionPref: boolean,
  ) {
    if (fxSidebarPositionPref && floorpSidebarPositionPref) {
      // Fx's sidebar -> browser -> Floorp's sidebar
      orders.value = {
        fxSidebar: 0,
        fxSidebarSplitter: 1,
        browserBox: 2,
        floorpSidebarSplitter: 3,
        floorpSidebar: 4,
        floorpSidebarSelectBox: 5,
      };
    } else if (fxSidebarPositionPref && !floorpSidebarPositionPref) {
      // Floorp sidebar -> Fx's sidebar -> browser
      orders.value = {
        floorpSidebarSelectBox: 0,
        floorpSidebar: 1,
        floorpSidebarSplitter: 2,
        fxSidebar: 3,
        fxSidebarSplitter: 4,
        browserBox: 5,
      };
    } else if (!fxSidebarPositionPref && floorpSidebarPositionPref) {
      // browser -> Vertical tab bar -> Fx's sidebar -> Floorp's sidebar
      orders.value = {
        browserBox: 0,
        verticaltabbarSplitter: 1,
        verticaltabbar: 2,
        fxSidebar: 3,
        fxSidebarSplitter: 4,
        floorpSidebarSplitter: 5,
        floorpSidebar: 6,
        floorpSidebarSelectBox: 7,
      };
    } else {
      // Floorp's sidebar -> browser -> Vertical tab bar -> Fx's sidebar
      orders.value = {
        floorpSidebarSelectBox: 0,
        floorpSidebar: 1,
        floorpSidebarSplitter: 2,
        browserBox: 3,
        verticaltabbarSplitter: 4,
        verticaltabbar: 5,
        fxSidebar: 6,
        fxSidebarSplitter: 7,
      };
    }
  }

  function renderOrderStyle() {
    render(() => (
      <style>
        {`
      #${fxSidebarId} {
        order: ${orders.value.fxSidebar} !important;
      }
      #${floorpSidebarId} {
        order: ${orders.value.floorpSidebar} !important;
      }
      #${floorpSidebarSelectBoxId} {
        order: ${orders.value.floorpSidebarSelectBox} !important;
      }
      #${floorpSidebarSplitterId} {
        order: ${orders.value.floorpSidebarSplitter} !important;
      }
      #${fxSidebarSplitterId} {
        order: ${orders.value.fxSidebarSplitter} !important;
      }
      #${browserBoxId} {
        order: ${orders.value.browserBox} !important;
      }
    `}
      </style>
    ), document?.head);
  }
}
