/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base.ts";
import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";
import { onCleanup } from "solid-js";

@noraComponent(import.meta.hot)
export default class ContextMenu extends NoraComponentBase {
  init() {
    this.logger.debug("init");
    ContextMenuUtils.contentAreaContextMenu()?.addEventListener(
      "popupshowing",
      ContextMenuUtils.onPopupShowing,
    );
    onCleanup(() => {
      this.logger.debug("onCleanup");
      ContextMenuUtils.contentAreaContextMenu()?.removeEventListener(
        "popupshowing",
        ContextMenuUtils.onPopupShowing,
      );
    });
  }
}
