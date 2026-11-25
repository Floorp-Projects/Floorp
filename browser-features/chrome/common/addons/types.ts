/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { NotificationCustomizer } from "./notification-customizer.ts";

export interface InstallInfo {
  installs: Array<{
    addon?: { name?: string };
  }>;
}

export interface XPInstallObserver {
  showInstallConfirmation: (
    browser: unknown,
    installInfo: InstallInfo,
    height?: number,
  ) => void;
}

export interface ChromeWebStoreWindow extends Window {
  __chromeWebStoreInstallInfo?: ChromeWebStoreInstallInfo;
  __floorpAddonsContext?: AddonsContext;
  gXPInstallObserver?: XPInstallObserver;
}

export interface ChromeWebStoreInstallInfo {
  name: string;
  id: string;
}

export interface CWSObserver {
  observe: (subject: unknown, topic: string, data: unknown) => void;
  dispose?: () => void;
}

export interface InstallInfoState {
  pendingChromeWebStoreInstall: ChromeWebStoreInstallInfo | null;
}

export interface AddonsLogger {
  debug: (...args: unknown[]) => void;
  warn?: (...args: unknown[]) => void;
  info?: (...args: unknown[]) => void;
}

export interface AddonsContext {
  state: InstallInfoState;
  customizer: NotificationCustomizer | null;
  logger: AddonsLogger | null;
  observer: CWSObserver | null;
  savedDescriptionChildren: Node[] | null;
  animationFrames: Set<number>;
  disposed: boolean;
}

export interface AddonPermissionTarget {
  documentGlobal?: {
    docShell?: {
      itemType: number;
      chromeEventHandler?: AddonPermissionTarget;
    };
    browsingContext?: { topChromeWindow?: ChromeWebStoreWindow };
  };
  getAttribute?: (name: string) => string | null;
}
