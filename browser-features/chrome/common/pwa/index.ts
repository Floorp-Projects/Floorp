/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { SsbPageAction } from "./SsbPageAction.tsx";
import { SsbPanelView } from "./SsbPanelView.tsx";
import { enabled } from "./config.ts";
import { PwaWindowSupport } from "./pwa-window.tsx";
import { PwaService } from "./pwaService.ts";
import { ManifestProcesser } from "./manifestProcesser.ts";
import { DataManager } from "./dataStore.ts";
import { SiteSpecificBrowserManager } from "./ssbManager.ts";

@noraComponent(import.meta.hot)
export default class Pwa extends NoraComponentBase {
  static ctx: PwaService | null = null;

  init() {
    if (!enabled()) return;
    const manifestProcesser = new ManifestProcesser();
    const dataManager = new DataManager();
    const ssbManager = new SiteSpecificBrowserManager(
      manifestProcesser,
      dataManager,
    );
    const ctx = new PwaService(ssbManager, manifestProcesser, dataManager);

    new SsbPageAction(ctx);
    new SsbPanelView(ctx);
    new PwaWindowSupport(ctx);

    Pwa.ctx = ctx;
    // Check if a startup SSB id was stored by the command line handler.
    // Run it immediately (init is invoked after the browser UI is ready in
    // NoraComponent lifecycle) and clear the pref to avoid repeated launches.
    (async () => {
      console.debug("Checking for startup SSB id...");
      try {
        const id = Services.prefs.getCharPref("floorp.ssb.startup.id", "");
        if (id) {
          const ssbObj = await ctx.getSsbObj(id);
          if (ssbObj) {
            await ctx.runSsbByUrl(ssbObj.start_url);
          }
          try {
            Services.prefs.clearUserPref("floorp.ssb.startup.id");
          } catch (e) {
            console.error("Failed to clear floorp.ssb.startup.id", e);
          }
        }
      } catch (e) {
        // If the pref doesn't exist or any error occurs, ignore it so startup
        // proceeds normally.
        // eslint-disable-next-line no-console
        console.debug("No startup SSB id or failed to start SSB:", e);
      }
    })();
  }
}
