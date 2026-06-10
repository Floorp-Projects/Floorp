/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { ManifestProcesser } from "./manifestProcesser.ts";
import type { DataManager } from "./dataStore.ts";
import { DataManager as DataManagerClass } from "./dataStore.ts";
import type { Browser, Manifest } from "./type.ts";
import { SsbRunner } from "./ssbRunner.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);
const { TaskbarExperiment } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/TaskbarExperiment.sys.mjs",
);

// deno-lint-ignore no-explicit-any
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

    globalThis.gBrowser.addTabsProgressListener(this.listener);

    // deno-lint-ignore no-explicit-any
    Services.obs.addObserver(async (subject: any) => {
      await this.renameSsb(
        subject?.wrappedJSObject?.id as string,
        subject?.wrappedJSObject?.newName as string,
      );
    }, "nora-ssb-rename");

    // deno-lint-ignore no-explicit-any
    Services.obs.addObserver(async (subject: any) => {
      await this.uninstallById(subject?.wrappedJSObject?.id as string);
    }, "nora-ssb-uninstall");

    // deno-lint-ignore no-explicit-any
    Services.obs.addObserver(async (subject: any) => {
      try {
        const id = subject?.wrappedJSObject?.id as string;
        const userContextId = subject?.wrappedJSObject?.userContextId as number;
        await this.setContainerForSsb(id, userContextId);
      } catch (error) {
        console.error(
          "[SiteSpecificBrowserManager] Failed to set SSB container",
          error,
        );
      }
    }, "nora-ssb-set-container");
  }

  private listener = {
    onLocationChange: () => {
      this.onCurrentTabChangedOrLoaded();
    },
  };

  onCommand() {
    this.installOrRunCurrentPageAsSsb(
      globalThis.gBrowser.selectedBrowser as Browser,
      true,
    );
  }

  closePopup() {
    const panel = document?.getElementById("ssb-panel") as unknown as XULElement & {
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

      if (ssbObj && globalThis.gBrowser.selectedBrowser.currentURI) {
        await this.runSsbByUrl(
          globalThis.gBrowser.selectedBrowser.currentURI.spec,
        );
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
      globalThis.setTimeout(() => {
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
      const { startUrl } = DataManagerClass.parseKey(key);
      if (
        startUrl === currentTabSsb.start_url ||
        currentTabSsb.start_url.startsWith(startUrl)
      ) {
        return true;
      }
    }
    return false;
  }

  /**
   * Returns whether `uri` is eligible to be installed as a site-specific
   * browser. Reads `uri.host` only inside the `http` branch — `nsIURI.host`
   * is not `[infallible]` per `netwerk/base/nsIURI.idl` and throws
   * `NS_ERROR_FAILURE` for the `nsSimpleURI` family of schemes (`data:`,
   * `blob:`, `moz-extension:`, `javascript:`, `about:`), so guarding the
   * `.host` access behind the scheme check keeps the throw unreachable.
   *
   * Returns `true` for any `https` URI, and for `http` URIs targeting
   * loopback hosts (`localhost`, `127.0.0.1`). Every other scheme returns
   * `false`.
   */
  private checkSiteCanBeInstall(uri: nsIURI): boolean {
    if (uri.scheme === "https") {
      return true;
    }
    if (uri.scheme === "http") {
      return uri.host === "localhost" || uri.host === "127.0.0.1";
    }
    return false;
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

    await this.dataManager.removeSsbData(
      DataManagerClass.buildKey(manifest.start_url, manifest.userContextId ?? 0),
    );
  }

  public async uninstallById(id: string) {
    const ssbObj = await this.getSsbObj(id);
    if (!ssbObj) {
      return;
    }
    await this.uninstall(ssbObj);
  }

  private async getIdByUrl(url: string, userContextId: number = 0) {
    const ssbData = await this.dataManager.getCurrentSsbData();
    const key = DataManagerClass.buildKey(url, userContextId);
    return ssbData[key];
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
    const browser = globalThis.gBrowser.selectedBrowser as Browser;
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

  public async setContainerForSsb(
    id: string,
    userContextId: number,
  ): Promise<boolean> {
    const ssbObj = await this.getSsbObj(id);
    if (!ssbObj) {
      return false;
    }

    const oldKey = DataManagerClass.buildKey(
      ssbObj.start_url,
      ssbObj.userContextId ?? 0,
    );

    // Remove from old key
    await this.dataManager.removeSsbData(oldKey);

    // Save with new key
    const updatedManifest: Manifest = {
      ...ssbObj,
      userContextId: userContextId > 0 ? userContextId : undefined,
    };
    await this.dataManager.saveSsbData(updatedManifest);
    return true;
  }

  public useOSIntegration() {
    return true;
  }
}
