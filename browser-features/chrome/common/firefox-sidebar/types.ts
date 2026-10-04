// SPDX-License-Identifier: MPL-2.0

export type NativeSidebarController = {
  promiseInitialized: Promise<void>;
  isOpen: boolean;
  currentID: string;
  lastOpenedId?: string | null;
  inSingleTabWindow?: boolean;
  sidebars: Map<string, { visible?: boolean }>;
  showInitially(commandID: string): Promise<boolean>;
  hide(options?: { dismissPanel?: boolean }): void;
  waitUntilStable(): Promise<unknown>;
  setPosition(): void;
  handleToolbarButtonClick?(): Promise<void>;
};

export type SidebarOverlaySettings = {
  overlay: boolean;
  hover: boolean;
};
