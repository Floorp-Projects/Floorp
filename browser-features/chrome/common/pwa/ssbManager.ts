/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { ManifestProcesser } from "./manifestProcesser.ts";
import type { DataManager } from "./dataStore.ts";
import type { Browser, Manifest } from "./type.ts";
import { SsbRunner } from "./ssbRunner.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
const { TaskbarExperiment } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs",
);

let SupportClass: any | null = null;
if (AppConstants.platform === "win") {
  const { WindowsSupport } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/supports/Windows.sys.mjs",
  );
  SupportClass = WindowsSupport;
} else if (AppConstants.platform === "linux") {
  const { LinuxSupport } = ChromeUtils.importESModule(
    "resource://noraneko/modules/pwa/supports/Linux.sys.mjs",
  );
  SupportClass = LinuxSupport;
}

export class SiteSpecificBrowserManager {
  private ssbRunner: SsbRunner;
  static instance: SiteSpecificBrowserManager | null = null;

  constructor(
    private manifestProcesser: ManifestProcesser,
    public readonly dataManager: DataManager,
  ) {
    this.ssbRunner = new SsbRunner(dataManager, this);
    SiteSpecificBrowserManager.instance = this;

    window.gBrowser.addProgressListener(this.listener);

    Services.obs.addObserver(async (subject: any) => {
      await this.renameSsb(
        subject?.wrappedJSObject?.id as string,
        subject?.wrappedJSObject?.newName as string,
      );
    }, "nora-ssb-rename");

    Services.obs.addObserver(async (subject: any) => {
      await this.uninstallById(subject?.wrappedJSObject?.id as string);
    }, "nora-ssb-uninstall");
  }

  private listener = {
    onLocationChange: () => {
      this.onCurrentTabChangedOrLoaded();
    },
  };

  async onCommand() {
    this.installOrRunCurrentPageAsSsb(window.gBrowser.selectedBrowser, true);
  }

  closePopup() {
    const panel = document?.getElementById("ssb-panel") as XULElement & {
      hidePopup: () => void;
    };
    if (panel) {
      panel.hidePopup();
    }
  }

  public async getIcon(browser: Browser) {
    const currentTabSsb = await this.getCurrentTabSsb(browser);

    if (!currentTabSsb) {
      return "";
    }

    return currentTabSsb.icon;
  }

  public async getManifest(browser: Browser) {
    const currentTabSsb = await this.getCurrentTabSsb(browser);
    return currentTabSsb;
  }

  public async installOrRunCurrentPageAsSsb(browser: Browser, asPwa = true) {
    const isInstalled = await this.checkCurrentPageIsInstalled(browser);

    if (isInstalled) {
      const currentTabSsb = await this.getCurrentTabSsb(browser);

      if (!currentTabSsb) {
        return;
      }

      const ssbObj = await this.getIdByUrl(currentTabSsb.start_url);

      if (ssbObj) {
        await this.runSsbByUrl(window.gBrowser.selectedBrowser.currentURI.spec);
      }
    } else {
      const manifest = await this.createFromBrowser(browser, {
        useWebManifest: asPwa,
      });

      if (!manifest) {
        return;
      }

      await this.install(manifest);

      // Installing needs some time to finish
      window.setTimeout(() => {
        this.runSsbByUrl(manifest.start_url);
      }, 3000);
    }
  }

  public async checkCurrentPageIsInstalled(browser: Browser): Promise<boolean> {
    if (!this.checkSiteCanBeInstall(browser.currentURI)) {
      return false;
    }

    const currentTabSsb = await this.getCurrentTabSsb(browser);
    const ssbData = await this.dataManager.getCurrentSsbData();

    if (!currentTabSsb) {
      return false;
    }

    for (const key in ssbData) {
      if (
        key === currentTabSsb.start_url ||
        currentTabSsb.start_url.startsWith(key)
      ) {
        return true;
      }
    }
    return false;
  }

  private checkSiteCanBeInstall(uri: nsIURI): boolean {
    if (uri.scheme === "chrome" || uri.scheme === "about") {
      return false;
    }

    return (
      uri.scheme === "https" ||
      uri.scheme === "http" ||
      uri.host === "localhost" ||
      uri.host === "127.0.0.1"
    );
  }

  private async getCurrentTabSsb(browser: Browser) {
    return await this.manifestProcesser.getManifestFromBrowser(browser, true);
  }

