/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { gFloorpBrowserAction } from "../browser-action/browser-action";
import iconStyle from "./icon.pcss?inline";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export class gFloorpUndoClosedTab {
  private static instance: gFloorpUndoClosedTab;
  public static getInstance() {
    if (!gFloorpUndoClosedTab.instance) {
      gFloorpUndoClosedTab.instance = new gFloorpUndoClosedTab();
    }
    return gFloorpUndoClosedTab.instance;
  }
  private StyleElement = () => {
    return <style>{iconStyle}</style>;
  };

  constructor() {
    gFloorpBrowserAction.createToolbarClickActionButton(
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
