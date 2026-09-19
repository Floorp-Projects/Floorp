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
import { handleContextMenuAfterMouseUp } from "./context-menu-policy.ts";
import { createRootHMR } from "@nora/solid-xul";
import { createEffect } from "solid-js";

interface nsIObserver {
  observe(subject: unknown, topic: string, data: string): void;
}

/**
 * MouseGestureService manages the lifecycle of gesture controllers across windows.
 */
export class MouseGestureService {
  private controllers: Map<Window, MouseGestureController> = new Map();
  private lastConfigString = "";
  private configObserver: nsIObserver | null = null;

  constructor() {
    this.initialize();

    // React to config changes
    createEffect(() => {
      const config = getConfig();
      const configString = JSON.stringify(config);
      const enabled = isEnabled();

      if (this.lastConfigString && this.lastConfigString !== configString) {
        // Recreate controllers when config changes
        this.destroyAllControllers();
        if (enabled) {
          this.attachToAllWindows();
        }
        handleContextMenuAfterMouseUp(enabled);
      }

      this.lastConfigString = configString;
    });

    // React to enabled state changes
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
    // Check if this window already has a controller registered from ANY context
    // This prevents duplicate controllers when multiple JS contexts try to attach to the same window
    // deno-lint-ignore no-explicit-any
    if ((win as any).__mouseGestureControllerAttached === true) {
      return;
    }

    if (this.controllers.has(win)) {
      return;
    }

    if (isEnabled()) {
      const controller = new MouseGestureController(win);
      this.controllers.set(win, controller);

      // Mark the window as having a controller attached
      // deno-lint-ignore no-explicit-any
      (win as any).__mouseGestureControllerAttached = true;

      const onUnload = () => {
        controller.destroy();
        this.controllers.delete(win);
        // Clean up the marker when the window is closed
        try {
          // deno-lint-ignore no-explicit-any
          delete (win as any).__mouseGestureControllerAttached;
        } catch (_e) {
          // Window might be already gone
        }
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
    for (const [win, controller] of this.controllers.entries()) {
      controller.destroy();
      // Clean up the marker
      try {
        // deno-lint-ignore no-explicit-any
        delete (win as any).__mouseGestureControllerAttached;
      } catch (_e) {
        // Window might be already gone
      }
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

export const mouseGestureService = createRootHMR(
  createMouseGestureService,
  import.meta.hot,
);
