// SPDX-License-Identifier: MPL-2.0

import { state, resetPaletteState } from "./data/state.ts";
import { isEnabled, addRecentCommand, getRecentCommands } from "./config.ts";
import { getPaletteCommands, searchCommands } from "./command-registry.ts";
import type { PaletteCommand } from "./command-registry.ts";

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
      this.handlePaletteKeyDown,
      true, // capture phase
    );
    this.eventListenersAttached = true;
  }

  public destroy(): void {
    if (this.eventListenersAttached) {
      this.targetWindow.removeEventListener(
        "keydown",
        this.handlePaletteKeyDown,
        true,
      );
      this.eventListenersAttached = false;
    }
    if (state.isVisible()) {
      this.hidePalette();
    }
  }

  public togglePalette(): void {
    if (!isEnabled()) return;

    if (state.isVisible()) {
      this.hidePalette();
    } else {
      this.showPalette();
    }
  }

  private handlePaletteKeyDown = (event: KeyboardEvent): void => {
    if (!state.isVisible()) return;

    switch (event.key) {
      case "Escape":
        event.preventDefault();
        event.stopPropagation();
        this.hidePalette();
        break;

      case "ArrowDown":
        event.preventDefault();
        event.stopPropagation();
        this.handleArrowDown();
        break;

      case "ArrowUp":
        event.preventDefault();
        event.stopPropagation();
        this.handleArrowUp();
        break;

      case "Enter":
        event.preventDefault();
        event.stopPropagation();
        this.handleEnter();
        break;

      case "Tab":
        event.preventDefault();
        event.stopPropagation();
        if (event.shiftKey) {
          this.handleArrowUp();
        } else {
          this.handleArrowDown();
        }
        break;
    }
  };

  private handleArrowDown(): void {
    const commands = state.filteredCommands();
    const idx = state.selectedIndex();
    if (commands.length > 0) {
      state.setSelectedIndex((idx + 1) % commands.length);
    }
  }

  private handleArrowUp(): void {
    const commands = state.filteredCommands();
    const idx = state.selectedIndex();
    if (commands.length > 0) {
      state.setSelectedIndex(
        (idx - 1 + commands.length) % commands.length,
      );
    }
  }

  private handleEnter(): void {
    const commands = state.filteredCommands();
    const idx = state.selectedIndex();
    if (commands[idx]) {
      this.executeCommand(commands[idx]);
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
      const input = this.targetWindow.document?.getElementById(
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
