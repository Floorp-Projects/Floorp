// SPDX-License-Identifier: MPL-2.0

const WORKSPACE_PANEL_VIEW = "#workspacesToolbarButtonPanel";
const WORKSPACE_BUTTON_SELECTOR = "#workspaces-toolbar-button";
const WIDGET_PANEL_SELECTOR = "#customizationui-widget-panel";
const WORKSPACE_VIEW_ID = "workspacesToolbarButtonPanel";

const { CustomizableUI } = ChromeUtils.importESModule(
  "moz-src:///browser/components/customizableui/CustomizableUI.sys.mjs",
);

interface PanelUIType {
  showSubView: (
    viewId: string,
    anchor: Element,
    event?: Event,
  ) => Promise<void>;
}

/**
 * ワークスペースパネルを開くヘルパー。
 * noautohide 等のパネル改変は行わない（ツアー終了後に閉じられなくなる副作用を避ける）。
 */
export class WorkspacePanelGuard {
  isVisible(): boolean {
    const el = document.querySelector(WORKSPACE_PANEL_VIEW);
    if (!el) return false;
    const rect = el.getBoundingClientRect();
    return rect.width > 0 && rect.height > 0;
  }

  /** 旧実装で残ったロック属性・noautohide を解除する */
  static cleanupStale(): void {
    const panel = document.querySelector(WIDGET_PANEL_SELECTOR) as
      | XULElement
      | null;
    if (!panel) return;

    const hadModification = panel.hasAttribute(
      "data-floorp-guided-tour-panel-lock",
    ) || panel.hasAttribute("noautohide");

    panel.removeAttribute("data-floorp-guided-tour-panel-lock");
    panel.removeAttribute("noautohide");

    if (hadModification) {
      try {
        CustomizableUI.addPanelCloseListeners(panel);
      } catch (error) {
        console.error("[GuidedTour] stale cleanup failed:", error);
      }
    }
  }

  async ensureOpen(): Promise<void> {
    if (this.isVisible()) return;

    const button = document.querySelector(WORKSPACE_BUTTON_SELECTOR);
    if (!button) return;

    const panelUI = (globalThis as typeof globalThis & { PanelUI?: PanelUIType })
      .PanelUI;
    if (panelUI?.showSubView) {
      try {
        await panelUI.showSubView(WORKSPACE_VIEW_ID, button);
        return;
      } catch (error) {
        console.error("[GuidedTour] PanelUI.showSubView failed:", error);
      }
    }

    if (button.getAttribute("open") !== "true") {
      (button as HTMLElement).click();
    }
  }
}
