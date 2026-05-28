// SPDX-License-Identifier: MPL-2.0

import { Show, createEffect, on } from "solid-js";
import { Portal } from "solid-js/web";
import i18next from "i18next";
import { commandPaletteService } from "../service.ts";
import { SearchInput } from "./SearchInput.tsx";
import { CommandList } from "./CommandList.tsx";
import { StepIndicator } from "./StepIndicator.tsx";
import { StepChoices } from "./StepChoices.tsx";
import type { PaletteCommand, CommandStepChoice } from "../types.ts";

function getController() {
  return commandPaletteService.getController(window);
}

export function CommandPaletteUI() {
  const controller = getController();
  if (!controller) return;

  const state = controller.state;

  const handleBackdropClick = (e: MouseEvent) => {
    if (e.target === e.currentTarget) {
      controller.hidePalette();
    }
  };

  const handleInput = (value: string) => {
    state.setQuery(value);
    controller.updateSearch(value);
  };

  const handleBack = () => {
    controller.goBackStep();
  };

  const handleChoiceSelect = (choice: CommandStepChoice) => {
    // Ensure selectedChoiceIndex is set to the clicked choice's index
    const idx = state.filteredStepChoices().findIndex((c) => c.value === choice.value);
    if (idx >= 0) state.setSelectedChoiceIndex(idx);
    state.setQuery(choice.label);
    controller.advanceStep();
  };

  const handleCommandSelect = (index: number) => {
    state.setSelectedIndex(index);
  };

  const handleCommandExecute = (cmd: PaletteCommand) => {
    controller.executeCommand(cmd);
  };

  const handleTransitionEnd = (e: TransitionEvent) => {
    if (e.target === e.currentTarget && !state.isVisible()) {
      state.setIsAnimatingOut(false);
    }
  };

  // Scroll selected item into view (command mode only)
  createEffect(
    on(state.selectedIndex, () => {
      if (!state.isVisible()) return;
      if (state.mode() !== "command") return;
      requestAnimationFrame(() => {
        const selected = document.querySelector(
          '.command-palette-item[data-selected="true"]',
        );
        selected?.scrollIntoView({ block: "nearest" });
      });
    }),
  );

  // Scroll selected step choice into view (input mode with choices)
  createEffect(
    on(state.selectedChoiceIndex, () => {
      if (!state.isVisible()) return;
      if (state.mode() !== "input") return;
      requestAnimationFrame(() => {
        const selected = document.querySelector(
          '.command-palette-step-choice-item[data-selected="true"]',
        );
        selected?.scrollIntoView({ block: "nearest" });
      });
    }),
  );

  return (
    <Portal mount={document.getElementById("main-window") ?? undefined}>
      <Show when={state.isVisible() || state.isAnimatingOut()}>
        <div
          id="command-palette-overlay"
          role="dialog"
          aria-modal="true"
          aria-label={i18next.t("commandPalette.dialogLabel", {
            defaultValue: "Command Palette",
          })}
          data-visible={state.isVisible() ? "true" : undefined}
          data-mode={state.mode()}
          onClick={handleBackdropClick}
          onTransitionEnd={handleTransitionEnd}
        >
          <div id="command-palette-container">
            <SearchInput
              query={state.query()}
              onInput={handleInput}
              onBack={handleBack}
              state={state}
            />
            <Show when={state.mode() === "input"}>
              <StepIndicator state={state} />
              <Show when={state.stepError()}>
                {(error) => (
                  <div class="command-palette-step-error">{error()}</div>
                )}
              </Show>
              <StepChoices
                state={state}
                onSelect={handleChoiceSelect}
              />
            </Show>
            <Show when={state.mode() === "command"}>
              <CommandList
                commands={state.filteredCommands()}
                selectedIndex={state.selectedIndex()}
                query={state.query()}
                onCommandSelect={handleCommandSelect}
                onCommandExecute={handleCommandExecute}
              />
            </Show>
          </div>
        </div>
      </Show>
    </Portal>
  );
}
