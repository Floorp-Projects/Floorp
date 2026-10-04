// SPDX-License-Identifier: MPL-2.0

import { createEffect, onCleanup } from "solid-js";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";
import { getChromeExtrasSettings } from "../designs/configs.ts";
import { FirefoxSidebarOverlayController } from "./overlay.ts";
import type { NativeSidebarController } from "./types.ts";

export let firefoxSidebarOverlay: FirefoxSidebarOverlayController | undefined;

@noraComponent(import.meta.hot)
export default class FirefoxSidebar extends NoraComponentBase {
  init(): void {
    const native = (globalThis as unknown as {
      SidebarController?: NativeSidebarController;
    }).SidebarController;
    if (!native) return;

    const controller = new FirefoxSidebarOverlayController(native);
    firefoxSidebarOverlay = controller;
    createEffect(() => {
      const settings = getChromeExtrasSettings();
      controller.configure({
        overlay: settings.sidebarOverlap,
        hover: settings.sidebarOverlap && settings.autohideSidebar,
      });
    });
    onCleanup(() => {
      controller.destroy();
      if (firefoxSidebarOverlay === controller) {
        firefoxSidebarOverlay = undefined;
      }
    });
  }
}
