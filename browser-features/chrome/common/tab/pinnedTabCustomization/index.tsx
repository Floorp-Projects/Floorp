/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "#features-chrome/common/designs/configs.ts";
import { createEffect } from "solid-js";
import style from "./style.css?inline";
import { createRootHMR, render } from "@nora/solid-xul";

export class TabPinnedTabCustomization {
  private dispose: (() => void) | null = null;

  private StyleElement() {
    return <style>{style}</style>;
  }

  private toggleTitleVisibility(showTitleEnabled: boolean) {
    if (showTitleEnabled) {
      const head = document?.head;
      if (!head) {
        console.warn(
          "[TabPinnedTabCustomization] document.head is unavailable; skip injecting style.",
        );
        return;
      }
      createRootHMR((dispose) => {
        try {
          render(() => this.StyleElement(), head);
        } catch (error) {
          const reason = error instanceof Error
            ? error
            : new Error(String(error));
          console.error(
            "[TabPinnedTabCustomization] Failed to render style element.",
            reason,
          );
          return;
        }

        this.dispose = dispose;
      }, import.meta.hot);
    } else {
      this.dispose?.();
    }
  }

  constructor() {
    createEffect(() => {
      const showTitleEnabled = config().tab.tabPinTitle;
      this.toggleTitleVisibility(showTitleEnabled);
    });
  }
}
