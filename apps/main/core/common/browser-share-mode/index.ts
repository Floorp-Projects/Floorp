/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { insert, render } from "@nora/solid-xul";
import { ShareModeElement } from "./browser-share-mode";

export function initShareMode() {
  render(ShareModeElement, document.querySelector("#menu_ToolsPopup"), {
    marker: document.querySelector("#menu_openFirefoxView")!,
    hotCtx: import.meta.hot,
  });

  import.meta.hot?.accept((m) => m?.initShareMode());
}
