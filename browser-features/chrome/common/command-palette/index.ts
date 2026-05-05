// SPDX-License-Identifier: MPL-2.0

import { h } from "preact";
import { safeRender } from "@nora/preact-xul";
import { addDisposer } from "@nora/preact-xul/lifetime";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { commandPaletteService } from "./service.ts";
import { CommandPaletteUI } from "./components/CommandPalette.tsx";
import style from "./style.css?inline";

@noraComponent("CommandPalette", import.meta.hot)
export default class CommandPalette extends NoraComponentBase {
  init(): void {
    // Inject styles (idempotent)
    if (!document.getElementById("command-palette-style")) {
      const styleEl = document.createElement("style");
      styleEl.id = "command-palette-style";
      styleEl.textContent = style;
      document.head?.appendChild(styleEl);
    }

    // Render the palette overlay via safeRender — appends a display:contents
    // wrapper so existing main-window children are not disturbed.
    const mainWindow = document.getElementById("main-window");
    if (mainWindow) {
      const dispose = safeRender(h(CommandPaletteUI, {}), mainWindow);
      addDisposer(dispose);
    }

    // Attach service — creates controller and manages lifecycle
    commandPaletteService.attachToWindow(window);
  }
}
