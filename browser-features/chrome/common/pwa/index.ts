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
      // Mac migration: disable PWA if no apps are installed
      const { AppConstants } = ChromeUtils.importESModule(
        "resource://gre/modules/AppConstants.sys.mjs",
      );
      if (AppConstants.platform === "macosx") {
        try {
          const migrationChecked = Services.prefs.getBoolPref(
            "floorp.browser.ssb.mac_migration_checked",
            false,
          );
          if (!migrationChecked) {
            const apps = await dataManager.getCurrentSsbData();
            if (Object.keys(apps).length === 0) {
              Services.prefs.setBoolPref("floorp.browser.ssb.enabled", false);
              // Since we disabled the feature, we should probably stop here
              // But strictly speaking, we are in the init() so the UI is already created.
              // A weird state, but it will be disabled effectively on next run or if components check enabled() reactively.
              // Given PwaWindowSupport checks enabled status implicitly or by existence, it might be fine.
              // Actually, PwaWindowSupport doesn't check 'enabled()' in constructor, it checks taskbartab attr.
              // But SsbPageAction etc might be visible.
              // However, typically this init runs once. If we disable it, the reactive config in config.ts will update.
              // Let's verify config.ts reactive behavior.
              // config.ts has a listener.
            }
            Services.prefs.setBoolPref(
              "floorp.browser.ssb.mac_migration_checked",
              true,
            );
          }
        } catch (e) {
          console.error("Failed to run PWA migration for Mac", e);
        }
      }

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
