/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { ManifestProcesser } from "./manifestProcesser.ts";
import type { DataManager } from "./dataStore.ts";
import { DataManager as DataManagerClass } from "./dataStore.ts";
import { getUserContextIdForBrowser } from "./containerUtils.ts";
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
        subject?.wrappedJSObject?.key as string | undefined,
      );
    }, "nora-ssb-rename");

    // deno-lint-ignore no-explicit-any
    Services.obs.addObserver(async (subject: any) => {
      await this.uninstallById(
        subject?.wrappedJSObject?.id as string,
        subject?.wrappedJSObject?.key as string | undefined,
      );
    }, "nora-ssb-uninstall");

    // deno-lint-ignore no-explicit-any
    Services.obs.addObserver(async (subject: any) => {
      await this.resetContainerForSsb(
        subject?.wrappedJSObject?.id as string,
        subject?.wrappedJSObject?.key as string | undefined,
      );
    }, "nora-ssb-reset-container");
  }

  private listener = {
    onLocationChange: () => {
      this.onCurrentTabChangedOrLoaded();
    },
  };

  public async installOrRunCurrentPageAsSsb(
    browser: Browser,
    asPwa = true,
    installUserContextId?: number,
  ) {
    const tabUserContextId = getUserContextIdForBrowser(browser);
    console.debug("[PWA:install-launch] installOrRunCurrentPageAsSsb", {
      asPwa,
      installUserContextId,
      tabUserContextId,
      pageUrl: browser.currentURI?.spec,
    });

    const isInstalled = installUserContextId !== undefined
      ? await this.checkPageIsInstalledForContainer(
        browser,
        installUserContextId,
      )
      : await this.checkCurrentPageIsInstalled(browser);

    console.debug("[PWA:install-launch] isInstalled check", { isInstalled });

    if (isInstalled) {
      const currentTabSsb = await this.getCurrentTabSsb(browser);

      if (!currentTabSsb) {
        console.warn(
          "[PWA:install-launch] isInstalled=true but getCurrentTabSsb returned null",
        );
        return;
      }

      const runUserContextId = installUserContextId !== undefined
        ? installUserContextId
        : tabUserContextId;
      console.debug("[PWA:install-launch] run existing SSB", {
        startUrl: currentTabSsb.start_url,
        runUserContextId,
      });
      await this.runSsbByUrl(currentTabSsb.start_url, runUserContextId);
    } else {
      const userContextId = installUserContextId ?? tabUserContextId;
      const manifest = await this.createFromBrowser(browser, {
        useWebManifest: asPwa,
        userContextId,
      });

      if (!manifest) {
        console.warn(
          "[PWA:install-launch] createFromBrowser returned null manifest",
        );
        return;
      }

      console.debug("[PWA:install-launch] installing manifest", {
        id: manifest.id,
        name: manifest.name,
        startUrl: manifest.start_url,
        userContextId: manifest.userContextId ?? 0,
      });

      await this.install(manifest);

      console.debug(
        "[PWA:install-launch] install() finished, scheduling launch in 3000ms",
        {
          startUrl: manifest.start_url,
          userContextId: manifest.userContextId,
        },
      );

      // Installing needs some time to finish
      globalThis.setTimeout(() => {
        console.debug(
          "[PWA:install-launch] setTimeout fired, calling runSsbByUrl",
          {
            startUrl: manifest.start_url,
            userContextId: manifest.userContextId,
          },
        );
        void this.runSsbByUrl(manifest.start_url, manifest.userContextId)
          .then(() => {
            console.debug("[PWA:install-launch] runSsbByUrl completed");
          })
          .catch((error) => {
            console.error(
              "[PWA:install-launch] runSsbByUrl failed after install:",
              error,
            );
          });
      }, 3000);
    }
  }

  public async checkPageIsInstalledForContainer(
    browser: Browser,
    userContextId: number,
  ): Promise<boolean> {
    if (!this.checkSiteCanBeInstall(browser.currentURI)) {
      return false;
    }

    const currentTabSsb = await this.getCurrentTabSsb(browser);

    if (!currentTabSsb) {
      return false;
    }

    const ssb = await this.getSsbByUrlAndContainer(
      currentTabSsb.start_url,
      userContextId,
    );
    return ssb !== undefined;
  }

  closePopup() {
    const panel = document?.getElementById("ssb-panel") as unknown as
      & XULElement
      & {
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

  public async checkCurrentPageIsInstalled(browser: Browser): Promise<boolean> {
    if (!this.checkSiteCanBeInstall(browser.currentURI)) {
      return false;
    }

    const currentTabSsb = await this.getCurrentTabSsb(browser);

    if (!currentTabSsb) {
      return false;
    }

    const userContextId = getUserContextIdForBrowser(browser);
    const ssb = await this.getSsbByUrlAndContainer(
      currentTabSsb.start_url,
      userContextId,
    );
    return ssb !== undefined;
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
    options: { useWebManifest: boolean; userContextId: number },
  ): Promise<Manifest | null> {
    const manifest = await this.manifestProcesser.getManifestFromBrowser(
      browser,
      options.useWebManifest,
    );

    if (!manifest) {
      return null;
    }

    if (options.userContextId > 0) {
      manifest.userContextId = options.userContextId;
    }

    return manifest;
  }

  private async install(manifest: Manifest) {
    console.debug("[PWA:install-launch] install() start", {
      id: manifest.id,
      platform: AppConstants.platform,
    });

    await this.dataManager.saveSsbData(manifest);
    console.debug("[PWA:install-launch] saveSsbData finished", {
      key: DataManagerClass.buildKey(
        manifest.start_url,
        manifest.userContextId ?? 0,
      ),
    });

    if (!SupportClass) {
      console.debug(
        "[PWA:install-launch] no OS SupportClass for this platform",
      );
      return;
    }

    void this.installOsIntegration(manifest).catch((error) => {
      console.error(
        "[PWA:install-launch] OS integration install failed:",
        error,
      );
    });
  }

  private async installOsIntegration(manifest: Manifest) {
    if (AppConstants.platform === "win") {
      if (TaskbarExperiment.isEnabledForCurrentPlatform()) {
        console.debug("[PWA:install-launch] Windows OS install starting");
        const windowsSupport = new SupportClass(this);
        await windowsSupport.install(manifest);
        console.debug("[PWA:install-launch] Windows OS install finished");
      } else {
        console.debug(
          "[SiteSpecificBrowserManager] PWA taskbar integration disabled by A/B test, skipping Windows install",
        );
      }
      return;
    }

    if (AppConstants.platform === "linux") {
      console.debug("[PWA:install-launch] Linux OS install starting");
      const linuxSupport = new SupportClass(this);
      await linuxSupport.install(manifest);
      console.debug("[PWA:install-launch] Linux OS install finished");
    }
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
      DataManagerClass.buildKey(
        manifest.start_url,
        manifest.userContextId ?? 0,
      ),
    );
  }

  public async uninstallById(id: string, key?: string) {
    const ssbObj = await this.getSsbObj(id, key);
    if (!ssbObj) {
      return;
    }
    await this.uninstall(ssbObj);
  }

  public async getSsbByUrlAndContainer(
    url: string,
    userContextId: number = 0,
  ): Promise<Manifest | undefined> {
    const ssbData = await this.dataManager.getCurrentSsbData();
    const key = DataManagerClass.buildKey(url, userContextId);
    return ssbData[key];
  }

  public async getSsbObj(id: string, key?: string) {
    const ssbData = await this.dataManager.getCurrentSsbData();
    if (key && ssbData[key]) {
      return ssbData[key] as Manifest;
    }
    for (const value of Object.values(ssbData)) {
      if ((value as Manifest).id === id) {
        return value as Manifest;
      }
    }
    return null;
  }

  public async runSsbByUrl(url: string, userContextId?: number) {
    await this.ssbRunner.runSsbByUrl(url, userContextId);
  }

  private async onCurrentTabChangedOrLoaded() {
    const browser = globalThis.gBrowser.selectedBrowser as Browser;
    const currentPageCanBeInstalled = this.checkSiteCanBeInstall(
      browser.currentURI,
    );
    const currentPageHasSsbManifest = await this.manifestProcesser
      .getManifestFromBrowser(browser, true);
    const currentPageIsInstalled = await this.checkCurrentPageIsInstalled(
      browser,
    );

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

  public async renameSsb(
    id: string,
    newName: string,
    key?: string,
  ): Promise<boolean> {
    const ssbObj = await this.getSsbObj(id, key);
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

  /**
   * Clears a deleted container assignment by moving the entry to the default
   * container key (userContextId 0).
   */
  public async resetContainerForSsb(
    id: string,
    key?: string,
  ): Promise<boolean> {
    const ssbObj = await this.getSsbObj(id, key);
    if (!ssbObj) {
      return false;
    }

    const currentContextId = ssbObj.userContextId ?? 0;
    if (currentContextId === 0) {
      return true;
    }

    const oldKey = DataManagerClass.buildKey(
      ssbObj.start_url,
      currentContextId,
    );
    const defaultKey = DataManagerClass.buildKey(ssbObj.start_url, 0);
    const currentSsbData = await this.dataManager.getCurrentSsbData();
    if (currentSsbData[defaultKey] && defaultKey !== oldKey) {
      console.warn(
        "[SiteSpecificBrowserManager] Cannot reset container because default container entry already exists",
        { startUrl: ssbObj.start_url, oldKey, defaultKey },
      );
      return false;
    }

    await this.dataManager.removeSsbData(oldKey);

    const updatedManifest: Manifest = {
      ...ssbObj,
      userContextId: undefined,
    };
    await this.dataManager.saveSsbData(updatedManifest);
    return true;
  }

  public useOSIntegration() {
    return true;
  }
}
