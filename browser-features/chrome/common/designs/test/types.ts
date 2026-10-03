// SPDX-License-Identifier: MPL-2.0

export interface AppearanceTestBrowser extends GBrowser {
  addTabGroup(tabs: XULElement[], options: { label: string }): XULElement;
}

export type MenuIconPopup = Element & {
  state: string;
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
