// SPDX-License-Identifier: MPL-2.0

type MenuPlacement =
  | string
  | string[]
  | { selector: string; optional?: boolean };

export type NativeMenuLayout = Record<string, {
  name: string;
  items: MenuPlacement[];
  open?: boolean;
}[]>;

export type NativeMenuMode = "classic" | "altstructure";

export interface NativeTabContextMenu {
  MENU_SECTIONS: Record<NativeMenuMode, NativeMenuLayout>;
  DYNAMIC_MENU_ITEM_SELECTORS: string[];
  _updateL10nIds(popup: Element, alternate: boolean): void;
}

export interface NativeMenuLayoutModule {
  MenuSectionLayout: new (
    layout: NativeMenuLayout,
    options: { dynamicItemSelectors: string[] },
  ) => { arrange(popup: Element): void };
}

export type NativeMenuL10nElement = Element & { _classicL10nId?: string };
