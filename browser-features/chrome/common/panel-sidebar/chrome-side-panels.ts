/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type {
  ChromeSidePanelManager,
  ChromeSidePanelWindow,
} from "./utils/type.ts";

const { ExtensionUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/ExtensionUtils.sys.mjs",
) as unknown as {
  ExtensionUtils: { getChromeExtensionId(extension: unknown): string | null };
};
const { WebExtensionPolicy } = Cu.getGlobalForObject(Services);

export function getChromeSidePanelManager(
  extensionId: string,
): ChromeSidePanelManager | null {
  const extension = WebExtensionPolicy.getByID(extensionId)?.extension;
  if (!extension || !ExtensionUtils.getChromeExtensionId(extension)) {
    return null;
  }
  return extension.apiManager.getAPI(
    "sidePanel",
    extension,
  ) as ChromeSidePanelManager;
}

export function toggleChromeSidePanel(
  extensionId: string,
  win: Window = globalThis as unknown as Window,
): boolean {
  const manager = getChromeSidePanelManager(extensionId);
  if (!manager) return false;
  manager.openForAction(
    (win as unknown as ChromeSidePanelWindow).gBrowser.selectedTab,
  );
  return true;
}

export function toggleLastChromeSidePanel(win: Window): void {
  const id =
    (win as unknown as ChromeSidePanelWindow).gFloorpLastChromeSidePanelId;
  if (id) toggleChromeSidePanel(id, win);
}

export function getActiveChromeSidePanelId(win: Window): string | null {
  const sidebar = (win as unknown as ChromeSidePanelWindow).SidebarController;
  const current = sidebar?.isOpen
    ? sidebar.sidebars.get(sidebar.currentID)
    : null;
  return current?.floorpChromeSidePanel ? current.extensionId ?? null : null;
}
