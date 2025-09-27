/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { checkPaddingEnabled } from "./titilebar-padding.ts";
import { config } from "../../designs/configs.ts";
import { TabbarStyleModifyCSSElement } from "./tabbar-style-element.tsx";
import { render } from "@nora/solid-xul";

export namespace gTabbarStyleFunctions {
  function getPanelUIMenuButton(): XULElement | null {
    return document?.querySelector("#PanelUI-menu-button") as XULElement | null;
  }
  function getTabbarElement(): XULElement | null {
    return document?.querySelector("#TabsToolbar") as XULElement | null;
  }
  function getNavbarElement(): XULElement | null {
    return document?.querySelector("#nav-bar") as XULElement | null;
  }
  function getNavigatorToolboxtabbarElement(): XULElement | null {
    return document?.querySelector("#navigator-toolbox") as XULElement | null;
  }
  function getBrowserElement(): XULElement | null {
    return document?.querySelector("#browser") as XULElement | null;
  }
  function getUrlbarContainer(): XULElement | null {
    return document?.querySelector("#urlbar-container") as XULElement | null;
  }
  function getSidebarVerticalTab(): XULElement | null {
    return document?.querySelector("#vertical-tabs") as XULElement | null;
  }

  export function revertToDefaultStyle() {
    const tabbarElement = getTabbarElement();
    const navigatorToolbox = getNavigatorToolboxtabbarElement();
    const urlbarContainer = getUrlbarContainer();
    const windowManageContainer = document?.querySelector(
      "#floorp-tabbar-window-manage-container",
    ) as Node;
    const tabbarModifyCss = document?.querySelector(
      "#floorp-tabbar-modify-css",
    );

    // Add flex to tabbarElement
    tabbarElement?.setAttribute("flex", "1");

    // Remove attributes and styles
    tabbarElement?.removeAttribute("floorp-tabbar-display-style");
    tabbarElement?.removeAttribute("hidden");

    // Move elements to default positions
    if (tabbarElement && windowManageContainer) {
      tabbarElement.appendChild(windowManageContainer);
    }

    if (tabbarElement && navigatorToolbox) {
      const menubar = document?.getElementById("toolbar-menubar") as
        | XULElement
        | null;
      if (menubar && menubar.parentNode === navigatorToolbox) {
        menubar.after(tabbarElement);
      } else {
        navigatorToolbox.prepend(tabbarElement);
      }
    }

    // Clean up
    tabbarModifyCss?.remove();
    tabbarElement?.removeAttribute("floorp-tabbar-display-style");
    urlbarContainer?.style.removeProperty("margin-top");
  }

  export function defaultTabbarStyle() {
    getNavigatorToolboxtabbarElement()?.setAttribute(
      "floorp-tabbar-display-style",
      "0",
    );
  }

  export function hideHorizontalTabbar() {
    getTabbarElement()?.setAttribute("floorp-tabbar-display-style", "1");
  }

  export function optimiseToVerticalTabbar() {
    const tabbarElement = getTabbarElement();
    const navbarElement = getNavbarElement();
    const windowManageContainer = document?.querySelector(
      "#floorp-tabbar-window-manage-container",
    ) as XULElement;

    tabbarElement?.setAttribute("hidden", "true");
    navbarElement?.appendChild(windowManageContainer);
    checkPaddingEnabled();
  }

  export function bottomOfNavigationToolbar() {
    const navigatorToolbox = getNavigatorToolboxtabbarElement();
    const tabbarElement = getTabbarElement();
    const panelUIMenuButton = getPanelUIMenuButton();

    if (navigatorToolbox && tabbarElement) {
      navigatorToolbox.appendChild(tabbarElement);
    }

    if (panelUIMenuButton) {
      panelUIMenuButton.after(
        document?.querySelector(
          "#floorp-tabbar-window-manage-container",
        ) as Node,
      );
    }

    tabbarElement?.setAttribute("floorp-tabbar-display-style", "2");
  }

  export function bottomOfWindow() {
    const browserElement = getBrowserElement();
    const tabbarElement = getTabbarElement();
    const panelUIMenuButton = getPanelUIMenuButton();

    if (browserElement && tabbarElement) {
      browserElement.after(tabbarElement);
    }

    if (panelUIMenuButton) {
      panelUIMenuButton.after(
        document?.querySelector(
          "#floorp-tabbar-window-manage-container",
        ) as Node,
      );
    }

    tabbarElement?.removeAttribute("flex");
    tabbarElement?.setAttribute("floorp-tabbar-display-style", "3");
    getUrlbarContainer()?.style.setProperty("margin-top", "5px");
  }

  function makeSidebarVerticalTabDrag() {
    const sidebarVerticalTab = getSidebarVerticalTab();
    if (sidebarVerticalTab) {
      sidebarVerticalTab.classList.add("browser-titlebar");
    }
  }

  export function applyTabbarStyle() {
    revertToDefaultStyle();
    makeSidebarVerticalTabDrag();
    render(
      () =>
        TabbarStyleModifyCSSElement({ style: config().tabbar.tabbarPosition }),
      document?.head,
      {
        // biome-ignore lint/suspicious/noExplicitAny: <explanation>
        hotCtx: import.meta.hot,
      },
    );

    switch (config().tabbar.tabbarPosition) {
      case "hide-horizontal-tabbar":
        hideHorizontalTabbar();
        break;
      case "optimise-to-vertical-tabbar":
        optimiseToVerticalTabbar();
        break;
      case "bottom-of-navigation-toolbar":
        bottomOfNavigationToolbar();
        break;
      case "bottom-of-window":
        bottomOfWindow();
        break;
      default:
        defaultTabbarStyle();
        break;
    }
  }
}
