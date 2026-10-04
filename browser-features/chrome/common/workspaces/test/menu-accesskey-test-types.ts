// SPDX-License-Identifier: MPL-2.0

export type TabContextMenuPopup = XULElement & {
  openPopup(
    anchor: Element | null,
    position: string,
    x: number,
    y: number,
    isContextMenu: boolean,
    attributesOverride: boolean,
  ): void;
  hidePopup(): void;
};
