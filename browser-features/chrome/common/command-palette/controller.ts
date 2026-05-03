// SPDX-License-Identifier: MPL-2.0

import { state, resetPaletteState } from "./data/state.ts";
import { isEnabled, addRecentCommand, getRecentCommands } from "./config.ts";
import { getPaletteCommands, searchCommands } from "./command-registry.ts";
import type { PaletteCommand } from "./command-registry.ts";

const TRIGGER_MODIFIERS = {
  alt: false,
  ctrl: true,
  meta: false,
  shift: true,
};
const TRIGGER_KEY = "KeyP";

export class CommandPaletteController {
  private eventListenersAttached = false;
  private targetWindow: Window;

  constructor(win: Window = globalThis as unknown as Window) {
    this.targetWindow = win;
    this.init();
  }

  private init(): void {
    if (this.eventListenersAttached) return;

    this.targetWindow.addEventListener(
      "keydown",
      this.handleTriggerKeyDown,
      true, // capture phase
    );
    this.eventListenersAttached = true;
  }

  public destroy(): void {
    if (this.eventListenersAttached) {
      this.targetWindow.removeEventListener(
        "keydown",
        this.handleTriggerKeyDown,
        true,
      );
      this.eventListenersAttached = false;
    }
    if (state.isVisible()) {
      this.hidePalette();
    }
  }

  private handleTriggerKeyDown = (event: KeyboardEvent): void => {
    if (!isEnabled()) return;

    // Toggle close with the same shortcut
    if (state.isVisible()) {
      if (this.isTriggerShortcut(event)) {
        event.preventDefault();
        event.stopPropagation();
        this.hidePalette();
        return;
      }
      this.handlePaletteKeyDown(event);
      return;
    }

    if (this.isTriggerShortcut(event)) {
      event.preventDefault();
      event.stopPropagation();
      this.showPalette();
    }
  };

  private isTriggerShortcut(event: KeyboardEvent): boolean {
    return (
      event.altKey === TRIGGER_MODIFIERS.alt &&
      event.ctrlKey === TRIGGER_MODIFIERS.ctrl &&
      event.metaKey === TRIGGER_MODIFIERS.meta &&
      event.shiftKey === TRIGGER_MODIFIERS.shift &&
      event.code === TRIGGER_KEY
    );
  }

  private handlePaletteKeyDown(event: KeyboardEvent): void {
    const commands = state.filteredCommands();
    const idx = state.selectedIndex();

    switch (event.key) {
      case "Escape":
        event.preventDefault();
        event.stopPropagation();
        this.hidePalette();
        break;

      case "ArrowDown":
        event.preventDefault();
        event.stopPropagation();
        if (commands.length > 0) {
          state.setSelectedIndex((idx + 1) % commands.length);
        }
        break;

      case "ArrowUp":
        event.preventDefault();
        event.stopPropagation();
        if (commands.length > 0) {
          state.setSelectedIndex(
            (idx - 1 + commands.length) % commands.length,
          );
        }
        break;

      case "Enter":
        event.preventDefault();
        event.stopPropagation();
        if (commands[idx]) {
          this.executeCommand(commands[idx]);
        }
        break;

      case "Tab":
        event.preventDefault();
        event.stopPropagation();
        if (commands.length > 0) {
          if (event.shiftKey) {
            state.setSelectedIndex(
              (idx - 1 + commands.length) % commands.length,
            );
          } else {
            state.setSelectedIndex((idx + 1) % commands.length);
          }
        }
        break;
    }
  }

  private showPalette(): void {
    resetPaletteState();

    // Show recent commands first, then all others
    const recentIds = getRecentCommands();
    const allCommands = getPaletteCommands();
    const recentSet = new Set(recentIds);
    const recentCommands = recentIds
      .map((id) => allCommands.find((c) => c.id === id))
      .filter((c): c is PaletteCommand => c !== undefined);
    const otherCommands = allCommands.filter((c) => !recentSet.has(c.id));

    state.setFilteredCommands([...recentCommands, ...otherCommands]);
    state.setIsVisible(true);

    // Focus the search input after render
    this.targetWindow.setTimeout(() => {
      const input = this.targetWindow.document.getElementById(
        "command-palette-search",
      );
      if (input) (input as HTMLInputElement).focus();
    }, 0);
  }

  private hidePalette(): void {
    state.setIsVisible(false);
    resetPaletteState();
  }

  public executeCommand(cmd: PaletteCommand): void {
    addRecentCommand(cmd.id);
    this.hidePalette();
    try {
      cmd.fn(this.targetWindow);
    } catch (e) {
      console.error(`[command-palette] Action failed: ${cmd.id}`, e);
    }
  }

  public updateSearch(query: string): void {
    const filtered = searchCommands(query);
    state.setFilteredCommands(filtered);
    state.setSelectedIndex(0);
  }
}
