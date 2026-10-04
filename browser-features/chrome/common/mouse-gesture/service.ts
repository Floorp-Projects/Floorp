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
import { createEffect, createRoot, onCleanup } from "solid-js";
import type { MouseGestureWindow } from "./types.ts";

/** Each browser window owns the service and controller loaded in its realm. */
export class MouseGestureService {
  private controller: MouseGestureController | null = null;
  private lastConfigString = "";
  private disposed = true;
  private disposeRoot: (() => void) | null = null;

  constructor(private readonly targetWindow: MouseGestureWindow = window) {
    // An asynchronous feature import can finish after a detached window closes.
    if (targetWindow.closed) return;
    this.disposed = false;

    this.disposeRoot = createRoot((dispose) => {
      targetWindow.addEventListener("unload", dispose, { once: true });
      onCleanup(() => {
        this.disposed = true;
        targetWindow.removeEventListener("unload", dispose);
        this.destroyController();
      });

      // config.ts already observes both preferences. Keep a single local
      // subscription instead of a second process-wide preference observer.
      createEffect(() => {
        const configString = JSON.stringify(getConfig());
        const enabled = isEnabled();
        if (this.disposed || targetWindow.closed) return;

        if (this.lastConfigString && this.lastConfigString !== configString) {
          this.destroyController();
        }
        this.lastConfigString = configString;
        if (enabled) this.attachToWindow(targetWindow);
        else this.destroyController();
        handleContextMenuAfterMouseUp(enabled);
      });
      return dispose;
    });

    // The enclosing createRootHMR disposes this service on a module update.
    onCleanup(() => this.destroy());
  }

  public attachToWindow(win: Window): void {
    if (
      this.disposed || win !== this.targetWindow || win.closed ||
      this.controller || !isEnabled()
    ) return;

    this.controller = new MouseGestureController(win);
    // This marker is diagnostic only; another realm cannot reserve this window.
    this.targetWindow.__mouseGestureControllerAttached = true;
  }

  private destroyController(): void {
    if (!this.controller) return;
    const controller = this.controller;
    this.controller = null;
    try {
      controller.destroy();
    } finally {
      delete this.targetWindow.__mouseGestureControllerAttached;
    }
  }

  public destroy(): void {
    this.disposeRoot?.();
    this.disposeRoot = null;
  }

  public isEnabled(): boolean {
    return isEnabled();
  }

  public setEnabled(value: boolean): void {
    if (!this.disposed) setEnabled(value);
  }

  public getConfig(): MouseGestureConfig {
    return getConfig();
  }

  public updateConfig(newConfig: MouseGestureConfig): void {
    if (!this.disposed) _setConfig(newConfig);
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

export const mouseGestureService = createRootHMR(
  () => new MouseGestureService(),
  import.meta.hot,
);