  private async createFromBrowser(
    browser: Browser,
    options: { useWebManifest: boolean },
  ): Promise<Manifest | null> {
    return await this.manifestProcesser.getManifestFromBrowser(
      browser,
      options.useWebManifest,
    );
  }

  private async install(manifest: Manifest) {
    if (SupportClass) {
      if (AppConstants.platform === "win") {
        // Windows install (taskbar integration) is controlled by A/B test
        if (TaskbarExperiment.isEnabledForCurrentPlatform()) {
          const windowsSupport = new SupportClass(this);
          await windowsSupport.install(manifest);
        } else {
          console.debug(
            "[SiteSpecificBrowserManager] PWA taskbar integration disabled by A/B test, skipping Windows install",
          );
        }
      } else if (AppConstants.platform === "linux") {
        // Linux install (.desktop file generation) is NOT controlled by A/B test
        // This is excluded from A/B test as per requirements
        const linuxSupport = new SupportClass(this);
        await linuxSupport.install(manifest);
      }
    }

    await this.dataManager.saveSsbData(manifest);
  }

  private async uninstall(manifest: Manifest) {
    if (SupportClass) {
      if (AppConstants.platform === "win") {
        // Windows uninstall (taskbar integration) is controlled by A/B test
        if (TaskbarExperiment.isEnabledForCurrentPlatform()) {
          const windowsSupport = new SupportClass(this);
          await windowsSupport.uninstall(manifest);
        } else {
          console.debug(
            "[SiteSpecificBrowserManager] PWA taskbar integration disabled by A/B test, skipping Windows uninstall",
          );
        }
      } else if (AppConstants.platform === "linux") {
        // Linux uninstall (.desktop file removal) is NOT controlled by A/B test
        // This is excluded from A/B test as per requirements
        const linuxSupport = new SupportClass(this);
        await linuxSupport.uninstall(manifest);
      }
    }

    await this.dataManager.removeSsbData(manifest.start_url);
  }

  public async uninstallById(id: string) {
    const ssbObj = await this.getSsbObj(id);
    if (!ssbObj) {
      return;
    }
    await this.uninstall(ssbObj);
  }

  private async getIdByUrl(url: string) {
    const ssbData = await this.dataManager.getCurrentSsbData();
    return ssbData[url];
  }

  public async getSsbObj(id: string) {
    const ssbData = await this.dataManager.getCurrentSsbData();
    for (const value of Object.values(ssbData)) {
      if ((value as Manifest).id === id) {
        return value as Manifest;
      }
    }
    return null;
  }

  public async runSsbByUrl(url: string) {
    await this.ssbRunner.runSsbByUrl(url);
  }

  private async onCurrentTabChangedOrLoaded() {
    const browser = window.gBrowser.selectedBrowser;
    const currentPageCanBeInstalled = this.checkSiteCanBeInstall(
      browser.currentURI,
    );
    const currentPageHasSsbManifest =
      await this.manifestProcesser.getManifestFromBrowser(browser, true);
    const currentPageIsInstalled =
      await this.checkCurrentPageIsInstalled(browser);

    if (
      (!currentPageCanBeInstalled || !currentPageHasSsbManifest) &&
      !currentPageIsInstalled
    ) {
      return;
    }

    // Update UI elements
    this.updateUIElements(currentPageIsInstalled);
    Services.obs.notifyObservers(
      {} as nsISupports,
      "nora-pwa-check-page-has-manifest",
    );
  }

  public updateUIElements(isInstalled: boolean) {
    const installButton = document?.getElementById("ssbPageAction");
    const image = document?.getElementById("ssbPageAction-image");

    if (installButton && image) {
      if (isInstalled) {
        image.setAttribute("open-ssb", "true");
      } else {
        image.removeAttribute("open-ssb");
      }
    }
  }

  public async getInstalledApps() {
    return await this.dataManager.getCurrentSsbData();
  }

  public async renameSsb(id: string, newName: string): Promise<boolean> {
    const ssbObj = await this.getSsbObj(id);
    if (!ssbObj) {
      return false;
    }

    const updatedManifest: Manifest = {
      ...ssbObj,
      name: newName,
      short_name: newName,
    };

    await this.uninstall(ssbObj);
    await this.install(updatedManifest);

    return true;
  }

  public useOSIntegration() {
    return true;
  }
}
