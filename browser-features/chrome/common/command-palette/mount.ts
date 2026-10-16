// SPDX-License-Identifier: MPL-2.0

import { onCleanup, render } from "@nora/solid-xul";
import { CommandPaletteUI } from "./components/CommandPalette.tsx";

/** Mount only into a palette-owned subtree; the native chrome is not a render root. */
export function mountCommandPalette(parent: Element): HTMLElement {
  const mount = parent.ownerDocument.createElement("div");
  mount.className = "command-palette-mount";
  mount.style.display = "contents";
  parent.appendChild(mount);
  onCleanup(() => mount.remove());

  // An empty keyed Show may clear all children of its render root. Keeping that
  // root separate protects the browser's head/body/tabbrowser during OFF/ON.
  render(CommandPaletteUI, mount);
  return mount;
}
