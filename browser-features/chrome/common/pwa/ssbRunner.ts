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

  public async runSsbByUrl(url: string) {
    const ssbData = await this.dataManager.getCurrentSsbData();
    const ssbToRun = Object.values(ssbData).find((ssb) =>
      url.startsWith(ssb.start_url),
    );
    if (ssbToRun) {
      await this.openSsbWindow(ssbToRun);
    }
  }

  public async openSsbWindow(ssb: Manifest) {
    const win = await SsbRunnerUtils.openSsbWindow(ssb);
    await SsbRunnerUtils.applyOSIntegration(ssb, win);
    return win;
  }
}
