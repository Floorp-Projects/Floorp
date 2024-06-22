/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { gFloorpBrowserAction } from "../browser-action/browser-action";
import iconStyle from "./icon.pcss?inline";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export class gReverseSidebarPosition {
  private static instance: gReverseSidebarPosition;
  public static getInstance() {
    if (!gReverseSidebarPosition.instance) {
      gReverseSidebarPosition.instance = new gReverseSidebarPosition();
    }
    return gReverseSidebarPosition.instance;
  }
  private StyleElement = () => {
    return <style>{iconStyle}</style>;
  };

  constructor() {
    gFloorpBrowserAction.createToolbarClickActionButton(
      "sidebar-reverse-position-toolbar",
      "sidebar-reverse-position-toolbar",
      () => {
        window.SidebarUI.reversePosition();
      },
      this.StyleElement(),
      CustomizableUI.AREA_NAVBAR,
      1,
      () => {
        // Sidebar button should right side of navbar
        CustomizableUI.addWidgetToArea(
          "sidebar-button",
          CustomizableUI.AREA_NAVBAR,
          0,
        );
      },
    );
  }
}
