/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  addDisposer,
  noraComponent,
  NoraComponentBase,
} from "#features-chrome/utils/base";
import { ContextMenuUtils } from "#features-chrome/utils/context-menu.tsx";

@noraComponent("ContextMenu", import.meta.hot)
export default class ContextMenu extends NoraComponentBase {
  init() {
    ContextMenuUtils.contentAreaContextMenu()?.addEventListener(
      "popupshowing",
      ContextMenuUtils.onPopupShowing,
    );
    addDisposer(() => {
      ContextMenuUtils.contentAreaContextMenu()?.removeEventListener(
        "popupshowing",
        ContextMenuUtils.onPopupShowing,
      );
    });
  }
}
