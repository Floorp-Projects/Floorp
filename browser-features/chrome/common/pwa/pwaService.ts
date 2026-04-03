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

  installOrRunCurrentPageAsSsb(browser: Browser, asPwa = true) {
    return this.ssbManager.installOrRunCurrentPageAsSsb(browser, asPwa);
  }

  checkCurrentPageIsInstalled(browser: Browser) {
    return this.ssbManager.checkCurrentPageIsInstalled(browser);
  }

  getIcon(browser: Browser) {
    return this.ssbManager.getIcon(browser);
  }

  getManifest(browser: Browser) {
    return this.ssbManager.getManifest(browser);
  }

  checkBrowserCanBeInstallAsPwa(browser: Browser) {
    return this.manifestProcesser.checkBrowserCanBeInstallAsPwa(browser);
  }

  getInstalledApps() {
    return this.ssbManager.getInstalledApps();
  }

  renameSsb(id: string, newName: string) {
    return this.ssbManager.renameSsb(id, newName);
  }

  uninstallById(id: string) {
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

  getSsbObj(id: string) {
    return this.ssbManager.getSsbObj(id);
  }

  runSsbByUrl(url: string) {
    return this.ssbManager.runSsbByUrl(url);
  }

  saveSsbData(manifest: import("./type").Manifest) {
    return this.dataManager.saveSsbData(manifest);
  }
}
