// SPDX-License-Identifier: MPL-2.0

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
