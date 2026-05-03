// SPDX-License-Identifier: MPL-2.0

import { isEnabled, setEnabled as setEnabledPref } from "./config.ts";
import { CommandPaletteController } from "./controller.ts";
import { createRootHMR } from "@nora/solid-xul";
import { createEffect } from "solid-js";

export class CommandPaletteService {
  private controllers: Map<Window, CommandPaletteController> = new Map();

  constructor() {
    this.initialize();

    createEffect(() => {
      const enabled = isEnabled();
      if (enabled) {
        this.attachToAllWindows();
      } else {
        this.destroyAllControllers();
      }
    });
  }

  private initialize(): void {
    if (isEnabled()) {
      this.attachToAllWindows();
    }
  }

  private attachToAllWindows(): void {
    const windows = Services.wm.getEnumerator("navigator:browser");
    while (windows.hasMoreElements()) {
      const win = windows.getNext() as Window;
      this.attachToWindow(win);
    }
  }

  public attachToWindow(win: Window): void {
    // deno-lint-ignore no-explicit-any
    if ((win as any).__commandPaletteControllerAttached === true) return;
    if (this.controllers.has(win)) return;

    if (isEnabled()) {
      const controller = new CommandPaletteController(win);
      this.controllers.set(win, controller);

      // deno-lint-ignore no-explicit-any
      (win as any).__commandPaletteControllerAttached = true;

      const onUnload = () => {
        controller.destroy();
        this.controllers.delete(win);
        try {
          // deno-lint-ignore no-explicit-any
          delete (win as any).__commandPaletteControllerAttached;
        } catch {
          // Window might be already gone
        }
      };

      win.addEventListener("unload", onUnload, { once: true });
    }
  }

  public getController(win: Window): CommandPaletteController | undefined {
    return this.controllers.get(win);
  }

  private destroyAllControllers(): void {
    for (const [win, controller] of this.controllers.entries()) {
      controller.destroy();
      try {
        // deno-lint-ignore no-explicit-any
        delete (win as any).__commandPaletteControllerAttached;
      } catch {
        // Window might be already gone
      }
    }
    this.controllers.clear();
  }
}

function createCommandPaletteService() {
  return new CommandPaletteService();
}

export const commandPaletteService = createRootHMR(
  createCommandPaletteService,
  import.meta.hot,
);
