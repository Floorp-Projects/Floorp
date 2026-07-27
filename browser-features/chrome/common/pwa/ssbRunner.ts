/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { DataManager } from "./dataStore.ts";
import type { Manifest } from "./type.ts";
import type { SiteSpecificBrowserManager } from "./ssbManager.ts";

const { SsbRunnerUtils } = ChromeUtils.importESModule(
  "resource://noraneko/modules/pwa/SsbCommandLineHandler.sys.mjs",
);

export class SsbRunner {
  constructor(
    private dataManager: DataManager,
    private ssbManager: SiteSpecificBrowserManager,
  ) {}

  public async runSsbByUrl(url: string, userContextId?: number) {
    console.debug("[PWA:install-launch] runSsbByUrl", { url, userContextId });

    const ssbData = await this.dataManager.getCurrentSsbData();
    const installedKeys = Object.keys(ssbData);
    console.debug("[PWA:install-launch] ssb store keys", installedKeys);

    const ssbToRun = Object.values(ssbData).find((ssb) => {
      const urlMatch = url.startsWith(ssb.start_url);
      const containerMatch = userContextId !== undefined
        ? (ssb.userContextId ?? 0) === userContextId
        : true;
      const matched = urlMatch && containerMatch;
      if (urlMatch) {
        console.debug("[PWA:install-launch] URL candidate", {
          id: ssb.id,
          startUrl: ssb.start_url,
          userContextId: ssb.userContextId ?? 0,
          containerMatch,
          matched,
        });
      }
      return matched;
    });

    if (ssbToRun) {
      console.debug("[PWA:install-launch] matched SSB, opening window", {
        id: ssbToRun.id,
        startUrl: ssbToRun.start_url,
        userContextId: ssbToRun.userContextId ?? 0,
      });
      await this.openSsbWindow(ssbToRun);
      return;
    }

    console.warn("[PWA:install-launch] no matching SSB found", {
      url,
      userContextId,
      installedCount: installedKeys.length,
    });
  }

  public async openSsbWindow(ssb: Manifest) {
    console.debug("[PWA:install-launch] openSsbWindow", {
      id: ssb.id,
      startUrl: ssb.start_url,
      userContextId: ssb.userContextId ?? 0,
    });
    const win = await SsbRunnerUtils.openSsbWindow(ssb);
    console.debug("[PWA:install-launch] openSsbWindow created window", {
      name: win?.name,
      location: win?.location?.href,
    });
    await SsbRunnerUtils.applyOSIntegration(ssb, win);
    console.debug("[PWA:install-launch] applyOSIntegration finished");
    return win;
  }
}
