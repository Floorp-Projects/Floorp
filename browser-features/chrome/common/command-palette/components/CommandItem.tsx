// SPDX-License-Identifier: MPL-2.0

import type { PaletteCommand } from "../types.ts";
import { getHighlightSegments, type TextSegment } from "../utils/highlight.ts";
import { getShortcutForAction } from "../command-registry.ts";

interface CommandItemProps {
  command: PaletteCommand;
  isSelected: boolean;
  query: string;
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

  const segments: TextSegment[] = getHighlightSegments(
    props.query,
    props.command.label,
  );
  const shortcut: string | null = getShortcutForAction(props.command.id);

  return (
    <div
      class="command-palette-item"
      data-selected={props.isSelected ? "true" : undefined}
      onMouseEnter={handleMouseEnter}
      onClick={handleClick}
      role="option"
      aria-selected={props.isSelected}
      tabIndex={props.isSelected ? 0 : -1}
    >
      <div class="command-palette-item-info">
        <span class="command-palette-item-label">
          {segments.map((seg) =>
            seg.matched ? (
              <strong class="command-palette-match">{seg.text}</strong>
            ) : (
              seg.text
            )
          )}
        </span>
        {props.command.description && (
          <span class="command-palette-item-description">
            {props.command.description}
          </span>
        )}
      </div>
      {shortcut && <kbd class="command-palette-shortcut-badge">{shortcut}</kbd>}
    </div>
  );
}
