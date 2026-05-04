// SPDX-License-Identifier: MPL-2.0

import { createPaletteState, type PaletteState } from "./data/state.ts";
import { isEnabled, addRecentCommand, getRecentCommands } from "./config.ts";
import { getPaletteCommands, searchCommands } from "./command-registry.ts";
import type { PaletteCommand } from "./command-registry.ts";

export class CommandPaletteController {
  private eventListenersAttached = false;
  private targetWindow: Window;
  readonly state: PaletteState = createPaletteState();

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
    if (this.state.isVisible()) {
      this.hidePalette();
    }
  }

  public togglePalette(): void {
    if (!isEnabled()) return;

    if (this.state.isVisible()) {
      this.hidePalette();
    } else {
      this.showPalette();
    }
  }

  private handlePaletteKeyDown = (event: KeyboardEvent): void => {
    if (!this.state.isVisible()) return;

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
    const commands = this.state.filteredCommands();
    const idx = this.state.selectedIndex();
    if (commands.length > 0) {
      this.state.setSelectedIndex((idx + 1) % commands.length);
    }
  }

  private handleArrowUp(): void {
    const commands = this.state.filteredCommands();
    const idx = this.state.selectedIndex();
    if (commands.length > 0) {
      this.state.setSelectedIndex(
        (idx - 1 + commands.length) % commands.length,
      );
    }
  }

  private handleEnter(): void {
    const commands = this.state.filteredCommands();
    const idx = this.state.selectedIndex();
    if (commands[idx]) {
      this.executeCommand(commands[idx]);
    }
  }

  private showPalette(): void {
    this.state.reset();
    this.state.setFilteredCommands(this.buildInitialCommandList());
    this.state.setIsVisible(true);

    // Focus the search input after render
    this.targetWindow.setTimeout(() => {
      const input = this.targetWindow.document?.getElementById(
        "command-palette-search",
      );
      if (input) (input as HTMLInputElement).focus();
    }, 0);
  }

  private hidePalette(): void {
    this.state.setIsVisible(false);
    this.state.reset();
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
    const filtered = query.trim()
      ? searchCommands(query)
      : this.buildInitialCommandList();
    this.state.setFilteredCommands(filtered);
    this.state.setSelectedIndex(0);
  }

  private buildInitialCommandList(): PaletteCommand[] {
    const recentIds = getRecentCommands();
    const allCommands = getPaletteCommands();
    const recentSet = new Set(recentIds);
    const recentCommands = recentIds
      .map((id) => allCommands.find((c) => c.id === id))
      .filter((c): c is PaletteCommand => c !== undefined);
    const otherCommands = allCommands.filter((c) => !recentSet.has(c.id));
    return [...recentCommands, ...otherCommands];
  }
}
