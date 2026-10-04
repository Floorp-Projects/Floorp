/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { signal } from "@preact/signals";
import { rootEffect } from "@nora/preact-xul/lifetime";
import { config } from "#features-chrome/common/designs/configs.ts";

export class TabColorManager {
  enableTabColor = signal(config.value.globalConfigs.faviconColor);

  constructor() {
    if (!globalThis.gFloorp) {
      globalThis.gFloorp = {};
    }
    globalThis.gFloorp.tabColor = {
      setEnable: (v: boolean) => {
        this.enableTabColor.value = v;
      },
    };
  }

  init() {
    rootEffect(() => {
      const currentConfig = config.value;
      if (currentConfig.globalConfigs.faviconColor !== this.enableTabColor.value) {
        this.enableTabColor.value = currentConfig.globalConfigs.faviconColor;
      }
    });
  }
}
