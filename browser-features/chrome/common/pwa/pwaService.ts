/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { SiteSpecificBrowserManager } from "./ssbManager";
import type { ManifestProcesser } from "./manifestProcesser";
import type { DataManager } from "./dataStore";
import type { Browser } from "./type";

export class PwaService {
  constructor(
    private ssbManager: SiteSpecificBrowserManager,
    private manifestProcesser: ManifestProcesser,
    private dataManager: DataManager,
  ) {}

  async installOrRunCurrentPageAsSsb(browser: Browser, asPwa = true) {
    return this.ssbManager.installOrRunCurrentPageAsSsb(browser, asPwa);
  }

  async checkCurrentPageIsInstalled(browser: Browser): Promise<boolean> {
    return this.ssbManager.checkCurrentPageIsInstalled(browser);
  }

  async getIcon(browser: Browser) {
    return this.ssbManager.getIcon(browser);
  }

  async getManifest(browser: Browser) {
    return this.ssbManager.getManifest(browser);
  }

  async checkBrowserCanBeInstallAsPwa(browser: Browser) {
    return this.manifestProcesser.checkBrowserCanBeInstallAsPwa(browser);
  }

  async getInstalledApps() {
    return this.ssbManager.getInstalledApps();
  }

  async renameSsb(id: string, newName: string): Promise<boolean> {
    return this.ssbManager.renameSsb(id, newName);
  }

  async uninstallById(id: string) {
    return this.ssbManager.uninstallById(id);
  }

  updateUIElements(isInstalled: boolean) {
    this.ssbManager.updateUIElements(isInstalled);
  }

  useOSIntegration() {
    return this.ssbManager.useOSIntegration();
  }

  closePopup() {
    this.ssbManager.closePopup();
  }

  async getSsbObj(id: string) {
    return this.ssbManager.getSsbObj(id);
  }

  async runSsbByUrl(url: string) {
    return this.ssbManager.runSsbByUrl(url);
  }
}
