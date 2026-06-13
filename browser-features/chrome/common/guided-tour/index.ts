// SPDX-License-Identifier: MPL-2.0

import { render } from "@nora/solid-xul";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { TourController } from "./controller.ts";
import { createTourOverlay } from "./components/TourOverlay.tsx";
import { WorkspacePanelGuard } from "./workspace-panel-guard.ts";
import style from "./style.css?inline";

@noraComponent(import.meta.hot)
export default class GuidedTour extends NoraComponentBase {
  init(): void {
    WorkspacePanelGuard.cleanupStale();
    if (!document.getElementById("floorp-guided-tour-style")) {
      const styleEl = document.createElement("style");
      styleEl.id = "floorp-guided-tour-style";
      styleEl.textContent = style;
      document.head?.appendChild(styleEl);
    }

    const controller = new TourController();

    const mainWindow = document.getElementById("main-window");
    if (mainWindow) {
      render(createTourOverlay(controller), mainWindow, {
        hotCtx: import.meta.hot,
      });
    }
  }
}
