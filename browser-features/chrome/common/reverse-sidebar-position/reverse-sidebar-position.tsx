// SPDX-License-Identifier: MPL-2.0

import { BrowserActionUtils } from "#features-chrome/utils/browser-action";
import iconStyle from "./icon.css?inline";

const { CustomizableUI } = ChromeUtils.importESModule(
  "resource:///modules/CustomizableUI.sys.mjs",
);

export class ReverseSidebarPosition {
  private StyleElement = () => {
    return <style>{iconStyle}</style>;
  };

  constructor() {
    BrowserActionUtils.createToolbarClickActionButton(
      "sidebar-reverse-position-toolbar",
      "sidebar-reverse-position-toolbar",
      () => {
        window.SidebarController.reversePosition();
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
