// SPDX-License-Identifier: MPL-2.0

import {
  DEFAULT_KEEP_WEB_APPS_RUNNING,
  KEEP_WEB_APPS_RUNNING_PREF,
  planAppQuit,
  supportsAppLifecycle,
} from "#libs/pwa/appLifecycle.ts";
import type {
  AppLifecycleAdapter,
  AppLifecycleCapabilities,
  AppLifecycleSettings,
  AppLifecycleSnapshot,
  AppQuitPlan,
  AppQuitRequest,
} from "#libs/pwa/appLifecycleTypes.ts";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

/**
 * Policy bridge only. It deliberately installs no quit observers and does not
 * discover native components: the verified native integration must register
 * explicitly. Current runtimes therefore retain their existing quit behavior.
 */
export const AppLifecycle = new class {
  private adapter: AppLifecycleAdapter | null = null;

  registerAdapter(adapter: AppLifecycleAdapter): () => void {
    if (this.adapter !== null) {
      throw new Error("[AppLifecycle] A native adapter is already registered");
    }
    this.adapter = adapter;
    let registered = true;
    return () => {
      if (registered && this.adapter === adapter) {
        this.adapter = null;
      }
      registered = false;
    };
  }

  private getCapabilities(): AppLifecycleCapabilities | null {
    if (AppConstants.platform !== "macosx" || this.adapter === null) {
      return null;
    }
    try {
      return this.adapter.getCapabilities();
    } catch (error) {
      console.error("[AppLifecycle] Native capability query failed:", error);
      return null;
    }
  }

  getSettings(): AppLifecycleSettings {
    return {
      supported: supportsAppLifecycle(this.getCapabilities()),
      keepRunningAfterBrowserQuit: Services.prefs.getBoolPref(
        KEEP_WEB_APPS_RUNNING_PREF,
        DEFAULT_KEEP_WEB_APPS_RUNNING,
      ),
    };
  }

  planQuit(
    request: AppQuitRequest,
    snapshot: AppLifecycleSnapshot,
  ): AppQuitPlan {
    return planAppQuit(
      request,
      snapshot,
      Services.prefs.getBoolPref(
        KEEP_WEB_APPS_RUNNING_PREF,
        DEFAULT_KEEP_WEB_APPS_RUNNING,
      ),
      this.getCapabilities(),
    );
  }
}();
