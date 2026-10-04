/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { h, render } from "@nora/preact-xul";
import { addDisposer } from "@nora/preact-xul/lifetime";
import { ShareModeElement } from "./browser-share-mode";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";

@noraComponent("BrowserShareMode", import.meta.hot)
export default class BrowserShareMode extends NoraComponentBase {
  init() {
    this.logger!.info("Initializing browser share mode menu injection");

    if (typeof document === "undefined") {
      this.logger!.warn(
        "Document is unavailable; skip initializing browser share mode.",
      );
      return;
    }

    // The event may run after this feature's synchronous setup scope ended.
    let disposed = false;
    let disposeMenu: (() => void) | undefined;
    const tryInit = () => {
      if (disposed) return;
      const menuPopup = document!.getElementById("menu_ToolsPopup");
      if (!menuPopup) {
        this.logger!.warn(
          "Failed to locate #menu_ToolsPopup; browser share mode menu item will not be injected.",
        );
        return;
      }
      disposeMenu = this.injectMenu(menuPopup);
    };
    addDisposer(() => {
      disposed = true;
      document.removeEventListener("DOMContentLoaded", tryInit);
      disposeMenu?.();
    });

    if (document!.readyState === "loading") {
      document!.addEventListener("DOMContentLoaded", tryInit, { once: true });
    } else {
      tryInit();
    }
  }

  private injectMenu(menuPopup: Element): (() => void) | undefined {
    const existingMenuitem = menuPopup.querySelector("#toggle_sharemode");
    if (existingMenuitem) {
      this.logger!.info(
        "Share mode menu item already exists; preserving its owner.",
      );
      return;
    }

    const marker = document!.getElementById("menu_openFirefoxView");
    if (!marker) {
      this.logger!.warn(
        "Marker element #menu_openFirefoxView not found; menu item will be appended without marker.",
      );
    } else if (marker.parentElement !== menuPopup) {
      this.logger!.warn(
        "Marker element #menu_openFirefoxView is not a child of #menu_ToolsPopup; menu item will be appended at the end.",
      );
    }

    try {
      const dispose = render(h(ShareModeElement, null), menuPopup, {
        marker: marker?.parentElement === menuPopup ? marker : null,
      });
      this.logger!.info("Browser share mode menu item rendered successfully.");
      return dispose;
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      this.logger!.error("Failed to render share mode menu item", reason);
    }
  }
}
