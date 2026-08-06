// SPDX-License-Identifier: MPL-2.0

import { isEnabled, setSelectableCommands, defaultConfig } from "./config.ts";
import { CommandPaletteController } from "./controller.ts";
import { getPaletteCommands, isTabCommand } from "./command-registry.ts";
import { createRootHMR } from "@nora/solid-xul";
import { createEffect } from "solid-js";
import { gestureActions } from "../mouse-gesture/utils/gestures.ts";

export class CommandPaletteService {
  private controllers: Map<Window, CommandPaletteController> = new Map();

  constructor() {
    this.registerAction();
    this.initialize();
    // Seed the default @s → search-web shortcut BEFORE caching commands so the
    // pref exists by the time config.ts signals initialize. If the user has
    // already cleared their shortcuts (empty array persisted), this never
    // re-seeds — only a truly absent pref (PREF_INVALID) is written.
    this.initDefaultShortcuts();
    this.cacheSelectableCommands();

    createEffect(() => {
      const enabled = isEnabled();
      if (enabled) {
        this.attachToAllWindows();
      } else {
        this.destroyAllControllers();
      }
    });
  }

  private registerAction(): void {
    gestureActions.registerAction({
      name: "floorp-toggle-command-palette",
      fn: (win) => {
        const controller = this.controllers.get(win);
        if (controller) {
          controller.togglePalette();
        }
      },
    });
  }

  private initialize(): void {
    if (isEnabled()) {
      this.attachToAllWindows();
    }
  }

  /**
   * Seed the default `@s` → `floorp-search-web` shortcut pref on first launch
   * so the settings page can see and manage it immediately. If the pref
   * already exists (including an empty array written by the user to disable
   * the default), this is a no-op — a cleared shortcut list is never
   * re-seeded.
   *
   * Idempotent and safe to call on every init / HMR reload.
   */
  private initDefaultShortcuts(): void {
    try {
      const PREF = "floorp.commandPalette.shortcuts";
      if (Services.prefs.getPrefType(PREF) === Services.prefs.PREF_INVALID) {
        // First launch: seed the default shortcuts (defined once in
        // `defaultConfig.shortcuts`) so they show up in the settings page and
        // are available immediately. If the user clears them, an empty array
        // is persisted and this never re-seeds.
        Services.prefs.setStringPref(
          PREF,
          JSON.stringify(defaultConfig.shortcuts),
        );
      }
    } catch (e) {
      console.error("[command-palette] Failed to init default shortcuts", e);
    }
  }

  /**
   * Cache the win-independent command catalogue (gesture + step commands;
   * tab commands are excluded because they depend on live window state and
   * are unsuitable for @prefix shortcut binding) into the
   * `floorp.commandPalette.selectableCommands` pref. The settings page reads
   * this (read-only) to populate its shortcut command picker.
   *
   * Idempotent — safe to call on every init / HMR reload. Labels reflect the
   * current i18n locale at cache time; a browser restart refreshes them.
   */
  private cacheSelectableCommands(): void {
    try {
      const enumerator = Services.wm.getEnumerator("navigator:browser");
      const win = enumerator.hasMoreElements()
        ? (enumerator.getNext() as Window)
        : undefined;
      const allCommands = getPaletteCommands(win);
      const selectable = allCommands
        .filter((c) => !isTabCommand(c.id))
        .map((c) => ({ id: c.id, label: c.label, category: c.category }));
      setSelectableCommands(selectable);
    } catch (e) {
      console.error(
        "[command-palette] Failed to cache selectable commands",
        e,
      );
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
