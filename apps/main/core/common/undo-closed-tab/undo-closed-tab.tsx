/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { BrowserActionUtils } from "@core/utils/browser-action";
import iconStyle from "./icon.css?inline";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export class UndoClosedTab {
  private StyleElement = () => {
    return <style>{iconStyle}</style>;
  };

  constructor() {
    BrowserActionUtils.createToolbarClickActionButton(
      "undo-closed-tab",
      "undo-closed-tab",
      () => {
        window.undoCloseTab();
      },
      this.StyleElement(),
      CustomizableUI.AREA_NAVBAR,
      2,
    );
  }
}
