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
      return step?.placeholder ?? step?.label ?? "";
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
      return step?.label ?? "";
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
          ‹
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
