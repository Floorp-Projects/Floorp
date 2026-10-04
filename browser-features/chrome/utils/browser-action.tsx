// SPDX-License-Identifier: MPL-2.0

// NOTICE: Do not add toolbar buttons code here. Create new folder or file for new toolbar buttons.

import { safeRender } from "@nora/preact-xul";
import { addDisposer } from "@nora/preact-xul/lifetime";
import type { ComponentChild } from "preact";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

// deno-lint-ignore no-namespace
export namespace BrowserActionUtils {
  const mounts = new Map<string, () => void>();
  function mount(key: string, content: ComponentChild, parent: Element) {
    mounts.get(key)?.();
    const dispose = safeRender(content, parent);
    mounts.set(key, dispose);
    addDisposer(() => {
      dispose();
      if (mounts.get(key) === dispose) mounts.delete(key);
    });
  }
  export function createToolbarClickActionButton(
    widgetId: string,
    l10nId: string | null,
    onCommandFunc: () => void,
    styleElement: ComponentChild | null = null,
    area: TCustomizableUIArea | null = CustomizableUI.AREA_NAVBAR,
    position: number | null = 0,
    onCreatedFunc: null | ((aNode: XULElement) => void) = null,
  ) {
    if (styleElement && document?.head) {
      mount(`${widgetId}-style`, styleElement, document.head);
    }

    // Create toolbar button only once per profile. Subsequent window opens reuse
    // the existing widget and respect saved customization data.
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
          // Note: no reactive owner needed in preact/signals — effects are self-contained.
          // Callers that set up reactive effects inside onCreatedFunc should use
          // createNodeDisposer(aNode, dispose) to tie cleanup to the node's lifetime.
          onCreatedFunc?.(aNode);
        },
        defaultArea: area ?? undefined,
      });

      if (!area) {
        return;
      }

      try {
        await (CustomizableUI.readyPromise ?? Promise.resolve());
      } catch (error) {
        console.error(
          "Failed waiting for CustomizableUI readiness for",
          widgetId,
          error,
        );
        return;
      }

      const hasUserCustomized = typeof Services !== "undefined" &&
        Services.prefs?.prefHasUserValue?.("browser.uiCustomization.state");

      if (hasUserCustomized) {
        return;
      }

      if (position === null || position === undefined) {
        CustomizableUI.addWidgetToArea(widgetId, area);
      } else {
        CustomizableUI.addWidgetToArea(widgetId, area, position);
      }
    })();
  }

  export function createMenuToolbarButton(
    widgetId: string,
    l10nId: string,
    targetViewId: string,
    popupElement: ComponentChild,
    onViewShowingFunc?: ((event: Event) => void) | null,
    onCreatedFunc?: ((aNode: XULElement) => void) | null,
    area: string = CustomizableUI.AREA_NAVBAR,
    styleElement: ComponentChild | null = null,
    position: number | null = 0,
  ) {
    if (styleElement && document?.head) {
      mount(`${widgetId}-style`, styleElement, document.head);
    }
    const popupSet = document?.getElementById("mainPopupSet");
    if (popupElement && popupSet) {
      mount(`${widgetId}-popup`, popupElement, popupSet);
    }

    const widget = CustomizableUI.getWidget(widgetId);
    if (widget && widget.type !== "custom") {
      return;
    }

    // Note: solid-js required getOwner()/createRoot(fn, owner) to pass reactive
    // context through the async CustomizableUI.createWidget callback boundary.
    // In preact/signals, effects are independently tracked — no owner needed.
    (async () => {
      CustomizableUI.createWidget({
        id: widgetId,
        type: "view",
        viewId: targetViewId,
        tooltiptext: document?.l10n?.formatValue(l10nId) ?? "",
        label: document?.l10n?.formatValue(l10nId) ?? "",
        removable: true,
        onCreated: (aNode: XULElement) => {
          onCreatedFunc?.(aNode);
        },
        onViewShowing: (event: Event) => {
          onViewShowingFunc?.(event);
        },
        defaultArea: area,
      });

      try {
        await (CustomizableUI.readyPromise ?? Promise.resolve());
      } catch (error) {
        console.error(
          "Failed waiting for CustomizableUI readiness for",
          widgetId,
          error,
        );
        return;
      }

      const hasUserCustomized = typeof Services !== "undefined" &&
        Services.prefs?.prefHasUserValue?.("browser.uiCustomization.state");

      if (hasUserCustomized) {
        return;
      }

      if (position === null || position === undefined) {
        CustomizableUI.addWidgetToArea(widgetId, area);
      } else {
        CustomizableUI.addWidgetToArea(widgetId, area, position);
      }
    })();
  }
}
