/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "#features-chrome/common/designs/configs.ts";
import { rootEffect } from "@nora/preact-xul/lifetime";
import style from "./style.css?inline";
import { render } from "@nora/preact-xul";

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

      try {
        this.dispose = render(() => this.StyleElement(), head);
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
    rootEffect(() => {
      const showTitleEnabled = config.value.tab.tabPinTitle;
      this.toggleTitleVisibility(showTitleEnabled);
      return () => {
        this.dispose?.();
        this.dispose = null;
      };
    });
  }
}
