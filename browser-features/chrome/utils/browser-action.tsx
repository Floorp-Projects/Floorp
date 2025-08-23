/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTICE: Do not add toolbar buttons code here. Create new folder or file for new toolbar buttons.

import { render } from "@nora/solid-xul";
import { createRoot, getOwner, type JSXElement } from "solid-js";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export namespace BrowserActionUtils {
  export function createToolbarClickActionButton(
    widgetId: string,
    l10nId: string | null,
    onCommandFunc: () => void,
    styleElement: JSXElement | null = null,
    area: TCustomizableUIArea = CustomizableUI.AREA_NAVBAR,
    position: number | null = 0,
    onCreatedFunc: null | ((aNode: XULElement) => void) = null,
  ) {
    // Add style Element for toolbar button icon.
    // This render is runnning every open browser window.
    if (styleElement) {
      render(() => styleElement, document?.head, {
        marker: document?.head?.lastChild as Element,
      });
    }

    // Create toolbar button. If widget already exists, return.
    // custom type is temporary widget type. It will be changed to button type.
    const widget = CustomizableUI.getWidget(widgetId);
    if (widget && widget.type !== "custom") {
      return;
    }
    (async () => {
      CustomizableUI.createWidget({
        id: widgetId,
        type: "button",
        tooltiptext: l10nId ? await document.l10n?.formatValue(l10nId) : null,
        label: l10nId ? await document.l10n?.formatValue(l10nId) : null,
        removable: true,
        onCommand: () => {
          onCommandFunc?.();
        },
        onCreated: (aNode: XULElement) => {
          onCreatedFunc?.(aNode);
        },
      });
      CustomizableUI.addWidgetToArea(widgetId, area, position ?? 0);
    })();
  }

  export function createMenuToolbarButton(
    widgetId: string,
    l10nId: string,
    targetViewId: string,
    popupElement: JSXElement,
    onViewShowingFunc?: ((event: Event) => void) | null,
    onCreatedFunc?: ((aNode: XULElement) => void) | null,
    area: string = CustomizableUI.AREA_NAVBAR,
    styleElement: JSXElement | null = null,
    position: number | null = 0,
  ) {
    if (styleElement) {
      render(() => styleElement, document?.head, {
        marker: document?.head?.lastChild as Element,
      });
    }

    if (popupElement) {
      render(() => popupElement, document?.getElementById("mainPopupSet"), {
        marker: document?.getElementById("mainPopupSet")?.lastChild as Element,
      });
    }

    const widget = CustomizableUI.getWidget(widgetId);
    if (widget && widget.type !== "custom") {
      return;
    }

    const owner = getOwner();
    CustomizableUI.createWidget({
      id: widgetId,
      type: "view",
      viewId: targetViewId,
      tooltiptext: document?.l10n?.formatValue(l10nId) ?? "",
      label: document?.l10n?.formatValue(l10nId) ?? "",
      removable: true,
      onCreated: (aNode: XULElement) => {
        createRoot(() => onCreatedFunc?.(aNode), owner);
      },
      onViewShowing: (event: Event) => {
        createRoot(() => onViewShowingFunc?.(event), owner);
      },
    });
    CustomizableUI.addWidgetToArea(widgetId, area, position ?? 0);
  }
}
