// SPDX-License-Identifier: MPL-2.0

import { onCleanup, render } from "@nora/solid-xul";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { commandPaletteService } from "./service.ts";
import { CommandPaletteUI } from "./components/CommandPalette.tsx";
import style from "./style.css?inline";

@noraComponent(import.meta.hot)
export default class CommandPalette extends NoraComponentBase {
  init(): void {
    // Inject styles (idempotent)
    if (!document.getElementById("command-palette-style")) {
      const styleEl = document.createElement("style");
      styleEl.id = "command-palette-style";
      styleEl.textContent = style;
      document.head?.appendChild(styleEl);
    }

    // Attach before rendering, and release this controller on component HMR.
    commandPaletteService.attachToWindow(window);
    onCleanup(() => commandPaletteService.detachFromWindow(window));

    // Render the palette overlay
    const mainWindow = document.getElementById("main-window");
    if (mainWindow) {
      render(CommandPaletteUI, mainWindow, {
        hotCtx: import.meta.hot,
      });
    }
  }
}
