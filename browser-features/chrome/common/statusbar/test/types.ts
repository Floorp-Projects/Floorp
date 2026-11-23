// SPDX-License-Identifier: MPL-2.0

export interface StatusbarTestCustomizableUI {
  areas: string[];
  getAreaType(area: string): string | null;
  getWidgetIdsInArea(area: string): string[];
}

export interface StatusbarTestWindow extends Window {
  CustomizableUI: StatusbarTestCustomizableUI;
  OpenBrowserWindow(): StatusbarTestWindow;
}

export type Fixture = {
  setEnabled: (enabled: boolean) => void;
  panel: HTMLElement;
  text: HTMLElement;
  label: HTMLElement;
  writes: () => number;
  dispose: () => void;
};
