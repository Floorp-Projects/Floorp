/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { FloorpPrivateContainer } from "./browser-private-container";
import { ContextMenu } from "./context-menu";
import { noraComponent, NoraComponentBase } from "#features-chrome/utils/base";

@noraComponent(import.meta.hot)
export default class PrivateContainer extends NoraComponentBase {
  init(): void {
    if (typeof document === "undefined") {
      this.logger.warn(
        "Document is unavailable; skip initializing private container context menu.",
      );
      return;
    }

    const tabContextMenu = document?.querySelector("#tabContextMenu");
    if (!tabContextMenu) {
      this.logger.error(
        "Failed to locate #tabContextMenu; private container menu item will not be injected.",
      );
      return;
    }

    const existingMenuItem = document?.getElementById(
      "context_toggleToPrivateContainer",
    );
    if (existingMenuItem) {
      this.logger.info(
        "Private container menu item already exists; skip duplicate injection.",
      );
    } else {
      const marker = document?.querySelector("#context_selectAllTabs");
      if (!marker) {
        this.logger.warn(
          "Marker element #context_selectAllTabs not found; menu item will be appended without marker.",
        );
      } else if (marker.parentElement !== tabContextMenu) {
        this.logger.warn(
          "Marker element #context_selectAllTabs is not a child of #tabContextMenu; menu item will be appended at the end.",
        );
      }

      try {
        render(ContextMenu, tabContextMenu, {
          marker: marker?.parentElement === tabContextMenu ? marker : undefined,
        });
        this.logger.info("Private container menu item rendered successfully.");
      } catch (error) {
        const reason =
          error instanceof Error ? error : new Error(String(error));
        this.logger.error(
          "Failed to render private container menu item",
          reason,
        );
      }
    }

    // Inject menu item to open link in private container.
    window.gFloorpPrivateContainer = new FloorpPrivateContainer();
  }
}
