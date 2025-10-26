/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { config } from "#features-chrome/common/designs/configs.ts";
import { createEffect } from "solid-js";
import style from "./style.css?inline";
import { render } from "@nora/solid-xul";

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
    render(() => styleElement, document?.head);
  }

  constructor() {
    render(() => this.StyleElement(), document?.head);
    createEffect(() => {
      const minH = config().tab.tabMinHeight;
      const minW = config().tab.tabMinWidth;
      this.setTabSizeSpecification(minH, minW);
    });
  }
}
