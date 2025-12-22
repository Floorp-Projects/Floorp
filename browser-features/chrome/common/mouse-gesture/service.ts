/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  _setConfig,
  type GesturePattern,
  getConfig,
  isEnabled,
  type MouseGestureConfig,
  setEnabled,
} from "./config.ts";
import { MouseGestureController } from "./controller.ts";
import { createRootHMR } from "@nora/solid-xul";
import { createEffect } from "solid-js";

interface nsIObserver {
  observe(subject: unknown, topic: string, data: string): void;
}

export class MouseGestureService {
  private controllers: Map<Window, MouseGestureController> = new Map();
  private lastConfigString = "";
  private configObserver: nsIObserver | null = null;

  constructor() {
    this.initialize();

    createEffect(() => {
      const config = getConfig();
      const configString = JSON.stringify(config);
      const enabled = isEnabled();

      if (this.lastConfigString && this.lastConfigString !== configString) {
        // Re-create all controllers when config changes (to apply new settings)
        this.destroyAllControllers();
        if (enabled) {
          this.attachToAllWindows();
        }

        handleContextMenuAfterMouseUp(enabled);
      }

      this.lastConfigString = configString;
    });

    createEffect(() => {
      const enabled = isEnabled();
      if (enabled) {
        this.attachToAllWindows();
      } else {
        this.destroyAllControllers();
      }
      handleContextMenuAfterMouseUp(enabled);
    });

    this.setupEnabledObserver();
  }

  private attachToAllWindows(): void {
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      const win = windows.getNext() as Window;
      this.attachToWindow(win);
    }
  }

  public attachToWindow(win: Window): void {
    if (this.controllers.has(win)) {
      return;
    }

    if (isEnabled()) {
      const controller = new MouseGestureController(win);
      this.controllers.set(win, controller);

      const onUnload = () => {
        controller.destroy();
        this.controllers.delete(win);
        try {
          win.removeEventListener("unload", onUnload);
        } catch (_e) {
          // Window might be already gone
        }
      };

      win.addEventListener("unload", onUnload, { once: true });
    }
  }

  private destroyAllControllers(): void {
    for (const controller of this.controllers.values()) {
      controller.destroy();
    }
    this.controllers.clear();
  }

  private setupEnabledObserver(): void {
    if (this.configObserver) {
      try {
        Services.prefs.removeObserver(
          "floorp.mousegesture.enabled",
          this.configObserver,
        );
      } catch (e) {
        console.error("[MouseGestureService] Error removing observer:", e);
      }
    }

    this.configObserver = {
      observe: (_subject: unknown, topic: string, data: string) => {
        if (
          topic === "nsPref:changed" &&
          data === "floorp.mousegesture.enabled"
        ) {
          const enabled = Services.prefs.getBoolPref(
            "floorp.mousegesture.enabled",
            false,
          );

          if (enabled) {
            this.attachToAllWindows();
          } else {
            this.destroyAllControllers();
          }

          handleContextMenuAfterMouseUp(enabled);
        }
      },
    };

    Services.prefs.addObserver(
      "floorp.mousegesture.enabled",
      this.configObserver,
    );
  }

  private initialize(): void {
    this.lastConfigString = JSON.stringify(getConfig());
    const enabled = isEnabled();

    if (enabled) {
      this.attachToAllWindows();
    }
    handleContextMenuAfterMouseUp(enabled);
  }

  public isEnabled(): boolean {
    return isEnabled();
  }

  public setEnabled(value: boolean): void {
    setEnabled(value);

    if (value) {
      this.attachToAllWindows();
    } else {
      this.destroyAllControllers();
    }

    handleContextMenuAfterMouseUp(value);
  }

  public getConfig(): MouseGestureConfig {
    return getConfig();
  }

  public updateConfig(newConfig: MouseGestureConfig): void {
    setConfig(newConfig);

    this.destroyAllControllers();
    if (isEnabled()) {
      this.attachToAllWindows();
    }
  }

  public patternToDisplayString(pattern: GesturePattern): string {
    const directionSymbols: Record<string, string> = {
      up: "↑",
      down: "↓",
      left: "←",
      right: "→",
      upRight: "↗",
      upLeft: "↖",
      downRight: "↘",
      downLeft: "↙",
    };

    return pattern.map((dir) => directionSymbols[dir] || dir).join(" ");
  }
}

function setConfig(config: MouseGestureConfig) {
  _setConfig(config);
}

function createMouseGestureService() {
  return new MouseGestureService();
}

const PREF_LAST_ENABLED_STATE = "floorp.mousegesture.last_enabled_state";

let lastEnabledState: boolean | null = null;

// Initialize lastEnabledState from pref if it exists so the value survives restarts
try {
  if (
    Services.prefs.getPrefType(PREF_LAST_ENABLED_STATE) ===
    Services.prefs.PREF_BOOL
  ) {
    lastEnabledState = Services.prefs.getBoolPref(PREF_LAST_ENABLED_STATE);
  }
} catch (e) {
  // It's expected that the preference might not exist on first run, so we'll just log it for debugging.
  console.log(
    "[MouseGestureService] Could not read preference for last enabled state:",
    e,
  );
}

function handleContextMenuAfterMouseUp(enabled: boolean) {
  if (Services.appinfo.OS === "WINNT") {
    return;
  }

  if (lastEnabledState === enabled) {
    return;
  }

  Services.prefs.setBoolPref("ui.context_menus.after_mouseup", enabled);
  lastEnabledState = enabled;

  // persist lastEnabledState so it is available after restart
  try {
    Services.prefs.setBoolPref(PREF_LAST_ENABLED_STATE, enabled);
  } catch (e) {
    console.error(
      "[MouseGestureService] Failed to save last enabled state pref:",
      e,
    );
  }
}

export const mouseGestureService = createRootHMR(
  createMouseGestureService,
  import.meta.hot,
);
