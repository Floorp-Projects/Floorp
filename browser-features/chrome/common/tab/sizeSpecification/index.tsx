/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "#features-chrome/common/designs/configs.ts";
import { effect } from "@preact/signals";
import style from "./style.css?inline";
import { safeRender } from "@nora/preact-xul";

export class TabSizeSpecification {
  private StyleElement() {
    return <style>{style}</style>;
  }

  private setTabSizeSpecification(minH = 45, minW = 150) {
    // Fallback defaults (same as defaults used in configs.ts)
    const DEFAULT_MIN_H = 30;
    const DEFAULT_MIN_W = 76;

    let validatedMinH = minH;
    let validatedMinW = minW;

    if (
      typeof validatedMinH !== "number" || validatedMinH < 20 ||
      validatedMinH > 100
    ) {
      console.warn(
        `Invalid tab height (${
          String(minH)
        }) provided. Falling back to default: ${DEFAULT_MIN_H}px`,
      );
      validatedMinH = DEFAULT_MIN_H;
    }

    if (
      typeof validatedMinW !== "number" || validatedMinW < 60 ||
      validatedMinW > 300
    ) {
      console.warn(
        `Invalid tab width (${
          String(minW)
        }) provided. Falling back to default: ${DEFAULT_MIN_W}px`,
      );
      validatedMinW = DEFAULT_MIN_W;
    }

    const styleElement = (
      <style id="floorp-tab-size-specification">
        {`:root {
          --floorp-tab-min-height: ${validatedMinH}px;
          --floorp-tab-min-width: ${validatedMinW}px;
        }`}
      </style>
    );

    const head = document?.head;
    if (!head) {
      console.warn(
        "[TabSizeSpecification] document.head is unavailable; skip updating tab size style.",
      );
      return;
    }

    try {
      safeRender(styleElement, head);
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      console.error(
        "[TabSizeSpecification] Failed to render tab size style element.",
        reason,
      );
    }
  }

  constructor() {
    const head = document?.head;
    if (!head) {
      console.warn(
        "[TabSizeSpecification] document.head is unavailable; skip initial style injection.",
      );
      return;
    }

    try {
      safeRender(this.StyleElement(), head);
    } catch (error) {
      const reason = error instanceof Error ? error : new Error(String(error));
      console.error(
        "[TabSizeSpecification] Failed to render base style element.",
        reason,
      );
      return;
    }

    effect(() => {
      const minH = config.value.tab.tabMinHeight;
      const minW = config.value.tab.tabMinWidth;
      this.setTabSizeSpecification(minH, minW);
    });
  }
}
