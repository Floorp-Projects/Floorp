/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
}

export interface ChromeWebStoreInstallInfo {
  name: string;
  id: string;
}

export interface CWSObserver {
  observe: (subject: unknown, topic: string, data: unknown) => void;
}
