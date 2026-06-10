// SPDX-License-Identifier: MPL-2.0

import { Show } from "solid-js";
import i18next from "i18next";
import type { PaletteState } from "../data/state.ts";

interface SearchInputProps {
  query: string;
  onInput: (value: string) => void;
  onBack?: () => void;
  state?: PaletteState;
}

export function SearchInput(props: SearchInputProps) {
  const placeholder = () => {
    if (props.state?.mode() === "input") {
      const cmd = props.state.activeCommand();
      const stepIndex = props.state.currentStepIndex();
      const step = cmd?.steps?.[stepIndex];
      return step?.placeholder ?? step?.label ?? i18next.t("commandPalette.placeholder", {
        defaultValue: "Type a command...",
      });
    }
    return i18next.t("commandPalette.placeholder", {
      defaultValue: "Type a command...",
    });
  };

  const ariaLabel = () => {
    if (props.state?.mode() === "input") {
      const cmd = props.state.activeCommand();
      const stepIndex = props.state.currentStepIndex();
      const step = cmd?.steps?.[stepIndex];
      return step?.label ?? i18next.t("commandPalette.placeholder", {
        defaultValue: "Type a command...",
      });
    }
    return i18next.t("commandPalette.placeholder", {
      defaultValue: "Type a command...",
    });
  };

  const handleInput = (e: Event) => {
    const target = e.target as HTMLInputElement;
    props.onInput(target.value);
  };

  const isInputMode = () => props.state?.mode() === "input";

  const handleBackClick = () => {
    props.onBack?.();
  };

  return (
    <div class="command-palette-search-wrapper">
      <Show when={isInputMode()}>
        <button
          type="button"
          class="command-palette-back-button"
          onClick={handleBackClick}
          title={i18next.t("commandPalette.backToCommands", {
            defaultValue: "Back to commands",
          })}
          aria-label={i18next.t("commandPalette.backToCommands", {
            defaultValue: "Back to commands",
          })}
        >
          <svg width="16" height="16" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg" aria-hidden="true">
            <path d="M10 3L5 8L10 13" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round"/>
          </svg>
        </button>
      </Show>
      <Show when={!isInputMode()}>
        <img
          class="command-palette-search-icon"
          src="chrome://global/skin/icons/search-glass.svg"
          alt=""
        />
      </Show>
      <input
        id="command-palette-search"
        type="text"
        value={props.query}
        onInput={handleInput}
        placeholder={placeholder()}
        aria-label={ariaLabel()}
        autocomplete="off"
        spellcheck={false}
      />
    </div>
  );
}
