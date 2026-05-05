/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "#features-chrome/common/designs/configs.ts";
import { effect } from "@preact/signals";
import style from "./style.css?inline";
import { render } from "preact";

export class TabPinnedTabCustomization {
  private dispose: (() => void) | null = null;
  private styleContainer: HTMLElement | null = null;

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

      if (!this.styleContainer) {
        const container = document.createElement("span");
        head.appendChild(container);
        this.styleContainer = container;
      }

      try {
        render(this.StyleElement(), this.styleContainer);
        this.dispose = () => {
          render(null, this.styleContainer!);
          this.styleContainer?.remove();
          this.styleContainer = null;
        };
      } catch (error) {
        const reason = error instanceof Error
          ? error
          : new Error(String(error));
        console.error(
          "[TabPinnedTabCustomization] Failed to render style element.",
          reason,
        );
      }
    } else {
      this.dispose?.();
      this.dispose = null;
    }
  }

  constructor() {
    effect(() => {
      const showTitleEnabled = config.value.tab.tabPinTitle;
      this.toggleTitleVisibility(showTitleEnabled);
    });
  }
}
