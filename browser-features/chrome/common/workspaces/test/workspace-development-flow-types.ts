// SPDX-License-Identifier: MPL-2.0
import type { TestWindow } from "./workspace-last-tab-test-types.ts";

export type WorkspaceFlowWindow = TestWindow & {
  TabContextMenu: { contextTab: XULElement };
};

export type WorkspaceFlowPopup = XULElement & {
  state: string;
  openPopup(
    anchor: Element,
    position: string,
    x: number,
    y: number,
    isContextMenu: boolean,
    attributesOverride: boolean,
  ): void;
  hidePopup(): void;
  activateItem(item: Element): void;
};
