/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { ChromeCSSService } from "./service.tsx";

/**
 * Chrome CSS component for user styles management
 */
@noraComponent(import.meta.hot)
export default class ChromeCSS extends NoraComponentBase {
  static ctx: ChromeCSSService | null = null;

  init(): void {
    const cssService = ChromeCSSService.getInstance();
    cssService.init();
    ChromeCSS.ctx = cssService;
  }
}
