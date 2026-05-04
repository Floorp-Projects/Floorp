// SPDX-License-Identifier: MPL-2.0

import { Show, createEffect, on } from "solid-js";
import { Portal } from "solid-js/web";
import { commandPaletteService } from "../service.ts";
import { SearchInput } from "./SearchInput.tsx";
import { CommandList } from "./CommandList.tsx";
import type { PaletteCommand } from "../command-registry.ts";

function getController() {
  return commandPaletteService.getController(window);
}

export function CommandPaletteUI() {
  const controller = getController();
  if (!controller) return;

  const state = controller.state;

  const handleBackdropClick = (e: MouseEvent) => {
    if (e.target === e.currentTarget) {
      state.setIsVisible(false);
    }
  };

  const handleInput = (value: string) => {
    state.setQuery(value);
    controller.updateSearch(value);
  };

  const handleCommandSelect = (index: number) => {
    state.setSelectedIndex(index);
  };

  const handleCommandExecute = (cmd: PaletteCommand) => {
    controller.executeCommand(cmd);
  };

  // Scroll selected item into view
  createEffect(
    on(state.selectedIndex, () => {
      if (!state.isVisible()) return;
      requestAnimationFrame(() => {
        const selected = document.querySelector(
          '.command-palette-item[data-selected="true"]',
        );
        selected?.scrollIntoView({ block: "nearest" });
      });
    }),
  );

  return (
    <Portal mount={document.getElementById("main-window") ?? undefined}>
      <Show when={state.isVisible()}>
        <div
          id="command-palette-overlay"
          onClick={handleBackdropClick}
        >
          <div id="command-palette-container">
            <SearchInput
              query={state.query()}
              onInput={handleInput}
            />
            <CommandList
              commands={state.filteredCommands()}
              selectedIndex={state.selectedIndex()}
              onCommandSelect={handleCommandSelect}
              onCommandExecute={handleCommandExecute}
            />
          </div>
        </div>
      </Show>
    </Portal>
  );
}
