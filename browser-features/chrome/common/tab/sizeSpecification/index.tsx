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
    if (minH < 20 || minH > 100) {
      console.error("Tab height must be between 20 and 100 pixels.");
      return;
    }
    if (minW < 60 || minW > 300) {
      console.error("Tab width must be between 60 and 300 pixels.");
      return;
    }

    const styleElement = (
      <style id="floorp-tab-size-specification">
        {`:root {
          --floorp-tab-min-height: ${minH}px;
          --floorp-tab-min-width: ${minW}px;
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
