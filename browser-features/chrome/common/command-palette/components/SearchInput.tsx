// SPDX-License-Identifier: MPL-2.0

import i18next from "i18next";

interface SearchInputProps {
  query: string;
  onInput: (value: string) => void;
}

export function SearchInput(props: SearchInputProps) {
  const placeholder = () =>
    i18next.t("commandPalette.placeholder", {
      defaultValue: "Type a command...",
    });

  const handleInput = (e: Event) => {
    const target = e.target as HTMLInputElement;
    props.onInput(target.value);
  };

  return (
    <div class="command-palette-search-wrapper">
      <img
        class="command-palette-search-icon"
        src="chrome://global/skin/icons/search-glass.svg"
        alt=""
      />
      <input
        id="command-palette-search"
        type="text"
        value={props.query}
        onInput={handleInput}
        placeholder={placeholder()}
        aria-label={placeholder()}
        autocomplete="off"
        spellcheck={false}
      />
    </div>
  );
}
