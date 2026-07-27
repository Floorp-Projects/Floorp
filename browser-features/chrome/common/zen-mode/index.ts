/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { createRoot, onCleanup } from "solid-js";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { BrowserActionUtils } from "../../utils/browser-action.tsx";
import {
  attachZenModeToWindow,
  destroyZenModeForWindow,
  toggleZenModeForWindow,
  ZenModeMenuElement,
} from "./zen-mode.tsx";
import { addI18nObserver } from "#i18n/config-browser-chrome.ts";
import i18next from "i18next";
import iconStyle from "./icon.css?inline";

const TOOLBAR_BUTTON_ID = "zen-mode-button";
const TOOLBAR_TOOLTIP_ID = "zen-mode-button-tooltip";
const TOOLBAR_ICON_STYLE_ID = "floorp-zen-mode-icon-style";

@noraComponent(import.meta.hot)
export default class ZenMode extends NoraComponentBase {
  init() {
    this.logger.info("Initializing Zen Mode");

    const targetDocument = typeof document === "undefined" ? null : document;
    const targetWindow = targetDocument?.defaultView as Window | null;
    const targetRoot = targetDocument?.documentElement;
    if (!targetDocument || !targetWindow || !targetRoot) {
      this.logger.warn("Document is unavailable; skip initializing Zen Mode.");
      return;
    }

    const controller = attachZenModeToWindow(targetWindow);
    if (!controller) {
      this.logger.error("Failed to create the Zen Mode window controller.");
      return;
    }

    let disposed = false;
    let domReadyListenerAttached = false;
    let menuDispose: (() => void) | null = null;
    let uiObserver: MutationObserver | null = null;
    let iconStyleElement: HTMLStyleElement | null = null;
    let toolbarNode: Element | null = null;
    let tooltipElement: Element | null = null;
    let buttonLabel = "Zen Mode";
    let tooltipText = "Toggle Zen Mode";

    const ensureIconStyle = () => {
      if (iconStyleElement?.isConnected) {
        return;
      }

      const existing = targetDocument.getElementById(TOOLBAR_ICON_STYLE_ID);
      if (existing?.localName === "style") {
        iconStyleElement = existing as HTMLStyleElement;
        iconStyleElement.textContent = iconStyle;
        return;
      }

      const style = targetDocument.createElement("style");
      style.id = TOOLBAR_ICON_STYLE_ID;
      style.setAttribute("data-floorp-zen-mode-owned", "true");
      style.textContent = iconStyle;
      (targetDocument.head ?? targetRoot).appendChild(style);
      iconStyleElement = style;
    };

    const syncToolbarPresentation = () => {
      if (disposed) {
        return;
      }

      const currentNode = targetDocument.getElementById(TOOLBAR_BUTTON_ID);
      if (toolbarNode !== currentNode) {
        if (toolbarNode?.getAttribute("tooltip") === TOOLBAR_TOOLTIP_ID) {
          toolbarNode.removeAttribute("tooltip");
        }
        toolbarNode = currentNode;
      }

      toolbarNode?.setAttribute("label", buttonLabel);

      const popupSet = targetDocument.getElementById("mainPopupSet");
      if (!popupSet || !toolbarNode) {
        return;
      }

      let tooltip = targetDocument.getElementById(TOOLBAR_TOOLTIP_ID);
      if (!tooltip) {
        tooltip = targetDocument.createXULElement("tooltip");
        tooltip.id = TOOLBAR_TOOLTIP_ID;
        tooltip.setAttribute("hasbeenopened", "false");
        popupSet.appendChild(tooltip);
      }
      tooltip.setAttribute("data-floorp-zen-mode-owned", "true");
      tooltip.setAttribute("label", tooltipText);
      toolbarNode.setAttribute("tooltip", TOOLBAR_TOOLTIP_ID);
      tooltipElement = tooltip;
    };

    const findToolbarButton = (target: EventTarget | null): Element | null => {
      let node = target as Node | null;
      while (node && node !== targetDocument) {
        if (node.nodeType === Node.ELEMENT_NODE) {
          const element = node as Element;
          if (element.id === TOOLBAR_BUTTON_ID) {
            return element.ownerDocument === targetDocument ? element : null;
          }
        }
        node = node.parentNode;
      }
      return null;
    };

    const handleToolbarCommand = (event: Event) => {
      const button = findToolbarButton(event.target);
      if (!button) {
        return;
      }

      const owningWindow = button.ownerDocument?.defaultView as Window | null;
      if (owningWindow) {
        toggleZenModeForWindow(owningWindow);
      }
    };

    const injectMenu = () => {
      const menuPopup = targetDocument.getElementById("menu_ToolsPopup");
      if (!menuPopup) {
        this.logger.warn(
          "Failed to locate #menu_ToolsPopup; Zen Mode menu item will not be injected.",
        );
        return;
      }

      targetDocument.getElementById("toggle_zenmode")?.remove();
      const marker = targetDocument.getElementById("menu_openFirefoxView");

      try {
        menuDispose = createRoot((dispose) => {
          render(
            () => ZenModeMenuElement({ targetWindow }),
            menuPopup,
            {
              marker: marker?.parentElement === menuPopup ? marker : undefined,
            },
          );
          return dispose;
        });
        this.logger.info("Zen Mode menu item rendered successfully.");
      } catch (error) {
        const reason = error instanceof Error
          ? error
          : new Error(String(error));
        this.logger.error("Failed to render Zen Mode menu item", reason);
      }
    };

    const createToolbarButton = () => {
      ensureIconStyle();

      // BrowserActionUtils does not forward the command event. Its callback is
      // intentionally inert; the owning document listener above performs the
      // window-stable routing for every widget instance and across HMR.
      BrowserActionUtils.createToolbarClickActionButton(
        TOOLBAR_BUTTON_ID,
        null,
        () => {},
        null,
        null,
        null,
        null,
      );
      syncToolbarPresentation();
    };

    const tryInit = () => {
      if (disposed) {
        return;
      }
      domReadyListenerAttached = false;
      injectMenu();
      createToolbarButton();
    };

    targetDocument.addEventListener("command", handleToolbarCommand, true);

    const MutationObserverConstructor = targetWindow.MutationObserver;
    if (typeof MutationObserverConstructor === "function") {
      const observer = new MutationObserverConstructor(() => {
        syncToolbarPresentation();
      });
      observer.observe(targetRoot, {
        childList: true,
        subtree: true,
      });
      uiObserver = observer;
    }

    const disposeI18n = createRoot((dispose) => {
      addI18nObserver(() => {
        buttonLabel = i18next.t("zen-mode.label", {
          defaultValue: "Zen Mode",
        });
        tooltipText = i18next.t("zen-mode.tooltiptext", {
          defaultValue: "Toggle Zen Mode",
        });
        syncToolbarPresentation();
      });
      return dispose;
    });

    if (targetDocument.readyState === "loading") {
      domReadyListenerAttached = true;
      targetDocument.addEventListener("DOMContentLoaded", tryInit, {
        once: true,
      });
    } else {
      tryInit();
    }

    const cleanup = () => {
      if (disposed) {
        return;
      }
      disposed = true;

      if (domReadyListenerAttached) {
        targetDocument.removeEventListener("DOMContentLoaded", tryInit);
        domReadyListenerAttached = false;
      }
      targetWindow.removeEventListener("unload", cleanup);
      targetDocument.removeEventListener("command", handleToolbarCommand, true);
      uiObserver?.disconnect();
      uiObserver = null;

      menuDispose?.();
      menuDispose = null;
      disposeI18n();

      if (toolbarNode?.getAttribute("tooltip") === TOOLBAR_TOOLTIP_ID) {
        toolbarNode.removeAttribute("tooltip");
      }
      toolbarNode = null;

      tooltipElement?.remove();
      tooltipElement = null;
      targetDocument.getElementById(TOOLBAR_TOOLTIP_ID)?.remove();

      iconStyleElement?.remove();
      iconStyleElement = null;
      targetDocument.getElementById(TOOLBAR_ICON_STYLE_ID)?.remove();

      destroyZenModeForWindow(targetWindow, controller);
    };

    targetWindow.addEventListener("unload", cleanup, { once: true });
    onCleanup(cleanup);
  }
}
