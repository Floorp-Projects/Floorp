/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// NOTICE: Do not add toolbar buttons code here. Create new folder or file for new toolbar buttons.

import { render } from "@nora/solid-xul";
import type {
  TCustomizableUI,
  TCustomizableUIArea,
} from "@types-gecko/CustomizableUI";
import type { JSXElement } from "solid-js";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
) as { CustomizableUI: typeof TCustomizableUI };

export namespace BrowserActionUtils {
  export function createToolbarClickActionButton(
    widgetId: string,
    l10nId: string,
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
        hotCtx: import.meta.hot,
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
        tooltiptext: (await document?.l10n?.formatValue(l10nId)) ?? "",
        label: (await document?.l10n?.formatValue(l10nId)) ?? "",
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
    popupElem: JSXElement,
    onCommandFunc: () => void,
    area: string = CustomizableUI.AREA_NAVBAR,
    styleElement: JSXElement | null = null,
    position: number | null = 0,
    onCreatedFunc: null | ((aNode: XULElement) => void) = null,
  ) {
    if (styleElement) {
      render(() => styleElement, document?.head, {
        hotCtx: import.meta.hot,
        marker: document?.head?.lastChild as Element,
      });
    }

    const widget = CustomizableUI.getWidget(widgetId);
    if (widget && widget.type !== "custom") {
      return;
    }

    (async () => {
      CustomizableUI.createWidget({
        id: widgetId,
        type: "button",
        tooltiptext: (await document?.l10n?.formatValue(l10nId)) ?? "",
        label: (await document?.l10n?.formatValue(l10nId)) ?? "",
        removable: true,
        onCommand: () => {
          onCommandFunc?.();
        },
        onCreated: (aNode: XULElement) => {
          aNode.setAttribute("type", "menu");
          render(() => popupElem, aNode, {
            hotCtx: import.meta.hot,
            marker: aNode.lastChild as Element,
          });
          onCreatedFunc?.(aNode);
        },
      });
      CustomizableUI.addWidgetToArea(
        widgetId,
        area as TCustomizableUIArea,
        position ?? 0,
      );
    })();
  }
}
