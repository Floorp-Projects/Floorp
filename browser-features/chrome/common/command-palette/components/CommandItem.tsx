// SPDX-License-Identifier: MPL-2.0

import type { PaletteCommand } from "../command-registry.ts";

interface CommandItemProps {
  command: PaletteCommand;
  isSelected: boolean;
  onSelect: () => void;
  onExecute: () => void;
}

export function CommandItem(props: CommandItemProps) {
  const handleMouseEnter = () => {
    props.onSelect();
  };

  const handleClick = () => {
    props.onExecute();
  };

  return (
    <div
      class="command-palette-item"
      data-selected={props.isSelected ? "true" : undefined}
      onMouseEnter={handleMouseEnter}
      onClick={handleClick}
      role="option"
      aria-selected={props.isSelected}
    >
      <div class="command-palette-item-info">
        <span class="command-palette-item-label">{props.command.label}</span>
        {props.command.description && (
          <span class="command-palette-item-description">
            {props.command.description}
          </span>
        )}
      </div>
    </div>
  );
}
