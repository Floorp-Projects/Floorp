// SPDX-License-Identifier: MPL-2.0

import { isEnabled, setSelectableCommands } from "./config.ts";
import { CommandPaletteController } from "./controller.ts";
import { getPaletteCommands, isTabCommand } from "./command-registry.ts";
import { createRootHMR } from "@nora/solid-xul";
import {
  createEffect,
  createRoot,
  createSignal,
  onCleanup,
  untrack,
} from "solid-js";
import { gestureActions } from "../mouse-gesture/utils/gestures.ts";
import type { CommandPaletteWindow } from "./types.ts";

export const COMMAND_PALETTE_TOGGLE_EVENT = "floorp-command-palette-toggle";

// Keyboard shortcut controllers can execute an action registered in another
// window's realm. Route the request to the target instead of capturing a service.
function requestPaletteToggle(win: Window): void {
  if (win.closed) return;
  win.dispatchEvent(new Event(COMMAND_PALETTE_TOGGLE_EVENT));
}

/** Each browser window owns the service and controller loaded in its realm. */
export class CommandPaletteService {
  private readonly controller = createSignal<CommandPaletteController>();
  private disposed = true;
  private disposeRoot: (() => void) | null = null;

  constructor(private readonly targetWindow: CommandPaletteWindow = window) {
    // A delayed feature import may finish after its browser window has closed.
    if (targetWindow.closed) return;
    this.disposed = false;
    this.registerAction();
    this.cacheSelectableCommands();

    this.disposeRoot = createRoot((dispose) => {
      // This listener belongs to the service, not a replaceable controller.
      // Preference toggles therefore never add another unload closure.
      targetWindow.addEventListener("unload", dispose, { once: true });
      targetWindow.addEventListener(
        COMMAND_PALETTE_TOGGLE_EVENT,
        this.handleToggleRequest,
      );
      onCleanup(() => {
        this.disposed = true;
        targetWindow.removeEventListener("unload", dispose);
        targetWindow.removeEventListener(
          COMMAND_PALETTE_TOGGLE_EVENT,
          this.handleToggleRequest,
        );
        this.detachFromWindow(targetWindow);
      });

      createEffect(() => {
        const enabled = isEnabled();
        untrack(() => {
          if (enabled) this.attachToWindow(targetWindow);
          else this.detachFromWindow(targetWindow);
        });
      });
      return dispose;
    });

    // The enclosing createRootHMR also owns this service's subscriptions.
    onCleanup(() => this.destroy());
  }

  private registerAction(): void {
    gestureActions.registerAction({
      name: "floorp-toggle-command-palette",
      fn: requestPaletteToggle,
    });
  }

  private handleToggleRequest = (): void => {
    if (this.disposed || this.targetWindow.closed || !isEnabled()) return;
    this.getController(this.targetWindow)?.togglePalette();
  };

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
      const allCommands = getPaletteCommands(this.targetWindow);
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

  public attachToWindow(win: Window): void {
    if (
      this.disposed || win !== this.targetWindow || win.closed ||
      this.controller[0]() || !isEnabled()
    ) return;

    this.controller[1](new CommandPaletteController(win));
    // Diagnostic only: another realm cannot reserve or own this window.
    this.targetWindow.__commandPaletteControllerAttached = true;
  }

  public getController(win: Window): CommandPaletteController | undefined {
    return win === this.targetWindow ? this.controller[0]() : undefined;
  }

  public detachFromWindow(win: Window): void {
    if (win !== this.targetWindow) return;
    const controller = this.controller[0]();
    if (!controller) return;
    this.controller[1](undefined);
    try {
      controller.destroy();
    } finally {
      delete this.targetWindow.__commandPaletteControllerAttached;
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
