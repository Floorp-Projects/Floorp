// SPDX-License-Identifier: MPL-2.0

import { Show, For } from "solid-js";
import type { PaletteCommand } from "../command-registry.ts";
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

  const segments = (): TextSegment[] =>
    getHighlightSegments(props.query, props.command.label);

  const shortcut = (): string | null =>
    getShortcutForAction(props.command.id);

  return (
    <div
      class="command-palette-item"
      data-selected={props.isSelected ? "true" : undefined}
      onMouseEnter={handleMouseEnter}
      onClick={handleClick}
      role="option"
      aria-selected={props.isSelected}
      tabindex={props.isSelected ? 0 : -1}
    >
      <div class="command-palette-item-info">
        <span class="command-palette-item-label">
          <For each={segments()}>
            {(seg) =>
              seg.matched ? (
                <strong class="command-palette-match">{seg.text}</strong>
              ) : (
                seg.text
              )
            }
          </For>
        </span>
        {props.command.description && (
          <span class="command-palette-item-description">
            {props.command.description}
          </span>
        )}
      </div>
      <Show when={shortcut()}>
        {(s) => <kbd class="command-palette-shortcut-badge">{s()}</kbd>}
      </Show>
    </div>
  );
}
