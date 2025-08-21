/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { render } from "@nora/solid-xul";
import { ShareModeElement } from "./browser-share-mode.tsx";
import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";

@noraComponent(import.meta.hot)
export default class BrowserShareMode extends NoraComponentBase {
  init() {
    this.logger.info("Hello from Logger!");
    render(ShareModeElement, document.querySelector("#menu_ToolsPopup"), {
      marker: document.querySelector("#menu_openFirefoxView")!,
      hotCtx: import.meta.hot,
    });
  }
}
