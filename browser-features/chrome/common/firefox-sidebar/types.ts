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

export type SidebarOpeningRequest = {
  token: number;
  generation: number;
  commandID: string;
  launcherWasHidden: boolean;
  hoverOwned: boolean;
  cancelled: boolean;
  cancel(): void;
};

export type NativeSidebarBrowser = HTMLElement & {
  contentWindow: Window | null;
};
