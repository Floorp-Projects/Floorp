// SPDX-License-Identifier: MPL-2.0

import { isEnabled } from "./config.ts";
import { CommandPaletteController } from "./controller.ts";
import { signal } from "@preact/signals";
import {
  addDisposer,
  createRoot,
  createRootHMR,
  rootEffect,
} from "@nora/preact-xul/lifetime";
import { gestureActions } from "../mouse-gesture/utils/gestures.ts";

export class CommandPaletteService {
  private controller: CommandPaletteController | null = null;
  private controllerRevision = signal(0);
  private disposed = true;
  private disposeRoot: (() => void) | null = null;

  constructor(private readonly targetWindow: Window = window) {
    if (targetWindow.closed) return;
    this.disposed = false;
    this.disposeRoot = createRoot((dispose) => {
      targetWindow.addEventListener("unload", dispose, { once: true });
      addDisposer(() => {
        this.disposed = true;
        targetWindow.removeEventListener("unload", dispose);
        this.destroyController();
      });
      this.registerAction();
      rootEffect(() => {
        if (isEnabled()) this.attachToWindow(targetWindow);
        else this.destroyController();
      });
      return dispose;
    });
  }

  private registerAction(): void {
    const name = "floorp-toggle-command-palette";
    const previous = gestureActions.getAllActions().get(name);
    const action = {
      name,
      fn: (win: Window) => {
        if (!this.disposed && win === this.targetWindow && !win.closed) {
          this.controller?.togglePalette();
        }
      },
    };
    gestureActions.registerAction(action);
    addDisposer(() => {
      const actions = gestureActions.getAllActions();
      // Do not remove a replacement installed by a newer module owner.
      if (actions.get(name) !== action) return;
      if (previous) actions.set(name, previous);
      else actions.delete(name);
    });
  }

  public attachToWindow(win: Window): void {
    if (
      this.disposed || win !== this.targetWindow || win.closed ||
      this.controller || !isEnabled()
    ) return;
    this.controller = new CommandPaletteController(win);
    this.controllerRevision.value = this.controllerRevision.peek() + 1;
    // Diagnostic only; each realm owns the controller consumed by its own UI.
    (win as Window & { __commandPaletteControllerAttached?: boolean })
      .__commandPaletteControllerAttached = true;
  }

  public getController(win: Window): CommandPaletteController | undefined {
    this.controllerRevision.value;
    return !this.disposed && win === this.targetWindow
      ? this.controller ?? undefined
      : undefined;
  }

  private destroyController(): void {
    if (!this.controller) return;
    const controller = this.controller;
    this.controller = null;
    try {
      controller.destroy();
    } finally {
      delete (this.targetWindow as Window & {
        __commandPaletteControllerAttached?: boolean;
      }).__commandPaletteControllerAttached;
      this.controllerRevision.value = this.controllerRevision.peek() + 1;
    }
  }

  public destroy(): void {
    this.disposeRoot?.();
    this.disposeRoot = null;
  }
}

function createCommandPaletteService() {
  return new CommandPaletteService();
}

export const commandPaletteService = createRootHMR(
  createCommandPaletteService,
  import.meta.hot,
);
