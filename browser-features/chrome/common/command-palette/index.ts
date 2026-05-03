// SPDX-License-Identifier: MPL-2.0

import { render } from "@nora/solid-xul";
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
    // Inject styles
    const styleEl = document.createElement("style");
    styleEl.textContent = style;
    document.head?.appendChild(styleEl);

    // Render the palette overlay
    const mainWindow = document.getElementById("main-window");
    if (mainWindow) {
      render(CommandPaletteUI, mainWindow, {
        hotCtx: import.meta.hot,
      });
    }

    // Attach service — creates controller and manages lifecycle
    commandPaletteService.attachToWindow(window);
  }
}
